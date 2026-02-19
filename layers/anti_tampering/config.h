#ifndef __ANTI_TAMPERING_CONFIG_H__
#define __ANTI_TAMPERING_CONFIG_H__

#include "../../config/utils.h"
#include "../../shared/utils/hasher/hasher.h"
#include <stddef.h>
#include <string.h>
#include <strings.h>

// Anti-tampering layer configuration structure
typedef enum {
  ANTI_TAMPERING_MODE_FILE,
  ANTI_TAMPERING_MODE_BLOCK,
} anti_tampering_mode_t;

typedef struct {
  char *data_layer;
  char *hash_layer;
  char *hashes_storage;
  hash_algorithm_t algorithm;
  anti_tampering_mode_t mode; // file or block mode
  size_t block_size;          // required for block mode (bytes)
} AntiTamperingConfig;

/**
 * @brief Convert algorithm string to hash_algorithm_t enum (case insensitive)
 */
static inline hash_algorithm_t
string_to_hash_algorithm(const char *algorithm_str) {
  if (strcasecmp(algorithm_str, "sha256") == 0)
    return HASH_SHA256;
  if (strcasecmp(algorithm_str, "sha512") == 0)
    return HASH_SHA512;

  char buf[256];
  snprintf(buf, sizeof(buf), "Invalid hash algorithm: %s; use: sha256, sha512",
           algorithm_str);
  toml_error(buf);
  return HASH_SHA256; // This line should never be reached due to toml_error
}

/**
 * @brief Convert hash_algorithm_t enum to string
 */
static inline const char *hash_algorithm_to_string(hash_algorithm_t algorithm) {
  switch (algorithm) {
  case HASH_SHA256:
    return "SHA256";
  case HASH_SHA512:
    return "SHA512";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Parse anti-tampering layer parameters
 */
static inline void anti_tampering_parse_params(toml_datum_t layer_table,
                                               AntiTamperingConfig *config) {
  toml_datum_t data_layer = toml_get(layer_table, "data_layer");
  if (data_layer.type != TOML_STRING) {
    toml_error("Anti-tampering layer must have a string for data_layer");
  }
  config->data_layer = strdup(data_layer.u.str.ptr);

  toml_datum_t hash_layer = toml_get(layer_table, "hash_layer");
  if (hash_layer.type != TOML_STRING) {
    toml_error("Anti-tampering layer must have a string for hash_layer");
  }
  config->hash_layer = strdup(hash_layer.u.str.ptr);

  toml_datum_t hashes_storage = toml_get(layer_table, "hashes_storage");
  if (hashes_storage.type != TOML_STRING) {
    toml_error("Anti-tampering layer must have a string for hashes_storage");
  }
  config->hashes_storage = strdup(hashes_storage.u.str.ptr);

  // Parse algorithm (optional, defaults to SHA256)
  toml_datum_t algorithm = toml_get(layer_table, "algorithm");
  if (algorithm.type == TOML_STRING) {
    config->algorithm = string_to_hash_algorithm(algorithm.u.str.ptr);
  } else {
    config->algorithm = HASH_SHA256; // Default to SHA256 if not specified
  }

  // Parse mode (optional, defaults to file)
  config->mode = ANTI_TAMPERING_MODE_FILE;
  toml_datum_t mode = toml_get(layer_table, "mode");
  if (mode.type == TOML_STRING) {
    if (strcasecmp(mode.u.str.ptr, "file") == 0) {
      config->mode = ANTI_TAMPERING_MODE_FILE;
    } else if (strcasecmp(mode.u.str.ptr, "block") == 0) {
      config->mode = ANTI_TAMPERING_MODE_BLOCK;
    } else {
      toml_error(
          "Anti-tampering layer has unsupported mode (use 'file' or 'block')");
    }
  }

  // Parse block_size (required only for block mode)
  config->block_size = 0;
  toml_datum_t block_size = toml_get(layer_table, "block_size");
  if (block_size.type == TOML_INT64) {
    config->block_size = (size_t)block_size.u.int64;
  } else if (config->mode == ANTI_TAMPERING_MODE_BLOCK) {
    toml_error("Anti-tampering layer in block mode must have an integer for "
               "block_size");
  }
}

#endif // __ANTI_TAMPERING_CONFIG_H__
