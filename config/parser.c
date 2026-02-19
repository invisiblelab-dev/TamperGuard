#include "parser.h"
#include "lib/tomlc17/src/tomlc17.h"
#include "types/services_context.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h> // For strcasecmp

/**
 * @brief Convert string to LayerType enum
 */
static LayerType string_to_layer_type(const char *type_str) {
  if (strcmp(type_str, "anti_tampering") == 0)
    return LAYER_ANTI_TAMPERING;
  if (strcmp(type_str, "local") == 0)
    return LAYER_LOCAL;
  if (strcmp(type_str, "remote") == 0)
    return LAYER_REMOTE;
  if (strcmp(type_str, "block_align") == 0)
    return LAYER_BLOCK_ALIGN;
  if (strcmp(type_str, "demultiplexer") == 0)
    return LAYER_DEMULTIPLEXER;
  if (strcmp(type_str, "s3_opendal") == 0)
    return LAYER_S3_OPENDAL;
  if (strcmp(type_str, "solana") == 0)
    return LAYER_SOLANA;
  if (strcmp(type_str, "ipfs_opendal") == 0)
    return LAYER_IPFS_OPENDAL;
  if (strcmp(type_str, "compression") == 0)
    return LAYER_COMPRESSION;
  if (strcmp(type_str, "benchmark") == 0)
    return LAYER_BENCHMARK;
  if (strcmp(type_str, "read_cache") == 0)
    return LAYER_READ_CACHE;
  if (strcmp(type_str, "encryption") == 0)
    return LAYER_ENCRYPTION;

  char buf[256];
  (void)snprintf(buf, sizeof(buf), "Unknown layer type: %s", type_str);
  toml_error(buf);
  return LAYER_LOCAL; // This line should never be reached due to toml_error
}

/**
 * @brief Convert log mode string to LogMode enum (case insensitive)
 */
static LogMode string_to_log_mode(const char *log_mode_str) {
  if (strcasecmp(log_mode_str, "disabled") == 0)
    return LOG_DISABLED;
  if (strcasecmp(log_mode_str, "screen") == 0)
    return LOG_SCREEN;
  if (strcasecmp(log_mode_str, "error") == 0)
    return LOG_ERROR;
  if (strcasecmp(log_mode_str, "warn") == 0)
    return LOG_WARN;
  if (strcasecmp(log_mode_str, "info") == 0)
    return LOG_INFO;
  if (strcasecmp(log_mode_str, "debug") == 0)
    return LOG_DEBUG;

  char buf[256];
  (void)snprintf(buf, sizeof(buf),
                 "Invalid log mode: %s; use: disabled, screen, "
                 "error, warn, info, debug",
                 log_mode_str);
  toml_error(buf);
  return LOG_DISABLED; // This line should never be reached due to toml_error
}
static ServiceType string_to_service_type(const char *service_type_str) {
  if (strcasecmp(service_type_str, "metadata") == 0) {
    return SERVICE_METADATA;
  }
  char buf[256];
  (void)snprintf(buf, sizeof(buf),
                 "Invalid service mode: %s; use: metadata or cache",
                 service_type_str);
  toml_error(buf);
  return SERVICE_METADATA;
}
static ServiceConfig *parse_service_config(toml_datum_t service_table) {
  ServiceConfig *config = malloc(sizeof(*config));
  toml_datum_t type_table = toml_get(service_table, "type");

  switch (string_to_service_type(type_table.u.s)) {

  case SERVICE_METADATA: {
    toml_datum_t cache_d = toml_get(service_table, "cache_size");
    toml_datum_t threads_d = toml_get(service_table, "threads");
    long cache_size = parse_long(cache_d);
    long threads = parse_long(threads_d);

    if (cache_size < 0 || threads < 0) {
      char buf[256];
      (void)snprintf(buf, sizeof(buf),
                     "Invalid cache size or number of backgroung threads (they "
                     "should be positive numbers)");
      toml_error(buf);
      free(config);
      exit(1);
    }

    config->type = SERVICE_METADATA;
    config->service.metadata.cache_size_bytes = (size_t)cache_size;
    config->service.metadata.num_background_threads = (size_t)threads;
    break;
  }
  }
  return config;
}
/**
 * @brief Parse layer-specific parameters based on type
 */
