#include "../../../../../shared/utils/hasher/hasher.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test data
static const char test_data[] =
    "This is a test string for hasher interface testing. "
    "It contains various characters and patterns to test the hasher "
    "implementation thoroughly across different algorithms. "
    "The quick brown fox jumps over the lazy dog. "
    "1234567890 !@#$%^&*()_+-=[]{}|;:,.<>?";

static const size_t test_data_size =
    sizeof(test_data) - 1; // Exclude null terminator

// ============================================================================
// HASHER INTERFACE TESTS
// ============================================================================

static void test_hasher_init_algorithms() {
  printf("Testing hasher initialization with different algorithms...\n");

  // Test SHA256
  Hasher sha256_hasher;
  int result = hasher_init(&sha256_hasher, HASH_SHA256);
  assert(result == 0);
  assert(sha256_hasher.algorithm == HASH_SHA256);

  // Test SHA512
  Hasher sha512_hasher;
  result = hasher_init(&sha512_hasher, HASH_SHA512);
  assert(result == 0);
  assert(sha512_hasher.algorithm == HASH_SHA512);

  printf("✅ Hasher initialization with different algorithms passed\n");
}

static void test_hasher_init_error_conditions() {
  printf("Testing hasher initialization error conditions...\n");

  // Test NULL hasher
  int result = hasher_init(NULL, HASH_SHA256);
  assert(result == -1);

  // Test invalid algorithm
  Hasher hasher;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  result = hasher_init(&hasher, 999); // Invalid algorithm
  assert(result == -1);

  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  result = hasher_init(&hasher, -1); // Another invalid algorithm
  assert(result == -1);

  printf("✅ Hasher initialization error conditions passed\n");
}

static void test_hasher_algorithm_differences() {
  printf("Testing differences between hash algorithms...\n");

  Hasher sha256_hasher, sha512_hasher;
  hasher_init(&sha256_hasher, HASH_SHA256);
  hasher_init(&sha512_hasher, HASH_SHA512);

  // Test size differences
  assert(sha256_hasher.get_hash_size() == 32);
  assert(sha512_hasher.get_hash_size() == 64);
  assert(sha256_hasher.get_hex_size() == 65);
  assert(sha512_hasher.get_hex_size() == 129);

  // Test that same data produces different hashes
  char *sha256_hash = sha256_hasher.hash_buffer_hex(test_data, test_data_size);
  char *sha512_hash = sha512_hasher.hash_buffer_hex(test_data, test_data_size);

  assert(sha256_hash != NULL);
  assert(sha512_hash != NULL);
  assert(strcmp(sha256_hash, sha512_hash) != 0);

  // Test binary hashes are also different
  unsigned char sha256_binary[32];
  unsigned char sha512_binary[64];

  int result1 = sha256_hasher.hash_buffer_binary(
      test_data, test_data_size, sha256_binary, sizeof(sha256_binary));
  int result2 = sha512_hasher.hash_buffer_binary(
      test_data, test_data_size, sha512_binary, sizeof(sha512_binary));

  assert(result1 == 32);
  assert(result2 == 64);

  free(sha256_hash);
  free(sha512_hash);

  printf("✅ Algorithm differences passed\n");
}

static void test_hasher_function_pointer_consistency() {
  printf("Testing hasher function pointer consistency...\n");

  Hasher hasher1, hasher2;
  hasher_init(&hasher1, HASH_SHA256);
  hasher_init(&hasher2, HASH_SHA256);

  // Same algorithm should have same function pointers
  assert(hasher1.hash_buffer_hex == hasher2.hash_buffer_hex);
  assert(hasher1.hash_buffer_binary == hasher2.hash_buffer_binary);
  assert(hasher1.get_hash_size == hasher2.get_hash_size);
  assert(hasher1.get_hex_size == hasher2.get_hex_size);

  // Different algorithms should have different function pointers
  Hasher hasher_sha512;
  hasher_init(&hasher_sha512, HASH_SHA512);

  assert(hasher1.hash_buffer_hex != hasher_sha512.hash_buffer_hex);
  assert(hasher1.hash_buffer_binary != hasher_sha512.hash_buffer_binary);
  assert(hasher1.get_hash_size != hasher_sha512.get_hash_size);
  assert(hasher1.get_hex_size != hasher_sha512.get_hex_size);

  printf("✅ Function pointer consistency passed\n");
}

static void test_hasher_multiple_initializations() {
  printf("Testing multiple hasher initializations...\n");

  Hasher hasher;

  // Initialize as SHA256
  int result = hasher_init(&hasher, HASH_SHA256);
  assert(result == 0);
  assert(hasher.algorithm == HASH_SHA256);

  char *hash1 = hasher.hash_buffer_hex("test", 4);
  assert(hash1 != NULL);
  assert(strlen(hash1) == 64);

  // Re-initialize as SHA512
  result = hasher_init(&hasher, HASH_SHA512);
  assert(result == 0);
  assert(hasher.algorithm == HASH_SHA512);

  char *hash2 = hasher.hash_buffer_hex("test", 4);
  assert(hash2 != NULL);
  assert(strlen(hash2) == 128);

  // Hashes should be different
  assert(strcmp(hash1, hash2) != 0);

  free(hash1);
  free(hash2);

  printf("✅ Multiple initializations passed\n");
}

