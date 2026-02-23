#==============================================================================
# Fuse Example Makefile
#==============================================================================

# Determine root directory first
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)../..)

# Include common configuration
include $(ROOT_DIR)/common.mk

# Example-specific build directories
EXAMPLE_BUILD_DIR := $(BUILD_DIR)/examples/fuse
EXAMPLE_BIN_DIR := $(BIN_DIR)/examples/fuse

# Example-specific compiler flags
CFLAGS := $(BASE_CFLAGS) $(BASE_INCLUDES)

# Example-specific libraries
BASE_FUSE_LIBS := $(BASE_LIBS) `pkg-config fuse3 --cflags --libs` -lpthread -lcrypto -L$(BUILD_LIB_DIR) -lmodular -Wl,-rpath=$(BUILD_LIB_DIR)
# Add invisible library if BUILD_INVISIBLE is set
ifeq ($(BUILD_INVISIBLE),1)
LIBS := $(BASE_FUSE_LIBS) -L$(INVISIBLE_TARGET_DIR) -linvisible_storage_bindings -Wl,-rpath=$(INVISIBLE_LIB_PATH)
else
LIBS := $(BASE_FUSE_LIBS)
endif

# Config file path (can be set via environment variable MODULAR_IO_CONFIG_PATH)
# If relative, it will be resolved relative to ROOT_DIR
# Using MODULAR_IO_CONFIG_PATH to avoid conflicts with other tools that might use CONFIG_PATH
MODULAR_IO_CONFIG_PATH ?=
# Convert relative paths to absolute (relative to ROOT_DIR)
ifdef MODULAR_IO_CONFIG_PATH
  ifneq ($(abspath $(MODULAR_IO_CONFIG_PATH)),$(MODULAR_IO_CONFIG_PATH))
    # Path is relative, make it absolute relative to ROOT_DIR
    MODULAR_IO_CONFIG_PATH := $(abspath $(ROOT_DIR)/$(MODULAR_IO_CONFIG_PATH))
  endif
endif

# Example object files
EXAMPLE_OBJS = $(EXAMPLE_BUILD_DIR)/passthrough.o

# All required objects for linking
ALL_OBJS = $(EXAMPLE_OBJS)

MOUNT_POINT ?= $(ROOT_DIR)/examples/fuse/mount_point
BACKEND_DATA ?= $(ROOT_DIR)/examples/fuse/backend_data

#==============================================================================
# Build Rules
#==============================================================================

# Compile example source files
$(EXAMPLE_BUILD_DIR)/%.o: %.c $(SHARED_DEPS)
	$(call create_example_build_dir,$(EXAMPLE_BUILD_DIR))
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

# Link the executable
$(EXAMPLE_BIN_DIR)/passthrough: $(ALL_OBJS)
	$(call create_example_build_dir,$(EXAMPLE_BIN_DIR))
	$(CC) -o $@ $(ALL_OBJS) $(CFLAGS) $(LIBS)

#==============================================================================
# Targets
#==============================================================================

# Default target
.DEFAULT_GOAL := fuse/help

# Help target
fuse/help:
	@echo "Available targets:"
	@echo "  fuse/build      - Build the fuse example (passthrough executable)"
	@echo "  fuse/clean      - Clean the fuse example build artifacts"
	@echo "  fuse/run        - Run the fuse example as a foreground process (requires sudo, creates test directories)"
	@echo "  fuse/run/daemon - Run the fuse example as a background process (requires sudo, creates test directories)"
	@echo "  fuse/stop       - Unmount the fuse filesystem"
	@echo ""
	@echo "Configurable paths (can be set via command line):"
	@echo "  MOUNT_POINT     - Where to mount the FUSE filesystem (default: \$$(ROOT_DIR)/examples/fuse/mount_point)"
	@echo "  BACKEND_DATA    - Where to store the actual data (default: \$$(ROOT_DIR)/examples/fuse/backend_data)"
	@echo ""
	@echo "Example usage with custom paths:"
	@echo "  make fuse/run MOUNT_POINT=/path/to/mount BACKEND_DATA=/path/to/storage"
	@echo ""
	@echo "To run manually: Build with 'fuse/build', then run './bin/examples/fuse/passthrough <mountpoint> [FUSE options]' (usually requires sudo)."

# Build target
fuse/build: $(EXAMPLE_BIN_DIR)/passthrough

# Debug target (similar to run, but using gdb)
fuse/debug: fuse/build
	@echo "Setting up FUSE debug environment..."
	@mkdir -p $(MOUNT_POINT)
	@mkdir -p $(BACKEND_DATA)
	@mkdir -p $(LOG_DIR)
	@echo "Launching FUSE in gdb..."
	cd $(ROOT_DIR) && sudo -E gdb -tui -ex "file $(EXAMPLE_BIN_DIR)/passthrough" --args \
		$(EXAMPLE_BIN_DIR)/passthrough $(MOUNT_POINT) \
		-omodules=subdir,subdir=$(BACKEND_DATA) \
		-oallow_other -f

