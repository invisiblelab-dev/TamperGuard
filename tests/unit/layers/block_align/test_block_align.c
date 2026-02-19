/**
 * WARNING: All the tests consider a block size of 4096.
 * Any other value can cause some tests to fail and also make some tests lose
 * their purpose, because, for example, the block frontiers change.
 */

#include "../../../../layers/block_align/block_align.h"
#include "../../../../layers/local/local.h"
#include "../../../mock_layer.h"
#include "types/layer_context.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TESTPATH "test_file.txt"

// Test state
static MockLayerState mock_state;
static LayerContext mock_layer;
static int mock_block_size = 4096; // 4KB blocks for testing

void setup_test_with_mock_layer() {
  reset_mock_state(&mock_state, 0, 10240); // 10KB initial file size
  mock_layer = create_mock_layer(&mock_state);
}

void fill_file(int fd, size_t block_size) {
  char *buffer = malloc(block_size * sizeof(char));
  int i;
  size_t j;

  for (i = 0; i < 5; i++) {
    char c = (char)(i + '0');
    for (j = 0; j < block_size; j++)
      buffer[j] = c;
    write(fd, buffer, block_size * sizeof(char));
  }

  free(buffer);
}

void test_read_just_1_block(LayerContext layer_block_align) {
  printf("Testing read from just one block\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[1000];
  ssize_t bytes_read =
      layer_block_align.ops->lpread(fd, buffer, 1000, 6000, layer_block_align);

  int r = bytes_read == 1000;
  for (int i = 0; i < 1000 && r; i++) {
    r = buffer[i] == '1';
  }
  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(r);

  printf("✅ Read from just one block test passed\n");
}

void test_write_just_1_block(LayerContext layer_block_align) {
  printf("Testing write to just one block\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[1500];
  for (int i = 0; i < 1500; i++) {
    buffer[i] = '9';
  }
  ssize_t bytes_written =
      layer_block_align.ops->lpwrite(fd, buffer, 1500, 5800, layer_block_align);

  int r = bytes_written == 1500;

  if (r) {
    for (int i = 0; i < 1500; i++)
      buffer[i] = '\0';
    pread(fd, buffer, 1500, 5800);
    for (int i = 0; i < 1500 && r; i++) {
      r = buffer[i] == '9';
    }
  }
  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(r);

  printf("✅ Write to just one block test passed\n");
}

void test_read_out_of_bounds(LayerContext layer_block_align) {
  printf("Testing read after the end of the file\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[200];
  ssize_t bytes_read =
      layer_block_align.ops->lpread(fd, buffer, 200, 20480, layer_block_align);
  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(bytes_read == 0);

  printf("✅ Read after the end of the file test passed\n");
}

void test_write_out_of_bounds(LayerContext layer_block_align) {
  printf("Testing write after the end of the file\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[250];
  for (int i = 0; i < 250; i++) {
    buffer[i] = '9';
  }
  ssize_t bytes_written =
      layer_block_align.ops->lpwrite(fd, buffer, 250, 20500, layer_block_align);

  int r = bytes_written == 250;

  if (r) {
    /*pread(fd, buffer, 20, 20481);
    for(int i = 0; i < 20 && r; i++) {
      r = buffer[i] == '\0';
    }*/
    for (int i = 0; i < 250; i++)
      buffer[i] = '\0';
    pread(fd, buffer, 250, 20500);
    for (int i = 0; i < 250 && r; i++) {
      r = buffer[i] == '9';
    }
  }
  layer_block_align.ops->lclose(fd, layer_block_align);

  assert(r);

  printf("✅ Write after the end of the file test passed\n");
}

void test_read_just_1_byte(LayerContext layer_block_align) {
  printf("Testing read of just one byte at block limit\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[1];
  ssize_t bytes_read =
      layer_block_align.ops->lpread(fd, buffer, 1, 12287, layer_block_align);

  int r = bytes_read == 1;
  if (r) {
    r = buffer[0] == '2';
  }

  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(r);

  printf("✅ read of just one byte at block limit test passed\n");
}

void test_write_just_1_byte(LayerContext layer_block_align) {
  printf("Testing write of just one byte at block limit\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC, 0666,
                                        layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  fill_file(fd, state->block_size);

  char buffer[1];
  buffer[0] = '9';
  ssize_t bytes_written =
      layer_block_align.ops->lpwrite(fd, buffer, 1, 12287, layer_block_align);

  int r = bytes_written == 1;
  if (r) {
    buffer[0] = '\0';
    pread(fd, buffer, 1, 12287);
    r = buffer[0] == '9';
  }

  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(r);

  printf("✅ write of just one byte at block limit test passed\n");
}

void test_write_end_file(LayerContext layer_block_align) {
  printf("Testing Write in the end of the file \n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;

  fill_file(fd, state->block_size);

  struct stat stbuf;
  int result = layer_block_align.ops->lfstat(fd, &stbuf, layer_block_align);
  assert(result == 0);
  off_t size = stbuf.st_size;

  printf("antes\n");
  ssize_t res_write = layer_block_align.ops->lpwrite(fd, "#End of file#", 13,
                                                     size, layer_block_align);
  assert(res_write == 13);

  printf("depois\n");
  char buf[13];
  ssize_t res_read = pread(fd, buf, 13, size);
  layer_block_align.ops->lclose(fd, layer_block_align);
  assert(res_read == res_write);

  assert(strncmp(buf, "#End of file#", res_read) == 0);
  printf("✅ Write in the end test passed\n");
}

void test_write_in_middle_and_end(LayerContext layer_block_align) {

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;

  fill_file(fd, state->block_size);

  struct stat stbuf;
  int result = layer_block_align.ops->lfstat(fd, &stbuf, layer_block_align);
  assert(result == 0);
  off_t size = stbuf.st_size;

  assert(size > 0);

  char *test_message = "#Write in the middle that surpasses the end#";
  unsigned long num_bytes_to_write = strlen(test_message);
  off_t middle_file = (off_t)(size - num_bytes_to_write / 2);

  ssize_t res_write = layer_block_align.ops->lpwrite(
      fd, test_message, num_bytes_to_write, middle_file, layer_block_align);

  assert(res_write == num_bytes_to_write);
  char buf[num_bytes_to_write];
  ssize_t res_read = pread(fd, buf, num_bytes_to_write, middle_file);
  layer_block_align.ops->lclose(fd, layer_block_align);

  assert(strncmp(buf, test_message, res_read) == 0);
}

void test_three_blocks(LayerContext layer_block_align) {
  printf("Testing ops in 3 different blocks\n");
  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;

  fill_file(fd, state->block_size);

  char *test_message = calloc(5001, sizeof(char));

  for (size_t i = 0; i < 5000; i++) {
    test_message[i] = '9';
  }

  int num_bytes_to_write = 5000;
  ssize_t res_write = layer_block_align.ops->lpwrite(
      fd, test_message, num_bytes_to_write, 4000, layer_block_align);

  assert(res_write == num_bytes_to_write);

  char buf[num_bytes_to_write];
  ssize_t res_read = pread(fd, buf, num_bytes_to_write, 4000);

  char buf_test[num_bytes_to_write];
  ssize_t res_read_test = layer_block_align.ops->lpread(
      fd, buf_test, num_bytes_to_write, 4000, layer_block_align);
  layer_block_align.ops->lclose(fd, layer_block_align);

  assert(res_read == res_read_test);
  assert(strncmp(buf, test_message, res_read) == 0);
  assert(strncmp(buf_test, test_message, res_read_test) == 0);

  free(test_message);
  printf("✅ Three blocks test passed\n");
}

void test_write_cross_block_at_eof(LayerContext layer_block_align) {
  printf("Testing 2-byte write across block boundary where last byte is EOF\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  size_t block_size = state->block_size;

  // Making file exactly 4096 + 1 bytes long
  char *buffer = calloc(block_size + 1, sizeof(char));
  memset(buffer, 'A', block_size);
  buffer[block_size] = 'B';
  write(fd, buffer, block_size + 1);
  free(buffer);

  // Prepare 2-byte write: one at offset 4095 (end of first block),
  // next at offset 4096 (first of second block)
  char data[2] = {'X', 'Y'};

  ssize_t written = layer_block_align.ops->lpwrite(
      fd, data, 2, (off_t)block_size - 1, layer_block_align);
  assert(written == 2);

  char readback[2];
  pread(fd, readback, 2, (off_t)block_size - 1);
  layer_block_align.ops->lclose(fd, layer_block_align);

  assert(readback[0] == 'X' && readback[1] == 'Y');
  printf("✅ Cross-block 2-byte write at EOF test passed\n");
}

void test_write_cross_block_not_eof(LayerContext layer_block_align) {
  printf("Testing 2-byte write across block boundary not at EOF\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  size_t block_size = state->block_size;

  // Fill file with 3 full blocks (to ensure we're not at EOF)
  char *buffer = malloc((int)(block_size * 3));
  memset(buffer, 'Z', (int)(block_size * 3));
  write(fd, buffer, (int)(block_size * 3));
  free(buffer);

  // Write across block boundary: offset 4095 + 4096
  char data[2] = {'M', 'N'};

  ssize_t written = layer_block_align.ops->lpwrite(
      fd, data, 2, (off_t)block_size - 1, layer_block_align);
  assert(written == 2);

  char readback[2];
  pread(fd, readback, 2, (off_t)block_size - 1);
  layer_block_align.ops->lclose(fd, layer_block_align);

  assert(readback[0] == 'M' && readback[1] == 'N');
  printf("✅ Cross-block 2-byte write not at EOF test passed\n");
}

void test_write_append_two_blocks(LayerContext layer_block_align) {
  printf("Testing append two blocks\n");

  int fd = layer_block_align.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT,
                                        0666, layer_block_align);
  BlockAlignState *state = (BlockAlignState *)layer_block_align.internal_state;
  size_t block_size = state->block_size;

  struct stat stbuf;
  int result = layer_block_align.ops->lfstat(fd, &stbuf, layer_block_align);
  assert(result == 0);
  off_t size = stbuf.st_size;

  size_t num_bytes_to_write = 2 * block_size;
  char *test_message = malloc(num_bytes_to_write * sizeof(char));
  if (!test_message) {
    layer_block_align.ops->lclose(fd, layer_block_align);
    return;
  }

  memset(test_message, '9', num_bytes_to_write);
  ssize_t written = layer_block_align.ops->lpwrite(
      fd, test_message, num_bytes_to_write, size, layer_block_align);
  assert(written == num_bytes_to_write);

  char buf[num_bytes_to_write];
  pread(fd, buf, num_bytes_to_write, size);
  assert(strncmp(test_message, buf, num_bytes_to_write) == 0);

  free(test_message);
  layer_block_align.ops->lclose(fd, layer_block_align);
  printf("✅ Append two blocks test passed\n");
}

void test_block_align_calls_next_layer_ftruncate() {
  printf("Testing block_align calls next_layer_ftruncate\n");

  setup_test_with_mock_layer();

  LayerContext block_layer = block_align_init(&mock_layer, 1, mock_block_size);

  int result = block_align_ftruncate(5, 2000, block_layer);

  assert(result == 0);
  assert(mock_state.ftruncate_called == 1);
  assert(mock_state.last_ftruncate_input_fd == 5);
  assert(mock_state.last_ftruncate_input_length == 2000);
  block_align_destroy(block_layer);
  printf("✅ block_align calls next_layer_ftruncate test passed\n");
}

void test_block_align_calls_next_layer_fstat() {
  printf("Testing block_align calls next_layer_fstat\n");

  setup_test_with_mock_layer();

  LayerContext block_layer = block_align_init(&mock_layer, 1, mock_block_size);

  assert(block_layer.ops->lfstat != NULL);
  assert(block_layer.ops->llstat != NULL);
  struct stat stbuf;

  int result = block_layer.ops->lfstat(5, &stbuf, block_layer);
  assert(result == 0);
  assert(mock_state.fstat_called == 1);

  block_align_destroy(block_layer);
  printf("✅ block_align calls next_layer_fstat test passed\n");
}

void test_block_align_calls_next_layer_lstat() {
  printf("Testing block_align calls next_layer_lstat\n");

  setup_test_with_mock_layer();

  LayerContext block_layer = block_align_init(&mock_layer, 1, mock_block_size);
  assert(block_layer.ops->lfstat != NULL);
  assert(block_layer.ops->llstat != NULL);

  struct stat stbuf;
  int result = block_layer.ops->llstat(TESTPATH, &stbuf, block_layer);
  assert(result == 0);
  assert(mock_state.lstat_called == 1);

  block_align_destroy(block_layer);
  printf("✅ block_align calls next_layer_lstat test passed\n");
}

void test_write_append_flag(LayerContext l) {

  printf("Testing write with an append flag...\n");

  int fd = l.ops->lopen(TESTPATH, O_TRUNC | O_APPEND | O_WRONLY, 0666, l);
  BlockAlignState *state = (BlockAlignState *)l.internal_state;
  size_t block_size = state->block_size;
  fill_file(fd, block_size);

  struct stat stbuf;
  int result = l.ops->lfstat(fd, &stbuf, l);
  assert(result == 0);
  off_t initial_size = stbuf.st_size;
  assert(initial_size == 20480);

  char buffer[10] = "ABCDEFGHIJ";
  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 10, 20480, l);
  assert(bytes_written == 10);

  result = l.ops->lfstat(fd, &stbuf, l);
  assert(result == 0);
  off_t final_size = stbuf.st_size;
  assert(final_size == 20490);

  l.ops->lclose(fd, l);

  printf("✅ Write with append flag test passed\n");
}

void test_read_not_allowed(LayerContext l) {

  printf("Testing read without access...\n");

  int fd = l.ops->lopen(TESTPATH, O_WRONLY, 0666, l);

  char buf[2];
  assert(l.ops->lpread(fd, buf, 2, 0, l) == -1);
  assert(errno == 9);

  assert(strncmp("Bad file descriptor", strerror(errno), 20) == 0);

  l.ops->lclose(fd, l);
  printf("✅ Read without access test passed\n");
}

void test_read_empty_file(LayerContext l) {

  printf("Testing the read operation on a empty file\n");

  int fd = l.ops->lopen(TESTPATH, O_CREAT | O_TRUNC | O_RDWR, 0666, l);
  char buffer[32] = {'\0'};
  size_t bytes_read = l.ops->lpread(fd, buffer, 20, 8, l);
  assert(bytes_read == 0);
  assert(strncmp(buffer, "\0", 32) == 0);
  l.ops->lclose(fd, l);
  printf("✅ Read operation on a empty file test passed\n");
}

void test_read_with_offset_bigger_than_size(LayerContext l) {
  printf("Testing the read operation on a offset bigger than the size of the "
         "file\n");
  BlockAlignState *state = (BlockAlignState *)l.internal_state;
  size_t block_size = state->block_size;
  int fd = l.ops->lopen(TESTPATH, O_CREAT | O_TRUNC | O_RDWR, 0666, l);
  fill_file(fd, block_size);
  char *buffer = malloc(block_size);
  memset(buffer, '\0', block_size);
  size_t bytes_read = l.ops->lpread(fd, buffer, 20, 20485, l);
  assert(bytes_read == 0);
  assert(strncmp(buffer, "\0", block_size) == 0);
  l.ops->lclose(fd, l);
  free(buffer);
  printf("✅ Read operation on offset bigger than the size of the file\n");
}

LayerContext build_tree() {
  LayerContext context_local = local_init();
  LayerContext context_block_align = block_align_init(&context_local, 1, 4096);
  return context_block_align;
}

void free_tree(LayerContext *tree) {}

int main(int argc, char const *argv[]) {

  printf("Running the block_align unit tests\n");

  LayerContext tree = build_tree();

  test_write_end_file(tree);
  test_write_in_middle_and_end(tree);
  test_three_blocks(tree);
  test_read_just_1_block(tree);
  test_write_just_1_block(tree);
  test_read_out_of_bounds(tree);
  test_write_out_of_bounds(tree);
  test_read_just_1_byte(tree);
  test_write_just_1_byte(tree);
  test_write_cross_block_at_eof(tree);
  test_write_cross_block_not_eof(tree);
  test_write_append_two_blocks(tree);
  test_write_append_flag(tree);
  test_read_not_allowed(tree);
  test_read_empty_file(tree);
  test_read_with_offset_bigger_than_size(tree);

  unlink(TESTPATH);

  block_align_destroy(tree);

  // ftruncate tests with mock layer
  test_block_align_calls_next_layer_ftruncate();
  // fstat tests with mock layer
  test_block_align_calls_next_layer_fstat();
  // lstat tests with mock layer
  test_block_align_calls_next_layer_lstat();

  printf("🎉 All block_align unit tests passed!\n");

  return 0;
}
