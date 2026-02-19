#include "compression.h"
#include "../../logdef.h"
#include "../../shared/utils/compressor/compressor.h"
#include "compression_utils.h"
#include "sparse_block.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static const LayerOps default_mode_ops = {
    .lpread = compression_pread,
    .lpwrite = compression_pwrite,
    .lftruncate = compression_ftruncate,
    .ltruncate = compression_truncate,
    .lfstat = compression_fstat,
    .llstat = compression_lstat,
};

static const LayerOps sparse_block_mode_ops = {
    .lpread = compression_sparse_block_pread,
    .lpwrite = compression_sparse_block_pwrite,
    .lftruncate = compression_sparse_block_ftruncate,
    .ltruncate = compression_sparse_block_truncate,
    .lfstat = compression_sparse_block_fstat,
    .llstat = compression_sparse_block_lstat,
};

static inline void require_block_size_or_exit(const CompressionConfig *config) {
  if (!config->block_size || config->block_size <= 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_INIT] Block size is not set");
    exit(1);
  }
}

static inline const LayerOps *ops_for_mode(compression_mode_t mode) {
  switch (mode) {
  case COMPRESSION_MODE_SPARSE_BLOCK:
    return &sparse_block_mode_ops;
  case COMPRESSION_MODE_FILE:
  default:
    return &default_mode_ops;
  }
}

static int get_or_set_original_size(int fd, const char *path, LayerContext l,
                                    off_t *original_size);
static int calculate_original_size_from_compressed_file(int fd,
                                                        const char *path,
                                                        LayerContext l,
                                                        off_t *original_size);
static ssize_t read_compressed_data(int fd, const char *path,
                                    const LayerContext *next_layers,
                                    void *compressed_data,
                                    size_t amount_to_read);
static int truncate_compression_file(int fd, const char *path, off_t length,
                                     CompressionState *state, LayerContext l);

/**
 * Compression layer (not ready for use)
 * - compresses data on write, decompresses data on read
 * - supports LZ4 and ZSTD algorithms
 */
LayerContext compression_init(LayerContext *next_layer,
                              const CompressionConfig *config) {
  LayerContext l;
  l.app_context = NULL;

  CompressionState *state = calloc(1, sizeof(CompressionState));
  if (!state) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_INIT] Failed to allocate memory "
              "for compression state");
    exit(1);
  }

  // Initialize fd_to_inode array
  for (int i = 0; i < MAX_FDS; i++) {
    state->fd_to_inode[i].fd = INVALID_FD;
    state->fd_to_inode[i].path = NULL;
    state->fd_to_inode[i].device = 0;
    state->fd_to_inode[i].inode = 0;
  }

  int res =
      compressor_init(&state->compressor, config->algorithm, config->level);
  if (res != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_INIT] Failed to initialize "
              "compressor");
    exit(1);
  }

  state->lock_table = locking_init();
  if (!state->lock_table) {
    free(state);
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_INIT] Failed to initialize lock "
              "table");
    exit(1);
  }

  l.internal_state = state;

  // Create LayerOps structure
  LayerOps *compression_ops = malloc(sizeof(LayerOps));
  if (!compression_ops) {
    free(state);
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_INIT] Failed to allocate memory "
              "for compression "
              "operations");
    exit(1);
  }

  // Select ops based on mode
  state->mode = config->mode;
  if (state->mode == COMPRESSION_MODE_SPARSE_BLOCK) {
    require_block_size_or_exit(config);
    state->block_size = (size_t)config->block_size;
    state->free_space = config->free_space;
  }
  *compression_ops = *ops_for_mode(state->mode);
  compression_ops->lopen = compression_open;
  compression_ops->lclose = compression_close;

  compression_ops->lunlink = compression_unlink;
  compression_ops->lreaddir = compression_readdir;
  compression_ops->lrename = compression_rename;
  compression_ops->lchmod = compression_chmod;
  compression_ops->lfsync = compression_fsync;

  l.ops = compression_ops;
  // TODO: We need to be consistent with the way we handle the next layers
  // This is a deep copy, like how block_align is handling it, but the other
  // layers are not using a deep copy.
  LayerContext *aux = malloc(sizeof(LayerContext));
  memcpy(aux, next_layer, sizeof(LayerContext));
  l.next_layers = aux;
  l.nlayers = 1;

  return l;
}

/**
 * @brief Write to an empty file
 *
 * This function:
 * 1. Allocates a buffer for the new data.
 * 2. Compresses the new data.
 * 3. Writes the compressed data to the file (no need for ftruncate because the
 * file is expected to be empty).
 * 4. Updates the original size of the file in the file_size_mapping hash table.
 *
 * @warning This function is not thread-safe.
 * @warning This function does not check if the file is empty, the caller must
 * before the call.
 *
 * @param fd -> file descriptor to write to
 * @param path -> path of the file to write to
 * @param buffer -> buffer to write
 * @param nbyte -> number of bytes to write
 * @param offset -> offset to write to
 * @param l -> layer context
 * @param new_size -> new size of the file
 * @return ssize_t -> number of bytes written
 */
