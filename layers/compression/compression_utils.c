#include "compression_utils.h"
#include "../../logdef.h"
#include "../../shared/utils/compressor/compressor.h"
#include "compression.h"
#include <limits.h>

// Helper: build binary key from (device, inode)
static inline size_t build_dev_ino_key(dev_t device, ino_t inode,
                                       unsigned char *key_out) {
  memcpy(key_out, &device, sizeof(dev_t));
  memcpy(key_out + sizeof(dev_t), &inode, sizeof(ino_t));
  return sizeof(dev_t) + sizeof(ino_t);
}

/**
 * @brief Check if a file descriptor is valid for the limit of MAX_FDS
 *
 * @param fd -> file descriptor to check
 * @return int -> 1 if valid, 0 otherwise
 */
int is_valid_compression_fd(int fd) { return (fd >= 0 && fd < MAX_FDS); }

/**
 * @brief Validate the file descriptor, offset and number of bytes
 *
 * @param fd -> file descriptor to check
 * @param offset -> offset to check
 * @param nbyte -> number of bytes to check
 * @param op_name -> name of the operation to print in the error message
 * @return bool -> true if valid, false otherwise
 */
bool validate_compression_fd_offset_and_nbyte(int fd, off_t offset,
                                              size_t nbyte,
                                              const char *op_name) {
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[%s] File descriptor %d is not valid", op_name, fd);
    return false;
  }
  if (offset < 0) {
    ERROR_MSG("[%s] Offset is negative", op_name);
    return false;
  }
  if (nbyte > SSIZE_MAX) {
    ERROR_MSG("[%s] Number of bytes to write is greater than SSIZE_MAX",
              op_name);
    return false;
  }
  return true;
}

/**
 * @brief Print an error message and release the lock if it exists
 *
 * @param msg -> error message to print
 * @param lock_table -> lock table to release
 * @param path -> path of the file to release the lock on
 */
void error_msg_and_release_lock(const char *msg, LockTable *lock_table,
                                const char *path) {
  ERROR_MSG(msg);
  if (lock_table) {
    locking_release(lock_table, path);
  }
}

/**
 * @brief Set the logical EOF (end-of-file position) of a file in the file
 * mapping
 * @warning This function is not thread-safe. Caller must ensure proper locking
 *          before calling this function. The entry must already exist.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param logical_eof -> logical (uncompressed) end-of-file position
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if there was an error
 */
int set_logical_eof_in_mapping(dev_t device, ino_t inode, off_t logical_eof,
                               LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry) {
    return -1;
  }
  entry->logical_eof = logical_eof;
  return 0;
}

/**
 * @brief Create a compressed file mapping if it doesn't exist
 * @warning This function is not thread-safe. Caller must ensure proper locking
 *          before calling this function.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param logical_eof -> logical (uncompressed) end-of-file position
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if there was an error
 */
int create_compressed_file_mapping(dev_t device, ino_t inode, off_t logical_eof,
                                   LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (entry)
    return -1;
  entry = calloc(1, sizeof(CompressedFileMapping));
  if (!entry)
    return -1;
  entry->device = device;
  entry->inode = inode;
  memcpy(entry->key, key, key_len);
  entry->logical_eof = logical_eof;
  entry->open_counter = 0;
  entry->unlink_called = 0;
  entry->num_blocks = 0;
  entry->capacity = 0;
  entry->sizes = NULL;
  entry->is_uncompressed = NULL;
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_ADD(hh, state->file_mapping, key, key_len, entry);
  return 0;
}

/**
 * @brief Get the logical EOF (end-of-file position) of a file from the file
 * mapping
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param l -> layer context
 * @param logical_eof -> pointer to receive the logical EOF position
 * @return int -> 0 if successful, -1 if the file is not found
 */
int get_logical_eof_from_mapping(dev_t device, ino_t inode, LayerContext l,
                                 off_t *logical_eof) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry)
    return -1;
  *logical_eof = entry->logical_eof;
  return 0;
}

/**
 * @brief Increment the open counter for a file
 *
 * This creates or updates the mapping entry to track that another fd was
 * opened.
 *
 * @warning This function is not thread-safe. Caller must hold appropriate
 * locks.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if there was an error
 */
