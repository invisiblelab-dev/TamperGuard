#include "../../../../../shared/utils/compressor/compressor.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#define ZSTD_STATIC_LINKING_ONLY
#include "../../../../../lib/zstd/lib/zstd.h"

// Test data
static const char test_data[] =
    "This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog. This is a test string for compression. "
    "It contains repeated patterns like 'test' and 'compression' "
    "to make it more compressible. The quick brown fox jumps over "
    "the lazy dog.";

static const size_t test_data_size =
    sizeof(test_data) - 1; // Exclude null terminator

// Helper function to create a buffer for compressed data and return the size of
// the buffer calculated with get_compress_bound
static void *create_compress_buffer(size_t original_size,
                                    const Compressor *compressor,
                                    size_t *out_buffer_size) {
  // Use a generous buffer size for compression
  size_t compress_bound =
      compressor->get_compress_bound(original_size, compressor->level);
  if (out_buffer_size) {
    *out_buffer_size = compress_bound;
  }
  return malloc(compress_bound);
}

// ============================================================================
// LZ4 COMPRESSOR TESTS
// ============================================================================

static void test_lz4_init() {
  printf("Testing LZ4 initialization...\n");

  Compressor compressor;
  int result = compressor_init(&compressor, COMPRESSION_LZ4, 5);

  assert(result == 0);
  assert(compressor.algorithm == COMPRESSION_LZ4);
  assert(compressor.level == 5);
  assert(compressor.compress_data != NULL);
  assert(compressor.decompress_data != NULL);

  printf("✅ LZ4 initialization passed\n");
}

static void test_lz4_compression_round_trip() {
  printf("Testing LZ4 compression round-trip...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 5);

  // Create buffers
  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);
  void *decompressed_buffer = malloc(test_data_size);

  assert(compress_buffer != NULL);
  assert(decompressed_buffer != NULL);
  assert(compress_bound > 0);

  // Compress
  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  assert(compressed_size > 0);
  assert(compressed_size <= compress_bound);

  // Decompress
  size_t decompressed_size = test_data_size;
  ssize_t decompressed_result =
      compressor.decompress_data(compress_buffer, compressed_size,
                                 decompressed_buffer, &decompressed_size);
  assert(decompressed_result == test_data_size);
  assert(decompressed_size == test_data_size);

  // Verify data integrity
  assert(memcmp(test_data, decompressed_buffer, test_data_size) == 0);

  // Cleanup
  free(compress_buffer);
  free(decompressed_buffer);

  printf("✅ LZ4 compression round-trip passed\n");
}

