#include "block_align.h"
#include "../../logdef.h"
#include "glib.h"
#include "types/layer_context.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LayerContext block_align_init(LayerContext *next_layer, int nlayers,
                              size_t block_size) {

  LayerContext layer_context;

  LayerOps *block_align_ops = malloc(sizeof(LayerOps));
  layer_context.ops = block_align_ops;
  layer_context.ops->ldestroy = block_align_destroy;
  layer_context.ops->lpread = block_align_pread;
  layer_context.ops->lpwrite = block_align_pwrite;
  layer_context.ops->lopen = block_align_open;
  layer_context.ops->lclose = block_align_close;
  layer_context.ops->lftruncate = block_align_ftruncate;
  layer_context.ops->lfstat = block_align_fstat;
  layer_context.ops->llstat = block_align_lstat;
  layer_context.ops->lunlink = block_align_unlink;

  LayerContext *aux = malloc(sizeof(LayerContext));
  memcpy(aux, next_layer, sizeof(LayerContext));
  layer_context.next_layers = aux;
  layer_context.nlayers = nlayers;

  layer_context.app_context = NULL;

  BlockAlignState *state = malloc(sizeof(BlockAlignState));
  state->block_size = block_size;
  state->fds_special_flags = g_hash_table_new(g_direct_hash, g_direct_equal);
  layer_context.internal_state = (void *)state;

  return layer_context;
}

void block_align_destroy(LayerContext l) {

  if (DEBUG_ENABLED()) {
    DEBUG_MSG("[BLOCK_ALIGN_LAYER] Destroy called");
  }

  BlockAlignState *state = (BlockAlignState *)l.internal_state;
  g_hash_table_destroy(state->fds_special_flags);
  l.next_layers->ops->ldestroy(*l.next_layers);
  free(l.ops);
  free(l.next_layers);
  free(state);
}

int block_align_open(const char *pathname, int flags, mode_t mode,
                     LayerContext l) {
  int fd;
  BlockAlignState *state = (BlockAlignState *)l.internal_state;

  int append = (flags & O_APPEND) != 0;
  int write = (flags & O_WRONLY) != 0;
  int value = 0; // this will accumulate the value to insert in the hash table
                 // in order to annotate what special cases this fd has

  if (append) {
    flags ^= O_APPEND; // remove append flag
    value |= O_APPEND;
  }

  if (write) {
    flags ^= O_WRONLY; // remove write flag
    flags |= O_RDWR;   // add read and write flag
    value |= O_WRONLY;
  }

  l.next_layers->app_context = l.app_context;
  fd = l.next_layers->ops->lopen(pathname, flags, mode, *l.next_layers);

  if (write || append)
    g_hash_table_insert(state->fds_special_flags, GINT_TO_POINTER(fd),
                        GINT_TO_POINTER(value));

  return fd;
}

int block_align_close(int fd, LayerContext l) {
  int res;
  l.next_layers->app_context = l.app_context;
  BlockAlignState *state = (BlockAlignState *)l.internal_state;
  res = l.next_layers->ops->lclose(fd, *l.next_layers);
  if (res == 0) {
    g_hash_table_remove(state->fds_special_flags, GINT_TO_POINTER(fd));
  }
  return res;
}

ssize_t block_align_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                          LayerContext l) {

  BlockAlignState *state = (BlockAlignState *)l.internal_state;

  int special = GPOINTER_TO_INT(
      g_hash_table_lookup(state->fds_special_flags, GINT_TO_POINTER(fd)));
  if ((special & O_WRONLY) != 0) { // this means that this file had O_RDONLY
                                   // forcefully added in the open
    errno = 9;
    return -1;
  }

  size_t block_size = state->block_size;
  size_t offset_fst_block = offset % block_size;
  int is_offset_aligned = offset_fst_block == 0;
  int is_nbytes_aligned = (nbytes % block_size) == 0;

  ssize_t bytes_return; // the number of bytes to be returned

  if (is_offset_aligned && is_nbytes_aligned) {
    bytes_return =
        l.next_layers->ops->lpread(fd, buffer, nbytes, offset, *l.next_layers);

  } else {

    size_t start_block = offset / block_size;
    size_t start_bytes = start_block * block_size;
    size_t final_block = (offset + nbytes - 1) / block_size;
    size_t num_blocks = final_block - start_block + 1;
    size_t total_block_bytes = num_blocks * block_size;

    char *block_buffer = malloc(total_block_bytes);

    if (block_buffer == NULL)
      return -1;

    ssize_t num_bytes_read =
        l.next_layers->ops->lpread(fd, block_buffer, total_block_bytes,
                                   (off_t)start_bytes, *l.next_layers);

    if (num_bytes_read == -1) {
      free(block_buffer);
      return -1;
    }

    // the number of bytes to be modified and then returned
    // depends on whether we're at the end of the file or not
    // left case -> end of the file; right case -> middle of the file
    bytes_return = (start_bytes + num_bytes_read < offset + nbytes)
                       ? (ssize_t)(num_bytes_read - offset_fst_block)
                       : (ssize_t)nbytes;
    if (bytes_return <= 0) {
      free(block_buffer);
      return 0;
    }

    memcpy(buffer, block_buffer + offset_fst_block, bytes_return);
    free(block_buffer);
  }

  return bytes_return;
}

