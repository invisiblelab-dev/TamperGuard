#include "cache_lib_wrapper.h"
#include <cmath>
#include <cstddef>
#include <event.h>
#include <event2/event.h>
#include <iostream>
#include <memory>
#include <stddef.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

using Cache = facebook::cachelib::LruAllocator;
using facebook::cachelib::PoolId;
using RemoveRes = facebook::cachelib::LruAllocator::RemoveRes;

size_t BLOCK_SIZE = 0;

extern "C" void *initialize_cache(size_t num_blocks, size_t block_size,
                                  char *name) {

  BLOCK_SIZE = block_size;

  // 128 -> average path length for the key
  // block_size*1.5 -> item header overhead and padding precaution
  size_t item_size =
      sizeof(Cache::Item) + 128 + sizeof(size_t) + block_size + block_size / 2;
  // CacheLib needs to be able to allocate at least one slab
  // here we're multiplying by 8.1 because of integer division rounding when
  // dividing by item_size
  size_t min_num_items = (static_cast<size_t>(1024 * 1024 * 8.1)) / item_size;
  size_t final_num_items_allocation =
      (num_blocks < min_num_items) ? min_num_items : num_blocks;

  // for some reason, the automatic hash table configuration fails with a value
  // smaller 321...
  size_t final_num_items_access =
      (final_num_items_allocation < 321) ? 321 : final_num_items_allocation;
  try {
    Cache::Config config;
    size_t cache_size = item_size * final_num_items_allocation;
    config.setCacheSize(cache_size)
        .setCacheName(name)
        .setAccessConfig(final_num_items_access)
        .validate();

    CacheWrapper *wrapper = new CacheWrapper;
    wrapper->cache = std::make_unique<Cache>(config);

    wrapper->defaultPool = wrapper->cache->addPool(
        "default", wrapper->cache->getCacheMemoryStats().ramCacheSize);

    return static_cast<void *>(wrapper);
  } catch (const std::exception &e) {
    std::cout << "[CACHELIB_WRAPPER] Invalid config: " << e.what() << "\n";
    return nullptr;
  }
}

extern "C" int insert_item(void *cache_wrapper, const char *key,
                           const void *block, size_t block_length) {
  CacheWrapper *wrapper = static_cast<CacheWrapper *>(cache_wrapper);
  auto pool = wrapper->defaultPool;

  // size_t item_size = sizeof(size_t) + block_length;

  try {
    // first, check if there's an already allocated handle
    auto write_handle = wrapper->cache->findToWrite(key);
    bool new_item = false;

    // if it doesn't already exist, allocate it
    if (!write_handle) {
      write_handle = wrapper->cache->allocate(pool, key, BLOCK_SIZE);
      new_item = true;
    }

    if (!write_handle) {
      std::cout
          << "[CACHELIB_WRAPPER] Failed to allocate memory for a WriteHandle\n";
      return -1;
    }

    std::byte *memory_to_write =
        static_cast<std::byte *>(write_handle->getMemory());
    std::memcpy(memory_to_write, &block_length, sizeof(size_t));
    // even though we allocate BLOCK_SIZE, it's only necessary to copy
    // block_length bytes, since the rest is just trash
    std::memcpy(memory_to_write + sizeof(size_t), block, block_length);

    // only insert if we the item is completely new (i.e., allocate was called)
    // if we got the handle through findToWrite, there's no need to insert it
    // again
    if (new_item)
      wrapper->cache->insert(write_handle);

    return 0;

  } catch (const std::exception &e) {
    std::cout << "[CACHELIB_WRAPPER] insert exception: " << e.what() << "\n";
    return -1;
  }
}

extern "C" void get_item(void *cache_wrapper, const char *key,
                         CacheEntry *item) {
  auto *wrapper = static_cast<CacheWrapper *>(cache_wrapper);

  auto find_handle = wrapper->cache->find(key);
  if (find_handle) {
    const std::byte *memory_to_read =
        static_cast<const std::byte *>(find_handle->getMemory());
    std::memcpy(&item->size, memory_to_read, sizeof(size_t));
    const std::byte *temp = memory_to_read + sizeof(size_t);
    item->block = static_cast<const void *>(temp);
  } else
    item->block = nullptr;
}

extern "C" int contain_item(void *cache_wrapper, const char *key) {
  auto *wrapper = static_cast<CacheWrapper *>(cache_wrapper);

  auto find_handle = wrapper->cache->find(key);
  if (find_handle) {
    return 1;
  } else {
    return 0;
  }
}

extern "C" int remove_item(void *cache_wrapper, const char *key) {
  auto *wrapper = static_cast<CacheWrapper *>(cache_wrapper);

  auto removed = wrapper->cache->remove(key);
  if (removed != RemoveRes::kSuccess)
    return -1;
  else
    return 0;
}

extern "C" unsigned long get_item_count(void *cache_wrapper) {

  auto *wrapper = static_cast<CacheWrapper *>(cache_wrapper);

  return wrapper->cache->getPoolStats(wrapper->defaultPool).numItems();
}

extern "C" void destroy_cache(void *cache_wrapper) {

  auto *wrapper = static_cast<CacheWrapper *>(cache_wrapper);
  wrapper->cache.reset();
  delete wrapper;
}
