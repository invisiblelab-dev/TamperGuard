#include "passthrough_ops.h"
#include "../../logdef.h"
#include "../../shared/types/layer_context.h"

/**
 * @brief Passthrough implementation of pread operation
 *
 * This function acts as a passthrough layer for pread operations. It simply
 * forwards the pread request to the next layer in the stack without any
 * modification or processing.
 *
 * @param fd File descriptor to read from
 * @param buff Buffer to store the read data
 * @param nbyte Number of bytes to read
 * @param offset Offset in the file to start reading from
 * @param l Layer context containing information about the current and next
 * layers
 * @return Number of bytes read on success
 */
ssize_t passthrough_pread(int fd, void *buff, size_t nbyte, off_t offset,
                          LayerContext l) {

  DEBUG_MSG("Passthrough read\n");
  // In demultiplexer context, passthrough means this layer doesn't perform any
  // operation. For now, just return nbyte to indicate success. The
  // demultiplexer will handle the actual operation distribution.
  return (ssize_t)nbyte;
}

/**
 * @brief Passthrough implementation of pwrite operation
 *
 * This function acts as a passthrough layer for pwrite operations. It simply
 * forwards the pwrite request to the next layer in the stack without any
 * modification or processing.
 *
 * @param fd File descriptor to write to
 * @param buff Buffer containing the data to write
 * @param nbyte Number of bytes to write
 * @param offset Offset in the file to start writing to
 * @param l Layer context containing information about the current and next
 * layers
 * @return Number of bytes written on success
 */
ssize_t passthrough_pwrite(int fd, const void *buff, size_t nbyte, off_t offset,
                           LayerContext l) {
  DEBUG_MSG("Passthrough write\n");
  // In demultiplexer context, passthrough means this layer doesn't perform any
  // operation

  // For now, just return nbyte to indicate success
  // The demultiplexer will handle the actual operation distribution
  return (ssize_t)nbyte;
}