int increment_open_counter(dev_t device, ino_t inode, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry)
    return -1;

  entry->open_counter++;
  return 0;
}

/**
 * @brief Decrement the open counter for a file
 *
 * Decrements the counter. The caller should check if cleanup is needed
 * using should_cleanup_mapping() and call remove_compressed_file_mapping()
 * if appropriate.
 *
 * @warning This function is not thread-safe. Caller must hold appropriate
 * locks.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if entry not found
 */
int decrement_open_counter(dev_t device, ino_t inode, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry)
    return -1;

  entry->open_counter--;

  return 0;
}

/**
 * @brief Mark a file as unlinked
 *
 * Sets the unlink_called flag and returns the current open counter value
 * so the caller can decide whether to clean up the mapping.
 *
 * @warning This function is not thread-safe. Caller must hold appropriate
 * locks.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param open_counter -> pointer to receive the current open counter value
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if entry not found
 */
int mark_as_unlinked(dev_t device, ino_t inode, int *open_counter,
                     LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry)
    return -1;

  entry->unlink_called = 1;

  // Return the current open counter so caller can decide cleanup
  if (open_counter != NULL) {
    *open_counter = entry->open_counter;
  }

  return 0;
}

/**
 * @brief Check if a mapping should be cleaned up
 *
 * Returns 1 if the file was unlinked and has no open fds.
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param l -> layer context
 * @return int -> 1 if should cleanup, 0 otherwise, -1 if entry not found
 */
int should_cleanup_mapping(dev_t device, ino_t inode, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry = NULL;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  if (!entry)
    return -1;

  return (entry->unlink_called && entry->open_counter <= 0) ? 1 : 0;
}

// Backward-compat wrappers using path (temporary)
int set_original_size_in_file_size_mapping(const char *path,
                                           off_t original_size,
                                           LayerContext l) {
  struct stat stbuf;
  if (l.next_layers->ops->llstat(path, &stbuf, *l.next_layers) != 0)
    return -1;
  // Try to set; if missing, create and set
  if (set_logical_eof_in_mapping(stbuf.st_dev, stbuf.st_ino, original_size,
                                 l) == 0) {
    return 0;
  }
  if (create_compressed_file_mapping(stbuf.st_dev, stbuf.st_ino, original_size,
                                     l) != 0) {
    return -1;
  }
  return 0;
}

int get_original_size_from_mapping(const char *path, LayerContext l,
                                   off_t *original_size) {
  struct stat stbuf;
  if (l.next_layers->ops->llstat(path, &stbuf, *l.next_layers) != 0)
    return -1;
  return get_logical_eof_from_mapping(stbuf.st_dev, stbuf.st_ino, l,
                                      original_size);
}

/**
 * @brief Get the total compressed size of a range of blocks
 *
 * @param initial_block_index -> initial block index
 * @param last_block_index -> last block index
 * @param block_index -> block index mapping
 * @return size_t -> total compressed size
 */
size_t get_total_compressed_size(off_t initial_block_index,
                                 off_t last_block_index,
                                 CompressedFileMapping *file_mapping) {
  size_t total_compressed_size = 0;
  for (off_t i = initial_block_index; i <= last_block_index; i++) {
    total_compressed_size += file_mapping->sizes[i];
  }
  return total_compressed_size;
}

/**
 * @brief Get a file mapping for a file (includes block index for sparse_block
 * mode)
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param state -> compression state
 * @return CompressedFileMapping * -> file mapping, or NULL if not found
 */
CompressedFileMapping *get_compressed_file_mapping(dev_t device, ino_t inode,
                                                   CompressionState *state) {
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  size_t key_len = build_dev_ino_key(device, inode, key);
  CompressedFileMapping *entry = NULL;
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_FIND(hh, state->file_mapping, key, key_len, entry);
  return entry;
}

/**
 * @brief Ensure block index arrays have sufficient capacity
 *
 * This function ensures that the block index arrays (offsets, sizes,
 * is_uncompressed) have at least the required capacity. If the current
 * capacity is insufficient, it reallocates the arrays and initializes
 * new entries as sparse (size 0).
 *
 * @param block_index -> block index mapping to expand
 * @param required_blocks -> minimum number of blocks needed
 * @return int -> 0 on success, -1 on error
 */
