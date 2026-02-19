#include "evp.h"
#include "../conversion.h"
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Generic EVP hash file and return hex string
 */
char *evp_hash_file_hex(int fd, LayerContext layer, const EVP_MD *evp_md,
                        size_t hash_size, size_t chunk_size) {
  if (fd < 0 || !layer.ops || !layer.ops->lpread || !evp_md) {
    return NULL;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  char *hex_hash_str;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == NULL) {
    return NULL;
  }

  if (EVP_DigestInit_ex(mdctx, evp_md, NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  char *read_buffer = malloc(chunk_size);
  if (read_buffer == NULL) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  ssize_t bytes_read;
  off_t offset = 0;
  while ((bytes_read = layer.ops->lpread(fd, read_buffer, chunk_size, offset,
                                         layer)) > 0) {
    if (EVP_DigestUpdate(mdctx, read_buffer, bytes_read) != 1) {
      free(read_buffer);
      EVP_MD_CTX_free(mdctx);
      return NULL;
    }
    offset += bytes_read;
  }

  free(read_buffer);

  if (bytes_read < 0) {
    // Error during read
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  EVP_MD_CTX_free(mdctx);

  // Calculate hex string size (hash_size * 2 + 1 for null terminator)
  size_t hex_size = hash_size * 2 + 1;
  hex_hash_str = malloc(hex_size);
  if (hex_hash_str == NULL) {
    return NULL;
  }

  bytes_to_hex(hash, hash_len, hex_hash_str);
  return hex_hash_str;
}

/**
 * @brief Generic EVP hash buffer and return hex string
 */
char *evp_hash_buffer_hex(const void *data_buffer, size_t data_size,
                          const EVP_MD *evp_md, size_t hash_size) {
  if (!data_buffer || !evp_md) {
    return NULL;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  char *hex_hash_str;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == NULL) {
    return NULL;
  }

  if (EVP_DigestInit_ex(mdctx, evp_md, NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  if (EVP_DigestUpdate(mdctx, data_buffer, data_size) != 1) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    return NULL;
  }

  EVP_MD_CTX_free(mdctx);

  // Calculate hex string size (hash_size * 2 + 1 for null terminator)
  size_t hex_size = hash_size * 2 + 1;
  hex_hash_str = malloc(hex_size);
  if (hex_hash_str == NULL) {
    return NULL;
  }

  bytes_to_hex(hash, hash_len, hex_hash_str);
  return hex_hash_str;
}

/**
 * @brief Generic EVP hash file and return binary hash
 */
int evp_hash_file_binary(int fd, LayerContext layer, const EVP_MD *evp_md,
                         void *hash_buffer, size_t hash_buffer_size,
                         size_t expected_hash_size, size_t chunk_size) {
  if (fd < 0 || !layer.ops || !layer.ops->lpread || !hash_buffer || !evp_md) {
    return -1;
  }

  if (hash_buffer_size < expected_hash_size) {
    return -1;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == NULL) {
    return -1;
  }

  if (EVP_DigestInit_ex(mdctx, evp_md, NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  char *read_buffer = malloc(chunk_size);
  if (read_buffer == NULL) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  ssize_t bytes_read;
  off_t offset = 0;
  while ((bytes_read = layer.ops->lpread(fd, read_buffer, chunk_size, offset,
                                         layer)) > 0) {
    if (EVP_DigestUpdate(mdctx, read_buffer, bytes_read) != 1) {
      free(read_buffer);
      EVP_MD_CTX_free(mdctx);
      return -1;
    }
    offset += bytes_read;
  }

  free(read_buffer);

  if (bytes_read < 0) {
    // Error during read
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  EVP_MD_CTX_free(mdctx);

  // Defensive check: ensure OpenSSL returned expected hash size
  if (hash_len != expected_hash_size) {
    return -1; // Unexpected hash length - indicates bug or corruption
  }

  // Copy hash to output buffer
  memcpy(hash_buffer, hash, hash_len);
  return (int)hash_len;
}

/**
 * @brief Generic EVP hash buffer and return binary hash
 */
int evp_hash_buffer_binary(const void *data_buffer, size_t data_size,
                           const EVP_MD *evp_md, void *hash_buffer,
                           size_t hash_buffer_size, size_t expected_hash_size) {
  if (!data_buffer || !hash_buffer || !evp_md) {
    return -1;
  }

  if (hash_buffer_size < expected_hash_size) {
    return -1;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == NULL) {
    return -1;
  }

  if (EVP_DigestInit_ex(mdctx, evp_md, NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  if (EVP_DigestUpdate(mdctx, data_buffer, data_size) != 1) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(mdctx);
    return -1;
  }

  EVP_MD_CTX_free(mdctx);

  // Defensive check: ensure OpenSSL returned expected hash size
  if (hash_len != expected_hash_size) {
    return -1; // Unexpected hash length - indicates bug or corruption
  }

  // Copy hash to output buffer
  memcpy(hash_buffer, hash, hash_len);
  return (int)hash_len;
}
