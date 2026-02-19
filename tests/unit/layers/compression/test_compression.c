#include "../../../../layers/compression/compression.h"
#include "../../../../layers/compression/compression_utils.h"
#include "../../../../lib/uthash/src/uthash.h"
#include "../../../../shared/utils/compressor/compressor.h"
#include "../../../mock_layer.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static MockLayerState mock_state;
static LayerContext mock_layer;

// Fixed device/inode used for path-based tests migrated to inode keys
static dev_t test_dev = (dev_t)1;
static ino_t test_ino = (ino_t)1;

static const char test_data[] =
    "This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog. This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog.";

typedef struct {
  void *data;
  size_t size;
} CompressedData;

static CompressedData mock_compressed_data(const Compressor *compressor,
                                           const char *test_data,
                                           size_t test_data_size) {
  size_t compress_bound =
      compressor->get_compress_bound(test_data_size, compressor->level);
  void *compressed_data = malloc(compress_bound);
  if (!compressed_data) {
    printf("Failed to allocate memory for compressed data\n");
    exit(1);
  }
  ssize_t compressed_size =
      compressor->compress_data(test_data, test_data_size, compressed_data,
                                compress_bound, compressor->level);
  return (CompressedData){compressed_data, (size_t)compressed_size};
}

void setup_mock_layer() {
  reset_mock_state(&mock_state, 0, 0);
  mock_layer = create_mock_layer(&mock_state);
}

LayerContext create_default_compression_layer() {
  CompressionConfig config = {
      .algorithm = COMPRESSION_ZSTD, .level = 3, .mode = COMPRESSION_MODE_FILE};
  LayerContext compression_layer = compression_init(&mock_layer, &config);
  return compression_layer;
}

static void add_manually_size_to_mapping(const char *path, off_t logical_size,
                                         CompressionState *state) {
  // Ensure mock lstat/fstat report the same (dev, ino) for this path
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;

  // Build a minimal LayerContext to access the mapping helpers
  LayerContext ctx = {0};
  ctx.internal_state = state;
  (void)path; // path no longer used for keying
  if (get_compressed_file_mapping(test_dev, test_ino, state) == NULL) {
    int rc =
        create_compressed_file_mapping(test_dev, test_ino, logical_size, ctx);
    assert(rc == 0);
  } else {
    int rc = set_logical_eof_in_mapping(test_dev, test_ino, logical_size, ctx);
    assert(rc == 0);
  }
}

//========Init tests========

void test_compression_init_lz4() {
  printf("Testing compression_init lz4 case...\n");

  setup_mock_layer();

  // Test with LZ4
  CompressionConfig config_lz4 = {
      .algorithm = COMPRESSION_LZ4, .level = 5, .mode = COMPRESSION_MODE_FILE};

  LayerContext compression_layer = compression_init(&mock_layer, &config_lz4);

  // Verify the layer was created
  assert(compression_layer.internal_state != NULL);
  assert(compression_layer.ops != NULL);
  assert(compression_layer.next_layers != NULL);
  assert(compression_layer.app_context == NULL);

  // Verify the compressor was initialized correctly
  const CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(state->compressor.algorithm == COMPRESSION_LZ4);
  assert(state->compressor.level == 5);
  assert(state->compressor.compress_data != NULL);
  assert(state->compressor.decompress_data != NULL);
  assert(state->compressor.get_compress_bound != NULL);
  assert(state->compressor.get_original_file_size != NULL);

  for (int i = 0; i < MAX_FDS; i++) {
    assert(state->fd_to_inode[i].path == NULL);
  }
  assert(state->file_mapping == NULL);
  assert(state->lock_table != NULL);

  // Verify all operations are set
  assert(compression_layer.ops->lpread == compression_pread);
  assert(compression_layer.ops->lpwrite == compression_pwrite);
  assert(compression_layer.ops->lopen == compression_open);
  assert(compression_layer.ops->lclose == compression_close);
  assert(compression_layer.ops->lftruncate == compression_ftruncate);
  assert(compression_layer.ops->lfstat == compression_fstat);
  assert(compression_layer.ops->llstat == compression_lstat);

  printf("✅ compression_init LZ4 test passed\n");

  compression_destroy(compression_layer);
}

void test_compression_init_zstd() {
  printf("Testing compression_init with ZSTD...\n");

  setup_mock_layer();

  CompressionConfig config_zstd = {.algorithm = COMPRESSION_ZSTD,
                                   .level = 10,
                                   .mode = COMPRESSION_MODE_FILE};

  LayerContext compression_layer = compression_init(&mock_layer, &config_zstd);

  // Verify the compressor was initialized correctly
  const CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(state->compressor.algorithm == COMPRESSION_ZSTD);
  assert(state->compressor.level == 10);
  assert(state->compressor.compress_data != NULL);
  assert(state->compressor.decompress_data != NULL);
  assert(state->compressor.get_compress_bound != NULL);
  assert(state->compressor.get_original_file_size != NULL);

  assert(state->fd_to_inode != NULL);
  for (int i = 0; i < MAX_FDS; i++) {
    assert(state->fd_to_inode[i].path == NULL);
  }
  assert(state->file_mapping == NULL);
  assert(state->lock_table != NULL);

  printf("✅ compression_init ZSTD test passed\n");

  compression_destroy(compression_layer);
}

