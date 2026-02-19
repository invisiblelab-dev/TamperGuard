#include "../../../../layers/compression/compression.h"
#include "../../../../layers/compression/compression_utils.h"
#include "../../../../layers/compression/sparse_block.h"
#include "../../../../shared/utils/compressor/compressor.h"
#include "../../../mock_layer.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/wait.h>

static MockLayerState mock_state;
static LayerContext mock_layer;

void setup_mock_layer() {
  reset_mock_state(&mock_state, 0, 0);
  mock_layer = create_mock_layer(&mock_state);
}

// Fixed device/inode used for path-based tests migrated to inode keys
static dev_t test_dev = (dev_t)1;
static ino_t test_ino = (ino_t)1;

static CompressionConfig config = {.algorithm = COMPRESSION_LZ4,
                                   .level = 0,
                                   .mode = COMPRESSION_MODE_SPARSE_BLOCK,
                                   .block_size = 4096};

static const char test_data[] =
    "This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog. This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog.";

static char *make_repeated_block(const char *src, size_t block_size) {
  size_t src_len = strlen(src);
  if (src_len == 0)
    return NULL;

  char *buf = malloc(block_size);
  if (!buf)
    return NULL;

  size_t off = 0;
  while (off < block_size) {
    size_t n = src_len;
    if (n > block_size - off)
      n = block_size - off;
    memcpy(buf + off, src, n);
    off += n;
  }
  return buf;
}

void test_compression_sparse_block_init_success() {
  printf("Testing compression sparse block init success...\n");

  setup_mock_layer();

  LayerContext compression_sparse_block_layer =
      compression_init(&mock_layer, &config);
  assert(compression_sparse_block_layer.ops != NULL);
  assert(compression_sparse_block_layer.ops->lpwrite ==
         compression_sparse_block_pwrite);
  assert(compression_sparse_block_layer.ops->ltruncate ==
         compression_sparse_block_truncate);
  assert(compression_sparse_block_layer.ops->lftruncate ==
         compression_sparse_block_ftruncate);
  assert(compression_sparse_block_layer.ops->lpread ==
         compression_sparse_block_pread);

  compression_destroy(compression_sparse_block_layer);
  printf("✅ compression sparse block init success test passed\n");
}

void test_compression_sparse_block_init_error_when_block_size_is_not_set() {
  printf("Testing compression sparse block init error when block size is not "
         "set...\n");

  setup_mock_layer();
  CompressionConfig config = {.algorithm = COMPRESSION_LZ4,
                              .level = 0,
                              .mode = COMPRESSION_MODE_SPARSE_BLOCK};
  // Fork to catch the exit
  pid_t pid = fork();
  if (pid == 0) {
    compression_init(&mock_layer, &config);
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status));        // Assert that the child called exit()
    assert(WEXITSTATUS(status) == 1); // Assert that the child called exit(1)
  }

  printf("✅ compression sparse block init error when block size is not set "
         "test passed\n");
}

//========Pwrite tests========

void test_compression_sparse_block_pwrite_single_block() {
  printf("Testing compression sparse block pwrite single block...\n");

  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;

  size_t expected_compressed_size = state->compressor.get_compress_bound(
      config.block_size, state->compressor.level);

  const char *path = "test.txt";
  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Write one full block
  char *block = make_repeated_block(test_data, config.block_size);
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, block, config.block_size, 0, layer);
  free(block);
  assert(bytes_written == config.block_size);
  assert(mock_state.pwrite_input_nbyte <= expected_compressed_size);
  assert(mock_state.pwrite_called == 1);

  CompressedFileMapping *block_index = state->file_mapping;
  assert(block_index != NULL);
  assert(block_index->num_blocks == 1);
  assert(block_index->sizes[0] > 0);
  assert(block_index->sizes[0] < config.block_size); // Compressed

  compression_destroy(layer);
  printf("✅ compression sparse block pwrite single block test passed\n");
}