int ensure_block_index_capacity(CompressedFileMapping *block_index,
                                size_t required_blocks) {
  if (required_blocks <= block_index->capacity) {
    // Already have enough capacity allocated
    // Just update num_blocks if needed
    if (required_blocks > block_index->num_blocks) {
      block_index->num_blocks = required_blocks;
    }
    return 0;
  }

  size_t old_capacity = block_index->capacity;

  // Expand block index arrays to accommodate the required blocks
  off_t *new_sizes =
      (off_t *)realloc(block_index->sizes, required_blocks * sizeof(off_t));
  if (!new_sizes) {
    return -1;
  }

  int *new_is_uncompressed = NULL;
  if (block_index->is_uncompressed) {
    new_is_uncompressed = (int *)realloc(block_index->is_uncompressed,
                                         required_blocks * sizeof(int));
  } else {
    new_is_uncompressed = (int *)calloc(required_blocks, sizeof(int));
  }

  if (!new_is_uncompressed) {
    // First realloc succeeded, need to restore or free appropriately
    if (new_sizes != block_index->sizes) {
      free(new_sizes);
    }
    return -1;
  }

  block_index->sizes = new_sizes;
  block_index->is_uncompressed = new_is_uncompressed;

  // Initialize new blocks as sparse (size 0) from old capacity
  for (size_t i = old_capacity; i < required_blocks; i++) {
    new_sizes[i] = 0;
    new_is_uncompressed[i] = 0;
  }

  block_index->num_blocks = required_blocks;
  block_index->capacity = required_blocks;

  return 0;
}

/**
 * @brief Remove a file's block index mapping and free all associated memory
 *
 * This function removes the CompressedFileMapping entry for the specified file
 * and frees all dynamically allocated arrays (sizes, is_uncompressed).
 *
 * @param device -> device id
 * @param inode -> inode number
 * @param l -> layer context
 * @return int -> 0 if successful, -1 if entry not found
 */
int remove_compressed_file_mapping(dev_t device, ino_t inode, LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  CompressedFileMapping *entry =
      get_compressed_file_mapping(device, inode, state);
  if (!entry)
    return -1;

  // Free dynamically allocated arrays
  if (entry->sizes) {
    free(entry->sizes);
  }
  if (entry->is_uncompressed) {
    free(entry->is_uncompressed);
  }

  // Remove from hash table
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  HASH_DEL(state->file_mapping, entry);

  // Free the entry itself
  free(entry);
  return 0;
}

int get_file_key_from_fd(int fd, LayerContext l, dev_t *device, ino_t *inode) {
  CompressionState *state = (CompressionState *)l.internal_state;
  if (!is_valid_compression_fd(fd)) {
    ERROR_MSG("[COMPRESSION_UTILS: GET_FILE_KEY_FROM_FD] File descriptor %d is "
              "not valid",
              fd);
    return -1;
  }

  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    ERROR_MSG(
        "[COMPRESSION_UTILS: GET_FILE_KEY_FROM_FD] File descriptor %d not "
        "found",
        fd);
    return -1;
  }
  *device = entry->device;
  *inode = entry->inode;

  return 0;
}

/**
 * @brief Lookup an FdToInode entry by file descriptor
 *
 * @param state Compression state
 * @param fd File descriptor to look up
 * @return FdToInode* Entry if found, NULL otherwise
 */
FdToInode *fd_to_inode_lookup(CompressionState *state, int fd) {
  FdToInode *entry = &state->fd_to_inode[fd];
  if (entry->fd == INVALID_FD) {
    DEBUG_MSG("[COMPRESSION_UTILS: FD_TO_INODE_LOOKUP] fd=%d, found=NO, "
              "entry=(nil) - slot empty",
              fd);
    return NULL;
  }
  return entry;
}

/**
 * @brief Insert a new FdToInode entry
 *
 * @param state Compression state
 * @param fd File descriptor (key)
 * @param device Device ID
 * @param inode Inode number
 * @param path File path (will be duplicated)
 * @return int 0 on success, -1 on failure
 */
