#include "demultiplexer.h"
#include "../../logdef.h"
#include "../../shared/utils/parallel.h"
#include "enforcement.h"
#include "passthrough_ops.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Init Demultiplexer Layer
 *
 * @param l             -> array of LayerContext (next layers in the three)
 * @param nlayers       -> number of next layers
 * @return LayerContext -> demultiplexer layer
 */
LayerContext demultiplexer_init(LayerContext *l, int nlayers,
                                int *passthrough_reads, int *passthrough_writes,
                                int *enforced_layers) {
  LayerContext new_layer;
  new_layer.app_context = NULL;

  DemultiplexerState *state = malloc(sizeof(DemultiplexerState));
  if (!state) {
    ERROR_MSG("Failed to allocate demultiplexer state");
    exit(1);
  }

  for (int i = 0; i < MAX_FDS; i++) {
    for (int j = 0; j < MAX_LAYERS; j++) {
      state->layer_fds[i][j] = INVALID_FD;
    }
  }

  state->options = malloc(sizeof(DemultiplexerOptions) * nlayers);
  if (!state->options) {
    ERROR_MSG("[DEMULTIPLEXER_INIT] Failed to allocate demultiplexer options");
    exit(1);
  }

  bool enforced_layers_found = false;
  for (int i = 0; i < nlayers; i++) {
    if (enforced_layers[i] == 1) {
      state->options[i].enforced = true;
      enforced_layers_found = true;
    } else {
      state->options[i].enforced = false;
    }
  }

  if (!enforced_layers_found) {
    state->options[0].enforced = true;
  }

  validate_passthrough_ops(passthrough_reads, passthrough_writes, nlayers);

  new_layer.internal_state = state;
  new_layer.next_layers = l;
  new_layer.nlayers = nlayers;

  LayerOps *ops = malloc(sizeof(LayerOps));
  if (!ops) {
    ERROR_MSG("Failed to allocate demultiplexer operations\n");
    free(state);
    exit(1);
  }
  ops->lpread = demultiplexer_pread;
  ops->lpwrite = demultiplexer_pwrite;
  ops->lopen = demultiplexer_open;
  ops->lclose = demultiplexer_close;
  ops->lftruncate = demultiplexer_ftruncate;
  ops->lfstat = demultiplexer_fstat;
  ops->llstat = demultiplexer_lstat;
  ops->lunlink = demultiplexer_unlink;

  // Set passthrough operations for the next layers
  for (int i = 0; i < nlayers; i++) {
    if (passthrough_reads[i] == 1) {
      if (enforced_layers[i] == 1) {
        ERROR_MSG("[DEMULTIPLEXER_LAYER: INIT] Layer %d cannot have "
                  "passthrough reads and be enforced",
                  i);
        exit(1);
      }
      l[i].ops->lpread = passthrough_pread;
    }
    if (passthrough_writes[i] == 1) {
      if (enforced_layers[i] == 1) {
        ERROR_MSG("Layer %d cannot have passthrough writes and be enforced", i);
        exit(1);
      }
      l[i].ops->lpwrite = passthrough_pwrite;
    }
  }

  new_layer.ops = ops;

  return new_layer;
}

/**
 * @brief Pread demultiplexed across the next layers
 *
 * @param fd       -> file descriptor (from first layer)
 * @param buff     -> buffer to read into
 * @param nbyte    -> number of bytes to read
 * @param offset   -> offset value
 * @param l        -> Context for the demultiplexer layer
 * @return ssize_t -> number of read bytes from layer 0
 */
ssize_t demultiplexer_pread(int fd, void *buff, size_t nbyte, off_t offset,
                            LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  int nlayers = l.nlayers;
  int layer_fds[nlayers];
  for (int i = 0; i < nlayers; i++) {
    layer_fds[i] = state->layer_fds[fd][i];
  }

  // Execute parallel reads and get all results
  ssize_t results[nlayers];
  int active_threads = 0;
  void **thread_buffers = NULL;
  pthread_t *threads =
      execute_parallel_reads(l.next_layers, nlayers, layer_fds, buff, nbyte,
                             offset, results, &active_threads, &thread_buffers);
  if (threads == NULL) {
    ERROR_MSG("[DEMULTIPLEXER_PREAD] Failed to allocate threads for parallel "
              "reads");
    return -1;
  }

  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  // Determine which layer's result to use and copy its data
  ssize_t final_result =
      get_enforced_layers_ssize_result(results, nlayers, state);

  // Find the first enforced layer that succeeded to copy its data
  int chosen_layer = -1;
  if (final_result > 0) {
    for (int i = 0; i < nlayers; i++) {
      if (state->options[i].enforced && results[i] > 0) {
        chosen_layer = i;
        break;
      }
    }
  }

  // Validate buffer access before memcpy to prevent buffer overflow
  if (chosen_layer >= 0 && thread_buffers && thread_buffers[chosen_layer] &&
      final_result > 0 && final_result <= nbyte) {
    memcpy(buff, thread_buffers[chosen_layer], final_result);
  } else {
    final_result = -1; // Indicate failure
  }

  // Clean up all thread buffers - now safe since all threads have completed
  if (thread_buffers) {
    for (int i = 0; i < nlayers; i++) {
      if (thread_buffers[i]) {
        free(thread_buffers[i]);
        thread_buffers[i] = NULL;
      }
    }
    free((void *)thread_buffers);
  }

  return final_result;
}

