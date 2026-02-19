#pragma once
#include "cachelib/allocator/CacheAllocator.h"
#include <cstddef>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CacheWrapper {
  std::unique_ptr<facebook::cachelib::LruAllocator> cache;
  facebook::cachelib::PoolId defaultPool;
};

struct CacheEntry {
  const void *block;
  size_t size;
};

/**
 * @brief Initializes the cache.
 *
 * Calculates the necessary size to allocate, considering the minimum size
 * of a slab (minimum amount of memory CacheLib has to allocate).
 *
 * @param num_blocks Maximum number of blocks the cache will store
 * @param block_size Maximum size of each block
 * @param name Unique name for the cache
 *
 * @return Pointer to a CacheWrapper struct or Null in the case of a failed
 * initialization.
 *
 * @warning Because of memory padding and metadata overhead, the calculated size
 * is an approximation.
 */
void *initialize_cache(size_t num_blocks, size_t block_size, char *name);

/**
 * @brief Inserts an item in the cache.
 *
 * Performs an insertion in the cache, updating it according to the chosen
 * eviction policy on initialization.
 *
 * @param cache_wrapper Pointer to a CacheWrapper
 * @param key Block key
 * @param block Pointer to a block to copy the content from
 * @param block_length Length of the block to insert
 *
 * @return 0 if the item was inserted correctly and -1 if the operation failed.
 */
int insert_item(void *cache_wrapper, const char *key, const void *block,
                size_t block_length);

/**
 * @brief Checks the cache for an item.
 *
 * Checks if the cache has the block identified by its key.
 * In case of a hit, the provided CacheEntry is filled with the cached content.
 * Otherwise, item->size is set to -1.
 *
 * @param cache_wrapper Pointer to a CacheWrapper
 * @param key Block key
 * @param item Pointer to a CacheEntry to fill with the result
 */
void get_item(void *cache_wrapper, const char *key, CacheEntry *item);

/**
 * @brief Checks if the key exists in the cache.
 *
 * Checks if the cache has the block identified by its key.
 *
 * @param cache_wrapper Pointer to a CacheWrapper
 * @param key Block key
 *
 * @return 1 in the case of a cache hit and 0 in the case of a cache miss.
 */
int contain_item(void *cache_wrapper, const char *key);

/* @brief Removes an item from the cache.
 *
 * Removes an item identified by its key if it exists.
 *
 * @param cache_wrapper Pointer to a CacheWrapper
 * @param key Block key
 *
 * @return 0 in the case that the item was removed and -1 in the case that the
 * item was not removed (either by not existing or internal error in finding
 * it).
 */
int remove_item(void *cache_wrapper, const char *key);

/**
 * @brief Returns the number of cached items a wrapper has.
 *
 * @param cache_wrapper Pointer to a CacheWrapper
 *
 * @return number of items in the pool.
 */
unsigned long get_item_count(void *cache_wrapper);

/**
 * @brief Frees the memory used by a cache wrapper.
 *
 * @param Pointer to a CacheWrapper
 */
void destroy_cache(void *cache_wrapper);

#ifdef __cplusplus
}
#endif
