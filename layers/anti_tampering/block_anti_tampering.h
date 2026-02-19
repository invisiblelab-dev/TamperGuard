#ifndef __BLOCK_ANTI_TAMPERING_H__
#define __BLOCK_ANTI_TAMPERING_H__

#include "../../shared/types/layer_context.h"
#include <sys/stat.h>
#include <unistd.h>

ssize_t block_anti_tampering_write(int fd, const void *buffer, size_t nbyte,
                                   off_t offset, LayerContext l);
ssize_t block_anti_tampering_read(int fd, void *buffer, size_t nbyte,
                                  off_t offset, LayerContext l);
int block_anti_tampering_open(const char *pathname, int flags, __mode_t mode,
                              LayerContext l);
int block_anti_tampering_close(int fd, LayerContext l);
int block_anti_tampering_ftruncate(int fd, off_t length, LayerContext l);
int block_anti_tampering_fstat(int fd, struct stat *stbuf, LayerContext l);
int block_anti_tampering_lstat(const char *pathname, struct stat *stbuf,
                               LayerContext l);
int block_anti_tampering_unlink(const char *pathname, LayerContext l);

#endif // __BLOCK_ANTI_TAMPERING_H__

