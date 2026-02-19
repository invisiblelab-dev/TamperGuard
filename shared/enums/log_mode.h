#ifndef LOG_MODE_H
#define LOG_MODE_H

#include <stdint.h>

/**
 * @file log_mode.h
 * @brief Logging mode enumeration
 *
 * This header defines the enumeration for different logging modes
 * available in the system.
 */

/**
 * @enum LogMode
 * @brief Enumeration of available logging modes
 *
 * Each mode represents a different level of logging verbosity or
 * destination for log messages.
 */
typedef enum {
  LOG_DISABLED, /**< Logging disabled */
  LOG_SCREEN,   /**< Log to screen/console */
  LOG_ERROR,    /**< Log error messages only */
  LOG_WARN,     /**< Log warning messages and above */
  LOG_INFO,     /**< Log informational messages and above */
  LOG_DEBUG     /**< Log debug messages and above (most verbose) */
} LogMode;

#endif /* LOG_MODE_H */
