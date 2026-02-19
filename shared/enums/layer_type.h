#ifndef LAYER_TYPE_H
#define LAYER_TYPE_H

#include <stdint.h>

/**
 * @file layer_type.h
 * @brief Layer type enumeration
 *
 * This header defines the enumeration for different types of layers
 * available in the modular I/O system.
 */

/**
 * @defgroup LayerInitFunctionNames Layer Initialization Function Names
 * @brief Constant string values for layer initialization function names.
 *
 * These constants represent the names of the initialization functions
 * for each layer type. The system uses these strings to dynamically
 * load and invoke the appropriate initialization routines for each
 * layer.
 * @{
 */
#define LAYER_ANTI_TAMPERING_INIT                                              \
  "anti_tampering_init" /**< Init function name for anti-tampering layer */
#define LAYER_LOCAL_INIT                                                       \
  "local_init" /**< Init function name for local storage layer */
#define LAYER_REMOTE_INIT                                                      \
  "remote_init" /**< Init function name for remote storage layer */
#define LAYER_COMPRESSION_INIT                                                 \
  "compression_init" /**< Init function name for compression layer */
#define LAYER_BLOCK_ALIGN_INIT                                                 \
  "block_align_init" /**< Init function name for block alignment layer */
#define LAYER_DEMULTIPLEXER_INIT                                               \
  "demultiplexer_init" /**< Init function name for demultiplexer layer */
#define LAYER_S3_OPENDAL_INIT                                                  \
  "s3_opendal_init" /**< Init function name for S3 OpenDAL layer */
#define LAYER_IPFS_OPENDAL_INIT                                                \
  "ipfs_opendal_init" /**< Init function name for IPFS OpenDAL layer */
#define LAYER_SOLANA_INIT                                                      \
  "solana_init" /**< Init function name for Solana layer */
#define LAYER_BENCHMARK_INIT                                                   \
  "benchmark_init" /**< Init function name for benchmark layer */
#define LAYER_READ_CACHE_INIT                                                  \
  "read_cache_init" /**< Init function name for cache read layer */
#define LAYER_ENCRYPTION_INIT                                                  \
  "encryption_init" /**< Init function name for encryption layer */
/** @} */

/**
 * @enum LayerType
 * @brief Enumeration of available layer types
 *
 * Each layer type represents a different functionality that can be
 * applied to I/O operations in the modular system.
 */
typedef enum {
  LAYER_ANTI_TAMPERING, /**< Anti-tampering layer */
  LAYER_LOCAL,          /**< Local storage layer */
  LAYER_REMOTE,         /**< Remote storage layer */
  LAYER_COMPRESSION,    /**< Compression layer */
  LAYER_BLOCK_ALIGN,    /**< Block alignment layer */
  LAYER_DEMULTIPLEXER,  /**< Demultiplexer layer */
  LAYER_S3_OPENDAL,     /**< S3 OpenDAL layer */
  LAYER_IPFS_OPENDAL,   /**< IPFS OpenDAL layer */
  LAYER_SOLANA,         /**< Solana layer */
  LAYER_BENCHMARK,      /**< Benchmark layer */
  LAYER_READ_CACHE,     /**< Read Cache Layer */
  LAYER_ENCRYPTION      /**< Encryption Layer */
} LayerType;

#endif /* LAYER_TYPE_H */