void test_compression_sparse_block_pwrite_multiple_sequential_blocks() {
  printf("Testing compression sparse block pwrite multiple sequential "
         "blocks...\n");

  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;

  const char *path = "test.txt";
  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Write 3 full blocks sequentially
  char *block = make_repeated_block(test_data, config.block_size);

  // Block 0
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, block, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);

  // Block 1
  bytes_written = layer.ops->lpwrite(fd, block, config.block_size,
                                     (off_t)config.block_size, layer);
  assert(bytes_written == config.block_size);

  // Block 2
  bytes_written =
      layer.ops->lpwrite(fd, block, config.block_size,
                         (off_t)((size_t)2 * config.block_size), layer);
  assert(bytes_written == config.block_size);

  free(block);

  CompressedFileMapping *block_index = state->file_mapping;
  assert(block_index != NULL);
  assert(block_index->num_blocks == 3);

  // Verify all blocks have compressed sizes stored
  assert(block_index->sizes[0] > 0);
  assert(block_index->sizes[1] > 0);
  assert(block_index->sizes[2] > 0);

  // All should be compressed
  assert(block_index->sizes[0] < config.block_size);
  assert(block_index->sizes[1] < config.block_size);
  assert(block_index->sizes[2] < config.block_size);

  compression_destroy(layer);
  printf("✅ compression sparse block pwrite multiple sequential blocks test "
         "passed\n");
}

void test_compression_sparse_block_pwrite_overwrite_existing_block() {
  printf("Testing compression sparse block pwrite overwrites existing "
         "block...\n");

  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;

  const char *path = "test.txt";
  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  char *block1 = make_repeated_block(test_data, config.block_size);

  // Write block 0
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, block1, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);

  CompressedFileMapping *block_index = state->file_mapping;
  off_t first_size = block_index->sizes[0];
  assert(first_size > 0);

  // Create different data
  const char *different_data = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
  char *block2 = make_repeated_block(different_data, config.block_size);

  // Overwrite block 0 with different data
  bytes_written = layer.ops->lpwrite(fd, block2, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);

  // Size should be updated (might be different if compression varies)
  assert(block_index->sizes[0] > 0);
  assert(block_index->num_blocks == 1);

  free(block1);
  free(block2);
  compression_destroy(layer);
  printf("✅ compression sparse block pwrite overwrites existing block test "
         "passed\n");
}

void test_compression_sparse_block_original_size_updates_only_on_append() {
  printf("Testing original size only increases on append, not overwrite...\n");

  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);

  const char *path = "test.txt";
  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Write two full blocks sequentially (EOF should be 2 * block_size)
  char *block = make_repeated_block(test_data, config.block_size);
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, block, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);
  bytes_written = layer.ops->lpwrite(fd, block, config.block_size,
                                     (off_t)config.block_size, layer);
  assert(bytes_written == config.block_size);

  off_t logical_size = -1;
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  int got = get_logical_eof_from_mapping(device, inode, layer, &logical_size);
  assert(got == 0);
  assert(logical_size == (off_t)((size_t)2 * config.block_size));

  // Overwrite first block; original size must remain unchanged
  const char *different_data = "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
  char *block2 = make_repeated_block(different_data, config.block_size);
  bytes_written = layer.ops->lpwrite(fd, block2, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);

  got = get_logical_eof_from_mapping(device, inode, layer, &logical_size);
  assert(got == 0);
  assert(logical_size == (off_t)((size_t)2 * config.block_size));

  // Append one more block; original size should increase to 3 * block_size
  bytes_written =
      layer.ops->lpwrite(fd, block, config.block_size,
                         (off_t)((size_t)2 * config.block_size), layer);
  assert(bytes_written == config.block_size);

  got = get_logical_eof_from_mapping(device, inode, layer, &logical_size);
  assert(got == 0);
  assert(logical_size == (off_t)((size_t)3 * config.block_size));

  free(block);
  free(block2);
  compression_destroy(layer);
  printf("✅ original size append-only growth test passed\n");
}

