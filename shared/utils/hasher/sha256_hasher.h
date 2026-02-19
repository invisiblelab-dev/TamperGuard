#ifndef __SHA256_HASHER_H__
#define __SHA256_HASHER_H__

#include "hasher.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief SHA256 hash file and return hex string
 *
 * @param fd File descriptor to read from
 * @param layer LayerContext to use for reading data
 * @return char* Hex string of the hash (must be freed by caller), or NULL on
 * error
 */
char *sha256_hash_file_hex(int fd, LayerContext layer);

/**
 * @brief SHA256 hash buffer and return hex string
 *
 * @param data_buffer Pointer to the input data to hash
 * @param data_size Size of the input data in bytes
 * @return char* Hex string of the hash (must be freed by caller), or NULL on
 * error
 */
char *sha256_hash_buffer_hex(const void *data_buffer, size_t data_size);

/**
 * @brief SHA256 hash file and return binary hash
 *
 * @param fd File descriptor to read from
 * @param layer LayerContext to use for reading data
 * @param hash_buffer Output buffer for binary hash
 * @param hash_buffer_size Size of the output buffer
 * @return int Number of bytes written to hash_buffer, or -1 on error
 */
int sha256_hash_file_binary(int fd, LayerContext layer, void *hash_buffer,
                            size_t hash_buffer_size);

/**
 * @brief SHA256 hash buffer and return binary hash
 *
 * @param data_buffer Pointer to the input data to hash
 * @param data_size Size of the input data in bytes
 * @param hash_buffer Output buffer for binary hash
 * @param hash_buffer_size Size of the output buffer
 * @return int Number of bytes written to hash_buffer, or -1 on error
 */
int sha256_hash_buffer_binary(const void *data_buffer, size_t data_size,
                              void *hash_buffer, size_t hash_buffer_size);

/**
 * @brief Get SHA256 binary hash size
 *
 * @return size_t Size of SHA256 hash in bytes (32)
 */
size_t sha256_get_hash_size(void);

/**
 * @brief Get SHA256 hex string size
 *
 * @return size_t Size of SHA256 hex string including null terminator (65)
 */
size_t sha256_get_hex_size(void);

/**
 * @brief Initialize SHA256 hasher
 *
 * @param hasher Pointer to the hasher to initialize
 * @return 0 on success, -1 on error
 */
int sha256_hasher_init(Hasher *hasher);

#endif /* __SHA256_HASHER_H__ */
