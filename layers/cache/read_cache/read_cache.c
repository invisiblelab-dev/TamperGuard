#include "read_cache.h"
#include "../../../logdef.h"
#include "glibconfig.h"
#include "types/layer_context.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static long total_misses = 0;
static long total_hits = 0;

guint inode_hash(gconstpointer key) {
  ino_t *inode = (ino_t *)key;
  return (guint)(*inode ^ (*inode >> 32));
}

gboolean inode_equal(gconstpointer a, gconstpointer b) {
  ino_t *aa = (ino_t *)a;
  ino_t *bb = (ino_t *)b;
  return *aa == *bb;
}

int remove_cached_entries_range(ino_t inode, long start, long end,
                                ReadCacheState *state) {
  char key[BUFSIZ] = {'\0'};
  int removed = -1;
  int contains = 0;
  for (long i = start; i <= end; i++) {

    if (snprintf(key, BUFSIZ, "%ld/%ld", inode, i) < 0)
      return -1;
    contains = state->ops.contain_item(state->cache_wrapper, key);
    if (contains == 1) {
      removed = state->ops.remove_item(state->cache_wrapper, key);
      if (removed == -1)
        return -1;
    }
  }
  return 0;
}

LayerContext read_cache_init(LayerContext *next_layer, int nlayers,
                             size_t block_size, size_t num_blocks) {

  LayerContext layer_context;

  LayerOps *read_cache_ops = malloc(sizeof(LayerOps));
  layer_context.ops = read_cache_ops;
  layer_context.ops->ldestroy = read_cache_destroy;
  layer_context.ops->lpread = read_cache_pread;
  layer_context.ops->lpwrite = read_cache_pwrite;
  layer_context.ops->lopen = read_cache_open;
  layer_context.ops->lclose = read_cache_close;
  layer_context.ops->lftruncate = read_cache_ftruncate;
  layer_context.ops->llstat = read_cache_lstat;
  layer_context.ops->lfstat = read_cache_fstat;
  layer_context.ops->lunlink = read_cache_unlink;
  LayerContext *aux = malloc(sizeof(LayerContext));
  memcpy(aux, next_layer, sizeof(LayerContext));
  layer_context.next_layers = aux;
  layer_context.nlayers = nlayers;

  layer_context.app_context = NULL;
  ReadCacheState *state = malloc(sizeof(ReadCacheState));
  state->block_size = block_size;
  state->num_blocks = num_blocks;
  state->fd_to_inode =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
  state->inode_to_info =
      g_hash_table_new_full(inode_hash, inode_equal, free, free);
  state->shared_lib_handle = dlopen("libcache_lib_wrapper.so", RTLD_LAZY);

  void *(*initializeCache)(size_t, size_t, char *) =
      dlsym(state->shared_lib_handle, "initialize_cache");

  void *cacheWrapper =
      initializeCache(state->num_blocks, state->block_size, "read_cache");
  if (cacheWrapper == NULL) {
    ERROR_MSG("[READ_CACHE_INIT] Failed to create a CacheWrapper instance");
    exit(1);
  }
  state->cache_wrapper = cacheWrapper;
  state->ops.insert_item = dlsym(state->shared_lib_handle, "insert_item");
  state->ops.get_item = dlsym(state->shared_lib_handle, "get_item");
  state->ops.remove_item = dlsym(state->shared_lib_handle, "remove_item");
  state->ops.contain_item = dlsym(state->shared_lib_handle, "contain_item");
  state->ops.get_item_count = dlsym(state->shared_lib_handle, "get_item_count");
  state->ops.destroy_cache = dlsym(state->shared_lib_handle, "destroy_cache");

  layer_context.internal_state = (void *)state;

  return layer_context;
}

void read_cache_destroy(LayerContext l) {

  if (DEBUG_ENABLED()) {
    DEBUG_MSG("[READ_CACHE_LAYER] Destroy called");
  }

  ReadCacheState *state = (ReadCacheState *)l.internal_state;

  g_hash_table_destroy(state->fd_to_inode);
  g_hash_table_destroy(state->inode_to_info);

  state->ops.destroy_cache(state->cache_wrapper);
  dlclose(state->shared_lib_handle);
  free(state);

  l.next_layers->ops->ldestroy(*l.next_layers);
  free(l.next_layers);
  free(l.ops);
}

