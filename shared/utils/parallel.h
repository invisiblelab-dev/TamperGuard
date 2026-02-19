#ifndef PARALLEL_H
#define PARALLEL_H

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

// Forward declarations
typedef struct layer_context LayerContext;

// Structure to hold thread parameters for write operations
typedef struct {
  int layer_index;
  int layer_fd;
  ssize_t *result_ptr;
  LayerContext *layer_context;
  const void *buffer;
  size_t nbyte;
  off_t offset;
} WriteThreadParams;

// Structure to hold thread parameters for read operations
typedef struct {
  int layer_index;
  int layer_fd;
  ssize_t *result_ptr;
  LayerContext *layer_context;
  void *thread_buffer;   // Each thread gets its own buffer
  void *original_buffer; // Pointer to the original buffer to copy to
  size_t nbyte;
  off_t offset;
  void **thread_buffers_array; // Array to store all thread buffer pointers
} ReadThreadParams;

// Structure to hold thread parameters for fstat operations
typedef struct {
  int layer_index;
  int layer_fd;
  int *result_ptr;
  int *errno_ptr; // Pointer to store errno from this thread
  LayerContext *layer_context;
  struct stat *thread_stbuf;   // Pointer to the thread's stbuf
  struct stat *original_stbuf; // Pointer to the original stbuf
} FstatThreadParams;

// Structure to hold thread paramters for lstat operations
typedef struct {
  int layer_index;
  const char *path;
  int *result_ptr;
  int *errno_ptr; // Pointer to store errno from this thread
  LayerContext *layer_context;
  struct stat *thread_stbuf;   // Pointer to the thread's stbuf
  struct stat *original_stbuf; // Pointer to the original stbuf
} LstatThreadParams;

// Structure to hold thread parameters for open operations
typedef struct {
  int layer_index;
  const char *pathname;
  int flags;
  mode_t mode;
  int *result_ptr;
  LayerContext *layer_context;
} OpenThreadParams;

// Structure to hold thread parameters for close operations
typedef struct {
  int layer_index;
  int layer_fd;
  int *result_ptr;
  LayerContext *layer_context;
} CloseThreadParams;

// Structure to hold thread parameters for ftruncate operations
typedef struct {
  int layer_index;
  int layer_fd;
  int *result_ptr;
  LayerContext *layer_context;
  off_t length; // Length to truncate to
} FtruncateThreadParams;

// Structure to hold thread parameters for unlink operations
typedef struct {
  int layer_index;
  const char *pathname;
  int *result_ptr;
  LayerContext *layer_context;
} UnlinkThreadParams;

// Function declarations
void *parallel_write_worker(void *arg);
void *parallel_read_worker(void *arg);
void *parallel_open_worker(void *arg);
void *parallel_close_worker(void *arg);
void *parallel_ftruncate_worker(void *arg);
void *parallel_unlink_worker(void *arg);

pthread_t *execute_parallel_writes(LayerContext *layers, int nlayers,
                                   int *layer_fds, const void *buffer,
                                   size_t nbyte, off_t offset, ssize_t *results,
                                   int *active_threads);
pthread_t *execute_parallel_reads(LayerContext *layers, int nlayers,
                                  int *layer_fds, void *buffer, size_t nbyte,
                                  off_t offset, ssize_t *results,
                                  int *active_threads, void ***thread_buffers);
pthread_t *execute_parallel_opens(LayerContext *layers, int nlayers,
                                  const char *pathname, int flags, mode_t mode,
                                  int *results, int *active_threads);
pthread_t *execute_parallel_closes(LayerContext *layers, int nlayers,
                                   int *layer_fds, int *results,
                                   int *active_threads);
pthread_t *execute_parallel_ftruncates(LayerContext *layers, int nlayers,
                                       int *layer_fds, off_t length,
                                       int *results, int *active_threads);
pthread_t *execute_parallel_fstats(LayerContext *layers, int nlayers,
                                   int *layer_fds, struct stat *stbuf,
                                   int *results, int *active_threads,
                                   int *thread_errnos,
                                   struct stat ***thread_stbufs);
pthread_t *execute_parallel_lstats(LayerContext *layers, int nlayers,
                                   const char *path, struct stat *stbuf,
                                   int *results, int *active_threads,
                                   int *thread_errnos,
                                   struct stat ***thread_stbufs);
pthread_t *execute_parallel_unlinks(LayerContext *layers, int nlayers,
                                    const char *pathname, int *results,
                                    int *active_threads);

#endif // PARALLEL_H