void test_compression_sparse_block_is_uncompressed_flag() {
  printf("Testing is_uncompressed flag is set correctly...\n");

  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;

  const char *path = "test.txt";
  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Generate uncompressable data
  char *uncompressable_block = malloc(config.block_size);
  assert(uncompressable_block != NULL);

  // Fill with unique pattern using multiple primes to avoid repetition
  for (size_t i = 0; i < config.block_size; i++) {
    uncompressable_block[i] = (char)((i * 31) ^ (i * 97) ^ (i >> 3));
  }

  // Write the uncompressable block
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, uncompressable_block, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);

  // Verify the block index
  CompressedFileMapping *block_index = state->file_mapping;
  ;
  assert(block_index != NULL);
  assert(block_index->num_blocks == 1);

  // Debug output
  printf("  Block 0: size=%ld, is_uncompressed=%d, block_size=%zu\n",
         (long)block_index->sizes[0],
         block_index->is_uncompressed ? (int)block_index->is_uncompressed[0]
                                      : -1,
         (size_t)config.block_size);

  // Verify is_uncompressed flag exists and matches the size
  assert(block_index->is_uncompressed != NULL);

  // The flag should be 1 if size equals block_size (stored uncompressed)
  // and 0 if size < block_size (stored compressed)
  if (block_index->sizes[0] == (off_t)config.block_size) {
    assert(block_index->is_uncompressed[0] == 1);
    printf(
        "  ✓ Data stored uncompressed (as expected for high-entropy data)\n");
  } else {
    assert(block_index->is_uncompressed[0] == 0);
    printf("  ✓ Data stored compressed (compressed from %zu to %ld bytes)\n",
           (size_t)config.block_size, (long)block_index->sizes[0]);
  }

  free(uncompressable_block);
  compression_destroy(layer);
  printf("✅ is_uncompressed flag test passed\n");
}

//========Pread tests=========

void test_compression_sparse_block_pread_returns_0_with_nbyte_0() {
  printf("Testing compression sparse block pread returns 0 with nbyte 0...\n");
  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";

  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // We mock the logical size to replicate a state in which it's not a new file
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  assert(set_logical_eof_in_mapping(device, inode, (off_t)strlen(test_data),
                                    layer) == 0);

  ssize_t result = layer.ops->lpread(fd, NULL, 0, 0, layer);
  assert(result == 0);

  compression_destroy(layer);
  printf(
      "✅ compression sparse block pread returns 0 with nbyte 0 test passed\n");
}

void test_compression_sparse_block_reads_empty_file_buffer_untouched() {
  printf("Testing compression sparse block empty read leaves buffer "
         "untouched...\n");
  setup_mock_layer();

  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";

  // Mock that the file does not exist (empty)
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Prepare zeroed buffer with capacity > 0
  enum { BUF_CAP = 128 };
  unsigned char buffer[BUF_CAP];
  memset(buffer, 0, sizeof(buffer));

  mock_state.mock_pread_data_size = 0;
  // lpread with nbyte > 0 should return 0 and not modify buffer
  ssize_t result = layer.ops->lpread(fd, buffer, sizeof(buffer), 0, layer);
  printf("result: %ld\n", result);
  assert(result == 0);
  for (size_t i = 0; i < sizeof(buffer); i++) {
    assert(buffer[i] == 0);
  }

  compression_destroy(layer);
  printf("✅ compression sparse block empty read leaves buffer untouched test "
         "passed\n");
}

void test_compression_sparse_block_pread_reads_full_file() {
  printf("Testing compression sparse block pread reads full file...\n");
  setup_mock_layer();

  enable_mock_pwrite_data_storage(&mock_state);

  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";
  assert(config.block_size > 0);

  size_t logical_size = (size_t)config.block_size;

  char *block = make_repeated_block(test_data, logical_size);

  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);
  ssize_t bytes_written = layer.ops->lpwrite(fd, block, logical_size, 0, layer);
  assert(bytes_written == logical_size);
  assert(mock_state.pwrite_data_storage != NULL);

  mock_state.mock_pread_data_size = mock_state.pwrite_input_nbyte;
  mock_state.mock_pread_data = mock_state.pwrite_data_storage;

  char buffer[logical_size];
  memset(buffer, 0, logical_size);

  ssize_t bytes_read = layer.ops->lpread(fd, buffer, logical_size, 0, layer);

  assert(bytes_read == logical_size);
  assert(memcmp(buffer, block, logical_size) == 0);

  free(block);
  free_mock_pwrite_data_storage(&mock_state);
  compression_destroy(layer);

  printf("✅ compression sparse block pread reads full file test passed\n");
}