int read_cache_open(const char *pathname, int flags, mode_t mode,
                    LayerContext l) {
  int fd, trunc, create;
  off_t size = 0;
  struct stat stbuf;

  ReadCacheState *state = l.internal_state;
  l.next_layers->app_context = l.app_context;

  trunc = (flags & O_TRUNC) != 0;
  create = (flags & O_CREAT) != 0;

  // try to execute lstat
  int stat_res = l.next_layers->ops->llstat(pathname, &stbuf, *l.next_layers);
  if (stat_res == -1) {
    /* if lstat failed and there's no O_CREAT flag,
    return error, else the file will be completely new*/
    if (!create) {
      return -1;
    } else
      size = 0;
  } else
    size = stbuf.st_size;

  fd = l.next_layers->ops->lopen(pathname, flags, mode, *l.next_layers);

  if (fd != -1) {

    // check if the first lstat failed (file created just now)
    // if it did, execute it now so we can get the inode number
    if (stat_res == -1) {
      stat_res = l.next_layers->ops->lfstat(fd, &stbuf, *l.next_layers);

      if (stat_res == -1) {
        l.ops->lclose(fd, l);
        return -1;
      }
    }

    ino_t *inode = malloc(sizeof(ino_t));
    *inode = stbuf.st_ino;
    g_hash_table_insert(state->fd_to_inode, GINT_TO_POINTER(fd), inode);

    InodeInfo *value = g_hash_table_lookup(state->inode_to_info, inode);

    // check if the inode was being mapped (in which case, the counter just
    // needs to be incremented) if the inode isn't present, insert a counter of
    // 1 (the only opened fd is the one we're currently opening) and the
    // unlinked flag as false
    if (value == NULL) {
      ino_t *new_key = malloc(sizeof(ino_t));
      *new_key = stbuf.st_ino;

      InodeInfo *new_value = malloc(sizeof(InodeInfo));
      new_value->counter = 1;
      new_value->unlinked = 0;

      g_hash_table_insert(state->inode_to_info, new_key, new_value);
    } else
      value->counter++;

    // if the file was truncated, it's necessary to remove the old content from
    // the cache
    if (trunc) {
      // converting the block_size to long shouldn't be a problem, unless
      // block_size is in the petabyte range...
      int r = remove_cached_entries_range(
          stbuf.st_ino, 0, ((size - 1) / (long)state->block_size), state);
      if (r == -1) {
        l.ops->lclose(fd, l);
        return -1;
      }
    }
  }

  return fd;
}

int read_cache_close(int fd, LayerContext l) {
  int res;
  ReadCacheState *state = l.internal_state;
  l.next_layers->app_context = l.app_context;

  ino_t *inodep = g_hash_table_lookup(state->fd_to_inode, GINT_TO_POINTER(fd));
  ino_t inode = *inodep;
  InodeInfo *value = g_hash_table_lookup(state->inode_to_info, &inode);

  // this means we're closing the last fd to the inode and unlink was called for
  // this file, so we must remove all the cached entries relative to this inode
  if (value->unlinked && value->counter == 1) {

    struct stat stbuf;
    res = l.next_layers->ops->lfstat(fd, &stbuf, *l.next_layers);

    if (res == -1)
      return -1;

    long end_block = stbuf.st_size / (long)state->block_size;
    res = remove_cached_entries_range(inode, 0, end_block, state);

    if (res == -1)
      return -1;

    res = l.next_layers->ops->lclose(fd, *l.next_layers);

    if (res != -1) {
      g_hash_table_remove(state->fd_to_inode, GINT_TO_POINTER(fd));
      g_hash_table_remove(state->inode_to_info, &inode);
    }
  } else {
    res = l.next_layers->ops->lclose(fd, *l.next_layers);

    if (res != -1) {
      g_hash_table_remove(state->fd_to_inode, GINT_TO_POINTER(fd));
      value->counter--;
    }
  }

  return res;
}

