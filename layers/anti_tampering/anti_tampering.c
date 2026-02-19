#include "anti_tampering.h"
#include "../../logdef.h"
#include "../../shared/utils/hasher/hasher.h"
#include "anti_tampering_utils.h"
#include "block_anti_tampering.h"
#include "config.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * ============================================================================
 * ANTI-TAMPERING LAYER IMPLEMENTATION
 * ============================================================================
 *
 * Provides data integrity verification using SHA-256 hashes with thread-safe
 * file locking. Verification happens on open() (checking stored hash against
 * current file content), hash computation and storage happens on close().
 * See README.md for detailed documentation on architecture, locking strategy,
 * and performance characteristics.
 * ============================================================================
 */

#define INVALID_FD (-1) // Value to indicate an invalid fd

static const LayerOps default_mode_ops = {
    .lpread = anti_tampering_read,
    .lpwrite = anti_tampering_write,
    .lopen = anti_tampering_open,
    .lclose = anti_tampering_close,
    .lftruncate = anti_tampering_ftruncate,
    .lfstat = anti_tampering_fstat,
    .llstat = anti_tampering_lstat,
    .lunlink = anti_tampering_unlink,
};

static const LayerOps block_mode_ops = {
    .lpread = block_anti_tampering_read,
    .lpwrite = block_anti_tampering_write,
    .lopen = block_anti_tampering_open,
    .lclose = block_anti_tampering_close,
    .lftruncate = block_anti_tampering_ftruncate,
    .lfstat = block_anti_tampering_fstat,
    .llstat = block_anti_tampering_lstat,
    .lunlink = block_anti_tampering_unlink,
};

static inline void
require_block_size_or_exit(const AntiTamperingConfig *config) {
  if (!config || config->block_size == 0) {
    ERROR_MSG("[ANTI_TAMPERING_INIT] Block size is not set for block mode");
    exit(1);
  }
}

static inline const LayerOps *ops_for_mode(anti_tampering_mode_t mode) {
  switch (mode) {
  case ANTI_TAMPERING_MODE_BLOCK:
    return &block_mode_ops;
  case ANTI_TAMPERING_MODE_FILE:
  default:
    return &default_mode_ops;
  }
}

/**
 * @brief Free the memory allocated for a FileMapping's file_path and hash_path
 *
 * @param mapping -> pointer to the FileMapping to free
 */

/**
 * @brief Perform atomic hash verification with shared file locking
 *
 * This function implements atomic hash verification by:
 * 1. Acquiring a READ lock on the file path
 * 2. Reading the stored hash from the hash layer
 * 3. Computing the current hash of the file content
 * 4. Comparing stored vs computed hashes
 * 5. Releasing the read lock
 *
 * The read lock ensures that:
 * - Hash verification is atomic (no partial file modifications)
 * - Multiple verifications can run concurrently
 * - Writers are excluded during verification
 *
 * @param file_fd         -> File descriptor for acquiring read lock
 * @param verify_fd       -> File descriptor to read data for hash computation
 * @param hash_fd         -> File descriptor of the hash storage file
 * @param state           -> AntiTamperingState pointer with layer contexts and
 * lock table
 * @param file_path       -> File path used as locking key
 * @param l               -> LayerContext passed to underlying layers
 * @return int            -> 0 on success, -1 on error
 */
