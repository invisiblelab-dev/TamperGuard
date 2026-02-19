#ifndef __IPFS_OPENDAL_CONFIG_H__
#define __IPFS_OPENDAL_CONFIG_H__

#include "../../../config/utils.h"

// IPFS OpenDAL layer configuration structure
typedef struct {
  char *api_endpoint;
  char *root;
} IpfsOpendalConfig;

/**
 * @brief Parse IPFS OpenDAL layer parameters
 */
static inline void ipfs_opendal_parse_params(toml_datum_t layer_table,
                                             IpfsOpendalConfig *config) {
  toml_datum_t api_endpoint = toml_get(layer_table, "api_endpoint");
  if (api_endpoint.type != TOML_STRING) {
    toml_error(
        "IPFS OpenDAL layer must have an api_endpoint, which is a string");
  }
  toml_datum_t root = toml_get(layer_table, "root");
  if (root.type != TOML_STRING) {
    toml_error("IPFS OpenDAL layer must have a root, which is a string");
  }

  config->api_endpoint = parse_string(api_endpoint);
  config->root = parse_string(root);
}

#endif // __IPFS_OPENDAL_CONFIG_H__
