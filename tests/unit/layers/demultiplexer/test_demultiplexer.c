#include "../../../../layers/demultiplexer/demultiplexer.h"
#include "../../../mock_layer.h"
#include <assert.h>
#include <errno.h> // Required for errno testing
#include <stdio.h>
#include <sys/wait.h> // Required for waitpid, WIFEXITED, WEXITSTATUS
#include <unistd.h>   // Required for unlink and fork

// Test state
static MockLayerState mock_states[3]; // For 3 layers
static LayerContext mock_layers[3];

void setup_test() {
  // Initialize mock layers
  for (int i = 0; i < 3; i++) {
    reset_mock_state(&mock_states[i], 0, 1024);
    mock_layers[i] = create_mock_layer(&mock_states[i]);
  }
}

void test_demultiplexer_ftruncate_success() {
  printf("Testing demultiplexer_ftruncate success case...\n");

  setup_test();

  // Create demultiplexer with 3 mock layers
  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings (simulate previous open calls)
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10; // Master FD 5 maps to layer 0 FD 10
  state->layer_fds[5][1] = 11; // Master FD 5 maps to layer 1 FD 11
  state->layer_fds[5][2] = 12; // Master FD 5 maps to layer 2 FD 12

  // Set mock return values
  mock_states[0].ftruncate_return_value = 0; // Success
  mock_states[1].ftruncate_return_value = 0; // Success
  mock_states[2].ftruncate_return_value = 0; // Success

  // Call ftruncate
  int result = demultiplexer_ftruncate(5, 512, demux);

  // Verify results
  assert(result == 0); // Should return result from first layer
  assert(mock_states[0].ftruncate_called == 1);
  assert(mock_states[1].ftruncate_called == 1);
  assert(mock_states[2].ftruncate_called == 1);
  assert(mock_states[0].last_ftruncate_input_fd == 10);
  assert(mock_states[1].last_ftruncate_input_fd == 11);
  assert(mock_states[2].last_ftruncate_input_fd == 12);
  assert(mock_states[0].last_ftruncate_input_length == 512);
  assert(mock_states[1].last_ftruncate_input_length == 512);
  assert(mock_states[2].last_ftruncate_input_length == 512);

  printf("✅ demultiplexer_ftruncate success test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_ftruncate_invalid_fd() {
  printf("Testing demultiplexer_ftruncate with invalid FD...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Test negative FD
  int result = demultiplexer_ftruncate(-1, 512, demux);
  assert(result == -1);

  // Test FD >= MAX_FDS
  result = demultiplexer_ftruncate(1000000, 512, demux);
  assert(result == -1);

  // Verify no mock calls were made
  assert(mock_states[0].ftruncate_called == 0);
  assert(mock_states[1].ftruncate_called == 0);
  assert(mock_states[2].ftruncate_called == 0);

  printf("✅ demultiplexer_ftruncate invalid FD test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_ftruncate_layer_errors() {
  printf("Testing demultiplexer_ftruncate with layer errors...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test case 1: First layer fails
  mock_states[0].ftruncate_return_value = -1; // First layer fails
  mock_states[1].ftruncate_return_value = 0;  // Others succeed
  mock_states[2].ftruncate_return_value = 0;

  int result = demultiplexer_ftruncate(5, 512, demux);
  assert(result == -1); // Should return first layer's result

  // Reset and test case 2: Second layer fails
  setup_test();
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  mock_states[0].ftruncate_return_value = 0;  // First layer succeeds
  mock_states[1].ftruncate_return_value = -1; // Second layer fails
  mock_states[2].ftruncate_return_value = 0;  // Third succeeds

  result = demultiplexer_ftruncate(5, 512, demux);
  assert(result == -1); // Should fail because second layer failed

  printf("✅ demultiplexer_ftruncate layer errors test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_ftruncate_different_lengths() {
  printf("Testing demultiplexer_ftruncate with different lengths...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test with zero length
  int result = demultiplexer_ftruncate(5, 0, demux);
  assert(result == 0);
  assert(mock_states[0].last_ftruncate_input_length == 0);
  assert(mock_states[1].last_ftruncate_input_length == 0);
  assert(mock_states[2].last_ftruncate_input_length == 0);

  // Reset and test with large length
  setup_test();
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  result = demultiplexer_ftruncate(5, (off_t)(1024 * 1024), demux);
  assert(result == 0);
  assert(mock_states[0].last_ftruncate_input_length == (off_t)(1024 * 1024));
  assert(mock_states[1].last_ftruncate_input_length == (off_t)(1024 * 1024));
  assert(mock_states[2].last_ftruncate_input_length == (off_t)(1024 * 1024));

  printf("✅ demultiplexer_ftruncate different lengths test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_ftruncate_single_layer() {
  printf("Testing demultiplexer_ftruncate with single layer...\n");

  setup_test();

  // Create demultiplexer with only 1 mock layer
  int passthrough_reads[] = {0};  // No passthrough reads
  int passthrough_writes[] = {0}; // No passthrough writes
  int enforced_layers[] = {1};    // Single layer enforced
  LayerContext demux = demultiplexer_init(mock_layers, 1, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;

  int result = demultiplexer_ftruncate(5, 256, demux);
  assert(result == 0);
  assert(mock_states[0].ftruncate_called == 1);
  assert(mock_states[0].last_ftruncate_input_fd == 10);
  assert(mock_states[0].last_ftruncate_input_length == 256);

  // Verify other layers weren't called
  assert(mock_states[1].ftruncate_called == 0);
  assert(mock_states[2].ftruncate_called == 0);

  printf("✅ demultiplexer_ftruncate single layer test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_ftruncate_multiple_fds() {
  printf("Testing demultiplexer_ftruncate with multiple FDs...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;

  // Set up mappings for two different FDs
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  state->layer_fds[7][0] = 20;
  state->layer_fds[7][1] = 21;
  state->layer_fds[7][2] = 22;

  // Call ftruncate on first FD
  int result1 = demultiplexer_ftruncate(5, 512, demux);
  assert(result1 == 0);

  // Reset mock states
  setup_test();
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;
  state->layer_fds[7][0] = 20;
  state->layer_fds[7][1] = 21;
  state->layer_fds[7][2] = 22;

  // Call ftruncate on second FD
  int result2 = demultiplexer_ftruncate(7, 1024, demux);
  assert(result2 == 0);

  // Verify correct FDs were used
  assert(mock_states[0].last_ftruncate_input_fd == 20);
  assert(mock_states[1].last_ftruncate_input_fd == 21);
  assert(mock_states[2].last_ftruncate_input_fd == 22);
  assert(mock_states[0].last_ftruncate_input_length == 1024);
  assert(mock_states[1].last_ftruncate_input_length == 1024);
  assert(mock_states[2].last_ftruncate_input_length == 1024);

  printf("✅ demultiplexer_ftruncate multiple FDs test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_passthrough_reads() {
  printf("Testing demultiplexer passthrough ops...\n");
  setup_test();

  int passthrough_reads[] = {0,
                             1}; // Mock layer (index 0) uses passthrough reads
  int passthrough_writes[] = {0, 0}; // No passthrough writes
  int enforced_layers[] = {0, 0};    // Both layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 2, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10; // Mock layer FD
  state->layer_fds[5][1] = 11; // Local layer FD

  mock_states[0].mock_pread_data = "test_data";
  mock_states[0].mock_pread_data_size = 10;
  mock_states[1].mock_pread_data = "test_data";
  mock_states[1].mock_pread_data_size = 10;

  // Test read operation - this should call passthrough_pread for mock layer
  char test_buffer[10];
  ssize_t read_result =
      demux.ops->lpread(5, test_buffer, sizeof(test_buffer), 0, demux);
  assert(read_result ==
         10); // should return 10 because mock layer is passthrough

  printf("✅ demultiplexer_passthrough_reads test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_init_invalid_both_passthrough() {
  printf("Testing demultiplexer_init with layer having both read and write "
         "passthrough...\n");

  setup_test();

  // Invalid: layer 0 has both read and write passthrough
  int passthrough_reads[] = {1, 0, 0};  // Layer 0 has passthrough reads
  int passthrough_writes[] = {1, 0, 0}; // Layer 0 has passthrough writes

  int enforced_layers[] = {1, 1, 1}; // All layers enforced

  pid_t pid = fork();
  if (pid == 0) {
    // Child process - this should exit with code 1
    demultiplexer_init(mock_layers, 3, passthrough_reads, passthrough_writes,
                       enforced_layers);
    exit(0); // Should not reach here
  } else if (pid > 0) {
    // Parent process - wait for child and check exit status
    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    printf(
        "✅ demultiplexer_init correctly failed for layer with both read and "
        "write passthrough\n");
  } else {
    // Fork failed
    assert(0 && "Fork failed");
  }
}

void test_demultiplexer_init_invalid_all_passthrough_reads() {
  printf("Testing demultiplexer_init with all layers having passthrough "
         "reads...\n");

  setup_test();

  // Invalid: all layers have passthrough reads
  int passthrough_reads[] = {1, 1, 1};  // All layers have passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced

  pid_t pid = fork();
  if (pid == 0) {
    // Child process - this should exit with code 1
    demultiplexer_init(mock_layers, 3, passthrough_reads, passthrough_writes,
                       enforced_layers);
    exit(0); // Should not reach here
  } else if (pid > 0) {
    // Parent process - wait for child and check exit status
    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    printf("✅ demultiplexer_init correctly failed for all layers with "
           "passthrough reads\n");
  } else {
    // Fork failed
    assert(0 && "Fork failed");
  }
}

void test_demultiplexer_init_invalid_passthrough_and_enforced() {
  printf("Testing demultiplexer_init with layer having passthrough reads and "
         "enforced...\n");

  setup_test();

  int passthrough_reads[] = {1, 0, 0};  // Layer 0 has passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced

  pid_t pid = fork();
  if (pid == 0) {
    // Child process - this should exit with code 1
    demultiplexer_init(mock_layers, 3, passthrough_reads, passthrough_writes,
                       enforced_layers);
    exit(0); // Should not reach here
  } else if (pid > 0) {
    // Parent process - wait for child and check exit status
    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    printf("✅ demultiplexer_init correctly failed for layer with passthrough "
           "reads and enforced\n");
  } else {
    // Fork failed
    assert(0 && "Fork failed");
  }
}

void test_demultiplexer_init_invalid_all_passthrough_writes() {
  printf("Testing demultiplexer_init with all layers having passthrough "
         "writes...\n");

  setup_test();

  // Invalid: all layers have passthrough writes (but not reads to avoid both
  // passthrough error)
  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {1, 1, 1}; // All layers have passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced

  pid_t pid = fork();
  if (pid == 0) {
    // Child process - this should exit with code 1
    demultiplexer_init(mock_layers, 3, passthrough_reads, passthrough_writes,
                       enforced_layers);
    exit(0); // Should not reach here
  } else if (pid > 0) {
    // Parent process - wait for child and check exit status
    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    printf("✅ demultiplexer_init correctly failed for all layers with "
           "passthrough writes\n");
  } else {
    // Fork failed
    assert(0 && "Fork failed");
  }
}

void test_demultiplexer_pread_success_with_no_enforced() {
  printf("Testing demultiplexer_pread with no enforced layers...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {0, 0, 0};    // No enforced layers
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  mock_states[0].mock_pread_data = "test_data";
  mock_states[0].mock_pread_data_size = 10;

  // Layer 1 is not enforced, so only the read result from layer 0 counts
  mock_states[1].mock_pread_data = "test_data2";
  mock_states[1].mock_pread_data_size = 11;
  mock_states[2].mock_pread_data = "test_data";
  mock_states[2].mock_pread_data_size = 10;

  char test_buffer[10];
  ssize_t read_result =
      demux.ops->lpread(5, test_buffer, sizeof(test_buffer), 0, demux);

  assert(read_result == 10);
  printf("Test buffer: %s\n", test_buffer);
  assert(memcmp(test_buffer, "test_data", 10) == 0);

  printf(
      "✅ demultiplexer_pread success with no enforced layers test passed\n");
}

// ================================ Fstat tests ================================
void test_demultiplexer_fstat_success() {
  printf("Testing demultiplexer_fstat success case...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes

  int enforced_layers[] = {1, 1, 1}; // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);
  assert(demux.ops->lfstat != NULL);

  struct stat stbuf;
  int result = demultiplexer_fstat(5, &stbuf, demux);
  assert(result == 0);
  assert(mock_states[0].fstat_called == 1);
  assert(mock_states[1].fstat_called == 1);
  assert(mock_states[2].fstat_called == 1);

  demultiplexer_destroy(demux);
  printf("✅ demultiplexer_fstat success test passed\n");
}

// ================================ Lstat tests ================================
void test_demultiplexer_lstat_success() {
  printf("Testing demultiplexer_lstat success case...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);
  assert(demux.ops->llstat != NULL);

  struct stat stbuf;
  int result = demultiplexer_lstat("test.txt", &stbuf, demux);
  assert(result == 0);
  assert(mock_states[0].lstat_called == 1);
  assert(mock_states[1].lstat_called == 1);
  assert(mock_states[2].lstat_called == 1);

  demultiplexer_destroy(demux);
  printf("✅ demultiplexer_lstat success test passed\n");
}

// ================================ Errno tests ================================
void test_demultiplexer_fstat_errno_propagation() {
  printf("Testing demultiplexer_fstat errno propagation...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test case 1: First enforced layer fails with EBADF
  mock_states[0].fstat_return_value = -1;
  mock_states[0].stat_errno_value = EBADF;
  mock_states[1].fstat_return_value = 0; // Success
  mock_states[2].fstat_return_value = 0; // Success

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_fstat(5, &stbuf, demux);

  assert(result == -1);
  assert(errno == EBADF); // Should propagate errno from first enforced layer
  assert(mock_states[0].fstat_called == 1);
  assert(mock_states[1].fstat_called == 1);
  assert(mock_states[2].fstat_called == 1);

  printf("✅ demultiplexer_fstat errno propagation test (EBADF) passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_fstat_errno_multiple_failures() {
  printf("Testing demultiplexer_fstat errno with multiple layer failures...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test case: Multiple layers fail with different errno values
  // Should propagate errno from first enforced layer that failed
  mock_states[0].fstat_return_value = -1;
  mock_states[0].stat_errno_value = EBADF;
  mock_states[1].fstat_return_value = -1;
  mock_states[1].stat_errno_value = ENOENT;
  mock_states[2].fstat_return_value = -1;
  mock_states[2].stat_errno_value = EACCES;

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_fstat(5, &stbuf, demux);

  assert(result == -1);
  assert(errno == EBADF); // Should propagate errno from first enforced layer
  assert(mock_states[0].fstat_called == 1);
  assert(mock_states[1].fstat_called == 1);
  assert(mock_states[2].fstat_called == 1);

  printf("✅ demultiplexer_fstat multiple failures errno test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_fstat_errno_second_layer_fails() {
  printf(
      "Testing demultiplexer_fstat errno when second layer fails first...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {0, 1, 1};    // Only layers 1 and 2 enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // First layer succeeds but is not enforced, second layer (first enforced)
  // fails
  mock_states[0].fstat_return_value = 0; // Success but not enforced
  mock_states[1].fstat_return_value = -1;
  mock_states[1].stat_errno_value = ENOENT;
  mock_states[2].fstat_return_value = 0; // Success

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_fstat(5, &stbuf, demux);

  assert(result == -1);
  assert(
      errno ==
      ENOENT); // Should propagate errno from first enforced layer that failed
  assert(mock_states[0].fstat_called == 1);
  assert(mock_states[1].fstat_called == 1);
  assert(mock_states[2].fstat_called == 1);

  printf("✅ demultiplexer_fstat second layer failure errno test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_lstat_errno_propagation() {
  printf("Testing demultiplexer_lstat errno propagation...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Test case 1: First enforced layer fails with ENOENT
  mock_states[0].lstat_return_value = -1;
  mock_states[0].stat_errno_value = ENOENT;
  mock_states[1].lstat_return_value = 0; // Success
  mock_states[2].lstat_return_value = 0; // Success

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_lstat("nonexistent.txt", &stbuf, demux);

  assert(result == -1);
  assert(errno == ENOENT); // Should propagate errno from first enforced layer
  assert(mock_states[0].lstat_called == 1);
  assert(mock_states[1].lstat_called == 1);
  assert(mock_states[2].lstat_called == 1);

  printf("✅ demultiplexer_lstat errno propagation test (ENOENT) passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_lstat_errno_multiple_failures() {
  printf("Testing demultiplexer_lstat errno with multiple layer failures...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Test case: Multiple layers fail with different errno values
  // Should propagate errno from first enforced layer that failed
  mock_states[0].lstat_return_value = -1;
  mock_states[0].stat_errno_value = EACCES;
  mock_states[1].lstat_return_value = -1;
  mock_states[1].stat_errno_value = ENOENT;
  mock_states[2].lstat_return_value = -1;
  mock_states[2].stat_errno_value = ENOTDIR;

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_lstat("test/path.txt", &stbuf, demux);

  assert(result == -1);
  assert(errno == EACCES); // Should propagate errno from first enforced layer
  assert(mock_states[0].lstat_called == 1);
  assert(mock_states[1].lstat_called == 1);
  assert(mock_states[2].lstat_called == 1);

  printf("✅ demultiplexer_lstat multiple failures errno test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_lstat_errno_permission_denied() {
  printf("Testing demultiplexer_lstat errno with permission denied...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {0, 1, 0};    // Only middle layer enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // First layer succeeds but not enforced, second layer (enforced) fails
  mock_states[0].lstat_return_value = 0; // Success but not enforced
  mock_states[1].lstat_return_value = -1;
  mock_states[1].stat_errno_value = EACCES;
  mock_states[2].lstat_return_value = 0; // Success but not enforced

  errno = 0; // Clear errno before test
  struct stat stbuf;
  int result = demultiplexer_lstat("protected.txt", &stbuf, demux);

  assert(result == -1);
  assert(errno == EACCES); // Should propagate errno from enforced layer
  assert(mock_states[0].lstat_called == 1);
  assert(mock_states[1].lstat_called == 1);
  assert(mock_states[2].lstat_called == 1);

  printf("✅ demultiplexer_lstat permission denied errno test passed\n");

  demultiplexer_destroy(demux);
}

void test_demultiplexer_errno_success_no_change() {
  printf("Testing demultiplexer operations don't change errno on success...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings for fstat
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Set all operations to succeed
  mock_states[0].fstat_return_value = 0;
  mock_states[1].fstat_return_value = 0;
  mock_states[2].fstat_return_value = 0;
  mock_states[0].lstat_return_value = 0;
  mock_states[1].lstat_return_value = 0;
  mock_states[2].lstat_return_value = 0;

  // Set errno to a specific value before operations
  errno = EINVAL;

  struct stat stbuf;

  // Test fstat success
  int fstat_result = demultiplexer_fstat(5, &stbuf, demux);
  assert(fstat_result == 0);
  assert(errno == EINVAL); // errno should be unchanged on success

  // Test lstat success
  int lstat_result = demultiplexer_lstat("test.txt", &stbuf, demux);
  assert(lstat_result == 0);
  assert(errno == EINVAL); // errno should be unchanged on success

  printf("✅ demultiplexer errno unchanged on success test passed\n");

  demultiplexer_destroy(demux);
}

// ================================ Unlink tests
// ================================
void test_demultiplexer_unlink_success() {
  printf("Testing demultiplexer_unlink success case...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {1, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);
  assert(demux.ops->lunlink != NULL);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test case: All layers succeed
  mock_states[0].unlink_return_value = 0; // Success
  mock_states[1].unlink_return_value = 0; // Success
  mock_states[2].unlink_return_value = 0; // Success

  int result = demux.ops->lunlink("test.txt", demux);
  assert(result == 0);
  assert(mock_states[0].unlink_called == 1);
  assert(mock_states[1].unlink_called == 1);
  assert(mock_states[2].unlink_called == 1);

  demultiplexer_destroy(demux);
  printf("✅ demultiplexer_unlink success test passed\n");
}

void test_demultiplexer_unlink_fails_when_enforced_layer_fails() {
  printf("Testing demultiplexer_unlink fails when enforced layer fails...\n");

  setup_test();

  int passthrough_reads[] = {0, 0, 0};  // No passthrough reads
  int passthrough_writes[] = {0, 0, 0}; // No passthrough writes
  int enforced_layers[] = {0, 1, 1};    // All layers enforced
  LayerContext demux = demultiplexer_init(mock_layers, 3, passthrough_reads,
                                          passthrough_writes, enforced_layers);

  // Set up file descriptor mappings
  DemultiplexerState *state = (DemultiplexerState *)demux.internal_state;
  state->layer_fds[5][0] = 10;
  state->layer_fds[5][1] = 11;
  state->layer_fds[5][2] = 12;

  // Test case 1: Optional layers fail
  mock_states[0].unlink_return_value = -1; // Success
  mock_states[1].unlink_return_value = 0;  // Success
  mock_states[2].unlink_return_value = 0;  // Success

  int result = demux.ops->lunlink("test.txt", demux);
  // Should return success because the enforced layers succeeded
  assert(result == 0);
  assert(mock_states[0].unlink_called == 1);
  assert(mock_states[1].unlink_called == 1);
  assert(mock_states[2].unlink_called == 1);

  // Test case 2: fails when the first enforced layer fails
  mock_states[0].unlink_return_value = 0;  // Success
  mock_states[1].unlink_return_value = -1; // Failure
  mock_states[2].unlink_return_value = 0;  // Success
  mock_states[0].unlink_called = 0;
  mock_states[1].unlink_called = 0;
  mock_states[2].unlink_called = 0;

  result = demux.ops->lunlink("test.txt", demux);
  assert(result == -1);
  assert(mock_states[0].unlink_called == 1);
  assert(mock_states[1].unlink_called == 1);
  assert(mock_states[2].unlink_called == 1);

  // Test case 3: fails when the last enforced layer fails
  mock_states[0].unlink_return_value = 0;  // Success
  mock_states[1].unlink_return_value = 0;  // Success
  mock_states[2].unlink_return_value = -1; // Failure
  mock_states[0].unlink_called = 0;
  mock_states[1].unlink_called = 0;
  mock_states[2].unlink_called = 0;

  result = demux.ops->lunlink("test.txt", demux);
  assert(result == -1);
  assert(mock_states[0].unlink_called == 1);
  assert(mock_states[1].unlink_called == 1);
  assert(mock_states[2].unlink_called == 1);

  demultiplexer_destroy(demux);
  printf(
      "✅ demultiplexer_unlink fails when enforced layer fails test passed\n");
}

int main() {
  printf("Starting demultiplexer unit tests...\n\n");

  test_demultiplexer_ftruncate_success();
  test_demultiplexer_ftruncate_invalid_fd();
  test_demultiplexer_ftruncate_layer_errors();
  test_demultiplexer_ftruncate_different_lengths();
  test_demultiplexer_ftruncate_single_layer();
  test_demultiplexer_ftruncate_multiple_fds();
  test_demultiplexer_passthrough_reads();
  test_demultiplexer_init_invalid_both_passthrough();
  test_demultiplexer_init_invalid_all_passthrough_reads();
  test_demultiplexer_init_invalid_passthrough_and_enforced();
  test_demultiplexer_init_invalid_all_passthrough_writes();

  test_demultiplexer_fstat_success();
  test_demultiplexer_fstat_errno_propagation();
  test_demultiplexer_fstat_errno_multiple_failures();
  test_demultiplexer_fstat_errno_second_layer_fails();

  test_demultiplexer_lstat_success();
  test_demultiplexer_lstat_errno_propagation();
  test_demultiplexer_lstat_errno_multiple_failures();
  test_demultiplexer_lstat_errno_permission_denied();

  test_demultiplexer_pread_success_with_no_enforced();
  test_demultiplexer_errno_success_no_change();

  test_demultiplexer_unlink_success();
  test_demultiplexer_unlink_fails_when_enforced_layer_fails();

  printf("\n🎉 All demultiplexer tests passed!\n");
  return 0;
}