void test_compression_init_returns_error_when_compressor_init_fails() {
  printf(
      "Testing compression_init returns error when compressor_init fails...\n");

  setup_mock_layer();

  CompressionConfig bad_config = {
      .algorithm =
          (compression_algorithm_t)9999, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
      .level = 0,
      .mode = COMPRESSION_MODE_FILE};

  // Fork creates a copy of the current process
  // - Parent process continues with pid > 0 (child's process ID)
  // - Child process starts with pid == 0
  pid_t pid = fork();
  if (pid == 0) {
    // CHILD PROCESS: Run the code that might call exit(1)
    compression_init(&mock_layer, &bad_config);
    // This line should never be reached - compression_init should exit(1) first
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status)); // Child called exit(), not killed by signal
    assert(WEXITSTATUS(status) == 1); // Child called exit(1)

    printf("✅ compression_init compressor_init failure test passed\n");
  }
}

//========Destroy tests========

void test_compression_destroy_success() {
  printf("Testing compression_destroy success case...\n");

  setup_mock_layer();

  CompressionConfig config = {.algorithm = COMPRESSION_LZ4, .level = 5};

  LayerContext compression_layer = compression_init(&mock_layer, &config);

  compression_destroy(compression_layer);

  printf("✅ compression_destroy test passed\n");
}

// Test: Null pathname
void test_compression_open_null_path() {
  printf("Testing compression_open with NULL path...\n");
  setup_mock_layer();

  LayerContext compression_layer = create_default_compression_layer();

  int fd = compression_open(NULL, O_RDONLY, 0, compression_layer);
  assert(fd == INVALID_FD);

  compression_destroy(compression_layer);
  printf("✅ compression_open NULL path test passed\n");
}

// Test: Lower layer open fails
void test_compression_open_lower_layer_fails() {
  printf("Testing compression_open when lower layer open fails...\n");
  setup_mock_layer();
  mock_state.open_return_value = -42; // Simulate failure
  LayerContext compression_layer = create_default_compression_layer();

  int fd = compression_open("file.txt", O_RDONLY, 0, compression_layer);
  assert(fd == -42);

  compression_destroy(compression_layer);
  printf("✅ compression_open lower layer fail test passed\n");
}

// Test: File descriptor exceeds MAX_FDS
void test_compression_open_fd_exceeds_max() {
  printf("Testing compression_open with fd >= MAX_FDS...\n");
  setup_mock_layer();
  mock_state.open_return_value = MAX_FDS; // Simulate fd too large
  LayerContext compression_layer = create_default_compression_layer();

  int fd = compression_open("file.txt", O_RDONLY, 0, compression_layer);
  assert(fd == INVALID_FD);
  assert(mock_state.close_called == 1);

  compression_destroy(compression_layer);
  printf("✅ compression_open fd exceeds MAX_FDS test passed\n");
}
// Test: Normal open
void test_compression_open_success() {
  printf("Testing compression_open success case...\n");
  setup_mock_layer();
  mock_state.open_return_value = 5; // Valid fd
  LayerContext compression_layer = create_default_compression_layer();

  int fd =
      compression_open("file.txt", O_RDONLY | O_CREAT, 0, compression_layer);
  assert(fd == 5);
  assert(mock_state.open_called == 1);

  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(state->fd_to_inode[5].path != NULL);
  assert(strcmp(state->fd_to_inode[5].path, "file.txt") == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_open success test passed\n");
}

// Test: Reusing an fd
void test_compression_open_reuse_fd() {
  printf("Testing compression_open reusing an fd...\n");

  setup_mock_layer();
  mock_state.open_return_value = 7; // Valid fd
  LayerContext compression_layer = create_default_compression_layer();

  // First open
  int fd1 =
      compression_open("file1.txt", O_RDONLY | O_CREAT, 0, compression_layer);
  assert(fd1 == 7);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(strcmp(state->fd_to_inode[7].path, "file1.txt") == 0);

  int res = compression_close(fd1, compression_layer);
  assert(res == 0);
  assert(state->fd_to_inode[7].path == NULL);

  // Second open with same fd, should replace mapping
  // We need to set the dev and ino because if the
  // mapping for the default values was set by the first open
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd2 =
      compression_open("file2.txt", O_RDONLY | O_CREAT, 0, compression_layer);
  assert(fd2 == 7);
  assert(strcmp(state->fd_to_inode[7].path, "file2.txt") == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_open reuse fd test passed\n");
}

// Test: Open with O_TRUNC flag
void test_compression_open_with_trunc_flag() {
  printf("Testing compression_open with O_TRUNC flag...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = 100;

  // We first manually add the file size to the mapping
  add_manually_size_to_mapping(path, original_size, state);
  // Check if the file size was updated in the file_size_mapping to 0
  LayerContext ctx = {0};
  ctx.internal_state = state;
  off_t ls_tmp = -1;
  assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx, &ls_tmp) == 0);
  assert(ls_tmp == 100);

  int fd = compression_open(path, O_WRONLY | O_TRUNC, 0, compression_layer);
  assert(fd == 0);

  // Check if the file size was updated in the file_size_mapping to 0
  ctx.internal_state = state;
  off_t ls_tmp2 = -1;
  assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx, &ls_tmp2) == 0);
  assert(ls_tmp2 == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_open with O_TRUNC flag test passed\n");
}

