#include "logdef.h"
#include "types/services_context.h"
#include <assert.h>
#include <rocksdb/c.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

rocksdb_t *db_instance = NULL;

rocksdb_options_t *options = NULL;
rocksdb_readoptions_t *readoptions = NULL;
rocksdb_writeoptions_t *writeoptions = NULL;

void metadata_init(ServiceConfig *config) {

  char *db_path = "./testdb";

  rocksdb_options_t *options = rocksdb_options_create();
  rocksdb_options_optimize_level_style_compaction(options, 0);

  size_t cpus = sysconf(_SC_NPROCESSORS_ONLN);

  if (config != NULL && config->type == SERVICE_METADATA) {
    cpus = config->service.metadata.num_background_threads;
  }

  rocksdb_options_increase_parallelism(options, (int)cpus);
  rocksdb_options_set_create_if_missing(options, 1);

  char *err = NULL;
  db_instance = rocksdb_open(options, db_path, &err);
  if (err) {
    if (DEBUG_ENABLED())
      DEBUG_MSG("Error opening rocksdb\n");

    rocksdb_free(err);
    exit(1);
  }

  readoptions = rocksdb_readoptions_create();
  writeoptions = rocksdb_writeoptions_create();
}

int metadata_put(char *key, size_t key_size, char *value, size_t value_size) {
  char *err = NULL;
  rocksdb_put(db_instance, writeoptions, key, key_size, value, value_size,
              &err);
  if (err != NULL) {
    rocksdb_free(err);
    return -1;
  }
  return 0;
}

void *metadata_get(char *key, size_t key_size, size_t *value_size) {
  char *err = NULL;
  char *returned_value =
      rocksdb_get(db_instance, readoptions, key, key_size, value_size, &err);
  if (err != NULL) {
    perror(err);
    rocksdb_free(err);
  }
  return returned_value;
}

int metadata_delete(char *key, size_t key_size) {
  char *err = NULL;
  rocksdb_delete(db_instance, writeoptions, key, key_size, &err);
  if (err != NULL) {
    perror(err);
    rocksdb_free(err);
    return -1;
  }
  return 0;
}

void metadata_close() {
  if (readoptions)
    rocksdb_readoptions_destroy(readoptions);
  if (writeoptions)
    rocksdb_writeoptions_destroy(writeoptions);
  if (db_instance)
    rocksdb_close(db_instance);
  if (options)
    rocksdb_options_destroy(options);

  readoptions = NULL;
  writeoptions = NULL;
  db_instance = NULL;
  options = NULL;
}

void metadata_free(void *ptr) { rocksdb_free(ptr); };