static ssize_t write_to_empty_file(int fd, char *path, const void *buffer,
                                   size_t nbyte, off_t offset, LayerContext l,
                                   off_t *new_size) {
  CompressionState *state = (CompressionState *)l.internal_state;

  // Calculate total size once with overflow check
  if (offset < 0 || (size_t)offset > (SIZE_MAX - nbyte)) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EMPTY_FILE] File size too large "
              "for memory allocation");
    return INVALID_FD;
  }

  size_t total_size = (size_t)offset + nbyte;
  *new_size = (off_t)total_size;

  // File is empty, allocate buffer for new data
  void *decompressed_data = calloc(1, total_size);
  if (!decompressed_data) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EMPTY_FILE] Failed to allocate "
              "memory for decompressed data");
    return INVALID_FD;
  }

  // Now safe to write
  memcpy((char *)decompressed_data + (size_t)offset, buffer, nbyte);

  // Before compressing, ensure compressed_data is large enough
  size_t max_compressed_size = state->compressor.get_compress_bound(
      total_size, state->compressor.level); // Use total_size

  void *new_compressed_data = malloc(max_compressed_size);
  if (!new_compressed_data) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EMPTY_FILE] Failed to allocate "
              "memory for compressed data");
    free(decompressed_data);
    return INVALID_FD;
  }

  // Now compress safely
  ssize_t new_compressed_size = state->compressor.compress_data(
      decompressed_data, total_size, new_compressed_data,
      max_compressed_size, // Use total_size
      state->compressor.level);
  if (new_compressed_size < 0) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: WRITE_TO_EMPTY_FILE] Failed to compress data");
    free(new_compressed_data);
    free(decompressed_data);
    return INVALID_FD;
  }

  // We write the compressed data to the file
  ssize_t write_size = l.next_layers->ops->lpwrite(
      fd, new_compressed_data, new_compressed_size, 0, *l.next_layers);
  if (write_size < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EMPTY_FILE] Failed to write "
              "compressed data");
    free(new_compressed_data);
    free(decompressed_data);
    return INVALID_FD;
  }
  free(new_compressed_data);
  free(decompressed_data);

  return (ssize_t)nbyte;
}

/**
 * @brief Get the decompressed data from storage

 * @warning This function is not thread-safe.
 *
 * @param fd -> file descriptor to read from
 * @param state -> compression state
 * @param next_layers -> next layers
 * @param decompressed_data -> buffer to read into
 * @param original_size -> original size of the file
 * @return int -> 0 if successful, -1 otherwise
 */
static int get_decompressed_data(int fd, const char *path,
                                 CompressionState *state,
                                 const LayerContext *next_layers,
                                 void *decompressed_data, off_t original_size) {
  struct stat stbuf;
  int res = next_layers->ops->lfstat(fd, &stbuf, *next_layers);
  if (res == -1) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_DECOMPRESSED_DATA] Failed to get "
              "compressed size");
    return -1;
  }
  off_t compressed_size = stbuf.st_size;
  void *compressed_data = malloc((size_t)compressed_size);
  if (!compressed_data) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_DECOMPRESSED_DATA] Failed to allocate "
              "memory for compressed "
              "data");
    return -1;
  }
  // We get the compressed data
  ssize_t read_size = read_compressed_data(
      fd, path, next_layers, compressed_data, (size_t)compressed_size);
  if (read_size < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_DECOMPRESSED_DATA] Failed to read "
              "compressed data from file");
    free(compressed_data);
    return -1;
  }

  if (original_size < 0 || original_size > SIZE_MAX) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_DECOMPRESSED_DATA] File too large for "
              "decompression");
    free(compressed_data);
    return -1;
  }

  size_t size_t_original_size = (size_t)original_size;
  ssize_t decompressed_size = state->compressor.decompress_data(
      compressed_data, compressed_size, decompressed_data,
      &size_t_original_size);
  if (decompressed_size < 0) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: GET_DECOMPRESSED_DATA] Failed to decompress data");
    free(compressed_data);
    return -1;
  }
  free(compressed_data);

  return 0;
}

/**
 * @brief Write to an existing file
 *
 * This function:
 * 1. Reads the whole compressed data stored.
 * 2. Decompresses the data.
 * 3. Writes the buffer data to the decompressed data.
 * 4. Compresses the new data.
 * 5. Truncates the current compressed data stored to 0 bytes.
 * 6. Writes the whole compressed data.
 *
 * @warning This function is not thread-safe.
 *
 * @param fd -> file descriptor to write to
 * @param path -> path of the file to write to
 * @param buffer -> buffer to write
 * @param nbyte -> number of bytes to write
 * @param offset -> offset to write to
 * @param l -> layer context
 * @param original_size -> original size of the file
 * @param new_size -> new size of the decompressed data
 * @return ssize_t -> number of decompressed bytes written
 */
static ssize_t write_to_existing_file(int fd, char *path, const void *buffer,
                                      size_t nbyte, off_t offset,
                                      LayerContext l, off_t original_size,
                                      off_t *new_size) {
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next = l.next_layers;

  if (original_size < 0 || original_size > SIZE_MAX) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] File too large for "
              "decompression");
    return INVALID_FD;
  }

  void *decompressed_data = malloc((size_t)original_size);
  if (!decompressed_data) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to allocate "
              "memory for "
              "decompressed data");
    return INVALID_FD;
  }

  int res = get_decompressed_data(fd, path, state, next, decompressed_data,
                                  original_size);
  if (res != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to get "
              "decompressed data");
    free(decompressed_data);
    return INVALID_FD;
  }

  // Calculate total size once with overflow check
  if (offset < 0 || (size_t)offset > (SIZE_MAX - nbyte)) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] File size too large "
              "for memory allocation");
    free(decompressed_data);
    return INVALID_FD;
  }
  size_t end_position = (size_t)offset + nbyte;

  if (end_position > (size_t)original_size) {
    void *new_buf = realloc(decompressed_data, end_position);
    if (!new_buf) {
      ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to "
                "reallocate memory for "
                "decompressed data");
      free(decompressed_data);
      return INVALID_FD;
    }
    // Only zero-fill the gap if there is one
    if (offset > original_size) {
      memset((char *)new_buf + original_size, 0, offset - original_size);
    }
    decompressed_data = new_buf;
    *new_size = (off_t)end_position;
  } else {
    *new_size = original_size;
  }

  // Now safe to write
  memcpy((char *)decompressed_data + offset, buffer, nbyte);

  size_t size_t_new_size = (size_t)*new_size;

  // Before compressing, ensure compressed_data buffer is large enough
  size_t max_compressed_size = state->compressor.get_compress_bound(
      size_t_new_size, state->compressor.level);

  void *new_compressed_data = malloc(max_compressed_size);
  if (!new_compressed_data) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to "
              "reallocate memory for compressed "
              "data");
    free(decompressed_data);
    return INVALID_FD;
  }

  // Now compress safely
  ssize_t new_compressed_size = state->compressor.compress_data(
      decompressed_data, size_t_new_size, new_compressed_data,
      max_compressed_size, state->compressor.level);
  if (new_compressed_size < 0) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to compress data");
    free(new_compressed_data);
    free(decompressed_data);
    return INVALID_FD;
  }
  free(decompressed_data);

  // We truncate the file to 0 bytes
  // This is necessary to avoid the file being corrupted by the new data
  // For example, if the compressed data is currently stored with 101 bytes
  // long and we write 100 bytes, the compressed data will still be 101 bytes
  // long. Corruption will happen if we don't truncate the file.
  res = next->ops->lftruncate(fd, 0, *next);
  if (res < 0) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to truncate file");
    free(new_compressed_data);
    return INVALID_FD;
  }

  // We write the compressed data to the file
  // We write from the beginning of the file because its a compressed file,
  // random writing is not possible.
  ssize_t write_size = next->ops->lpwrite(fd, new_compressed_data,
                                          new_compressed_size, 0, *next);
  if (write_size < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: WRITE_TO_EXISTING_FILE] Failed to write "
              "compressed data");
    free(new_compressed_data);
    return INVALID_FD;
  }
  free(new_compressed_data);

  return (ssize_t)nbyte;
}