void test_compression_open_with_create_flag() {
  printf("Testing compression_open with O_CREAT flag...\n");
  setup_mock_layer();

  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;

  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd =
      compression_layer.ops->lopen("test.txt", O_CREAT, 0, compression_layer);
  assert(fd == 0);

  LayerContext ctx = {0};
  ctx.internal_state = state;
  off_t ls_tmp = -1;
  assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx, &ls_tmp) == 0);
  assert(ls_tmp == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_open with O_CREAT flag test passed\n");
}

// Test: Invalid file descriptor
void test_compression_close_invalid_fd() {
  printf("Testing compression_close with invalid fd...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();

  // Test negative fd
  int result = compression_close(-1, compression_layer);
  assert(result == INVALID_FD);

  // Test fd >= MAX_FDS
  result = compression_close(MAX_FDS, compression_layer);
  assert(result == INVALID_FD);

  compression_destroy(compression_layer);
  printf("✅ compression_close invalid fd test passed\n");
}

// Test: Lower layer close fails
void test_compression_close_lower_layer_fails() {
  printf("Testing compression_close when lower layer close fails...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();

  int fd =
      compression_open("test.txt", O_RDONLY | O_CREAT, 0, compression_layer);
  assert(fd >= 0);
  mock_state.close_return_value = -42; // Simulate failure
  int result = compression_close(fd, compression_layer);
  assert(result == -42);

  compression_destroy(compression_layer);
  printf("✅ compression_close lower layer fail test passed\n");
}

// Test: Close cleans path mapping
void test_compression_close_with_mapping() {
  printf("Testing compression_close with path mapping...\n");
  setup_mock_layer();
  mock_state.close_return_value = 0; // Success
  LayerContext compression_layer = create_default_compression_layer();

  // First, create a mapping by opening a file
  mock_state.open_return_value = 7;
  int fd =
      compression_open("test.txt", O_RDONLY | O_CREAT, 0, compression_layer);
  assert(fd == 7);

  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(state->fd_to_inode[7].path != NULL);
  assert(strcmp(state->fd_to_inode[7].path, "test.txt") == 0);

  // Now close the file
  int result = compression_close(7, compression_layer);
  assert(result == 0);

  // Verify the mapping was freed and set to NULL
  assert(state->fd_to_inode[7].path == NULL);

  compression_destroy(compression_layer);
  printf("✅ compression_close with mapping test passed\n");
}

// Test: Close multiple files
void test_compression_close_multiple_files() {
  printf("Testing compression_close with multiple files...\n");
  setup_mock_layer();
  mock_state.close_return_value = 0; // Success
  LayerContext compression_layer = create_default_compression_layer();

  // Open multiple files
  mock_state.open_return_value = 5;
  compression_open("file1.txt", O_RDONLY, 0, compression_layer);
  mock_state.open_return_value = 7;
  compression_open("file2.txt", O_RDONLY, 0, compression_layer);

  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  assert(state->fd_to_inode[5].path != NULL);
  assert(state->fd_to_inode[7].path != NULL);

  // Close first file
  int result1 = compression_close(5, compression_layer);
  assert(result1 == 0);
  assert(state->fd_to_inode[5].path == NULL);
  assert(state->fd_to_inode[7].path !=
         NULL); // Second mapping should still exist

  // Close second file
  int result2 = compression_close(7, compression_layer);
  assert(result2 == 0);
  assert(state->fd_to_inode[7].path == NULL);

  compression_destroy(compression_layer);
  printf("✅ compression_close multiple files test passed\n");
}

void test_compression_close_with_unlinked_file() {
  printf("Testing compression_close with unlinked file...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;

  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int cr =
      create_compressed_file_mapping(test_dev, test_ino, 0, compression_layer);
  assert(cr == 0);
  CompressedFileMapping *block_index =
      get_compressed_file_mapping(test_dev, test_ino, state);
  assert(block_index != NULL);
  assert(block_index->num_blocks == 0);

  int fd = compression_open("test.txt", O_RDONLY, 0, compression_layer);
  assert(fd >= 0);

  int result = compression_close(fd, compression_layer);
  assert(result == 0);
  assert(state->fd_to_inode[fd].fd == INVALID_FD);
  assert(state->fd_to_inode[fd].device == 0);
  assert(state->fd_to_inode[fd].inode == 0);
  assert(state->fd_to_inode[fd].path == NULL);

  off_t logical_size = -1;
  result = get_logical_eof_from_mapping(test_dev, test_ino, compression_layer,
                                        &logical_size);
  // First the mapping should be present
  assert(result == 0);

  fd = compression_open("test.txt", O_RDONLY, 0, compression_layer);
  assert(fd >= 0);

  result = compression_unlink("test.txt", compression_layer);
  assert(result == 0);

  result = compression_close(fd, compression_layer);
  assert(result == 0);
  assert(state->fd_to_inode[fd].fd == INVALID_FD);
  assert(state->fd_to_inode[fd].device == 0);
  assert(state->fd_to_inode[fd].inode == 0);
  assert(state->fd_to_inode[fd].path == NULL);

  result = get_logical_eof_from_mapping(test_dev, test_ino, compression_layer,
                                        &logical_size);
  assert(result == -1);

  block_index = get_compressed_file_mapping(test_dev, test_ino, state);
  assert(block_index == NULL);

  compression_destroy(compression_layer);
  printf("✅ compression_close with unlinked file test passed\n");
}

//========Pwrite tests========

void test_compression_pwrite_new_file_success() {
  printf("Testing compression_pwrite with new file...\n");

  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  int fd = compression_open("test.txt", O_WRONLY, 0, compression_layer);

  size_t expected_size = strlen(test_data);
  size_t expected_compressed_size = state->compressor.get_compress_bound(
      expected_size, state->compressor.level);

  ssize_t result =
      compression_pwrite(fd, test_data, expected_size, 0, compression_layer);
  assert(result == expected_size);
  assert(mock_state.pwrite_input_nbyte <= expected_compressed_size);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.fstat_called == 1);
  // We don't read the file nor truncate it, we just write to it because it is a
  // new file or empty file.
  assert(mock_state.pread_called == 0);
  assert(mock_state.ftruncate_called == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_pwrite success test passed\n");
}

void test_compression_pwrite_to_eof_existing_file_success() {
  printf("Testing compression_pwrite to eof with existing file...\n");

  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t current_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, current_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  // Add logical size mapping
  LayerContext ctx = {0};
  ctx.internal_state = state;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  assert(set_logical_eof_in_mapping(test_dev, test_ino, current_size, ctx) ==
         0);

  int fd = compression_open(path, O_WRONLY, 0, compression_layer);

  char new_data[] = "new data is this";
  size_t new_data_size = strlen(new_data);
  size_t expected_compressed_size = state->compressor.get_compress_bound(
      current_size + new_data_size, state->compressor.level);

  off_t offset = current_size;
  ssize_t result = compression_pwrite(fd, new_data, new_data_size, offset,
                                      compression_layer);
  assert(result == new_data_size);
  assert(mock_state.pwrite_input_nbyte <= expected_compressed_size);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(mock_state.pread_called == 1);
  assert(mock_state.ftruncate_called == 1);

  // Check if the logical size was updated
  ctx.internal_state = state;
  off_t logical_size = -1;
  assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx, &logical_size) ==
         0);
  assert(logical_size == current_size + new_data_size);
  free(compressed_data.data);

  compression_destroy(compression_layer);
  printf("✅ compression_pwrite existing file success test passed\n");
}

void test_compression_pwrite_to_existing_file_success() {
  printf("Testing compression_pwrite to existing file at offset 0...\n");

  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t current_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, current_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  // Add logical size mapping
  LayerContext ctx = {0};
  ctx.internal_state = state;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  assert(set_logical_eof_in_mapping(test_dev, test_ino, current_size, ctx) ==
         0);

  int fd = compression_open(path, O_WRONLY, 0, compression_layer);

  char new_data[] = "new data is this";
  size_t new_data_size = strlen(new_data);
  size_t expected_compressed_size = state->compressor.get_compress_bound(
      current_size, state->compressor.level);

  off_t offset = 0;
  ssize_t result = compression_pwrite(fd, new_data, new_data_size, offset,
                                      compression_layer);
  assert(result == new_data_size);
  assert(mock_state.pwrite_input_nbyte <= expected_compressed_size);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(mock_state.pread_called == 1);
  assert(mock_state.ftruncate_called == 1);

  // Check if the logical size was unchange
  ctx.internal_state = state;
  off_t logical_size = -1;
  assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx, &logical_size) ==
         0);
  assert(logical_size == current_size);

  free(compressed_data.data);

  compression_destroy(compression_layer);
  printf("✅ compression_pwrite to existing file at offset 0 success test "
         "passed\n");
}

