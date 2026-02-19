/*
  LogDef inspired by the version of the SafeFS project
  (https://github.com/safecloud-project/safefs)
*/

#include "lib/zlog/src/zlog.h"
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "logdef.h"

struct zlog_category_s *CATEGORY;

#define MAX_CFG_LINE 4096

typedef void (*debug_func_t)(const char *format, va_list args);
typedef void (*error_func_t)(const char *format, va_list args);
typedef void (*screen_func_t)(const char *format, va_list args);
typedef void (*info_func_t)(const char *format, va_list args);
typedef void (*warn_func_t)(const char *format, va_list args);

/**
 * Check if a file exists
 * @param filename Path to the file to check
 * @returns 1 if file exists, 0 otherwise
 */
static int file_exists(const char *filename) {
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

/**
 * Check if the directory hosting the file stored at path exists and is a
 * directory.
 * @param path
 * @returns 0 if OK, -1 if not
 */
static int check_directory_existence(char *path) {
  char *directory = dirname(path);
  struct stat s;
  int err = stat(directory, &s);
  if (-1 == err) {
    if (ENOENT == errno) {
      (void)fprintf(stderr, "The logging directory %s does not exist\n",
                    directory);
      return -1;
    } else {
      perror("stat");
      return -1;
    }
  } else {
    if (!S_ISDIR(s.st_mode)) {
      (void)fprintf(stderr, "The logging destination %s is not a directory\n",
                    directory);
      return -1;
    }
  }
  return 0;
}

/**
 * Simple environment variable expansion for %E(VAR) syntax
 * @param path Input path that may contain %E(VAR) syntax
 * @param expanded_path Output buffer for expanded path
 * @param size Size of output buffer
 * @returns 0 on success, -1 on failure
 */
static int expand_env_vars(const char *path, char *expanded_path, size_t size) {
  const char *p = path;
  char *out = expanded_path;
  char *out_end = expanded_path + size - 1;

  while (*p && out < out_end) {
    if (*p == '%' && *(p + 1) == 'E' && *(p + 2) == '(') {
      // Found %E( - extract environment variable name
      p += 3; // Skip %E(
      const char *var_start = p;
      while (*p && *p != ')')
        p++;
      if (*p != ')') {
        (void)fprintf(stderr, "Malformed environment variable in path: %s\n",
                      path);
        return -1;
      }

      // Extract variable name
      size_t var_len = p - var_start;
      char var_name[256];
      if (var_len >= sizeof(var_name)) {
        (void)fprintf(stderr,
                      "Environment variable name too long in path: %s\n", path);
        return -1;
      }
      strncpy(var_name, var_start, var_len);
      var_name[var_len] = '\0';

      // Get environment variable value
      const char *var_value = getenv(var_name);
      if (!var_value) {
        (void)fprintf(stderr, "Environment variable %s not set in path: %s\n",
                      var_name, path);
        return -1;
      }

      // Copy variable value to output
      size_t value_len = strlen(var_value);
      if (out + value_len >= out_end) {
        (void)fprintf(stderr, "Expanded path too long: %s\n", path);
        return -1;
      }
      strcpy(out, var_value);
      out += value_len;

      p++; // Skip closing )
    } else {
      // Regular character
      *out++ = *p++;
    }
  }

  *out = '\0';
  return 0;
}

/**
 * Check if the directories hosting the log files actually exist. If they don't,
 * the program exits with return code EXIT_FAILURE (1).
 * @parm path Path to the zlog configuration file to test
 */
static void check_log_file(char *zlog_config_path) {
  char buffer[MAX_CFG_LINE + 1];
  FILE *conf_file = fopen(zlog_config_path, "r");
  char path[256];
  char expanded_path[512];
  while (fgets(buffer, MAX_CFG_LINE, conf_file) != NULL) {
    path[0] = '\0';
    (void)sscanf(buffer, "modular_lib.DEBUG \"%s\"", path);
    if (strlen(path) > 0) {
      // Remove trailing quote if present
      if (path[strlen(path) - 1] == '"') {
        path[strlen(path) - 1] = '\0';
      }
      // Expand environment variables
      if (expand_env_vars(path, expanded_path, sizeof(expanded_path)) == 0) {
        if (check_directory_existence(expanded_path) != 0) {
          exit(EXIT_FAILURE);
        }
      } else {
        exit(EXIT_FAILURE);
      }
    }
    path[0] = '\0';
    (void)sscanf(buffer, "modular_lib.ERROR \"%s\"", path);
    if (strlen(path) > 0) {
      // Remove trailing quote if present
      if (path[strlen(path) - 1] == '"') {
        path[strlen(path) - 1] = '\0';
      }
      // Expand environment variables
      if (expand_env_vars(path, expanded_path, sizeof(expanded_path)) == 0) {
        if (check_directory_existence(expanded_path) != 0) {
          exit(EXIT_FAILURE);
        }
      } else {
        exit(EXIT_FAILURE);
      }
    }
  }
  (void)fclose(conf_file);
}

/**
 * An empty function that does not do anything but can be used in replacement of
 * the other logging functions.
 * @param format Format of the message
 * @param args A list of arguments
 */
static void DROP_MSG(const char *format, va_list args) {}

/**
 * A function that logs debug level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
static void ACTIVE_DEBUG_MSG(const char *format, va_list args) {
  assert(CATEGORY != NULL);
  vzlog_debug(CATEGORY, format, args);
}

/**
 * A function that logs error level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_ERROR_MSG(const char *format, va_list args) {
  assert(CATEGORY != NULL);
  vzlog_error(CATEGORY, format, args);
}

/**
 * A function that logs info level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_INFO_MSG(const char *format, va_list args) {
  assert(CATEGORY != NULL);
  vzlog_info(CATEGORY, format, args);
}

/**
 * A function that logs warning level messages using vzlog.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_WARN_MSG(const char *format, va_list args) {
  assert(CATEGORY != NULL);
  vzlog_warn(CATEGORY, format, args);
}

/**
 * A function that logs messages on the screen using vprintf.
 * @param format Format of the message
 * @param args A list of arguments accompanying the format
 */
void ACTIVE_SCREEN_MSG(const char *format, va_list args) {
  assert(CATEGORY != NULL);
  vprintf(format, args);
}

static debug_func_t debug = DROP_MSG;
static error_func_t error = DROP_MSG;
static screen_func_t screen = DROP_MSG;
static info_func_t info = DROP_MSG;
static warn_func_t warn = DROP_MSG;

/**
 * Initialize zlog infrastructure (for modes that need file logging)
 */
static void init_zlog() {
  char *zlog_config_path = LOCAL_ZLOGCONFIG_PATH;

  if (!zlog_config_path && file_exists(DEFAULT_ZLOGCONFIG_PATH)) {
    zlog_config_path = DEFAULT_ZLOGCONFIG_PATH;
  }

  if (!zlog_config_path) {
    (void)fprintf(stderr, "[logdef::LOG_INIT] Could not find any configuration "
                          "file for zlog!\n");
    exit(EXIT_FAILURE);
  }

  int rc = zlog_init(zlog_config_path);
  if (rc) {
    (void)fprintf(
        stderr,
        "[logdef::LOG_INIT] Could not load zlog configuration file (rc = "
        "%d)\n",
        rc);
    exit(EXIT_FAILURE);
  }

  check_log_file(zlog_config_path);
  CATEGORY = zlog_get_category("modular_lib");
  if (!CATEGORY) {
    (void)fprintf(stderr,
                  "[logdef::LOG_INIT] Could not load category modular_lib "
                  "from zlog.conf\n");
    exit(EXIT_FAILURE);
  }
}

/**
 * Initializes the logging infrastructure by looking at the various
 * configuration files of the project.
 * @param mode Logging level: 0=disabled, 1=screen only, 2=error+screen,
 * 3=warn+error+screen, 4=info+warn+error+screen, 5=debug+info+warn+error+screen
 */
void LOG_INIT(LogMode mode) {
  switch (mode) {
  case LOG_DISABLED:
    // All logging disabled
    debug = DROP_MSG;
    error = DROP_MSG;
    warn = DROP_MSG;
    info = DROP_MSG;
    screen = DROP_MSG;
    (void)fprintf(stdout, "logging disabled\n");
    break;

  case LOG_SCREEN:
    // Screen only (no zlog needed)
    debug = DROP_MSG;
    error = DROP_MSG;
    warn = DROP_MSG;
    info = DROP_MSG;
    screen = ACTIVE_SCREEN_MSG;
    (void)fprintf(stdout, "screen logging enabled (terminal output only)\n");
    break;

  case LOG_ERROR:
    // Error + Screen
    init_zlog();
    debug = DROP_MSG;
    error = ACTIVE_ERROR_MSG;
    warn = DROP_MSG;
    info = DROP_MSG;
    screen = ACTIVE_SCREEN_MSG;
    (void)fprintf(stdout, "error level logging enabled (error + screen)\n");
    break;

  case LOG_WARN:
    // Warn + Error + Screen
    init_zlog();
    debug = DROP_MSG;
    error = ACTIVE_ERROR_MSG;
    warn = ACTIVE_WARN_MSG;
    info = DROP_MSG;
    screen = ACTIVE_SCREEN_MSG;
    (void)fprintf(stdout,
                  "warning level logging enabled (warn + error + screen)\n");
    break;

  case LOG_INFO:
    // Info + Warn + Error + Screen
    init_zlog();
    debug = DROP_MSG;
    error = ACTIVE_ERROR_MSG;
    warn = ACTIVE_WARN_MSG;
    info = ACTIVE_INFO_MSG;
    screen = ACTIVE_SCREEN_MSG;
    (void)fprintf(
        stdout, "info level logging enabled (info + warn + error + screen)\n");
    break;

  case LOG_DEBUG:
    // Debug + Info + Warn + Error + Screen (all)
    init_zlog();
    debug = ACTIVE_DEBUG_MSG;
    error = ACTIVE_ERROR_MSG;
    warn = ACTIVE_WARN_MSG;
    info = ACTIVE_INFO_MSG;
    screen = ACTIVE_SCREEN_MSG;
    (void)fprintf(
        stdout,
        "debug level logging enabled (debug + info + warn + error + screen)\n");
    break;

  default:
    // Invalid mode - default to disabled
    debug = DROP_MSG;
    error = DROP_MSG;
    warn = DROP_MSG;
    info = DROP_MSG;
    screen = DROP_MSG;
    (void)fprintf(stdout, "invalid logging mode %d, logging disabled\n",
                  (int)mode);
    break;
  }
}

/**
 * Tears down the logging infrastructure
 */
void LOG_EXIT(LogMode mode) {
  if (mode) {
    zlog_fini();
  }
}

void DEBUG_MSG(const char *format, ...) {
  va_list args;
  va_start(args, format);
  debug(format, args);
  va_end(args);
}

void ERROR_MSG(const char *format, ...) {
  va_list args;
  va_start(args, format);
  error(format, args);
  va_end(args);
}

void INFO_MSG(const char *format, ...) {
  va_list args;
  va_start(args, format);
  info(format, args);
  va_end(args);
}

void WARN_MSG(const char *format, ...) {
  va_list args;
  va_start(args, format);
  warn(format, args);
  va_end(args);
}

void SCREEN_MSG(const char *format, ...) {
  va_list args;
  va_start(args, format);
  screen(format, args);
  va_end(args);
}

int DEBUG_ENABLED() { return debug != DROP_MSG; }

int ERROR_ENABLED() { return error != DROP_MSG; }

int SCREEN_ENABLED() { return screen != DROP_MSG; }

int INFO_ENABLED() { return info != DROP_MSG; }

int WARN_ENABLED() { return warn != DROP_MSG; }
