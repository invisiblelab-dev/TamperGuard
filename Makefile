#==============================================================================
# Root Makefile
#==============================================================================

export AM_I_THE_ROOT_MAKEFILE := true

# Include common configuration
include common.mk

# Set final compiler flags
CFLAGS := $(BASE_CFLAGS) $(BASE_INCLUDES)
LIBS := $(BASE_LIBS)
PIC_CFLAGS := $(CFLAGS) -fPIC

# Optional build flags
BUILD_INVISIBLE ?= 0 # 1: For Solana and AWS S3 storage support.

# Export variables for sub-makefiles
export CFLAGS LIBS

LIBMODULAR_SO := $(BUILD_LIB_DIR)/libmodular.so

#==============================================================================
# Shared Components
#==============================================================================

#------------------------------------------------------------------------------
# Shared Object Build Rules
#------------------------------------------------------------------------------

# Compile root source files
$(ROOT_BUILD_DIR)/lib.o: lib.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(ROOT_BUILD_DIR)/logdef.o: logdef.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(ROOT_BUILD_DIR)/parser.o: config/parser.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(ROOT_BUILD_DIR)/builder.o: config/builder.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(ROOT_BUILD_DIR)/loader.o: config/loader.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(ROOT_BUILD_DIR)/toml.o: lib/tomlc17/src/tomlc17.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

# Compile layer source files

$(LAYERS_BUILD_DIR)/local.o: layers/local/local.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/remote.o: layers/remote/remote.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/demultiplexer.o: layers/demultiplexer/demultiplexer.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/passthrough_ops.o: layers/demultiplexer/passthrough_ops.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/enforcement.o: layers/demultiplexer/enforcement.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/block_align.o: layers/block_align/block_align.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/compressor.o: shared/utils/compressor/compressor.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/anti_tampering.o: layers/anti_tampering/anti_tampering.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/block_anti_tampering.o: layers/anti_tampering/block_anti_tampering.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/anti_tampering_utils.o: layers/anti_tampering/anti_tampering_utils.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/compression.o: layers/compression/compression.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/sparse_block.o: layers/compression/sparse_block.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/compression_utils.o: layers/compression/compression_utils.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/encryption.o: layers/encryption/encryption.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/aes_xts.o: layers/encryption/ciphers/aes_xts.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/benchmark.o: layers/benchmark/benchmark.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(LAYERS_BUILD_DIR)/read_cache.o: layers/cache/read_cache/read_cache.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/parallel.o: shared/utils/parallel.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/locking.o: shared/utils/locking.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/conversion.o: shared/utils/conversion.c  $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/hasher/hasher.o: shared/utils/hasher/hasher.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/hasher/evp.o: shared/utils/hasher/evp.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/hasher/sha256_hasher.o: shared/utils/hasher/sha256_hasher.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)

$(UTILS_BUILD_DIR)/hasher/sha512_hasher.o: shared/utils/hasher/sha512_hasher.c $(SHARED_DEPS)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(PIC_CFLAGS)


# Link shared library
$(LIBMODULAR_SO): $(SHARED_OBJS)
	mkdir -p $(dir $@)
	@echo "Linking shared library $@..."
	$(CPP) -shared -o $@ $^ \
		$(LIBS) \
		-Wl,-rpath="'$$ORIGIN':$(LIB_DIR):$(INVISIBLE_LIB_PATH)" \
		-Wl,--enable-new-dtags


#------------------------------------------------------------------------------
# Shared Targets
#------------------------------------------------------------------------------

shared/build: zlog/build lz4/build zstd/build $(LIBMODULAR_SO)
	@echo "Shared library $(LIBMODULAR_SO) built successfully"


# Clean shared objects
shared/clean: lz4/clean zstd/clean
	@echo "Cleaning shared objects and library..."
	rm -f $(SHARED_OBJS) $(LIBMODULAR_SO)

#==============================================================================
# External Libraries
#==============================================================================

#------------------------------------------------------------------------------
# Invisible Storage
#------------------------------------------------------------------------------

