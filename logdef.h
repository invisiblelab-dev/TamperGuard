/*
  LogDef inspired by the version of the SafeFS project
  (https://github.com/safecloud-project/safefs)
*/

#ifndef __LOGDEF_H__
#define __LOGDEF_H__

#include "shared/enums/log_mode.h"

#define LOCAL_ZLOGCONFIG_PATH "zlog.conf"
#define DEFAULT_ZLOGCONFIG_PATH "/etc/modular-lib/zlog.conf"
/**
 * Initializes the logging facilities
 */
void LOG_INIT(LogMode mode);

/**
 * Tears down the logging facilities
 */
void LOG_EXIT(LogMode mode);

/**
 * Logs a debug message
 * @param format Format message
 */
void DEBUG_MSG(const char *format, ...);

/**
 * Check if debug logging is currently enabled
 */
int DEBUG_ENABLED();

/**
 * Check if error logging is currently enabled
 */
int ERROR_ENABLED();

/**
 * Check if screen (stdout) logging is currently enabled
 */
int SCREEN_ENABLED();

/**
 * Check if info logging is currently enabled
 */
int INFO_ENABLED();

/**
 * Check if warning logging is currently enabled
 */
int WARN_ENABLED();

/**
 * Logs an error message
 * @param format Format message
 */
void ERROR_MSG(const char *format, ...);

/**
 * Prints a message on the screen
 * @param format Format message
 */
void SCREEN_MSG(const char *format, ...);

/**
 * Logs an info message
 * @param format Format message
 */
void INFO_MSG(const char *format, ...);

/**
 * Logs a warning message
 * @param format Format message
 */
void WARN_MSG(const char *format, ...);

#endif /*__LOGDEF_H__*/
