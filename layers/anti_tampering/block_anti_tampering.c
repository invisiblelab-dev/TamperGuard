#include "block_anti_tampering.h"

#include "../../logdef.h"
#include "anti_tampering.h"
#include "anti_tampering_utils.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_FD (-1)

int block_anti_tampering_open(const char *pathname, int flags, __mode_t mode,
                              LayerContext l) {
  // Reuse the normal open path to populate fd->(file_path, hash_path) mapping
  int fd = anti_tampering_open(pathname, flags, mode, l);
  if (fd < 0) {
    return fd;
  }

  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!state || !is_valid_anti_tampering_fd(fd)) {
    return fd;
  }

  const char *hash_path = state->mappings[fd].hash_path;
  if (hash_path && hash_path[0] != '\0') {
    (void)ensure_hash_file_exists(state, hash_path, l);
  }

  return fd;
}

int block_anti_tampering_close(int fd, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!state || !is_valid_anti_tampering_fd(fd)) {
    return INVALID_FD;
  }

  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;
  char *hash_path = state->mappings[fd].hash_path;

  if (!file_path) {
    return INVALID_FD;
  }

  state->data_layer.app_context = l.app_context;
  int rc = 0;
  if (file_fd != INVALID_FD) {
    rc = state->data_layer.ops->lclose(file_fd, state->data_layer);
  }

  if (hash_path) {
    free(hash_path);
  }
  free(file_path);
  state->mappings[fd].file_fd = INVALID_FD;
  state->mappings[fd].file_path = NULL;
  state->mappings[fd].hash_path = NULL;

  return rc;
}

ssize_t block_anti_tampering_write(int fd, const void *buffer, size_t nbyte,
                                   off_t offset, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!state || !is_valid_anti_tampering_fd(fd)) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] Invalid file descriptor");
    return -1;
  }

  if (nbyte == 0) {
    return 0;
  }

  const size_t block_size = state->block_size;
  if (block_size == 0) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] Block size is 0");
    return -1;
  }

  const size_t first_block_idx = (size_t)(offset / (off_t)block_size);

  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;
  char *hash_path = state->mappings[fd].hash_path;
  if (!file_path || !hash_path) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] File path or hash path is NULL");
    return -1;
  }

  // Serialize data write + hash update.
  if (locking_acquire_write(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] Failed to acquire write lock on file %s "
              "(fd=%d)",
              file_path, file_fd);
    return -1;
  }

  // 1) Write to the data layer.
  state->data_layer.app_context = l.app_context;
  ssize_t res = state->data_layer.ops->lpwrite(file_fd, buffer, nbyte, offset,
                                               state->data_layer);
  if (res != (ssize_t)nbyte) {
    locking_release(state->lock_table, file_path);
    return res;
  }

  // 2) Compute per-block hashes (fixed-length hex strings).
  // hash_blocks_to_hex will calculate num_blocks internally, including partial
  // last block
  size_t hex_chars, concat_len;
  char *concat = hash_blocks_to_hex(buffer, nbyte, block_size, &state->hasher,
                                    &hex_chars, &concat_len);
  if (!concat) {
    locking_release(state->lock_table, file_path);
    return INVALID_FD;
  }

  // 3) Write concatenated hashes into the per-file hash file.
  // Each block hash occupies `hex_chars` bytes at offset block_idx*hex_chars.
  const off_t hash_off = (off_t)(first_block_idx * hex_chars);
  state->hash_layer.app_context = l.app_context;
  int hash_fd = state->hash_layer.ops->lopen(hash_path, O_RDWR | O_CREAT, 0644,
                                             state->hash_layer);
  if (hash_fd < 0) {
    free(concat);
    locking_release(state->lock_table, file_path);
    return INVALID_FD;
  }

  ssize_t hw = state->hash_layer.ops->lpwrite(hash_fd, concat, concat_len,
                                              hash_off, state->hash_layer);
  (void)state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
  free(concat);

  locking_release(state->lock_table, file_path);

  if (hw != (ssize_t)concat_len) {
    ERROR_MSG("[ANTI_TAMPERING_WRITE] Failed to write concatenated hashes into "
              "the per-file hash file");
    return -1;
  }
  return res;
}

