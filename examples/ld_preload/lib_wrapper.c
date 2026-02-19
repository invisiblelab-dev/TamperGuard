/* Define function wrappers for our library with proper name and setup, suitable
 * to use LD_PRELOAD */

#include "glib.h"
#include "glibconfig.h"
#include "lib.h"
#include "shared/types/layer_context.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

LayerContext lroot = {0};

// these booleans are used to detect if the lib needs to be initialized and if
// it is currently being initialized (avoid infinite cycles)
static int lib_is_initializing = 0;
static int lib_is_init = 0;

// this hash table (working as a set) contains file descriptors that weren't
// opened with our library and therefore must be closed with libc close
GHashTable *fds_set = NULL;

// original libc functions that will be dinamically loaded
static void *libc_handle = NULL;
static int (*libc_close)(int fd) = NULL;
static int (*libc_open)(const char *pathname, int flags, ...) = NULL;
static int (*libc_openat)(int dirfd, const char *pathname, int flags,
                          ...) = NULL;
static ssize_t (*libc_pread)(int fd, void *buf, size_t count,
                             off_t offset) = NULL;
static ssize_t (*libc_pwrite)(int fd, const void *buf, size_t count,
                              off_t offset) = NULL;
static int (*libc_timerfd_create)(int clockid, int flags) = NULL;
static int (*libc_pipe2)(int pipefd[2], int flags) = NULL;

__attribute__((destructor)) static void preload_destructor() {
  g_hash_table_destroy(fds_set);
  dlclose(libc_handle);
}

void check_lib_init() {

  if (fds_set == NULL)
    fds_set = g_hash_table_new(g_direct_hash, g_direct_equal);
  if (libc_handle == NULL)
    libc_handle = dlopen("libc.so.6", RTLD_LAZY);
  if (libc_pipe2 == NULL)
    libc_pipe2 = dlsym(libc_handle, "pipe2");
  if (libc_close == NULL)
    libc_close = dlsym(libc_handle, "close");
  if (libc_timerfd_create == NULL)
    libc_timerfd_create = dlsym(libc_handle, "timerfd_create");
  if (libc_open == NULL)
    libc_open = dlsym(libc_handle, "open");
  if (libc_openat == NULL)
    libc_openat = dlsym(libc_handle, "openat");
  if (libc_pread == NULL)
    libc_pread = dlsym(libc_handle, "pread");
  if (libc_pwrite == NULL)
    libc_pwrite = dlsym(libc_handle, "pwrite");

  // if we haven't started libinit yet, call it and accordingly update
  // lib_is_initializing to avoid infinite cycles
  if (!lib_is_init) {
    lib_is_initializing = 1;
    lroot = libinit(NULL);
    lib_is_initializing = 0;
    lib_is_init = 1;
  }
}

int timerfd_create(int clockid, int flags) {
  if (lib_is_initializing)
    return libc_timerfd_create(clockid, flags);
  check_lib_init();

  int fd = libc_timerfd_create(clockid, flags);

  // mark this fd to make a distinction in the close function, avoiding
  // entering our library with an fd that wasn't opened there
  if (fd >= 0)
    g_hash_table_add(fds_set, GINT_TO_POINTER(fd));

  return fd;
}

int pipe2(int pipefd[2], int flags) {
  if (lib_is_initializing)
    return libc_pipe2(pipefd, flags);
  check_lib_init();

  int ret = libc_pipe2(pipefd, flags);

  // mark this fd to make a distinction in the close function, avoiding
  // entering our library with an fd that wasn't opened there
  if (ret == 0) {
    g_hash_table_add(fds_set, GINT_TO_POINTER(pipefd[0]));
    g_hash_table_add(fds_set, GINT_TO_POINTER(pipefd[1]));
  }

  return ret;
}

int open64(const char *pathname, int flags, ...) {

  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  if (lib_is_initializing)
    return libc_open(pathname, flags, mode);
  check_lib_init();

  int fd = libopen(pathname, flags, mode, lroot);

  return fd;
}

// Fow now, this function isn't thread safe
int openat(int dirfd, const char *pathname, int flags, ...) {

  mode_t mode = 0;

  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  if (lib_is_initializing)
    return libc_openat(dirfd, pathname, flags, mode);
  check_lib_init();

  int fd;

  if (pathname[0] == '/' || dirfd == AT_FDCWD) {
    // the behavior of openat in these conditions is the same as open
    fd = libopen(pathname, flags, mode, lroot);
  } else {

    // save current working directory
    int prev_dir = libopen(".", O_RDONLY | O_DIRECTORY, mode, lroot);

    if (prev_dir == -1) {
      perror("Failed save working directory: ");
      return -1;
    }

    // change to the target directory
    int r = fchdir(dirfd);

    if (r == -1) {
      perror("Failed to change directory: ");
      close(prev_dir);
      return -1;
    }

    fd = libopen(pathname, flags, mode, lroot);

    if (fd == -1) {
      perror("Failed to open file: ");
      close(prev_dir);
      return -1;
    }

    // return to the original working directory
    r = fchdir(prev_dir);

    close(prev_dir);

    if (r == -1) {
      perror("Failed to return to the working directory: ");
      close(fd);
      return -1;
    }
  }

  return fd;
}

int close(int fd) {
  gpointer key = GINT_TO_POINTER(fd);
  gboolean contains = g_hash_table_contains(fds_set, key);

  if (lib_is_initializing) {
    if (contains)
      g_hash_table_remove(fds_set, key);
    return libc_close(fd);
  }

  check_lib_init();

  if (contains) {
    g_hash_table_remove(fds_set, key);
    return libc_close(fd);
  }
  return libclose(fd, lroot);
}

ssize_t pread64(int fd, void *buf, size_t count, off_t offset) {

  if (lib_is_initializing)
    return libc_pread(fd, buf, count, offset);
  check_lib_init();

  return libpread(fd, buf, count, offset, lroot);
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off_t offset) {

  if (lib_is_initializing)
    return libc_pwrite(fd, buf, count, offset);
  check_lib_init();
  return libpwrite(fd, buf, count, offset, lroot);
}
