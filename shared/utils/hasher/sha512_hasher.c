#include "sha512_hasher.h"
#include "evp.h"
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#define SHA512_HASH_SIZE 64
#define SHA512_HASH_CHUNK_SIZE 4096

/**
 * @brief SHA512 hash file and return hex string
 */
char *sha512_hash_file_hex(int fd, LayerContext layer) {
  return evp_hash_file_hex(fd, layer, EVP_sha512(), sha512_get_hash_size(),
                           SHA512_HASH_CHUNK_SIZE);
}

/**
 * @brief SHA512 hash buffer and return hex string
 */
char *sha512_hash_buffer_hex(const void *data_buffer, size_t data_size) {
  return evp_hash_buffer_hex(data_buffer, data_size, EVP_sha512(),
                             sha512_get_hash_size());
}

/**
 * @brief SHA512 hash file and return binary hash
 */
int sha512_hash_file_binary(int fd, LayerContext layer, void *hash_buffer,
                            size_t hash_buffer_size) {
  return evp_hash_file_binary(fd, layer, EVP_sha512(), hash_buffer,
                              hash_buffer_size, sha512_get_hash_size(),
                              SHA512_HASH_CHUNK_SIZE);
}

/**
 * @brief SHA512 hash buffer and return binary hash
 */
int sha512_hash_buffer_binary(const void *data_buffer, size_t data_size,
                              void *hash_buffer, size_t hash_buffer_size) {
  return evp_hash_buffer_binary(data_buffer, data_size, EVP_sha512(),
                                hash_buffer, hash_buffer_size,
                                sha512_get_hash_size());
}

/**
 * @brief Get SHA512 binary hash size
 */
size_t sha512_get_hash_size(void) { return SHA512_HASH_SIZE; }

/**
 * @brief Get SHA512 hex string size
 */
size_t sha512_get_hex_size(void) {
  return SHA512_HASH_SIZE * 2 +
         1; // 64 bytes * 2 characters + 1 null terminator
}

/**
 * @brief Initialize SHA512 hasher
 */
int sha512_hasher_init(Hasher *hasher) {
  if (!hasher) {
    return -1;
  }

  hasher->algorithm = HASH_SHA512;
  hasher->hash_file_hex = sha512_hash_file_hex;
  hasher->hash_buffer_hex = sha512_hash_buffer_hex;
  hasher->hash_file_binary = sha512_hash_file_binary;
  hasher->hash_buffer_binary = sha512_hash_buffer_binary;
  hasher->get_hash_size = sha512_get_hash_size;
  hasher->get_hex_size = sha512_get_hex_size;

  return 0;
}
