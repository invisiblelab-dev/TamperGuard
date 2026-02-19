#include "builder.h"
#include "../shared/enums/layer_type.h"
#include "../shared/types/layer_context.h"
#include "../shared/utils/hasher/hasher.h"
#include "utils.h"
#include <stddef.h>

/**
 * @brief Find layer configuration by name
 */
static LayerConfig *find_layer_config(Config *config, const char *layer_name) {
  for (int i = 0; i < config->n_layers; i++) {
    if (strcmp(config->layers[i].name, layer_name) == 0) {
      return &config->layers[i];
    }
  }
  return NULL;
}

/**
 * @brief Get the init function name for a given layer type
 *
 * @param type The layer type enum value
 * @return Pointer to the function name string, or NULL if not found
 */
const char *get_layer_init_function(LayerType type) {
  switch (type) {
  case LAYER_ANTI_TAMPERING:
    return LAYER_ANTI_TAMPERING_INIT;
  case LAYER_LOCAL:
    return LAYER_LOCAL_INIT;
  case LAYER_REMOTE:
    return LAYER_REMOTE_INIT;
  case LAYER_BLOCK_ALIGN:
    return LAYER_BLOCK_ALIGN_INIT;
  case LAYER_DEMULTIPLEXER:
    return LAYER_DEMULTIPLEXER_INIT;
  case LAYER_S3_OPENDAL:
    return LAYER_S3_OPENDAL_INIT;
  case LAYER_IPFS_OPENDAL:
    return LAYER_IPFS_OPENDAL_INIT;
  case LAYER_SOLANA:
    return LAYER_SOLANA_INIT;
  case LAYER_COMPRESSION:
    return LAYER_COMPRESSION_INIT;
  case LAYER_BENCHMARK:
    return LAYER_BENCHMARK_INIT;
  case LAYER_READ_CACHE:
    return LAYER_READ_CACHE_INIT;
  case LAYER_ENCRYPTION:
    return LAYER_ENCRYPTION_INIT;
  default:
    toml_error("Unknown layer type");
    return NULL; // Should not be reached
  }
}

/**
 * @brief Load the init function for a layer specified by the layer type
 */
static void *load_init_function(LayerType type) {
  // Get the init function name from constants
  const char *init_name = get_layer_init_function(type);

  // check if layer is external
  int is_external = (type == LAYER_S3_OPENDAL || type == LAYER_SOLANA ||
                     type == LAYER_IPFS_OPENDAL);

  // load appropriate shared library based on layer type
  void *layers_handle;
  if (is_external) {
    layers_handle = dlopen(EXTERNAL_LIB, RTLD_LAZY);
  } else {
    layers_handle = dlopen(SHARED_LIB, RTLD_LAZY);
  }

  if (!layers_handle) {
    toml_error("Failed to load shared libraries");
  }

  // load layer init function
  void *layer_init = dlsym(layers_handle, init_name);

  // check if init function was loaded correctly
  if (!layer_init) {
    char buf[256];
    (void)snprintf(buf, sizeof(buf), "Failed to load function %s", init_name);
    toml_error(buf);
  }

  return layer_init;
}

/**
 * @brief Build a single layer and its dependencies
 */
