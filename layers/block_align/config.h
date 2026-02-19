#ifndef __BLOCK_ALIGN_H__
#define __BLOCK_ALIGN_H__

#include "../../config/utils.h"
#include <stddef.h>

// BlockAlign layer configuration structure
typedef struct {
  char *next_Layer;
  size_t block_size;
} BlockAlignConfig;

/**
 * @brief Parse BlockAlign layer parameters
 */
static inline void block_align_parse_params(toml_datum_t layer_table,
                                            BlockAlignConfig *config) {
  toml_datum_t next = toml_get(layer_table, "next");
  toml_datum_t block_size = toml_get(layer_table, "block_size");
  long temp = parse_long(block_size);
  config->block_size = temp < 1 ? 4096 : temp;
  config->next_Layer = parse_string(next);
}

#endif // __BLOCK_ALIGN_H__