/**
 * @brief pwrite demultiplexed across the next layers
 *
 * @param fd       -> file descriptor (from first layer)
 * @param buff     -> buffer to write
 * @param nbyte    -> number of bytes to write
 * @param offset   -> offset value
 * @param l        -> context of the demultiplexer layer
 * @return ssize_t -> number of written bytes in the first layer (index 0)
 */
ssize_t demultiplexer_pwrite(int fd, const void *buff, size_t nbyte,
                             off_t offset, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }
  int nlayers = l.nlayers;

  int layer_fds[nlayers];
  for (int i = 0; i < nlayers; i++) {
    layer_fds[i] = state->layer_fds[fd][i];
  }

  ssize_t results[nlayers];
  int active_threads = 0;
  pthread_t *threads =
      execute_parallel_writes(l.next_layers, nlayers, layer_fds, buff, nbyte,
                              offset, results, &active_threads);
  if (threads == NULL) {
    ERROR_MSG("[DEMULTIPLEXER_PWRITE] Failed to allocate threads for parallel "
              "writes");
    return -1;
  }

  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  return get_enforced_layers_ssize_result(results, nlayers, state);
}

/**
 * @brief open demultiplexer
 *
 * @param pathname -> path of the file to open
 * @param flags    -> flags for opened file
 * @param mode     -> mode (permissions) when creating a file
 * @param l        -> LayerContext for current layer
 * @return int     -> first layer's file descriptor (master FD)
 */
int demultiplexer_open(const char *pathname, int flags, mode_t mode,
                       LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;
  int nlayers = l.nlayers;
  int results[nlayers];
  int active_threads = 0;
  pthread_t *threads = execute_parallel_opens(
      l.next_layers, nlayers, pathname, flags, mode, results, &active_threads);
  if (threads == NULL) {
    ERROR_MSG("[DEMULTIPLEXER_OPEN] Failed to allocate threads for parallel "
              "opens");
    return -1;
  }
  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  // the returned fd is the one from the first layer
  // as the open waits for all layers to complete,
  // there is no need to check for enforced layers
  int master_fd = results[0];

  if (master_fd >= 0) {
    for (int i = 0; i < nlayers; i++) {
      state->layer_fds[master_fd][i] = results[i];
    }
  }

  return master_fd;
}

/**
 * @brief close demultiplexer
 *
 * @param fd    -> file descriptor (master FD)
 * @param l     -> LayerContext for current layer
 * @return int  -> result from the first layer (index 0)
 */
int demultiplexer_close(int fd, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  int nlayers = l.nlayers;
  int layer_fds[nlayers];
  for (int i = 0; i < nlayers; i++) {
    layer_fds[i] = state->layer_fds[fd][i];
    state->layer_fds[fd][i] = INVALID_FD; // Clear the mapping
  }

  int results[nlayers];
  int active_threads = 0;
  pthread_t *threads = execute_parallel_closes(
      l.next_layers, nlayers, layer_fds, results, &active_threads);
  if (threads == NULL) {
    ERROR_MSG("[DEMULTIPLEXER_CLOSE] Failed to allocate threads for parallel "
              "closes");
    return -1;
  }
  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  return results[0];
}

/**
 * @brief ftruncate demultiplexed across the next layers
 *
 * @param fd       -> file descriptor (master FD)
 * @param length   -> new length of the file
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the first layer (index 0)
 */
int demultiplexer_ftruncate(int fd, off_t length, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }
  int nlayers = l.nlayers;

  int layer_fds[nlayers];
  for (int i = 0; i < nlayers; i++) {
    layer_fds[i] = state->layer_fds[fd][i];
  }

  int results[nlayers];
  int active_threads = 0;
  pthread_t *threads = execute_parallel_ftruncates(
      l.next_layers, nlayers, layer_fds, length, results, &active_threads);
  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  return get_enforced_layers_int_result(results, nlayers, state);
}

/**
 * @brief fstat demultiplexed across the next layers
 *
 * @param fd       -> file descriptor (master FD)
 * @param stbuf    -> pointer to the stat structure
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the first layer (index 0), gets copied to the
 * original buffer
 */