ssize_t read_cache_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                         LayerContext l) {
  if (nbytes == 0)
    return 0;

  ReadCacheState *state = l.internal_state;

  if (INFO_ENABLED()) {
    unsigned long item_count = state->ops.get_item_count(state->cache_wrapper);
    INFO_MSG("[READ_CACHE_LAYER] Currently cached items count: %lu",
             item_count);
  }

  int insert_error = 0;

  // get inode associated to the fd
  ino_t *inode = g_hash_table_lookup(state->fd_to_inode, GINT_TO_POINTER(fd));

  char key[BUFSIZ] = {'\0'};

  size_t block_size = state->block_size;
  size_t start = offset / block_size;
  size_t end = (offset + nbytes - 1) / block_size;

  ssize_t bytes_read; // bytes read in an iteration
  size_t total_bytes_read = 0;
  CacheEntry cached_block;
  size_t blocks_to_read = 0; // Number of consecutive cache misses to read at
                             // once from the lower layer

  size_t i = 0;
  // build up the result block by block, coalescing contiguous requests
  for (i = start; i <= end; i++) {

    if (snprintf(key, BUFSIZ, "%ld/%ld", *inode, i) < 0)
      return -1;

    state->ops.get_item(state->cache_wrapper, key, &cached_block);

    if (cached_block.block == NULL) { // Cache miss
      if (INFO_ENABLED())
        INFO_MSG("[READ_CACHE_PREAD] Cache miss for key %s (total %ld)", key,
                 ++total_misses);
      blocks_to_read++;
      bytes_read = 0;
    } else { // cache hit

      if (INFO_ENABLED())
        INFO_MSG("[READ_CACHE_PREAD] Cache hit for key %s (total %ld)", key,
                 ++total_hits);

      size_t bytes_to_read = blocks_to_read * block_size;

      // copy the content from the cache to the buffer in the correct place,
      // i.e., after all the contiguous misses and previously read blocks
      memcpy(buffer + bytes_to_read + total_bytes_read, cached_block.block,
             cached_block.size);

      bytes_read = (long)cached_block.size;

      // if there are accumulated misses, coalesce the request into a single
      // pread to the next layer
      if (blocks_to_read > 0) {

        bytes_read += l.next_layers->ops->lpread(
            fd, buffer + total_bytes_read, bytes_to_read,
            offset + (long)total_bytes_read, *l.next_layers);
        if (bytes_read == -1)
          return -1;
        // update cache, block by block
        for (int j = 0; j < blocks_to_read; j++) {

          if (snprintf(key, BUFSIZ, "%ld/%d", *inode,
                       (int)(i - blocks_to_read + j)) < 0)
            return -1;
          insert_error =
              state->ops.insert_item(state->cache_wrapper, key,
                                     (const void *)buffer + total_bytes_read +
                                         (size_t)(j * block_size),
                                     block_size);
          if (insert_error == -1) {
            ERROR_MSG("[READ_CACHE_PREAD] Failed to insert item with key %s",
                      key);
          }
        }
        blocks_to_read = 0;
      }
    }

    total_bytes_read += bytes_read;
  }

  // If there was no cache hit by the end of the loop, call pread for the
  // letftover blocks
  if (blocks_to_read > 0) {

    bytes_read = l.next_layers->ops->lpread(
        fd, buffer + total_bytes_read, block_size * blocks_to_read,
        offset + (long)total_bytes_read, *l.next_layers);
    if (bytes_read == -1)
      return -1;
    size_t blocks_read = bytes_read / block_size;
    size_t last_block_offset = bytes_read % block_size;
    size_t blocks_to_add =
        last_block_offset > 0 ? blocks_read + 1 : blocks_read;

    // Update cache, block by block
    for (int j = 0; j < blocks_to_add; j++) {

      size_t entry_size = block_size;

      // checking if we're in the last block, which can be incomplete
      if (j + 1 == blocks_to_add)
        entry_size = last_block_offset > 0 ? last_block_offset : block_size;

      if (snprintf(key, BUFSIZ, "%ld/%d", *inode,
                   (int)(i - blocks_to_read + j)) < 0)
        return -1;
      insert_error = state->ops.insert_item(
          state->cache_wrapper, key, (const void *)buffer + total_bytes_read,
          entry_size);
      if (insert_error == -1) {
        ERROR_MSG("[READ_CACHE_PREAD] Failed to insert item with key %s", key);
      }
      total_bytes_read += entry_size;
    }
  }

  return (ssize_t)total_bytes_read;
}

ssize_t read_cache_pwrite(int fd, const void *buffer, size_t nbytes,
                          off_t offset, LayerContext l) {

  ReadCacheState *state = (ReadCacheState *)l.internal_state;

  char key[BUFSIZ] = {'\0'};
  ssize_t bytes_written =
      l.next_layers->ops->lpwrite(fd, buffer, nbytes, offset, *l.next_layers);
  if (bytes_written <= 0)
    return bytes_written;
  // get inode associated to the fd
  ino_t *inode = g_hash_table_lookup(state->fd_to_inode, GINT_TO_POINTER(fd));
  size_t block_size = state->block_size;
  size_t start = offset / block_size;
  size_t end = (offset + nbytes - 1) / block_size;

  size_t last_block_offset = nbytes % block_size;
  size_t bytes_to_write = block_size;
  int contains = 0;
  // update the cache block by block
  for (size_t i = start, j = 0; i <= end; i++, j++) {
    if (i == end)
      bytes_to_write = last_block_offset == 0 ? block_size : last_block_offset;

    if (snprintf(key, BUFSIZ, "%ld/%ld", *inode, i) < 0)
      return -1;

    contains = state->ops.contain_item(state->cache_wrapper, key);

    // the block is in cache, so we update it
    if (contains == 1) {
      int insert_error = state->ops.insert_item(
          state->cache_wrapper, key, buffer + j * block_size, bytes_to_write);
      if (insert_error == -1) {
        ERROR_MSG("[READ_CACHE_PWRITE] Failed to insert item with key %s", key);
      }
    }
  }

  return bytes_written;
}