static int atomic_hash_verify(int file_fd, int verify_fd, int hash_fd,
                              AntiTamperingState *state, const char *file_path,
                              LayerContext l) {
  // Acquire shared lock for atomic verification
  if (locking_acquire_read(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_VERIFY] Failed to acquire read lock on file %s "
              "(fd=%d)",
              file_path, file_fd);
    return -1;
  }

  int result = 0;

  // read the hash from the hash layer
  size_t hex_size = state->hasher.get_hex_size();
  char *stored_hash = malloc(hex_size);
  if (!stored_hash) {
    locking_release(state->lock_table, file_path);
    return -1;
  }

  state->hash_layer.app_context = l.app_context;
  ssize_t hash_res = state->hash_layer.ops->lpread(
      hash_fd, stored_hash, hex_size - 1, 0, state->hash_layer);
  if (hash_res > 0) {
    stored_hash[hash_res] = '\0';

    // compute the hash of the file (file is already locked)
    char *file_hex_hash =
        state->hasher.hash_file_hex(verify_fd, state->data_layer);
    if (file_hex_hash) {
      // compare the computed hash with the stored hash
      if (strcmp(file_hex_hash, stored_hash) != 0 && WARN_ENABLED()) {
        // Get file size first for debugging
        struct stat stbuf;
        int res =
            state->data_layer.ops->lfstat(verify_fd, &stbuf, state->data_layer);
        if (res == -1) {
          ERROR_MSG("[ANTI_TAMPERING_VERIFY] Failed to get file size for file "
                    "%s (fd=%d)",
                    file_path, verify_fd);
          free(file_hex_hash);
          free(stored_hash);
          locking_release(state->lock_table, file_path);
          return -1;
        }
        off_t file_size = stbuf.st_size;

        // ignore files with size 0: could have been just created
        if (file_size != 0) {
          WARN_MSG("[ANTI_TAMPERING_OPEN] Hash mismatch for file %s "
                   "(size=%ld, verify_fd=%d); Stored "
                   "hash: %s; Computed hash: %s",
                   file_path, (long)file_size, verify_fd, stored_hash,
                   file_hex_hash);
        }
      }
      free(file_hex_hash);
    } else {
      ERROR_MSG(
          "[ANTI_TAMPERING_VERIFY] Failed to compute hash for file %s (fd=%d)",
          file_path, verify_fd);

      result = -1;
    }
  } else {
    result = -1;
  }

  // Release the lock
  locking_release(state->lock_table, file_path);

  // Free the allocated stored_hash
  free(stored_hash);

  return result;
}

/**
 * @brief Init Anti-Tampering Layer
 * @param data_layer     -> data layer
 * @param hash_layer     -> hash layer
 * @param hashes_storage  -> folder where the hashes are stored
 * @param algorithm      -> hash algorithm to use
 * @return LayerContext -> anti-tampering layer
 */
LayerContext anti_tampering_init(LayerContext data_layer,
                                 LayerContext hash_layer,
                                 const AntiTamperingConfig *config) {
  LayerContext new_layer;
  new_layer.app_context = NULL;

  if (!config || !config->hashes_storage) {
    ERROR_MSG("[ANTI_TAMPERING_INIT] Missing configuration");
    exit(1);
  }

  AntiTamperingState *state = malloc(sizeof(AntiTamperingState));
  if (!state) {
    exit(1);
  }

  for (int i = 0; i < MAX_FDS; i++) {
    state->mappings[i].file_fd = INVALID_FD;
    state->mappings[i].file_path = NULL;
    state->mappings[i].hash_path = NULL;
  }
  new_layer.internal_state = state;
  // one data layer and one hash layer
  // for multiple layers, a demultiplexer layer should be used
  state->data_layer = data_layer;
  state->hash_layer = hash_layer;

  // Configurable hash prefix
  state->hash_prefix = strdup(config->hashes_storage);

  // Mode selection (default: file mode)
  state->mode = config->mode;
  state->block_size = 0;
  if (state->mode == ANTI_TAMPERING_MODE_BLOCK) {
    require_block_size_or_exit(config);
    state->block_size = config->block_size;
  }

  // Initialize the path-based locking system
  state->lock_table = locking_init();
  if (!state->lock_table) {
    free(state);
    exit(1);
  }

  // Initialize the hasher with the specified algorithm
  if (hasher_init(&state->hasher, config->algorithm) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_INIT] Failed to initialize hasher with "
              "algorithm %d (%s)",
              config->algorithm, hash_algorithm_to_string(config->algorithm));
    locking_destroy(state->lock_table);
    free(state);
    exit(1);
  }

  INFO_MSG("[ANTI_TAMPERING_INIT] Hasher initialized successfully with "
           "algorithm: %s",
           hash_algorithm_to_string(config->algorithm));

  // NULL as the layers are in its internal state
  new_layer.next_layers = NULL;

  // Total number of layers at this point
  new_layer.nlayers = data_layer.nlayers + hash_layer.nlayers;

  LayerOps *ops = malloc(sizeof(LayerOps));
  if (!ops) {
    free(state);
    exit(1);
  }

  *ops = *ops_for_mode(state->mode);

  new_layer.ops = ops;

  return new_layer;
}

/**
 * @brief Check if an anti-tampering layer FD is valid and in use
 *
 * @param state -> AntiTamperingState pointer
 * @param fd    -> anti-tampering layer FD to check
 * @return int  -> 1 if valid and in use, 0 otherwise
 */

