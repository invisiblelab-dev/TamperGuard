#ifndef __READCACHE_H__
#define __READCACHE_H__

#include "../../../shared/types/layer_context.h"
#include "config.h"
#include "config/declarations.h"
#include <glib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct cacheentry {
  const void *block;
  size_t size;
} CacheEntry;

typedef struct {
  int counter;  // number of fds opened for a certain inode
  int unlinked; // true if unlinked was called to a certain inode
} InodeInfo;

typedef struct {
  int (*insert_item)(void *cache_wrapper, const char *key, const void *block,
                     size_t block_length);
  void (*get_item)(void *cache_wrapper, const char *key, CacheEntry *item);
  int (*remove_item)(void *cache_wrapper, const char *key);
  int (*contain_item)(void *cache_wrapper, const char *key);
  unsigned long (*get_item_count)(void *cache_wrapper);
  void (*destroy_cache)(void *cache_wrapper);

} CacheLibOps;

typedef struct {
  size_t block_size;
  size_t num_blocks;
  GHashTable *fd_to_inode;   // maps fds to inodes
  GHashTable *inode_to_info; // maps inodes to InodeInfo structs
  void *shared_lib_handle;
  void *cache_wrapper;
  CacheLibOps ops;
} ReadCacheState;

LayerContext read_cache_init(LayerContext *next_layer, int nlayers,
                             size_t block_size, size_t num_blocks);

void read_cache_destroy(LayerContext l);

/**
 * @brief Stores important data for the cache and forwards the request.
 *
 * After getting the fd from the next layer, it is used as a key to
 * store the inode number in this layer's internal state's mapping.
 * The fd counter is also incremented for this inode.
 * If O_TRUNC is used, every cache entry related to this file will be removed.
 *
 * @param pathname path to the file to open
 * @param flags open flags
 * @param mode creation permissions
 * @param l Layer context
 *
 * @return fd of the file, -1 on error
 */
int read_cache_open(const char *pathname, int flags, mode_t mode,
                    LayerContext l);

/**
 * @brief Frees important data for the cache and forwards the request.
 *
 * After obtaining the result of the close from the next layer,
 * the fd->inode hash table entry for this fd is removed.
 * Also, the open fd counter for the inode is decremented; if it reaches 0 and
 * the inode was previously unlinked, every cached entry related to the file is
 * removed.
 *
 * @param fd fd to close
 * @param l Layer context
 *
 * @return -1 on error
 */
int read_cache_close(int fd, LayerContext l);

/**
 * @brief Reads the requested data, consulting the cache.
 *
 * Reads, block by block, the requested bytes.
 * For each block, the cache is consulted. If the block is in cache,
 * it copies the content to the buffer and moves on the next block.
 * If the block is not in cache, it is requested to next layer and the cache is
 * updated.
 *
 * @param fd fd to read from
 * @param buffer buffer to store the read data
 * @param nbytes number of bytes to read
 * @param offset file offset to start reading
 * @param l Layer context
 *
 * @return number of bytes read or -1 on error
 */
ssize_t read_cache_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                         LayerContext l);

/**
 * @brief Writes the requested data, updating the cached blocks.
 *
 * Writes the requested bytes, block by block, checking the cache for each
 * block. If the block is there, updates its content according to the passed
 * buffer.
 *
 * @param fd fd to write to
 * @param buffer data to write
 * @param nbytes number of bytes to write
 * @param offset position to start writing
 * @param l Layer context
 *
 * @return number of bytes written or -1 on error.
 */
ssize_t read_cache_pwrite(int fd, const void *buffer, size_t nbytes,
                          off_t offset, LayerContext l);

/**
 * @brief ftruncate updating the cache
 *
 * Removes from the cache every block that is between
 * the requested length and the current size of the file.
 *
 * @param fd fd to truncate
 * @param length length to truncate to
 * @param l Layer context
 *
 * @return 0 on success or -1 on error
 */
int read_cache_ftruncate(int fd, off_t length, LayerContext l);

/**
 * @brief fstat for read_cache layer. Calls fstat on the underlying layer.
 *
 * @param fd File descriptor.
 * @param stbuf Stat buffer to fill.
 * @param l Layer context.
 *
 * @return 0 on success, or -1 on error.
 */
int read_cache_fstat(int fd, struct stat *stbuf, LayerContext l);

/**
 * @brief lstat for read_cache layer. Calls lstat on the underlying layer.
 *
 * @param pathname Path to the file.
 * @param stbuf Stat buffer to fill.
 * @param l Layer context.
 *
 * @return 0 on success, or -1 on error.
 */
int read_cache_lstat(const char *pathname, struct stat *stbuf, LayerContext l);

/**
 * @brief Unlink that updates the cache when all of the open fds for that path
 * are closed
 *
 * Removes from the cache all blocks related to this file only when there are no
 * more fds opened for the path. If there are still opened fds for that path we
 * mark the specific inode as unlinked, so the removal is handled in the close.
 *
 * @param pathname path to the file to be unlinked
 * @param l Layer context
 *
 * @return 0 on success or -1 on error
 */
int read_cache_unlink(const char *pathname, LayerContext l);

#endif
