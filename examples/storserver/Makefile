#==============================================================================
# Storserver Example Makefile
#==============================================================================

# Determine root directory first
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)../..)

# Include common configuration
include $(ROOT_DIR)/common.mk

# Example-specific build directories
EXAMPLE_BUILD_DIR := $(BUILD_DIR)/examples/storserver
EXAMPLE_BIN_DIR := $(BIN_DIR)/examples/storserver

# Example-specific compiler flags
CFLAGS := $(BASE_CFLAGS) $(BASE_INCLUDES)

# Example-specific libraries (storserver is a standalone server, no FUSE needed)
LIBS := $(BASE_LIBS) -lpthread -lcrypto

# Example-specific dependencies (only local storserver.h)
EXAMPLE_DEPS = storserver.h

# Example object files (storserver is standalone - only its own objects)
EXAMPLE_OBJS = $(EXAMPLE_BUILD_DIR)/storserver.o

#==============================================================================
# Build Rules
#==============================================================================

# Compile example source files
$(EXAMPLE_BUILD_DIR)/%.o: %.c $(EXAMPLE_DEPS)
	$(call create_example_build_dir,$(EXAMPLE_BUILD_DIR))
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

# Link the executable (storserver is standalone - no shared objects needed)
$(EXAMPLE_BIN_DIR)/storserver: $(EXAMPLE_OBJS)
	$(call create_example_build_dir,$(EXAMPLE_BIN_DIR))
	$(CC) -o $@ $(EXAMPLE_OBJS) $(CFLAGS) $(LIBS)

#==============================================================================
# Targets
#==============================================================================

# Default target
.DEFAULT_GOAL := storserver/help

# Help target
storserver/help:
	@echo "Available targets:"
	@echo "  storserver/build - Build the storserver example (standalone server)"
	@echo "  storserver/clean - Clean the storserver example"
	@echo "  storserver/run   - Run the storserver example"

# Build target (storserver is independent - no shared/build dependency)
storserver/build: $(EXAMPLE_BIN_DIR)/storserver

# Run target
storserver/run: storserver/build
	@echo "Starting storage server..."
	@echo "Server listening on port 5000"
	@echo "Note: Server uses hardcoded path /home/vagrant/server/ for file storage"
	@echo "You may need to create this directory: mkdir -p /home/vagrant/server"
	@echo "Press Ctrl+C to stop the server"
	$(EXAMPLE_BIN_DIR)/storserver

# Clean target
storserver/clean:
	@echo "Cleaning storserver example build artifacts..."
	rm -rf $(EXAMPLE_BUILD_DIR)
	rm -f $(EXAMPLE_BIN_DIR)/storserver

#==============================================================================
# Phony targets
#==============================================================================

.PHONY: storserver/help storserver/build storserver/clean storserver/run
