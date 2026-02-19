#ifndef __LOCAL_CONFIG_H__
#define __LOCAL_CONFIG_H__

#include "../../config/utils.h"

// Local layer configuration structure
typedef struct {
  char *placeholder; // placeholder for now
} LocalConfig;

/**
 * @brief Parse local layer parameters
 */
static inline void local_parse_params(toml_datum_t layer_table,
                                      LocalConfig *config) {
  config->placeholder = NULL;
}

#endif // __LOCAL_CONFIG_H__
