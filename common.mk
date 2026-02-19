#==============================================================================
# Common Build Configuration
# This file is included by the root Makefile and example Makefiles
#==============================================================================

# Determine the root directory based on where this file is included from
ifndef ROOT_DIR
    MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
    ROOT_DIR := $(abspath $(MAKEFILE_DIR))
endif

# If this is being called from an example directory, adjust ROOT_DIR
ifeq ($(notdir $(ROOT_DIR)),examples)
    ROOT_DIR := $(abspath $(ROOT_DIR)/..)
endif

# Build directories (absolute paths for compatibility)
BUILD_DIR := $(ROOT_DIR)/build
LAYERS_BUILD_DIR := $(ROOT_DIR)/build/layers
ROOT_BUILD_DIR := $(ROOT_DIR)/build
BIN_DIR := $(ROOT_DIR)/bin
LIB_DIR := $(ROOT_DIR)/lib
BUILD_LIB_DIR := $(BUILD_DIR)/lib
LOG_DIR := $(ROOT_DIR)/logs
SHARED_BUILD_DIR := $(BUILD_DIR)/shared
UTILS_BUILD_DIR := $(SHARED_BUILD_DIR)/utils
SERVICES_BUILD_DIR := $(BUILD_DIR)/services
# CacheLib library path
CACHELIB_DIR = $(ROOT_DIR)/lib/CacheLib

# RocksDB library paths
ROCKSDB_DIR = $(ROOT_DIR)/lib/rocksdb
ROCKSDB_INCLUDE_PATH = $(ROCKSDB_DIR)/include/

# Zlog library paths (defined early for use in BASE_INCLUDES)
ZLOG_DIR = $(ROOT_DIR)/lib/zlog
ZLOG_LIB_PATH = $(ZLOG_DIR)/src
ZLOG_INCLUDE_PATH = $(ZLOG_DIR)/src

# Compression library paths
LZ4_DIR = $(ROOT_DIR)/lib/lz4
ZSTD_DIR = $(ROOT_DIR)/lib/zstd
LZ4_LIB_PATH = $(LZ4_DIR)/lib
ZSTD_LIB_PATH = $(ZSTD_DIR)/lib

# Compiler configuration
CC := gcc
CPP := g++
BASE_CFLAGS := -Wall -g -Wno-unknown-pragmas
BASE_INCLUDES := -I$(ROOT_DIR) -I$(ROOT_DIR)/layers -I$(ROOT_DIR)/shared \
                 -I$(ZLOG_INCLUDE_PATH) -I$(ROCKSDB_INCLUDE_PATH) -I$(LZ4_DIR)/lib -I$(ZSTD_DIR)/lib \
                 $(shell pkg-config --cflags glib-2.0)

# Common dependencies
SHARED_DEPS = $(ROOT_DIR)/lib.h \
              $(ROOT_DIR)/layers/local/local.h \
              $(ROOT_DIR)/layers/remote/remote.h \
              $(ROOT_DIR)/layers/demultiplexer/demultiplexer.h \
              $(ROOT_DIR)/layers/demultiplexer/passthrough_ops.h \
              $(ROOT_DIR)/layers/anti_tampering/anti_tampering.h \
              $(ROOT_DIR)/layers/block_align/block_align.h \
              $(ROOT_DIR)/config/declarations.h \
              $(ROOT_DIR)/lib/tomlc17/src/tomlc17.h \
              $(ROOT_DIR)/layers/encryption/encryption.h \
              $(ROOT_DIR)/layers/compression/compression.h \
              $(ROOT_DIR)/layers/benchmark/benchmark.h \
              $(ROOT_DIR)/layers/cache/read_cache/read_cache.h \
              $(ROOT_DIR)/shared/utils/parallel.h \
              $(ROOT_DIR)/shared/utils/locking.h \
              $(ROOT_DIR)/shared/utils/hasher/hasher.h \
              $(ROOT_DIR)/shared/utils/hasher/evp.h \
              $(ROOT_DIR)/shared/utils/hasher/sha256_hasher.h \
              $(ROOT_DIR)/shared/utils/hasher/sha512_hasher.h \
              $(ROOT_DIR)/services/metadata.h

