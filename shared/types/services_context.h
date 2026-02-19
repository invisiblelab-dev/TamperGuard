#ifndef SERVICES_CONTEXT_H
#define SERVICES_CONTEXT_H
#include <stddef.h>

// cache_size_bytes its not being used currently
typedef struct metadata_service {
  size_t num_background_threads;
  size_t cache_size_bytes;
} MetadataService;

typedef union service_union {
  MetadataService metadata;
} ServiceUnion;

typedef enum { SERVICE_METADATA } ServiceType;

typedef struct service_config {
  ServiceUnion service;
  ServiceType type;
} ServiceConfig;
#endif