# Invisible storage targets
libinvisible/build: external/libinvisible/build

libinvisible/clean:
	@echo "Cleaning invisible storage..."
	@if [ -d "$(INVISIBLE_LIB_DIR)" ]; then \
		echo "  Cleaning Rust build artifacts in $(INVISIBLE_LIB_DIR)..."; \
		(cd $(INVISIBLE_LIB_DIR) && cargo clean || true); \
	else \
		echo "  Directory $(INVISIBLE_LIB_DIR) does not exist, nothing to clean."; \
	fi

# Rust library operations
external/libinvisible/build:
	@echo "Building Rust library in $(INVISIBLE_LIB_DIR)"
	@if [ ! -d "$(INVISIBLE_LIB_DIR)" ]; then \
		echo "Error: invisible-storage-bindings submodule not found. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	cd $(INVISIBLE_LIB_DIR) && CARGO_NET_GIT_FETCH_WITH_CLI=true cargo build --release

#------------------------------------------------------------------------------
# Other external libraries will be added here
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Zlog Library
#------------------------------------------------------------------------------

ZLOG_DIR = $(ROOT_DIR)/lib/zlog
ZLOG_LIB_PATH = $(ZLOG_DIR)/src

# Zlog targets
zlog/build:
	@echo "Building zlog library in $(ZLOG_DIR)"
	@if [ ! -d "$(ZLOG_DIR)" ]; then \
		echo "Error: zlog submodule not found. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	cd $(ZLOG_DIR) && $(MAKE)

zlog/clean:
	@echo "Cleaning zlog library..."
	@if [ -d "$(ZLOG_DIR)" ]; then \
		cd $(ZLOG_DIR) && $(MAKE) clean; \
	else \
		echo "  Directory $(ZLOG_DIR) does not exist, nothing to clean."; \
	fi

#------------------------------------------------------------------------------
# LZ4 Library
#------------------------------------------------------------------------------

LZ4_DIR = $(ROOT_DIR)/lib/lz4
LZ4_LIB_PATH = $(LZ4_DIR)/lib

# LZ4 targets
lz4/build:
	@echo "Building LZ4 library in $(LZ4_DIR)/lib"
	@if [ ! -d "$(LZ4_DIR)" ]; then \
		echo "Error: LZ4 submodule not found. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	cd $(LZ4_DIR)/lib && $(MAKE)

lz4/clean:
	@echo "Cleaning LZ4 library..."
	@if [ -d "$(LZ4_DIR)" ]; then \
		cd $(LZ4_DIR) && $(MAKE) clean; \
	else \
		echo "  Directory $(LZ4_DIR) does not exist, nothing to clean."; \
	fi

#------------------------------------------------------------------------------
# ZSTD Library
#------------------------------------------------------------------------------

ZSTD_DIR = $(ROOT_DIR)/lib/zstd
ZSTD_LIB_PATH = $(ZSTD_DIR)/lib

# ZSTD targets
zstd/build:
	@echo "Building ZSTD library in $(ZSTD_DIR)/lib"
	@if [ ! -d "$(ZSTD_DIR)" ]; then \
		echo "Error: ZSTD submodule not found. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	cd $(ZSTD_DIR)/lib && $(MAKE)

zstd/clean:
	@echo "Cleaning ZSTD library..."
	@if [ -d "$(ZSTD_DIR)" ]; then \
		cd $(ZSTD_DIR) && $(MAKE) clean; \
	else \
		echo "  Directory $(ZSTD_DIR) does not exist, nothing to clean."; \
	fi

#==============================================================================
# Examples
#==============================================================================

#------------------------------------------------------------------------------
# Invisible Example
#------------------------------------------------------------------------------

# Include example's Makefile
include examples/invisible/Makefile

# Example targets
examples/invisible/build: shared/build
	@echo "Building invisible example..."
	$(MAKE) -C examples/invisible invisible/build

examples/invisible/clean:
	@echo "Cleaning invisible example..."
	$(MAKE) -C examples/invisible invisible/clean

