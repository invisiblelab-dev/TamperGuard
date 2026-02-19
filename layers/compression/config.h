#ifndef __COMPRESSION_CONFIG_H__
#define __COMPRESSION_CONFIG_H__

#include "../../config/utils.h"
#include "../../shared/utils/compressor/compressor.h"

// Compression layer modes
typedef enum {
  COMPRESSION_MODE_FILE,
  COMPRESSION_MODE_SPARSE_BLOCK,
} compression_mode_t;

// Compression layer configuration structure
typedef struct {
  compression_algorithm_t algorithm;
  int level;
  char *next_layer;
  compression_mode_t mode; // file or block mode
  int block_size;          // required for block mode (bytes)
  int free_space; // option: enable fallocate punch (only for sparse_block)
} CompressionConfig;

/**
 * @brief Parse compression layer parameters
 */
static inline void compression_parse_params(toml_datum_t layer_table,
                                            CompressionConfig *config) {
  // Parse algorithm
  toml_datum_t algorithm = toml_get(layer_table, "algorithm");
  if (algorithm.type == TOML_STRING) {
    if (strcmp(algorithm.u.s, "lz4") == 0) {
      config->algorithm = COMPRESSION_LZ4;
    } else if (strcmp(algorithm.u.s, "zstd") == 0) {
      config->algorithm = COMPRESSION_ZSTD;
    } else {
      toml_error("Unsupported compression algorithm");
    }
  } else {
    toml_error("Invalid compression algorithm field");
  }

  // Parse next layer
  toml_datum_t next_layer = toml_get(layer_table, "next");
  if (next_layer.type == TOML_STRING) {
    config->next_layer = parse_string(next_layer);
  } else {
    toml_error("Invalid next layer field");
  }

  // Parse compression level
  toml_datum_t level = toml_get(layer_table, "level");
  if (level.type == TOML_INT64) {
    config->level = level.u.int64;
  } else {
    toml_error("Invalid compression level field");
  }

  config->mode = COMPRESSION_MODE_FILE;
  toml_datum_t mode = toml_get(layer_table, "mode");
  if (mode.type == TOML_STRING) {
    if (strcmp(mode.u.s, "file") == 0) {
      config->mode = COMPRESSION_MODE_FILE;
    } else if (strcmp(mode.u.s, "sparse_block") == 0) {
      config->mode = COMPRESSION_MODE_SPARSE_BLOCK;
    } else {
      toml_error("Unsupported compression mode (use 'file' or "
                 "'sparse_block')");
    }
  } else {
    toml_error("Invalid compression mode field");
  }

  // Parse block_size (optional for file, recommended for block)
  config->block_size = 4096; // sensible default
  toml_datum_t block_size = toml_get(layer_table, "block_size");
  if (block_size.type == TOML_INT64) {
    config->block_size = (int)block_size.u.int64;
  }

  // Parse options table (currently only free_space) valid for sparse_block
  config->free_space = 0; // default disabled
  if (config->mode == COMPRESSION_MODE_SPARSE_BLOCK) {
    toml_datum_t options = toml_get(layer_table, "options");
    if (options.type == TOML_TABLE) {
      toml_datum_t free_space = toml_get(options, "free_space");
      if (free_space.type == TOML_BOOLEAN) {
        config->free_space = free_space.u.boolean ? 1 : 0;
      }
    }
  }
}

#endif