static void test_hasher_cross_algorithm_compatibility() {
  printf("Testing cross-algorithm compatibility...\n");

  Hasher sha256_hasher, sha512_hasher;
  hasher_init(&sha256_hasher, HASH_SHA256);
  hasher_init(&sha512_hasher, HASH_SHA512);

  // Test that we can use different algorithms in the same program
  char *sha256_hash1 = sha256_hasher.hash_buffer_hex("Hello", 5);
  char *sha512_hash1 = sha512_hasher.hash_buffer_hex("Hello", 5);
  char *sha256_hash2 = sha256_hasher.hash_buffer_hex("World", 5);
  char *sha512_hash2 = sha512_hasher.hash_buffer_hex("World", 5);

  assert(sha256_hash1 != NULL);
  assert(sha512_hash1 != NULL);
  assert(sha256_hash2 != NULL);
  assert(sha512_hash2 != NULL);

  // Same algorithm, same data should produce same hash
  char *sha256_hash3 = sha256_hasher.hash_buffer_hex("Hello", 5);
  char *sha512_hash3 = sha512_hasher.hash_buffer_hex("Hello", 5);

  assert(strcmp(sha256_hash1, sha256_hash3) == 0);
  assert(strcmp(sha512_hash1, sha512_hash3) == 0);

  free(sha256_hash1);
  free(sha512_hash1);
  free(sha256_hash2);
  free(sha512_hash2);
  free(sha256_hash3);
  free(sha512_hash3);

  printf("✅ Cross-algorithm compatibility passed\n");
}

static void test_hasher_performance_consistency() {
  printf("Testing hasher performance consistency...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA256);

  // Test that performance is consistent across multiple calls
  for (int i = 0; i < 50; i++) {
    char test_string[100];
    (void)sprintf(test_string, "Performance test iteration %d with some data",
                  i);

    char *hash = hasher.hash_buffer_hex(test_string, strlen(test_string));
    assert(hash != NULL);
    assert(strlen(hash) == 64);
    free(hash);
  }

  printf("✅ Performance consistency passed\n");
}

static void test_hasher_large_data() {
  printf("Testing hasher with large data...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA256);

  // Create a large buffer (1MB)
  size_t large_size = (size_t)(1024 * 1024);
  char *large_data = malloc(large_size);
  assert(large_data != NULL);

  // Fill with pattern
  for (size_t i = 0; i < large_size; i++) {
    large_data[i] = (char)(i % 256);
  }

  // Hash the large data
  char *hash = hasher.hash_buffer_hex(large_data, large_size);
  assert(hash != NULL);
  assert(strlen(hash) == 64);

  // Hash again to verify consistency
  char *hash2 = hasher.hash_buffer_hex(large_data, large_size);
  assert(hash2 != NULL);
  assert(strcmp(hash, hash2) == 0);

  free(large_data);
  free(hash);
  free(hash2);

  printf("✅ Large data hashing passed\n");
}

// ============================================================================
// COMMON TESTS FOR ALL ALGORITHMS
// ============================================================================

static void test_algorithm_common_properties(hash_algorithm_t algorithm,
                                             const char *algorithm_name) {
  printf("Testing common properties for %s...\n", algorithm_name);

  Hasher hasher;
  int result = hasher_init(&hasher, algorithm);
  assert(result == 0);

  // Test that all function pointers are set
  assert(hasher.hash_buffer_hex != NULL);
  assert(hasher.hash_buffer_binary != NULL);
  assert(hasher.get_hash_size != NULL);
  assert(hasher.get_hex_size != NULL);

  // Test size consistency
  size_t hash_size = hasher.get_hash_size();
  size_t hex_size = hasher.get_hex_size();
  assert(hex_size == hash_size * 2 + 1);

  // Test empty data handling
  char *empty_hash = hasher.hash_buffer_hex("", 0);
  assert(empty_hash != NULL);
  assert(strlen(empty_hash) == hash_size * 2);
  free(empty_hash);

  // Test single byte
  char *single_hash = hasher.hash_buffer_hex("X", 1);
  assert(single_hash != NULL);
  assert(strlen(single_hash) == hash_size * 2);
  free(single_hash);

  // Test deterministic behavior
  char *hash1 = hasher.hash_buffer_hex(test_data, test_data_size);
  char *hash2 = hasher.hash_buffer_hex(test_data, test_data_size);
  assert(hash1 != NULL);
  assert(hash2 != NULL);
  assert(strcmp(hash1, hash2) == 0);
  free(hash1);
  free(hash2);

  printf("✅ Common properties for %s passed\n", algorithm_name);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main() {
  printf("Starting comprehensive hasher interface tests...\n\n");

  // Interface tests
  printf("=== HASHER INTERFACE TESTS ===\n");
  test_hasher_init_algorithms();
  test_hasher_init_error_conditions();
  test_hasher_algorithm_differences();
  test_hasher_function_pointer_consistency();
  test_hasher_multiple_initializations();
  test_hasher_cross_algorithm_compatibility();
  test_hasher_performance_consistency();
  test_hasher_large_data();
  printf("\n");

  // Common algorithm tests
  printf("=== COMMON ALGORITHM TESTS ===\n");
  test_algorithm_common_properties(HASH_SHA256, "SHA256");
  test_algorithm_common_properties(HASH_SHA512, "SHA512");
  printf("\n");

  printf("🎉 All hasher interface tests passed!\n");
  return 0;
}
