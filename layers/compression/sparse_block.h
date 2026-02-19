#ifndef __SPARSE_BLOCK_H__
#define __SPARSE_BLOCK_H__

#include "compression.h"

ssize_t compression_sparse_block_pwrite(int fd, const void *buffer,
                                        size_t nbyte, off_t offset,
                                        LayerContext l);
ssize_t compression_sparse_block_pread(int fd, void *buffer, size_t nbyte,
                                       off_t offset, LayerContext l);
int compression_sparse_block_ftruncate(int fd, off_t length, LayerContext l);
int compression_sparse_block_truncate(const char *path, off_t length,
                                      LayerContext l);
int compression_sparse_block_fstat(int fd, struct stat *stbuf, LayerContext l);
int compression_sparse_block_lstat(const char *pathname, struct stat *stbuf,
                                   LayerContext l);

#endif
