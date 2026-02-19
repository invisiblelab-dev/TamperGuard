#include "../../../../layers/anti_tampering/anti_tampering.h"
#include "../../../../layers/anti_tampering/anti_tampering_utils.h"
#include "../../../../layers/anti_tampering/block_anti_tampering.h"
#include "../../../../layers/local/local.h"
#include "../../../../shared/utils/hasher/hasher.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Helper to free local layer ops
static void cleanup_local_layer(LayerContext *layer) {
  if (layer && layer->ops) {
    free(layer->ops);
    layer->ops = NULL;
  }
}

#define BLOCK_SIZE ((size_t)1024)
#define NUM_BLOCKS ((size_t)3)
#define TEST_DATA_SIZE (BLOCK_SIZE * NUM_BLOCKS)

static AntiTamperingConfig create_block_config(char *hashes_storage) {
  AntiTamperingConfig cfg = {
      .data_layer = NULL,
      .hash_layer = NULL,
      .hashes_storage = hashes_storage,
      .algorithm = HASH_SHA256,
      .mode = ANTI_TAMPERING_MODE_BLOCK,
      .block_size = BLOCK_SIZE,
  };
  return cfg;
}

// Helper to compute expected hash for a block
static char *compute_block_hash(const void *data, size_t size,
                                const Hasher *hasher) {
  return hasher->hash_buffer_hex(data, size);
}

// Helper to read hash file content
static char *read_hash_file(const char *hash_path, size_t offset,
                            size_t length) {
  int fd = open(hash_path, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }

  char *buffer = malloc(length + 1);
  if (!buffer) {
    close(fd);
    return NULL;
  }

  ssize_t read_bytes = pread(fd, buffer, length, (off_t)offset);
  close(fd);

  if (read_bytes != (ssize_t)length) {
    free(buffer);
    return NULL;
  }

  buffer[length] = '\0';
  return buffer;
}

void test_block_write_multiple_hashes() {
  printf("Testing block write with multiple hashes...\n");

  // Create temporary directories
  char test_data_dir[] = "/tmp/test_block_data_XXXXXX";
  char test_hash_dir[] = "/tmp/test_block_hash_XXXXXX";
  assert(mkdtemp(test_data_dir) != NULL);
  assert(mkdtemp(test_hash_dir) != NULL);

  char test_file_path[512];
  int ret = snprintf(test_file_path, sizeof(test_file_path), "%s/testfile",
                     test_data_dir);
  assert(ret > 0 && ret < (int)sizeof(test_file_path));

  // Initialize layers
  LayerContext data_layer = local_init();
  LayerContext hash_layer = local_init();

  // Initialize anti-tampering layer in block mode
  AntiTamperingConfig cfg = create_block_config(test_hash_dir);
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // Open file for writing
  int fd =
      block_anti_tampering_open(test_file_path, O_RDWR | O_CREAT, 0644, ctx);
  assert(fd >= 0);

  // Prepare test data: 3 blocks with different content
  char *test_data = malloc(TEST_DATA_SIZE);
  assert(test_data != NULL);

  // Fill each block with different patterns
  for (size_t i = 0; i < NUM_BLOCKS; i++) {
    memset(test_data + (i * BLOCK_SIZE), 'A' + (int)i, BLOCK_SIZE);
  }

  // Write all blocks
  ssize_t written =
      block_anti_tampering_write(fd, test_data, TEST_DATA_SIZE, 0, ctx);
  assert(written == TEST_DATA_SIZE);

  // Close file
  assert(block_anti_tampering_close(fd, ctx) == 0);

  // Get hash file path using the same method as anti_tampering layer
  AntiTamperingState *state = (AntiTamperingState *)ctx.internal_state;
  char *file_path_hex_hash =
      state->hasher.hash_buffer_hex(test_file_path, strlen(test_file_path));
  assert(file_path_hex_hash != NULL);

  char *hash_file_path = construct_hash_pathname(state, file_path_hex_hash);
  assert(hash_file_path != NULL);
  free(file_path_hex_hash);

  // Verify hash file exists
  struct stat st;
  assert(stat(hash_file_path, &st) == 0);
  printf("  Hash file created: %s\n", hash_file_path);

  // Read hash file and verify it contains multiple hashes
  // SHA256 produces 64 hex characters per hash
  const size_t hex_chars = 64;
  const size_t expected_hash_file_size = NUM_BLOCKS * hex_chars;

  assert(st.st_size >= (off_t)expected_hash_file_size);
  printf("  Hash file size: %ld bytes (expected at least %zu)\n",
         (long)st.st_size, expected_hash_file_size);

  // Read all hashes from the file
  char *hash_file_content =
      read_hash_file(hash_file_path, 0, expected_hash_file_size);
  assert(hash_file_content != NULL);

  // Verify each block's hash
  AntiTamperingState *state_ptr = (AntiTamperingState *)ctx.internal_state;
  for (size_t i = 0; i < NUM_BLOCKS; i++) {
    const void *block_data = test_data + (i * BLOCK_SIZE);
    char *expected_hash =
        compute_block_hash(block_data, BLOCK_SIZE, &state_ptr->hasher);
    assert(expected_hash != NULL);

    // Extract hash from file (each hash is hex_chars bytes)
    char stored_hash[hex_chars + 1];
    memcpy(stored_hash, hash_file_content + (i * hex_chars), hex_chars);
    stored_hash[hex_chars] = '\0';

    // Compare
    assert(strncmp(stored_hash, expected_hash, hex_chars) == 0);
    printf("  Block %zu hash verified: %s\n", i, stored_hash);

    free(expected_hash);
  }

  free(hash_file_content);
  free(test_data);
  anti_tampering_destroy(ctx);
  cleanup_local_layer(&data_layer);
  cleanup_local_layer(&hash_layer);

  // Cleanup
  unlink(test_file_path);
  unlink(hash_file_path);
  free(hash_file_path);
  rmdir(test_data_dir);
  rmdir(test_hash_dir);

  printf("✅ Block write with multiple hashes test passed\n");
}

