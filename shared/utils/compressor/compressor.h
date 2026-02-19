#ifndef __COMPRESSOR_H__
#define __COMPRESSOR_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct Compressor Compressor;

// Compression algorithm types
typedef enum { COMPRESSION_ZSTD, COMPRESSION_LZ4 } compression_algorithm_t;

#define UINT64_TO_OFF_T_CONVERSION_ERROR -2
#define LZ4F_CREATE_DECOMPRESSION_CONTEXT_ERROR -3
#define LZ4F_FRAME_INFO_ERROR -4
#define ZSTD_GET_FRAME_CONTENT_SIZE_ERROR -5

/**
 * @brief Compressor structure for handling different compression algorithms
 *
 * This structure provides a unified interface for different compression
 * algorithms (ZSTD and LZ4) through function pointers. It
 * allows switching between compression methods at runtime without changing the
 * calling code.
 *
 * @example
 * ```c
 * Compressor compressor;
 * compressor_init(&compressor, COMPRESSION_LZ4, 5);
 *
 * // Compress data
 * int compressed_size = compressor.compress_data(
 *     input_data, input_size, output_buffer, output_buffer_size,
 * compressor.level);
 *
 * // Decompress data
 * size_t decompressed_size = original_size;
 * int result = compressor.decompress_data(
 *     compressed_data, compressed_size,
 *     decompressed_buffer, &decompressed_size);
 * ```
 */
typedef struct Compressor {
  /** @brief Compression algorithm type */
  compression_algorithm_t algorithm;

  /**
   * @brief Compression level
   *
   * The valid range and effect depends on the algorithm:
   * - LZ4: The level represents compression level, where higher levels (1-12)
   * provide better compression ratio but slower speed. Level 0 is default fast
   * mode. Negative values enable fast acceleration mode.
   * - ZSTD: The level represents the compression ratio. The lower the level,
   * the faster the compression and lower the compression ratio. The valid range
   * is -131072 to 22 (negative=ultra-fast modes, positive=normal compression,
   * 3=default)
   *
   * For ZSTD, negative levels enable ultra-fast compression that prioritizes
   * speed over compression ratio. Positive levels provide the traditional
   * speed vs. compression tradeoff.
   */
  int level;

  /**
   * @brief Function pointer for compressing data
   *
   * @param file_buffer Pointer to the input data to compress
   * @param file_size Size of the input data in bytes
   * @param compressed_buffer Pointer to the output buffer for compressed data
   * @param compressed_buffer_size Size of the output buffer in bytes
   * @param level Compression level to use
   * @return Number of bytes written to compressed_buffer, or -1 on error
   *
   * @note The output buffer must be at least as large as the value returned
   *       by get_compress_bound(file_size) to avoid buffer overflows.
   */
  ssize_t (*compress_data)(const void *file_buffer, size_t file_size,
                           void *compressed_buffer,
                           size_t compressed_buffer_size, int level);

  /**
   * @brief Function pointer for decompressing data
   *
   * @param compressed_buffer Pointer to the compressed data
   * @param compressed_size Size of the compressed data in bytes
   * @param real_buffer Pointer to the output buffer for decompressed data
   * @param real_size Pointer to the size of the output buffer. On return,
   *                  contains the actual size of the decompressed data.
   * @return Number of bytes written to real_buffer, or -1 on error
   *
   * @note The output buffer must be large enough to hold the decompressed data.
   *       The caller should know the original uncompressed size.
   */
  ssize_t (*decompress_data)(const void *compressed_buffer,
                             size_t compressed_size, void *real_buffer,
                             size_t *real_size);

  /**
   * @brief Function pointer for calculating maximum compressed size
   *
   * @param file_size Size of the input data in bytes
   * @param level Compression level to use
   * @return Maximum number of bytes that compressed data could occupy
   *
   * @note This is used to determine the required size of the output buffer
   *       for compression operations. The actual compressed size may be
   * smaller.
   */
  size_t (*get_compress_bound)(size_t file_size, int level);

  /**
   * @brief Function pointer for getting the original size of the data
   *
   * @param compressed_buffer Pointer to the compressed data
   * @param compressed_size Size of the compressed data in bytes
   * @return Size of the original data in bytes
   */
  off_t (*get_original_file_size)(const void *compressed_buffer,
                                  size_t compressed_size);

  /**
   * @brief Function pointer for getting the maximum header size that a frame
   * can have. Determined by the library used.
   *
   * @return Maximum header size in bytes
   */
  size_t (*get_max_header_size)();

  /**
   * @brief Function pointer for getting the compressed size of a frame
   *
   * This function finds the actual compressed size of a frame by parsing
   * the frame format. For ZSTD, this is done by reading the frame header.
   * For LZ4F, this requires streaming decompression to find where the frame
   * ends.
   *
   * @param compressed_buffer Pointer to the compressed data (must contain at
   * least the frame header)
   * @param max_size Maximum size to read (e.g., block_size for sparse blocks)
   * @param expected_uncompressed_size Expected uncompressed size (for LZ4F
   * buffer allocation, can be block_size)
   * @param compressed_size_out Output: actual compressed size of the frame
   * @return 0 on success, -1 on error
   *
   * @note The compressed_buffer must contain at least the frame header, and
   *       ideally the entire frame (up to max_size bytes).
   */
  int (*get_compressed_size)(const void *compressed_buffer, size_t max_size,
                             size_t expected_uncompressed_size,
                             size_t *compressed_size_out);

  /**
   * @brief Function pointer for detecting if data matches this compressor's
   * format
   *
   * This function checks if the data at the given buffer matches this
   * compressor's format. This is used to identify compressed blocks during
   * crash recovery.
   *
   * @param data Buffer to check (must be at least 4 bytes)
   * @param data_size Size of the data buffer (must be >= 4)
   * @return 0 if data matches this compressor's format, -1 if it doesn't or
   * on error
   *
   * @note The buffer must contain at least 4 bytes to check the format.
   *       For uncompressed data or unknown formats, returns -1.
   */
  int (*detect_format)(const void *data, size_t data_size);
} Compressor;

/**
 * @brief Initialize a compressor with the specified algorithm
 *
 * @param compressor Pointer to the compressor to initialize
 * @param algorithm Compression algorithm to use
 * @param level Compression level
 * @return 0 on success, -1 on error
 *
 * @note For LZ4: level > 0 <= 12 (0=default(fast mode), 12=best compression,
 * negative values=fast acceleration)
 * @note For ZSTD: level 1-22 (1=fastest, 22=best compression)
 */
int compressor_init(Compressor *compressor, compression_algorithm_t algorithm,
                    int level);

#endif