/**
 * @brief pwrite with anti-tampering guarantee - performs atomic write operation
 * with exclusive file locking to ensure data integrity
 *
 * This function:
 * 1. Acquires a WRITE lock on the file path
 * 2. Performs the write operation atomically
 * 3. Releases the lock after completion
 *
 * The write lock ensures that:
 * - No other operations (read/write/open/close) can access the file during
 * write
 * - Write operations are fully atomic and consistent
 * - No partial writes are visible to other processes
 *
 * @param fd       -> anti-tampering layer file descriptor
 * @param buffer   -> buffer to write
 * @param nbyte    -> number of bytes to write
 * @param offset   -> offset value
 * @param l        -> context of the anti-tampering layer
 * @return ssize_t -> number of written bytes from the file layer, or -1 on
 * error
 */
ssize_t anti_tampering_write(int fd, const void *buffer, size_t nbyte,
                             off_t offset, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (!is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;

  // Acquire exclusive lock for atomic write operation
  if (locking_acquire_write(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] Failed to acquire write lock on file %s "
              "(fd=%d)",
              file_path, file_fd);
    return -1;
  }

  // write to the file layer
  state->data_layer.app_context = l.app_context;
  ssize_t res = state->data_layer.ops->lpwrite(file_fd, buffer, nbyte, offset,
                                               state->data_layer);

  // Release the exclusive lock
  locking_release(state->lock_table, file_path);

  return res;
}

/**
 * @brief pread with anti-tampering guarantee - performs atomic read operation
 * with shared file locking to ensure data anti-tampering
 *
 * This function:
 * 1. Acquires a READ lock on the file path
 * 2. Performs the read operation atomically
 * 3. Releases the lock after completion
 *
 * The read lock ensures that:
 * - Multiple readers can access the file simultaneously
 * - Readers are blocked during write operations (preventing partial reads)
 * - Read operations see consistent file content
 * - No interference from concurrent writers
 *
 * @param fd       -> anti-tampering layer file descriptor
 * @param buff     -> buffer to read into
 * @param nbyte    -> number of bytes to read
 * @param offset   -> offset value
 * @param l        -> Context for the anti-tampering layer
 * @return ssize_t -> bytes read from the file layer, or -1 on error
 */
