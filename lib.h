#ifndef __LIB_H__
#define __LIB_H__

#include "config/loader.h"
#include "logdef.h"
#include "shared/types/layer_context.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// lib functions to be used by the application
LayerContext libinit(const char *config_path);
void libdestroy(LayerContext lroot);
ssize_t libpread(int fd, void *buffer, size_t nbyte, off_t offset,
                 LayerContext lroot);
ssize_t libpwrite(int fd, const void *buffer, size_t nbyte, off_t offset,
                  LayerContext lroot);
int libopen(const char *pathname, int flags, mode_t mode, LayerContext lroot);
int libclose(int fd, LayerContext lroot);
int libfsync(int fd, int isdatasync, LayerContext lroot);
int libftruncate(int fd, off_t length, LayerContext lroot);
int liblstat(const char *path, struct stat *stbuf, LayerContext lroot);
int libreaddir(const char *path, void *buf,
               int (*filler)(void *buf, const char *name,
                             const struct stat *stbuf, off_t off,
                             unsigned int flags),
               off_t offset, struct fuse_file_info *fi, unsigned int flags,
               LayerContext lroot);
int librename(const char *from, const char *to, unsigned int flags,
              LayerContext lroot);
int libchmod(const char *path, mode_t mode, LayerContext lroot);
int libunlink(const char *path, LayerContext lroot);

#endif
