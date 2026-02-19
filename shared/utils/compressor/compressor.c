#include "compressor.h"
#include "../../../lib/lz4/lib/lz4frame.h"
#include "../../../lib/zstd/lib/zstd.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// ZSTD frame header size max set to 18 bytes as its done in the zstd library.
// Check ZSTD_FRAMEHEADERSIZE_MAX in zstd.h
static const size_t zstd_frame_header_size_max = 18;

// Cached maximum off_t value
// This is required because OFF_MAX is not defined in the standard
static off_t max_off_t_value = 0;

// Helper function to get maximum off_t value (calculated once)
static off_t get_max_off_t(void) {
  // Compile-time check - if this fails, compilation stops
  _Static_assert(
      sizeof(off_t) == sizeof(long long) || sizeof(off_t) == sizeof(long) ||
          sizeof(off_t) == sizeof(int),
      "Unsupported off_t size - please add support for this platform");

  if (max_off_t_value == 0) {
    if (sizeof(off_t) == sizeof(long long)) {
      max_off_t_value = (off_t)LLONG_MAX;
    } else if (sizeof(off_t) == sizeof(long)) {
      max_off_t_value = (off_t)LONG_MAX;
    } else if (sizeof(off_t) == sizeof(int)) {
      max_off_t_value = (off_t)INT_MAX;
    }
  }
  return max_off_t_value;
}

// LZ4 compression implementation
static LZ4F_preferences_t lz4_build_preferences(int level, size_t file_size) {
  LZ4F_preferences_t prefs = {.frameInfo = {.contentSize = file_size},
                              .compressionLevel = level};
  return prefs;
}

static ssize_t convert_to_ssize_t(size_t size) {
  if (size > SSIZE_MAX) {
    return -1;
  }
  return (ssize_t)size;
}

ssize_t lz4_compress_data(const void *file_buffer, size_t file_size,
                          void *compressed_buffer,
                          size_t compressed_buffer_size, int level) {
  LZ4F_preferences_t prefs = lz4_build_preferences(level, file_size);

  size_t result = LZ4F_compressFrame(compressed_buffer, compressed_buffer_size,
                                     file_buffer, file_size, &prefs);
  if (!LZ4F_isError(result)) {
    return convert_to_ssize_t(result);
  }
  return -1;
}

// Frame decompression data in LZ4 is done by calling LZ4F_decompress()
// repeatedly until the entire frame is decompressed. Unlike ZSTD that
// decompresses the entire frame in one go, LZ4 decompresses the frame in
// chunks.
ssize_t lz4_decompress_data(const void *compressed_buffer,
                            size_t compressed_size, void *decompressed_buffer,
                            size_t *decompressed_capacity) {
  LZ4F_dctx *dctx;
  LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
  if (LZ4F_isError(err))
    return -1;

  size_t src_offset = 0;
  size_t dst_offset = 0;

  while (src_offset < compressed_size && dst_offset < *decompressed_capacity) {
    size_t src_size = compressed_size - src_offset;
    size_t dst_size = *decompressed_capacity - dst_offset;

    size_t ret = LZ4F_decompress(
        dctx, (char *)decompressed_buffer + dst_offset, &dst_size,
        (const char *)compressed_buffer + src_offset, &src_size, NULL);

    if (LZ4F_isError(ret)) {
      LZ4F_freeDecompressionContext(dctx);
      return -1;
    }

    src_offset += src_size;
    dst_offset += dst_size;

    if (ret == 0)
      break;
  }

  LZ4F_freeDecompressionContext(dctx);

  *decompressed_capacity = dst_offset;
  return (ssize_t)dst_offset;
}

static size_t lz4_get_compress_bound(size_t file_size, int level) {
  LZ4F_preferences_t prefs = lz4_build_preferences(level, file_size);
  return LZ4F_compressFrameBound(file_size, &prefs);
}