ssize_t block_align_pwrite(int fd, const void *buffer, size_t nbytes,
                           off_t offset, LayerContext l) {

  BlockAlignState *state = (BlockAlignState *)l.internal_state;

  ssize_t bytes_written;
  size_t bytes_to_write;

  size_t block_size = state->block_size;

  int value = GPOINTER_TO_INT(
      g_hash_table_lookup(state->fds_special_flags, GINT_TO_POINTER(fd)));

  if ((value & O_APPEND) != 0) // simulate O_APPEND behaviour
  {
    struct stat stbuf;
    int res = l.next_layers->ops->lfstat(fd, &stbuf, *l.next_layers);
    if (res == -1) {
      ERROR_MSG("[BLOCK_ALIGN_PWRITE] Failed to get file size for file (fd=%d)",
                fd);
      return -1;
    }
    offset = stbuf.st_size;
  }
  size_t offset_fst_block = offset % block_size;
  int is_offset_aligned = offset_fst_block == 0;
  int is_nbytes_aligned = (nbytes % block_size) == 0;

  if (is_offset_aligned &&
      is_nbytes_aligned) { // we're writing whole blocks, so there's no need to
                           // read first

    bytes_to_write = nbytes;
    bytes_written = l.next_layers->ops->lpwrite(fd, buffer, bytes_to_write,
                                                offset, *l.next_layers);

  } else { // we're modifying parts of a block, so it's necessary to read and
           // then modify

    size_t start_block = offset / block_size;
    size_t start_bytes = start_block * block_size;
    size_t final_block = (offset + nbytes - 1) / block_size;
    size_t num_blocks = final_block - start_block + 1;
    size_t total_block_bytes = num_blocks * block_size;

    char *block_buffer = malloc(total_block_bytes);

    if (block_buffer == NULL)
      return -1;

    ssize_t num_bytes_read =
        l.next_layers->ops->lpread(fd, block_buffer, total_block_bytes,
                                   (off_t)start_bytes, *l.next_layers);

    if (num_bytes_read == -1) {
      free(block_buffer);
      return -1;
    }

    memcpy(block_buffer + offset_fst_block, buffer, nbytes);

    /* the number of bytes to be re-written depends on whether
    we'll cross the end of the file or not
    left case -> cross end of the file; right case -> middle of the file*/
    bytes_to_write = (start_bytes + num_bytes_read < offset + nbytes)
                         ? nbytes + offset_fst_block
                         : num_bytes_read;

    bytes_written = l.next_layers->ops->lpwrite(
        fd, block_buffer, bytes_to_write, (off_t)start_bytes, *l.next_layers);

    free(block_buffer);
  }

  /* if the write fails (i.e., returns -1), -1 is returned;
  however, -1 is also returned if the write
  goes partially wrong (e.g., disk full)*/
  if (bytes_written != bytes_to_write)
    return -1;
  else
    return (ssize_t)nbytes;
}

int block_align_ftruncate(int fd, off_t length, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lftruncate(fd, length, *l.next_layers);
}

int block_align_fstat(int fd, struct stat *stbuf, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
}

int block_align_lstat(const char *pathname, struct stat *stbuf,
                      LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
}

int block_align_unlink(const char *pathname, LayerContext l) {

  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lunlink(pathname, *l.next_layers);
}
