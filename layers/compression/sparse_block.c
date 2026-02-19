#define _GNU_SOURCE
#include "sparse_block.h"
#include "../../logdef.h"
#include "compression_utils.h"
#include <fcntl.h>
#include <linux/falloc.h>

// Forward declaration of helper functions
static int compress_or_store_raw(CompressionState *state, const void *data,
                                 size_t data_size, void **out_buffer,
                                 size_t *out_size, int *out_is_uncompressed);

ssize_t compression_sparse_block_pwrite(int fd, const void *buffer,
                                        size_t nbyte, off_t offset,
                                        LayerContext l) {
  if (!validate_compression_fd_offset_and_nbyte(
          fd, offset, nbyte, "COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE")) {
    return INVALID_FD;
  }

  CompressionState *state = (CompressionState *)l.internal_state;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] File descriptor not found",
        NULL, NULL);
    return INVALID_FD;
  }
  const char *path = entry->path;

  if (locking_acquire_write(state->lock_table, path) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to acquire write lock",
        NULL, NULL);
    return INVALID_FD;
  }

  // Resolve (device,inode) for inode-keyed mappings
  dev_t device;
  ino_t inode;
  if (get_file_key_from_fd(fd, l, &device, &inode) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to get file key", NULL,
        NULL);
    return INVALID_FD;
  }

  CompressedFileMapping *block_index =
      get_compressed_file_mapping(device, inode, state);
  if (!block_index) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Missing block index mapping",
        state->lock_table, path);
    return INVALID_FD;
  }

  const size_t block_size = state->block_size;

  if (nbyte == 0) {
    locking_release(state->lock_table, path);
    return 0;
  }

  size_t num_blocks = (nbyte + block_size - 1) / block_size;
  size_t required = (size_t)(offset / block_size) + num_blocks;

  if (ensure_block_index_capacity(block_index, required) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to ensure block index "
        "capacity",
        state->lock_table, path);
    return INVALID_FD;
  }

  // Get current logical EOF (logical size) if present
  off_t current_logical_eof = 0;
  if (get_logical_eof_from_mapping(device, inode, l, &current_logical_eof) !=
      0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to get logical size",
        state->lock_table, path);
    return INVALID_FD;
  }

  // Compress and write each block
  for (size_t i = 0; i < num_blocks; i++) {
    size_t current_block_index = (size_t)(offset / block_size) + i;

    size_t block_start = i * block_size;
    const void *block_data = (const char *)buffer + block_start;

    // Determine logical size for this block (last block might be partial)
    size_t logical_size = block_size;
    if (i == num_blocks - 1) {
      logical_size = nbyte - i * block_size;
    }

    // Use helper function to compress and decide storage format
    void *data_to_store = NULL;
    size_t store_size = 0;
    int is_uncompressed = 0;
    if (compress_or_store_raw(state, block_data, logical_size, &data_to_store,
                              &store_size, &is_uncompressed) < 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to compress block",
          state->lock_table, path);
      return INVALID_FD;
    }

    off_t physical_offset = (off_t)(current_block_index * block_size);

    ssize_t write_result = l.next_layers->ops->lpwrite(
        fd, data_to_store, store_size, physical_offset, *l.next_layers);

    if (write_result < 0) {
      free(data_to_store);
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to write to storage",
          state->lock_table, path);
      return INVALID_FD;
    }

    // Punch a hole if the new stored size is smaller than the previously
    // stored payload for this block. This reclaims trailing bytes in-place.
    off_t old_stored_size = 0;
    if (current_block_index < block_index->capacity) {
      old_stored_size = block_index->sizes[current_block_index];
    }

    if (state->free_space && l.next_layers->ops->lfallocate != NULL &&
        old_stored_size > (off_t)store_size) {
      off_t punch_offset = physical_offset + (off_t)store_size;
      off_t punch_len = old_stored_size - (off_t)store_size;
      if (l.next_layers->ops->lfallocate(
              fd, punch_offset, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
              punch_len, l) < 0) {
        ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] "
                  "Failed to punch trailing bytes");
      }
    }

    // Update mapping after successful write
    block_index->sizes[current_block_index] = (off_t)store_size;
    block_index->is_uncompressed[current_block_index] = is_uncompressed;

    // Update num_blocks if necessary
    if (current_block_index >= block_index->num_blocks) {
      block_index->num_blocks = current_block_index + 1;
    }

    DEBUG_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Block %zu: "
              "stored_size=%zu (is_uncompressed=%d, physical_offset=%ld)",
              current_block_index, store_size,
              block_index->is_uncompressed[current_block_index],
              physical_offset);

    free(data_to_store);
  }

  // Only extend the original size if this write goes beyond current EOF
  off_t candidate_new_eof = offset + (off_t)nbyte;
  if (candidate_new_eof > current_logical_eof) {
    if (set_logical_eof_in_mapping(device, inode, candidate_new_eof, l) != 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_PWRITE] Failed to update original "
          "file size in mapping",
          state->lock_table, path);
      return INVALID_FD;
    }
  }

  locking_release(state->lock_table, path);
  return (ssize_t)nbyte;
}

