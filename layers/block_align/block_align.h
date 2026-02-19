#ifndef __BLOCKALIGN_H__
#define __BLOCKALIGN_H__

#include "../../shared/types/layer_context.h"
#include "config.h"
#include <glib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
  size_t block_size;
  GHashTable *fds_special_flags; // indicates fds that must have special
                                 // treatment (currently O_READ and O_APPEND)
} BlockAlignState;

LayerContext block_align_init(LayerContext *next_layer, int nlayers,
                              size_t block_size);

void block_align_destroy(LayerContext l);

/**
 * @brief open for block_align
 *
 * When reading, this function is just a passthrough.
 * When writing, it also adds the read flag because of this layer's behaviour.
 * If the file doesn't have read permissions, -1 is returned and errno is set
 * accordingly. If a read request is made to one of these file descriptors, -1
 * is also returned and errno is set accordingly. Finally, if @c flags includes
 * O_APPEND, the flag is removed, but the appending behaviour is preserved.
 *
 * @param pathname File path
 * @param flags Open flags
 * @param mode Creation mode
 * @param l Layer Context
 *
 * @return file descriptor or -1 on error
 *
 * @warning When using O_APPEND, the atomicity guaranteed by this flag isn't
 * present.
 */
int block_align_open(const char *pathname, int flags, mode_t mode,
                     LayerContext l);

int block_align_close(int fd, LayerContext l);

/**
 * @brief pread with block alignment.
 *
 * Passes through the read request, but aligned with the defined block size.
 * Even though it reads complete blocks, it only writes to @c buffer the bytes
 * that were requested.
 *
 * @param fd file to read
 * @param buffer buffer to write the bytes read to
 * @param nbytes number of bytes to read
 * @param offset position to start reading
 * @param l current layer context
 *
 * @return number of bytes read (independent from the blocks read)
 */
ssize_t block_align_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                          LayerContext l);

/**
 * @brief pwrite with block alignment.
 *
 * First, it reads the necessary blocks, opening a new file descriptor for
 * reading. After this, the blocks read are modified according to the write
 * request that was made. Finally, the whole blocks are written (after being
 * modified).
 *
 * @param fd file to write
 * @param buffer buffer with the content to write
 * @param nbytes number of bytes to write
 * @param offset position to start writing
 * @param l current layer context
 *
 * @return number of bytes that were written (independent from the blocks that
 * were read and then re-written)
 */
ssize_t block_align_pwrite(int fd, const void *buffer, size_t nbytes,
                           off_t offset, LayerContext l);

/**
 * @brief ftruncate for block align layer.
 *
 * @param fd file to truncate
 * @param length new length of the file
 * @param l current layer context
 *
 * @return 0 on success, -1 on error
 */
int block_align_ftruncate(int fd, off_t length, LayerContext l);

/**
 * @brief fstat for block align layer. Call fstat on the underlying layer.
 *
 * @param fd file to stat
 * @param stbuf buffer to write the stat information to
 * @param l current layer context
 *
 * @return 0 on success, -1 on error
 */
int block_align_fstat(int fd, struct stat *stbuf, LayerContext l);

/**
 * @brief lstat for block align layer. Call lstat on the underlying layer.
 *
 * @param pathname path to stat
 * @param stbuf buffer to write the stat information to
 * @param l current layer context
 *
 * @return 0 on success, -1 on error
 */
int block_align_lstat(const char *pathname, struct stat *stbuf, LayerContext l);

/**
 * @brief unlink for block align layer. Call unlink on the underlying layer.
 *
 * @param pathname path to file
 * @param l current layer context
 *
 * @return 0 on success, -1 on error
 */
int block_align_unlink(const char *pathname, LayerContext l);
#endif
