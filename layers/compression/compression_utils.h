#ifndef __COMPRESSION_UTILS_H__
#define __COMPRESSION_UTILS_H__

#include "../../shared/types/layer_context.h"
#include "../../shared/utils/locking.h"
#include "compression.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

int is_valid_compression_fd(int fd);
bool validate_compression_fd_offset_and_nbyte(int fd, off_t offset,
                                              size_t nbyte,
                                              const char *op_name);
void error_msg_and_release_lock(const char *msg, LockTable *lock_table,
                                const char *path);
int create_compressed_file_mapping(dev_t device, ino_t inode, off_t logical_eof,
                                   LayerContext l);
int set_logical_eof_in_mapping(dev_t device, ino_t inode, off_t logical_eof,
                               LayerContext l);
int get_logical_eof_from_mapping(dev_t device, ino_t inode, LayerContext l,
                                 off_t *logical_eof);
int increment_open_counter(dev_t device, ino_t inode, LayerContext l);
int decrement_open_counter(dev_t device, ino_t inode, LayerContext l);
int mark_as_unlinked(dev_t device, ino_t inode, int *open_counter,
                     LayerContext l);
int should_cleanup_mapping(dev_t device, ino_t inode, LayerContext l);
size_t get_total_compressed_size(off_t initial_block_index,
                                 off_t last_block_index,
                                 CompressedFileMapping *file_mapping);
CompressedFileMapping *get_compressed_file_mapping(dev_t device, ino_t inode,
                                                   CompressionState *state);
int ensure_block_index_capacity(CompressedFileMapping *block_index,
                                size_t required_blocks);
int shrink_block_index(CompressedFileMapping *block_index,
                       size_t required_blocks);
int remove_compressed_file_mapping(dev_t device, ino_t inode, LayerContext l);

// Helper to extract (device,inode) from fd via lower layer
int get_file_key_from_fd(int fd, LayerContext l, dev_t *device, ino_t *inode);

// FdToInode hash table operations
FdToInode *fd_to_inode_lookup(CompressionState *state, int fd);
int fd_to_inode_insert(CompressionState *state, int fd, dev_t device,
                       ino_t inode, const char *path);
int fd_to_inode_remove(CompressionState *state, int fd);

// --- Backward-compat wrappers (to be removed after full migration) ---
int set_original_size_in_file_size_mapping(const char *path,
                                           off_t original_size, LayerContext l);
int get_original_size_from_mapping(const char *path, LayerContext l,
                                   off_t *original_size);

// Crash recovery: rebuild mappings from storage
int rebuild_block_mapping_from_storage(int fd, dev_t device, ino_t inode,
                                       LayerContext l);
#endif
