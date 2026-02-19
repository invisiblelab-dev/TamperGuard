#include "conversion.h"
#include <stdio.h>
#include <string.h>

/*
 * ============================================================================
 * CONVERSION UTILITIES IMPLEMENTATION
 * ============================================================================
 *
 * This implementation provides efficient and thread-safe conversion utilities
 * for transforming between different data representations. All functions are
 * designed to be reentrant and can be safely used in multi-threaded
 * environments.
 *
 * IMPLEMENTATION NOTES:
 * - All functions perform input validation
 * - Memory safety is ensured through proper bounds checking
 * - Error handling is consistent across all functions
 * - No dynamic memory allocation is performed
 * ============================================================================
 */

/**
 * @brief Convert binary data to a hex string.
 *
 * Converts binary data to a lowercase hexadecimal string representation.
 * Each byte is converted to two hexadecimal characters.
 *
 * @param bytes      -> input binary buffer
 * @param len        -> length of binary buffer
 * @param hex_str    -> output hex string buffer. Must be at least 2*len + 1
 * bytes.
 * @return void
 *
 * @note The caller is responsible for ensuring hex_str has sufficient space.
 * @note The output string is null-terminated.
 * @note This function is thread-safe.
 */
void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_str) {
  if (!bytes || !hex_str) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    (void)sprintf(hex_str + (i * 2), "%02x", bytes[i]);
  }
  hex_str[len * 2] = '\0';
}

/**
 * @brief Convert a single hexadecimal character to its numeric value.
 *
 * @param hex_char -> hexadecimal character (0-9, a-f, A-F)
 * @return int     -> numeric value (0-15), or -1 if invalid character
 */
static int hex_char_to_value(char hex_char) {
  if (hex_char >= '0' && hex_char <= '9') {
    return hex_char - '0';
  } else if (hex_char >= 'a' && hex_char <= 'f') {
    return hex_char - 'a' + 10;
  } else if (hex_char >= 'A' && hex_char <= 'F') {
    return hex_char - 'A' + 10;
  } else {
    return -1; // Invalid character
  }
}

/**
 * @brief Convert hexadecimal string to binary data.
 *
 * Converts a hexadecimal string to binary data. The input string must
 * contain only valid hexadecimal characters (0-9, a-f, A-F).
 *
 * @param hex_str    -> input hex string (null-terminated)
 * @param bytes      -> output binary buffer. Must be at least strlen(hex_str)/2
 * bytes.
 * @param max_bytes  -> maximum number of bytes to write to output buffer
 * @return size_t    -> number of bytes written to output buffer, or 0 on error
 *
 * @note The caller is responsible for ensuring bytes buffer has sufficient
 * space.
 * @note Input string length must be even (each byte requires 2 hex characters).
 * @note This function is thread-safe.
 */
size_t hex_to_bytes(const char *hex_str, unsigned char *bytes,
                    size_t max_bytes) {
  if (!hex_str || !bytes || max_bytes == 0) {
    return 0;
  }

  size_t hex_len = strlen(hex_str);

  // Check if the hex string length is even
  if (hex_len % 2 != 0) {
    return 0;
  }

  size_t bytes_needed = hex_len / 2;

  // Check if we have enough space in the output buffer
  if (bytes_needed > max_bytes) {
    return 0;
  }

  // Convert hex string to bytes
  for (size_t i = 0; i < bytes_needed; i++) {
    int high_nibble = hex_char_to_value(hex_str[i * 2]);
    int low_nibble = hex_char_to_value(hex_str[i * 2 + 1]);

    if (high_nibble == -1 || low_nibble == -1) {
      return 0; // Invalid hex character
    }

    bytes[i] = (unsigned char)((high_nibble << 4) | low_nibble);
  }

  return bytes_needed;
}