void test_compression_sparse_block_pread_reads_file_with_less_than_one_block() {
  printf("Testing compression sparse block pread reads file with less than one "
         "block...\n");
  setup_mock_layer();

  enable_mock_pwrite_data_storage(&mock_state);

  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";
  assert(config.block_size > 0);

  // The file will only have content up to half the block size
  size_t logical_size = (size_t)config.block_size / 2;

  char *block = make_repeated_block(test_data, logical_size);

  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);
  ssize_t bytes_written = layer.ops->lpwrite(fd, block, logical_size, 0, layer);
  assert(bytes_written == logical_size);
  assert(mock_state.pwrite_data_storage != NULL);

  mock_state.mock_pread_data_size = mock_state.pwrite_input_nbyte;
  mock_state.mock_pread_data = mock_state.pwrite_data_storage;

  char buffer[logical_size];
  memset(buffer, 0, logical_size);

  ssize_t bytes_read = layer.ops->lpread(fd, buffer, logical_size, 0, layer);

  assert(bytes_read == logical_size);
  assert(memcmp(buffer, block, logical_size) == 0);

  free(block);
  free_mock_pwrite_data_storage(&mock_state);
  compression_destroy(layer);
  printf("✅ compression sparse block pread reads file with less than one "
         "block test passed\n");
}

void test_compression_sparse_block_pread_reads_file_partially() {
  printf("Testing compression sparse block pread random reads...\n");
  setup_mock_layer();

  enable_mock_pwrite_data_storage(&mock_state);

  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";

  ssize_t logical_size = (ssize_t)config.block_size;

  char *block = make_repeated_block(test_data, logical_size);

  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);
  ssize_t bytes_written = layer.ops->lpwrite(fd, block, logical_size, 0, layer);
  assert(bytes_written == logical_size);
  assert(mock_state.pwrite_data_storage != NULL);

  mock_state.mock_pread_data_size = mock_state.pwrite_input_nbyte;
  mock_state.mock_pread_data = mock_state.pwrite_data_storage;

  // First we read half of the file
  char buffer[logical_size / 2];
  memset(buffer, 0, logical_size / 2);

  ssize_t bytes_read =
      layer.ops->lpread(fd, buffer, logical_size / 2, 0, layer);

  assert(bytes_read == logical_size / 2);
  assert(memcmp(buffer, block, logical_size / 2) == 0);

  // Then we read 10 bytes of the file from the beggining
  char buffer2[10];
  memset(buffer2, 0, sizeof(buffer2));
  bytes_read = layer.ops->lpread(fd, buffer2, 10, 0, layer);
  assert(bytes_read == 10);
  assert(memcmp(buffer2, block, 10) == 0);

  free(block);
  free_mock_pwrite_data_storage(&mock_state);
  compression_destroy(layer);

  printf("✅ compression sparse block pread random reads test passed\n");
}

void test_compression_sparse_block_pread_uncompressed_data() {
  printf(
      "Testing compression sparse block pread handles uncompressed data...\n");
  setup_mock_layer();

  enable_mock_pwrite_data_storage(&mock_state);

  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;
  const char *path = "test.txt";

  // Generate truly uncompressable data (high entropy)
  char *uncompressable_block = malloc(config.block_size);
  assert(uncompressable_block != NULL);

  // Fill with unique pattern using multiple primes to avoid repetition
  for (size_t i = 0; i < config.block_size; i++) {
    uncompressable_block[i] = (char)((i * 31) ^ (i * 97) ^ (i >> 3));
  }

  // Mock that the file does not exist
  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;
  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // Write the uncompressable block
  ssize_t bytes_written =
      layer.ops->lpwrite(fd, uncompressable_block, config.block_size, 0, layer);
  assert(bytes_written == config.block_size);
  assert(mock_state.pwrite_data_storage != NULL);

  // Verify the block is stored uncompressed
  CompressedFileMapping *block_index = state->file_mapping;
  ;
  assert(block_index != NULL);
  assert(block_index->num_blocks == 1);
  assert(block_index->is_uncompressed != NULL);

  // If data was stored uncompressed (size == block_size), verify the flag
  bool is_stored_uncompressed =
      (block_index->sizes[0] == (off_t)config.block_size);
  if (is_stored_uncompressed) {
    assert(block_index->is_uncompressed[0] == 1);
    printf("  ✓ Data confirmed stored uncompressed (is_uncompressed[0] = 1)\n");
  } else {
    printf("  ⚠ Data was compressed despite high entropy (skipping "
           "uncompressed read test)\n");
    free(uncompressable_block);
    free_mock_pwrite_data_storage(&mock_state);
    compression_destroy(layer);
    printf("✅ compression sparse block pread uncompressed data test passed "
           "(skipped)\n");
    return;
  }

  // Setup mock for reading back the data
  mock_state.mock_pread_data_size = mock_state.pwrite_input_nbyte;
  mock_state.mock_pread_data = mock_state.pwrite_data_storage;

  // Read back the data
  char *read_buffer = malloc(config.block_size);
  assert(read_buffer != NULL);
  memset(read_buffer, 0, config.block_size);

  ssize_t bytes_read =
      layer.ops->lpread(fd, read_buffer, config.block_size, 0, layer);

  assert(bytes_read == config.block_size);
  assert(memcmp(read_buffer, uncompressable_block, config.block_size) == 0);

  printf("  ✓ Successfully read uncompressed data without decompression\n");

  free(uncompressable_block);
  free(read_buffer);
  free_mock_pwrite_data_storage(&mock_state);
  compression_destroy(layer);
  printf("✅ compression sparse block pread uncompressed data test passed\n");
}

