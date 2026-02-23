#==============================================================================
# Test Makefile
#==============================================================================

# Determine paths
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

# Include common configuration
include $(ROOT_DIR)/common.mk

PIC_CFLAGS := $(CFLAGS) -fPIC

# Test directories
TEST_DIR = $(ROOT_DIR)/tests
UNIT_DIR = $(TEST_DIR)/unit
TESTS_BUILD_DIR := $(BUILD_DIR)/tests
TESTS_BIN_DIR := $(BIN_DIR)/tests

# Test objects
UNIT_OBJS = $(TESTS_BUILD_DIR)/layers/block_align/test_block_align_config.o \
			$(TESTS_BUILD_DIR)/layers/block_align/test_block_align.o \
            $(TESTS_BUILD_DIR)/layers/benchmark/test_benchmark.o \
			$(TESTS_BUILD_DIR)/layers/read_cache/test_read_cache.o \
            $(TESTS_BUILD_DIR)/layers/compression/test_config.o \
            $(TESTS_BUILD_DIR)/shared/utils/compressor/test_compressor.o \
            $(TESTS_BUILD_DIR)/layers/local/test_local.o \
            $(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering.o \
            $(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering_block.o \
            $(TESTS_BUILD_DIR)/layers/demultiplexer/test_demultiplexer.o \
            $(TESTS_BUILD_DIR)/shared/utils/hasher/test_hasher.o \
            $(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha256.o \
	    $(TESTS_BUILD_DIR)/services/test_metadata.o \
            $(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha512.o

# Test binaries
UNIT_BINS = $(TESTS_BIN_DIR)/layers/block_align/test_block_align_config \
            $(TESTS_BIN_DIR)/layers/block_align/test_block_align \
            $(TESTS_BIN_DIR)/layers/benchmark/test_benchmark \
			$(TESTS_BIN_DIR)/layers/read_cache/test_read_cache \
            $(TESTS_BIN_DIR)/layers/compression/test_config \
            $(TESTS_BIN_DIR)/shared/utils/compressor/test_compressor \
            $(TESTS_BIN_DIR)/layers/local/test_local \
            $(TESTS_BIN_DIR)/layers/anti_tampering/test_anti_tampering \
            $(TESTS_BIN_DIR)/layers/anti_tampering/test_anti_tampering_block \
            $(TESTS_BIN_DIR)/layers/demultiplexer/test_demultiplexer \
            $(TESTS_BIN_DIR)/layers/compression/test_compression \
            $(TESTS_BIN_DIR)/layers/compression/test_sparse_block \
            $(TESTS_BIN_DIR)/shared/utils/hasher/test_hasher \
            $(TESTS_BIN_DIR)/shared/utils/hasher/test_sha256 \
	    $(TESTS_BIN_DIR)/services/test_metadata \
            $(TESTS_BIN_DIR)/shared/utils/hasher/test_sha512


# Test dependencies
TEST_DEPS = $(ROOT_DIR)/lib.h \
            $(ROOT_DIR)/layers/compression/compression.h \
	    	$(ROOT_DIR)/shared/utils/compressor/compressor.h \
            $(ROOT_DIR)/layers/compression/config.h \
            $(ROOT_DIR)/layers/local/local.h \
	    	$(ROOT_DIR)/layers/block_align/config.h \
	    	$(ROOT_DIR)/layers/block_align/block_align.h \
            $(ROOT_DIR)/layers/benchmark/benchmark.h \
	    	$(ROOT_DIR)/layers/cache/read_cache/read_cache.h \
            $(ROOT_DIR)/layers/demultiplexer/demultiplexer.h \
            $(ROOT_DIR)/shared/utils/parallel.h \
            $(ROOT_DIR)/shared/utils/hasher/hasher.h \
            $(ROOT_DIR)/shared/utils/hasher/sha256_hasher.h \
            $(ROOT_DIR)/shared/utils/hasher/sha512_hasher.h \
            $(ROOT_DIR)/lib/tomlc17/src/tomlc17.h \
	    $(ROOT_DIR)/services/metadata.h \
            $(TEST_DIR)/mock_layer.h

# Mock object
MOCK_OBJ = $(TESTS_BUILD_DIR)/mock_layer.o

# Mock object build rule
$(TESTS_BUILD_DIR)/mock_layer.o: $(TEST_DIR)/mock_layer.c $(TEST_DIR)/mock_layer.h
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

#==============================================================================
# Test Build Rules
#==============================================================================

# Unit tests
$(TESTS_BIN_DIR)/layers/compression/test_config: $(TESTS_BUILD_DIR)/layers/compression/test_config.o $(ROOT_BUILD_DIR)/toml.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/compression/test_config.o: $(UNIT_DIR)/layers/compression/test_config.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BUILD_DIR)/shared/utils/compressor/test_compressor.o: $(UNIT_DIR)/shared/utils/compressor/test_compressor.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/shared/utils/compressor/test_compressor: \
    $(TESTS_BUILD_DIR)/shared/utils/compressor/test_compressor.o \
    $(ROOT_BUILD_DIR)/layers/compressor.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/local/test_local.o: $(UNIT_DIR)/layers/local/test_local.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/local/test_local: \
    $(TESTS_BUILD_DIR)/layers/local/test_local.o \
    $(ROOT_BUILD_DIR)/layers/local.o \
	$(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BIN_DIR)/layers/block_align/test_block_align: \
	$(TESTS_BUILD_DIR)/layers/block_align/test_block_align.o \
	$(MOCK_OBJ) \
	$(ROOT_BUILD_DIR)/layers/block_align.o \
	$(ROOT_BUILD_DIR)/layers/local.o \
	$(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/block_align/test_block_align.o: $(UNIT_DIR)/layers/block_align/test_block_align.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/block_align/test_block_align_config: $(TESTS_BUILD_DIR)/layers/block_align/test_block_align_config.o $(ROOT_BUILD_DIR)/toml.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/block_align/test_block_align_config.o: $(UNIT_DIR)/layers/block_align/test_block_align_config.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BUILD_DIR)/layers/benchmark/test_benchmark.o: $(UNIT_DIR)/layers/benchmark/test_benchmark.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/benchmark/test_benchmark: \
  $(TESTS_BUILD_DIR)/layers/benchmark/test_benchmark.o \
  $(ROOT_BUILD_DIR)/layers/block_align.o \
  $(ROOT_BUILD_DIR)/layers/local.o \
  $(ROOT_BUILD_DIR)/layers/benchmark.o \
  $(ROOT_BUILD_DIR)/logdef.o
	@mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BIN_DIR)/layers/read_cache/test_read_cache: \
	$(TESTS_BUILD_DIR)/layers/read_cache/test_read_cache.o \
	$(ROOT_BUILD_DIR)/layers/read_cache.o \
	$(ROOT_BUILD_DIR)/layers/block_align.o \
	$(ROOT_BUILD_DIR)/layers/local.o \
	$(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/read_cache/test_read_cache.o: $(UNIT_DIR)/layers/read_cache/test_read_cache.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)


$(TESTS_BIN_DIR)/services/test_metadata: \
	$(TESTS_BUILD_DIR)/services/test_metadata.o \
	$(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CPP) -o $@ $^ $(LIBS) \
		-L$(SERVICES_BUILD_DIR) -lmetadata \
		-Wl,-rpath,'$$ORIGIN/../../../build/services'


$(TESTS_BUILD_DIR)/services/test_metadata.o: \
	$(UNIT_DIR)/services/test_metadata.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)



$(TESTS_BIN_DIR)/layers/anti_tampering/test_anti_tampering: \
    $(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering.o \
    $(MOCK_OBJ) \
    $(ROOT_BUILD_DIR)/layers/anti_tampering.o \
    $(ROOT_BUILD_DIR)/layers/block_anti_tampering.o \
    $(ROOT_BUILD_DIR)/layers/anti_tampering_utils.o \
    $(ROOT_BUILD_DIR)/shared/utils/locking.o \
    $(ROOT_BUILD_DIR)/shared/utils/conversion.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/evp.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha256_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha512_hasher.o \
    $(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering.o: $(UNIT_DIR)/layers/anti_tampering/test_anti_tampering.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/anti_tampering/test_anti_tampering_block: \
    $(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering_block.o \
    $(ROOT_BUILD_DIR)/layers/anti_tampering.o \
    $(ROOT_BUILD_DIR)/layers/block_anti_tampering.o \
    $(ROOT_BUILD_DIR)/layers/anti_tampering_utils.o \
    $(LAYERS_BUILD_DIR)/local.o \
    $(ROOT_BUILD_DIR)/shared/utils/locking.o \
    $(ROOT_BUILD_DIR)/shared/utils/conversion.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/evp.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha256_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha512_hasher.o \
    $(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/anti_tampering/test_anti_tampering_block.o: $(UNIT_DIR)/layers/anti_tampering/test_anti_tampering_block.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/demultiplexer/test_demultiplexer: \
    $(TESTS_BUILD_DIR)/layers/demultiplexer/test_demultiplexer.o \
    $(MOCK_OBJ) \
    $(ROOT_BUILD_DIR)/layers/demultiplexer.o \
    $(ROOT_BUILD_DIR)/layers/passthrough_ops.o \
    $(ROOT_BUILD_DIR)/layers/enforcement.o \
    $(ROOT_BUILD_DIR)/layers/local.o \
    $(ROOT_BUILD_DIR)/shared/utils/parallel.o \
    $(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/demultiplexer/test_demultiplexer.o: $(UNIT_DIR)/layers/demultiplexer/test_demultiplexer.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/compression/test_compression: \
    $(TESTS_BUILD_DIR)/layers/compression/test_compression.o \
    $(MOCK_OBJ) \
    $(ROOT_BUILD_DIR)/layers/compression.o \
    $(ROOT_BUILD_DIR)/layers/compression_utils.o \
    $(ROOT_BUILD_DIR)/layers/sparse_block.o \
    $(ROOT_BUILD_DIR)/layers/compressor.o \
    $(ROOT_BUILD_DIR)/shared/utils/locking.o \
    $(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/compression/test_compression.o: $(UNIT_DIR)/layers/compression/test_compression.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/layers/compression/test_sparse_block: \
    $(TESTS_BUILD_DIR)/layers/compression/test_sparse_block.o \
    $(MOCK_OBJ) \
    $(ROOT_BUILD_DIR)/layers/compression.o \
    $(ROOT_BUILD_DIR)/layers/sparse_block.o \
    $(ROOT_BUILD_DIR)/layers/compression_utils.o \
    $(ROOT_BUILD_DIR)/layers/compressor.o \
    $(ROOT_BUILD_DIR)/shared/utils/locking.o \
    $(ROOT_BUILD_DIR)/logdef.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/layers/compression/test_sparse_block.o: $(UNIT_DIR)/layers/compression/test_sparse_block.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/shared/utils/hasher/test_hasher: \
    $(TESTS_BUILD_DIR)/shared/utils/hasher/test_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/evp.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha256_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha512_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/conversion.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/shared/utils/hasher/test_hasher.o: $(UNIT_DIR)/shared/utils/hasher/test_hasher.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/shared/utils/hasher/test_sha256: \
    $(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha256.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/evp.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha256_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha512_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/conversion.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha256.o: $(UNIT_DIR)/shared/utils/hasher/test_sha256.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(TESTS_BIN_DIR)/shared/utils/hasher/test_sha512: \
    $(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha512.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/evp.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha256_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/hasher/sha512_hasher.o \
    $(ROOT_BUILD_DIR)/shared/utils/conversion.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LIBS)

$(TESTS_BUILD_DIR)/shared/utils/hasher/test_sha512.o: $(UNIT_DIR)/shared/utils/hasher/test_sha512.c $(TEST_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

#==============================================================================
# Test Targets
#==============================================================================

TEST_EXTRA_DEPS :=
TEST_LD_PATH_SUFFIX :=

# Build all tests - add dependency on external libraries
tests/build: lz4/build zstd/build $(TEST_EXTRA_DEPS) $(UNIT_BINS)
	@echo ""
	@echo "All tests built successfully"

# Run unit tests
tests/unit: $(UNIT_BINS)
	@echo ""
	@echo "Running unit tests..."
	@for test in $(UNIT_BINS); do \
		echo "\nRunning $$test..."; \
	    LD_LIBRARY_PATH=$(BUILD_LIB_DIR)$(TEST_LD_PATH_SUFFIX):$$LD_LIBRARY_PATH:$(ZLOG_LIB_PATH) $$test || exit 1; \
	done
	@echo "\n✅ All unit tests passed!"


# Run all tests
tests/run: tests/unit 
	@echo "\n🎉 All tests passed!"

# Clean tests
tests/clean:
	@echo "Cleaning tests..."
	rm -rf $(TESTS_BUILD_DIR) $(TESTS_BIN_DIR)

#==============================================================================
# Phony targets
#==============================================================================

.PHONY: tests/build tests/unit tests/run tests/clean 