examples/invisible/run:
	@echo "Running invisible example..."
	$(MAKE) -C examples/invisible invisible/run

#------------------------------------------------------------------------------
# Fuse Example
#------------------------------------------------------------------------------

# Include example's Makefile
include examples/fuse/Makefile

# Example targets
examples/fuse/build: shared/build
	@echo "Building fuse example..."
	$(MAKE) -C examples/fuse fuse/build

examples/fuse/clean:
	@echo "Cleaning fuse example..."
	$(MAKE) -C examples/fuse fuse/clean

examples/fuse/debug:
	@echo "Running fuse with gdb..."
	$(MAKE) -C examples/fuse fuse/debug

examples/fuse/run:
	@echo "Running fuse example..."
	$(MAKE) -C examples/fuse fuse/run

examples/fuse/run/daemon:
	@echo "Running fuse example as daemon..."
	$(MAKE) -C examples/fuse fuse/run/daemon

examples/fuse/stop:
	@echo "Stopping fuse example..."
	$(MAKE) -C examples/fuse fuse/stop

#------------------------------------------------------------------------------
# Storserver Example
#------------------------------------------------------------------------------

# Include example's Makefile
include examples/storserver/Makefile

# Example targets
examples/storserver/build:
	@echo "Building storserver example..."
	$(MAKE) -C examples/storserver storserver/build

examples/storserver/clean:
	@echo "Cleaning storserver example..."
	$(MAKE) -C examples/storserver storserver/clean

examples/storserver/run:
	@echo "Running storserver example..."
	$(MAKE) -C examples/storserver storserver/run

#==============================================================================
# Documentation
#==============================================================================

#------------------------------------------------------------------------------
# MkDocs Documentation
#------------------------------------------------------------------------------

# Create symbolic links for documentation
docs/links:
	@echo "Creating documentation symbolic links..."
	./scripts/create_docs_links.sh

# Serve documentation with auto-reload
docs/serve: docs/links
	@echo "Starting MkDocs development server..."
	@if ! command -v mkdocs >/dev/null 2>&1; then \
		echo "Error: mkdocs not found. Install with: pipx install mkdocs && pipx inject mkdocs mkdocs-material"; \
		exit 1; \
	fi
	mkdocs serve

# Build static documentation
docs/build: docs/links
	@echo "Building static documentation..."
	@if ! command -v mkdocs >/dev/null 2>&1; then \
		echo "Error: mkdocs not found. Install with: pipx install mkdocs && pipx inject mkdocs mkdocs-material"; \
		exit 1; \
	fi
	mkdocs build

# Clean documentation
docs/clean:
	@echo "Cleaning documentation..."
	rm -rf docs/mkdocs docs/mkdocs_static

#==============================================================================
# Main Targets
#==============================================================================

# Default target
.DEFAULT_GOAL := help