ssize_t compression_sparse_block_pread(int fd, void *buffer, size_t nbyte,
                                       off_t offset, LayerContext l) {
  if (!validate_compression_fd_offset_and_nbyte(
          fd, offset, nbyte, "COMPRESSION_LAYER: SPARSE_BLOCK_PREAD")) {
    return INVALID_FD;
  }

  if (nbyte == 0) {
    return 0;
  }

  CompressionState *state = (CompressionState *)l.internal_state;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] File descriptor not found",
        NULL, NULL);
    return INVALID_FD;
  }
  const char *path = entry->path;

  if (locking_acquire_read(state->lock_table, path) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] Failed to acquire "
        "read lock on file",
        NULL, NULL);
    return INVALID_FD;
  }

  // Resolve (device,inode) for inode-keyed mappings
  dev_t device;
  ino_t inode;
  if (get_file_key_from_fd(fd, l, &device, &inode) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] Failed to get file key",
        state->lock_table, path);
    return INVALID_FD;
  }

  size_t bytes_to_read = nbyte;
  off_t original_size;
  if (get_logical_eof_from_mapping(device, inode, l, &original_size) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] Failed to get "
        "original size",
        state->lock_table, path);
    return INVALID_FD;
  }

  if (offset >= original_size || original_size == 0) {
    locking_release(state->lock_table, path);
    return 0;
  }

  if (offset + nbyte > original_size) {
    bytes_to_read = original_size - offset;
  }

  CompressedFileMapping *block_index =
      get_compressed_file_mapping(device, inode, state);
  if (!block_index) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] Missing block index mapping",
        state->lock_table, path);
    return INVALID_FD;
  }

  const size_t block_size = state->block_size;

  off_t initial_block_index = (off_t)(offset / block_size);
  off_t last_block_index = (off_t)((offset + bytes_to_read - 1) / block_size);
  size_t num_blocks = last_block_index - initial_block_index + 1;

  // Ensure block index has enough capacity for the blocks we're trying to read
  size_t max_block_needed = (size_t)initial_block_index + num_blocks;
  if (ensure_block_index_capacity(block_index, max_block_needed) < 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] "
                               "Failed to expand block index",
                               state->lock_table, path);
    return INVALID_FD;
  }

  // Aligned I/O: each block is stored at its logical start (idx * block_size)
  // Read each compressed block from its logical start, then decompress directly
  // into the caller buffer at the corresponding offset from the request base.
  for (size_t i = 0; i < num_blocks; i++) {
    size_t idx = (size_t)initial_block_index + i;

    // Destination inside user buffer relative to request base (aligned)
    uint8_t *dst_decompressed = (uint8_t *)buffer + i * block_size;
    size_t out_size =
        (i == num_blocks - 1) ? (bytes_to_read - i * block_size) : block_size;

    size_t cblock_len = (size_t)block_index->sizes[idx];
    if (cblock_len == 0) {
      DEBUG_MSG("Sparse block - return zeros");
      memset(dst_decompressed, 0, out_size);
      continue;
    }

    off_t phys_off = (off_t)(idx * block_size); // sparse layout: logical start

    // Read compressed block into a temporary buffer
    uint8_t *cbuf = malloc(cblock_len);
    if (!cbuf) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_PREAD] "
                                 "Failed to allocate compressed buffer",
                                 state->lock_table, path);
      return INVALID_FD;
    }
    ssize_t res = l.next_layers->ops->lpread(fd, cbuf, cblock_len, phys_off,
                                             *l.next_layers);
    if (res != (ssize_t)cblock_len) {
      free(cbuf);
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: COMPRESSION_SPARSE_BLOCK_PREAD] short read",
          state->lock_table, path);
      return INVALID_FD;
    }

    if (block_index->is_uncompressed &&
        block_index->is_uncompressed[idx] == 1) {
      // Uncompressed block: copy directly
      size_t to_copy = cblock_len < out_size ? cblock_len : out_size;
      memcpy(dst_decompressed, cbuf, to_copy);
    } else {
      // Compressed block: decompress into the caller buffer
      if (state->compressor.decompress_data(cbuf, cblock_len, dst_decompressed,
                                            &out_size) < 0) {
        free(cbuf);
        error_msg_and_release_lock(
            "[COMPRESSION_LAYER: COMPRESSION_SPARSE_BLOCK_PREAD] Failed to "
            "decompress data",
            state->lock_table, path);
        return INVALID_FD;
      }
    }

    free(cbuf);
  }
  locking_release(state->lock_table, path);

  return (ssize_t)bytes_to_read;
}

