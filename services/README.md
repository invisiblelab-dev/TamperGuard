# Metadata Service

This README describes how the **persistent metadata service** works and its public API.

## Overview

The persistent metadata service provides:

- **Generic way to store metadata** across the whole project.
- **Persistent metadata storage**, allowing metadata to be reloaded when the program using the library restarts.

## Key Features

- **Use of RocksDB** to ensure thread safety and high performance.
- **Simple API** for simple and homogeneous usage across the codebase.

## API

- **metadata_init**  
  Initializes the metadata service and allocates all required internal structures.  
  It receives a `ServiceConfig` structure that configures a variety of RocksDB's
  parameters (currently, only the number of background threads and the cache size
  are supported).

- **metadata_put**  
  Stores or updates metadata associated with a given key.

- **metadata_get**  
  Retrieves metadata associated with a given key.  
  Returns a pointer to memory allocated internally by the service, which **must**
  be freed using `metadata_free(...)`.

- **metadata_delete**  
  Removes metadata associated with a given key.

- **metadata_free**  
  Frees any memory returned by the metadata service that was allocated internally
  (for example, by `metadata_get`).

- **metadata_close**  
  Shuts down the metadata service and releases all internal resources.

## How to use
```
  // called when initializing the library, so no need to call this in a layer
  metadata_init(NULL);

  char *key = "key_test";
  char *value = "test";
  size_t value_size;

  int ret = metadata_put(key, strlen(key), value, strlen(value) + 1);

  void* ret_value = metadata_get(key, strlen(key), &value_size);
  metadata_free(ret_value);

  // like init, this shouldn't be called in a layer
  metadata_close();
```

## Example TOML config:
```toml
[service]
type = "metadata"
threads = 2
cache_size = 1048576  # in bytes
```

## Future Improvements

- **Configurable key-value backend**  
  Allow the metadata service to be configured to use different key-value stores
  (e.g., RocksDB, LevelDB).

- **Metadata deduplication**  
  Deduplicate metadata entries that are shared across multiple layers.