/**
 * @brief Write data to a file
 *
 * This function:
 * 1. Reads the whole compressed data stored.
 * 2. Decompresses the data.
 * 3. Writes the buffer data to the decompressed data.
 * 4. Compresses the new data.
 * 5. Writes the whole compressed data.
 * 6. Updates the original size of the file in the file_size_mapping hash table.
 *
 * @warning This function is not thread-safe.
 * @param fd -> file descriptor to write to
 * @param buffer -> buffer to write
 * @param nbyte -> number of bytes to write
 * @param offset -> offset to write to
 * @param l -> layer context
 * @return ssize_t -> number of bytes written
 */
ssize_t compression_pwrite(int fd, const void *buffer, size_t nbyte,
                           off_t offset, LayerContext l) {
  if (!validate_compression_fd_offset_and_nbyte(
          fd, offset, nbyte, "COMPRESSION_LAYER: COMPRESSION_PWRITE")) {
    return INVALID_FD;
  }
  if (nbyte == 0) {
    return 0;
  }

  CompressionState *state = (CompressionState *)l.internal_state;

  char *path = state->fd_to_inode[fd].path;
  if (!path) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PWRITE] File path not found", NULL,
        NULL);
    return INVALID_FD;
  }

  if (locking_acquire_write(state->lock_table, path) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PWRITE] Failed "
                               "to acquire write lock on file",
                               NULL, NULL);
    return INVALID_FD;
  }

  off_t original_size;
  if (get_or_set_original_size(fd, path, l, &original_size) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PWRITE] Failed to get original size",
        state->lock_table, path);
    return INVALID_FD;
  }

  off_t new_size;
  ssize_t bytes_written;
  if (original_size == 0) {
    bytes_written =
        write_to_empty_file(fd, path, buffer, nbyte, offset, l, &new_size);
    if (bytes_written < 0) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PWRITE] "
                                 "Failed to write to empty file",
                                 state->lock_table, path);
      return INVALID_FD;
    }
  } else {
    bytes_written = write_to_existing_file(fd, path, buffer, nbyte, offset, l,
                                           original_size, &new_size);
    if (bytes_written < 0) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PWRITE] "
                                 "Failed to write to existing file",
                                 state->lock_table, path);
      return INVALID_FD;
    }
  }

  // We update the original size of the file
  if (set_original_size_in_file_size_mapping(path, new_size, l) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PWRITE] Failed "
                               "to set original size in mapping",
                               state->lock_table, path);
    return INVALID_FD;
  }

  locking_release(state->lock_table, path);

  return bytes_written;
}

/**
 * @brief Read data from a file
 *
 * This function:
 * 1. Reads the whole compressed data stored.
 * 2. Decompresses the data.
 * 3. Copies the wanted data to the buffer.
 *
 * It is responsibility of the caller to provide a buffer large enough to read
 * the data (nbyte).
 *
 * @param fd -> file descriptor to read from
 * @param buffer -> buffer to read into
 * @param nbyte -> number of bytes to read
 * @param offset -> offset to read from
 * @param l -> layer context
 * @return ssize_t -> number of bytes read of the original uncompressed data
 */