// perform physical truncate and report on failure
static int physical_truncate(const LayerContext *next_layers, int fd,
                             off_t size, LockTable *lock_table,
                             const char *path, const char *errmsg) {
  int res = next_layers->ops->lftruncate(fd, size, *next_layers);
  if (res < 0) {
    error_msg_and_release_lock(errmsg, lock_table, path);
    return -1;
  }
  return 0;
}

// Helper: compress data and decide whether to store compressed or uncompressed
// Always returns a buffer ready to write in *out_buffer (caller must free).
// If compression is not beneficial, returns a copy of the original data.
// Returns size in *out_size and compression flag in *out_is_uncompressed.
// Returns 0 on success, -1 on failure.
static int compress_or_store_raw(CompressionState *state, const void *data,
                                 size_t data_size, void **out_buffer,
                                 size_t *out_size, int *out_is_uncompressed) {
  size_t max_comp =
      state->compressor.get_compress_bound(data_size, state->compressor.level);
  uint8_t *compressed = (uint8_t *)malloc(max_comp);
  if (!compressed) {
    return -1;
  }

  ssize_t comp_size = state->compressor.compress_data(
      data, data_size, compressed, max_comp, state->compressor.level);
  if (comp_size < 0) {
    free(compressed);
    return -1;
  }

  // If compression is not beneficial (compressed >= original), store
  // uncompressed
  if ((size_t)comp_size >= data_size) {
    free(compressed);
    // Allocate a copy of the original data
    void *uncompressed_copy = malloc(data_size);
    if (!uncompressed_copy) {
      return -1;
    }
    memcpy(uncompressed_copy, data, data_size);
    *out_buffer = uncompressed_copy;
    *out_size = data_size;
    *out_is_uncompressed = 1;
  } else {
    *out_buffer = compressed;
    *out_size = (size_t)comp_size;
    *out_is_uncompressed = 0;
  }
  return 0;
}

// shrink file by truncating at or within a block
// If keep_bytes == 0, truncates at exact block boundary (keeps last_block_index
// complete blocks) If keep_bytes > 0, truncates within the last block (partial
// block truncation)
static int shrink_at_boundary(int fd, CompressedFileMapping *bim,
                              off_t last_block_index, size_t keep_bytes,
                              off_t new_logical_size, dev_t device, ino_t inode,
                              LayerContext l) {
  // Derive parameters from structs
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next_layers = l.next_layers;
  const size_t block_size = state->block_size;
  LockTable *lock_table = state->lock_table;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    return -1;
  }
  const char *path = entry->path;

  off_t phys_off = last_block_index * (off_t)block_size;
  off_t phys_trunc;

  if (keep_bytes == 0) {
    // Exact block boundary: keep the complete last block
    phys_trunc = phys_off + (off_t)bim->sizes[last_block_index];
  } else {
    // Partial block: truncate within the block
    phys_trunc = phys_off + (off_t)keep_bytes;
    bim->sizes[last_block_index] = (off_t)keep_bytes;
  }

  if (physical_truncate(next_layers, fd, phys_trunc, lock_table, path,
                        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Failed "
                        "physical truncate") < 0) {
    return -1;
  }

  size_t new_num_blocks = (size_t)last_block_index + 1;
  if (shrink_block_index(bim, new_num_blocks) < 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to shrink block index arrays",
                               lock_table, path);
    return -1;
  }
  if (set_logical_eof_in_mapping(device, inode, new_logical_size, l) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
        "Failed to set original size in file size mapping",
        lock_table, path);
    return -1;
  }
  return 0;
}

