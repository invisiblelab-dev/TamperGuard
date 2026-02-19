#include "../../../../layers/anti_tampering/anti_tampering.h"
#include "../../../../shared/utils/hasher/hasher.h"
#include "../../../mock_layer.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

static AntiTamperingConfig create_default_anti_tampering_config(void) {
  AntiTamperingConfig cfg = {
      .data_layer = NULL,
      .hash_layer = NULL,
      .hashes_storage = "/tmp/hashes",
      .algorithm = HASH_SHA256,
      .mode = ANTI_TAMPERING_MODE_FILE,
      .block_size = 0,
  };
  return cfg;
}

void test_anti_tampering_ftruncate_basic() {
  printf("Testing basic ftruncate functionality...\n");

  // Setup mock layers
  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  // Initialize anti-tampering layer
  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // Open a file to create a mapping, we use a random name expecting its not
  // existing so no hash validation is done.
  int fd = anti_tampering_open("/tmp/test_anti_tampering_ftruncate_12345.txt",
                               O_RDWR | O_CREAT, 0, ctx);
  assert(fd >= 0);

  // Test ftruncate
  int result = anti_tampering_ftruncate(fd, 200, ctx);

  // Verify the mock was called correctly
  assert(data_state.ftruncate_called == 1);
  assert(data_state.last_ftruncate_input_length == 200);

  // Verify the hash layer was not called
  assert(hash_state.ftruncate_called == 0);
  assert(result == 0); // Should return success

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);

  printf("✅ Basic ftruncate test passed\n");
}