static void test_lz4_edge_cases() {
  printf("Testing LZ4 edge cases...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 5);

  // Test empty data
  size_t empty_buffer_size = 0;
  void *empty_buffer =
      create_compress_buffer(0, &compressor, &empty_buffer_size);
  ssize_t result = compressor.compress_data(
      "", 0, empty_buffer, empty_buffer_size, compressor.level);
  assert(result > 0);
  free(empty_buffer);

  // Test single byte compression
  char single_byte = 'A';
  size_t single_buffer_size = 0;
  void *single_buffer =
      create_compress_buffer(1, &compressor, &single_buffer_size);
  result = compressor.compress_data(&single_byte, 1, single_buffer,
                                    single_buffer_size, compressor.level);
  assert(result > 0);
  free(single_buffer);

  printf("✅ LZ4 edge cases passed\n");
}

static void test_lz4_get_original_file_size() {
  printf("Testing LZ4 get_original_file_size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  off_t original_size =
      compressor.get_original_file_size(compress_buffer, compressed_size);

  assert(original_size == test_data_size);

  free(compress_buffer);

  printf("✅ LZ4 get_original_file_size passed\n");
}

static void
test_lz4_get_original_file_size_returns_error_when_invalid_buffer() {
  printf("Testing LZ4 get_original_file_size returns error when invalid "
         "buffer...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  // We pass a non compressed buffer
  off_t original_size =
      compressor.get_original_file_size(compress_buffer, compress_bound);
  assert(original_size == LZ4F_FRAME_INFO_ERROR);

  free(compress_buffer);

  printf("✅ LZ4 get_original_file_size returns error when invalid buffer "
         "passed\n");
}

static void test_lz4_get_original_file_size_returns_error_when_invalid_size() {
  printf("Testing LZ4 get_original_file_size returns error when invalid "
         "size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  compressor.compress_data(test_data, test_data_size, compress_buffer,
                           compress_bound, compressor.level);

  // We pass a size of 0, so it should return an error
  off_t original_size = compressor.get_original_file_size(compress_buffer, 0);
  assert(original_size == LZ4F_FRAME_INFO_ERROR);

  free(compress_buffer);

  printf(
      "✅ LZ4 get_original_file_size returns error when invalid size passed\n");
}

static void test_lz4_get_compressed_size() {
  printf("Testing LZ4 get_compressed_size...\n");
  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);
  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  size_t compressed_size_out = 0;
  int result = compressor.get_compressed_size(
      compress_buffer, compressed_size, test_data_size, &compressed_size_out);

  assert(result == 0);
  assert(compressed_size_out > 0);
  assert(compressed_size_out == compressed_size);

  free(compress_buffer);

  printf("✅ LZ4 get_compressed_size passed\n");
}

static void test_lz4_detect_format() {
  printf("Testing LZ4 detect_format...\n");
  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_LZ4, 3);

  int result = compressor.detect_format(test_data, test_data_size);
  assert(result == -1);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);
  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  result = compressor.detect_format(compress_buffer, compressed_size);
  assert(result == 0);

  free(compress_buffer);

  printf("✅ LZ4 detect_format passed\n");
}

// ============================================================================
// ZSTD COMPRESSOR TESTS
// ============================================================================

static void test_zstd_init() {
  printf("Testing ZSTD initialization...\n");

  Compressor compressor;
  int result = compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  assert(result == 0);
  assert(compressor.algorithm == COMPRESSION_ZSTD);
  assert(compressor.level == 3);
  assert(compressor.compress_data != NULL);
  assert(compressor.decompress_data != NULL);

  printf("✅ ZSTD initialization passed\n");
}

static void test_zstd_level_validation() {
  printf("Testing ZSTD level validation...\n");

  Compressor compressor;

  // Test minimum level
  int result = compressor_init(&compressor, COMPRESSION_ZSTD, INT_MIN);
  assert(result == 0);
  assert(compressor.level == ZSTD_minCLevel());

  // Test maximum level
  result = compressor_init(&compressor, COMPRESSION_ZSTD, INT_MAX);
  assert(result == 0);
  assert(compressor.level == ZSTD_maxCLevel());

  printf("✅ ZSTD level validation passed\n");
}

static void test_zstd_compression_round_trip() {
  printf("Testing ZSTD compression round-trip...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  // Create buffers
  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);
  void *decompressed_buffer = malloc(test_data_size);

  assert(compress_buffer != NULL);
  assert(decompressed_buffer != NULL);

  // Compress
  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);
  assert(compressed_size > 0);
  assert(compressed_size <= compress_bound);

  // Decompress
  size_t decompressed_size = test_data_size;
  ssize_t decompressed_result =
      compressor.decompress_data(compress_buffer, compressed_size,
                                 decompressed_buffer, &decompressed_size);
  assert(decompressed_result == test_data_size);
  assert(decompressed_size == test_data_size);

  // Verify data integrity
  assert(memcmp(test_data, decompressed_buffer, test_data_size) == 0);

  // Cleanup
  free(compress_buffer);
  free(decompressed_buffer);

  printf("✅ ZSTD compression round-trip passed\n");
}

