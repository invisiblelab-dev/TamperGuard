/**
 * WARNING: All the tests consider a block size of 16.
 * Any other value can cause some tests to fail and also make some tests lose
 * their purpose, because, for example, the block frontiers change.
 */

#include "../../../../layers/block_align/block_align.h"
#include "../../../../layers/cache/read_cache/read_cache.h"
#include "../../../../layers/local/local.h"
#include "types/layer_context.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#define TESTPATH "test_file.txt"

void fill_file(int fd, size_t block_size) {
  char *buffer = malloc(block_size * sizeof(char));
  int i, j;

  for (i = 0; i < 5; i++) {
    char c = (char)(i + '0'); // only works with i < 9
    for (j = 0; j < block_size; j++)
      buffer[j] = c;
    write(fd, buffer, block_size * sizeof(char));
  }

  free(buffer);
}

void test_read_single_block_middle_of_file(LayerContext l) {

  printf("Testing reading a single block in the middle of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[5] = {'\0'};

  // 1st read (cache miss)
  ssize_t bytes_read = l.ops->lpread(fd, buffer, 5, 8, l);

  int r = bytes_read == 5, i;

  for (i = 0; i < 5 && r; i++)
    r = buffer[i] == '0';

  assert(r);

  // 2nd read (cache hit)
  for (i = 0; i < 5; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 5, 8, l);

  r = bytes_read == 5;

  for (i = 0; i < 5 && r; i++)
    r = buffer[i] == '0';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Reading a single block in the middle of the file test passed\n");
}

void test_read_2_blocks_middle_of_file(LayerContext l) {
  printf("Testing reading 2 blocks in the middle of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[12] = {'\0'};

  // 1st read (cache miss)

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 12, 25, l);

  int r = bytes_read == 12, i;

  for (i = 0; i < 12 && r; i++) {
    if (i < 7)
      r = buffer[i] == '1';
    else
      r = buffer[i] == '2';
  }

  assert(r);

  // 2nd read (cache hit)

  for (i = 0; i < 12; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 12, 25, l);

  r = bytes_read == 12;

  for (i = 0; i < 12 && r; i++) {
    if (i < 7)
      r = buffer[i] == '1';
    else
      r = buffer[i] == '2';
  }

  l.ops->lclose(fd, l);

  printf("✅ Reading 2 blocks in the middle of the file test passed\n");
}

void test_read_3_blocks_middle_and_end_of_file(LayerContext l) {
  printf(
      "Testing reading 3 blocks in the middle crossing the end of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[40] = {'\0'};

  // 1st read (cache miss)

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 40, 45, l);

  assert(bytes_read == 35);

  assert(strncmp("22233333333333333334444444444444444", buffer, 35) == 0);

  int r = 1, i;

  for (i = 36; i < 41 && r; i++)
    r = buffer[i] == '\0';

  assert(r);

  // 2nd read (cache hit)

  for (i = 0; i < 36; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 40, 45, l);

  assert(bytes_read == 35);

  assert(strncmp("22233333333333333334444444444444444", buffer, 35) == 0);

  for (i = 36; i < 41 && r; i++)
    r = buffer[i] == '\0';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Reading 3 blocks in the middle and crossing the end of the file "
         "test passed\n");
}

