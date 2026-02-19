#ifndef __METADATA_H__
#define __METADATA_H__

#include "types/services_context.h"
#include <stddef.h>

/**
 * @brief Initializes the metadata service.
 *
 * Creates and configures the RocksDB instance, as well as the
 * read and write options associated with the service.
 *
 * If @p config is NULL, default values are used.
 *
 * @param config Service configuration structure.
 */
void metadata_init(ServiceConfig *config);

/**
 * @brief Inserts or updates a key-value pair.
 *
 * If the key already exists, the associated value is replaced.
 *
 * @param key Pointer to the key.
 * @param key_size Key size in bytes.
 * @param value Pointer to the value.
 * @param value_size Value size in bytes.
 *
 * @return 0 on success, -1 on error.
 */
int metadata_put(char *key, size_t key_size, char *value, size_t value_size);

/**
 * @brief Retrieves the value associated with a key.
 *
 * The returned value is allocated internally by RocksDB and **must**
 * be freed by the caller using ::metadata_free().
 *
 * The size of the returned value is written to @p value_size.
 *
 * @param key Pointer to the key.
 * @param key_size Key size in bytes.
 * @param value_size Pointer where the size of the returned value
 *                   will be stored.
 *
 * @return Pointer to the value associated with the key,
 *         or NULL if the key does not exist or an error occurs.
 */
void *metadata_get(char *key, size_t key_size, size_t *value_size);

/**
 * @brief Removes a key-value pair from the storage.
 *
 * @param key Pointer to the key.
 * @param key_size Key size in bytes.
 *
 * @return 0 on success, -1 on error.
 */
int metadata_delete(char *key, size_t key_size);

/**
 * @brief Shuts down the metadata service and releases all internal resources.
 *
 * After this call, no other API function should be used
 * without a new call to ::metadata_init().
 */
void metadata_close();

/**
 * @brief Frees memory returned by ::metadata_get().
 *
 * This function must be used to release any pointer
 * returned by the metadata service.
 *
 * @param ptr Pointer to be freed.
 */
void metadata_free(void *ptr);

#endif