int demultiplexer_fstat(int fd, struct stat *stbuf, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  int nlayers = l.nlayers;
  int layer_fds[nlayers];
  for (int i = 0; i < nlayers; i++) {
    layer_fds[i] = state->layer_fds[fd][i];
  }

  int results[nlayers];
  int thread_errnos[nlayers];
  int active_threads = 0;
  struct stat **thread_stbufs = NULL;
  pthread_t *threads =
      execute_parallel_fstats(l.next_layers, nlayers, layer_fds, stbuf, results,
                              &active_threads, thread_errnos, &thread_stbufs);
  wait_for_all_threads(threads, active_threads, nlayers, state);

  int final_result = get_enforced_layers_int_result(results, nlayers, state);

  // Find the first enforced layer that succeeded and copy its stat data
  int chosen_layer = -1;
  if (final_result == 0) {
    for (int i = 0; i < nlayers; i++) {
      if (state->options[i].enforced && results[i] == 0) {
        chosen_layer = i;
        break;
      }
    }
  }

  // Copy stat data from the chosen layer to the original buffer
  if (chosen_layer >= 0 && thread_stbufs && thread_stbufs[chosen_layer]) {
    memcpy(stbuf, thread_stbufs[chosen_layer], sizeof(struct stat));
  }

  // Clean up thread stat buffers
  if (thread_stbufs) {
    for (int i = 0; i < nlayers; i++) {
      if (thread_stbufs[i]) {
        free(thread_stbufs[i]);
      }
    }
    free((void *)thread_stbufs);
  }

  if (final_result < 0) {
    for (int i = 0; i < nlayers; i++) {
      if (state->options[i].enforced && results[i] < 0) {
        errno = thread_errnos[i];
        break;
      }
    }
  }

  return final_result;
}

/**
 * @brief lstat demultiplexed across the next layers
 *
 * @param path     -> path of the file to stat
 * @param stbuf    -> pointer to the stat structure
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the first layer (index 0), gets copied to the
 * original buffer
 */
int demultiplexer_lstat(const char *path, struct stat *stbuf, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;

  int nlayers = l.nlayers;

  int results[nlayers];
  int thread_errnos[nlayers];
  int active_threads = 0;
  struct stat **thread_stbufs = NULL;
  pthread_t *threads =
      execute_parallel_lstats(l.next_layers, nlayers, path, stbuf, results,
                              &active_threads, thread_errnos, &thread_stbufs);
  wait_for_all_threads(threads, active_threads, nlayers, state);

  int final_result = get_enforced_layers_int_result(results, nlayers, state);

  // Find the first enforced layer that succeeded and copy its stat data
  int chosen_layer = -1;
  if (final_result == 0) {
    for (int i = 0; i < nlayers; i++) {
      if (state->options[i].enforced && results[i] == 0) {
        chosen_layer = i;
        break;
      }
    }
  }

  // Copy stat data from the chosen layer to the original buffer
  if (chosen_layer >= 0 && thread_stbufs && thread_stbufs[chosen_layer]) {
    memcpy(stbuf, thread_stbufs[chosen_layer], sizeof(struct stat));
  }

  // Clean up thread stat buffers
  if (thread_stbufs) {
    for (int i = 0; i < nlayers; i++) {
      if (thread_stbufs[i]) {
        free(thread_stbufs[i]);
      }
    }
    free((void *)thread_stbufs);
  }

  if (final_result < 0) {
    for (int i = 0; i < nlayers; i++) {
      if (state->options[i].enforced && results[i] < 0) {
        errno = thread_errnos[i];
        break;
      }
    }
  }

  return final_result;
}

/**
 * @brief unlink demultiplexed across the next layers
 *
 * @param pathname -> path of the file to unlink
 * @param l        -> LayerContext for current layer
 * @return int     -> result from the first layer (index 0)
 */
int demultiplexer_unlink(const char *pathname, LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;
  int nlayers = l.nlayers;

  int results[nlayers];
  int active_threads = 0;
  pthread_t *threads = execute_parallel_unlinks(
      l.next_layers, nlayers, pathname, results, &active_threads);
  wait_for_all_threads(threads, active_threads, nlayers, state);
  free(threads);

  return get_enforced_layers_int_result(results, nlayers, state);
}

/**
 * @brief function to destroy the layer
 *
 * @param l
 */
void demultiplexer_destroy(LayerContext l) {
  DemultiplexerState *state = (DemultiplexerState *)l.internal_state;
  if (state) {
    free(state);
  }

  if (l.ops) {
    free(l.ops);
  }
}

/**
 * @brief Validate passthrough operations
 *
 * @param passthrough_reads
 * @param passthrough_writes
 * @param n_layers
 */
void validate_passthrough_ops(int *passthrough_reads, int *passthrough_writes,
                              int n_layers) {
  int has_non_passthrough_read = 1;
  int has_non_passthrough_write = 1;

  for (int i = 0; i < n_layers; i++) {
    // Rule: No layer can have both read and write passthrough
    if (passthrough_reads[i] == 1 && passthrough_writes[i] == 1) {
      ERROR_MSG("Layer cannot have both read and write passthrough operations");
      exit(1);
    }

    // Check if there are layers that can actually perform operations (not just
    // passthrough)
    if (passthrough_reads[i] == 0) {
      has_non_passthrough_read = 0;
    }
    if (passthrough_writes[i] == 0) {
      has_non_passthrough_write = 0;
    }
  }

  if (has_non_passthrough_read == 1) {
    ERROR_MSG("At least one layer must be able to perform read operations "
              "(not all can be passthrough)");
    exit(1);
  }
  if (has_non_passthrough_write == 1) {
    ERROR_MSG("At least one layer must be able to perform write operations "
              "(not all can be passthrough)");
    exit(1);
  }
}
