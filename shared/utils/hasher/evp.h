#ifndef __EVP_H__
#define __EVP_H__

#include "../../types/layer_context.h"
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * @brief Generic EVP hash file and return hex string
 *
 * @param fd File descriptor to read from
 * @param layer LayerContext to use for reading data
 * @param evp_md EVP message digest to use (e.g., EVP_sha256(), EVP_sha512())
 * @param hash_size Expected hash size in bytes
 * @param chunk_size Chunk size for reading file data
 * @return char* Hex string of the hash (must be freed by caller), or NULL on
 * error
 */
char *evp_hash_file_hex(int fd, LayerContext layer, const EVP_MD *evp_md,
                        size_t hash_size, size_t chunk_size);

/**
 * @brief Generic EVP hash buffer and return hex string
 *
 * @param data_buffer Pointer to the input data to hash
 * @param data_size Size of the input data in bytes
 * @param evp_md EVP message digest to use (e.g., EVP_sha256(), EVP_sha512())
 * @param hash_size Expected hash size in bytes
 * @return char* Hex string of the hash (must be freed by caller), or NULL on
 * error
 */
char *evp_hash_buffer_hex(const void *data_buffer, size_t data_size,
                          const EVP_MD *evp_md, size_t hash_size);

/**
 * @brief Generic EVP hash file and return binary hash
 *
 * @param fd File descriptor to read from
 * @param layer LayerContext to use for reading data
 * @param evp_md EVP message digest to use (e.g., EVP_sha256(), EVP_sha512())
 * @param hash_buffer Output buffer for binary hash
 * @param hash_buffer_size Size of the output buffer
 * @param expected_hash_size Expected hash size in bytes
 * @param chunk_size Chunk size for reading file data
 * @return int Number of bytes written to hash_buffer, or -1 on error
 */
int evp_hash_file_binary(int fd, LayerContext layer, const EVP_MD *evp_md,
                         void *hash_buffer, size_t hash_buffer_size,
                         size_t expected_hash_size, size_t chunk_size);

/**
 * @brief Generic EVP hash buffer and return binary hash
 *
 * @param data_buffer Pointer to the input data to hash
 * @param data_size Size of the input data in bytes
 * @param evp_md EVP message digest to use (e.g., EVP_sha256(), EVP_sha512())
 * @param hash_buffer Output buffer for binary hash
 * @param hash_buffer_size Size of the output buffer
 * @param expected_hash_size Expected hash size in bytes
 * @return int Number of bytes written to hash_buffer, or -1 on error
 */
int evp_hash_buffer_binary(const void *data_buffer, size_t data_size,
                           const EVP_MD *evp_md, void *hash_buffer,
                           size_t hash_buffer_size, size_t expected_hash_size);

#endif
