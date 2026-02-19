#ifndef __ENCRYPTION_H__
#define __ENCRYPTION_H__

#include "../../shared/types/layer_context.h"
#include "config.h"

// state struct --- TODO :: check if more state is necessary
typedef struct {
  int block_size;
  const unsigned char *key;
} EncryptionState;

LayerContext encryption_init(LayerContext *next_layer,
                             const EncryptionConfig *config);
ssize_t encryption_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                         LayerContext l);
ssize_t encryption_pwrite(int fd, const void *buffer, size_t nbyte,
                          off_t offset, LayerContext l);
int encryption_open(const char *pathname, int flags, mode_t mode,
                    LayerContext l);
int encryption_close(int fd, LayerContext l);
void encryption_destroy(LayerContext l);
int encryption_ftruncate(int fd, off_t length, LayerContext l);
int encryption_truncate(const char *path, off_t length, LayerContext l);
int encryption_fstat(int fd, struct stat *stbuf, LayerContext l);
int encryption_lstat(const char *pathname, struct stat *stbuf, LayerContext l);
int encryption_unlink(const char *pathname, LayerContext l);

int encryption_readdir(const char *path, void *buf,
                       int (*filler)(void *buf, const char *name,
                                     const struct stat *stbuf, off_t off,
                                     unsigned int flags),
                       off_t offset, struct fuse_file_info *fi,
                       unsigned int flags, LayerContext l);

int encryption_rename(const char *from, const char *to, unsigned int flags,
                      LayerContext l);

int encryption_chmod(const char *path, mode_t mode, LayerContext l);
int encryption_fsync(int fd, int isdatasync, LayerContext l);

#endif
