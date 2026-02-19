#include "../../shared/types/layer_context.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @file invisible_storage.h
 * @brief C API for the Invisible Storage library
 */

#ifndef INVISIBLE_STORAGE_H
#define INVISIBLE_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Destroy a storage context
 *
 * @param context The storage context to destroy
 */
void invisible_storage_destroy(void *context);

/**
 * @brief Initialize an S3Opendal storage service
 *
 * @return A pointer to the Layer context, or NULL if an error occurred
 */
LayerContext s3_opendal_init();

/**
 * @brief Initialize a Solana SDK storage service
 *
 * @return A pointer to the Layer context, or NULL if an error occurred
 */
LayerContext solana_init();

#ifdef __cplusplus
}
#endif

#endif /* INVISIBLE_STORAGE_H */
