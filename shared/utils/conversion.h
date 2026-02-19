#ifndef CONVERSION_H
#define CONVERSION_H

#include <stddef.h>

/*
 * ============================================================================
 * CONVERSION UTILITIES
 * ============================================================================
 *
 * This module provides utility functions for converting between different
 * data representations (binary, hex, etc.). These utilities are commonly
 * needed across different layers for data serialization, cryptographic
 * operations, and debugging.
 *
 * KEY FEATURES:
 * - Binary to hexadecimal string conversion
 * - Hexadecimal string to binary conversion
 * - Thread-safe operations
 * - Memory-efficient implementations
 *
 * USAGE:
 * 1. Include this header in your source files
 * 2. Link against the conversion utilities object file
 * 3. Use the provided functions for data conversion
 *
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
 *
 * @example
 *   unsigned char data[] = {0xDE, 0xAD, 0xBE, 0xEF};
 *   char hex_output[9]; // 4 bytes * 2 + 1 for null terminator
 *   bytes_to_hex(data, 4, hex_output);
 *   // hex_output will contain "deadbeef"
 */
void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_str);

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
 *
 * @example
 *   char hex_input[] = "deadbeef";
 *   unsigned char data[4];
 *   size_t result = hex_to_bytes(hex_input, data, 4);
 *   // data will contain {0xDE, 0xAD, 0xBE, 0xEF}, result will be 4
 */
size_t hex_to_bytes(const char *hex_str, unsigned char *bytes,
                    size_t max_bytes);

#endif // CONVERSION_H