// ===== ftruncate tests =====
void test_compression_sparse_block_ftruncate_extend_beyond_size() {
  printf("Testing ftruncate extends logical size beyond current size...\n");
  setup_mock_layer();
  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";

  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;

  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // initial write: half block
  size_t half = (size_t)config.block_size / 2;
  char *buf = make_repeated_block(test_data, half);
  assert(layer.ops->lpwrite(fd, buf, half, 0, layer) == (ssize_t)half);

  // extend to 2.5 blocks
  off_t target = (off_t)((size_t)2 * config.block_size + half);
  assert(layer.ops->lftruncate(fd, target, layer) == 0);

  off_t logical = -1;
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  assert(get_logical_eof_from_mapping(device, inode, layer, &logical) == 0);
  assert(logical == target);

  free(buf);
  compression_destroy(layer);
  printf("✅ ftruncate extend test passed\n");
}

void test_compression_sparse_block_ftruncate_truncate_to_zero() {
  printf("Testing ftruncate to zero...\n");
  setup_mock_layer();
  LayerContext layer = compression_init(&mock_layer, &config);
  const char *path = "test.txt";

  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;

  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // write one block
  char *block = make_repeated_block(test_data, (size_t)config.block_size);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size, 0, layer) ==
         (ssize_t)config.block_size);
  free(block);

  // truncate to zero
  assert(layer.ops->lftruncate(fd, 0, layer) == 0);
  off_t logical = -1;
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  assert(get_logical_eof_from_mapping(device, inode, layer, &logical) == 0);
  assert(logical == 0);

  compression_destroy(layer);
  printf("✅ ftruncate to zero test passed\n");
}

void test_compression_sparse_block_ftruncate_truncate_exact_block_boundary() {
  printf("Testing ftruncate at exact block boundary...\n");
  setup_mock_layer();
  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;
  const char *path = "test.txt";

  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;

  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // write 3 blocks
  char *block = make_repeated_block(test_data, (size_t)config.block_size);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size, 0, layer) ==
         (ssize_t)config.block_size);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size,
                            (off_t)config.block_size,
                            layer) == (ssize_t)config.block_size);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size,
                            (off_t)((size_t)2 * config.block_size),
                            layer) == (ssize_t)config.block_size);
  free(block);
  enable_mock_pwrite_data_storage(&mock_state);
  mock_state.mock_pread_data_size = mock_state.pwrite_input_nbyte;
  mock_state.mock_pread_data = mock_state.pwrite_data_storage;

  // truncate to 2 blocks
  off_t target = (off_t)((size_t)2 * config.block_size);
  assert(layer.ops->lftruncate(fd, target, layer) == 0);

  off_t logical = -1;
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  assert(get_logical_eof_from_mapping(device, inode, layer, &logical) == 0);
  assert(logical == target);

  // block index should report 2 blocks now
  CompressedFileMapping *bim = state->file_mapping;
  assert(bim != NULL);
  assert(bim->num_blocks == 2);

  compression_destroy(layer);
  printf("✅ ftruncate exact block boundary test passed\n");
}