static void test_zstd_get_original_file_size() {
  printf("Testing ZSTD get_original_file_size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  off_t original_size =
      compressor.get_original_file_size(compress_buffer, compressed_size);

  assert(original_size == test_data_size);

  free(compress_buffer);

  printf("✅ ZSTD get_original_file_size passed\n");
}

static void
test_zstd_get_original_file_size_returns_error_when_invalid_buffer() {
  printf("Testing ZSTD get_original_file_size returns error when invalid "
         "buffer...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  // We pass a non compressed buffer
  off_t original_size =
      compressor.get_original_file_size(compress_buffer, compress_bound);

  assert(original_size == ZSTD_GET_FRAME_CONTENT_SIZE_ERROR);

  free(compress_buffer);

  printf("✅ ZSTD get_original_file_size returns error when invalid buffer "
         "passed\n");
}

static void test_zstd_get_original_file_size_returns_error_when_invalid_size() {
  printf("Testing ZSTD get_original_file_size returns error when invalid "
         "size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  compressor.compress_data(test_data, test_data_size, compress_buffer,
                           compress_bound, compressor.level);

  // We pass a size of 0, so it should return an error
  off_t original_size = compressor.get_original_file_size(compress_buffer, 0);
  assert(original_size == ZSTD_GET_FRAME_CONTENT_SIZE_ERROR);

  free(compress_buffer);

  printf("✅ ZSTD get_original_file_size returns error when invalid size "
         "passed\n");
}

static void test_zstd_get_compressed_size() {
  printf("Testing ZSTD get_compressed_size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);

  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  size_t compressed_size_out = 0;
  int result = compressor.get_compressed_size(
      compress_buffer, compressed_size, test_data_size, &compressed_size_out);
  assert(result == 0);
  assert(compressed_size_out > 0);
  assert(compressed_size_out == compressed_size);

  free(compress_buffer);

  printf("✅ ZSTD get_compressed_size passed\n");
}

static void test_zstd_detect_format() {
  printf("Testing ZSTD detect_format...\n");
  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  int result = compressor.detect_format(test_data, test_data_size);
  assert(result == -1);

  size_t compress_bound = 0;
  void *compress_buffer =
      create_compress_buffer(test_data_size, &compressor, &compress_bound);
  ssize_t compressed_size =
      compressor.compress_data(test_data, test_data_size, compress_buffer,
                               compress_bound, compressor.level);

  result = compressor.detect_format(compress_buffer, compressed_size);
  assert(result == 0);

  free(compress_buffer);

  printf("✅ ZSTD detect_format passed\n");
}

static void test_zstd_edge_cases() {
  printf("Testing ZSTD edge cases...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  // Test empty data
  size_t empty_buffer_size = 0;
  void *empty_buffer =
      create_compress_buffer(0, &compressor, &empty_buffer_size);
  ssize_t result = compressor.compress_data(
      "", 0, empty_buffer, empty_buffer_size, compressor.level);
  assert(result > 0);
  free(empty_buffer);

  // Test single byte compression
  char single_byte = 'A';
  size_t single_buffer_size = 0;
  void *single_buffer =
      create_compress_buffer(1, &compressor, &single_buffer_size);
  result = compressor.compress_data(&single_byte, 1, single_buffer,
                                    single_buffer_size, compressor.level);
  assert(result > 0);
  free(single_buffer);

  printf("✅ ZSTD edge cases passed\n");
}

static void test_zstd_get_max_header_size() {
  printf("Testing ZSTD get_max_header_size...\n");

  Compressor compressor;
  compressor_init(&compressor, COMPRESSION_ZSTD, 3);

  size_t max_header_size = compressor.get_max_header_size();
  // We compare with the value in zstd.h
  if (max_header_size != ZSTD_FRAMEHEADERSIZE_MAX) {
    (void)fprintf(stderr,
                  "ERROR: max_header_size (%zu) does not match "
                  "ZSTD_FRAMEHEADERSIZE_MAX (%zu)! Please ensure zstd has not "
                  "changed its expected max header size\n",
                  max_header_size, (size_t)ZSTD_FRAMEHEADERSIZE_MAX);
  }
  assert(max_header_size == ZSTD_FRAMEHEADERSIZE_MAX);

  printf("✅ ZSTD get_max_header_size passed\n");
}
// ============================================================================
// COMMON TESTS
// ============================================================================

static void test_error_conditions() {
  printf("Testing error conditions...\n");

  // Test NULL compressor
  int result = compressor_init(NULL, COMPRESSION_LZ4, 5);
  assert(result == -1);

  result = compressor_init(NULL, COMPRESSION_ZSTD, 3);
  assert(result == -1);

  // Test invalid algorithm
  Compressor compressor;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  result = compressor_init(&compressor, 999, 0); // Invalid enum value
  assert(result == -1);

  // Test with -1 (another invalid value)
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  result = compressor_init(&compressor, -1, 0);
  assert(result == -1);

  printf("✅ Error conditions passed\n");
}

static void test_compression_ratios() {
  printf("Testing compression ratios...\n");

  Compressor lz4_compressor, zstd_compressor;
  compressor_init(&lz4_compressor, COMPRESSION_LZ4, 0);
  compressor_init(&zstd_compressor, COMPRESSION_ZSTD, 3);

  // Compress with both algorithms
  size_t lz4_compress_bound = 0;
  void *lz4_buffer = create_compress_buffer(test_data_size, &lz4_compressor,
                                            &lz4_compress_bound);
  size_t zstd_compress_bound = 0;
  void *zstd_buffer = create_compress_buffer(test_data_size, &zstd_compressor,
                                             &zstd_compress_bound);

  ssize_t lz4_size =
      lz4_compressor.compress_data(test_data, test_data_size, lz4_buffer,
                                   lz4_compress_bound, lz4_compressor.level);
  ssize_t zstd_size =
      zstd_compressor.compress_data(test_data, test_data_size, zstd_buffer,
                                    zstd_compress_bound, zstd_compressor.level);

  assert(lz4_size > 0);
  assert(zstd_size > 0);

  printf("  Original size: %zu bytes\n", test_data_size);
  printf("  LZ4 compressed: %zu bytes (%.1f%%)\n", lz4_size,
         (float)lz4_size / (float)test_data_size * 100);
  printf("  ZSTD compressed: %zu bytes (%.1f%%)\n", zstd_size,
         (float)zstd_size / (float)test_data_size * 100);

  free(lz4_buffer);
  free(zstd_buffer);

  printf("✅ Compression ratios test passed\n");
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main() {
  printf("Starting compressor unit tests...\n\n");

  // LZ4 Tests
  printf("=== LZ4 COMPRESSOR TESTS ===\n");
  test_lz4_init();
  test_lz4_compression_round_trip();
  test_lz4_edge_cases();
  test_lz4_get_original_file_size();
  test_lz4_get_original_file_size_returns_error_when_invalid_buffer();
  test_lz4_get_original_file_size_returns_error_when_invalid_size();
  test_lz4_get_compressed_size();
  test_lz4_detect_format();
  printf("\n");

  // ZSTD Tests
  printf("=== ZSTD COMPRESSOR TESTS ===\n");
  test_zstd_init();
  test_zstd_level_validation();
  test_zstd_compression_round_trip();
  test_zstd_edge_cases();
  test_zstd_get_original_file_size();
  test_zstd_get_original_file_size_returns_error_when_invalid_buffer();
  test_zstd_get_original_file_size_returns_error_when_invalid_size();
  test_zstd_get_max_header_size();
  test_zstd_get_compressed_size();
  test_zstd_detect_format();
  printf("\n");

  // Common Tests
  printf("=== COMMON TESTS ===\n");
  test_error_conditions();
  test_compression_ratios();
  printf("\n");

  printf("🎉 All compressor tests passed!\n");
  return 0;
}
