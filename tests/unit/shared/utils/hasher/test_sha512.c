#include "../../../../../shared/utils/hasher/hasher.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test data
static const char test_data[] =
    "This is a test string for hashing. "
    "It contains various characters and patterns to test the SHA512 "
    "implementation thoroughly. The quick brown fox jumps over the lazy dog. "
    "1234567890 !@#$%^&*()_+-=[]{}|;:,.<>?";

static const size_t test_data_size =
    sizeof(test_data) - 1; // Exclude null terminator

// Test data for empty string
static const char empty_data[] = "";
static const size_t empty_data_size = 0;
static const char expected_empty_sha512_hex[] =
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d"
    "85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e";

// ============================================================================
// SHA512 HASHER TESTS
// ============================================================================

static void test_sha512_init() {
  printf("Testing SHA512 hasher initialization...\n");

  Hasher hasher;
  int result = hasher_init(&hasher, HASH_SHA512);

  assert(result == 0);
  assert(hasher.algorithm == HASH_SHA512);
  assert(hasher.hash_file_hex != NULL);
  assert(hasher.hash_buffer_hex != NULL);
  assert(hasher.hash_file_binary != NULL);
  assert(hasher.hash_buffer_binary != NULL);
  assert(hasher.get_hash_size != NULL);
  assert(hasher.get_hex_size != NULL);

  printf("✅ SHA512 hasher initialization passed\n");
}