ssize_t compression_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                          LayerContext l) {
  if (!validate_compression_fd_offset_and_nbyte(
          fd, offset, nbyte, "COMPRESSION_LAYER: COMPRESSION_PREAD")) {
    return INVALID_FD;
  }
  if (nbyte == 0) {
    return 0;
  }

  CompressionState *state = (CompressionState *)l.internal_state;

  const char *path = state->fd_to_inode[fd].path;
  if (!path) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PREAD] File path not found", NULL,
        NULL);
    return INVALID_FD;
  }

  if (locking_acquire_write(state->lock_table, path) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed "
                               "to acquire write lock on file",
                               NULL, NULL);
    return INVALID_FD;
  }

  off_t original_size;
  if (get_or_set_original_size(fd, path, l, &original_size) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed to get original size",
        state->lock_table, path);
    return INVALID_FD;
  }

  size_t safe_original_size = (size_t)original_size;

  // If we are trying to read past the end of the file, we return 0
  if (offset >= safe_original_size) {
    locking_release(state->lock_table, path);
    return 0;
  }

  size_t bytes_to_read = nbyte;
  if (offset + nbyte > safe_original_size) {
    bytes_to_read = safe_original_size - offset;
  }

  struct stat stbuf;
  int res = l.next_layers->ops->lfstat(fd, &stbuf, *l.next_layers);
  if (res == -1 || stbuf.st_size < 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed to get compressed size",
        state->lock_table, path);
    return INVALID_FD;
  }
  off_t compressed_size = stbuf.st_size;

  void *compressed_data = malloc(compressed_size);
  if (!compressed_data) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed "
                               "to allocate memory for compressed data",
                               state->lock_table, path);
    return INVALID_FD;
  }

  // We read the complete compressed data
  ssize_t read_size = read_compressed_data(fd, path, l.next_layers,
                                           compressed_data, compressed_size);
  if (read_size < 0) {
    free(compressed_data);
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed to read compressed data",
        state->lock_table, path);
    return INVALID_FD;
  }

  void *decompressed_data = malloc(safe_original_size);
  if (!decompressed_data) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed "
                               "to allocate memory for decompressed data",
                               state->lock_table, path);
    free(compressed_data);
    return INVALID_FD;
  }

  ssize_t decompressed_size = state->compressor.decompress_data(
      compressed_data, compressed_size, decompressed_data, &safe_original_size);
  if (decompressed_size < 0) {
    free(compressed_data);
    free(decompressed_data);
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: COMPRESSION_PREAD] Failed to decompress data",
        state->lock_table, path);
    return INVALID_FD;
  }

  memcpy(buffer, (char *)decompressed_data + offset, bytes_to_read);
  free(compressed_data);
  free(decompressed_data);
  locking_release(state->lock_table, path);

  // We should be safe to cast to ssize_t because we checked that nbyte is less
  // than SSIZE_MAX. And nbytes is less or equal to the bytes_to_read.
  return (ssize_t)bytes_to_read;
}

/**
 * @brief Open a file and map it to a file descriptor
 *
 * In this layer, we create a mapping between the file descriptor and the
 * pathname to be able to later use that fd to get the original size of the file
 * with the FileSizeMapping hash table.
 *
 * @param pathname -> pathname of the file to open
 * @param flags -> flags to open the file with
 * @param mode -> mode to open the file with
 * @param l -> layer context
 * @return int -> file descriptor if successful, INVALID_FD otherwise
 */
int compression_open(const char *pathname, int flags, mode_t mode,
                     LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next = l.next_layers;

  if (!pathname) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Pathname is NULL");
    return INVALID_FD;
  }

  // Track whether we successfully acquired a lock (not just the flag)
  int lock_acquired = 0;

  if (flags & O_TRUNC) {
    if (locking_acquire_write(state->lock_table, pathname) != 0) {
      ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to acquire write "
                "lock on file");
      return INVALID_FD;
    }
    lock_acquired = 1; // Successfully acquired lock
  }

  int file_fd = next->ops->lopen(pathname, flags, mode, *next);

  if (file_fd < 0) {
    if (lock_acquired) {
      locking_release(state->lock_table, pathname);
    }
    return file_fd;
  }

  if (file_fd >= MAX_FDS) {
    next->ops->lclose(file_fd, *next);
    if (lock_acquired) {
      locking_release(state->lock_table, pathname);
    }
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] File descriptor %d "
              "exceeds MAX_FDS (%d)",
              file_fd, MAX_FDS);
    return INVALID_FD;
  }

  // Remove any existing mapping for this fd (in case of reuse)
  // This is safe to call even if no mapping exists
  fd_to_inode_remove(state, file_fd);

  // Get device and inode for the file
  struct stat st_key;
  if (next->ops->lfstat(file_fd, &st_key, *next) != 0) {
    next->ops->lclose(file_fd, *next);
    if (lock_acquired) {
      locking_release(state->lock_table, pathname);
    }
    ERROR_MSG(
        "[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to get stat of file");
    return INVALID_FD;
  }

  // Insert fd -> (device, inode, path) mapping into hash table
  if (fd_to_inode_insert(state, file_fd, st_key.st_dev, st_key.st_ino,
                         pathname) != 0) {
    next->ops->lclose(file_fd, *next);
    if (lock_acquired) {
      locking_release(state->lock_table, pathname);
    }
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to insert fd "
              "mapping");
    return INVALID_FD;
  }

  if (flags & O_CREAT) {
    if (st_key.st_size == 0) {
      int acquired_here = 0;
      if (!lock_acquired) {
        if (locking_acquire_write(state->lock_table, pathname) != 0) {
          ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to acquire "
                    "write lock on file");
          return INVALID_FD;
        }
        acquired_here = 1;
      }
      // We can delete the mapping in case it was not properly removed before
      remove_compressed_file_mapping(st_key.st_dev, st_key.st_ino, l);
      if (create_compressed_file_mapping(st_key.st_dev, st_key.st_ino, 0, l) !=
          0) {
        if (acquired_here) {
          locking_release(state->lock_table, pathname);
        }
        ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to create "
                  "compressed file mapping");
        return INVALID_FD;
      }
      if (acquired_here) {
        locking_release(state->lock_table, pathname);
      }
    }
  }

  // Handle O_TRUNC flag after any O_CREAT mapping creation
  if (lock_acquired) {
    CompressedFileMapping *file_mapping =
        get_compressed_file_mapping(st_key.st_dev, st_key.st_ino, state);
    if (file_mapping == NULL) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed "
                                 "to get compressed file mapping",
                                 state->lock_table, pathname);
      return INVALID_FD;
    }
    file_mapping->logical_eof = 0;
    if (shrink_block_index(file_mapping, 0) != 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to shrink block index",
          state->lock_table, pathname);
      return INVALID_FD;
    }
    locking_release(state->lock_table, pathname);
  }

  // Increment the open counter for this file
  if (increment_open_counter(st_key.st_dev, st_key.st_ino, l) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to increment open "
              "counter");
    return INVALID_FD;
  }

  // Rebuild block mapping from storage for sparse_block mode after
  // crash/restart Only rebuild if mapping doesn't already exist (i.e., after a
  // crash/restart)
  if (state->mode == COMPRESSION_MODE_SPARSE_BLOCK && !(flags & O_CREAT) &&
      !(flags & O_TRUNC)) {
    // Check if block mapping already exists (cheap check first)
    CompressedFileMapping *existing_mapping =
        get_compressed_file_mapping(st_key.st_dev, st_key.st_ino, state);
    // Check if mapping doesn't exist or is empty, or if file has data
    if ((!existing_mapping || existing_mapping->num_blocks == 0) &&
        st_key.st_size > 0) {
      // File has data, rebuild mapping from storage
      DEBUG_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Rebuilding block "
                "mapping for dev=%lu, ino=%lu",
                (unsigned long)st_key.st_dev, (unsigned long)st_key.st_ino);
      if (rebuild_block_mapping_from_storage(file_fd, st_key.st_dev,
                                             st_key.st_ino, l) < 0) {
        ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_OPEN] Failed to rebuild "
                  "block mapping");
        return INVALID_FD;
      }
    }
  }
  return file_fd;
}

