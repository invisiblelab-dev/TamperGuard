#ifndef __LOCAL_H__
#define __LOCAL_H__

#include "../../shared/types/layer_context.h"
#include "config.h"
#include <sys/stat.h>

typedef struct {
  int (*open)(const char *pathname, int flags, ...);
  int (*close)(int fd);
  ssize_t (*pread)(int fd, void *buf, size_t count, off_t offset);
  ssize_t (*pwrite)(int fd, const void *buf, size_t count, off_t offset);
} LibCOps;

LayerContext local_init();
void local_destroy(LayerContext l);
ssize_t local_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                    LayerContext l);
ssize_t local_pwrite(int fd, const void *buffer, size_t nbyte, off_t offset,
                     LayerContext l);
int local_open(const char *pathname, int flags, mode_t mode, LayerContext l);
int local_close(int fd, LayerContext l);
int local_ftruncate(int fd, off_t length, LayerContext l);
int local_truncate(const char *path, off_t length, LayerContext l);
int local_fstat(int fd, struct stat *stbuf, LayerContext l);
int local_lstat(const char *path, struct stat *stbuf, LayerContext l);
int local_unlink(const char *path, LayerContext l);
int local_readdir(const char *path, void *buf,
                  int (*filler)(void *buf, const char *name,
                                const struct stat *stbuf, off_t off,
                                unsigned int flags),
                  off_t offset, struct fuse_file_info *fi, unsigned int flags,
                  LayerContext l);
int local_rename(const char *from, const char *to, unsigned int flags,
                 LayerContext l);
int local_fallocate(int fd, off_t offset, int mode, off_t length,
                    LayerContext l);
int local_chmod(const char *path, mode_t mode, LayerContext l);
int local_fsync(int fd, int isdatasync, LayerContext l);

#endif