ssize_t anti_tampering_read(int fd, void *buff, size_t nbyte, off_t offset,
                            LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (!is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;

  // Acquire shared lock for consistent read operation
  if (locking_acquire_read(state->lock_table, file_path) != 0) {
    ERROR_MSG(
        "[ANTI_TAMPERING_READ] Failed to acquire read lock on file %s (fd=%d)",
        file_path, file_fd);
    return -1;
  }

  // read the file from the file layer
  state->data_layer.app_context = l.app_context;
  ssize_t res = state->data_layer.ops->lpread(file_fd, buff, nbyte, offset,
                                              state->data_layer);

  // Release the shared lock
  locking_release(state->lock_table, file_path);

  return res;
}

/**
 * @brief open anti-tampering layer - opens files and performs hash verification
 * with atomic locking to ensure data integrity
 *
 * This function:
 * 1. Opens the file in the underlying layer
 * 2. Constructs hash file path from the original file path
 * 3. If hash file exists, performs atomic hash verification:
 *    - Acquires READ lock on the file path
 *    - Reads stored hash and computes current file hash
 *    - Compares hashes while file is locked
 *    - Releases lock after verification
 *
 * The read lock during verification ensures that:
 * - Hash verification is done against consistent file content
 * - Multiple opens can verify simultaneously
 * - Writers are blocked during verification process
 *
 * @param pathname -> path of the file to open
 * @param flags    -> flags for opened file
 * @param mode     -> mode (permissions) when creating a file
 * @param l        -> LayerContext for current layer
 * @return int     -> anti-tampering layer file descriptor, or INVALID_FD on
 * error
 */
int anti_tampering_open(const char *pathname, int flags, mode_t mode,
                        LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (!pathname) {
    return INVALID_FD;
  }

  // open the file in the file layer
  state->data_layer.app_context = l.app_context;
  int file_fd =
      state->data_layer.ops->lopen(pathname, flags, mode, state->data_layer);

  if (file_fd < 0) {
    return file_fd;
  }

  if (file_fd >= MAX_FDS) {
    state->data_layer.ops->lclose(file_fd, state->data_layer);
    return INVALID_FD;
  }

  // Clean any existing mapping first
  if (state->mappings[file_fd].file_path) {
    free(state->mappings[file_fd].file_path);
  }
  if (state->mappings[file_fd].hash_path) {
    free(state->mappings[file_fd].hash_path);
  }

  // set the mapping for the file layer
  char *path_copy = strdup(pathname);
  if (!path_copy) {
    state->data_layer.ops->lclose(file_fd, state->data_layer);
    return INVALID_FD;
  }

  state->mappings[file_fd].file_fd = file_fd;
  state->mappings[file_fd].file_path = path_copy;
  state->mappings[file_fd].hash_path = NULL;

  // construct the hash file path, from the file path
  char *file_path_hex_hash =
      state->hasher.hash_buffer_hex(pathname, strlen(pathname));
  if (!file_path_hex_hash) {
    // Clean up allocated memory
    free(state->mappings[file_fd].file_path);
    state->mappings[file_fd].file_path = NULL;
    state->mappings[file_fd].file_fd = INVALID_FD;
    // Close the file we already opened
    state->data_layer.ops->lclose(file_fd, state->data_layer);
    return INVALID_FD;
  }

  // construct the hash file path, from the file path hash
  char *hash_pathname = construct_hash_pathname(state, file_path_hex_hash);
  if (!hash_pathname) {
    ERROR_MSG("[ANTI_TAMPERING] Failed to construct hash pathname");
    free(file_path_hex_hash);
    return -1;
  }
  free(file_path_hex_hash);

  // set the mapping for the hash layer
  char *hash_path_copy = strdup(hash_pathname);
  free(hash_pathname);

  if (!hash_path_copy) {
    free(state->mappings[file_fd].file_path);
    state->mappings[file_fd].file_path = NULL;
    state->mappings[file_fd].file_fd = INVALID_FD;
    state->data_layer.ops->lclose(file_fd, state->data_layer);
    return INVALID_FD;
  }
  state->mappings[file_fd].hash_path = hash_path_copy;

  // check if the hash file exists
  state->hash_layer.app_context = l.app_context;
  int hash_fd = state->hash_layer.ops->lopen(hash_path_copy, O_RDONLY, 0644,
                                             state->hash_layer);

  // if the hash file exists, we make a anti-tampering check
  // Skip verification in block mode - block mode uses per-block hashes and
  // verification happens during read operations, not on open
  if (hash_fd > 0 && state->mode == ANTI_TAMPERING_MODE_FILE) {
    // Open separate read-only FD for verification: original FD for locking,
    // verify FD for reading. This separation ensures the verification process
    // doesn't interfere with the file's current state or position, and provides
    // a clean read-only view of the file for hash calculation. But we'll do it
    // AFTER acquiring the lock on the original file to ensure atomicity
    state->data_layer.app_context = l.app_context;
    int verify_fd = state->data_layer.ops->lopen(path_copy, O_RDONLY, 0644,
                                                 state->data_layer);

    if (verify_fd > 0) {
      // Use the original file descriptor for locking, verify_fd for reading
      atomic_hash_verify(file_fd, verify_fd, hash_fd, state, path_copy, l);

      // close the verification file descriptor
      state->data_layer.ops->lclose(verify_fd, state->data_layer);
    } else {
      ERROR_MSG(
          "[ANTI_TAMPERING_OPEN] Failed to open verification fd for file %s",
          path_copy);
    }

    // close the hash file
    state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
  } else if (hash_fd > 0) {

    state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
  } else if (state->mode == ANTI_TAMPERING_MODE_FILE) {
    DEBUG_MSG("[ANTI_TAMPERING_OPEN] Hash file %s does not exist for file %s. "
              "Note: it is only created on close.",
              hash_path_copy, path_copy);
  }

  return file_fd;
}

/**
 * @brief close anti-tampering layer - performs atomic hash computation and
 * closes files with exclusive locking to ensure data integrity
 *
 * This function:
 * 1. Acquires WRITE lock on the file path
 * 2. Opens a new read-only file descriptor for hash computation
 * 3. Computes SHA-256 hash of the entire file content
 * 4. Stores the hash in the hash layer
 * 5. Releases the write lock
 * 6. Closes all file descriptors
 *
 * The write lock ensures that:
 * - Hash computation captures a consistent snapshot of the file
 * - No other operations can modify the file during hash computation
 * - Hash computation is fully atomic and reliable
 *
 * @param fd    -> anti-tampering layer file descriptor
 * @param l     -> LayerContext for current layer
 * @return int  -> 0 on success, INVALID_FD on error
 */
int anti_tampering_close(int fd, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (!is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  int result = 0; // Track the result of operations

  // get FD from the mapping
  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;
  char *hash_path = state->mappings[fd].hash_path;

  // Validate we have a valid file path
  if (!file_path) {
    return INVALID_FD;
  }

  // Make a copy of the paths since we'll need them after clearing the mapping
  char *file_path_copy = strdup(file_path);
  char *hash_path_copy = hash_path ? strdup(hash_path) : NULL;

  if (!file_path_copy) {
    if (hash_path_copy)
      free(hash_path_copy);
    return INVALID_FD;
  }

  // Clear the mapping
  free_file_mapping(&state->mappings[fd]);

  // Acquire exclusive lock on the original file for atomic hash computation
  if (locking_acquire_write(state->lock_table, file_path_copy) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_CLOSE] Failed to acquire write lock on file %s "
              "(fd=%d)",
              file_path_copy, file_fd);
    free(file_path_copy);
    if (hash_path_copy)
      free(hash_path_copy);
    return INVALID_FD;
  }

  // Open separate read-only FD for hash computation: avoids interfering with
  // original FD state New FD ensures clean read from start of file, regardless
  // of original FD's current position
  state->data_layer.app_context = l.app_context;
  int new_file_fd = state->data_layer.ops->lopen(file_path_copy, O_RDONLY, 0644,
                                                 state->data_layer);
  if (new_file_fd < 0) {
    locking_release(state->lock_table, file_path_copy); // Release lock on error
    free(file_path_copy);
    if (hash_path_copy)
      free(hash_path_copy);
    return 0;
  }

  // compute hash of the file (file is already locked)
  char *file_hex_hash =
      state->hasher.hash_file_hex(new_file_fd, state->data_layer);
  if (!file_hex_hash) {
    locking_release(state->lock_table, file_path_copy); // Release lock on error
    free(file_path_copy);
    if (hash_path_copy)
      free(hash_path_copy);
    state->data_layer.ops->lclose(new_file_fd, state->data_layer);
    return INVALID_FD;
  }

  // open the hash file in the hash layer using the computed hash path
  state->hash_layer.app_context = l.app_context;
  int hash_fd = state->hash_layer.ops->lopen(
      hash_path_copy, O_RDWR | O_CREAT | O_TRUNC, 0644, state->hash_layer);

  if (hash_fd < 0) {
    ERROR_MSG(
        "[ANTI_TAMPERING_CLOSE] Failed to open hash file %s; [HINT] use an "
        "absolute path for the hashes_storage: %s",
        hash_path_copy, state->hash_prefix);

    locking_release(state->lock_table, file_path_copy); // Release lock on error
    free(file_path_copy);
    if (hash_path_copy)
      free(hash_path_copy);
    free(file_hex_hash); // Free the computed hash
    state->data_layer.ops->lclose(
        new_file_fd, state->data_layer); // Close the hash computation fd
    return INVALID_FD;
  } else {
    DEBUG_MSG("[ANTI_TAMPERING_CLOSE] Hash file %s created for file %s",
              hash_path_copy, file_path_copy);
  }

  // write the hex hash string to the hash layer while still with the lock
  state->hash_layer.app_context = l.app_context;
  ssize_t hash_res = state->hash_layer.ops->lpwrite(
      hash_fd, file_hex_hash, strlen(file_hex_hash), 0, state->hash_layer);

  if (hash_res > 0) {
    DEBUG_MSG("[ANTI_TAMPERING_CLOSE] Hash file %s written (%ld bytes) to hash "
              "layer",
              hash_path_copy, hash_res);
  } else {
    ERROR_MSG("[ANTI_TAMPERING_CLOSE] Failed to write hash file %s to hash "
              "layer",
              hash_path_copy);
  }

  // Release the exclusive lock after hash computation and storage
  locking_release(state->lock_table, file_path_copy);

  free(file_hex_hash);
  if (hash_res < 0) {
    free(file_path_copy);
    if (hash_path_copy)
      free(hash_path_copy);
    state->data_layer.ops->lclose(new_file_fd, state->data_layer);
    return INVALID_FD;
  }

  // close the new file descriptor
  state->data_layer.app_context = l.app_context;
  if (new_file_fd != INVALID_FD) {
    int new_file_fd_close_res =
        state->data_layer.ops->lclose(new_file_fd, state->data_layer);
    if (new_file_fd_close_res < 0) {
      result = new_file_fd_close_res;
    }
  }

  // close the file layer if it exists
  state->data_layer.app_context = l.app_context;
  if (file_fd != INVALID_FD) {
    int file_fd_close_res =
        state->data_layer.ops->lclose(file_fd, state->data_layer);
    if (file_fd_close_res < 0) {
      result = file_fd_close_res;
    }
  }

  // close the hash layer if it exists
  state->hash_layer.app_context = l.app_context;
  if (hash_fd != INVALID_FD) {
    int hash_fd_close_res =
        state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
    if (hash_fd_close_res < 0) {
      result = hash_fd_close_res;
    }
  }

  free(file_path_copy);
  if (hash_path_copy)
    free(hash_path_copy);

  return result;
}

