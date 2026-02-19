#include "parallel.h"
#include "../../logdef.h"
#include "../../shared/types/layer_context.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INVALID_FD (-1)

static void init_results_array(ssize_t *results, int nlayers) {
  for (int i = 0; i < nlayers; i++) {
    results[i] = INVALID_FD;
  }
}

static void init_int_results_array(int *results, int nlayers) {
  for (int i = 0; i < nlayers; i++) {
    results[i] = INVALID_FD;
  }
}

// Worker function for write operations
void *parallel_write_worker(void *arg) {
  WriteThreadParams *params = (WriteThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lpwrite(
      params->layer_fd, params->buffer, params->nbyte, params->offset,
      *params->layer_context);

  DEBUG_MSG("layer %d wrote %ld bytes", params->layer_index,
            *params->result_ptr);

  free(params);

  return NULL;
}

// Worker function for read operations
void *parallel_read_worker(void *arg) {
  ReadThreadParams *params = (ReadThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lpread(
      params->layer_fd, params->thread_buffer, params->nbyte, params->offset,
      *params->layer_context);

  if (params->thread_buffers_array) {
    params->thread_buffers_array[params->layer_index] = params->thread_buffer;
  }

  free(params);
  return NULL;
}

// Worker function for fstat operations
void *parallel_fstat_worker(void *arg) {
  FstatThreadParams *params = (FstatThreadParams *)arg;

  int res = params->layer_context->ops->lfstat(
      params->layer_fd, params->thread_stbuf, *params->layer_context);
  *params->result_ptr = res;

  if (params->errno_ptr) {
    *params->errno_ptr = (res < 0) ? errno : 0;
  }

  DEBUG_MSG("layer %d fstat with result %d", params->layer_index,
            *params->result_ptr);

  free(params);
  return NULL;
}

// Worker function for lstat operations
void *parallel_lstat_worker(void *arg) {
  LstatThreadParams *params = (LstatThreadParams *)arg;

  int res = params->layer_context->ops->llstat(
      params->path, params->thread_stbuf, *params->layer_context);
  *params->result_ptr = res;

  if (params->errno_ptr) {
    *params->errno_ptr = (res < 0) ? errno : 0;
  }

  // Don't copy stat data here - let the demultiplexer decide which layer's data
  // to use based on enforcement rules (first enforced layer that succeeded)

  DEBUG_MSG("layer %d lstat with result %d", params->layer_index,
            *params->result_ptr);

  // Don't free thread_stbuf here - demultiplexer will clean it up after
  // deciding which layer's data to use
  free(params);
  return NULL;
}

// Worker function for open operations
void *parallel_open_worker(void *arg) {
  OpenThreadParams *params = (OpenThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lopen(
      params->pathname, params->flags, params->mode, *params->layer_context);

  DEBUG_MSG("layer %d opened with fd %d", params->layer_index,
            *params->result_ptr);

  free(params);

  return NULL;
}

// Worker function for close operations
void *parallel_close_worker(void *arg) {
  CloseThreadParams *params = (CloseThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lclose(
      params->layer_fd, *params->layer_context);

  DEBUG_MSG("layer %d closed with result %d", params->layer_index,
            *params->result_ptr);

  free(params);

  return NULL;
}

// Worker function for ftruncate operations
void *parallel_ftruncate_worker(void *arg) {
  FtruncateThreadParams *params = (FtruncateThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lftruncate(
      params->layer_fd, params->length, *params->layer_context);

  if (*params->result_ptr >= 0) {
    DEBUG_MSG("layer %d ftruncated with result %d", params->layer_index,
              *params->result_ptr);
  } else {
    ERROR_MSG("layer %d ftruncated failed with result %d", params->layer_index,
              *params->result_ptr);
  }

  free(params);

  return NULL;
}

// Worker function for unlink operations
void *parallel_unlink_worker(void *arg) {
  UnlinkThreadParams *params = (UnlinkThreadParams *)arg;

  *params->result_ptr = params->layer_context->ops->lunlink(
      params->pathname, *params->layer_context);

  if (*params->result_ptr == 0) {
    DEBUG_MSG("layer %d unlinked with result %d", params->layer_index,
              *params->result_ptr);
  } else {
    ERROR_MSG("layer %d unlinked failed with result %d", params->layer_index,
              *params->result_ptr);
  }

  free(params);
  return NULL;
}

// Auxiliary function to create a write thread
static int create_write_thread(pthread_t *thread, int layer_index, int layer_fd,
                               LayerContext *layer_context, const void *buffer,
                               size_t nbyte, off_t offset,
                               ssize_t *result_ptr) {
  // Allocate parameter structure on heap
  WriteThreadParams *params = malloc(sizeof(WriteThreadParams));
  if (!params) {
    return -1;
  }

  params->layer_index = layer_index;
  params->layer_fd = layer_fd;
  params->buffer = buffer;
  params->nbyte = nbyte;
  params->offset = offset;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;

  return pthread_create(thread, NULL, parallel_write_worker, (void *)params);
}

// Auxiliary function to create an open thread
static int create_open_thread(pthread_t *thread, int layer_index,
                              const char *pathname, int flags, mode_t mode,
                              LayerContext *layer_context, int *result_ptr) {
  OpenThreadParams *params = malloc(sizeof(OpenThreadParams));
  if (!params) {
    return -1;
  }

  params->layer_index = layer_index;
  params->pathname = pathname;
  params->flags = flags;
  params->mode = mode;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;

  return pthread_create(thread, NULL, parallel_open_worker, (void *)params);
}

// Auxiliary function to create a close thread
static int create_close_thread(pthread_t *thread, int layer_index, int layer_fd,
                               LayerContext *layer_context, int *result_ptr) {
  CloseThreadParams *params = malloc(sizeof(CloseThreadParams));
  if (!params) {
    return -1;
  }

  params->layer_index = layer_index;
  params->layer_fd = layer_fd;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;

  return pthread_create(thread, NULL, parallel_close_worker, (void *)params);
}

// Auxiliary function to create a read thread
static int create_read_thread(pthread_t *thread, int layer_index, int layer_fd,
                              LayerContext *layer_context, void *buffer,
                              size_t nbyte, off_t offset, ssize_t *result_ptr,
                              void **thread_buffers_array) {
  ReadThreadParams *params = malloc(sizeof(ReadThreadParams));
  if (!params) {
    ERROR_MSG("Failed to allocate ReadThreadParams for layer %d\n",
              layer_index);
    return -1;
  }

  void *thread_buffer = malloc(nbyte);
  if (!thread_buffer) {
    ERROR_MSG("Failed to allocate thread buffer for layer %d\n", layer_index);
    free(params);
    return -1;
  }

  params->layer_index = layer_index;
  params->layer_fd = layer_fd;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;
  params->thread_buffer = thread_buffer;
  params->original_buffer = buffer;
  params->nbyte = nbyte;
  params->offset = offset;
  params->thread_buffers_array = thread_buffers_array;

  int result =
      pthread_create(thread, NULL, parallel_read_worker, (void *)params);
  if (result != 0) {
    ERROR_MSG("Failed to create read thread for layer %d: %s\n", layer_index,
              strerror(result));
    free(thread_buffer);
    free(params);
    return -1;
  }

  return 0;
}

// Auxiliary function to create a fstat thread
static int create_fstat_thread(pthread_t *thread, int layer_index, int layer_fd,
                               LayerContext *layer_context, struct stat *stbuf,
                               int *result_ptr, int *errno_ptr,
                               struct stat **stbuf_array) {
  FstatThreadParams *params = malloc(sizeof(FstatThreadParams));
  if (!params) {
    ERROR_MSG("Failed to allocate FstatThreadParams for layer %d\n",
              layer_index);
    return -1;
  }

  void *thread_stbuf = malloc(sizeof(struct stat));
  if (!thread_stbuf) {
    ERROR_MSG("Failed to allocate thread stbuf for layer %d\n", layer_index);
    free(params);
    return -1;
  }

  // Store the thread-specific buffer in the array so demultiplexer can access
  // it
  if (stbuf_array) {
    stbuf_array[layer_index] = (struct stat *)thread_stbuf;
  }

  params->layer_index = layer_index;
  params->layer_fd = layer_fd;
  params->result_ptr = result_ptr;
  params->errno_ptr = errno_ptr;
  params->layer_context = layer_context;
  params->thread_stbuf = thread_stbuf;
  params->original_stbuf = stbuf;

  int result =
      pthread_create(thread, NULL, parallel_fstat_worker, (void *)params);
  if (result != 0) {
    ERROR_MSG("Failed to create fstat thread for layer %d: %s\n", layer_index,
              strerror(result));
    free(thread_stbuf);
    free(params);
    return -1;
  }

  return 0;
}

// Auxiliary function to create a lstat thread
static int create_lstat_thread(pthread_t *thread, int layer_index,
                               const char *path, LayerContext *layer_context,
                               struct stat *stbuf, int *result_ptr,
                               int *errno_ptr, struct stat **stbuf_array) {
  LstatThreadParams *params = malloc(sizeof(LstatThreadParams));
  if (!params) {
    ERROR_MSG("Failed to allocate LstatThreadParams for layer %d\n",
              layer_index);
    return -1;
  }

  void *thread_stbuf = malloc(sizeof(struct stat));
  if (!thread_stbuf) {
    ERROR_MSG("Failed to allocate thread stbuf for lstat thread for layer %d\n",
              layer_index);
    free(params);
    return -1;
  }

  // Store the thread-specific buffer in the array so demultiplexer can access
  // it
  if (stbuf_array) {
    stbuf_array[layer_index] = (struct stat *)thread_stbuf;
  }

  params->layer_index = layer_index;
  params->path = path;
  params->result_ptr = result_ptr;
  params->errno_ptr = errno_ptr;
  params->layer_context = layer_context;
  params->thread_stbuf = thread_stbuf;
  params->original_stbuf = stbuf;

  int result =
      pthread_create(thread, NULL, parallel_lstat_worker, (void *)params);
  if (result != 0) {
    ERROR_MSG("Failed to create lstat thread for layer %d: %s\n", layer_index,
              strerror(result));
    free(thread_stbuf);
    free(params);
    return -1;
  }

  return 0;
}

// Auxiliary function to create a ftruncate thread
static int create_ftruncate_thread(pthread_t *thread, int layer_index,
                                   int layer_fd, LayerContext *layer_context,
                                   int *result_ptr, off_t length) {
  FtruncateThreadParams *params = malloc(sizeof(FtruncateThreadParams));
  if (!params) {
    return -1;
  }

  params->layer_index = layer_index;
  params->layer_fd = layer_fd;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;
  params->length = length;

  return pthread_create(thread, NULL, parallel_ftruncate_worker,
                        (void *)params);
}

// Auxiliary function to create a unlink thread
static int create_unlink_thread(pthread_t *thread, int layer_index,
                                const char *pathname,
                                LayerContext *layer_context, int *result_ptr) {
  UnlinkThreadParams *params = malloc(sizeof(UnlinkThreadParams));
  if (!params) {
    return -1;
  }

  params->layer_index = layer_index;
  params->pathname = pathname;
  params->result_ptr = result_ptr;
  params->layer_context = layer_context;

  return pthread_create(thread, NULL, parallel_unlink_worker, (void *)params);
}

// Execute parallel write operations across multiple layers
pthread_t *execute_parallel_writes(LayerContext *layers, int nlayers,
                                   int *layer_fds, const void *buffer,
                                   size_t nbyte, off_t offset, ssize_t *results,
                                   int *active_threads) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (create_write_thread(&threads[*active_threads], i, layer_fds[i],
                            &layers[i], buffer, nbyte, offset,
                            &results[i]) != 0) {
      ERROR_MSG("Failed to create write thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

// Execute parallel open operations across multiple layers
pthread_t *execute_parallel_opens(LayerContext *layers, int nlayers,
                                  const char *pathname, int flags, mode_t mode,
                                  int *results, int *active_threads) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (create_open_thread(&threads[*active_threads], i, pathname, flags, mode,
                           &layers[i], &results[i]) != 0) {
      ERROR_MSG("Failed to create open thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

// Execute parallel close operations across multiple layers
pthread_t *execute_parallel_closes(LayerContext *layers, int nlayers,
                                   int *layer_fds, int *results,
                                   int *active_threads) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (create_close_thread(&threads[*active_threads], i, layer_fds[i],
                            &layers[i], &results[i]) != 0) {
      ERROR_MSG("Failed to create close thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

/**
 * @brief Execute parallel read operations across multiple layers
 *
 * This function creates a thread for each layer and waits for all threads to
 * complete. The thread buffers are stored in the thread_buffers array.
 *
 * The caller must wait for intended threads to complete and copy the data from
 * the thread buffers to the buffer array.
 *
 * @param layers The array of layer contexts
 * @param nlayers The number of layers
 * @param layer_fds The array of layer file descriptors
 * @param buffer The buffer to write the read data to
 * @param nbyte The number of bytes to read
 * @param offset The offset to read from
 * @param results The array of results
 * @param active_threads The number of active threads
 * @param thread_buffers The array of thread buffers
 * @return The array of threads
 */
pthread_t *execute_parallel_reads(LayerContext *layers, int nlayers,
                                  int *layer_fds, void *buffer, size_t nbyte,
                                  off_t offset, ssize_t *results,
                                  int *active_threads, void ***thread_buffers) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_results_array(results, nlayers);

  void **buffers_array = (void **)calloc(nlayers, sizeof(void *));
  if (!buffers_array) {
    free(threads);
    *active_threads = 0;
    return NULL;
  }

  *thread_buffers = buffers_array;

  for (int i = 0; i < nlayers; i++) {
    if (create_read_thread(&threads[*active_threads], i, layer_fds[i],
                           &layers[i], buffer, nbyte, offset, &results[i],
                           buffers_array) != 0) {
      ERROR_MSG("Failed to create read thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

pthread_t *execute_parallel_fstats(LayerContext *layers, int nlayers,
                                   int *layer_fds, struct stat *stbuf,
                                   int *results, int *active_threads,
                                   int *thread_errnos,
                                   struct stat ***thread_stbufs) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  // Allocate array of pointers to thread stat buffers
  struct stat **stbuf_array =
      (struct stat **)malloc(nlayers * sizeof(struct stat *));
  if (!stbuf_array) {
    free(threads);
    *active_threads = 0;
    return NULL;
  }

  // Initialize all pointers to NULL
  for (int i = 0; i < nlayers; i++) {
    stbuf_array[i] = NULL;
  }

  *active_threads = 0;
  *thread_stbufs = stbuf_array;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (!layers[i].ops || !layers[i].ops->lfstat) {
      ERROR_MSG(
          "Skipping fstat thread for layer %d: operation not implemented\n", i);
      results[i] = -1;
      if (thread_errnos)
        thread_errnos[i] = ENOSYS;
      continue;
    }
    if (create_fstat_thread(&threads[*active_threads], i, layer_fds[i],
                            &layers[i], stbuf, &results[i],
                            thread_errnos ? &thread_errnos[i] : NULL,
                            stbuf_array) != 0) {
      ERROR_MSG("Failed to create fstat thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

pthread_t *execute_parallel_lstats(LayerContext *layers, int nlayers,
                                   const char *path, struct stat *stbuf,
                                   int *results, int *active_threads,
                                   int *thread_errnos,
                                   struct stat ***thread_stbufs) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  // Allocate array of pointers to thread stat buffers
  struct stat **stbuf_array =
      (struct stat **)malloc(nlayers * sizeof(struct stat *));
  if (!stbuf_array) {
    free(threads);
    *active_threads = 0;
    return NULL;
  }

  // Initialize all pointers to NULL
  for (int i = 0; i < nlayers; i++) {
    stbuf_array[i] = NULL;
  }

  *active_threads = 0;
  *thread_stbufs = stbuf_array;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (!layers[i].ops || !layers[i].ops->llstat) {
      ERROR_MSG(
          "Skipping lstat thread for layer %d: operation not implemented\n", i);
      results[i] = -1;
      if (thread_errnos)
        thread_errnos[i] = ENOSYS;
      continue;
    }
    if (create_lstat_thread(
            &threads[*active_threads], i, path, &layers[i], stbuf, &results[i],
            thread_errnos ? &thread_errnos[i] : NULL, stbuf_array) != 0) {
      ERROR_MSG("Failed to create lstat thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

pthread_t *execute_parallel_ftruncates(LayerContext *layers, int nlayers,
                                       int *layer_fds, off_t length,
                                       int *results, int *active_threads) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (create_ftruncate_thread(&threads[*active_threads], i, layer_fds[i],
                                &layers[i], &results[i], length) != 0) {
      ERROR_MSG("[PARALLEL_EXECUTE_PARALLEL_FTRUNCATE] Failed to create "
                "ftruncate thread for layer %d\n",
                i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}

pthread_t *execute_parallel_unlinks(LayerContext *layers, int nlayers,
                                    const char *pathname, int *results,
                                    int *active_threads) {
  pthread_t *threads = malloc(nlayers * sizeof(pthread_t));
  if (!threads) {
    *active_threads = 0;
    return NULL;
  }

  *active_threads = 0;

  init_int_results_array(results, nlayers);

  for (int i = 0; i < nlayers; i++) {
    if (!layers[i].ops || !layers[i].ops->lunlink) {
      ERROR_MSG(
          "Skipping unlink thread for layer %d: operation not implemented\n",
          i);
      results[i] = -1;
      continue;
    }
    if (create_unlink_thread(&threads[*active_threads], i, pathname, &layers[i],
                             &results[i]) != 0) {
      ERROR_MSG("Failed to create unlink thread for layer %d\n", i);
      continue;
    }
    (*active_threads)++;
  }

  return threads;
}