static off_t lz4_get_original_file_size(const void *compressed_buffer,
                                        size_t compressed_size) {
  size_t compressed_size_copy = compressed_size;
  LZ4F_dctx *dctx;
  LZ4F_errorCode_t error = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
  if (LZ4F_isError(error)) {
    return LZ4F_CREATE_DECOMPRESSION_CONTEXT_ERROR;
  }

  LZ4F_frameInfo_t frame_info;
  size_t header_size = LZ4F_getFrameInfo(dctx, &frame_info, compressed_buffer,
                                         &compressed_size_copy);

  LZ4F_freeDecompressionContext(dctx);
  if (LZ4F_isError(header_size)) {
    return LZ4F_FRAME_INFO_ERROR;
  }

  uint64_t original_size = frame_info.contentSize;

  if (original_size > (uint64_t)get_max_off_t()) {
    return UINT64_TO_OFF_T_CONVERSION_ERROR;
  }

  return (off_t)original_size;
}

static size_t lz4_get_max_header_size() { return LZ4F_HEADER_SIZE_MAX; }

/**
 * @brief Helper to decompress LZ4F frame and track consumed input
 *
 * This is the core decompression logic used by both lz4_decompress_data
 * and lz4_get_compressed_size. It tracks consumed input size and works
 * with unknown compressed size (up to max_size).
 *
 * @param compressed_buffer Input buffer
 * @param max_size Maximum input bytes to read (upper bound).
 *                 - For lz4_decompress_data: pass the known compressed_size
 *                 - For lz4_get_compressed_size: pass block_size (max possible
 * size)
 * @param decompressed_buffer Output buffer
 * @param decompressed_capacity Input/output: capacity and actual size
 * @param consumed_out Output: actual consumed input size (may be less than
 * max_size if frame ends early)
 * @return 0 on success (frame complete), -1 on error
 */
static int lz4_decompress_and_track_consumed(const void *compressed_buffer,
                                             size_t max_size,
                                             void *decompressed_buffer,
                                             size_t *decompressed_capacity,
                                             size_t *consumed_out) {
  LZ4F_dctx *dctx;
  LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
  if (LZ4F_isError(err))
    return -1;

  size_t src_offset = 0;
  size_t dst_offset = 0;

  while (src_offset < max_size && dst_offset < *decompressed_capacity) {
    size_t src_size = max_size - src_offset;
    size_t dst_size = *decompressed_capacity - dst_offset;

    size_t ret = LZ4F_decompress(
        dctx, (char *)decompressed_buffer + dst_offset, &dst_size,
        (const char *)compressed_buffer + src_offset, &src_size, NULL);

    if (LZ4F_isError(ret)) {
      LZ4F_freeDecompressionContext(dctx);
      return -1;
    }

    src_offset += src_size;
    dst_offset += dst_size;

    if (ret == 0) {
      // Frame complete
      LZ4F_freeDecompressionContext(dctx);
      *decompressed_capacity = dst_offset;
      *consumed_out = src_offset;
      return 0;
    }

    // If no input was consumed but we haven't reached the end, might be
    // corrupted
    if (src_size == 0 && src_offset < max_size) {
      LZ4F_freeDecompressionContext(dctx);
      return -1;
    }
  }

  // Reached max_size or buffer limit without completing frame
  LZ4F_freeDecompressionContext(dctx);
  return -1;
}

/**
 * @brief Get compressed size of LZ4F frame by streaming decompression
 *
 * This function finds the actual compressed size by decompressing the frame
 * in streaming mode and tracking how much input was consumed. It reuses
 * the same decompression logic as lz4_decompress_data.
 *
 * @param compressed_buffer Buffer containing compressed data
 * @param max_size Maximum size to read (block_size for sparse blocks)
 * @param expected_uncompressed_size Expected uncompressed size (for buffer
 * allocation)
 * @param compressed_size_out Output: actual compressed size
 * @return 0 on success, -1 on error
 */