/**
 * @brief ftruncate anti-tampering layer - ftruncate the file in the data layer
 *
 * @param fd       -> file descriptor (master FD)
 * @param length   -> new length of the file
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the data layer
 */
int anti_tampering_ftruncate(int fd, off_t length, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (!is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  // Get the file descriptor from the mapping
  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;

  // Acquire exclusive lock for atomic write operation
  if (locking_acquire_write(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_FTRUNCATE] Failed to acquire write lock on file "
              "%s (fd=%d)",
              file_path, file_fd);
    return -1;
  }

  // Call the next layer's ftruncate operation
  state->data_layer.app_context = l.app_context;
  int res =
      state->data_layer.ops->lftruncate(file_fd, length, state->data_layer);

  // Release the exclusive lock
  locking_release(state->lock_table, file_path);

  return res;
}

/**
 * @brief Destroy the anti-tampering layer and free all resources
 *
 * @param l -> LayerContext to destroy
 */
void anti_tampering_destroy(LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!state) {
    return;
  }

  // Free all mappings
  for (int i = 0; i < MAX_FDS; i++) {
    free_file_mapping(&state->mappings[i]);
  }

  // Destroy the locking system
  if (state->lock_table) {
    locking_destroy(state->lock_table);
  }

  // Free the operations structure
  if (l.ops) {
    free(l.ops);
  }

  // Free the state itself
  free(state);
}

