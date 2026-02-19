#ifndef __DEMULTIPLEXER_CONFIG_H__
#define __DEMULTIPLEXER_CONFIG_H__

#include "../../config/utils.h"

// Demultiplexer layer configuration structure
typedef struct {
  char **layers;
  int n_layers;
  char **passthrough_reads;
  int n_passthrough_reads;
  char **passthrough_writes;
  int n_passthrough_writes;
  char **enforced_layers;
  int n_enforced_layers;
} DemultiplexerConfig;

/**
 * @brief Parse demultiplexer layer parameters
 */
static inline void demultiplexer_parse_params(toml_datum_t layer_table,
                                              DemultiplexerConfig *config) {
  toml_datum_t layers = toml_get(layer_table, "layers");
  if (layers.type != TOML_ARRAY || layers.u.arr.size == 0) {
    toml_error(
        "Demultiplexer layer must have an array of layers, with at least "
        "one layer");
  }

  config->layers = parse_string_array(layers, &config->n_layers);

  // Parse optional settings from options table
  toml_datum_t options_table = toml_get(layer_table, "options");
  if (options_table.type == TOML_TABLE) {
    // Parse optional passthrough_reads array
    toml_datum_t passthrough_reads =
        toml_get(options_table, "passthrough_reads");
    if (passthrough_reads.type == TOML_ARRAY) {
      config->passthrough_reads =
          parse_string_array(passthrough_reads, &config->n_passthrough_reads);
    } else {
      config->passthrough_reads = NULL;
      config->n_passthrough_reads = 0;
    }

    // Parse optional passthrough_writes array
    toml_datum_t passthrough_writes =
        toml_get(options_table, "passthrough_writes");
    if (passthrough_writes.type == TOML_ARRAY) {
      config->passthrough_writes =
          parse_string_array(passthrough_writes, &config->n_passthrough_writes);
    } else {
      config->passthrough_writes = NULL;
      config->n_passthrough_writes = 0;
    }

    // Parse optional enforced_layers array
    toml_datum_t enforced_layers = toml_get(options_table, "enforced_layers");
    if (enforced_layers.type == TOML_ARRAY) {
      config->enforced_layers =
          parse_string_array(enforced_layers, &config->n_enforced_layers);
    } else {
      config->enforced_layers = NULL;
      config->n_enforced_layers = 0;
    }
  } else {
    // No options table - set all optional settings to defaults
    config->passthrough_reads = NULL;
    config->n_passthrough_reads = 0;
    config->passthrough_writes = NULL;
    config->n_passthrough_writes = 0;
    config->enforced_layers = NULL;
    config->n_enforced_layers = 0;
  }
}

#endif // __DEMULTIPLEXER_CONFIG_H__
