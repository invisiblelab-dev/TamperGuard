#define _GNU_SOURCE
#include "local.h"
#include "logdef.h"
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/falloc.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Local layer
 * - writes and reads locally
 */

// terminal layer does not need arguments, there is no next layer
LayerContext local_init() {
  // Create local layer state
  LayerContext layer_state;

  // At the moment this layer does not have neither internal state or app
  // context
  LibCOps *lib_c_ops = malloc(sizeof(LibCOps));
  void *handle = dlopen("libc.so.6", RTLD_LAZY);
  lib_c_ops->open = dlsym(handle, "open");
  lib_c_ops->close = dlsym(handle, "close");
  lib_c_ops->pread = dlsym(handle, "pread");
  lib_c_ops->pwrite = dlsym(handle, "pwrite");
  layer_state.internal_state = (void *)lib_c_ops;
  layer_state.app_context = NULL;

  // Create LayerOps structure
  LayerOps *local_ops = malloc(sizeof(LayerOps));
  local_ops->ldestroy = local_destroy;
  local_ops->lpread = local_pread;
  local_ops->lpwrite = local_pwrite;
  local_ops->lopen = local_open;
  local_ops->lclose = local_close;
  local_ops->lfsync = local_fsync;
  local_ops->lftruncate = local_ftruncate;
  local_ops->ltruncate = local_truncate;
  local_ops->lfstat = local_fstat;
  local_ops->llstat = local_lstat;
  local_ops->lunlink = local_unlink;
  local_ops->lreaddir = local_readdir;
  local_ops->lrename = local_rename;
  local_ops->lchmod = local_chmod;
  local_ops->lfallocate = local_fallocate;

  layer_state.ops = local_ops;

  // Terminal layer, there are no next layers
  layer_state.next_layers = NULL;

  return layer_state;
}

void local_destroy(LayerContext l) {

  if (DEBUG_ENABLED()) {
    DEBUG_MSG("[LOCAL_LAYER] Destroy called");
  }

  free(l.ops);
}

ssize_t local_pwrite(int fd, const void *buffer, size_t nbyte, off_t offset,
                     LayerContext l) {

  LibCOps *lib_ops = (LibCOps *)l.internal_state;
  ssize_t res;
  // calls local pwrite
  res = lib_ops->pwrite(fd, buffer, nbyte, offset);

  return res;
}

ssize_t local_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                    LayerContext l) {

  LibCOps *lib_ops = (LibCOps *)l.internal_state;
  ssize_t res;
  // calls local pread
  res = lib_ops->pread(fd, buffer, nbyte, offset);
  return res;
}

int local_open(const char *pathname, int flags, mode_t mode, LayerContext l) {
  LibCOps *lib_ops = (LibCOps *)l.internal_state;
  int fd;

  if (mode != 0) {
    fd = lib_ops->open(pathname, flags, mode);
  } else
    fd = lib_ops->open(pathname, flags);
  return fd;
}

int local_close(int fd, LayerContext l) {
  LibCOps *lib_ops = (LibCOps *)l.internal_state;
  int res;
  res = lib_ops->close(fd);
  return res;
}

int local_ftruncate(int fd, off_t length, LayerContext l) {
  int res;
  res = ftruncate(fd, length);
  return res;
}

int local_truncate(const char *path, off_t length, LayerContext l) {
  int res;
  res = truncate(path, length);
  return res;
}

int local_fstat(int fd, struct stat *stbuf, LayerContext l) {
  int res;
  res = fstat(fd, stbuf);
  return res;
}

int local_lstat(const char *path, struct stat *stbuf, LayerContext l) {
  int res;
  res = lstat(path, stbuf);
  return res;
}

int local_unlink(const char *path, LayerContext l) {
  int res;
  res = unlink(path);
  return res;
}

int local_readdir(const char *path, void *buf,
                  int (*filler)(void *buf, const char *name,
                                const struct stat *stbuf, off_t off,
                                unsigned int flags),
                  off_t offset, struct fuse_file_info *fi, unsigned int flags,
                  LayerContext l) {
  DIR *dp;
  struct dirent *de;
  int res = 0;

  (void)offset;
  (void)fi;
  (void)flags;
  (void)l;

  dp = opendir(path);
  if (dp == NULL) {
    return -errno;
  }

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;

    if (filler(buf, de->d_name, &st, 0, 0)) {
      break;
    }
  }

  closedir(dp);
  return res;
}
int local_rename(const char *from, const char *to, unsigned int flags,
                 LayerContext l) {
  int res;
  res = rename(from, to);
  return res;
}

int local_chmod(const char *path, mode_t mode, LayerContext l) {
  int res;
  res = chmod(path, mode);
  return res;
}

int local_fsync(int fd, int isdatasync, LayerContext l) {
  if (isdatasync) {
    return fdatasync(fd);
  }
  return fsync(fd);
}

int local_fallocate(int fd, off_t offset, int mode, off_t length,
                    LayerContext l) {
  int res = fallocate(fd, mode, offset, length);
  return res;
}
