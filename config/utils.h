#ifndef __CONFIG_UTILS_H__
#define __CONFIG_UTILS_H__
#include "../lib/tomlc17/src/tomlc17.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void toml_error(const char *msg) {
  fprintf(stderr, "ERROR: %s\n", msg);
  exit(1);
}

/**
 * @brief Parse string array from TOML array
 */
static inline char **parse_string_array(toml_datum_t arr, int *count) {
  if (arr.type != TOML_ARRAY) {
    *count = 0;
    return NULL;
  }

  *count = arr.u.arr.size;
  if (*count == 0)
    return NULL;

  char **result = (char **)malloc(sizeof(char *) * (*count));
  if (!result) {
    toml_error("Failed to allocate memory for string array");
  }

  for (int i = 0; i < *count; i++) {
    toml_datum_t elem = arr.u.arr.elem[i];
    if (elem.type != TOML_STRING) {
      toml_error("Expected string in array");
    }
    result[i] = strdup(elem.u.s);
    if (!result[i]) {
      toml_error("Failed to duplicate string");
    }
  }

  return result;
}

/**
 * @brief Safely extract string from TOML datum
 */
static inline char *parse_string(toml_datum_t datum) {
  return (datum.type == TOML_STRING) ? strdup(datum.u.s) : NULL;
}

static inline long parse_long(toml_datum_t datum) {

  if (datum.type == TOML_INT64) {
    return datum.u.int64;
  } else {
    return -1;
  }
}

static inline int parse_int(toml_datum_t datum) {

  if (datum.type == TOML_INT64) {
    return (int)datum.u.int64;
  } else {
    return -1;
  }
}

/**
 * @brief Check if a layer name exists in a string array
 * @param string_array Array of strings to search in
 * @param array_size Size of the string array
 * @param layer_name Layer name to search for
 * @return 1 if found, 0 if not found
 */
static inline int is_layer_in_array(char **string_array, int array_size,
                                    const char *layer_name) {
  if (string_array == NULL) {
    return 0;
  }

  for (int i = 0; i < array_size; i++) {
    if (strcmp(string_array[i], layer_name) == 0) {
      return 1;
    }
  }

  return 0;
}
#endif // __CONFIG_UTILS_H__