static LayerContext build_layer(Config *config, const char *layer_name) {
  LayerConfig *layer_config = find_layer_config(config, layer_name);
  if (!layer_config) {
    char buf[256];
    (void)snprintf(buf, sizeof(buf), "Layer not found: %s", layer_name);
    toml_error(buf);
  }

  // Handle each layer type with its specific initialization signature
  switch (layer_config->type) {
  case LAYER_LOCAL: {
    // Local layer has no dependencies
    LayerContext (*init)() = load_init_function(layer_config->type);
    return init();
  }

  case LAYER_REMOTE: {
    // Remote layer has no dependencies
    LayerContext (*init)() = load_init_function(layer_config->type);
    return init();
  }

  case LAYER_BLOCK_ALIGN: {
    // Block_align layer takes a single next layer as dependency
    const char *next_layer = layer_config->params.block_align.next_Layer;
    if (!next_layer) {
      toml_error("Block_align layer must have a 'next' layer");
    }
    LayerContext next_ctx = build_layer(config, next_layer);
    LayerContext (*init)(LayerContext *, int, size_t) =
        load_init_function(layer_config->type);
    return init(&next_ctx, 1, layer_config->params.block_align.block_size);
  }

  case LAYER_BENCHMARK: {
    const char *next_layer = layer_config->params.benchmark.next_Layer;
    if (!next_layer) {
      toml_error("Benchmark layer must have a 'next' layer");
    }
    int ops_rep = layer_config->params.benchmark.ops_reps;
    if (ops_rep == -1) {
      toml_error("Benchmark layer must have a 'reps' parameter, and it must be "
                 "greater than 0.");
    }
    LayerContext next_ctx = build_layer(config, next_layer);
    LayerContext (*init)(LayerContext *, int, int) =
        load_init_function(layer_config->type);
    return init(&next_ctx, 1, layer_config->params.benchmark.ops_reps);
  }

  case LAYER_READ_CACHE: {
    const char *next_layer = layer_config->params.read_cache.next_Layer;
    if (!next_layer) {
      toml_error("Read_Cache layer must have a 'next' layer");
    }
    LayerContext next_ctx = build_layer(config, next_layer);
    LayerContext (*init)(LayerContext *, int, size_t, size_t) =
        load_init_function(layer_config->type);
    return init(&next_ctx, 1, layer_config->params.read_cache.block_size,
                layer_config->params.read_cache.num_blocks);
  }

  case LAYER_S3_OPENDAL: {
    // S3 layer takes configuration parameters
    LayerContext (*init)(const char *, const char *, const char *, const char *,
                         const char *, const char *) =
        load_init_function(layer_config->type);
    return init(layer_config->params.s3_opendal.endpoint,
                layer_config->params.s3_opendal.access_key_id,
                layer_config->params.s3_opendal.secret_access_key,
                layer_config->params.s3_opendal.region,
                layer_config->params.s3_opendal.bucket,
                layer_config->params.s3_opendal.root);
  }

  case LAYER_IPFS_OPENDAL: {
    // IPFS layer takes configuration parameters
    LayerContext (*init)(const char *, const char *) =
        load_init_function(layer_config->type);
    return init(layer_config->params.ipfs_opendal.api_endpoint,
                layer_config->params.ipfs_opendal.root);
  }
  case LAYER_SOLANA: {
    // Solana layer takes configuration parameters
    LayerContext (*init)(const char *, const char *) =
        load_init_function(layer_config->type);
    return init(layer_config->params.solana.keypair_path,
                layer_config->params.solana.rpc_url);
  }

  case LAYER_ANTI_TAMPERING: {
    LayerContext data_layer =
        build_layer(config, layer_config->params.anti_tampering.data_layer);
    LayerContext hash_layer =
        build_layer(config, layer_config->params.anti_tampering.hash_layer);

    LayerContext (*init)(LayerContext, LayerContext,
                         const AntiTamperingConfig *) =
        load_init_function(layer_config->type);
    return init(data_layer, hash_layer, &layer_config->params.anti_tampering);
  }

  case LAYER_DEMULTIPLEXER: {
    // Demultiplexer layer takes an array of layers
    int n_layers = layer_config->params.demultiplexer.n_layers;
    if (n_layers == 0) {
      toml_error("Demultiplexer layer must have at least one layer");
    }

    char **passthrough_reads_names =
        layer_config->params.demultiplexer.passthrough_reads;
    int n_passthrough_reads =
        layer_config->params.demultiplexer.n_passthrough_reads;
    char **passthrough_writes_names =
        layer_config->params.demultiplexer.passthrough_writes;
    int n_passthrough_writes =
        layer_config->params.demultiplexer.n_passthrough_writes;
    char **enforced_layers_names =
        layer_config->params.demultiplexer.enforced_layers;
    int n_enforced_layers =
        layer_config->params.demultiplexer.n_enforced_layers;

    int *passthrough_reads = malloc(sizeof(int) * n_layers);
    int *passthrough_writes = malloc(sizeof(int) * n_layers);
    int *enforced_layers = malloc(sizeof(int) * n_enforced_layers);

    LayerContext *layers = malloc(sizeof(LayerContext) * n_layers);
    if (!layers) {
      toml_error("Failed to allocate memory for demultiplexer layers");
    }

    for (int i = 0; i < n_layers; i++) {
      char *next_layer_name = layer_config->params.demultiplexer.layers[i];
      layers[i] = build_layer(config, next_layer_name);

      // Check if layer name exists in passthrough arrays
      passthrough_reads[i] = is_layer_in_array(
          passthrough_reads_names, n_passthrough_reads, next_layer_name);
      passthrough_writes[i] = is_layer_in_array(
          passthrough_writes_names, n_passthrough_writes, next_layer_name);
      enforced_layers[i] = is_layer_in_array(
          enforced_layers_names, n_enforced_layers, next_layer_name);
    }

    LayerContext (*init)(LayerContext *, int, int *, int *, int *) =
        load_init_function(layer_config->type);
    LayerContext result = init(layers, n_layers, passthrough_reads,
                               passthrough_writes, enforced_layers);

    free(passthrough_reads);
    free(passthrough_writes);
    free(enforced_layers);

    return result;
  }

  case LAYER_COMPRESSION: {
    // Compression layer takes a single next layer as dependency
    const char *next_layer = layer_config->params.compression.next_layer;
    if (!next_layer) {
      toml_error("Compression layer must have a 'next' layer");
    }
    LayerContext next_ctx = build_layer(config, next_layer);
    LayerContext (*init)(LayerContext *, const CompressionConfig *) =
        load_init_function(layer_config->type);
    return init(&next_ctx, &layer_config->params.compression);
  }

  case LAYER_ENCRYPTION: {
    // Encryption layer takes a single next layer as dependency
    const char *next_layer = layer_config->params.encryption.next_layer;
    if (!next_layer) {
      toml_error("Encryption layer must have a 'next' layer");
    }
    LayerContext next_ctx = build_layer(config, next_layer);
    LayerContext (*init)(LayerContext *, const EncryptionConfig *) =
        load_init_function(layer_config->type);
    return init(&next_ctx, &layer_config->params.encryption);
  }

  default:
    toml_error("Unknown layer type");
    exit(1); // Should not be reached
  }
}

LayerContext build_layer_tree(Config *config) {
  return build_layer(config, config->root_layer);
}