// Helper: shrink when last block is stored compressed
static int shrink_compressed_partial(int fd, CompressedFileMapping *bim,
                                     off_t last_block_index, size_t keep,
                                     off_t new_logical_size, dev_t device,
                                     ino_t inode, LayerContext l) {
  // Derive parameters from structs
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next_layers = l.next_layers;
  const size_t block_size = state->block_size;
  LockTable *lock_table = state->lock_table;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    return -1;
  }
  const char *path = entry->path;

  off_t phys_off = last_block_index * (off_t)block_size;
  off_t csize = bim->sizes[last_block_index];
  if (csize <= 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Invalid compressed size for last block",
                               lock_table, path);
    return -1;
  }

  uint8_t *compressed_src = (uint8_t *)malloc((size_t)csize);
  if (!compressed_src) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to allocate compressed buffer",
                               lock_table, path);
    return -1;
  }
  ssize_t read_result = next_layers->ops->lpread(
      fd, compressed_src, (size_t)csize, phys_off, *next_layers);
  if (read_result != (ssize_t)csize) {
    free(compressed_src);
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to read last compressed block",
                               lock_table, path);
    return -1;
  }

  uint8_t *decompressed = (uint8_t *)malloc(block_size);
  if (!decompressed) {
    free(compressed_src);
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to allocate decompress buffer",
                               lock_table, path);
    return -1;
  }
  size_t out_size = block_size;
  ssize_t decompress_result = state->compressor.decompress_data(
      compressed_src, (size_t)csize, decompressed, &out_size);
  if (decompress_result < 0) {
    free(decompressed);
    free(compressed_src);
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Decompression failed",
        lock_table, path);
    return -1;
  }
  free(compressed_src);

  if (physical_truncate(next_layers, fd, phys_off, lock_table, path,
                        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Failed "
                        "truncate before rewrite") < 0) {
    free(decompressed);
    return -1;
  }

  void *write_buf = NULL;
  size_t write_len = 0;
  int mark_uncompressed = 0;
  if (compress_or_store_raw(state, decompressed, keep, &write_buf, &write_len,
                            &mark_uncompressed) < 0) {
    free(decompressed);
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Recompression failed",
        lock_table, path);
    return -1;
  }

  ssize_t wr = next_layers->ops->lpwrite(fd, write_buf, write_len, phys_off,
                                         *next_layers);
  if (wr != (ssize_t)write_len) {
    free(write_buf);
    free(decompressed);
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to write rewritten block",
                               lock_table, path);
    return -1;
  }

  // Punch a hole if the new stored size of the last block is smaller than the
  // previously stored payload for this block. This reclaims trailing bytes
  // in-place.
  off_t old_stored_size = csize;

  if (state->free_space && l.next_layers->ops->lfallocate != NULL &&
      old_stored_size > (off_t)write_len) {
    off_t punch_offset = phys_off + (off_t)write_len;
    off_t punch_len = old_stored_size - (off_t)write_len;
    if (l.next_layers->ops->lfallocate(
            fd, punch_offset, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
            punch_len, l) < 0) {
      ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                "Failed to punch trailing bytes");
    }
  }

  bim->sizes[last_block_index] = (off_t)write_len;
  if (bim->is_uncompressed) {
    bim->is_uncompressed[last_block_index] = mark_uncompressed ? 1 : 0;
  }
  size_t new_num_blocks = (size_t)last_block_index + 1;
  if (shrink_block_index(bim, new_num_blocks) < 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to shrink block index arrays",
                               lock_table, path);
    return -1;
  }
  (void)set_logical_eof_in_mapping(device, inode, new_logical_size, l);

  free(write_buf);
  free(decompressed);
  return 0;
}

// Helper: truncate file to zero and reset mapping
static int truncate_to_zero(const LayerContext *next_layers, int fd,
                            CompressionState *state, const char *path,
                            dev_t device, ino_t inode, LayerContext l) {
  if (physical_truncate(next_layers, fd, 0, state->lock_table, path,
                        "[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] Failed to "
                        "truncate file to 0") < 0) {
    return -1;
  }
  (void)set_logical_eof_in_mapping(device, inode, 0, l);
  CompressedFileMapping *bim0 =
      get_compressed_file_mapping(device, inode, state);
  if (bim0) {
    // Use shrink function to properly free arrays and reset both num_blocks and
    // capacity
    (void)shrink_block_index(bim0, 0);
  }
  return 0;
}

