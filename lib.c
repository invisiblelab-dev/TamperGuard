#include "lib.h"
#include "shared/types/layer_context.h"
#include <dlfcn.h>
#include <services/metadata.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LayerContext libinit(const char *config_path) {
  // Use default path if NULL is provided
  const char *path = config_path ? config_path : "./config.toml";
  char *filepath = strdup(path);
  if (!filepath) {
    // Handle allocation failure
    return (LayerContext){0};
  }
  LayerContext root = load_config_toml(filepath);
  free(filepath);
  return root;
}

void libdestroy(LayerContext lroot) { lroot.ops->ldestroy(lroot); }

ssize_t libpread(int fd, void *buffer, size_t nbyte, off_t offset,
                 LayerContext lroot) {
  ssize_t res;

  // call operations of the root layer
  res = lroot.ops->lpread(fd, buffer, nbyte, offset, lroot);
  return res;
}

ssize_t libpwrite(int fd, const void *buffer, size_t nbyte, off_t offset,
                  LayerContext lroot) {
  ssize_t res;
  // call operations of the root layer
  res = lroot.ops->lpwrite(fd, buffer, nbyte, offset, lroot);
  return res;
}

int libopen(const char *pathname, int flags, mode_t mode, LayerContext lroot) {
  int fd;
  fd = lroot.ops->lopen(pathname, flags, mode, lroot);
  return fd;
}

int libclose(int fd, LayerContext lroot) {
  int res;
  res = lroot.ops->lclose(fd, lroot);
  return res;
}

int libftruncate(int fd, off_t length, LayerContext lroot) {
  return lroot.ops->lftruncate(fd, length, lroot);
}

int libfsync(int fd, int isdatasync, LayerContext lroot) {
  int res;
  res = lroot.ops->lfsync(fd, isdatasync, lroot);
  return res;
}

int liblstat(const char *path, struct stat *stbuf, LayerContext lroot) {
  int res;
  res = lroot.ops->llstat(path, stbuf, lroot);
  return res;
}

int libreaddir(const char *path, void *buf,
               int (*filler)(void *buf, const char *name,
                             const struct stat *stbuf, off_t off,
                             unsigned int flags),
               off_t offset, struct fuse_file_info *fi, unsigned int flags,
               LayerContext lroot) {
  int res;
  res = lroot.ops->lreaddir(path, buf, filler, offset, fi, flags, lroot);
  return res;
}

int librename(const char *from, const char *to, unsigned int flags,
              LayerContext lroot) {
  int res;
  res = lroot.ops->lrename(from, to, flags, lroot);
  return res;
}

int libchmod(const char *path, mode_t mode, LayerContext lroot) {
  int res;
  res = lroot.ops->lchmod(path, mode, lroot);
  return res;
}

int libunlink(const char *path, LayerContext lroot) {
  int res;
  res = lroot.ops->lunlink(path, lroot);
  return res;
}