# Run target (foreground)
fuse/run: fuse/build
	@echo "Setting up FUSE example directories..."
	@mkdir -p $(MOUNT_POINT)
	@mkdir -p $(BACKEND_DATA)
	@echo "Starting FUSE passthrough..."
	@echo "Mount point: $(MOUNT_POINT)"
	@echo "Local backend data: $(BACKEND_DATA)"
	@echo "Note: This requires sudo privileges and will run in foreground (-f)"
	@echo "Press Ctrl+C to stop, or run 'make fuse/stop' from another terminal"
	@mkdir -p $(LOG_DIR)
	cd $(ROOT_DIR) && sudo -E LD_LIBRARY_PATH=$(BUILD_LIB_DIR)$(if $(filter 1,$(BUILD_INVISIBLE)),:$(INVISIBLE_LIB_PATH))$(if $(filter 1,$(BUILD_CACHELIB)),:$(LAYERS_BUILD_DIR)):$$LD_LIBRARY_PATH:$(ZLOG_LIB_PATH):$(SERVICES_BUILD_DIR) $(EXAMPLE_BIN_DIR)/passthrough $(MOUNT_POINT) \
		$(if $(MODULAR_IO_CONFIG_PATH),--config $(MODULAR_IO_CONFIG_PATH)) \
		-omodules=subdir,subdir=$(BACKEND_DATA) \
		-oallow_other -f

# Run target (daemon)
fuse/run/daemon: fuse/build
	$(eval FUSE_LOG_FILE_BASENAME := $(shell date +'%Y%m%d-%H%M%S')_fuse.log)
	@echo "Setting up FUSE example directories..."
	@mkdir -p $(MOUNT_POINT)
	@mkdir -p $(BACKEND_DATA)
	@echo "Starting FUSE passthrough..."
	@echo "Mount point: $(MOUNT_POINT)"
	@echo "Local backend data: $(BACKEND_DATA)"
	@echo "Note: This requires sudo privileges and will run in foreground (-f)"
	@echo "Press Ctrl+C to stop, or run 'make fuse/stop' from another terminal"
	@echo "Passthrough daemon stdout/stderr will be logged to $(LOG_DIR)/$(FUSE_LOG_FILE_BASENAME)"
	@echo "LibFUSE debug messages (if any) should go to syslog (check with journalctl -f or tail -f /var/log/syslog)"
	@mkdir -p $(LOG_DIR)
	cd $(ROOT_DIR) && sudo -E nohup sh -c 'LD_LIBRARY_PATH=$(BUILD_LIB_DIR)$(if $(filter 1,$(BUILD_INVISIBLE)),:$(INVISIBLE_LIB_PATH))$(if $(filter 1,$(BUILD_CACHELIB)),:$(LAYERS_BUILD_DIR)):$$LD_LIBRARY_PATH:$(ZLOG_LIB_PATH) $(EXAMPLE_BIN_DIR)/passthrough $(MOUNT_POINT) \
		$(if $(MODULAR_IO_CONFIG_PATH),--config $(MODULAR_IO_CONFIG_PATH)) \
		-omodules=subdir,subdir=$(BACKEND_DATA) \
		-oallow_other -f' > $(LOG_DIR)/$(FUSE_LOG_FILE_BASENAME) 2>&1 &
	@sleep 1 # Give FUSE a moment to start

# Stop target
fuse/stop:
	@echo "Attempting to unmount FUSE filesystem at $(MOUNT_POINT)..."
	@if mountpoint -q $(MOUNT_POINT); then \
		if sudo umount $(MOUNT_POINT); then \
			echo "FUSE filesystem unmounted successfully."; \
		else \
			echo "Standard umount failed (target might be busy). Attempting lazy unmount..."; \
			if sudo fusermount3 -u -z $(MOUNT_POINT); then \
				echo "FUSE filesystem lazy unmounted successfully. It will be fully released when the FUSE process exits."; \
			else \
				echo "Lazy unmount also failed. Manual intervention might be required."; \
				echo "You can try: ps aux | grep passthrough   (to find the process ID)"; \
				echo "Then: sudo kill <PID>"; \
				echo "And then try unmounting again."; \
			fi; \
		fi; \
	else \
		echo "FUSE filesystem is not mounted at $(MOUNT_POINT)."; \
	fi

# Clean target
fuse/clean:
	@echo "Cleaning fuse example build artifacts..."
	@echo "Ensuring FUSE filesystem is unmounted..."
	@if mountpoint -q $(ROOT_DIR)/examples/fuse/mount_point 2>/dev/null; then \
		sudo umount $(ROOT_DIR)/examples/fuse/mount_point || true; \
	fi
	rm -rf $(EXAMPLE_BUILD_DIR)
	rm -f $(EXAMPLE_BIN_DIR)/passthrough
	@echo "Note: mount_point and backend_data directories are preserved"

#==============================================================================
# Phony targets
#==============================================================================

.PHONY: fuse/help fuse/build fuse/debug fuse/clean fuse/run fuse/stop
