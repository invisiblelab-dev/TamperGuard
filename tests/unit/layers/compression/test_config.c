#include "../../../../layers/compression/config.h"
#include "../../../../lib/tomlc17/src/tomlc17.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void test_zstd_parsing() {
  printf("Testing ZSTD parsing...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "algorithm = \"zstd\"\n"
                         "level = 5\n"
                         "mode = \"file\"\n"
                         "next = \"layer_2\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  CompressionConfig config = {0};
  compression_parse_params(layer, &config);

  assert(config.algorithm == COMPRESSION_ZSTD);
  assert(config.level == 5);
  if (config.next_layer != NULL) {
    free(config.next_layer);
  }
  toml_free(result);

  printf("✅ ZSTD parsing test passed\n");
}

void test_lz4_parsing() {
  printf("Testing LZ4 parsing...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "algorithm = \"lz4\"\n"
                         "level = 9\n"
                         "mode = \"file\"\n"
                         "next = \"layer_2\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  CompressionConfig config = {0};
  compression_parse_params(layer, &config);

  assert(config.algorithm == COMPRESSION_LZ4);
  assert(config.level == 9);

  if (config.next_layer != NULL) {
    free(config.next_layer);
  }
  toml_free(result);
  printf("✅ LZ4 parsing test passed\n");
}

void test_invalid_algorithm_panics() {
  printf("Testing invalid algorithm...\n");

  const char *toml_str =
      "[layer_1]\n"
      "type = \"compression\"\n"
      "algorithm = \"invalid_algorithm\"\n" // Invalid algorithm
      "level = 5\n"
      "mode = \"file\"\n"
      "next = \"layer_2\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  // Fork to catch the exit
  pid_t pid = fork();
  if (pid == 0) {
    CompressionConfig config;
    compression_parse_params(layer, &config);
    // This line should never be reached - compression_parse_params should
    // exit(1) first
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status));        // Assert that the child called exit()
    assert(WEXITSTATUS(status) == 1); // Assert that the child called exit(1)
  }

  toml_free(result);
  printf("✅ Invalid algorithm test passed\n");
}

void test_invalid_level_panics() {
  printf("Testing invalid level...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "algorithm = \"zstd\"\n"
                         "level = \"invalid_level\"\n"
                         "mode = \"file\"\n"
                         "next = \"layer_2\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  // Fork to catch the exit
  pid_t pid = fork();
  if (pid == 0) {
    CompressionConfig config;
    compression_parse_params(layer, &config);
    // This line should never be reached - compression_parse_params should
    // exit(1) first
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status));        // Assert that the child called exit()
    assert(WEXITSTATUS(status) == 1); // Assert that the child called exit(1)
  }

  toml_free(result);
  printf("✅ Invalid level test passed\n");
}

void test_invalid_next_layer_panics() {
  printf("Testing invalid next layer...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "algorithm = \"zstd\"\n"
                         "mode = \"file\"\n"
                         "level = 5\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  // Fork to catch the exit
  pid_t pid = fork();
  if (pid == 0) {
    CompressionConfig config;
    compression_parse_params(layer, &config);
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status));        // Assert that the child called exit()
    assert(WEXITSTATUS(status) == 1); // Assert that the child called exit(1)
  }

  toml_free(result);
  printf("✅ Invalid next layer test passed\n");
}

void test_invalid_mode_panics() {
  printf("Testing invalid mode...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "next = \"layer_2\"\n"
                         "algorithm = \"zstd\"\n"
                         "level = 5\n"
                         "mode = \"invalid_mode\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  // Fork to catch the exit
  pid_t pid = fork();
  if (pid == 0) {
    CompressionConfig config;
    compression_parse_params(layer, &config);
    exit(0);
  } else {
    int status;
    waitpid(pid, &status, 0); // Wait for child process to terminate

    assert(WIFEXITED(status));        // Assert that the child called exit()
    assert(WEXITSTATUS(status) == 1); // Assert that the child called exit(1)
  }

  toml_free(result);
  printf("✅ Invalid mode test passed\n");
}

void test_block_size_and_mode_parsing() {
  printf("Testing block size and mode parsing...\n");

  // FIRST CONFIG
  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "next = \"layer_2\"\n"
                         "algorithm = \"zstd\"\n"
                         "level = 5\n"
                         "mode = \"sparse_block\"\n"
                         "block_size = 8192\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  CompressionConfig config;
  compression_parse_params(layer, &config);
  assert(config.mode == COMPRESSION_MODE_SPARSE_BLOCK);
  assert(config.block_size == 8192);

  if (config.next_layer) {
    free(config.next_layer);
    config.next_layer = NULL;
  }

  toml_free(result);

  // SECOND CONFIG
  toml_str = "[layer_1]\n"
             "type = \"compression\"\n"
             "next = \"layer_2\"\n"
             "algorithm = \"zstd\"\n"
             "level = 5\n"
             "mode = \"file\"\n";

  result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  table = result.toptab;
  layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  compression_parse_params(layer, &config);
  assert(config.mode == COMPRESSION_MODE_FILE);
  assert(config.block_size == 4096); // Default value

  if (config.next_layer) {
    free(config.next_layer);
    config.next_layer = NULL;
  }

  toml_free(result);
  printf("✅ Block size and mode parsing test passed\n");
}

int main() {
  printf("Running compression/config.c tests...\n");

  test_zstd_parsing();
  test_lz4_parsing();
  test_invalid_algorithm_panics();
  test_invalid_level_panics();
  test_invalid_mode_panics();
  test_block_size_and_mode_parsing();

  printf("🎉 All compression parsing tests passed!\n");
  return 0;
}
