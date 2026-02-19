#include "../../../../layers/block_align/config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_block_size_default_value() {
  printf("Testing block size default value...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "next = \"layer2\"\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  BlockAlignConfig config;
  block_align_parse_params(layer, &config);
  assert(config.block_size == 4096);

  printf("✅ Default value test passed\n");
  free(config.next_Layer);
  toml_free(result);
}

void test_block_size_parsing() {
  printf("Testing block size parsing...\n");

  const char *toml_str = "[layer_1]\n"
                         "type = \"compression\"\n"
                         "next = \"layer2\"\n"
                         "block_size = 8192\n";

  toml_result_t result = toml_parse(toml_str, (int)strlen(toml_str));
  assert(result.ok);

  toml_datum_t table = result.toptab;
  toml_datum_t layer = table.u.tab.value[0];
  assert(layer.type == TOML_TABLE);

  BlockAlignConfig config;
  block_align_parse_params(layer, &config);
  assert(config.block_size == 8192);

  printf("✅ Block size parsing test passed\n");
  free(config.next_Layer);
  toml_free(result);
}

int main() {
  printf("Running block_align/config.c tests...\n");

  test_block_size_default_value();
  test_block_size_parsing();

  printf("🎉 All block_align parsing tests passed!\n");
  return 0;
}