/**
 * @brief Close a file and remove the mapping between the file descriptor and
 * the pathname.
 *
 * @param fd -> file descriptor to close
 * @param l -> layer context
 * @return int -> INVALID_FD or the return value of the lower layer.
 */
int compression_close(int fd, LayerContext l) {
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CLOSE] File descriptor %d is "
              "not valid",
              fd);
    return INVALID_FD;
  }

  const LayerContext *next = l.next_layers;
  CompressionState *state = (CompressionState *)l.internal_state;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CLOSE] File descriptor %d not "
              "found",
              fd);
    return INVALID_FD;
  }

  // Extract values before removing entry
  dev_t device = entry->device;
  ino_t inode = entry->inode;
  const char *path = entry->path;

  char *path_copy = strdup(path);
  if (!path_copy) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CLOSE] Failed to allocate "
              "memory for path copy");
    return INVALID_FD;
  }

  // Now acquire lock using the copied path to update internal state
  if (locking_acquire_write(state->lock_table, path_copy) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CLOSE] Failed to acquire write "
              "lock on file: %s",
              path_copy);
    free(path_copy);
    return INVALID_FD;
  }

  fd_to_inode_remove(state, fd);

  int result = next->ops->lclose(fd, *next);
  if (result < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CLOSE] Failed "
              "to close file descriptor in layer below, fd: %d",
              fd);
    locking_release(state->lock_table, path_copy);
    free(path_copy);
    return result;
  }

  // Decrement the open counter for this file
  decrement_open_counter(device, inode, l);

  // Check if we should clean up the mapping if the file was unlinked
  // and this was the last fd
  if (should_cleanup_mapping(device, inode, l) == 1) {
    remove_compressed_file_mapping(device, inode, l);
  }

  locking_release(state->lock_table, path_copy);
  free(path_copy);

  return result;
}

void compression_destroy(LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (!state) {
    return;
  }

  // Clean up all fd_to_inode entries
  for (int i = 0; i < MAX_FDS; i++) {
    if (state->fd_to_inode[i].path) {
      free(state->fd_to_inode[i].path);
      state->fd_to_inode[i].path = NULL;
    }
  }

  // Clean up all file mappings (unified mapping)
  CompressedFileMapping *entry, *tmp;
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_ITER(hh, state->file_mapping, entry, tmp) {
    // Free dynamically allocated arrays (sparse_block fields)
    if (entry->sizes) {
      free(entry->sizes);
    }
    if (entry->is_uncompressed) {
      free(entry->is_uncompressed);
    }
    // NOLINTNEXTLINE(bugprone-casting-through-void)
    HASH_DEL(state->file_mapping, entry);
    free(entry);
  }

  if (state) {
    free(state);
  }

  if (l.ops) {
    free(l.ops);
  }
}

/**
 * @brief Truncate a file

 * This function:
 * 1. Gets the original size of the file.
 * 2. If the new length is equal to the original size, we don't do anything.
 * 3. If the new length is greater than the original size, we decompress the
 * current data and zero the new region.
 * 4. If the new length is less than the original size, we decompress the
 * current
 * data and truncate the decompressed data to the new length.
 * 5. Compresses the new data.
 * 6. Truncates the current compressed data stored to 0 bytes.
 * 6. Writes the new compressed data to storage.
 * 7. Updates the original/decompressed size of the file in the
 * file_size_mapping hash table.
 *
 * @param fd -> file descriptor to truncate
 * @param length -> new length of the file
 * @param l -> layer context
 * @return int -> 0 if successful, INVALID_FD otherwise
 */
int compression_ftruncate(int fd, off_t length, LayerContext l) {
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] File descriptor %d "
              "is not valid",
              fd);
    return INVALID_FD;
  }

  CompressionState *state = (CompressionState *)l.internal_state;
  char *path = state->fd_to_inode[fd].path;
  if (!path) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] File descriptor %d "
              "not found",
              fd);
    return INVALID_FD;
  }

  return truncate_compression_file(fd, path, length, state, l);
}

int compression_truncate(const char *path, off_t length, LayerContext l) {
  if (!path) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_TRUNCATE] Path is NULL");
    return INVALID_FD;
  }
  CompressionState *state = (CompressionState *)l.internal_state;

  int fd = l.next_layers->ops->lopen(path, O_RDONLY, 0, *l.next_layers);
  if (fd < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_TRUNCATE] Failed to open file");
    return INVALID_FD;
  }

  int res = truncate_compression_file(fd, path, length, state, l);
  l.next_layers->ops->lclose(fd, *l.next_layers);
  return res;
}