static void parse_layer_params(toml_datum_t layer_table, LayerType type,
                               LayerParams *params) {
  switch (type) {
  case LAYER_S3_OPENDAL:
    s3_opendal_parse_params(layer_table, &params->s3_opendal);
    break;
  case LAYER_IPFS_OPENDAL:
    ipfs_opendal_parse_params(layer_table, &params->ipfs_opendal);
    break;
  case LAYER_SOLANA:
    solana_parse_params(layer_table, &params->solana);
    break;
  case LAYER_ANTI_TAMPERING:
    anti_tampering_parse_params(layer_table, &params->anti_tampering);
    break;
  case LAYER_BLOCK_ALIGN:
    block_align_parse_params(layer_table, &params->block_align);
    break;
  case LAYER_READ_CACHE:
    read_cache_parse_params(layer_table, &params->read_cache);
    break;
  case LAYER_DEMULTIPLEXER:
    demultiplexer_parse_params(layer_table, &params->demultiplexer);
    break;
  case LAYER_LOCAL:
    local_parse_params(layer_table, &params->local);
    break;
  case LAYER_REMOTE:
    remote_parse_params(layer_table, &params->remote);
    break;
  case LAYER_COMPRESSION:
    compression_parse_params(layer_table, &params->compression);
    break;
  case LAYER_BENCHMARK:
    benchmark_parse_params(layer_table, &params->benchmark);
    break;
  case LAYER_ENCRYPTION:
    encryption_parse_params(layer_table, &params->encryption);
    break;
  }
}

/**
 * @brief Parse a single layer configuration
 */
static LayerConfig parse_layer_config(const char *layer_name,
                                      toml_datum_t layer_table) {
  LayerConfig config = {0};

  // Set layer name
  config.name = strdup(layer_name);
  if (!config.name) {
    toml_error("Failed to duplicate layer name");
  }

  // Get layer type
  toml_datum_t type_datum = toml_get(layer_table, "type");
  if (type_datum.type != TOML_STRING) {
    toml_error("Layer type must be a string");
  }
  config.type = string_to_layer_type(type_datum.u.s);

  // Parse type-specific parameters (including layer references)
  parse_layer_params(layer_table, config.type, &config.params);

  return config;
}

/**
 * @brief Parse the entire configuration
 */
Config parse_config(toml_datum_t root_table) {
  Config config = {0};

  // Get root layer
  toml_datum_t root = toml_get(root_table, "root");
  if (root.type != TOML_STRING) {
    toml_error("Root layer must be specified as a string");
  }
  config.root_layer = strdup(root.u.s);
  if (!config.root_layer) {
    toml_error("Failed to duplicate root layer name");
  }

  // Set default log_mode to LOG_DISABLED, override if found in TOML
  config.log_mode = LOG_DISABLED;

  // Get log_mode
  toml_datum_t log_mode = toml_get(root_table, "log_mode");
  if (log_mode.type != TOML_STRING) {
    toml_error("Log mode must be a string: disabled, screen, error, warn, "
               "info, debug");
  }
  // Convert log mode string to LogMode enum
  config.log_mode = string_to_log_mode(log_mode.u.s);

  toml_datum_t service_table = toml_get(root_table, "services");
  if (service_table.type == TOML_UNKNOWN)
    config.serviceConfig = NULL;
  else
    config.serviceConfig = parse_service_config(service_table);

  // Count non-root, non-log_mode keys (layer definitions)
  config.n_layers = 0;
  for (int i = 0; i < root_table.u.tab.size; i++) {
    const char *key = root_table.u.tab.key[i];
    if (strcmp(key, "root") != 0 && strcmp(key, "log_mode") != 0 &&
        strcmp(key, "services") != 0) {
      config.n_layers++;
    }
  }

  if (config.n_layers == 0) {
    toml_error("No layer definitions found");
  }

  // Allocate layer configurations
  config.layers = malloc(sizeof(LayerConfig) * config.n_layers);
  if (!config.layers) {
    toml_error("Failed to allocate memory for layer configurations");
  }

  // Parse each layer
  int layer_idx = 0;
  for (int i = 0; i < root_table.u.tab.size; i++) {
    const char *key = root_table.u.tab.key[i];
    if (strcmp(key, "root") == 0 || strcmp(key, "log_mode") == 0 ||
        strcmp(key, "services") == 0)
      continue;

    toml_datum_t layer_datum = root_table.u.tab.value[i];
    if (layer_datum.type != TOML_TABLE) {
      char buf[256];
      (void)snprintf(buf, sizeof(buf), "Layer %s must be a table", key);
      toml_error(buf);
    }

    config.layers[layer_idx] = parse_layer_config(key, layer_datum);
    layer_idx++;
  }

  return config;
}

