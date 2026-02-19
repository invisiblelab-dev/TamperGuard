#ifndef CONFIG_DECLARATIONS_H
#define CONFIG_DECLARATIONS_H
#include "../lib/tomlc17/src/tomlc17.h"
#include "../shared/enums/layer_type.h"
#include "../shared/enums/log_mode.h"
#include "types/services_context.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration to break circular dependency with lib.h
struct layer_context;
typedef struct layer_context LayerContext;

// Include layer-specific parameter header files (no circular dependencies)
#include "../layers/anti_tampering/config.h"
#include "../layers/benchmark/config.h"
#include "../layers/block_align/config.h"
#include "../layers/cache/read_cache/config.h"
#include "../layers/compression/config.h"
#include "../layers/demultiplexer/config.h"
#include "../layers/encryption/config.h"
#include "../layers/invisible_storage/ipfs_opendal/config.h"
#include "../layers/invisible_storage/s3_opendal/config.h"
#include "../layers/invisible_storage/solana/config.h"
#include "../layers/local/config.h"
#include "../layers/remote/config.h"

#define LAYER_DEPS "layer_dependencies"
#define SHARED_LIB "libmodular.so"
#define EXTERNAL_LIB "libinvisible_storage_bindings.so"

// external layers from the external library
static const char *const external_layers[] = {"s3_opendal", "solana",
                                              "ipfs_opendal"};

// Union to hold layer-specific parameters
typedef union {
  S3OpendalConfig s3_opendal;
  IpfsOpendalConfig ipfs_opendal;
  SolanaConfig solana;
  AntiTamperingConfig anti_tampering;
  BlockAlignConfig block_align;
  DemultiplexerConfig demultiplexer;
  LocalConfig local;
  RemoteConfig remote;
  CompressionConfig compression;
  BenchmarkConfig benchmark;
  ReadCacheLayerConfig read_cache;
  EncryptionConfig encryption;
} LayerParams;

// New layer configuration structure
typedef struct layer_config {
  char *name;
  LayerType type;
  LayerParams params; // type-specific parameters
} LayerConfig;

// Configuration structure to hold all parsed data
typedef struct {
  char *root_layer;    // name of the root layer
  LayerConfig *layers; // array of layer configurations
  int n_layers;        // number of layers
  LogMode log_mode;    // logging mode
  ServiceConfig *serviceConfig;
} Config;

#endif // __CONFIG_DECLARATIONS_H__
