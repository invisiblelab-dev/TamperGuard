#ifndef __DEMULTIPLEXER_H__
#define __DEMULTIPLEXER_H__

#include "../../config/utils.h"
#include "../../shared/types/layer_context.h"
#include <stdlib.h>
#include <unistd.h>

#define MAX_FDS 10000 // TODO: This number should be dynamic and configurable
#define MAX_LAYERS 10 // Maximum number of layers supported
#define INVALID_FD -1 // Value to indicate an invalid fd

typedef struct {
  bool enforced;
} DemultiplexerOptions;

// Structure to store FD mappings - master fd to layer fds
typedef struct {
  int layer_fds[MAX_FDS][MAX_LAYERS]; // Maps master fd -> array of layer fds
  DemultiplexerOptions *options;
} DemultiplexerState;

LayerContext demultiplexer_init(LayerContext *l, int nlayers,
                                int *passthrough_reads, int *passthrough_writes,
                                int *enforced_layers);
void validate_passthrough_ops(int *passthrough_reads, int *passthrough_writes,
                              int n_layers);
ssize_t demultiplexer_pread(int fd, void *buff, size_t nbyte, off_t offset,
                            LayerContext l);
ssize_t demultiplexer_pwrite(int fd, const void *buff, size_t nbyte,
                             off_t offset, LayerContext l);
int demultiplexer_open(const char *pathname, int flags, mode_t mode,
                       LayerContext l);
int demultiplexer_close(int fd, LayerContext l);
void demultiplexer_destroy(LayerContext l);
int demultiplexer_ftruncate(int fd, off_t length, LayerContext l);
int demultiplexer_fstat(int fd, struct stat *stbuf, LayerContext l);
int demultiplexer_lstat(const char *path, struct stat *stbuf, LayerContext l);
int demultiplexer_unlink(const char *pathname, LayerContext l);

#endif
