#ifndef __BENCHMARK__
#define __BENCHMARK__

#include "../../config/utils.h"

// Benchmark layer configuration structure
typedef struct {
  char *next_Layer;
  int ops_reps;
} BenchmarkConfig;

/**
 * @brief Parse Benchmark layer parameters
 */
static inline void benchmark_parse_params(toml_datum_t layer_table,
                                          BenchmarkConfig *config) {
  toml_datum_t next = toml_get(layer_table, "next");
  toml_datum_t ops_reps = toml_get(layer_table, "reps");
  int temp = parse_int(ops_reps);
  config->ops_reps = temp <= 0 ? 1 : temp;
  config->next_Layer = parse_string(next);
}

#endif // __BENCHMARK__