static int lz4_get_compressed_size(const void *compressed_buffer,
                                   size_t max_size,
                                   size_t expected_uncompressed_size,
                                   size_t *compressed_size_out) {
  // Allocate output buffer
  uint8_t *decompressed = (uint8_t *)malloc(expected_uncompressed_size);
  if (!decompressed) {
    return -1;
  }

  size_t decompressed_capacity = expected_uncompressed_size;
  size_t consumed = 0;

  // Use the helper function that shares logic with lz4_decompress_data
  int result = lz4_decompress_and_track_consumed(
      compressed_buffer, max_size, decompressed, &decompressed_capacity,
      &consumed);

  free(decompressed);

  if (result == 0) {
    *compressed_size_out = consumed;
    return 0;
  }

  return -1;
}

// ZSTD compression implementation
static ssize_t zstd_compress_data(const void *file_buffer, size_t file_size,
                                  void *compressed_buffer,
                                  size_t compressed_buffer_size, int level) {
  size_t result = ZSTD_compress(compressed_buffer, compressed_buffer_size,
                                file_buffer, file_size, level);
  if (!ZSTD_isError(result)) {
    return convert_to_ssize_t(result);
  }
  return -1;
}

static ssize_t zstd_decompress_data(const void *compressed_buffer,
                                    size_t compressed_size, void *real_buffer,
                                    size_t *real_size) {
  size_t result = ZSTD_decompress(real_buffer, *real_size, compressed_buffer,
                                  compressed_size);
  if (!ZSTD_isError(result)) {
    return convert_to_ssize_t(result);
  }
  return -1;
}

static size_t zstd_get_compress_bound(size_t file_size, int level) {
  (void)level; // ZSTD_compressBound doesn't need level
  return ZSTD_compressBound(file_size);
}

static off_t zstd_get_original_file_size(const void *compressed_buffer,
                                         size_t compressed_size) {
  uint64_t original_size =
      ZSTD_getFrameContentSize(compressed_buffer, compressed_size);
  if (!ZSTD_isError(original_size) && original_size != ZSTD_CONTENTSIZE_ERROR &&
      original_size != ZSTD_CONTENTSIZE_UNKNOWN) {
    if (original_size > (uint64_t)get_max_off_t()) {
      return UINT64_TO_OFF_T_CONVERSION_ERROR;
    }
    return (off_t)original_size;
  }
  return ZSTD_GET_FRAME_CONTENT_SIZE_ERROR;
}

static size_t zstd_get_max_header_size() {
  // We can not directly use ZSTD_FRAMEHEADERSIZE_MAX as it is avaiable only if
  // the library is statically linked.
  // For that reason, we set it to the current value and test it has not changed
  // in the tests.
  return zstd_frame_header_size_max;
}

/**
 * @brief Get compressed size of ZSTD frame by parsing frame header
 *
 * This function uses ZSTD_findFrameCompressedSize() to find the exact
 * compressed size without decompressing the frame.
 *
 * @param compressed_buffer Buffer containing compressed data
 * @param max_size Maximum size to read (block_size for sparse blocks)
 * @param expected_uncompressed_size Not used for ZSTD (kept for interface
 * consistency)
 * @param compressed_size_out Output: actual compressed size
 * @return 0 on success, -1 on error
 */
static int zstd_get_compressed_size(const void *compressed_buffer,
                                    size_t max_size,
                                    size_t expected_uncompressed_size,
                                    size_t *compressed_size_out) {
  (void)expected_uncompressed_size; // Not needed for ZSTD

  size_t compressed_size =
      ZSTD_findFrameCompressedSize(compressed_buffer, max_size);
  if (ZSTD_isError(compressed_size)) {
    return -1;
  }

  *compressed_size_out = compressed_size;
  return 0;
}

/**
 * @brief Detect if data matches LZ4F format
 *
 * Uses a hybrid approach:
 * 1. First checks the magic number (fast, requires only 4 bytes)
 * 2. If magic number matches, validates the frame header with
 * LZ4F_getFrameInfo() (more robust, ensures it's a valid frame structure)
 *
 * @param data Buffer to check (must be at least 4 bytes for magic check,
 *             LZ4F_HEADER_SIZE_MIN for full validation)
 * @param data_size Size of the data buffer
 * @return 0 if data matches LZ4F format, -1 if it doesn't or on error
 */