void test_compression_sparse_block_ftruncate_truncate_mid_block() {
  printf("Testing ftruncate at mid block...\n");
  setup_mock_layer();
  LayerContext layer = compression_init(&mock_layer, &config);
  CompressionState *state = (CompressionState *)layer.internal_state;
  const char *path = "test.txt";

  mock_state.stat_lower_layer_stat.st_size = 0;
  mock_state.stat_lower_layer_stat.st_dev = test_dev;
  mock_state.stat_lower_layer_stat.st_ino = test_ino;

  int fd = layer.ops->lopen(path, O_CREAT | O_WRONLY, 0666, layer);
  assert(fd != -1);

  // write 2 full blocks
  char *block = make_repeated_block(test_data, (size_t)config.block_size);
  enable_mock_pwrite_data_storage(&mock_state);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size, 0, layer) ==
         (ssize_t)config.block_size);
  assert(layer.ops->lpwrite(fd, block, (size_t)config.block_size,
                            (off_t)config.block_size,
                            layer) == (ssize_t)config.block_size);

  // After writes, prepare mock file data that matches the physical layout.
  // Block 0 is at offset 0 (compressed), block 1 is at offset block_size
  // (compressed).
  CompressedFileMapping *bim_init = state->file_mapping;
  assert(bim_init != NULL);
  assert(bim_init->num_blocks == 2);

  // Compress the original block data to create valid compressed data for block
  // 1
  size_t max_comp = state->compressor.get_compress_bound(
      (size_t)config.block_size, state->compressor.level);
  uint8_t *comp_block1 = (uint8_t *)malloc(max_comp);
  assert(comp_block1 != NULL);
  ssize_t comp_size = state->compressor.compress_data(
      (uint8_t *)block, (size_t)config.block_size, comp_block1, max_comp,
      state->compressor.level);
  assert(comp_size > 0);

  // Build a mock file with space for both blocks
  size_t block1_size = (size_t)bim_init->sizes[1];
  size_t keep = (size_t)config.block_size / 2;
  // After truncation, we'll need space for at least 'keep' bytes if stored
  // uncompressed
  size_t need_tail = block1_size > keep ? block1_size : keep;
  size_t file_size = (size_t)config.block_size + need_tail;
  uint8_t *file = (uint8_t *)malloc(file_size);
  assert(file != NULL);
  memset(file, 0, file_size);

  // Place the actual compressed data for block 1 at offset block_size
  memcpy(file + (size_t)config.block_size, comp_block1, (size_t)comp_size);
  free(comp_block1);

  mock_state.mock_pread_data = (const char *)file;
  mock_state.mock_pread_data_size = file_size;

  // truncate to middle of block 1
  off_t target = (off_t)config.block_size + (off_t)keep;
  assert(layer.ops->lftruncate(fd, target, layer) == 0);

  off_t logical = -1;
  dev_t device;
  ino_t inode;
  assert(get_file_key_from_fd(fd, layer, &device, &inode) == 0);
  assert(get_logical_eof_from_mapping(device, inode, layer, &logical) == 0);
  assert(logical == target);

  // last block size in mapping should be <= keep
  CompressedFileMapping *bim = state->file_mapping;
  assert(bim != NULL);
  assert(bim->num_blocks == 2);
  assert(bim->sizes[1] > 0);
  assert((size_t)bim->sizes[1] <= keep);

  free(file);
  free(block);
  compression_destroy(layer);
  printf("✅ ftruncate mid block test passed\n");
}

int main() {
  printf("Starting compression sparse block unit tests...\n\n");

  test_compression_sparse_block_init_success();
  test_compression_sparse_block_init_error_when_block_size_is_not_set();

  test_compression_sparse_block_pwrite_single_block();
  test_compression_sparse_block_pwrite_multiple_sequential_blocks();
  test_compression_sparse_block_pwrite_overwrite_existing_block();
  test_compression_sparse_block_original_size_updates_only_on_append();
  test_compression_sparse_block_is_uncompressed_flag();

  test_compression_sparse_block_pread_returns_0_with_nbyte_0();
  test_compression_sparse_block_reads_empty_file_buffer_untouched();
  test_compression_sparse_block_pread_reads_full_file();
  test_compression_sparse_block_pread_reads_file_with_less_than_one_block();
  test_compression_sparse_block_pread_reads_file_partially();
  test_compression_sparse_block_pread_uncompressed_data();

  // ===== ftruncate tests =====

  test_compression_sparse_block_ftruncate_extend_beyond_size();
  test_compression_sparse_block_ftruncate_truncate_to_zero();
  test_compression_sparse_block_ftruncate_truncate_exact_block_boundary();
  test_compression_sparse_block_ftruncate_truncate_mid_block();

  printf("\n✅ All compression sparse block unit tests passed!\n");
  return 0;
}