int compression_fstat(int fd, struct stat *stbuf, LayerContext l) {
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FSTAT] File descriptor %d "
              "is not valid",
              fd);
    return INVALID_FD;
  }

  CompressionState *state = (CompressionState *)l.internal_state;
  const char *path = state->fd_to_inode[fd].path;
  if (!path) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: COMPRESSION_FSTAT] File descriptor %d not found",
        fd);
    return INVALID_FD;
  }

  if (locking_acquire_read(state->lock_table, path) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FSTAT] Failed to acquire read "
              "lock on file %s",
              path);
    return INVALID_FD;
  }

  int res = l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
  if (res != 0) {
    locking_release(state->lock_table, path);
    return res;
  }

  // Only process regular files (not directories, symlinks, devices, etc.)
  if (!S_ISREG(stbuf->st_mode)) {
    locking_release(state->lock_table, path);
    return res;
  }

  off_t original_size;
  if (get_or_set_original_size(fd, path, l, &original_size) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FSTAT] Failed "
                               "to get or set original size of file",
                               state->lock_table, path);
    return INVALID_FD;
  }

  stbuf->st_size = original_size;
  locking_release(state->lock_table, path);
  return res;
}

int compression_lstat(const char *pathname, struct stat *stbuf,
                      LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (locking_acquire_read(state->lock_table, pathname) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed to acquire read "
              "lock on file %s",
              pathname);
    return INVALID_FD;
  }

  // First, get the stat info from the lower layer
  int res = l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
  if (res != 0) {
    locking_release(state->lock_table, pathname);
    return res;
  }

  // Only process regular files (not directories, symlinks, devices, etc.)
  if (!S_ISREG(stbuf->st_mode)) {
    locking_release(state->lock_table, pathname);
    return res;
  }

  // Now handle compressed files
  off_t original_size;
  // We will fist try to get the original size from the file_size_mapping hash
  // table.
  if (get_original_size_from_mapping(pathname, l, &original_size) != 0) {
    // If the file is not found in the file_size_mapping hash table, we need to
    // open the file and calculate the original size from the compressed file.
    int fd = l.next_layers->ops->lopen(pathname, O_RDONLY, 0, *l.next_layers);
    if (!is_valid_compression_fd(fd)) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed to open file",
          state->lock_table, pathname);
      return INVALID_FD;
    }

    if (get_or_set_original_size(fd, pathname, l, &original_size) != 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed "
          "to get or set original size of file",
          state->lock_table, pathname);
      l.next_layers->ops->lclose(fd, *l.next_layers);
      return INVALID_FD;
    }
    l.next_layers->ops->lclose(fd, *l.next_layers);
  }

  stbuf->st_size = original_size;
  locking_release(state->lock_table, pathname);
  return res;
}

/**
 * @brief Unlink a file
 *
 * This function removes the directory entry for the file. However, following
 * UNIX semantics, the actual file data and our internal mappings are preserved
 * as long as there are open file descriptors. The mapping will be cleaned up
 * automatically when the last fd is closed.
 *
 * @param pathname -> path of the file to unlink
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if the file is not found
 */
int compression_unlink(const char *pathname, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (locking_acquire_write(state->lock_table, pathname) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_UNLINK] Failed to acquire write "
              "lock on file %s",
              pathname);
    return INVALID_FD;
  }

  // Get the device and inode before unlinking
  struct stat stbuf_before;
  if (l.next_layers->ops->llstat(pathname, &stbuf_before, *l.next_layers) !=
      0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_UNLINK] Failed to stat file "
              "before unlink: %s",
              pathname);
    locking_release(state->lock_table, pathname);
    return INVALID_FD;
  }

  int res = l.next_layers->ops->lunlink(pathname, *l.next_layers);
  if (res != 0) {
    locking_release(state->lock_table, pathname);
    return res;
  }

  // Mark as unlinked and get the current open counter
  int open_count = 0;
  if (mark_as_unlinked(stbuf_before.st_dev, stbuf_before.st_ino, &open_count,
                       l) == 0) {
    // If no file descriptors are open, clean up both mappings immediately
    if (open_count == 0) {
      DEBUG_MSG("[COMPRESSION_LAYER: COMPRESSION_UNLINK] Removing compressed "
                "file mapping for dev=%lu, ino=%lu",
                (unsigned long)stbuf_before.st_dev,
                (unsigned long)stbuf_before.st_ino);
      if (remove_compressed_file_mapping(stbuf_before.st_dev,
                                         stbuf_before.st_ino, l) != 0) {
        ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_UNLINK] Failed to remove "
                  "compressed file mapping for dev=%lu, ino=%lu",
                  (unsigned long)stbuf_before.st_dev,
                  (unsigned long)stbuf_before.st_ino);
        // We don't return an error because the actual unlink operation
        // succeeded
      }
    }
  } else {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_UNLINK] Failed to mark file as "
              "unlinked: %s",
              pathname);
  }

  locking_release(state->lock_table, pathname);
  return res;
}

int compression_rename(const char *from, const char *to, unsigned int flags,
                       LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (locking_acquire_write(state->lock_table, from) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_RENAME] Failed to acquire write "
              "lock on file %s",
              from);
    return INVALID_FD;
  }
  int result = l.next_layers->ops->lrename(from, to, flags, *l.next_layers);
  locking_release(state->lock_table, from);
  return result;
}

int compression_chmod(const char *path, mode_t mode, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (locking_acquire_write(state->lock_table, path) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_CHMOD] Failed to acquire write "
              "lock on file %s",
              path);
    return -1;
  }
  int result = l.next_layers->ops->lchmod(path, mode, *l.next_layers);
  locking_release(state->lock_table, path);
  return result;
}

int compression_fsync(int fd, int isdatasync, LayerContext l) {
  if (!is_valid_compression_fd(fd)) {
    return INVALID_FD;
  }
  const LayerContext *next = l.next_layers;
  if (next && next->ops && next->ops->lfsync) {
    return next->ops->lfsync(fd, isdatasync, *next);
  }
  return -ENOSYS;
}

