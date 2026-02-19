#ifndef __MOCK_LAYER_H__
#define __MOCK_LAYER_H__

#include "../shared/types/layer_context.h"
#include "../shared/utils/compressor/compressor.h"
#include <string.h>

// Mock layer state
typedef struct {
  int ftruncate_called;
  int last_ftruncate_input_fd;
  off_t last_ftruncate_input_length;
  int ftruncate_return_value;

  int truncate_called;
  int truncate_return_value;

  int open_called;
  int last_open_input_flags;
  char last_open_input_path[256];

  int close_called;

  int pwrite_called;
  size_t pwrite_input_nbyte;
  char *pwrite_data_storage;
  size_t pwrite_data_storage_size;

  int pread_called;
  const char *mock_pread_data;
  size_t mock_pread_data_size;

  int open_return_value;
  int close_return_value;

  int fstat_called;
  int fstat_return_value;

  int lstat_called;
  int lstat_return_value;

  int stat_errno_value; // errno to set when fstat fails
  struct stat stat_lower_layer_stat;

  int unlink_called;
  char *unlink_pathname_called_str;
  int unlink_return_value;

  int enable_pwrite_data_storage; // Flag to enable capturing writes
} MockLayerState;

// Function declarations
LayerContext create_mock_layer(MockLayerState *state);
void reset_mock_state(MockLayerState *state, int ftruncate_return_value,
                      off_t file_size);
void destroy_mock_layer(LayerContext layer);
void enable_mock_pwrite_data_storage(MockLayerState *state);
void free_mock_pwrite_data_storage(MockLayerState *state);

#endif