int fd_to_inode_insert(CompressionState *state, int fd, dev_t device,
                       ino_t inode, const char *path) {
  // Check if entry already exists
  FdToInode *existing = fd_to_inode_lookup(state, fd);
  if (existing) {
    ERROR_MSG("[COMPRESSION_UTILS: FD_TO_INODE_INSERT] File descriptor %d "
              "already exists",
              fd);
    return -1;
  }

  // Get direct access to array slot
  FdToInode *entry = &state->fd_to_inode[fd];

  // Duplicate path
  entry->path = strdup(path);
  if (!entry->path) {
    ERROR_MSG("[COMPRESSION_UTILS: FD_TO_INODE_INSERT] Failed to allocate "
              "memory for path");
    return -1;
  }

  // Populate entry
  entry->fd = fd;
  entry->device = device;
  entry->inode = inode;

  return 0;
}

/**
 * @brief Remove an FdToInode entry by file descriptor
 *
 * This function is safe to call even if the entry doesn't exist.
 * It will silently succeed if the fd is not found (useful for cleanup).
 *
 * @param state Compression state
 * @param fd File descriptor to remove
 * @return int 0 on success (always succeeds)
 */
int fd_to_inode_remove(CompressionState *state, int fd) {
  FdToInode *entry = fd_to_inode_lookup(state, fd);
  if (!entry) {
    return 0; // Return success, nothing to remove
  }

  // Free resources
  if (entry->path) {
    free(entry->path);
  }

  // Clear the slot
  entry->fd = INVALID_FD;
  entry->path = NULL;
  entry->device = 0;
  entry->inode = 0;

  DEBUG_MSG(
      "[COMPRESSION_UTILS: FD_TO_INODE_REMOVE] Removed fd=%d from slot=%d", fd,
      fd);

  return 0;
}

/**
 * @brief Process a single block during rebuild: read, detect format, and set
 * metadata
 *
 * This helper function reads a block from storage, detects its compression
 * format, and updates the block index mapping with the appropriate size and
 * compression status.
 *
 * @param fd File descriptor
 * @param block_idx Block index to process
 * @param phys_offset Physical offset of the block
 * @param block_size Block size
 * @param bim Block index mapping to update
 * @param l Layer context
 * @return 0 on success, -1 on failure
 */
static int process_block_for_rebuild(int fd, size_t block_idx,
                                     off_t phys_offset, size_t block_size,
                                     CompressedFileMapping *bim,
                                     LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next = l.next_layers;

  uint8_t *block_buffer = (uint8_t *)malloc(block_size);
  if (!block_buffer) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to allocate block "
              "buffer");
    return -1;
  }

  ssize_t read_res =
      next->ops->lpread(fd, block_buffer, block_size, phys_offset, *next);

  if (read_res < 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to read block "
              "from storage");
    free(block_buffer);
    return -1;
  }

  if (read_res == 0) {
    // Sparse block - no data
    bim->sizes[block_idx] = 0;
    bim->is_uncompressed[block_idx] = 0;
    free(block_buffer);
    return 0;
  }

  // Need at least 4 bytes to detect format
  if ((size_t)read_res < 4) {
    // Too small, must be uncompressed
    bim->sizes[block_idx] = read_res;
    bim->is_uncompressed[block_idx] = 1;
    free(block_buffer);
    DEBUG_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Block %zu: uncompressed, "
              "size=%zu",
              block_idx, read_res);
    return 0;
  }

  if (state->compressor.detect_format(block_buffer, (size_t)read_res) == 0) {
    // Block is compressed
    size_t compressed_size = 0;
    int res = state->compressor.get_compressed_size(
        block_buffer, (size_t)read_res, block_size, &compressed_size);
    if (res < 0) {
      ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to get "
                "compressed size");
      free(block_buffer);
      return -1;
    }

    bim->sizes[block_idx] = (off_t)compressed_size;
    bim->is_uncompressed[block_idx] = 0;

    DEBUG_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Block %zu: compressed, "
              "size=%zu",
              block_idx, compressed_size);
  } else {
    // Block is uncompressed
    bim->sizes[block_idx] = read_res;
    bim->is_uncompressed[block_idx] = 1;
    DEBUG_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Block %zu: uncompressed, "
              "size=%zu",
              block_idx, read_res);
  }

  free(block_buffer);
  return 0;
}

