/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */

#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <stdbool.h>

#include "../../logdef.h"
#include "passthrough_helpers.h"

static int fill_dir_plus = 0;
LayerContext lroot;

static void *xmp_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  cfg->use_ino = 1;

  /* Pick up changes from lower filesystem right away. This is
     also necessary for better hardlink support. When the kernel
     calls the unlink() handler, it does not know the inode of
     the to-be-removed entry and can therefore not invalidate
     the cache of the associated inode - resulting in an
     incorrect st_nlink value being reported for any remaining
     hardlinks to this inode. */
  cfg->entry_timeout = 0;
  cfg->attr_timeout = 0;
  cfg->negative_timeout = 0;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("FUSE init called, userid %d, pid %d", f_ctx->uid, f_ctx->pid);
  }

  return NULL;
}

static void xmp_destroy(void *private_data) {
  (void)private_data;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("FUSE destroy called, userid %d, pid %d", f_ctx->uid, f_ctx->pid);
  }

  libdestroy(lroot);
}

static int xmp_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi) {
  (void)fi;
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("getattr called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = liblstat(path, stbuf, lroot);

  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_access(const char *path, int mask) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("access called for %s, mask %d, userid %d, pid %d", path, mask,
              f_ctx->uid, f_ctx->pid);
  }

  res = access(path, mask);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("readlink called for %s, size %zu, userid %d, pid %d", path, size,
              f_ctx->uid, f_ctx->pid);
  }

  res = (int)readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;

  buf[res] = '\0';
  return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
  DIR *dp;
  struct dirent *de;

  (void)offset;
  (void)fi;
  (void)flags;
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("readdir called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  dp = opendir(path);
  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0, fill_dir_plus))
      break;
  }

  closedir(dp);
  return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("mknod called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = mknod_wrapper(AT_FDCWD, path, NULL, (int)mode, rdev);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_unlink(const char *path) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("unlink called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = libunlink(path, lroot);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("mkdir called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = mkdir(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_rmdir(const char *path) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("rmdir called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = rmdir(path);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_symlink(const char *from, const char *to) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("symlink called from %s to %s, userid %d, pid %d", from, to,
              f_ctx->uid, f_ctx->pid);
  }

  res = symlink(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("rename called from %s to %s, flags %u, userid %d, pid %d", from,
              to, flags, f_ctx->uid, f_ctx->pid);
  }

  if (flags)
    return -EINVAL;

  res = rename(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_link(const char *from, const char *to) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("link called from %s to %s, userid %d, pid %d", from, to,
              f_ctx->uid, f_ctx->pid);
  }

  res = link(from, to);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
  (void)fi;
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("chmod called for %s, mode %o, userid %d, pid %d", path, mode,
              f_ctx->uid, f_ctx->pid);
  }

  res = chmod(path, mode);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
                     struct fuse_file_info *fi) {
  (void)fi;
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("chown called for %s, uid %u, gid %u, userid %d, pid %d", path,
              uid, gid, f_ctx->uid, f_ctx->pid);
  }

  res = lchown(path, uid, gid);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_truncate(const char *path, off_t size,
                        struct fuse_file_info *fi) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("truncate called for %s, size %ld, userid %d, pid %d", path, size,
              f_ctx->uid, f_ctx->pid);
  }

  if (fi != NULL) {
    res = libftruncate((int)fi->fh, size, lroot);
  } else {
    int fd = libopen(path, O_WRONLY, 0, lroot);
    if (fd < 0)
      return -errno;
    res = libftruncate(fd, size, lroot);
    libclose(fd, lroot);
  }
  if (res == -1)
    return -errno;

  return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
                       struct fuse_file_info *fi) {
  (void)fi;
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("utimens called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  /* don't use utime/utimes since they follow symlinks */
  res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
  if (res == -1)
    return -errno;

  return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi) {
  int res;
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("create called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = libopen(path, fi->flags, mode, lroot);
  if (res == -1)
    return -errno;

  fi->fh = res;
  return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("open called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = libopen(path, fi->flags, 0, lroot);
  if (res == -1)
    return -errno;

  fi->fh = res;
  return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
  int res, fd;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("read called for %s, size %zu, offset %ld, userid %d, pid %d",
              path, size, offset, f_ctx->uid, f_ctx->pid);
  }

  if (fi == NULL)
    fd = libopen(path, O_RDONLY, 0, lroot);
  else
    fd = (int)fi->fh;

  if (fd == -1)
    return -errno;

  lroot.app_context = strdup(path);
  res = (int)libpread(fd, buf, size, offset, lroot);

  if (res == -1)
    res = -errno;

  if (fi == NULL)
    libclose(fd, lroot);

  return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
  int res, fd;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("write called for %s, size %zu, offset %ld, userid %d, pid %d",
              path, size, offset, f_ctx->uid, f_ctx->pid);
  }

  (void)fi;
  if (fi == NULL)
    fd = libopen(path, O_WRONLY, 0, lroot);
  else
    fd = (int)fi->fh;

  if (fd == -1)
    return -errno;

  lroot.app_context = strdup(path);
  res = (int)libpwrite(fd, buf, size, offset, lroot);
  if (res == -1)
    res = -errno;

  if (fi == NULL)
    libclose(fd, lroot);

  return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
  int res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("statfs called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  res = statvfs(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi) {
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("release called for %s, userid %d, pid %d", path, f_ctx->uid,
              f_ctx->pid);
  }

  return libclose((int)fi->fh, lroot);
}

static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi) {

  (void)path;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("fsync called for %s, isdatasync %d, userid %d, pid %d", path,
              isdatasync, f_ctx->uid, f_ctx->pid);
  }

  // If the datasync parameter is non-zero, then only the user data should be
  // flushed, not the meta data.
  if (isdatasync == 0) {
    return fsync((int)fi->fh);
  }

  return fdatasync((int)fi->fh);
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags) {
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("setxattr called for %s, name %s, size %zu, flags %d, userid %d, "
              "pid %d",
              path, name, size, flags, f_ctx->uid, f_ctx->pid);
  }

  int res = lsetxattr(path, name, value, size, flags);
  if (res == -1)
    return -errno;
  return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
                        size_t size) {
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("getxattr called for %s, name %s, size %zu, userid %d, pid %d",
              path, name, size, f_ctx->uid, f_ctx->pid);
  }

  int res = lgetxattr(path, name, value, size);
  if (res == -1)
    return -errno;
  return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size) {
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("listxattr called for %s, size %zu, userid %d, pid %d", path,
              size, f_ctx->uid, f_ctx->pid);
  }

  int res = llistxattr(path, list, size);
  if (res == -1)
    return -errno;
  return res;
}

