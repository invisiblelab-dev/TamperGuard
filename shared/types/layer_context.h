#ifndef LAYER_CONTEXT_H
#define LAYER_CONTEXT_H

#include <sys/stat.h>  /* for struct stat */
#include <sys/types.h> /* for ssize_t, size_t, off_t, mode_t */

struct fuse_file_info;

/**
 * @file layer_context.h
 * @brief Layer context and operations definitions
 *
 * This header defines the structures and function pointers for managing
 * layered I/O operations in a modular system.
 */

/* Forward declaration */
struct layer_ops;

/**
 * @struct layer_context
 * @brief Structure to manage Layer context and state
 *
 * @param ops            Operations exported by this layer
 * @param internal_state Layer's internal state (e.g., file paths, cipher
 * schemes, etc.)
 * @param app_context    Context from the application layer (e.g., file paths)
 * @param nlayers        Number of next layers
 * @param next_layers    Pointer to the next layer(s) context
 */
typedef struct layer_context {
  struct layer_ops *ops;
  void *internal_state;
  void *app_context;
  int nlayers;
  struct layer_context *next_layers;
} LayerContext;

/**
 * @struct layer_ops
 * @brief Operations exposed by a layer
 *
 * Function pointers for various I/O operations that can be performed
 * by a layer in the modular I/O system.
 */
typedef struct layer_ops {
  void (*ldestroy)(LayerContext l);
  ssize_t (*lpread)(int fd, void *buffer, size_t nbyte, off_t offset,
                    LayerContext l);
  ssize_t (*lpwrite)(int fd, const void *buffer, size_t nbytes, off_t offset,
                     LayerContext l);
  int (*lopen)(const char *pathname, int flags, mode_t mode, LayerContext l);
  int (*lclose)(int fd, LayerContext l);
  int (*lftruncate)(int fd, off_t length, LayerContext l);
  int (*ltruncate)(const char *path, off_t length, LayerContext l);
  int (*lfstat)(int fd, struct stat *stbuf, LayerContext l);
  int (*llstat)(const char *path, struct stat *stbuf, LayerContext l);
  int (*lunlink)(const char *path, LayerContext l);

  // Non supported by every layer
  int (*lreaddir)(const char *path, void *buf,
                  int (*filler)(void *buf, const char *name,
                                const struct stat *stbuf, off_t off,
                                unsigned int flags),
                  off_t offset, struct fuse_file_info *fi, unsigned int flags,
                  LayerContext l);
  int (*lrename)(const char *from, const char *to, unsigned int flags,
                 LayerContext l);
  int (*lchmod)(const char *path, mode_t mode, LayerContext l);
  int (*lfsync)(int fd, int isdatasync, LayerContext l);
  int (*lfallocate)(int fd, off_t offset, int mode, off_t length,
                    LayerContext l);
} LayerOps;

#endif /* LAYER_CONTEXT_H */
