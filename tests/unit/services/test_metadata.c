#include <assert.h>
#include <services/metadata.h>
#include <stdio.h>
#include <string.h>

void test_put_get_delete_get() {
  printf("Testing put -> get -> delete -> get \n");

  metadata_init(NULL);

  char *key = "key_test";
  char *value = "test";

  int ret = metadata_put(key, strlen(key), value, strlen(value) + 1);
  assert(ret == 0);

  size_t value_size;
  char *got = metadata_get(key, strlen(key), &value_size);
  assert(got != NULL);
  assert(strcmp(got, value) == 0);
  metadata_free(got);

  ret = metadata_delete(key, strlen(key));
  assert(ret == 0);

  got = metadata_get(key, strlen(key), &value_size);
  assert(got == NULL);
  metadata_free(got);
  metadata_close();

  printf("✅ Put -> get -> delete -> get test passed\n");
}

void test_get_existing_key() {
  printf("Testing get with existing key\n");

  metadata_init(NULL);

  char *key = "key_test";
  char *value = "test";
  size_t value_size;

  int ret = metadata_put(key, strlen(key), value, strlen(value) + 1);
  assert(ret == 0);

  char *got = metadata_get(key, strlen(key), &value_size);
  assert(got != NULL);
  assert(strcmp(got, value) == 0);
  metadata_free(got);

  metadata_close();

  printf("✅ Get with existing key test passed\n");
}

void test_get_non_existing_key() {
  printf("Testing get non-existing key\n");

  metadata_init(NULL);

  char *key = "key_does_not_exist";
  size_t value_size;

  char *got = metadata_get(key, strlen(key), &value_size);
  assert(got == NULL);

  metadata_close();

  printf("✅ Get non-existing key test passed\n");
}

void test_put_only() {
  printf("Testing put \n");

  metadata_init(NULL);

  char *key = "key_test";
  char *value = "test";

  int ret = metadata_put(key, strlen(key), value, strlen(value) + 1);
  assert(ret == 0);

  metadata_close();

  printf("✅ Put test passed\n");
}

int main(int argc, char *argv[]) {

  printf("Running the metadata service unit tests\n");
  test_put_get_delete_get();
  test_get_non_existing_key();
  test_put_only();
  test_get_existing_key();
  printf("🎉 All metadata service unit tests passed!\n");

  return 0;
}
