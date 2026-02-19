#include "../../../../layers/local/local.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Test helper function to create a temporary file with content
// Returns the file descriptor and stores the path in temp_path_out
int create_temp_file(const char *content, size_t content_size,
                     char *temp_path_out) {
  char temp_path[] = "/tmp/test_ftruncate_XXXXXX";
  int fd = mkstemp(temp_path);
  if (fd == -1) {
    perror("mkstemp failed");
    return -1;
  }

  // Copy the actual path to the output parameter
  strcpy(temp_path_out, temp_path);

  if (content && content_size > 0) {
    ssize_t written = write(fd, content, content_size);
    if (written != (ssize_t)content_size) {
      perror("write failed");
      close(fd);
      unlink(temp_path);
      return -1;
    }
  }

  return fd;
}

// Test helper function to get file size
off_t get_file_size(int fd) {
  struct stat st;
  if (fstat(fd, &st) == -1) {
    return -1;
  }
  return st.st_size;
}

void test_ftruncate_success() {
  printf("Testing successful ftruncate...\n");

  char temp_path[256];
  // Create a temporary file with some content
  const char *content = "Hello, World! This is test content.";
  size_t content_size = strlen(content);
  int fd = create_temp_file(content, content_size, temp_path);
  assert(fd != -1);

  // Verify initial size
  off_t initial_size = get_file_size(fd);
  assert(initial_size == (off_t)content_size);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test truncating to a smaller size
  off_t new_size = 10;
  int result = local_ftruncate(fd, new_size, ctx);
  assert(result == 0);

  // Verify the file was truncated
  off_t final_size = get_file_size(fd);
  assert(final_size == new_size);

  // Clean up immediately after test
  close(fd);
  unlink(temp_path);
  printf("✅ Successful ftruncate test passed\n");
}

void test_ftruncate_extend() {
  printf("Testing ftruncate to extend file...\n");

  char temp_path[256];
  // Create a temporary file with some content
  const char *content = "Hello";
  size_t content_size = strlen(content);
  int fd = create_temp_file(content, content_size, temp_path);
  assert(fd != -1);

  // Verify initial size
  off_t initial_size = get_file_size(fd);
  assert(initial_size == (off_t)content_size);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test extending the file
  off_t new_size = 100;
  int result = local_ftruncate(fd, new_size, ctx);
  assert(result == 0);

  // Verify the file was extended
  off_t final_size = get_file_size(fd);
  assert(final_size == new_size);

  // Clean up immediately after test
  close(fd);
  unlink(temp_path);
  printf("✅ File extension test passed\n");
}

void test_ftruncate_zero() {
  printf("Testing ftruncate to zero...\n");

  char temp_path[256];
  // Create a temporary file with some content
  const char *content = "Some content to truncate";
  size_t content_size = strlen(content);
  int fd = create_temp_file(content, content_size, temp_path);
  assert(fd != -1);

  // Verify initial size
  off_t initial_size = get_file_size(fd);
  assert(initial_size == (off_t)content_size);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test truncating to zero
  int result = local_ftruncate(fd, 0, ctx);
  assert(result == 0);

  // Verify the file was truncated to zero
  off_t final_size = get_file_size(fd);
  assert(final_size == 0);

  // Clean up immediately after test
  close(fd);
  unlink(temp_path);
  printf("✅ Zero truncation test passed\n");
}

void test_ftruncate_invalid_fd() {
  printf("Testing ftruncate with invalid file descriptor...\n");

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test with invalid file descriptor
  int result = local_ftruncate(-1, 100, ctx);
  assert(result == -1);

  printf("✅ Invalid file descriptor test passed\n");
}

void test_ftruncate_negative_length() {
  printf("Testing ftruncate with negative length...\n");

  off_t size = 4;
  char temp_path[256];
  // Create a temporary file
  int fd = create_temp_file("test", size, temp_path);
  assert(fd != -1);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test with negative length (should fail)
  int result = local_ftruncate(fd, -1, ctx);
  assert(result == -1);

  off_t final_size = get_file_size(fd);
  assert(final_size == size);

  // Clean up immediately after test
  close(fd);
  unlink(temp_path);
  printf("✅ Negative length test passed\n");
}

void test_ftruncate_readonly_file() {
  printf("Testing ftruncate on read-only file...\n");

  // Create a temporary file
  char temp_path[] = "/tmp/test_ftruncate_ro_XXXXXX";
  int fd = mkstemp(temp_path);
  assert(fd != -1);

  // Write some content
  const char *content = "test content";
  write(fd, content, strlen(content));

  // Close and reopen as read-only
  close(fd);
  fd = open(temp_path, O_RDONLY);
  assert(fd != -1);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test truncating read-only file (should fail)
  int result = local_ftruncate(fd, 0, ctx);
  assert(result == -1);

  // Clean up immediately after test
  close(fd);
  unlink(temp_path);
  printf("✅ Read-only file test passed\n");
}

