#include "mock_layer.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Mock layer operations
static ssize_t mock_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                          LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->pread_called++;

  size_t mock_file_size =
      state->mock_pread_data_size; // Use the actual data size

  if (offset >= mock_file_size) {
    return 0;
  }

  // Calculate how many bytes we can actually read
  ssize_t bytes_to_read = (ssize_t)nbyte;
  if (offset + nbyte > mock_file_size) {
    bytes_to_read = (ssize_t)(mock_file_size - offset);
  }

  // Copy the data into the buffer
  memcpy(buffer, state->mock_pread_data + offset, bytes_to_read);

  return bytes_to_read;
}

static ssize_t mock_pwrite(int fd, const void *buffer, size_t nbyte,
                           off_t offset, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->pwrite_called++;
  state->pwrite_input_nbyte = nbyte;

  // If pwrite data storage is enabled, make an owned copy of the data
  if (state->enable_pwrite_data_storage) {
    // Free previous storage (we only keep the last write)
    if (state->pwrite_data_storage) {
      free(state->pwrite_data_storage);
    }

    // Allocate new storage for this write
    state->pwrite_data_storage = malloc(nbyte);
    if (!state->pwrite_data_storage) {
      (void)fprintf(stderr,
                    "Mock layer: failed to allocate pwrite data storage\n");
      return -1;
    }

    // Copy the data (now we own it and can safely free it later)
    memcpy(state->pwrite_data_storage, buffer, nbyte);
    state->pwrite_data_storage_size = nbyte;
  }

  return (ssize_t)nbyte;
}

static int mock_open(const char *pathname, int flags, mode_t mode,
                     LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->open_called++;
  state->last_open_input_flags = flags;
  strncpy(state->last_open_input_path, pathname,
          sizeof(state->last_open_input_path) - 1);
  return state->open_return_value;
}

static int mock_close(int fd, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->close_called++;
  return state->close_return_value;
}

static int mock_ftruncate(int fd, off_t length, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->ftruncate_called++;
  state->last_ftruncate_input_fd = fd;
  state->last_ftruncate_input_length = length;
  return state->ftruncate_return_value;
}

static int mock_truncate(const char *path, off_t length, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->truncate_called++;
  return state->truncate_return_value;
}

static int mock_fstat(int fd, struct stat *stbuf, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->fstat_called++;
  *stbuf = state->stat_lower_layer_stat;

  // Set errno if operation should fail
  if (state->fstat_return_value < 0 && state->stat_errno_value != 0) {
    errno = state->stat_errno_value;
  }

  return state->fstat_return_value;
}

static int mock_lstat(const char *pathname, struct stat *stbuf,
                      LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->lstat_called++;
  *stbuf = state->stat_lower_layer_stat;

  // Set errno if operation should fail
  if (state->lstat_return_value < 0 && state->stat_errno_value != 0) {
    errno = state->stat_errno_value;
  }

  return state->lstat_return_value;
}

static int mock_unlink(const char *pathname, LayerContext l) {
  MockLayerState *state = (MockLayerState *)l.internal_state;
  state->unlink_called++;
  // We copy the pathname to avoid the pointer being invalidated
  state->unlink_pathname_called_str = strdup(pathname);
  return state->unlink_return_value;
}

// Create mock layer operations structure
static LayerOps mock_ops = {.lpread = mock_pread,
                            .lpwrite = mock_pwrite,
                            .lopen = mock_open,
                            .lclose = mock_close,
                            .lftruncate = mock_ftruncate,
                            .ltruncate = mock_truncate,
                            .lfstat = mock_fstat,
                            .llstat = mock_lstat,
                            .lunlink = mock_unlink,
                            .ldestroy = destroy_mock_layer};

LayerContext create_mock_layer(MockLayerState *state) {
  // Create a copy of mock_ops for each layer to avoid shared state issues
  LayerOps *ops_copy = malloc(sizeof(LayerOps));
  if (!ops_copy) {
    exit(1);
  }
  *ops_copy = mock_ops; // Copy the structure

  LayerContext layer = {.ops = ops_copy,
                        .internal_state = state,
                        .app_context = NULL,
                        .nlayers = 0,
                        .next_layers = NULL};
  return layer;
}

void reset_mock_state(MockLayerState *state, int ftruncate_return_value,
                      off_t file_size) {
  if (state->pwrite_data_storage) {
    free(state->pwrite_data_storage);
  }

  memset(state, 0, sizeof(MockLayerState));
  state->ftruncate_return_value = ftruncate_return_value;
  state->unlink_pathname_called_str = NULL;

  // Initialize fstat structure with the file size
  state->stat_lower_layer_stat.st_size = file_size;
  state->stat_lower_layer_stat.st_mode = S_IFREG; // Regular file
  state->unlink_return_value = 0;
  state->mock_pread_data = NULL;
  state->mock_pread_data_size = 0;
  state->pwrite_input_nbyte = 0;
}

void destroy_mock_layer(LayerContext layer) {

  // Free the LayerOps structure if it exists
  if (layer.ops) {
    free(layer.ops);
    layer.ops = NULL;
  }

  // Clean up MockLayerState if it exists
  if (layer.internal_state) {
    MockLayerState *state = (MockLayerState *)layer.internal_state;

    // Free any dynamically allocated strings
    if (state->unlink_pathname_called_str) {
      free(state->unlink_pathname_called_str);
      state->unlink_pathname_called_str = NULL;
    }

    // Free pwrite data storage
    if (state->pwrite_data_storage) {
      free(state->pwrite_data_storage);
      state->pwrite_data_storage = NULL;
    }

    state->pwrite_input_nbyte = 0;
  }

  // Reset the layer context to safe state
  layer.internal_state = NULL;
  layer.app_context = NULL;
  layer.nlayers = 0;
  layer.next_layers = NULL;
}

void enable_mock_pwrite_data_storage(MockLayerState *state) {
  state->enable_pwrite_data_storage = 1;
}

void free_mock_pwrite_data_storage(MockLayerState *state) {
  if (state->pwrite_data_storage) {
    free(state->pwrite_data_storage);
    state->pwrite_data_storage = NULL;
    state->pwrite_data_storage_size = 0;
  }
  state->enable_pwrite_data_storage = 0;
}