int read_cache_ftruncate(int fd, off_t length, LayerContext l) {

  int insert_error = 0;
  struct stat stbuf;
  int res = l.next_layers->ops->lfstat(fd, &stbuf, *l.next_layers);
  if (res == -1)
    return -1;
  off_t size = stbuf.st_size;

  res = l.next_layers->ops->lftruncate(fd, length, *l.next_layers);
  if (res == -1)
    return -1;

  ReadCacheState *state = (ReadCacheState *)l.internal_state;
  ino_t *inode = g_hash_table_lookup(state->fd_to_inode, GINT_TO_POINTER(fd));

  size_t block_size = state->block_size;

  // the file will be lengthened, so there's no need to remove blocks, but
  // rather fill with \0s (if it is in cache) the block that was previously the
  // last one
  if (length > size) {

    off_t last_block = (size - 1) / (off_t)block_size;
    char key[BUFSIZ] = {'\0'};
    if (snprintf(key, BUFSIZ, "%ld/%ld", *inode, last_block) < 0)
      return -1;
    CacheEntry cached_block;
    state->ops.get_item(state->cache_wrapper, key, &cached_block);

    if (cached_block.block != NULL) {
      size_t last_block_length = cached_block.size;
      size_t added_length = length - size;
      size_t necessary_bytes = block_size - last_block_length;
      size_t bytes_to_zero =
          (necessary_bytes <= added_length) ? necessary_bytes : added_length;

      if (bytes_to_zero != 0) {
        void *block_buffer = calloc(sizeof(void), block_size);
        memcpy(block_buffer, cached_block.block, cached_block.size);

        insert_error =
            state->ops.insert_item(state->cache_wrapper, key, block_buffer,
                                   last_block_length + bytes_to_zero);
        if (insert_error == -1) {
          ERROR_MSG("[READ_CACHE_FTRUNCATE] Failed to insert item with key %s",
                    key);
        }

        free(block_buffer);
      }
    }
  } else { // the file will be shortened, so it's necessary to remove the
           // affected blocks
    size_t first_block_rm; // first block to remove from the cache
    size_t last_block_rm;  // last block to remove from the cache

    // the file will be shortened to an aligned length, so there won't be any
    // incomplete blocks
    if (length % block_size == 0) {

      first_block_rm = length / block_size;
      last_block_rm = (size - 1) / block_size;
    }
    // the file will be shortened to a non-aligned length, so the last block
    // will be incomplete
    else {

      size_t last_block =
          length / block_size; // last block that will stay in the file
      size_t last_block_length =
          length % block_size; // length that last block will have

      char key[BUFSIZ] = {'\0'};
      if (snprintf(key, BUFSIZ, "%ld/%ld", *inode, last_block) < 0)
        return -1;
      CacheEntry cached_block;
      state->ops.get_item(state->cache_wrapper, key, &cached_block);

      if (cached_block.block != NULL) {
        /* the content of the block doesn't change, so it's only necessary to
        update the size of the cached block*/
        insert_error = state->ops.insert_item(
            state->cache_wrapper, key, cached_block.block, last_block_length);
        if (insert_error == -1) {
          ERROR_MSG("[READ_CACHE_FTRUNCATE] Failed to insert item with key %s",
                    key);
        }
      }

      first_block_rm =
          (length / block_size) + 1; // + 1 to not remove the updated block
      last_block_rm = (size - 1) / block_size;
    }

    int removed_entries = remove_cached_entries_range(
        *inode, (long)first_block_rm, (long)last_block_rm, state);
    if (removed_entries == -1)
      return -1;
  }

  return 0;
}

int read_cache_fstat(int fd, struct stat *stbuf, LayerContext l) {
  return l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
}

int read_cache_lstat(const char *pathname, struct stat *stbuf, LayerContext l) {
  return l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
}

int read_cache_unlink(const char *pathname, LayerContext l) {

  struct stat stbuf;
  int res = l.next_layers->ops->llstat(pathname, &stbuf, *l.next_layers);
  if (res == -1)
    return -1;

  res = l.next_layers->ops->lunlink(pathname, *l.next_layers);

  if (res != -1) {
    ReadCacheState *state = (ReadCacheState *)l.internal_state;

    InodeInfo *value = g_hash_table_lookup(state->inode_to_info, &stbuf.st_ino);
    // if value is NULL, it means the file was never opened by us, so there
    // isn't anything in cache
    if (value != NULL) {

      // there's no currently opened fds to this path, so it won't go through
      // our close
      if (value->counter == 0) {

        long end_block = stbuf.st_size / (long)state->block_size;
        res = remove_cached_entries_range(stbuf.st_ino, 0, end_block, state);
        g_hash_table_remove(state->fd_to_inode, &stbuf.st_ino);

      }
      // the cached entries will eventually be removed in the close
      else
        value->unlinked = 1;
    }
  }

  return res;
}