void test_cross_block_read(LayerContext l) {
  printf("Testing cross-block 2-byte read (1 byte in each block) \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size); // 5 blocks, 16 bytes each

  char buffer[2] = {'\0'};

  // Offset 15 = last byte of block 0, next byte is in block 1
  ssize_t bytes_read = l.ops->lpread(fd, buffer, 2, 15, l);

  assert(bytes_read == 2);
  assert(buffer[0] == '0');
  assert(buffer[1] == '1');

  // Cache hit
  buffer[0] = buffer[1] = '\0';
  bytes_read = l.ops->lpread(fd, buffer, 2, 15, l);
  assert(bytes_read == 2);
  assert(buffer[0] == '0');
  assert(buffer[1] == '1');

  l.ops->lclose(fd, l);

  printf("✅ Cross-block 2-byte read test passed\n");
}

void test_read_beyond_eof(LayerContext l) {
  printf("Testing read beyond the end of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size); // 5 blocks => 80 bytes

  char buffer[10] = {'\0'};

  // Try to read 10 bytes from byte 78 (there's only 2 bytes to read)
  ssize_t bytes_read = l.ops->lpread(fd, buffer, 10, 78, l);

  assert(bytes_read == 2);
  assert(buffer[0] == '4');
  assert(buffer[1] == '4');

  // Check if the other 8 bytes were not modified
  for (int i = 2; i < 10; i++)
    assert(buffer[i] == '\0');

  // Cache hit
  buffer[0] = buffer[1] = '\0';
  bytes_read = l.ops->lpread(fd, buffer, 10, 78, l);
  assert(bytes_read == 2);
  assert(buffer[0] == '4');
  assert(buffer[1] == '4');

  // Check if the other 8 bytes were not modified
  for (int i = 2; i < 10; i++)
    assert(buffer[i] == '\0');

  l.ops->lclose(fd, l);

  printf("✅ Read beyond end of file test passed\n");
}

void test_write_single_block_middle_of_file(LayerContext l) {

  printf("Testing writing a single block in the middle of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[5] = {'\0'};

  // read first to check if cached blocks are being updated

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 5, 20, l);

  int r = bytes_read == 5, i;

  for (i = 0; i < 5 && r; i++)
    r = buffer[i] == '1';

  assert(r);

  // write (updates the cached block)

  for (i = 0; i < 5; i++)
    buffer[i] = '9';

  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 5, 18, l);

  r = bytes_written == 5;

  assert(r);

  // 2nd read (cache hit with updated content)

  for (i = 0; i < 5; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 5, 20, l);

  assert(bytes_read == 5);

  assert(strncmp("99911", buffer, 5) == 0);

  l.ops->lclose(fd, l);

  printf("✅ Writing a single block in the middle of the file test passed\n");
}

void test_write_2_blocks_middle_of_file(LayerContext l) {

  printf("Testing writing 2 blocks in the middle of the file \n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[10] = {'\0'};

  // read first to check if cached blocks are being updated

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 10, 40, l);

  assert(bytes_read == 10);

  assert(strncmp("2222222233", buffer, 10) == 0);

  // write (updates the cached block)
  for (int i = 0; i < 10; i++)
    buffer[i] = '9';

  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 10, 42, l);

  assert(bytes_written == 10);

  // 2nd read (cache hit with updated content)

  for (int i = 0; i < 10; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 10, 44, l);

  assert(bytes_read == 10);

  assert(strncmp("9999999933", buffer, 10) == 0);

  l.ops->lclose(fd, l);

  printf("✅ Writing 2 blocks in the middle of the file test passed\n");
}

void test_write_to_last_and_new_block(LayerContext l) {

  printf("Testing writing 2 bytes, 1 in the end of the file and append 1\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  struct stat stbuf;
  assert(l.ops->lfstat(fd, &stbuf, l) != -1);
  off_t size = stbuf.st_size;

  char buffer[10] = {'\0'};
  ssize_t bytes_read = l.ops->lpread(fd, buffer, 10, size - 5, l);

  assert(bytes_read == 5);
  assert(strncmp("44444", buffer, 5) == 0);

  ssize_t bytes_written = l.ops->lpwrite(fd, "99", 2, size - 1, l);

  assert(bytes_written == 2);

  for (int i = 0; i < 10; i++) {
    buffer[i] = '\0';
  }

  bytes_read = l.ops->lpread(fd, buffer, 2, size - 1, l);

  assert(bytes_read == 2);
  assert(strncmp("99", buffer, 2) == 0);
  assert(l.ops->lfstat(fd, &stbuf, l) != -1);
  size_t new_size = stbuf.st_size;
  assert(new_size == size + 1);

  l.ops->lclose(fd, l);
  printf("✅ Writing 2 bytes, 1 in the end of the file and append 1 test "
         "passed \n");
}

void test_write_new_content(LayerContext l) {

  printf("Testing writing new content\n");

  int fd =
      l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT | O_APPEND, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[20] = {'\0'};

  struct stat stbuf;
  assert(l.ops->lfstat(fd, &stbuf, l) != -1);
  off_t offset = stbuf.st_size;

  // read first to check if cached blocks are being updated

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 20, offset, l);

  assert(bytes_read == 0);

  // write (updates the cached block)
  for (int i = 0; i < 20; i++)
    buffer[i] = '9';

  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 20, 0, l);

  assert(bytes_written == 20);

  // 2nd read (cache hit with updated content)

  for (int i = 0; i < 20; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 20, offset, l);

  assert(bytes_read == 20);

  int r = 1;
  for (int i = 0; i < 20 && r; i++)
    r = buffer[i] == '9';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Writing new content test passed\n");
}