void test_compression_pwrite_returns_0_with_nbyte_0() {
  printf("Testing compression_pwrite with nbyte 0...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  ssize_t result = compression_pwrite(7, test_data, 0, 0, compression_layer);
  assert(result == 0);
  compression_destroy(compression_layer);
  printf("✅ compression_pwrite with nbyte 0 success test passed\n");
}

void test_compression_pwrite_fails_with_invalid_fd() {
  printf("Testing compression_pwrite with invalid fd...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  ssize_t result = compression_pwrite(-1, test_data, strlen(test_data), 0,
                                      compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pwrite with invalid fd test passed\n");
}

void test_compression_pwrite_fails_with_negative_offset() {
  printf("Testing compression_pwrite with negative offset...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  ssize_t result = compression_pwrite(7, test_data, strlen(test_data), -1,
                                      compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pwrite with negative offset test passed\n");
}

void test_compression_pwrite_fails_with_no_open() {
  printf("Testing compression_pwrite with no open...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  ssize_t result =
      compression_pwrite(7, test_data, strlen(test_data), 0, compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pwrite with no open test passed\n");
}

void test_compression_pwrite_fails_with_get_file_sizeerror() {
  printf("Testing compression_pwrite with fstat error...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  mock_state.fstat_return_value = -1;
  int fd = compression_open("test.txt", O_WRONLY, 0, compression_layer);
  ssize_t result = compression_pwrite(fd, test_data, strlen(test_data), 0,
                                      compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pwrite with fstat error test passed\n");
}

//========Pread tests========

void test_compression_pread_success() {
  printf("Testing compression_pread success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;

  off_t original_size = strlen(test_data);

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  size_t nbyte = 100;
  assert(original_size > nbyte);

  int fd = compression_open("test.txt", O_RDONLY, 0, compression_layer);

  char buffer[nbyte];
  ssize_t result = compression_pread(fd, buffer, nbyte, 0, compression_layer);
  assert(result == nbyte);

  // It is called pread and getsize of the next layer 2 times because we do not
  // have the original size mapped yet. So compression_get_file_size makes those
  // additional calls.
  // 2 = 1 (from compression_pread) + 1 (from compression_get_file_size)
  assert(mock_state.pread_called == 2);
  assert(mock_state.fstat_called == 2);
  assert(mock_state.pwrite_called == 0);

  assert(strncmp(buffer, test_data, nbyte) == 0);

  result = compression_pread(fd, buffer, nbyte, 0, compression_layer);
  assert(result == nbyte);
  // second read should only call pread and getsize to the next layer once
  // because in the first pread we map the original size to the file size
  // mapping. 3 = 2 (first pread) + 1 (second pread)
  assert(mock_state.pread_called == 3);
  assert(mock_state.fstat_called == 3);

  assert(strncmp(buffer, test_data, nbyte) == 0);

  free(compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_pread success test passed\n");
}

void test_compression_pread_does_not_read_past_eof() {
  printf("Testing compression_pread does not read past eof...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  size_t nbyte = original_size + 1;

  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  char buffer[nbyte];

  ssize_t result = compression_pread(fd, buffer, nbyte, 0, compression_layer);
  assert(result == original_size);
  // This time we had the original size mapped, so compression_get_file_size
  // does not make the additional calls. See test_compression_pread_success for
  // the opposite example.
  assert(mock_state.pread_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(strncmp(buffer, test_data, original_size) == 0);

  free(compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_pread does not read past eof test passed\n");
}

void test_compression_pread_allows_partial_read() {
  printf("Testing compression_pread allows partial read...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  size_t nbyte = 5;
  assert(original_size > nbyte);

  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  char buffer[nbyte];
  ssize_t result = compression_pread(
      fd, buffer, nbyte, (off_t)(original_size - 2 * nbyte), compression_layer);
  assert(result == nbyte);
  // This time we had the original size mapped, so compression_get_file_size
  // does not make the additional calls. See test_compression_pread_success for
  // the opposite example.
  assert(mock_state.pread_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(strncmp(buffer, test_data + original_size - 2 * nbyte, nbyte) == 0);

  free(compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_pread allows partial read test passed\n");
}

void test_compression_pread_returns_0_with_nbyte_0() {
  printf("Testing compression_pread with nbyte 0...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";
  char buffer[100];

  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  off_t offset = (off_t)(original_size + 1);
  ssize_t result = compression_pread(fd, buffer, 0, offset, compression_layer);
  assert(result == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_pread with nbyte 0 test passed\n");
}

void test_compression_pread_returns_0_when_offset_is_past_eof() {
  printf("Testing compression_pread with offset past eof...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  off_t original_size = strlen(test_data);
  off_t offset = (off_t)(original_size + 1);
  char buffer[100];

  int fd = compression_open("test.txt", O_RDONLY, 0, compression_layer);

  ssize_t result =
      compression_pread(fd, buffer, 100, offset, compression_layer);
  assert(result == 0);

  compression_destroy(compression_layer);
  printf("✅ compression_pread with offset past eof test passed\n");
}

void test_compression_pread_fails_with_invalid_fd() {
  printf("Testing compression_pread with invalid fd...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  char buffer[100];
  ssize_t result = compression_pread(-1, buffer, 100, 0, compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pread with invalid fd test passed\n");
}

void test_compression_pread_fails_with_invalid_offset() {
  printf("Testing compression_pread with invalid offset...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t offset = 100;
  off_t original_size = offset + 1;
  char *path = "test.txt";

  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);
  char buffer[100];
  ssize_t result = compression_pread(7, buffer, 100, -1, compression_layer);
  assert(result == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_pread with invalid offset test passed\n");
}

//========Ftruncate tests========

void test_compression_ftruncate_complete_file_success() {
  printf("Testing compression_ftruncate success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";
  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int res = compression_ftruncate(fd, 0, compression_layer);
  assert(res == 0);
  assert(mock_state.ftruncate_called == 1);

  // We don't call getsize because we have the original size mapped
  assert(mock_state.fstat_called == 0);

  // We don't call pwrite or pread because we are truncating the file to 0 bytes
  assert(mock_state.pwrite_called == 0);
  assert(mock_state.pread_called == 0);

  // Check logical size updated
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == 0);
  }

  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate complete file success test passed\n");
}

void test_compression_ftruncate_same_size_success() {
  printf("Testing compression_ftruncate same size success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";
  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int res = compression_ftruncate(fd, (off_t)original_size, compression_layer);
  assert(res == 0);
  // We don't call ftruncate because the new size is the same as the original
  // size
  assert(mock_state.ftruncate_called == 0);
  assert(mock_state.fstat_called == 0);
  assert(mock_state.pwrite_called == 0);
  assert(mock_state.pread_called == 0);

  // Check that the file size was not updated in the file_size_mapping
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == original_size);
  }

  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate same size success test passed\n");
}

void test_compression_ftruncate_smaller_size_success() {
  printf("Testing compression_ftruncate smaller size success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  size_t len = strlen(test_data);
  char *expected_data = malloc(len);
  if (!expected_data) {
    printf("Failed to allocate memory for expected data\n");
    exit(1);
  }
  memcpy(expected_data, test_data, len - 1);

  // We try to anticipate what will be the compressed data
  CompressedData expected_compressed_data = mock_compressed_data(
      &state->compressor, expected_data, original_size - 1);
  free(expected_data);

  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int res =
      compression_ftruncate(fd, (off_t)(original_size - 1), compression_layer);
  assert(res == 0);
  assert(mock_state.ftruncate_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.pread_called == 1);
  assert(mock_state.pwrite_input_nbyte == expected_compressed_data.size);

  // Check logical size updated
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == original_size - 1);
  }

  free(compressed_data.data);
  free(expected_compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate smaller size success test passed\n");
}

void test_compression_ftruncate_empty_file_success() {
  printf("Testing compression_ftruncate empty file success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = 0;
  char *path = "test.txt";
  const off_t length = 1000;
  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  // This should be the uncompressed data that the function will create
  void *expected_data = calloc(1, length);
  if (!expected_data) {
    printf("Failed to allocate memory for decompressed data\n");
    exit(1);
  }
  // We try to anticipate what will be the compressed data
  CompressedData expected_compressed_data =
      mock_compressed_data(&state->compressor, expected_data, length);
  free(expected_data);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int res = compression_ftruncate(fd, length, compression_layer);
  assert(res == 0);
  assert(mock_state.ftruncate_called == 1);
  assert(mock_state.fstat_called == 0);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.pread_called == 0);
  assert(mock_state.pwrite_input_nbyte == expected_compressed_data.size);

  // Check logical size updated
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == length);
  }

  free(expected_compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate empty file success test passed\n");
}

void test_compression_ftruncate_larger_size_success() {
  printf("Testing compression_ftruncate larger size success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;

  int64_t original_size = strlen(test_data);
  char *path = "test.txt";

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  size_t len = strlen(test_data);
  char *expected_data = calloc(len + 2, 1);
  if (!expected_data) {
    printf("Failed to allocate memory for expected data\n");
    exit(1);
  }
  memcpy(expected_data, test_data, len + 1);

  // We try to anticipate what will be the compressed data
  CompressedData expected_compressed_data = mock_compressed_data(
      &state->compressor, expected_data, original_size + 1);
  free(expected_data);

  // Add logical size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int res = compression_ftruncate(fd, original_size + 1, compression_layer);
  assert(res == 0);
  assert(mock_state.ftruncate_called == 1);
  assert(mock_state.fstat_called == 1);
  assert(mock_state.pwrite_called == 1);
  assert(mock_state.pread_called == 1);
  assert(mock_state.pwrite_input_nbyte == expected_compressed_data.size);

  // Check logical size updated
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == original_size + 1);
  }

  free(compressed_data.data);
  free(expected_compressed_data.data);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate larger size success test passed\n");
}

void test_compression_ftruncate_error_when_invalid_fd() {
  printf("Testing compression_ftruncate with invalid fd...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  int res = compression_ftruncate(-1, 0, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate with invalid fd test passed\n");
}

void test_compression_ftruncate_error_when_fd_not_mapped() {
  printf("Testing compression_ftruncate with fd not mapped...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  int res = compression_ftruncate(7, 0, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate with fd not mapped test passed\n");
}

void test_compression_ftruncate_error_when_lock_acquire_fails() {
  printf("Testing compression_ftruncate with lock acquire fails...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  // We simulate the lock being held by another process
  int lock_result = locking_acquire_write(state->lock_table, path);
  assert(lock_result == 0);

  int res = compression_ftruncate(fd, 0, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate with lock acquire fails test passed\n");
}

void test_compression_ftruncate_fails_when_we_cannot_get_original_size() {
  printf("Testing compression_ftruncate fails when we cannot get original "
         "size...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  char *path = "test.txt";

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = -1;
  mock_state.stat_lower_layer_stat.st_size = 100;
  int res = compression_ftruncate(fd, 0, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_ftruncate fails when we cannot get original size test "
         "passed\n");
}

//========Truncate tests========

void test_compression_truncate_success() {
  printf("Testing compression_truncate success...\n");
  setup_mock_layer();

  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->ltruncate != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";
  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);

  int res = compression_layer.ops->ltruncate(path, 0, compression_layer);
  assert(res == 0);
  assert(mock_state.ftruncate_called == 1);
  // We don't call truncate because we are using shared logic with ftruncate
  assert(mock_state.truncate_called == 0);
  assert(mock_state.open_called == 1);

  // We don't call getsize because we have the original size mapped
  assert(mock_state.fstat_called == 0);

  // We don't call pwrite or pread because we are truncating the file to 0 bytes
  assert(mock_state.pwrite_called == 0);
  assert(mock_state.pread_called == 0);

  // Check if the file size was updated in the file_size_mapping
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == 0);
  }

  compression_destroy(compression_layer);

  printf("✅ compression_truncate success test passed\n");
}

void test_compression_truncate_error_when_open_fails() {
  printf("Testing compression_truncate with open fails...\n");
  setup_mock_layer();

  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->ltruncate != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  off_t original_size = strlen(test_data);
  char *path = "test.txt";
  // Add manually the file size mapping
  add_manually_size_to_mapping(path, original_size, state);
  mock_state.open_return_value = -1;

  int res = compression_layer.ops->ltruncate(path, 0, compression_layer);
  assert(res == -1);

  compression_destroy(compression_layer);
  printf("✅ compression_truncate with open fails test passed\n");
}

/// The rest of the logic is shared with ftruncate
// We don't need to test it again

//========Fstat tests========

void test_compression_fstat_success() {
  printf("Testing compression_fstat success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->lfstat != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns a size that is half of the
  // original (e.g compressed size)
  mock_state.stat_lower_layer_stat.st_size = original_size / 2;
  mock_state.stat_lower_layer_stat.st_mode = S_IFREG;

  int fd = compression_layer.ops->lopen(path, O_RDONLY, 0, compression_layer);
  struct stat stbuf;
  int res = compression_layer.ops->lfstat(fd, &stbuf, compression_layer);
  assert(res == 0);
  assert(stbuf.st_size == original_size);
  assert(mock_state.fstat_called == 1);

  // We test lock was released by calling a function that acquires a lock
  int lock_result = locking_acquire_write(state->lock_table, path);
  assert(lock_result == 0);
  locking_release(state->lock_table, path);

  compression_destroy(compression_layer);

  printf("✅ compression_fstat success test passed\n");
}

void test_compression_fstat_error_when_invalid_fd() {
  printf("Testing compression_fstat with invalid fd...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  int res = compression_layer.ops->lfstat(-1, NULL, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_fstat with invalid fd test passed\n");
}

void test_compression_fstat_error_when_fd_not_mapped() {
  printf("Testing compression_fstat with fd not mapped...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  int res = compression_layer.ops->lfstat(7, NULL, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_fstat with fd not mapped test passed\n");
}

void test_compression_fstat_error_when_lock_acquire_fails() {
  printf("Testing compression_fstat with lock acquire fails...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";

  int fd = compression_open(path, O_RDONLY, 0, compression_layer);

  int lock_result = locking_acquire_write(state->lock_table, path);
  assert(lock_result == 0); // Should succeed

  int res = compression_layer.ops->lfstat(fd, NULL, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_fstat with lock acquire fails test passed\n");
}

void test_compression_fstat_error_when_underlying_fstat_fails() {
  printf("Testing compression_fstat with underlaying fstat fails...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns a size that is half of the
  // original (e.g compressed size)
  mock_state.fstat_return_value = -1;

  int fd = compression_layer.ops->lopen(path, O_RDONLY, 0, compression_layer);
  struct stat stbuf;
  int res = compression_layer.ops->lfstat(fd, &stbuf, compression_layer);
  assert(res == INVALID_FD);
  compression_destroy(compression_layer);
  printf("✅ compression_fstat with underlaying fstat fails test passed\n");
}

void test_compression_fstat_no_change_when_file_is_not_regular() {
  printf("Testing compression_fstat no change when file is not regular...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->lfstat != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns a size that is half of the
  // original (e.g compressed size)
  mock_state.stat_lower_layer_stat.st_size = original_size / 2;
  mock_state.stat_lower_layer_stat.st_mode = S_IFDIR;

  int fd = compression_layer.ops->lopen(path, O_RDONLY, 0, compression_layer);
  struct stat stbuf;
  int res = compression_layer.ops->lfstat(fd, &stbuf, compression_layer);
  assert(res == 0);
  assert(stbuf.st_size == original_size);
  assert(mock_state.fstat_called == 1);

  compression_destroy(compression_layer);
  printf(
      "✅ compression_fstat no change when file is not regular test passed\n");
}

//========Lstat tests========

void test_compression_lstat_success() {
  printf("Testing compression_lstat success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->llstat != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns a size that is half of the
  // original (e.g compressed size)
  mock_state.stat_lower_layer_stat.st_size = original_size / 2;
  mock_state.stat_lower_layer_stat.st_mode = S_IFREG;

  struct stat stbuf;
  int res = compression_layer.ops->llstat(path, &stbuf, compression_layer);
  assert(res == 0);
  assert(stbuf.st_size == original_size);
  assert(mock_state.lstat_called == 1);
  compression_destroy(compression_layer);
  printf("✅ compression_lstat success test passed\n");
}

void test_compression_lstat_error_when_lock_acquire_fails() {
  printf("Testing compression_lstat with lock acquire fails...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns even though we should not be able
  // to get to this step in the compression_lstat
  mock_state.stat_lower_layer_stat.st_size = original_size;
  mock_state.stat_lower_layer_stat.st_mode = S_IFREG;

  int lock_result = locking_acquire_write(state->lock_table, path);
  assert(lock_result == 0);

  struct stat stbuf;
  int res = compression_layer.ops->llstat(path, &stbuf, compression_layer);
  assert(res == INVALID_FD);
  locking_release(state->lock_table, path);
  compression_destroy(compression_layer);
  printf("✅ compression_lstat with lock acquire fails test passed\n");
}

void test_compression_lstat_error_when_lower_layer_returns_non_regular_file() {
  printf("Testing compression_lstat with lower layer returns non regular "
         "file...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);

  // We simulate that the lower layer returns the path as a symlink
  mock_state.stat_lower_layer_stat.st_size = original_size;
  mock_state.stat_lower_layer_stat.st_mode = S_IFLNK;

  struct stat stbuf;
  int res = compression_layer.ops->llstat(path, &stbuf, compression_layer);
  assert(res == INVALID_FD);

  // We simulate that the lower layer returns a directory
  mock_state.stat_lower_layer_stat.st_size = original_size;
  mock_state.stat_lower_layer_stat.st_mode = S_IFDIR;
  res = compression_layer.ops->llstat(path, &stbuf, compression_layer);
  assert(res == INVALID_FD);

  compression_destroy(compression_layer);
  printf("✅ compression_lstat with lower layer returns non regular file test "
         "passed\n");
}

void test_compression_lstat_success_even_when_file_is_not_mapped() {
  printf("Testing compression_lstat success even when file is not mapped...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);

  // We simulate that the lower layer returns even though we should not be able
  // to get to this step in the compression_lstat
  mock_state.stat_lower_layer_stat.st_size = original_size;
  mock_state.stat_lower_layer_stat.st_mode = S_IFREG;

  // Mock compressed data
  CompressedData compressed_data =
      mock_compressed_data(&state->compressor, test_data, original_size);
  mock_state.mock_pread_data = compressed_data.data;
  mock_state.mock_pread_data_size = compressed_data.size;

  // Set up fstat to return the compressed size
  mock_state.fstat_return_value = 0;
  mock_state.stat_lower_layer_stat.st_size = (off_t)compressed_data.size;

  struct stat stbuf;
  int res = compression_layer.ops->llstat(path, &stbuf, compression_layer);
  assert(res == 0);
  assert(stbuf.st_size == original_size);
  assert(mock_state.lstat_called == 1);
  // We should have called the lower layer to get the original size
  // and to read the compressed data to map the original size
  assert(mock_state.pread_called == 1);
  assert(mock_state.fstat_called == 1);
  compression_destroy(compression_layer);
  printf("✅ compression_lstat success even when file is not mapped test "
         "passed\n");
}

//========Unlink tests========

void test_compression_unlink_success() {
  printf("Testing compression_unlink success...\n");
  setup_mock_layer();
  LayerContext compression_layer = create_default_compression_layer();
  assert(compression_layer.ops->lunlink != NULL);
  CompressionState *state =
      (CompressionState *)compression_layer.internal_state;
  char *path = "test.txt";
  off_t original_size = strlen(test_data);
  add_manually_size_to_mapping(path, original_size, state);
  // Check if the file size was updated in the file_size_mapping
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) == 0);
    assert(logical_size == original_size);
  }

  int res = compression_layer.ops->lunlink(path, compression_layer);
  assert(res == 0);
  assert(mock_state.unlink_called == 1);

  // Check mapping removed
  {
    LayerContext ctx = {0};
    ctx.internal_state = state;
    off_t logical_size = -1;
    assert(get_logical_eof_from_mapping(test_dev, test_ino, ctx,
                                        &logical_size) != 0);
  }

  compression_destroy(compression_layer);
  printf("✅ compression_unlink success test passed\n");
}

int main() {
  printf("Starting compression layer unit tests...\n\n");

  test_compression_init_lz4();
  test_compression_init_zstd();
  test_compression_init_returns_error_when_compressor_init_fails();

  test_compression_destroy_success();

  test_compression_open_null_path();
  test_compression_open_lower_layer_fails();
  test_compression_open_fd_exceeds_max();
  test_compression_open_success();
  test_compression_open_reuse_fd();
  test_compression_open_with_trunc_flag();
  test_compression_open_with_create_flag();

  test_compression_close_invalid_fd();
  test_compression_close_lower_layer_fails();
  test_compression_close_with_mapping();
  test_compression_close_multiple_files();
  test_compression_close_with_unlinked_file();

  // test_compression_pwrite_new_file_success();
  // test_compression_pwrite_to_eof_existing_file_success();
  // test_compression_pwrite_to_existing_file_success();
  // test_compression_pwrite_fails_with_invalid_fd();
  // test_compression_pwrite_fails_with_negative_offset();
  // test_compression_pwrite_fails_with_no_open();
  // test_compression_pwrite_fails_with_get_file_sizeerror();
  // test_compression_pwrite_returns_0_with_nbyte_0();

  // test_compression_pread_success();
  // test_compression_pread_does_not_read_past_eof();
  // test_compression_pread_allows_partial_read();
  // test_compression_pread_returns_0_with_nbyte_0();
  // test_compression_pread_returns_0_when_offset_is_past_eof();
  // test_compression_pread_fails_with_invalid_fd();
  // test_compression_pread_fails_with_invalid_offset();

  // test_compression_ftruncate_complete_file_sutccess();
  // test_compression_ftruncate_same_size_success();
  // test_compression_ftruncate_smaller_size_success();
  // test_compression_ftruncate_larger_size_success();
  // test_compression_ftruncate_empty_file_success();
  // test_compression_ftruncate_error_when_invalid_fd();
  // test_compression_ftruncate_error_when_fd_not_mapped();
  // test_compression_ftruncate_error_when_lock_acquire_fails();
  // test_compression_ftruncate_fails_when_we_cannot_get_original_size();

  // test_compression_truncate_success();
  // test_compression_truncate_error_when_open_fails();

  // test_compression_fstat_success();
  // test_compression_fstat_error_when_invalid_fd();
  // test_compression_fstat_error_when_fd_not_mapped();
  // test_compression_fstat_error_when_lock_acquire_fails();
  // test_compression_fstat_error_when_underlying_fstat_fails();

  // test_compression_lstat_success();
  // test_compression_lstat_error_when_lock_acquire_fails();
  // test_compression_lstat_success_even_when_file_is_not_mapped();

  test_compression_unlink_success();

  printf("\nAll compression layer tests passed!\n");
  return 0;
}
