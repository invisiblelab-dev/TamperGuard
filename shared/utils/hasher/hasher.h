#ifndef __HASHER_H__
#define __HASHER_H__

#include "../../types/layer_context.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// Hash algorithm types
typedef enum { HASH_SHA256, HASH_SHA512 } hash_algorithm_t;

/**
 * @brief Hasher structure for handling different hash algorithms
 *
 * This structure provides a unified interface for different hash
 * algorithms (SHA256 and SHA512) through function pointers. It
 * allows switching between hash methods at runtime without changing the
 * calling code.
 *
 * @example
 * ```c
 * Hasher hasher;
 * hasher_init(&hasher, HASH_SHA256);
 *
 * // Hash a file and get hex string
 * char *hex_hash = hasher.hash_file_hex(fd, layer);
 * if (hex_hash) {
 *     printf("File hash: %s\n", hex_hash);
 *     free(hex_hash);
 * }
 *
 * // Hash a buffer and get hex string
 * char *buffer_hash = hasher.hash_buffer_hex(data, data_size);
 * if (buffer_hash) {
 *     printf("Buffer hash: %s\n", buffer_hash);
 *     free(buffer_hash);
 * }
 * ```
 */
typedef struct Hasher {
  /** @brief Hash algorithm type */
  hash_algorithm_t algorithm;

  /**
   * @brief Function pointer for hashing a file and returning hex string
   *
   * @param fd File descriptor to read from
   * @param layer LayerContext to use for reading data
   * @return char* Hex string of the hash (must be freed by caller), or NULL on
   * error
   *
   * @note This function reads the file in chunks for memory efficiency
   * @note The returned string must be freed by the caller using free()
   */
  char *(*hash_file_hex)(int fd, LayerContext layer);

  /**
   * @brief Function pointer for hashing a buffer and returning hex string
   *
   * @param data_buffer Pointer to the input data to hash
   * @param data_size Size of the input data in bytes
   * @return char* Hex string of the hash (must be freed by caller), or NULL on
   * error
   *
   * @note The returned string must be freed by the caller using free()
   */
  char *(*hash_buffer_hex)(const void *data_buffer, size_t data_size);

  /**
   * @brief Function pointer for hashing a file and returning binary hash
   *
   * @param fd File descriptor to read from
   * @param layer LayerContext to use for reading data
   * @param hash_buffer Output buffer for binary hash
   * @param hash_buffer_size Size of the output buffer
   * @return int Number of bytes written to hash_buffer, or -1 on error
   *
   * @note The output buffer must be at least get_hash_size() bytes
   */
  int (*hash_file_binary)(int fd, LayerContext layer, void *hash_buffer,
                          size_t hash_buffer_size);

  /**
   * @brief Function pointer for hashing a buffer and returning binary hash
   *
   * @param data_buffer Pointer to the input data to hash
   * @param data_size Size of the input data in bytes
   * @param hash_buffer Output buffer for binary hash
   * @param hash_buffer_size Size of the output buffer
   * @return int Number of bytes written to hash_buffer, or -1 on error
   *
   * @note The output buffer must be at least get_hash_size() bytes
   */
  int (*hash_buffer_binary)(const void *data_buffer, size_t data_size,
                            void *hash_buffer, size_t hash_buffer_size);

  /**
   * @brief Function pointer for getting the size of the binary hash
   *
   * @return size_t Size of the binary hash in bytes
   */
  size_t (*get_hash_size)(void);

  /**
   * @brief Function pointer for getting the size of the hex hash string
   *
   * @return size_t Size of the hex hash string including null terminator
   */
  size_t (*get_hex_size)(void);
} Hasher;

/**
 * @brief Initialize a hasher with the specified algorithm
 *
 * @param hasher Pointer to the hasher to initialize
 * @param algorithm Hash algorithm to use
 * @return 0 on success, -1 on error
 */
int hasher_init(Hasher *hasher, hash_algorithm_t algorithm);

#endif