static int xmp_removexattr(const char *path, const char *name) {
  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("removexattr called for %s, name %s, userid %d, pid %d", path,
              name, f_ctx->uid, f_ctx->pid);
  }

  int res = lremovexattr(path, name);
  if (res == -1)
    return -errno;
  return 0;
}
#endif /* HAVE_SETXATTR */

static off_t xmp_lseek(const char *path, off_t off, int whence,
                       struct fuse_file_info *fi) {
  int fd;
  off_t res;

  if (DEBUG_ENABLED()) {
    struct fuse_context *f_ctx = fuse_get_context();
    DEBUG_MSG("lseek called for %s, offset %ld, whence %d, userid %d, pid %d",
              path, off, whence, f_ctx->uid, f_ctx->pid);
  }

  if (fi == NULL)
    fd = libopen(path, O_RDONLY, 0, lroot);
  else
    fd = (int)fi->fh;

  if (fd == -1)
    return -errno;

  res = lseek(fd, off, whence);
  if (res == -1)
    res = -errno;

  if (fi == NULL)
    libclose(fd, lroot);
  return res;
}

static const struct fuse_operations xmp_oper = {
    .init = xmp_init,
    .destroy = xmp_destroy,
    .getattr = xmp_getattr,
    .access = xmp_access,
    .readlink = xmp_readlink,
    .readdir = xmp_readdir,
    .mknod = xmp_mknod,
    .mkdir = xmp_mkdir,
    .symlink = xmp_symlink,
    .unlink = xmp_unlink,
    .rmdir = xmp_rmdir,
    .rename = xmp_rename,
    .link = xmp_link,
    .chmod = xmp_chmod,
    .chown = xmp_chown,
    .truncate = xmp_truncate,
#ifdef HAVE_UTIMENSAT
    .utimens = xmp_utimens,
#endif
    .open = xmp_open,
    .create = xmp_create,
    .read = xmp_read,
    .write = xmp_write,
    .statfs = xmp_statfs,
    .release = xmp_release,
    .fsync = xmp_fsync,
#ifdef HAVE_SETXATTR
    .setxattr = xmp_setxattr,
    .getxattr = xmp_getxattr,
    .listxattr = xmp_listxattr,
    .removexattr = xmp_removexattr,
#endif
    .lseek = xmp_lseek,
};

int main(int argc, char *argv[]) {
  enum { MAX_ARGS = 10 };
  int i, new_argc;
  char *new_argv[MAX_ARGS];
  const char *config_path = NULL;

  umask(0);

  /* Process the "--plus" and "--config" options */
  for (i = 0, new_argc = 0; (i < argc) && (new_argc < MAX_ARGS); i++) {
    if (!strcmp(argv[i], "--plus")) {
      fill_dir_plus = FUSE_FILL_DIR_PLUS;
    } else if (!strcmp(argv[i], "--config") && i + 1 < argc) {
      config_path = argv[++i]; // Skip the next argument (config path)
    } else {
      new_argv[new_argc++] = argv[i];
    }
  }

  lroot = libinit(config_path); // Use provided config path or default to NULL
  if (!lroot.ops) {
    (void)fprintf(stderr, "Failed to initialize library with config: %s\n",
                  config_path ? config_path : "./config.toml");
    return 1;
  }

  return fuse_main(new_argc, new_argv, &xmp_oper, NULL);
}