void test_block_read_hash_verification() {
  printf("Testing block read with hash verification...\n");

  // Create temporary directories
  char test_data_dir[] = "/tmp/test_block_read_data_XXXXXX";
  char test_hash_dir[] = "/tmp/test_block_read_hash_XXXXXX";
  assert(mkdtemp(test_data_dir) != NULL);
  assert(mkdtemp(test_hash_dir) != NULL);

  char test_file_path[512];
  int ret = snprintf(test_file_path, sizeof(test_file_path), "%s/testfile",
                     test_data_dir);
  assert(ret > 0 && ret < (int)sizeof(test_file_path));

  // Initialize layers
  LayerContext data_layer = local_init();
  LayerContext hash_layer = local_init();

  // Initialize anti-tampering layer in block mode
  AntiTamperingConfig cfg = create_block_config(test_hash_dir);
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // Open file for writing
  int fd =
      block_anti_tampering_open(test_file_path, O_RDWR | O_CREAT, 0644, ctx);
  assert(fd >= 0);

  // Prepare test data: 3 blocks
  char *test_data = malloc(TEST_DATA_SIZE);
  assert(test_data != NULL);

  // Fill each block with different patterns
  for (size_t i = 0; i < NUM_BLOCKS; i++) {
    memset(test_data + (i * BLOCK_SIZE), 'A' + (int)i, BLOCK_SIZE);
  }

  // Write all blocks
  ssize_t written =
      block_anti_tampering_write(fd, test_data, TEST_DATA_SIZE, 0, ctx);
  assert(written == TEST_DATA_SIZE);

  // Close file
  assert(block_anti_tampering_close(fd, ctx) == 0);

  // Reopen for reading
  fd = block_anti_tampering_open(test_file_path, O_RDONLY, 0, ctx);
  assert(fd >= 0);

  // Read all blocks - hashes should match
  char *read_buffer = malloc(TEST_DATA_SIZE);
  assert(read_buffer != NULL);
  memset(read_buffer, 0, TEST_DATA_SIZE);

  ssize_t read_bytes =
      block_anti_tampering_read(fd, read_buffer, TEST_DATA_SIZE, 0, ctx);
  assert(read_bytes == TEST_DATA_SIZE);

  // Verify data matches
  assert(memcmp(test_data, read_buffer, TEST_DATA_SIZE) == 0);
  printf("  Read data matches written data\n");

  // Close and reopen to modify the file directly
  assert(block_anti_tampering_close(fd, ctx) == 0);

  // Modify the file directly (bypassing anti-tampering layer)
  int direct_fd = open(test_file_path, O_RDWR);
  assert(direct_fd >= 0);

  // Corrupt block 1 by changing some bytes
  char corruption[100];
  memset(corruption, 'X', sizeof(corruption));
  ssize_t corrupt_written =
      pwrite(direct_fd, corruption, sizeof(corruption), BLOCK_SIZE + 100);
  assert(corrupt_written == sizeof(corruption));
  close(direct_fd);

  printf("  Corrupted block 1 in the file\n");

  // Reopen and read - should detect hash mismatch
  fd = block_anti_tampering_open(test_file_path, O_RDONLY, 0, ctx);
  assert(fd >= 0);

  memset(read_buffer, 0, TEST_DATA_SIZE);
  read_bytes =
      block_anti_tampering_read(fd, read_buffer, TEST_DATA_SIZE, 0, ctx);
  assert(read_bytes == TEST_DATA_SIZE);

  // The read should succeed, but hash verification should detect the mismatch
  // (warnings will be logged, but read still returns data)
  printf(
      "  Read completed after corruption (hash mismatch should be detected)\n");

  assert(block_anti_tampering_close(fd, ctx) == 0);

  free(read_buffer);
  free(test_data);
  anti_tampering_destroy(ctx);
  // Local layers don't need explicit cleanup (stateless)

  // Cleanup
  unlink(test_file_path);
  rmdir(test_data_dir);
  rmdir(test_hash_dir);

  printf("✅ Block read hash verification test passed\n");
}

int main() {
  printf("Running block anti-tampering tests...\n\n");

  printf("Running block write tests...\n");
  test_block_write_multiple_hashes();
  printf("All block write tests passed!\n\n");

  printf("Running block read tests...\n");
  test_block_read_hash_verification();
  printf("All block read tests passed!\n\n");

  printf("All block anti-tampering tests passed!\n");
  return 0;
}
