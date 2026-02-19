#ifndef __ENCRYPTION_CONFIG_H__
#define __ENCRYPTION_CONFIG_H__

#include "../../config/utils.h"

typedef struct {
  int block_size;
  char *next_layer;
  char *encryption_key;
  char *api_key;
  char *vault_addr;
  char *secret_path;
} EncryptionConfig;

/**
 * @brief Parse encryption layer parameters
 *
 */
static inline void encryption_parse_params(toml_datum_t layer_table,
                                           EncryptionConfig *config) {
  // get next layer name from the config params
  toml_datum_t next_layer = toml_get(layer_table, "next");

  if (next_layer.type == TOML_STRING) { // Check if next layer is defined
    config->next_layer = parse_string(
        next_layer); // configure the next layer in the EncryptionConfig
  } else {
    toml_error("Invalid next layer filed");
  }

  config->block_size = 4096; // Default block size
  toml_datum_t block_size = toml_get(layer_table, "block_size");
  if (block_size.type == TOML_INT64) {
    config->block_size = (int)block_size.u.int64;
  } else {
    toml_error("Invalid block_size field");
  }

  // Initialize optional fields
  config->api_key = NULL;
  config->vault_addr = NULL;
  config->secret_path = NULL;
  config->encryption_key = NULL;

  // Check for API key (optional - for fetching from Vault)
  toml_datum_t api_key = toml_get(layer_table, "api_key");
  if (api_key.type == TOML_STRING) {
    config->api_key = parse_string(api_key);
  }

  // Check for Vault address (optional)
  toml_datum_t vault_addr = toml_get(layer_table, "vault_addr");
  if (vault_addr.type == TOML_STRING) {
    config->vault_addr = parse_string(vault_addr);
  }

  // Check for secret path (required when using api_key)
  toml_datum_t secret_path = toml_get(layer_table, "secret_path");
  if (secret_path.type == TOML_STRING) {
    config->secret_path = parse_string(secret_path);
  }

  // Check for encryption key (optional - either this or api_key must be
  // provided)
  toml_datum_t encryption_key = toml_get(layer_table, "encryption_key");
  if (encryption_key.type == TOML_STRING) {
    config->encryption_key = parse_string(encryption_key);
  }

  // Validate: either encryption_key or api_key must be provided
  if (!config->encryption_key && !config->api_key) {
    toml_error("Either encryption_key or api_key must be provided");
  }

  // If api_key is provided, vault_addr and secret_path must also be provided
  if (config->api_key && !config->vault_addr) {
    toml_error("vault_addr must be provided when using api_key");
  }

  if (config->api_key && !config->secret_path) {
    toml_error("secret_path must be provided when using api_key");
  }
}

#endif
