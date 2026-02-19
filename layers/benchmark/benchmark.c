#include "benchmark.h"
#include "../../logdef.h"
#include <time.h>
#include <unistd.h>

void print_times(struct timespec start, struct timespec end, int reps, int fd) {

  double elapsed = (double)(end.tv_sec - start.tv_sec) +
                   (double)(end.tv_nsec - start.tv_nsec) / 1e9;

  double time_per_op = elapsed / reps;
  dprintf(fd,
          "Time necessary to do %d times this operation: %.6fs\nEach operation "
          "took in average %.8fs to conclude\n",
          reps, elapsed, time_per_op);
}

LayerContext benchmark_init(LayerContext *next_layer, int nlayers,
                            int ops_rep) {
  LayerContext layer_state;

  int *ops_rep_mem = malloc(sizeof(int));

  *ops_rep_mem = ops_rep;
  layer_state.internal_state = (void *)ops_rep_mem;
  layer_state.app_context = NULL;

  // Create LayerOps structure
  LayerOps *benchmark_ops = malloc(sizeof(LayerOps));
  benchmark_ops->lpread = benchmark_pread;
  benchmark_ops->lpwrite = benchmark_pwrite;
  benchmark_ops->lopen = benchmark_open;
  benchmark_ops->lclose = benchmark_close;
  benchmark_ops->lftruncate = benchmark_ftruncate;
  benchmark_ops->llstat = benchmark_lstat;
  benchmark_ops->lfstat = benchmark_fstat;
  benchmark_ops->lunlink = benchmark_unlink;
  layer_state.ops = benchmark_ops;

  LayerContext *aux = malloc(sizeof(LayerContext));
  memcpy(aux, next_layer, sizeof(LayerContext));
  layer_state.next_layers = aux;
  layer_state.nlayers = nlayers;

  return layer_state;
}

int benchmark_open(const char *pathname, int flags, mode_t mode,
                   LayerContext l) {
  int fd;
  l.next_layers->app_context = l.app_context;
  fd = l.next_layers->ops->lopen(pathname, flags, mode, *l.next_layers);
  return fd;
}

int benchmark_close(int fd, LayerContext l) {
  int res;
  l.next_layers->app_context = l.app_context;
  res = l.next_layers->ops->lclose(fd, *l.next_layers);
  return res;
}

int benchmark_fstat(int fd, struct stat *stbuf, LayerContext l) {
  int ops_rep = *(int *)l.internal_state;
  l.next_layers->app_context = l.app_context;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  int res = -1;
  for (int i = 0; i < ops_rep; i++) {
    res = l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  print_times(start, end, ops_rep, STDOUT_FILENO);
  return res;
}

int benchmark_lstat(const char *pathname, struct stat *stbuf, LayerContext l) {
  int ops_rep = *(int *)l.internal_state;
  l.next_layers->app_context = l.app_context;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  int res = -1;
  for (int i = 0; i < ops_rep; i++) {
    res = l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  print_times(start, end, ops_rep, STDOUT_FILENO);

  return res;
}

ssize_t benchmark_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                        LayerContext l) {
  int ops_rep = *(int *)l.internal_state;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  ssize_t num_bytes_read = -1;
  for (int i = 0; i < ops_rep; i++) {
    num_bytes_read =
        l.next_layers->ops->lpread(fd, buffer, nbytes, offset, *l.next_layers);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  print_times(start, end, ops_rep, STDOUT_FILENO);

  return num_bytes_read;
}

ssize_t benchmark_pwrite(int fd, const void *buffer, size_t nbytes,
                         off_t offset, LayerContext l) {

  int ops_rep = *(int *)l.internal_state;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  ssize_t res = -1;
  for (int i = 0; i < ops_rep; i++) {
    res =
        l.next_layers->ops->lpwrite(fd, buffer, nbytes, offset, *l.next_layers);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  print_times(start, end, ops_rep, STDOUT_FILENO);

  return res;
}

int benchmark_ftruncate(int fd, off_t length, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  int ops_rep = *(int *)l.internal_state;
  int res = -1;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (int i = 0; i < ops_rep; i++) {
    res = l.next_layers->ops->lftruncate(fd, length, *l.next_layers);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  print_times(start, end, ops_rep, STDOUT_FILENO);

  return res;
}

int benchmark_unlink(const char *pathname, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lunlink(pathname, *l.next_layers);
}
