#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include "../../shared/types/layer_context.h"
#include "config.h"
/**
 * @brief Initializes the benchmark layer context.
 *
 * @param next_layer Pointer to the next layer context.
 * @param nlayers Number of layers in the stack.
 * @param ops_rep Number of repetitions for benchmarking operations.
 * @return Initialized LayerContext for the benchmark layer.
 */
LayerContext benchmark_init(LayerContext *next_layer, int nlayers, int ops_rep);

/**
 * @brief Opens a file.
 *
 * @param pathname Path to the file.
 * @param flags Open flags (e.g., O_RDONLY).
 * @param mode File mode (used when creating).
 * @param l Benchmark layer context.
 * @return File descriptor, or -1 on error.
 */
int benchmark_open(const char *pathname, int flags, mode_t mode,
                   LayerContext l);

/**
 * @brief Closes a file descriptor.
 *
 * @param fd File descriptor.
 * @param l Benchmark layer context.
 * @return 0 on success, or -1 on error.
 */
int benchmark_close(int fd, LayerContext l);

/**
 * @brief Performs lstat as many times as the user configured the reps parameter
 * in the layer config.
 *
 * @param pathname Path to the file.
 * @param stbuf Pointer to struct stat to fill.
 * @param l Benchmark layer context.
 * @return 0 on success, or -1 on error.
 */
int benchmark_lstat(const char *pathname, struct stat *stbuf, LayerContext l);

/**
 * @brief Performs fstat as many times as the user configured the reps parameter
 * in the layer config.
 *
 * @param fd File descriptor.
 * @param stbuf Pointer to struct stat to fill.
 * @param l Benchmark layer context.
 * @return 0 on success, or -1 on error.
 */
int benchmark_fstat(int fd, struct stat *stbuf, LayerContext l);

/**
 * @brief Reads from a file at a given offset as many times as the user
 * configured the reps parameter in the layer config.
 *
 * @param fd File descriptor.
 * @param buffer Destination buffer.
 * @param nbytes Number of bytes to read.
 * @param offset Offset to read from.
 * @param l Benchmark layer context.
 * @return Number of bytes read, or -1 on error.
 */
ssize_t benchmark_pread(int fd, void *buffer, size_t nbytes, off_t offset,
                        LayerContext l);

/**
 * @brief Writes to a file at a given offset as many times as the user
 * configured the reps parameter in the layer config.
 *
 * @param fd File descriptor.
 * @param buffer Source buffer.
 * @param nbytes Number of bytes to write.
 * @param offset Offset to write to.
 * @param l Benchmark layer context.
 * @return Number of bytes written, or -1 on error.
 */
ssize_t benchmark_pwrite(int fd, const void *buffer, size_t nbytes,
                         off_t offset, LayerContext l);

/**
 * @brief Truncates a file to a specified length as many times as the user
 * configured the reps parameter in the layer config.
 *
 * @param fd File descriptor.
 * @param length Desired file length.
 * @param l Benchmark layer context.
 * @return 0 on success, or -1 on error.
 */
int benchmark_ftruncate(int fd, off_t length, LayerContext l);

/**
 * @brief Unlinks a file.
 *
 * @param pathname Path to the file.
 * @param l Benchmark layer context.
 * @return 0 on success, or -1 on error.
 */
int benchmark_unlink(const char *pathname, LayerContext l);
#endif