# Help target
help:
	@echo "Available targets:"
	@echo "  build                       - Build all components (optional: invisible-storage-bindings)"
	@echo ""
	@echo "Build flags:"
	@echo "  BUILD_INVISIBLE=1           - Include invisible-storage-bindings in build"
	@echo ""
	@echo "Examples:"
	@echo "  make build                  - Build without invisible-storage-bindings"
	@echo "  make build BUILD_INVISIBLE=1 - Build with invisible-storage-bindings"
	@echo "  clean                       - Clean all build artifacts, excluding the rust library binaries"
	@echo "  clean/logs                  - Clean all logs"
	@echo "  clean/all                   - Clean all build artifacts, logs, and rust library (submodules preserved)"
	@echo "  format                      - Format all C/C++ source files with clang-format"
	@echo "  lint                        - Run clang-tidy static analysis (shows output, fails on warnings)"
	@echo "  submodules/fetch            - Fetch all submodules (gets the latest version of the submodules)"
	@echo "  shared/build                - Build shared objects only (layers/*.o and lib.o)"
	@echo "  shared/clean                - Clean shared objects only (layers/*.o and lib.o)"
	@echo "  external/libinvisible/build - Builds the external rust invisible library"
	@echo "  libinvisible/build          - Builds the external rust invisible library"
	@echo "  libinvisible/clean          - Cleans the external rust invisible library"
	@echo "  zlog/build                  - Build the zlog library from submodule"
	@echo "  zlog/clean                  - Clean the zlog library"
	@echo "  zlog/install                - Install the zlog library system-wide"
	@echo "  examples/invisible/build    - Build the invisible example"
	@echo "  examples/invisible/clean    - Clean the invisible example"
	@echo "  examples/invisible/run      - Run the invisible example"
	@echo "  examples/fuse/build         - Build the fuse example"
	@echo "  examples/fuse/clean         - Clean the fuse example"
	@echo "  examples/fuse/run           - Run the fuse example as a foreground process"
	@echo "  examples/fuse/run/daemon    - Run the fuse example as a background process"
	@echo "  examples/fuse/stop          - Stop the fuse example"
	@echo "  examples/storserver/build   - Build the storserver example"
	@echo "  examples/storserver/clean   - Clean the storserver example"
	@echo "  examples/storserver/run     - Run the storserver example"
	@echo "  tests/build                  - Build all tests"
	@echo "  tests/unit                   - Run unit tests"
	@echo "  tests/integration            - Run integration tests"
	@echo "  tests/run                    - Run all tests"
	@echo "  tests/clean                  - Clean test artifacts"
	@echo "  docs/links                   - Create symbolic links for documentation"
	@echo "  docs/serve                   - Start MkDocs development server with auto-reload"
	@echo "  docs/build                   - Build static HTML documentation"
	@echo "  docs/clean                   - Clean documentation build artifacts"

# Build all components
build:
	@echo "Building all components..."
	$(MAKE) submodules/fetch
ifeq ($(BUILD_INVISIBLE),1)
	@echo "Building invisible-storage-bindings..."
	$(MAKE) libinvisible/build
	$(MAKE) examples/invisible/build
endif

	$(MAKE) shared/build
	$(MAKE) examples/fuse/build
	$(MAKE) examples/storserver/build
	$(MAKE) tests/build
	@echo "Build complete!"

# Fetch all the submodules
submodules/fetch:
	@echo "Fetching required submodules..."
	# Core submodules (always required)
	git submodule update --init --recursive \
		lib/zlog \
		lib/tomlc17 \
		lib/uthash \
		lib/lz4 \
		lib/zstd
ifeq ($(BUILD_INVISIBLE),1)
	@echo "Fetching invisible-storage-bindings submodule (BUILD_INVISIBLE=1)..."
	git submodule update --init --recursive lib/invisible-storage-bindings
endif

# Clean all build artifacts
clean:
	@echo "Cleaning all build artifacts (excluding the rust library)..."
	$(MAKE) examples/invisible/clean
	$(MAKE) examples/fuse/clean
	$(MAKE) examples/storserver/clean
	$(MAKE) shared/clean
	$(MAKE) zlog/clean
	$(MAKE) lz4/clean
	$(MAKE) zstd/clean
	$(MAKE) tests/clean
	@echo "Cleaning submodule build artifacts from build directory..."
	rm -rf $(BUILD_DIR)/static $(BUILD_DIR)/dynamic
	@echo "Cleaning complete!"