void test_ftruncate_shortening(LayerContext l) {

  printf("Testing the shortening case of the ftruncate function\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[5] = {'\0'};

  // read to put a block in cache

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 5, 70, l);

  assert(bytes_read == 5);

  assert(strncmp("44444", buffer, 5) == 0);

  // truncate file, removing cached entries

  l.ops->lftruncate(fd, 72, l);

  // read again, this time expecting only 2 bytes

  for (int i = 0; i < 5; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 5, 70, l);

  assert(bytes_read == 2);

  assert(strncmp("44", buffer, 2) == 0);
  l.ops->lclose(fd, l);

  printf("✅ Shortening case of truncating file test passed\n");
}

void test_ftruncate_lengthening(LayerContext l) {

  printf("Testing the lengthening case of the ftruncate function\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[10] = {'\0'};

  // read to put a block in cache

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 10, 75, l);

  assert(bytes_read == 5);

  assert(strncmp("44444", buffer, 5) == 0);

  // truncate file, updating cached entries

  l.ops->lftruncate(fd, 90, l);

  // read again, this time expecting 10 bytes

  for (int i = 0; i < 10; i++)
    buffer[i] = '9';

  bytes_read = l.ops->lpread(fd, buffer, 10, 75, l);

  assert(bytes_read == 10);

  int r = 1, i;
  for (i = 0; i < 5 && r; i++)
    r = buffer[i] == '4';
  for (i = 5; i < 10 && r; i++)
    r = buffer[i] == '\0';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Lengthening case of truncating file test passed\n");
}

void test_write_to_2_bytes_in_2_blocks(LayerContext l) {
  printf("Testing writing 2 bytes, one in each block\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[2] = {'\0'};

  // read first to check if cached blocks are being updated

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 2, 47, l);

  assert(bytes_read == 2);

  assert(buffer[0] == '2');
  assert(buffer[1] == '3');

  // write (updates the cached block)
  buffer[0] = '9';
  buffer[1] = '9';

  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 2, 47, l);

  assert(bytes_written == 2);

  // 2nd read (cache hit with updated content)

  buffer[0] = '\0';
  buffer[1] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 2, 47, l);

  assert(bytes_read == 2);

  assert(buffer[0] == '9');
  assert(buffer[1] == '9');

  l.ops->lclose(fd, l);

  printf("✅ Writing 2 bytes, one in each block test passed\n");
}

void test_various_multiblock_rds_wrs(LayerContext l) {
  printf("Testing reading 2 blocks, modifying 3 and reading 4\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[64] = {'\0'};

  // read blocks 0 and 1

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 30, 2, l);

  assert(bytes_read == 30);

  int r = 1, i;
  for (i = 0; i < 14 && r; i++)
    r = buffer[i] == '0';
  for (i = 14; i < 30 && r; i++)
    r = buffer[i] == '1';

  // write blocks 0, 1 and 2
  for (i = 0; i < 35; i++)
    buffer[i] = '9';

  ssize_t bytes_written = l.ops->lpwrite(fd, buffer, 35, 10, l);

  assert(bytes_written == 35);

  // read blocks 0, 1, 2 and 3

  for (i = 0; i < 35; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 60, 0, l);

  assert(bytes_read == 60);

  for (i = 0; i < 10 && r; i++)
    r = buffer[i] == '0';
  for (i = 10; i < 45 && r; i++)
    r = buffer[i] == '9';
  for (i = 45; i < 48 && r; i++)
    r = buffer[i] == '2';
  for (i = 48; i < 60 && r; i++)
    r = buffer[i] == '3';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Various multiblock operations test passed\n");
}