void test_anti_tampering_ftruncate_invalid_fd() {
  printf("Testing ftruncate with invalid file descriptor...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // Test with invalid FD
  int result = anti_tampering_ftruncate(-10, 100, ctx);
  assert(result == -1);
  assert(data_state.ftruncate_called == 0); // Should not call underlying layer

  // Test with FD out of range
  result = anti_tampering_ftruncate(1000000, 100, ctx);
  assert(result == -1);
  assert(data_state.ftruncate_called == 0);

  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ Invalid FD test passed\n");
}

void test_anti_tampering_ftruncate_multiple_calls() {
  printf("Testing ftruncate with multiple calls...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // Open file to create mapping
  int fd = anti_tampering_open("/tmp/test.txt", O_RDWR | O_CREAT, 0644, ctx);
  assert(fd >= 0);

  // Test ftruncate with different lengths
  int result1 = anti_tampering_ftruncate(fd, 50, ctx);
  int result2 = anti_tampering_ftruncate(fd, 150, ctx);

  // Verify both calls worked
  assert(result1 == 0);
  assert(result2 == 0);
  assert(data_state.ftruncate_called == 2);
  assert(data_state.last_ftruncate_input_length == 150);

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ Multiple calls test passed\n");
}

void test_anti_tampering_ftruncate_error_propagation() {
  printf("Testing ftruncate error propagation...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};
  data_state.ftruncate_return_value = -5; // Simulate error

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  int fd = anti_tampering_open("/tmp/test_anti_tampering_ftruncate_12345.txt",
                               O_RDWR | O_CREAT, 0644, ctx);
  assert(fd >= 0);

  // Test ftruncate with error
  int result = anti_tampering_ftruncate(fd, 100, ctx);

  // Verify error is propagated
  assert(result == -5);

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);

  printf("✅ Error propagation test passed\n");
}

void test_anti_tampering_ftruncate_lock_contention() {
  printf("Testing ftruncate with lock contention...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  int fd = anti_tampering_open("/tmp/test_anti_tampering_ftruncate_12345.txt",
                               O_RDWR | O_CREAT, 0, ctx);
  assert(fd >= 0);

  // Manually acquire the write lock first to simulate contention
  AntiTamperingState *state = (AntiTamperingState *)ctx.internal_state;
  char *file_path = state->mappings[fd].file_path;

  // We simulate the lock being held by another process
  int lock_result = locking_acquire_write(state->lock_table, file_path);
  assert(lock_result == 0); // Should succeed

  // Now try to ftruncate - should fail to acquire lock
  int result = anti_tampering_ftruncate(fd, 200, ctx);
  assert(result == -1); // Should fail due to lock contention

  // We release the lock to allow the ftruncate to succeed
  locking_release(state->lock_table, file_path);

  // Now ftruncate should work
  result = anti_tampering_ftruncate(fd, 200, ctx);
  assert(result == 0); // Should succeed now

  // Verify the mock was called correctly
  assert(data_state.ftruncate_called == 1);
  assert(data_state.last_ftruncate_input_length == 200);

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);

  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ Lock contention test passed\n");
}

void test_anti_tampering_fstat_success() {
  printf("Testing fstat success...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  data_state.ftruncate_return_value = 0;
  int fd = anti_tampering_open("/tmp/test_anti_tampering_fstat_12345.txt",
                               O_RDWR | O_CREAT, 0, ctx);
  assert(fd >= 0);

  struct stat st;
  int result = anti_tampering_fstat(fd, &st, ctx);
  assert(result == 0);
  assert(data_state.fstat_called == 1);
  assert(hash_state.fstat_called == 0);

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);

  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ fstat success test passed\n");
}

void test_anti_tampering_fstat_fails_if_lock_contention() {
  printf("Testing fstat fails if lock contention...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  int fd = anti_tampering_open("/tmp/test_anti_tampering_fstat_12345.txt",
                               O_RDWR | O_CREAT, 0, ctx);
  assert(fd >= 0);

  // Manually acquire the write lock first to simulate contention
  AntiTamperingState *state = (AntiTamperingState *)ctx.internal_state;
  char *file_path = state->mappings[fd].file_path;

  // We simulate the lock being held by another process
  int lock_result = locking_acquire_write(state->lock_table, file_path);
  assert(lock_result == 0); // Should succeed

  // Now try to fstat - should fail to acquire lock
  struct stat st;
  int result = anti_tampering_fstat(fd, &st, ctx);
  assert(result == -1); // Should fail due to lock contention

  // We release the lock to allow the fstat to succeed
  locking_release(state->lock_table, file_path);

  // Now fstat should work
  result = anti_tampering_fstat(fd, &st, ctx);
  assert(result == 0); // Should succeed now

  anti_tampering_close(fd, ctx);
  anti_tampering_destroy(ctx);

  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ fstat fails if lock contention test passed\n");
}

void test_anti_tampering_lstat_success() {
  printf("Testing lstat success...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  struct stat st;
  int result = anti_tampering_lstat("/tmp/test_anti_tampering_lstat_12345.txt",
                                    &st, ctx);
  assert(result == 0);
  assert(data_state.lstat_called == 1);
  assert(hash_state.lstat_called == 0);

  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ lstat success test passed\n");
}

void test_anti_tampering_lstat_fails_if_lock_contention() {
  printf("Testing lstat fails if lock contention...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext ctx = anti_tampering_init(data_layer, hash_layer, &cfg);

  // We simulate the lock being held by another process
  AntiTamperingState *state = (AntiTamperingState *)ctx.internal_state;
  int lock_result = locking_acquire_write(
      state->lock_table, "/tmp/test_anti_tampering_lstat_12345.txt");
  assert(lock_result == 0); // Should succeed

  // Now try to lstat - should fail to acquire lock
  struct stat st;
  int result = anti_tampering_lstat("/tmp/test_anti_tampering_lstat_12345.txt",
                                    &st, ctx);
  assert(result == -1);

  // We release the lock to allow the lstat to succeed
  locking_release(state->lock_table,
                  "/tmp/test_anti_tampering_lstat_12345.txt");
  // Now lstat should work
  result = anti_tampering_lstat("/tmp/test_anti_tampering_lstat_12345.txt", &st,
                                ctx);
  assert(result == 0); // Should succeed now

  anti_tampering_destroy(ctx);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ lstat fails if lock contention test passed\n");
}

void test_anti_tampering_unlink_success() {
  printf("Testing unlink success...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext anti_tampering_layer =
      anti_tampering_init(data_layer, hash_layer, &cfg);
  AntiTamperingState *state =
      (AntiTamperingState *)anti_tampering_layer.internal_state;

  assert(anti_tampering_layer.ops->lunlink != NULL);

  // We make an open to later compare the hash pathnames created
  int res = anti_tampering_layer.ops->lopen(
      "/tmp/test_anti_tampering_unlink_12345.txt", O_RDWR | O_CREAT, 0,
      anti_tampering_layer);
  assert(res >= 0);
  char *hash_pathname = strdup(state->mappings[res].hash_path);
  assert(hash_pathname != NULL);
  anti_tampering_layer.ops->lclose(res, anti_tampering_layer);

  int result = anti_tampering_layer.ops->lunlink(
      "/tmp/test_anti_tampering_unlink_12345.txt", anti_tampering_layer);
  assert(result == 0);
  assert(data_state.unlink_called == 1);
  assert(hash_state.unlink_called == 1);
  // We ensure the hash pathname is the same as the one we opened
  assert(hash_state.unlink_pathname_called_str != NULL);
  assert(strcmp(hash_state.unlink_pathname_called_str, hash_pathname) == 0);
  free(hash_pathname);

  anti_tampering_destroy(anti_tampering_layer);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ unlink success test passed\n");
}

void test_anti_tampering_unlink_fails_if_lock_contention() {
  printf("Testing unlink fails if lock contention...\n");

  MockLayerState data_state = {0};
  MockLayerState hash_state = {0};

  LayerContext data_layer = create_mock_layer(&data_state);
  LayerContext hash_layer = create_mock_layer(&hash_state);

  AntiTamperingConfig cfg = create_default_anti_tampering_config();
  LayerContext anti_tampering_layer =
      anti_tampering_init(data_layer, hash_layer, &cfg);

  assert(anti_tampering_layer.ops->lunlink != NULL);

  // We simulate the lock being held by another process
  AntiTamperingState *state =
      (AntiTamperingState *)anti_tampering_layer.internal_state;
  int lock_result = locking_acquire_write(
      state->lock_table, "/tmp/test_anti_tampering_unlink_12345.txt");
  assert(lock_result == 0); // Should succeed

  // Now try to unlink - should fail to acquire lock
  int result = anti_tampering_unlink(
      "/tmp/test_anti_tampering_unlink_12345.txt", anti_tampering_layer);
  assert(result == -1);

  // We release the lock to allow the unlink to succeed
  locking_release(state->lock_table,
                  "/tmp/test_anti_tampering_unlink_12345.txt");
  // Now unlink should work
  result = anti_tampering_unlink("/tmp/test_anti_tampering_unlink_12345.txt",
                                 anti_tampering_layer);
  assert(result == 0); // Should succeed now

  anti_tampering_destroy(anti_tampering_layer);
  destroy_mock_layer(data_layer);
  destroy_mock_layer(hash_layer);
  printf("✅ unlink fails if lock contention test passed\n");
}

int main() {
  printf("Running anti-tampering tests...\n\n");

  printf("Running anti-tampering ftruncate tests...\n");
  test_anti_tampering_ftruncate_basic();
  test_anti_tampering_ftruncate_invalid_fd();
  test_anti_tampering_ftruncate_multiple_calls();
  test_anti_tampering_ftruncate_error_propagation();
  test_anti_tampering_ftruncate_lock_contention();
  printf("All anti-tampering ftruncate tests passed!\n");

  printf("Running anti-tampering fstat tests...\n");
  test_anti_tampering_fstat_success();
  test_anti_tampering_fstat_fails_if_lock_contention();
  printf("All anti-tampering fstat tests passed!\n");

  printf("Running anti-tampering lstat tests...\n");
  test_anti_tampering_lstat_success();
  test_anti_tampering_lstat_fails_if_lock_contention();
  printf("All anti-tampering lstat tests passed!\n");

  printf("Running anti-tampering unlink tests...\n");
  test_anti_tampering_unlink_success();
  test_anti_tampering_unlink_fails_if_lock_contention();
  printf("All anti-tampering unlink tests passed!\n");

  printf("\nAll anti-tampering tests passed!\n");
  return 0;
}