int compression_sparse_block_ftruncate(int fd, off_t length, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;

  // Lookup fd in hash table
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] File descriptor %d "
              "not found",
              fd);
    return INVALID_FD;
  }
  const char *path = entry->path;

  if (locking_acquire_write(state->lock_table, path) != 0) {
    ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Failed to acquire "
              "write lock on file %s",
              path);
    return INVALID_FD;
  }

  dev_t device;
  ino_t inode;
  if (get_file_key_from_fd(fd, l, &device, &inode) != 0) {
    error_msg_and_release_lock(
        "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Failed to get file key",
        state->lock_table, path);
    return INVALID_FD;
  }

  const LayerContext *next_layers = l.next_layers;

  off_t original_size = 0;
  if (get_logical_eof_from_mapping(device, inode, l, &original_size) != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                               "Failed to get original size of file",
                               state->lock_table, path);
    return INVALID_FD;
  }

  // If the new size is the same as the original size, we don't need to do
  // anything.
  if ((off_t)length == original_size) {
    locking_release(state->lock_table, path);
    return 0;
  }

  // If the new size is 0, we truncate the file to 0 bytes.
  if (length == 0) {
    if (truncate_to_zero(next_layers, fd, state, path, device, inode, l) < 0) {
      return INVALID_FD;
    }
    locking_release(state->lock_table, path);
    return 0;
  }

  // If extending file, update block index mapping and logical size
  if (length > original_size) {
    const size_t block_size = state->block_size;
    CompressedFileMapping *bim =
        get_compressed_file_mapping(device, inode, state);
    if (!bim) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                                 "Missing block index mapping",
                                 state->lock_table, path);
      return INVALID_FD;
    }

    // Calculate how many blocks we need for the new size
    size_t new_num_blocks =
        (size_t)((length + (off_t)block_size - 1) / (off_t)block_size);

    // Expand block index arrays if needed
    if (ensure_block_index_capacity(bim, new_num_blocks) < 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] Failed to expand "
          "block arrays",
          state->lock_table, path);
      return INVALID_FD;
    }

    // Update the logical size
    if (set_logical_eof_in_mapping(device, inode, length, l) != 0) {
      error_msg_and_release_lock("[COMPRESSION_LAYER: SPARSE_BLOCK_FTRUNCATE] "
                                 "Failed to update logical size",
                                 state->lock_table, path);
      return INVALID_FD;
    }

    locking_release(state->lock_table, path);
    return 0;
  }

  // Shrinking case
  const size_t block_size = state->block_size;
  off_t last_block_index = (off_t)((length - 1) / (off_t)block_size);
  size_t bytes_to_keep = (size_t)(length % (off_t)block_size);

  CompressedFileMapping *bim =
      get_compressed_file_mapping(device, inode, state);
  if (!bim) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                               "Missing block index mapping",
                               state->lock_table, path);
    return INVALID_FD;
  }

  // should never happen, but as a safety measure, we check if the last block
  // index is out of range.
  if ((size_t)last_block_index >= bim->num_blocks && bytes_to_keep != 0) {
    error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_FTRUNCATE] "
                               "Last block index out of range",
                               state->lock_table, path);
    return INVALID_FD;
  }

  // Check if we can use simple truncation (exact boundary or uncompressed
  // partial)
  int is_uncompressed = (bytes_to_keep > 0 && bim->is_uncompressed &&
                         bim->is_uncompressed[last_block_index] == 1);

  if (bytes_to_keep == 0 || is_uncompressed) {
    // Either at exact block boundary, or partial block stored uncompressed
    if (shrink_at_boundary(fd, bim, last_block_index, bytes_to_keep, length,
                           device, inode, l) < 0) {
      return INVALID_FD;
    }
    locking_release(state->lock_table, path);
    return 0;
  }

  // Data in the last block is stored compressed, we need to decompress it and
  // then truncate it.
  if (shrink_compressed_partial(fd, bim, last_block_index, bytes_to_keep,
                                length, device, inode, l) < 0) {
    return INVALID_FD;
  }
  locking_release(state->lock_table, path);
  return 0;
}

int compression_sparse_block_truncate(const char *path, off_t length,
                                      LayerContext l) {
  return -1;
}