/**
 * @brief Calculate the logical (uncompressed) size of a compressed block
 *
 * This helper function reads a compressed block and returns its uncompressed
 * size. Used for calculating logical EOF of the last block when it's
 * compressed.
 *
 * @param fd File descriptor
 * @param phys_offset Physical offset of the block
 * @param compressed_size Compressed size of the block
 * @param block_size Block size
 * @param l Layer context
 * @return Uncompressed size on success, -1 on failure
 */
static off_t get_compressed_block_logical_size(int fd, off_t phys_offset,
                                               size_t compressed_size,
                                               size_t block_size,
                                               LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next = l.next_layers;

  uint8_t *block_buffer = (uint8_t *)malloc(block_size);
  if (!block_buffer) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to allocate block "
              "buffer for logical size calculation");
    return -1;
  }

  ssize_t read_res =
      next->ops->lpread(fd, block_buffer, block_size, phys_offset, *next);
  if (read_res < 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to read block for "
              "logical size calculation");
    free(block_buffer);
    return -1;
  }

  off_t uncompressed_size =
      state->compressor.get_original_file_size(block_buffer, compressed_size);
  free(block_buffer);

  if (uncompressed_size < 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to get "
              "uncompressed size");
    return -1;
  }

  return uncompressed_size;
}

/**
 * @brief Rebuild BlockIndexMapping by scanning stored blocks
 *
 * This function recovers block metadata after a crash by reading block
 * headers from storage. It reconstructs the BlockIndexMapping for a file by:
 * 1. Getting the physical file size
 * 2. Scanning each block position in the sparse layout
 * 3. Reading and parsing block headers
 * 4. Rebuilding the sizes, and is_uncompressed arrays
 *
 * @param fd File descriptor of the compressed file
 * @param device Device ID
 * @param inode Inode number
 * @param l Layer context
 * @return 0 on success, -1 on failure
 */
int rebuild_block_mapping_from_storage(int fd, dev_t device, ino_t inode,
                                       LayerContext l) {
  CompressionState *state = (CompressionState *)l.internal_state;
  const LayerContext *next = l.next_layers;

  // Get physical file EOF (end-of-file position)
  struct stat stbuf;
  if (next->ops->lfstat(fd, &stbuf, *next) != 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to stat file");
    return -1;
  }

  // Skip directories and non-regular files
  if (!S_ISREG(stbuf.st_mode)) {
    DEBUG_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Skipping non-regular file "
              "(directory or special file)");
    return 0; // Not an error, just skip
  }

  off_t physical_eof = stbuf.st_size;
  if (physical_eof == 0) {
    // Empty file, nothing to rebuild
    return 0;
  }

  // Ensure a block index mapping exists (explicit create if missing)
  CompressedFileMapping *bim =
      get_compressed_file_mapping(device, inode, state);
  if (!bim) {
    if (create_compressed_file_mapping(device, inode, -1, l) != 0) {
      ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to create block "
                "index");
      return -1;
    }
    bim = get_compressed_file_mapping(device, inode, state);
    if (!bim) {
      ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Mapping not found after "
                "creation");
      return -1;
    }
  }

  size_t block_size = state->block_size;
  size_t max_blocks = (size_t)((physical_eof + block_size - 1) / block_size);

  // Allocate or resize arrays
  if (ensure_block_index_capacity(bim, max_blocks) < 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to allocate block "
              "index arrays");
    return -1;
  }

  if (max_blocks == 0) {
    return 0;
  }

  // Scan all blocks except the last one (which may be partial)
  // Process blocks 0 to max_blocks-2 (all assumed to be full blocks)
  size_t last_block_idx = max_blocks - 1;

  for (size_t block_idx = 0; block_idx < last_block_idx; block_idx++) {
    off_t phys_offset = (off_t)(block_idx * block_size);

    if (process_block_for_rebuild(fd, block_idx, phys_offset, block_size, bim,
                                  l) < 0) {
      return -1;
    }
  }

  // Handle the last block separately (may be partial)
  off_t logical_eof = 0;
  off_t last_block_phys_offset = (off_t)(last_block_idx * block_size);
  off_t last_block_logical_start = (off_t)(last_block_idx * block_size);

  // Check if there's any data at this position
  if (physical_eof <= last_block_phys_offset) {
    // No data → sparse block
    bim->sizes[last_block_idx] = 0;
    bim->is_uncompressed[last_block_idx] = 0;
    // logical_eof remains unchanged (no data in last block)
    logical_eof = physical_eof;
  } else {
    // Data exists - determine if compressed or uncompressed
    if (process_block_for_rebuild(fd, last_block_idx, last_block_phys_offset,
                                  block_size, bim, l) < 0) {
      return -1;
    }

    // Calculate logical_eof based on block metadata
    if (bim->is_uncompressed[last_block_idx]) {
      // Uncompressed: logical size is stored in bim->sizes[last_block_idx]
      logical_eof = last_block_logical_start + bim->sizes[last_block_idx];

      // Validate logical EOF matches physical EOF for uncompressed blocks
      if (logical_eof != physical_eof) {
        ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Logical EOF %ld does "
                  "not match physical EOF %ld when last block is uncompressed",
                  (long)logical_eof, (long)physical_eof);
        return -1;
      }
    } else {
      // Compressed: need to read block again to get uncompressed size
      size_t compressed_size = (size_t)bim->sizes[last_block_idx];
      off_t uncompressed_size = get_compressed_block_logical_size(
          fd, last_block_phys_offset, compressed_size, block_size, l);
      if (uncompressed_size < 0) {
        ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to get "
                  "uncompressed size for last block");
        return -1;
      }
      logical_eof = last_block_logical_start + uncompressed_size;
    }
  }

  // Set logical size in mapping
  if (set_logical_eof_in_mapping(device, inode, logical_eof, l) != 0) {
    ERROR_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Failed to set logical "
              "size");
    return -1;
  }

  DEBUG_MSG("[COMPRESSION_UTILS: REBUILD_MAPPING] Rebuilt mapping for "
            "dev=%lu, ino=%lu: %zu blocks, logical_eof=%ld",
            (unsigned long)device, (unsigned long)inode, max_blocks,
            logical_eof);

  return 0;
}

