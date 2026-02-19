#ifndef __S3_OPENDAL_CONFIG_H__
#define __S3_OPENDAL_CONFIG_H__

#include "../../../config/utils.h"

// S3 OpenDAL layer configuration structure
typedef struct {
  char *endpoint;
  char *access_key_id;
  char *secret_access_key;
  char *bucket;
  char *region;
  char *root;
} S3OpendalConfig;

/**
 * @brief Parse S3 OpenDAL layer parameters
 */
static inline void s3_opendal_parse_params(toml_datum_t layer_table,
                                           S3OpendalConfig *config) {
  toml_datum_t endpoint = toml_get(layer_table, "endpoint");
  if (endpoint.type != TOML_STRING) {
    toml_error("S3 OpenDAL layer must have an endpoint, which is a string");
  }
  toml_datum_t access_key_id = toml_get(layer_table, "access_key_id");
  if (access_key_id.type != TOML_STRING) {
    toml_error(
        "S3 OpenDAL layer must have an access_key_id, which is a string");
  }
  toml_datum_t secret_access_key = toml_get(layer_table, "secret_access_key");
  if (secret_access_key.type != TOML_STRING) {
    toml_error(
        "S3 OpenDAL layer must have a secret_access_key, which is a string");
  }
  toml_datum_t bucket = toml_get(layer_table, "bucket");
  if (bucket.type != TOML_STRING) {
    toml_error("S3 OpenDAL layer must have a bucket, which is a string");
  }
  toml_datum_t region = toml_get(layer_table, "region");
  if (region.type != TOML_STRING) {
    toml_error("S3 OpenDAL layer must have a region, which is a string");
  }
  toml_datum_t root = toml_get(layer_table, "root");
  if (root.type != TOML_STRING) {
    toml_error("S3 OpenDAL layer must have a root, which is a string");
  }

  config->endpoint = parse_string(endpoint);
  config->access_key_id = parse_string(access_key_id);
  config->secret_access_key = parse_string(secret_access_key);
  config->bucket = parse_string(bucket);
  config->region = parse_string(region);
  config->root = parse_string(root);
}

#endif // __S3_OPENDAL_CONFIG_H__
