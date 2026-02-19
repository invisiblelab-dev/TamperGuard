#ifndef __READ_CACHE_H__
#define __READ_CACHE_H__

#include "../../../config/utils.h"

// CacheLayer layer configuration structure
typedef struct {
  char *next_Layer;
  size_t block_size;
  size_t num_blocks;
} ReadCacheLayerConfig;

/**
 * @brief Parse CacheLayer layer parameters
 */
static inline void read_cache_parse_params(toml_datum_t layer_table,
                                           ReadCacheLayerConfig *config) {
  toml_datum_t next = toml_get(layer_table, "next");
  toml_datum_t block_size = toml_get(layer_table, "block_size");
  toml_datum_t num_blocks = toml_get(layer_table, "num_blocks");
  config->next_Layer = parse_string(next);
  long temp = parse_long(block_size);
  config->block_size = (temp < 1) ? 4096 : temp;
  temp = parse_long(num_blocks);
  config->num_blocks = (temp < 1) ? 100 : temp;
}

#endif // __READ_CACHE_H__