/**
 * @brief Get the original size of a file from the FileSizeMapping hash table
 * or calculate it from the compressed file
 *
 * We first check if we have the original size stored in memory in
 * file_size_mapping. If we don't, we calculate the original size from the
 * compressed file and store it in the file_size_mapping.
 *
 * @warning This function modifies the hash table and is not thread-safe.
 *          Caller must ensure proper locking before calling this function.
 *          Recommended to have a write lock on the file before calling this
 *          function because it potentially modifies the hash table.
 *
 * @param fd -> file descriptor to get the size of
 * @param path -> path of the file
 * @param l -> layer context
 * @param original_size -> pointer to the original size
 * @return int -> 0 if successful, -1 if the file is not found
 */
static int get_or_set_original_size(int fd, const char *path, LayerContext l,
                                    off_t *original_size) {
  if (get_original_size_from_mapping(path, l, original_size) == 0) {
    return 0;
  }
  if (calculate_original_size_from_compressed_file(fd, path, l,
                                                   original_size) != 0) {
    return -1;
  }
  if (set_original_size_in_file_size_mapping(path, *original_size, l) != 0) {
    return -1;
  }
  return 0;
}

/**
 * @brief Calculate the original size of a file from the compressed file
 *
 * @param fd -> file descriptor to get the size of
 * @param path -> path of the file
 * @param l -> layer context
 * @param original_size -> pointer to the original size
 * @return int -> 0 if successful, -1 if the file is not found
 */
static int calculate_original_size_from_compressed_file(int fd,
                                                        const char *path,
                                                        LayerContext l,
                                                        off_t *original_size) {
  const LayerContext *next = l.next_layers;
  CompressionState *state = (CompressionState *)l.internal_state;

  // We first get the compressed size
  struct stat stbuf;
  int res = next->ops->lfstat(fd, &stbuf, *next);
  if (res == -1) {
    ERROR_MSG("[COMPRESSION_LAYER: "
              "COMPRESSION_CALCULATE_ORIGINAL_SIZE_FROM_COMPRESSED_FILE] "
              "Failed to get compressed size of file %s",
              path);
    return -1;
  }
  off_t compressed_size = stbuf.st_size;

  // We get the maximum header size that a frame can have for the compressor
  // used.
  size_t max_header_size = state->compressor.get_max_header_size();

  size_t approximated_compressed_size =
      (size_t)(compressed_size < max_header_size ? compressed_size
                                                 : max_header_size);

  // We only read the compressed data if the approximated compressed size is
  // greater than 0.
  if (approximated_compressed_size > 0) {
    void *compressed_data = malloc(approximated_compressed_size);
    if (!compressed_data) {
      ERROR_MSG("[COMPRESSION_LAYER: "
                "COMPRESSION_CALCULATE_ORIGINAL_SIZE_FROM_COMPRESSED_FILE] "
                "Failed to allocate memory for "
                "compressed data");
      return -1;
    }

    ssize_t read_size = read_compressed_data(fd, path, next, compressed_data,
                                             approximated_compressed_size);
    if (read_size < 0) {
      ERROR_MSG("[COMPRESSION_LAYER: "
                "COMPRESSION_CALCULATE_ORIGINAL_SIZE_FROM_COMPRESSED_FILE] "
                "Failed to read compressed data from file %s",
                path);
      free(compressed_data);
      return -1;
    }

    // We get the original size with the compressor, which will read the frame
    // header to get the original size.
    off_t compressor_original_size =
        state->compressor.get_original_file_size(compressed_data, read_size);

    if (compressor_original_size < 0) {
      ERROR_MSG("[COMPRESSION_LAYER: "
                "COMPRESSION_CALCULATE_ORIGINAL_SIZE_FROM_COMPRESSED_FILE] "
                "Failed to get original size of file %s",
                path);
      free(compressed_data);
      return -1;
    }
    free(compressed_data);
    *original_size = compressor_original_size;
  } else {
    *original_size = 0;
  }
  return 0;
}

/**
 * @brief Read compressed file
 *
 * We try to read the compressed data from the original file descriptor. If it
 * fails, we open a new file descriptor and read the compressed data from it.
 * We close the file after reading the data. This is necessary because FUSE and
 * other use cases might open the file with write flag, not allowing to read
 * the compressed data, which we need to make writes to the file. We also need
 * to read the compressed data from the original file descriptor because the
 * file might be opened with write flag, not allowing to read the compressed
 * data, which we need to make writes to the file.
 *
 * @param fd -> file descriptor to read from
 * @param path -> path of the file
 * @param next_layers -> next layers
 * @param compressed_data -> pointer to the compressed data
 * @param amount_to_read -> amount of bytes to read (usually the compressed
 * size)
 * @return ssize_t -> number of bytes read
 */
static ssize_t read_compressed_data(int fd, const char *path,
                                    const LayerContext *next_layers,
                                    void *compressed_data,
                                    size_t amount_to_read) {

  // Try using the original fd first
  ssize_t read_size = next_layers->ops->lpread(fd, compressed_data,
                                               amount_to_read, 0, *next_layers);
  if (read_size >= 0) {
    return read_size;
  }

  // If the original fd fails, we open a new one
  int new_fd = next_layers->ops->lopen(path, O_RDONLY, 0, *next_layers);
  if (new_fd < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_COMPRESSED_DATA_WITH_NEW_FD] Failed to "
              "open file");
    return -1;
  }
  read_size = next_layers->ops->lpread(new_fd, compressed_data, amount_to_read,
                                       0, *next_layers);
  if (read_size < 0) {
    ERROR_MSG("[COMPRESSION_LAYER: GET_COMPRESSED_DATA_WITH_NEW_FD] Failed to "
              "read compressed data");
    next_layers->ops->lclose(new_fd, *next_layers);
    return -1;
  }
  next_layers->ops->lclose(new_fd, *next_layers);
  return read_size;
}

/**
 * @brief Read directory operation for compression layer
 *
 * Passes through the readdir request to the next layer.
 *
 * @param path Directory path to read
 * @param buf Buffer for directory entries
 * @param filler Function to fill directory entries
 * @param offset Offset in directory
 * @param fi File info
 * @param flags Read directory flags
 * @param l Layer context
 * @return 0 on success, negative error code on failure
 */