# Clean all logs (needs to be done manually, as it's not a build artifact)
clean/logs:
	@echo "Cleaning all logs..."
	rm -rf logs/*

# Clean all
clean/all:
	@echo "Cleaning all build artifacts, logs, and rust library..."
	$(MAKE) clean
	$(MAKE) clean/logs
	$(MAKE) libinvisible/clean
	$(MAKE) zlog/clean
	$(MAKE) lz4/clean
	$(MAKE) zstd/clean
	@echo "Note: Submodules are preserved as they are managed by git"

# Format all source files (and ensure all text files end with a newline)
format:
	@echo "Formatting source files with clang-format..."
	@rm -f /tmp/format_count.tmp 2>/dev/null || true
	@find . \( -name "*.c" -o -name "*.h" -o -name "*.cpp" \) -not -path "./lib/*" -not -path "./docs/*" -print0 | \
	xargs -0 -n1 -P$$(nproc) -I{} sh -c ' \
		echo "  Formatting: {}"; \
		clang-format -i "{}"; \
		echo "1" >> /tmp/format_count.tmp'
	@count=$$(cat /tmp/format_count.tmp 2>/dev/null | wc -l || echo 0); \
	rm -f /tmp/format_count.tmp; \
	echo "Formatting complete! ($$count files formatted)"
	@echo "Sanitizing text files to ensure they end with newlines..."
	@for f in $$(git grep --cached -Il '' | grep -v '^docs/'); do \
		tail -c1 "$$f" | read -r _ || echo >> "$$f"; \
	done
	@echo "Git sanitization complete!"

# Run clang-tidy static analysis on all source files (shows output and fails on warnings)
lint:
	@echo "Running clang-tidy static analysis..."
	@if ! command -v clang-tidy >/dev/null 2>&1; then \
		echo "Error: clang-tidy not found. Please install clang-tidy:"; \
		echo "  Ubuntu/Debian: sudo apt-get install clang-tidy"; \
		echo "  macOS: brew install llvm"; \
		exit 1; \
	fi
	@echo "Analyzing C source files in parallel..."
	@rm -f /tmp/lint_* 2>/dev/null || true
	@find . \( -name "*.c" \) \
		-not -path "./lib/*" \
		-not -path "./docs/*" \
		-not -path "./build/*" \
		-not -path "./.git/*" \
		-print0 | \
	xargs -0 -n1 -P$$(nproc) -I{} sh -c ' \
		echo "  Analyzing: {}"; \
		output=$$(clang-tidy "{}" -- $(CFLAGS) $$(pkg-config --cflags glib-2.0) 2>&1); \
		if echo "$$output" | grep -q ": \(warning\|error\):"; then \
			echo "$$output"; \
			echo "LINT_FAILED" > /tmp/lint_failed_$$$$; \
			warnings=$$(echo "$$output" | grep -c ": warning:" || echo 0); \
			errors=$$(echo "$$output" | grep -c ": error:" || echo 0); \
			echo "$$warnings" >> /tmp/lint_warnings_$$$$; \
			echo "$$errors" >> /tmp/lint_errors_$$$$; \
		fi'
	@if ls /tmp/lint_failed_* >/dev/null 2>&1; then \
		echo ""; \
		echo "=== LINT SUMMARY ==="; \
		echo "❌ Static analysis failed!"; \
		warning_count=$$(cat /tmp/lint_warnings_* 2>/dev/null | awk '{sum += $$1} END {print sum+0}' || echo 0); \
		error_count=$$(cat /tmp/lint_errors_* 2>/dev/null | awk '{sum += $$1} END {print sum+0}' || echo 0); \
		if [ $$warning_count -gt 0 ]; then \
			echo "   Warnings found: $$warning_count"; \
		fi; \
		if [ $$error_count -gt 0 ]; then \
			echo "   Errors found: $$error_count"; \
		fi; \
		echo "   Total issues: $$((warning_count + error_count))"; \
		rm -f /tmp/lint_*; \
		exit 1; \
	fi
	@echo "✅ Static analysis passed! No warnings or errors found."

#==============================================================================
# Phony targets
#==============================================================================
.PHONY: build clean help format lint submodules/fetch clean/logs clean/all \
        shared/build shared/clean \
				external/libinvisible/build \
        libinvisible/build libinvisible/clean \
        zlog/build zlog/clean zlog/install \
        lz4/build lz4/clean \
        zstd/build zstd/clean \
        examples/invisible/build examples/invisible/clean examples/invisible/run \
        examples/fuse/build examples/fuse/clean examples/fuse/run examples/fuse/run/daemon examples/fuse/stop \
        examples/storserver/build examples/storserver/clean examples/storserver/run \
        tests/build tests/unit tests/integration tests/run tests/clean \
        docs/links docs/serve docs/build docs/clean
include tests/Makefile