ssize_t block_anti_tampering_read(int fd, void *buffer, size_t nbyte,
                                  off_t offset, LayerContext l) {
  AntiTamperingState *state = (AntiTamperingState *)l.internal_state;
  if (!state || !is_valid_anti_tampering_fd(fd)) {
    ERROR_MSG("[ANTI_TAMPERING_READ] Invalid file descriptor");
    return INVALID_FD;
  }

  if (nbyte == 0) {
    return 0;
  }

  const size_t block_size = state->block_size;
  if (block_size == 0) {
    ERROR_MSG("[ANTI_TAMPERING_READ] Block size is 0");
    return INVALID_FD;
  }

  const size_t first_block_idx = (size_t)(offset / (off_t)block_size);

  int file_fd = state->mappings[fd].file_fd;
  char *file_path = state->mappings[fd].file_path;
  char *hash_path = state->mappings[fd].hash_path;
  if (!file_path || !hash_path) {
    ERROR_MSG("[ANTI_TAMPERING_READ] File path or hash path is NULL");
    return INVALID_FD;
  }

  if (locking_acquire_read(state->lock_table, file_path) != 0) {
    ERROR_MSG("[ANTI_TAMPERING_READ] Failed to acquire read lock on file %s "
              "(fd=%d)",
              file_path, file_fd);
    return INVALID_FD;
  }

  // 1) Read full blocks
  state->data_layer.app_context = l.app_context;
  ssize_t rr = state->data_layer.ops->lpread(file_fd, buffer, nbyte, offset,
                                             state->data_layer);
  if (rr != (ssize_t)nbyte) {
    locking_release(state->lock_table, file_path);
    return rr;
  }

  // 2) Hash each individual block to hex
  // hash_blocks_to_hex will calculate num_blocks internally, including partial
  // last block
  size_t hex_chars, concat_len;
  char *computed = hash_blocks_to_hex(buffer, nbyte, block_size, &state->hasher,
                                      &hex_chars, &concat_len);
  if (!computed) {
    locking_release(state->lock_table, file_path);
    return -1;
  }

  // Calculate num_blocks from concat_len and hex_chars for comparison loop
  const size_t num_blocks = concat_len / hex_chars;

  char *stored = malloc(concat_len);
  if (!stored) {
    free(computed);
    locking_release(state->lock_table, file_path);
    ERROR_MSG(
        "[ANTI_TAMPERING_READ] Failed to allocate memory for stored hashes");
    return -1;
  }

  // 3) Read the stored hashes for these blocks
  memset(stored, 0, concat_len);
  const off_t hash_off = (off_t)(first_block_idx * hex_chars);
  state->hash_layer.app_context = l.app_context;
  int hash_fd = state->hash_layer.ops->lopen(hash_path, O_RDONLY, 0644,
                                             state->hash_layer);
  if (hash_fd >= 0) {
    (void)state->hash_layer.ops->lpread(hash_fd, stored, concat_len, hash_off,
                                        state->hash_layer);
    (void)state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
  }

  // 4) Compare and emit warnings on mismatch
  if (hash_fd < 0) {
    WARN_MSG("[ANTI_TAMPERING_BLOCK_READ] missing hash file file=%s hash=%s",
             file_path, hash_path);
  }

  for (size_t i = 0; i < num_blocks; i++) {
    const size_t off = i * hex_chars;
    if (memcmp(stored + off, computed + off, hex_chars) != 0) {
      char *s_stored = malloc(hex_chars + 1);
      char *s_computed = malloc(hex_chars + 1);
      if (s_stored && s_computed) {
        memcpy(s_stored, stored + off, hex_chars);
        s_stored[hex_chars] = '\0';
        memcpy(s_computed, computed + off, hex_chars);
        s_computed[hex_chars] = '\0';
        WARN_MSG("[ANTI_TAMPERING_BLOCK_READ] hash mismatch file=%s block=%zu "
                 "data_off=%ld stored=%s computed=%s",
                 file_path, first_block_idx + i,
                 (long)(offset + (off_t)(i * block_size)), s_stored,
                 s_computed);
      }
      free(s_stored);
      free(s_computed);
    }
  }

  free(computed);
  free(stored);
  locking_release(state->lock_table, file_path);
  return rr;
}

int block_anti_tampering_ftruncate(int fd, off_t length, LayerContext l) {
  return anti_tampering_ftruncate(fd, length, l);
}

int block_anti_tampering_fstat(int fd, struct stat *stbuf, LayerContext l) {
  return anti_tampering_fstat(fd, stbuf, l);
}

int block_anti_tampering_lstat(const char *pathname, struct stat *stbuf,
                               LayerContext l) {
  return anti_tampering_lstat(pathname, stbuf, l);
}

int block_anti_tampering_unlink(const char *pathname, LayerContext l) {
  return anti_tampering_unlink(pathname, l);
}
