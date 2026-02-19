#include "loader.h"
#include "../logdef.h"
#include "../shared/types/layer_context.h"
#include "builder.h"
#include "parser.h"
#include "services/metadata.h"
#include "utils.h"

/**
 * @brief  Loads configuration file and builds the necessary layers
 *
 * @param filepath      -> toml config file path
 * @return LayerContext -> built layers
 */
LayerContext load_config_toml(char *filepath) {
  FILE *fp;

  // open config file
  fp = fopen(filepath, "r");
  if (!fp) {
    char buf[256];
    (void)snprintf(buf, sizeof(buf), "Failed to open config file: %s",
                   filepath);
    toml_error(buf);
  }

  // parse config file
  toml_result_t conf = toml_parse_file(fp);
  (void)fclose(fp);

  if (!conf.ok) {
    toml_error("Failed to parse TOML file");
  }

  // Parse the configuration
  Config config = parse_config(conf.toptab);

  // Initialize logging
  LOG_INIT(config.log_mode);
  DEBUG_MSG("Log mode integer: %d", config.log_mode);
  metadata_init(config.serviceConfig);
  // Build the layer tree starting from root
  LayerContext result = build_layer_tree(&config);

  // Cleanup
  free_config(&config);
  toml_free(conf);

  return result;
}