void test_fstat_success() {
  printf("Testing fstat success...\n");

  char temp_path[256];
  int fd = create_temp_file("test", 4, temp_path);
  assert(fd != -1);
  close(fd);

  // Initialize layer context
  LayerContext ctx = local_init();

  fd = local_open(temp_path, O_RDWR | O_CREAT, 0644, ctx);
  assert(fd != -1);

  struct stat st;
  int result = local_fstat(fd, &st, ctx);
  assert(result == 0);

  // Verify the file size
  assert(st.st_size == 4);
  assert((st.st_mode & S_IFMT) == S_IFREG);
  assert(st.st_nlink == 1);
  assert(st.st_uid >= 0);
  assert(st.st_gid >= 0);
  assert(st.st_atime > 0);
  assert(st.st_mtime > 0);
  assert(st.st_ctime > 0);
  local_close(fd, ctx);
  unlink(temp_path);
  printf("✅ fstat test passed\n");
}

void test_lstat_success() {
  printf("Testing lstat success...\n");

  char temp_path[256];
  int fd = create_temp_file("test", 4, temp_path);
  assert(fd != -1);
  close(fd);

  // Initialize layer context
  LayerContext ctx = local_init();

  struct stat st;
  int result = local_lstat(temp_path, &st, ctx);
  assert(result == 0);

  assert(st.st_size == 4);
  assert((st.st_mode & S_IFMT) == S_IFREG);
  assert(st.st_nlink == 1);
  assert(st.st_uid >= 0);
  assert(st.st_gid >= 0);
  assert(st.st_atime > 0);
  assert(st.st_mtime > 0);
  assert(st.st_ctime > 0);

  // Clean up immediately after test
  local_close(fd, ctx);
  unlink(temp_path);
  printf("✅ lstat test passed\n");
}

// This test is for the case where the file is a symlink
// the difference between lstat and stat is that lstat does not follow the
// symlink and stat does https://pubs.opengroup.org/onlinepubs/9799919799/
void test_lstat_symlink() {
  printf("Testing lstat with symlink...\n");

  char temp_path[256];
  int fd = create_temp_file("target content", 13, temp_path);
  assert(fd != -1);
  close(fd);

  // Create a symlink to the file
  char symlink_path[] = "/tmp/test_symlink_XXXXXX";
  int symlink_fd = mkstemp(symlink_path);
  close(symlink_fd);
  unlink(symlink_path); // Remove the temp file, keep the path

  int result = symlink(temp_path, symlink_path);
  assert(result == 0);

  // Initialize layer context
  LayerContext ctx = local_init();

  // Test lstat on symlink (should return symlink info, not target info)
  struct stat st;
  result = local_lstat(symlink_path, &st, ctx);
  assert(result == 0);

  // Verify it's a symlink, not a regular file
  assert((st.st_mode & S_IFMT) == S_IFLNK);
  assert(st.st_size > 0); // Size of the symlink path string

  // Clean up
  unlink(symlink_path);
  unlink(temp_path);
  printf("✅ lstat symlink test passed\n");
}

void test_unlink_success() {
  printf("Testing unlink success...\n");

  // Guarantee that the file does not exist before the test
  unlink("/tmp/test_unlink");

  LayerContext ctx = local_init();

  int fd = ctx.ops->lopen("/tmp/test_unlink", O_CREAT | O_RDWR, 0644, ctx);
  assert(fd >= 0);

  ctx.ops->lclose(fd, ctx); // Closes the file to allow its removal

  int res = ctx.ops->lunlink("/tmp/test_unlink", ctx);
  assert(res == 0);

  fd = ctx.ops->lopen("/tmp/test_unlink", O_RDWR, 0644,
                      ctx); // Try to acess the removed file
  assert(fd == -1);
  assert(
      errno ==
      ENOENT); // Confirm the errno to see if the error is the file not existing

  free(ctx.ops);
  printf("✅ unlink success test passed\n");
}

int main() {
  printf("Running local layer tests...\n");
  printf("Running ftruncate tests...\n");

  test_ftruncate_success();
  test_ftruncate_extend();
  test_ftruncate_zero();
  test_ftruncate_invalid_fd();
  test_ftruncate_negative_length();
  test_ftruncate_readonly_file();
  test_fstat_success();
  test_lstat_success();
  test_lstat_symlink();
  test_unlink_success();

  printf("All local_ftruncate tests passed!\n");
  printf("\n🎉All local layer tests passed!\n");
  return 0;
}
