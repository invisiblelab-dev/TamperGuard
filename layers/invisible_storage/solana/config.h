#ifndef __SOLANA_CONFIG_H__
#define __SOLANA_CONFIG_H__

#include "../../../config/utils.h"

// Solana layer configuration structure
typedef struct {
  char *rpc_url;
  char *keypair_path;
} SolanaConfig;

/**
 * @brief Parse Solana layer parameters
 */
static inline void solana_parse_params(toml_datum_t layer_table,
                                       SolanaConfig *config) {
  toml_datum_t rpc_url = toml_get(layer_table, "rpc_url");
  if (rpc_url.type != TOML_STRING) {
    toml_error("Solana layer must have an rpc_url, which is a string");
  }
  toml_datum_t keypair_path = toml_get(layer_table, "keypair_path");
  if (keypair_path.type != TOML_STRING) {
    toml_error("Solana layer must have a keypair_path, which is a string");
  }

  config->rpc_url = parse_string(rpc_url);
  config->keypair_path = parse_string(keypair_path);
}

#endif // __SOLANA_CONFIG_H__