/**
 * @brief fstat anti-tampering layer - will pass the call to the data layer
 *
 * @param fd       -> file descriptor (master FD)
 * @param stbuf    -> pointer to the stat structure
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the data layer
 */
int anti_tampering_fstat(int fd, struct stat *stbuf, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  // Get the file descriptor from the mapping
  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;

  if (locking_acquire_read(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_FSTAT] Failed to acquire read lock on file %s "
              "(fd=%d)",
              file_path, file_fd);
    return -1;
  }

  state->data_layer.app_context = l.app_context;
  int res = state->data_layer.ops->lfstat(fd, stbuf, state->data_layer);

  locking_release(state->lock_table, file_path);

  return res;
}

int anti_tampering_lstat(const char *pathname, struct stat *stbuf,
                         LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;

  if (locking_acquire_read(state->lock_table, pathname) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_LSTAT] Failed to acquire read lock on file %s",
              pathname);
    return -1;
  }
  state->data_layer.app_context = l.app_context;
  int res = state->data_layer.ops->llstat(pathname, stbuf, state->data_layer);
  locking_release(state->lock_table, pathname);
  return res;
}

int anti_tampering_unlink(const char *pathname, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (locking_acquire_write(state->lock_table, pathname) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_UNLINK] Failed to acquire write lock on file %s",
              pathname);
    return -1;
  }

  state->data_layer.app_context = l.app_context;
  int res = state->data_layer.ops->lunlink(pathname, state->data_layer);
  if (res == 0) {
    // remove the hash file
    // construct the hash file path, from the file path
    char *file_path_hex_hash =
        state->hasher.hash_buffer_hex(pathname, strlen(pathname));
    if (!file_path_hex_hash) {
      ERROR_MSG("[ANTI_TAMPERING_UNLINK] Failed to get hex hash of file %s",
                pathname);
      return -1;
    }

    // construct the hash file path, from the file path hash
    char *hash_pathname = construct_hash_pathname(state, file_path_hex_hash);
    if (!hash_pathname) {
      ERROR_MSG("[ANTI_TAMPERING] Failed to construct hash pathname");
      free(file_path_hex_hash);
      return -1;
    }

    res = state->hash_layer.ops->lunlink(hash_pathname, state->hash_layer);
    free(hash_pathname);
  }
  locking_release(state->lock_table, pathname);
  return res;
}

/**
 * @brief Construct the hash pathname from the file path hex hash
 *
 * @param state -> AntiTamperingState
 * @param file_path_hex_hash -> file path hex hash
 * @return char* -> hash pathname
 */