# Common shared objects
SHARED_OBJS = $(ROOT_BUILD_DIR)/lib.o \
              $(ROOT_BUILD_DIR)/logdef.o \
              $(LAYERS_BUILD_DIR)/local.o \
              $(LAYERS_BUILD_DIR)/remote.o \
              $(LAYERS_BUILD_DIR)/demultiplexer.o \
              $(LAYERS_BUILD_DIR)/passthrough_ops.o \
              $(LAYERS_BUILD_DIR)/enforcement.o \
              $(LAYERS_BUILD_DIR)/anti_tampering.o \
              $(LAYERS_BUILD_DIR)/block_anti_tampering.o \
              $(LAYERS_BUILD_DIR)/anti_tampering_utils.o \
              $(LAYERS_BUILD_DIR)/block_align.o \
              $(LAYERS_BUILD_DIR)/benchmark.o \
              $(LAYERS_BUILD_DIR)/read_cache.o \
              $(LAYERS_BUILD_DIR)/encryption.o \
              $(LAYERS_BUILD_DIR)/aes_xts.o \
              $(ROOT_BUILD_DIR)/loader.o \
              $(ROOT_BUILD_DIR)/parser.o \
              $(ROOT_BUILD_DIR)/builder.o \
              $(ROOT_BUILD_DIR)/toml.o \
              $(LAYERS_BUILD_DIR)/compressor.o \
              $(LAYERS_BUILD_DIR)/compression.o \
              $(LAYERS_BUILD_DIR)/sparse_block.o \
              $(LAYERS_BUILD_DIR)/compression_utils.o \
              $(UTILS_BUILD_DIR)/parallel.o \
              $(UTILS_BUILD_DIR)/locking.o \
              $(UTILS_BUILD_DIR)/conversion.o \
              $(UTILS_BUILD_DIR)/hasher/hasher.o \
              $(UTILS_BUILD_DIR)/hasher/evp.o \
              $(UTILS_BUILD_DIR)/hasher/sha256_hasher.o \
              $(UTILS_BUILD_DIR)/hasher/sha512_hasher.o \
              $(SERVICES_BUILD_DIR)/libmetadata.so \

# External library paths
INVISIBLE_LIB_DIR = $(ROOT_DIR)/lib/invisible-storage-bindings
INVISIBLE_TARGET_DIR = $(INVISIBLE_LIB_DIR)/target/release
INVISIBLE_LIB_PATH = $(INVISIBLE_TARGET_DIR)

# Base library configuration - link to built libraries
BASE_LIBS = `pkg-config --cflags --libs glib-2.0` -L$(ZLOG_LIB_PATH) -lzlog -L$(SERVICES_BUILD_DIR) -lmetadata -L$(LZ4_LIB_PATH) -llz4 -L$(ZSTD_LIB_PATH) -lzstd -lpthread -lcrypto -lcurl -Wl,-rpath=$(ZLOG_LIB_PATH):$(LZ4_LIB_PATH):$(ZSTD_LIB_PATH):$(LAYERS_BUILD_DIR):$(SERVICES_BUILD_DIR)

#==============================================================================
# Common Build Rules for Shared Objects
#==============================================================================

# Function to create fallback rules for shared objects
define create_fallback_rule
ifndef AM_I_THE_ROOT_MAKEFILE
$(1):
	@echo "Building shared object $$@ from example Makefile (fallback)..."
	$$(MAKE) -C $$(ROOT_DIR) -f Makefile $(1)
endif
endef

# Generate fallback rules for all shared objects
$(eval $(call create_fallback_rule,$(ROOT_BUILD_DIR)/lib.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/local.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/remote.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/demultiplexer.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/passthrough_ops.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/anti_tampering.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/block_anti_tampering.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/anti_tampering_utils.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/block_align.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/benchmark.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/read_cache.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/compression.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/sparse_block.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/compression_utils.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/compressor.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/encryption.o))
$(eval $(call create_fallback_rule,$(LAYERS_BUILD_DIR)/aes_xts.o))
$(eval $(call create_fallback_rule,$(ROOT_BUILD_DIR)/loader.o))
$(eval $(call create_fallback_rule,$(ROOT_BUILD_DIR)/parser.o))
$(eval $(call create_fallback_rule,$(ROOT_BUILD_DIR)/builder.o))
$(eval $(call create_fallback_rule,$(ROOT_BUILD_DIR)/toml.o))
$(eval $(call create_fallback_rule,$(UTILS_BUILD_DIR)/locking.o))
$(eval $(call create_fallback_rule,$(UTILS_BUILD_DIR)/parallel.o))
$(eval $(call create_fallback_rule,$(UTILS_BUILD_DIR)/hasher/hasher.o))
$(eval $(call create_fallback_rule,$(UTILS_BUILD_DIR)/hasher/sha256_hasher.o))
$(eval $(call create_fallback_rule,$(UTILS_BUILD_DIR)/hasher/sha512_hasher.o))
$(eval $(call create_fallback_rule,$(SERVICES_BUILD_DIR)/metadata.o))

#==============================================================================
# Common Utility Functions
#==============================================================================

# Function to create example build directory
define create_example_build_dir
	@mkdir -p $(1)
endef

# Export common variables
export CC BASE_CFLAGS BASE_INCLUDES
export BUILD_DIR BIN_DIR LIB_DIR BUILD_LIB_DIR LAYERS_BUILD_DIR ROOT_BUILD_DIR LOG_DIR SERVICES_BUILD_DIR ROCKSDB_DIR
export SHARED_DEPS SHARED_OBJS BASE_LIBS
export INVISIBLE_LIB_PATH

export INVISIBLE_LIB_PATH

export INVISIBLE_LIB_PATH