static void test_sha512_hash_sizes() {
  printf("Testing SHA512 hash sizes...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  size_t hash_size = hasher.get_hash_size();
  size_t hex_size = hasher.get_hex_size();

  assert(hash_size == 64); // SHA512 produces 64 bytes
  assert(hex_size == 129); // 64 bytes * 2 + 1 null terminator

  printf("✅ SHA512 hash sizes passed\n");
}

static void test_sha512_buffer_hashing_hex() {
  printf("Testing SHA512 buffer hashing (hex output)...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Hash test data
  char *hex_hash = hasher.hash_buffer_hex(test_data, test_data_size);

  assert(hex_hash != NULL);
  assert(strlen(hex_hash) == 128); // 64 bytes * 2 characters

  // Verify against known hash
  // For now, just verify we got a valid hex string
  for (size_t i = 0; i < strlen(hex_hash); i++) {
    assert((hex_hash[i] >= '0' && hex_hash[i] <= '9') ||
           (hex_hash[i] >= 'a' && hex_hash[i] <= 'f'));
  }

  free(hex_hash);

  // Test empty data
  char *empty_hex = hasher.hash_buffer_hex(empty_data, empty_data_size);
  assert(empty_hex != NULL);
  assert(strlen(empty_hex) == 128);
  assert(strcmp(empty_hex, expected_empty_sha512_hex) == 0);
  free(empty_hex);

  printf("✅ SHA512 buffer hashing (hex) passed\n");
}

static void test_sha512_buffer_hashing_binary() {
  printf("Testing SHA512 buffer hashing (binary output)...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Hash test data
  unsigned char binary_hash[64];
  int result = hasher.hash_buffer_binary(test_data, test_data_size, binary_hash,
                                         sizeof(binary_hash));

  assert(result == 64); // Should return number of bytes written

  // Verify we got non-zero hash (very basic check)
  int all_zero = 1;
  for (int i = 0; i < 64; i++) {
    if (binary_hash[i] != 0) {
      all_zero = 0;
      break;
    }
  }
  assert(!all_zero); // Hash should not be all zeros

  // Test consistency: hex and binary should match
  char *hex_hash = hasher.hash_buffer_hex(test_data, test_data_size);
  char hex_from_binary[129];

  // Convert binary to hex
  for (int i = 0; i < 64; i++) {
    (void)sprintf(hex_from_binary + (ptrdiff_t)(i * 2), "%02x", binary_hash[i]);
  }
  hex_from_binary[128] = '\0';

  assert(strcmp(hex_hash, hex_from_binary) == 0);
  free(hex_hash);

  printf("✅ SHA512 buffer hashing (binary) passed\n");
}

static void test_sha512_buffer_hashing_consistency() {
  printf("Testing SHA512 buffer hashing consistency...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Hash same data multiple times
  char *hash1 = hasher.hash_buffer_hex(test_data, test_data_size);
  char *hash2 = hasher.hash_buffer_hex(test_data, test_data_size);
  char *hash3 = hasher.hash_buffer_hex(test_data, test_data_size);

  assert(hash1 != NULL);
  assert(hash2 != NULL);
  assert(hash3 != NULL);

  // All hashes should be identical
  assert(strcmp(hash1, hash2) == 0);
  assert(strcmp(hash2, hash3) == 0);

  free(hash1);
  free(hash2);
  free(hash3);

  printf("✅ SHA512 buffer hashing consistency passed\n");
}

static void test_sha512_different_data() {
  printf("Testing SHA512 with different data...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Hash different data
  char *hash1 = hasher.hash_buffer_hex("Hello", 5);
  char *hash2 = hasher.hash_buffer_hex("World", 5);
  char *hash3 = hasher.hash_buffer_hex("Hello", 5); // Same as hash1

  assert(hash1 != NULL);
  assert(hash2 != NULL);
  assert(hash3 != NULL);

  // Different data should produce different hashes
  assert(strcmp(hash1, hash2) != 0);

  // Same data should produce same hash
  assert(strcmp(hash1, hash3) == 0);

  free(hash1);
  free(hash2);
  free(hash3);

  printf("✅ SHA512 different data passed\n");
}

static void test_sha512_edge_cases() {
  printf("Testing SHA512 edge cases...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Test single byte
  char *single_byte_hash = hasher.hash_buffer_hex("A", 1);
  assert(single_byte_hash != NULL);
  assert(strlen(single_byte_hash) == 128);
  free(single_byte_hash);

  // Test binary with insufficient buffer
  unsigned char small_buffer[32]; // Too small for SHA512
  int result = hasher.hash_buffer_binary(test_data, test_data_size,
                                         small_buffer, sizeof(small_buffer));
  assert(result == -1); // Should fail

  // Test binary with exact buffer size
  unsigned char exact_buffer[64];
  result = hasher.hash_buffer_binary(test_data, test_data_size, exact_buffer,
                                     sizeof(exact_buffer));
  assert(result == 64); // Should succeed

  printf("✅ SHA512 edge cases passed\n");
}

static void test_sha512_error_conditions() {
  printf("Testing SHA512 error conditions...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Test NULL data buffer
  char *null_hash = hasher.hash_buffer_hex(NULL, 10);
  assert(null_hash == NULL);

  // Test NULL output buffer for binary
  int result = hasher.hash_buffer_binary(test_data, test_data_size, NULL, 64);
  assert(result == -1);

  // Test with zero-sized buffer for binary
  unsigned char buffer[64];
  result = hasher.hash_buffer_binary(test_data, test_data_size, buffer, 0);
  assert(result == -1);

  printf("✅ SHA512 error conditions passed\n");
}

static void test_sha512_memory_management() {
  printf("Testing SHA512 memory management...\n");

  Hasher hasher;
  hasher_init(&hasher, HASH_SHA512);

  // Test that we can hash and free many times without issues
  for (int i = 0; i < 100; i++) {
    char test_string[50];
    (void)sprintf(test_string, "Test iteration %d", i);

    char *hash = hasher.hash_buffer_hex(test_string, strlen(test_string));
    assert(hash != NULL);
    assert(strlen(hash) == 128);
    free(hash);
  }

  printf("✅ SHA512 memory management passed\n");
}

static void test_sha512_vs_sha256_difference() {
  printf("Testing SHA512 vs SHA256 difference...\n");

  Hasher sha256_hasher, sha512_hasher;
  hasher_init(&sha256_hasher, HASH_SHA256);
  hasher_init(&sha512_hasher, HASH_SHA512);

  // Hash same data with both algorithms
  char *sha256_hash = sha256_hasher.hash_buffer_hex(test_data, test_data_size);
  char *sha512_hash = sha512_hasher.hash_buffer_hex(test_data, test_data_size);

  assert(sha256_hash != NULL);
  assert(sha512_hash != NULL);

  // Hashes should be different
  assert(strcmp(sha256_hash, sha512_hash) != 0);

  // Different lengths
  assert(strlen(sha256_hash) == 64);
  assert(strlen(sha512_hash) == 128);

  // Different sizes
  assert(sha256_hasher.get_hash_size() == 32);
  assert(sha512_hasher.get_hash_size() == 64);
  assert(sha256_hasher.get_hex_size() == 65);
  assert(sha512_hasher.get_hex_size() == 129);

  free(sha256_hash);
  free(sha512_hash);

  printf("✅ SHA512 vs SHA256 difference passed\n");
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main() {
  printf("Starting SHA512 hasher unit tests...\n\n");

  printf("=== SHA512 HASHER TESTS ===\n");
  test_sha512_init();
  test_sha512_hash_sizes();
  test_sha512_buffer_hashing_hex();
  test_sha512_buffer_hashing_binary();
  test_sha512_buffer_hashing_consistency();
  test_sha512_different_data();
  test_sha512_edge_cases();
  test_sha512_error_conditions();
  test_sha512_memory_management();
  test_sha512_vs_sha256_difference();
  printf("\n");

  printf("🎉 All SHA512 hasher tests passed!\n");
  return 0;
}
