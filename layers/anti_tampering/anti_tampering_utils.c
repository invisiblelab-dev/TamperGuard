#include "anti_tampering_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_FD (-1)

/**
 * @brief Check if a file descriptor is valid for the limit of MAX_FDS
 *
 * @param fd File descriptor to check
 * @return int 1 if valid, 0 otherwise
 */
int is_valid_anti_tampering_fd(int fd) { return (fd >= 0 && fd < MAX_FDS); }

/**
 * @brief Free the memory allocated for a FileMapping's file_path and hash_path
 *
 * @param mapping Pointer to the FileMapping to free
 */
void free_file_mapping(FileMapping *mapping) {
  if (mapping) {
    free(mapping->file_path);
    free(mapping->hash_path);

    mapping->file_fd = INVALID_FD;
    mapping->file_path = NULL;
    mapping->hash_path = NULL;
  }
}

/**
 * @brief Construct the hash pathname from the file path hex hash
 *
 * @param state AntiTamperingState containing hash_prefix
 * @param file_path_hex_hash Hex hash of the file path
 * @return char* Allocated hash pathname string, or NULL on error
 */
char *construct_hash_pathname(const AntiTamperingState *state,
                              const char *file_path_hex_hash) {
  if (!state || !file_path_hex_hash) {
    return NULL;
  }

  size_t path_len = strlen(state->hash_prefix) + strlen(file_path_hex_hash) +
                    7; // +6 for ".hash\0" + 1 for "/"
  char *hash_pathname = malloc(path_len);
  if (!hash_pathname) {
    return NULL;
  }

  (void)sprintf(hash_pathname, "%s/%s.hash", state->hash_prefix,
                file_path_hex_hash);
  return hash_pathname;
}

/**
 * @brief Ensure a hash file exists by creating it if necessary
 *
 * @param state AntiTamperingState containing hash_layer
 * @param path Path to the hash file
 * @param l LayerContext to pass to underlying layers
 * @return int 0 on success, -1 on error
 */
int ensure_hash_file_exists(AntiTamperingState *state, const char *path,
                            LayerContext l) {
  if (!state || !path) {
    errno = EINVAL;
    return -1;
  }
  state->hash_layer.app_context = l.app_context;
  int hash_fd = state->hash_layer.ops->lopen(path, O_RDWR | O_CREAT, 0644,
                                             state->hash_layer);
  if (hash_fd < 0) {
    return -1;
  }
  return state->hash_layer.ops->lclose(hash_fd, state->hash_layer);
}

/**
 * @brief Allocate buffer and hash multiple blocks into it.
 *
 * @param buffer Source buffer containing the data blocks
 * @param buffer_size Total size of the buffer in bytes
 * @param block_size Size of each full block in bytes
 * @param hasher Hasher instance
 * @param out_hex_chars Output: number of hex characters per hash
 * @param out_concat_len Output: total length of allocated buffer
 * @return Allocated and hashed buffer on success, NULL on error (errno is set)
 */
char *hash_blocks_to_hex(const void *buffer, size_t buffer_size,
                         size_t block_size, const Hasher *hasher,
                         size_t *out_hex_chars, size_t *out_concat_len) {
  if (!buffer || !hasher || !hasher->hash_buffer_hex || !hasher->get_hex_size ||
      buffer_size == 0 || block_size == 0 || !out_hex_chars ||
      !out_concat_len) {
    errno = EINVAL;
    return NULL;
  }

  // Calculate number of blocks (including partial last block)
  const size_t num_blocks = (buffer_size + block_size - 1) / block_size;

  // Calculate hex parameters
  const size_t hex_size = hasher->get_hex_size();
  if (hex_size < 2) {
    errno = EINVAL;
    return NULL;
  }
  *out_hex_chars = hex_size - 1;
  *out_concat_len = num_blocks * (*out_hex_chars);

  // Allocate destination buffer
  char *dest = malloc(*out_concat_len);
  if (!dest) {
    errno = ENOMEM;
    return NULL;
  }

  // Hash each block and concatenate hex strings
  for (size_t i = 0; i < num_blocks; i++) {
    const size_t offset = i * block_size;
    const void *blk = (const uint8_t *)buffer + offset;

    // Last block (which can also be the first if there's only one) may be
    // partial
    size_t blk_size;
    if (offset + block_size <= buffer_size) {
      // Full block
      blk_size = block_size;
    } else {
      // Partial last block
      blk_size = buffer_size - offset;
    }

    char *hex = hasher->hash_buffer_hex(blk, blk_size);
    if (!hex) {
      free(dest);
      errno = EIO;
      return NULL;
    }
    memcpy(dest + (i * (*out_hex_chars)), hex, *out_hex_chars);
    free(hex);
  }

  return dest;
}