static int lz4_detect_format(const void *data, size_t data_size) {
  if (data == NULL || data_size < 4) {
    return -1;
  }

  // Fast magic number check
  uint32_t magic = 0;
  memcpy(&magic, data, sizeof(uint32_t));

  if (magic != LZ4F_MAGICNUMBER) {
    return -1; // Not LZ4F format
  }

  LZ4F_dctx *dctx;
  LZ4F_errorCode_t error = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
  if (LZ4F_isError(error)) {
    // Failed to create context, but magic number matches - assume LZ4F
    return 0;
  }

  LZ4F_frameInfo_t frame_info;
  size_t src_size = data_size;
  size_t result = LZ4F_getFrameInfo(dctx, &frame_info, data, &src_size);

  LZ4F_freeDecompressionContext(dctx);

  // If LZ4F_getFrameInfo() succeeds, it's a valid LZ4F frame
  if (!LZ4F_isError(result)) {
    return 0; // Valid LZ4F frame confirmed
  }

  // Magic number matched but frame validation failed - likely corrupted
  // Return -1 to indicate it's not a valid LZ4F format
  return -1;
}

/**
 * @brief Detect if data matches ZSTD format
 *
 * Checks if the data starts with the ZSTD magic number (0xFD2FB528).
 * This is fast and sufficient for format identification.
 *
 * @param data Buffer to check (must be at least 4 bytes)
 * @param data_size Size of the data buffer
 * @return 0 if data matches ZSTD format, -1 if it doesn't or on error
 */
static int zstd_detect_format(const void *data, size_t data_size) {
  if (data == NULL || data_size < 4) {
    return -1;
  }

  uint32_t magic = 0;
  memcpy(&magic, data, sizeof(uint32_t));

  if (magic == ZSTD_MAGICNUMBER) {
    return 0; // ZSTD format detected
  }

  return -1; // Not ZSTD format
}

static int compressor_init_lz4(Compressor *compressor, int level) {
  if (!compressor)
    return -1;

  compressor->algorithm = COMPRESSION_LZ4;
  compressor->level = level;
  compressor->compress_data = lz4_compress_data;
  compressor->decompress_data = lz4_decompress_data;
  compressor->get_compress_bound = lz4_get_compress_bound;
  compressor->get_original_file_size = lz4_get_original_file_size;
  compressor->get_max_header_size = lz4_get_max_header_size;
  compressor->get_compressed_size = lz4_get_compressed_size;
  compressor->detect_format = lz4_detect_format;

  return 0;
}

static int compressor_init_zstd(Compressor *compressor, int level) {
  if (!compressor)
    return -1;

  // Get ZSTD's actual limits
  // According to current zstd.h:
  // level=-131072 is the minimum (see ZSTD_minCLevel())
  // level=22 is the max (see ZSTD_maxCLevel())
  // level=3 is the default (see ZSTD_defaultCLevel())
  int zstd_min = ZSTD_minCLevel();
  int zstd_max = ZSTD_maxCLevel();

  // Validate level
  if (level < zstd_min)
    level = zstd_min;
  if (level > zstd_max)
    level = zstd_max;

  compressor->algorithm = COMPRESSION_ZSTD;
  compressor->level = level;
  compressor->compress_data = zstd_compress_data;
  compressor->decompress_data = zstd_decompress_data;
  compressor->get_compress_bound = zstd_get_compress_bound;
  compressor->get_original_file_size = zstd_get_original_file_size;
  compressor->get_max_header_size = zstd_get_max_header_size;
  compressor->get_compressed_size = zstd_get_compressed_size;
  compressor->detect_format = zstd_detect_format;

  return 0;
}

int compressor_init(Compressor *compressor, compression_algorithm_t algorithm,
                    int level) {
  if (!compressor) {
    return -1;
  }

  switch (algorithm) {
  case COMPRESSION_LZ4:
    return compressor_init_lz4(compressor, level);

  case COMPRESSION_ZSTD:
    return compressor_init_zstd(compressor, level);

  default:
    return -1; // Invalid algorithm
  }
}
