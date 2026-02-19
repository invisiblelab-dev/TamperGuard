#ifndef __REMOTE_CONFIG_H__
#define __REMOTE_CONFIG_H__

#include "../../config/utils.h"

// Remote layer configuration structure
typedef struct {
  char *placeholder; // placeholder for now
} RemoteConfig;

/**
 * @brief Parse remote layer parameters
 */
static inline void remote_parse_params(toml_datum_t layer_table,
                                       RemoteConfig *config) {
  config->placeholder = NULL;
}

#endif // __REMOTE_CONFIG_H__