/**
 * @brief Free configuration data structures
 */
void free_config(Config *config) {
  if (!config)
    return;

  if (config->root_layer) {
    free(config->root_layer);
  }

  for (int i = 0; i < config->n_layers; i++) {
    LayerConfig *layer = &config->layers[i];

    if (layer->name)
      free(layer->name);

    // Free type-specific parameters
    switch (layer->type) {
    case LAYER_S3_OPENDAL:
      if (layer->params.s3_opendal.endpoint)
        free(layer->params.s3_opendal.endpoint);
      if (layer->params.s3_opendal.access_key_id)
        free(layer->params.s3_opendal.access_key_id);
      if (layer->params.s3_opendal.secret_access_key)
        free(layer->params.s3_opendal.secret_access_key);
      if (layer->params.s3_opendal.bucket)
        free(layer->params.s3_opendal.bucket);
      if (layer->params.s3_opendal.region)
        free(layer->params.s3_opendal.region);
      if (layer->params.s3_opendal.root)
        free(layer->params.s3_opendal.root);
      break;
    case LAYER_IPFS_OPENDAL:
      if (layer->params.ipfs_opendal.api_endpoint)
        free(layer->params.ipfs_opendal.api_endpoint);
      if (layer->params.ipfs_opendal.root)
        free(layer->params.ipfs_opendal.root);
      break;
    case LAYER_SOLANA:
      if (layer->params.solana.keypair_path)
        free(layer->params.solana.keypair_path);
      if (layer->params.solana.rpc_url)
        free(layer->params.solana.rpc_url);
      break;
    case LAYER_ANTI_TAMPERING:
      if (layer->params.anti_tampering.data_layer)
        free(layer->params.anti_tampering.data_layer);
      if (layer->params.anti_tampering.hash_layer)
        free(layer->params.anti_tampering.hash_layer);
      if (layer->params.anti_tampering.hashes_storage)
        free(layer->params.anti_tampering.hashes_storage);
      break;
    case LAYER_DEMULTIPLEXER:
      // Free string arrays for demultiplexer layer
      for (int j = 0; j < layer->params.demultiplexer.n_layers; j++) {
        free(layer->params.demultiplexer.layers[j]);
      }
      if (layer->params.demultiplexer.layers)
        free((void *)layer->params.demultiplexer.layers);

      // Free passthrough_reads array
      for (int j = 0; j < layer->params.demultiplexer.n_passthrough_reads;
           j++) {
        free(layer->params.demultiplexer.passthrough_reads[j]);
      }
      if (layer->params.demultiplexer.passthrough_reads)
        free((void *)layer->params.demultiplexer.passthrough_reads);

      // Free passthrough_writes array
      for (int j = 0; j < layer->params.demultiplexer.n_passthrough_writes;
           j++) {
        free(layer->params.demultiplexer.passthrough_writes[j]);
      }
      if (layer->params.demultiplexer.passthrough_writes)
        free((void *)layer->params.demultiplexer.passthrough_writes);
      break;
    case LAYER_COMPRESSION:
      // No cleanup needed - only enum and int values
      break;
    case LAYER_BLOCK_ALIGN:
      free(layer->params.block_align.next_Layer);
      break;
    case LAYER_BENCHMARK:
      free(layer->params.benchmark.next_Layer);
      break;
    case LAYER_READ_CACHE:
      free(layer->params.read_cache.next_Layer);
      break;
    case LAYER_ENCRYPTION:
      free(layer->params.encryption.next_layer);
      break;
    default:
      break;
    }
  }

  if (config->layers) {
    free(config->layers);
  }
}