void test_cache_misses_grouping(LayerContext l) {
  printf("Testing the cache misses grouping mechanism\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[60] = {'\0'};

  // read from block 0 to cache it

  ssize_t bytes_read = l.ops->lpread(fd, buffer, 10, 2, l);

  assert(bytes_read == 10);

  assert(strncmp("0000000000", buffer, 10) == 0);

  // read from block 3 to cache it

  for (int i = 0; i < 10; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 10, 50, l);

  assert(bytes_read == 10);

  assert(strncmp("3333333333", buffer, 10) == 0);

  // now read blocks 0, 1, 2 and 3 at once to check if blocks 2 and 3 are being
  // correctly read

  for (int i = 0; i < 10; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 60, 1, l);

  assert(bytes_read == 60);

  int r = 1, i;
  for (i = 0; i < 15 && r; i++)
    r = buffer[i] == '0';
  for (i = 15; i < 31 && r; i++)
    r = buffer[i] == '1';
  for (i = 31; i < 47 && r; i++)
    r = buffer[i] == '2';
  for (i = 47; i < 60 && r; i++)
    r = buffer[i] == '3';

  assert(r);

  // finally, read blocks 1 and 2 to check if they were correctly cached

  for (i = 0; i < 60; i++)
    buffer[i] = '\0';

  bytes_read = l.ops->lpread(fd, buffer, 25, 19, l);

  assert(bytes_read == 25);

  for (i = 0; i < 13 && r; i++)
    r = buffer[i] == '1';
  for (i = 13; i < 25 && r; i++)
    r = buffer[i] == '2';

  assert(r);

  l.ops->lclose(fd, l);

  printf("✅ Cache misses grouping mechanism test passed\n");
}

void test_unlink_with_no_fds_open(LayerContext l) {
  printf("Testing the unlink operation with no fds opened\n");
  int fd = l.ops->lopen(TESTPATH, O_TRUNC | O_CREAT | O_RDWR, 0666, l);
  ReadCacheState *state = (ReadCacheState *)l.internal_state;
  fill_file(fd, state->block_size);
  char buffer[48] = {'\0'};
  char not_cached_buffer[48] = {'\0'};
  size_t bytes_read = l.ops->lpread(fd, buffer, 40, 0, l);
  assert(bytes_read == 40);
  l.ops->lclose(fd, l);

  l.ops->lunlink(TESTPATH, l);
  fd = l.ops->lopen(TESTPATH, O_CREAT | O_RDWR, 0666, l);
  bytes_read = l.ops->lpread(fd, not_cached_buffer, 40, 0, l);
  assert(bytes_read == 0);
  assert(strncmp(buffer, not_cached_buffer, 48) != 0);
  l.ops->lclose(fd, l);
  printf("✅ Unlink operation with no fds opened test passed\n");
}

void test_unlink_opened_file(LayerContext l) {
  printf("Testing unlink with an opened file\n");

  int fd = l.ops->lopen(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666, l);
  ReadCacheState *rcs = (ReadCacheState *)l.internal_state;
  fill_file(fd, rcs->block_size);

  char buffer[30] = {'\0'};

  // read some content to cache it
  ssize_t bytes_read = l.ops->lpread(fd, buffer, 30, 0, l);
  assert(bytes_read == 30);
  int r = 1, i;
  for (i = 0; i < 16; i++)
    r = buffer[i] == '0';
  for (i = 16; i < 30; i++)
    r = buffer[i] == '1';
  assert(r);

  // unlink the file with the fd still opened
  int res = l.ops->lunlink(TESTPATH, l);
  assert(res != -1);

  // create another file for the same path
  int fd2 = l.ops->lopen(TESTPATH, O_RDWR | O_CREAT, 0666, l);

  // read from the new file, which should return 0
  for (int i = 0; i < 30; i++)
    buffer[i] = '\0';
  bytes_read = l.ops->lpread(fd2, buffer, 30, 0, l);
  assert(bytes_read == 0);

  // read from the unlinked file, which should return the original 30
  bytes_read = l.ops->lpread(fd, buffer, 30, 0, l);
  assert(bytes_read == 30);
  for (i = 0; i < 16; i++)
    r = buffer[i] == '0';
  for (i = 16; i < 30; i++)
    r = buffer[i] == '1';
  assert(r);

  l.ops->lclose(fd, l);
  l.ops->lclose(fd2, l);

  printf("✅ Unlinking opened file test passed\n");
}

LayerContext build_tree() {
  LayerContext context_local = local_init();
  LayerContext context_read_cache = read_cache_init(&context_local, 1, 16, 10);
  LayerContext context_block_align =
      block_align_init(&context_read_cache, 1, 16);

  return context_block_align;
}

int main() {

  printf("Running the read_cache unit tests\n");

  LayerContext tree = build_tree();
  test_read_single_block_middle_of_file(tree);
  test_read_2_blocks_middle_of_file(tree);
  test_read_3_blocks_middle_and_end_of_file(tree);
  test_cross_block_read(tree);
  test_read_beyond_eof(tree);
  test_write_single_block_middle_of_file(tree);
  test_write_2_blocks_middle_of_file(tree);
  test_write_to_last_and_new_block(tree);
  test_write_new_content(tree);
  test_ftruncate_shortening(tree);
  test_ftruncate_lengthening(tree);
  test_write_to_2_bytes_in_2_blocks(tree);
  test_various_multiblock_rds_wrs(tree);
  test_cache_misses_grouping(tree);
  test_unlink_with_no_fds_open(tree);
  test_unlink_opened_file(tree);

  unlink(TESTPATH);

  block_align_destroy(tree);

  printf("🎉 All read_cache unit tests passed!\n");

  return 0;
}