/**
 * @brief Shrink block index arrays and update
 *
 * This function updates the number of blocks and the capacity of the block
 * index mapping. Only shrinks the is_uncompressed and sizes arrays if the new
 * size is less than 50% of current capacity to avoid frequent small
 * reallocations.
 *
 * @param block_index Block index mapping to update
 * @param required_blocks Number of blocks actually needed
 * @return 0 on success, -1 on failure
 */
int shrink_block_index(CompressedFileMapping *block_index,
                       size_t required_blocks) {
  size_t current_capacity = block_index->capacity;

  // Only shrink if new size is less than 50% of current capacity
  // This avoids frequent small reallocations while reclaiming memory for
  // significant truncations
  if (required_blocks == 0) {
    // Truncate to zero - always shrink
    if (block_index->sizes) {
      free(block_index->sizes);
      block_index->sizes = NULL;
    }
    if (block_index->is_uncompressed) {
      free(block_index->is_uncompressed);
      block_index->is_uncompressed = NULL;
    }
    block_index->num_blocks = 0;
    block_index->capacity = 0;
    return 0;
  }

  if (required_blocks * 2 < current_capacity && current_capacity > 0) {
    // Significant reduction - shrink arrays
    off_t *new_sizes =
        (off_t *)realloc(block_index->sizes, required_blocks * sizeof(off_t));
    if (!new_sizes && required_blocks > 0) {
      return -1;
    }

    int *new_is_uncompressed = NULL;
    if (block_index->is_uncompressed) {
      new_is_uncompressed = (int *)realloc(block_index->is_uncompressed,
                                           required_blocks * sizeof(int));
      if (!new_is_uncompressed && required_blocks > 0) {
        // Realloc failed for is_uncompressed, but sizes succeeded
        // Restore original sizes if it was reallocated
        if (new_sizes != block_index->sizes) {
          free(new_sizes);
        }
        return -1;
      }
    }

    block_index->sizes = new_sizes;
    block_index->is_uncompressed = new_is_uncompressed;
    block_index->capacity = required_blocks;
    block_index->num_blocks = required_blocks;
  } else {
    // Just update num_blocks without reallocating
    // Capacity remains the same
    block_index->num_blocks = required_blocks;
  }

  return 0;
}