int compression_sparse_block_fstat(int fd, struct stat *stbuf, LayerContext l) {
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_FSTAT] File descriptor %d is "
              "not valid",
              fd);
    return INVALID_FD;
  }
  int res = l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
  if (res == 0 && stbuf->st_size > 0) {
    // We need to calculate the logical size from the compressed last block.
    dev_t device = stbuf->st_dev;
    ino_t inode = stbuf->st_ino;

    CompressionState *state = (CompressionState *)l.internal_state;

    // Lookup fd in hash table
    FdToInode *entry = fd_to_inode_lookup(state, fd);
    if (!entry) {
      ERROR_MSG("[COMPRESSION_LAYER: SPARSE_BLOCK_FSTAT] File descriptor %d "
                "not found",
                fd);
      return INVALID_FD;
    }

    const char *path = entry->path;
    int lock_result = locking_acquire_read(state->lock_table, path);
    if (lock_result != 0) {
      ERROR_MSG(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_FSTAT] Failed to acquire read "
          "lock on file %s",
          path);

      return INVALID_FD;
    }

    off_t logical_eof = 0;
    if (get_logical_eof_from_mapping(device, inode, l, &logical_eof) != 0) {
      error_msg_and_release_lock(
          "[COMPRESSION_LAYER: SPARSE_BLOCK_FSTAT] Failed to get logical EOF "
          "of file from mapping table",
          state->lock_table, path);
      return INVALID_FD;
    }
    stbuf->st_size = logical_eof;
    locking_release(state->lock_table, path);
  }
  return res;
}

static int rebuild_block_mapping_from_storage_with_pathname(
    const char *pathname, dev_t device, ino_t inode, LayerContext l) {
  int fd = l.next_layers->ops->lopen(pathname, O_RDONLY, 0, *l.next_layers);
  if (fd < 0) {
    ERROR_MSG(
        "[COMPRESSION_LAYER: REBUILD_BLOCK_MAPPING_FROM_STORAGE_WITH_PATHNAME] "
        "Failed to open file %s",
        pathname);
    return INVALID_FD;
  }

  if (rebuild_block_mapping_from_storage(fd, device, inode, l) < 0) {
    l.next_layers->ops->lclose(fd, *l.next_layers);
    ERROR_MSG(
        "[COMPRESSION_LAYER: REBUILD_BLOCK_MAPPING_FROM_STORAGE_WITH_PATHNAME] "
        "Failed to rebuild "
        "block mapping",
        pathname);
    return INVALID_FD;
  }

  l.next_layers->ops->lclose(fd, *l.next_layers);

  return 0;
}

int compression_sparse_block_lstat(const char *pathname, struct stat *stbuf,
                                   LayerContext l) {
  int res = l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
  // Only process regular files, skip directories and other special files
  if (res == 0 && S_ISREG(stbuf->st_mode) && stbuf->st_size > 0) {
    // We need to calculate the logical size from the compressed last block.
    dev_t device = stbuf->st_dev;
    ino_t inode = stbuf->st_ino;

    CompressionState *state = (CompressionState *)l.internal_state;
    int lock_result = locking_acquire_read(state->lock_table, pathname);
    if (lock_result != 0) {
      ERROR_MSG("[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed to acquire read "
                "lock on file %s",
                pathname);
      return INVALID_FD;
    }

    off_t logical_eof = 0;
    if (get_logical_eof_from_mapping(device, inode, l, &logical_eof) != 0) {
      DEBUG_MSG("[COMPRESSION_LAYER: COMPRESSION_LSTAT] "
                "Failed to get logical EOF of file from mapping table, we "
                "will try to rebuild the mapping from storage");
      locking_release(state->lock_table, pathname);

      lock_result = locking_acquire_write(state->lock_table, pathname);
      if (lock_result != 0) {
        ERROR_MSG(
            "[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed to acquire write "
            "lock on file %s",
            pathname);
        return INVALID_FD;
      }

      // Re-check after acquiring write lock (another thread may have rebuilt
      // during the lock upgrade window)
      if (get_logical_eof_from_mapping(device, inode, l, &logical_eof) != 0) {
        // Still missing, rebuild now that we have exclusive access
        if (rebuild_block_mapping_from_storage_with_pathname(pathname, device,
                                                             inode, l) != 0) {
          error_msg_and_release_lock(
              "[COMPRESSION_LAYER: COMPRESSION_LSTAT] Failed to rebuild block "
              "mapping from storage",
              state->lock_table, pathname);
          return INVALID_FD;
        }

        if (get_logical_eof_from_mapping(device, inode, l, &logical_eof) != 0) {
          error_msg_and_release_lock("[COMPRESSION_LAYER: COMPRESSION_LSTAT] "
                                     "Failed to get logical EOF "
                                     "of file from mapping table",
                                     state->lock_table, pathname);
          return INVALID_FD;
        }
      }
    }
    stbuf->st_size = logical_eof;
    locking_release(state->lock_table, pathname);
  }
  return res;
}
