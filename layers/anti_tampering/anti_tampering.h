#ifndef __ANTI_TAMPERING_H__
#define __ANTI_TAMPERING_H__

#include "../../shared/types/layer_context.h"
#include "../../shared/utils/hasher/hasher.h"
#include "../../shared/utils/locking.h"
#include "config.h"
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FDS 1000000 // TODO: This number should be dynamic and configurable

typedef struct {
  int file_fd;
  char *file_path;
  char *hash_path;
} FileMapping;

typedef struct {
  Hasher hasher;                 // hasher instance for computing hashes
  LayerContext hash_layer;       // hash layer where the hash will be stored
  LayerContext data_layer;       // data layer where the data is stored
  FileMapping mappings[MAX_FDS]; // centralized mapping
  char *hash_prefix;             // prefix for the hash path
  LockTable *lock_table;         // path-based reader-writer lock table
  anti_tampering_mode_t mode;    // file or block mode
  size_t block_size;             // only meaningful for block mode
} AntiTamperingState;

LayerContext anti_tampering_init(LayerContext data_layer,
                                 LayerContext hash_layer,
                                 const AntiTamperingConfig *config);

ssize_t anti_tampering_write(int fd, const void *buffer, size_t nbyte,
                             off_t offset, LayerContext l);
ssize_t anti_tampering_read(int fd, void *buffer, size_t nbyte, off_t offset,
                            LayerContext l);
int anti_tampering_open(const char *pathname, int flags, __mode_t mode,
                        LayerContext l);
int anti_tampering_close(int fd, LayerContext l);
void anti_tampering_destroy(LayerContext l);
int anti_tampering_ftruncate(int fd, off_t length, LayerContext l);
int anti_tampering_fstat(int fd, struct stat *stbuf, LayerContext l);
int anti_tampering_lstat(const char *pathname, struct stat *stbuf,
                         LayerContext l);
int anti_tampering_unlink(const char *pathname, LayerContext l);

#endif
