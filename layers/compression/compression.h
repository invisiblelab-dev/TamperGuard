#ifndef __COMPRESSION_H__
#define __COMPRESSION_H__

#define INVALID_FD -1 // Value to indicate an invalid fd

#include "../../shared/types/layer_context.h"
// Include uthash which is not a library, it's just a header file
#include "../../lib/uthash/src/uthash.h"
#include "../../shared/utils/locking.h"
#include "../anti_tampering/anti_tampering.h"
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h> /* for dev_t, ino_t */
#include <unistd.h>

typedef struct {
  int fd; /* file descriptor */
  dev_t device;
  ino_t inode;
  char *path; /* optional: owned by layer if set */
} FdToInode;

/**
 * @brief Unified mapping for compressed file metadata
 *
 * This structure combines logical size tracking (used by all compression modes)
 * with block-level indexing (only used by COMPRESSION_MODE_SPARSE_BLOCK).
 *
 * Fields marked as "sparse_block only" are ignored in other compression modes.
 */
typedef struct {
  // Common fields (used by all compression modes)
  dev_t device;
  ino_t inode;
  unsigned char key[sizeof(dev_t) + sizeof(ino_t)];
  off_t logical_eof; /* Logical (uncompressed) end-of-file position */
  int open_counter;  /* Number of open file descriptors for this file */
  int unlink_called; /* 1 if unlink was called, 0 otherwise */

  // Sparse block mode fields (only used in COMPRESSION_MODE_SPARSE_BLOCK)
  // These fields are ignored/unused in other compression modes
  size_t num_blocks; /* Number of blocks allocated */
  size_t capacity; /* allocated capacity for sizes and is_uncompressed arrays */
  off_t *sizes;    /* Compressed physical size of each block */
  int *is_uncompressed; /* Per-block flag: 1 if stored uncompressed, 0 if
                           compressed */

  UT_hash_handle hh;
} CompressedFileMapping;

typedef struct {
  FdToInode fd_to_inode[MAX_FDS]; /* Array of fd mappings */
  CompressedFileMapping *file_mapping;
  Compressor compressor;
  LockTable *lock_table; // path-based reader-writer lock table
  compression_mode_t mode;
  size_t block_size; /* Global block size (same for all files) */
  int free_space;    // enable fallocate punch behavior in sparse_block mode
} CompressionState;

LayerContext compression_init(LayerContext *next_layer,
                              const CompressionConfig *config);
ssize_t compression_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                          LayerContext l);
ssize_t compression_pwrite(int fd, const void *buffer, size_t nbyte,
                           off_t offset, LayerContext l);
int compression_open(const char *pathname, int flags, mode_t mode,
                     LayerContext l);
int compression_close(int fd, LayerContext l);
void compression_destroy(LayerContext l);
int compression_ftruncate(int fd, off_t length, LayerContext l);
int compression_truncate(const char *path, off_t length, LayerContext l);
int compression_fstat(int fd, struct stat *stbuf, LayerContext l);
int compression_lstat(const char *pathname, struct stat *stbuf, LayerContext l);
int compression_unlink(const char *pathname, LayerContext l);
int compression_readdir(const char *path, void *buf,
                        int (*filler)(void *buf, const char *name,
                                      const struct stat *stbuf, off_t off,
                                      unsigned int flags),
                        off_t offset, struct fuse_file_info *fi,
                        unsigned int flags, LayerContext l);
int compression_rename(const char *from, const char *to, unsigned int flags,
                       LayerContext l);

int compression_chmod(const char *path, mode_t mode, LayerContext l);
int compression_fsync(int fd, int isdatasync, LayerContext l);

#endif