int compression_readdir(const char *path, void *buf,
                        int (*filler)(void *buf, const char *name,
                                      const struct stat *stbuf, off_t off,
                                      unsigned int flags),
                        off_t offset, struct fuse_file_info *fi,
                        unsigned int flags, LayerContext l) {

  // Validate input parameters
  if (!path || !buf || !filler) {
    ERROR_MSG("[COMPRESSION_READDIR] Invalid parameters");
    return -EINVAL;
  }

  // Pass through to the next layer
  if (l.next_layers && l.next_layers->ops && l.next_layers->ops->lreaddir) {
    return l.next_layers->ops->lreaddir(path, buf, filler, offset, fi, flags,
                                        *l.next_layers);
  }

  ERROR_MSG("[COMPRESSION_READDIR] No next layer readdir available");
  return -ENOSYS;
}

/**
 * @brief Truncate a compressed file
 *
 * This function is used to truncate a compressed file. It will get the original
 * size of the file, truncate the file to the new length, and compress the new
 * data. It will also update the original size of the file in the
 * file_size_mapping hash table.
 *
 * @warning This function acquires a write lock on the file.
 *
 * @param fd -> file descriptor to truncate
 * @param path -> path of the file
 * @param length -> new length of the file
 * @param state -> state of the compression layer
 * @param l -> layer context
 * @return int -> 0 if successful, INVALID_FD otherwise
 */
static int truncate_compression_file(int fd, const char *path, off_t length,
                                     CompressionState *state, LayerContext l) {
  if (locking_acquire_write(state->lock_table, path) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to acquire "
              "write lock on file %s",
              path);
    return INVALID_FD;
  }

  const LayerContext *next_layers = l.next_layers;

  off_t original_size;
  if (get_or_set_original_size(fd, path, l, &original_size) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                               "Failed to get or set original size of file",
                               state->lock_table, path);
    return INVALID_FD;
  }

  size_t safe_original_size = (size_t)original_size;

  // If the new size is the same as the original size, we don't need to do
  // anything.
  if (safe_original_size == length) {
    locking_release(state->lock_table, path);
    return 0;
  }

  // If the length to truncate is 0, we truncate the file to 0 bytes.
  if (length == 0) {
    int res = next_layers->ops->lftruncate(fd, length, *next_layers);
    if (res < 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to truncate file",
          state->lock_table, path);
      return INVALID_FD;
    }
  } else {
    void *decompressed_data = NULL;

    // If the original size is 0, we need to allocate a zero-filled buffer
    // because the file is empty. And we don't need to get the decompressed data
    // from storage.
    if (safe_original_size == 0) {
      decompressed_data = calloc(1, length);
      if (!decompressed_data) {
        error_msg_and_release_lock(
            "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to allocate "
            "memory for decompressed data",
            state->lock_table, path);
        return INVALID_FD;
      }
    } else {
      decompressed_data = malloc(safe_original_size);
      if (!decompressed_data) {
        error_msg_and_release_lock(
            "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to allocate "
            "memory for decompressed data",
            state->lock_table, path);
        return INVALID_FD;
      }

      int res = get_decompressed_data(fd, path, state, next_layers,
                                      decompressed_data, original_size);
      if (res != 0) {
        error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                                   "Failed to get decompressed data",
                                   state->lock_table, path);
        free(decompressed_data);
        return INVALID_FD;
      }

      // Resize buffer to the new size
      void *tmp = realloc(decompressed_data, length);
      if (!tmp) {
        free(decompressed_data);
        error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                                   "Failed to realloc buffer",
                                   state->lock_table, path);
        return INVALID_FD;
      }
      decompressed_data = tmp;

      if (length > safe_original_size) {
        // Zero-fill the new region
        memset((char *)decompressed_data + safe_original_size, 0,
               length - safe_original_size);
      }
    }

    // Before compressing, ensure compressed_data buffer is large enough
    size_t max_compressed_size =
        state->compressor.get_compress_bound(length, state->compressor.level);

    void *new_compressed_data = malloc(max_compressed_size);
    if (!new_compressed_data) {
      ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to "
                "reallocate memory for compressed "
                "data");
      free(decompressed_data);
      return INVALID_FD;
    }

    // Now compress safely
    ssize_t new_compressed_size = state->compressor.compress_data(
        decompressed_data, length, new_compressed_data, max_compressed_size,
        state->compressor.level);
    if (new_compressed_size < 0) {
      ERROR_MSG(
          "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to compress data");
      free(new_compressed_data);
      free(decompressed_data);
      return INVALID_FD;
    }
    free(decompressed_data);

    // We truncate the file to 0 bytes
    // This is necessary to avoid the file being corrupted by the new data
    // For example, if the compressed data is currently stored with 101 bytes
    // long and we write 100 bytes, the compressed data will still be 101 bytes
    // long. Corruption will happen if we don't truncate the file.
    int res = next_layers->ops->lftruncate(fd, 0, *next_layers);
    if (res < 0) {
      ERROR_MSG(
          "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to truncate file");
      return INVALID_FD;
    }

    // We write the compressed data to the file
    // We write from the beginning of the file because its a compressed file,
    // random writing is not possible.
    ssize_t write_size = next_layers->ops->lpwrite(
        fd, new_compressed_data, new_compressed_size, 0, *next_layers);
    if (write_size < 0) {
      ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to write "
                "compressed data");
      free(new_compressed_data);
      return INVALID_FD;
    }
    free(new_compressed_data);
  }

  struct stat stbuf;
  if (l.next_layers->ops->llstat(path, &stbuf, *l.next_layers) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                               "Failed to stat path for logical size update",
                               state->lock_table, path);
    return INVALID_FD;
  }
  if (set_logical_eof_in_mapping(stbuf.st_dev, stbuf.st_ino, length, l) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                               "Failed to set original size in mapping",
                               state->lock_table, path);
    return INVALID_FD;
  }
  locking_release(state->lock_table, path);

  return 0;
}
