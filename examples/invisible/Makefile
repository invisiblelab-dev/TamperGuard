#==============================================================================
# Invisible Example Makefile
#==============================================================================

# Determine root directory first
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)../..)

# Include common configuration
include $(ROOT_DIR)/common.mk

# Example-specific build directories
EXAMPLE_BUILD_DIR := $(BUILD_DIR)/examples/invisible
EXAMPLE_BIN_DIR := $(BIN_DIR)/examples/invisible

# Example-specific compiler flags
CFLAGS := $(BASE_CFLAGS) $(BASE_INCLUDES)
CFLAGS += -I$(INVISIBLE_LIB_DIR)/src/c \
          -I$(ROOT_DIR)/layers/invisible_storage/external/invisible-storage-bindings/examples/c/include

# Example-specific libraries
LIBS := $(BASE_LIBS) -L$(INVISIBLE_TARGET_DIR) -linvisible_storage_bindings -L$(BUILD_LIB_DIR) -lmodular

# Example object files
EXAMPLE_OBJS = $(EXAMPLE_BUILD_DIR)/invisible.o

# All required objects for linking
ALL_OBJS = $(EXAMPLE_OBJS)

#==============================================================================
# Build Rules
#==============================================================================

# Ensure library is built
$(INVISIBLE_TARGET_DIR)/libinvisible_storage_bindings.so:
	$(MAKE) -C $(ROOT_DIR) libinvisible/build

# Compile example source files
$(EXAMPLE_BUILD_DIR)/%.o: %.c $(SHARED_DEPS)
	$(call create_example_build_dir,$(EXAMPLE_BUILD_DIR))
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

# Link the executable
$(EXAMPLE_BIN_DIR)/invisible: $(ALL_OBJS) $(INVISIBLE_TARGET_DIR)/libinvisible_storage_bindings.so
	$(call create_example_build_dir,$(EXAMPLE_BIN_DIR))
	$(CC) -o $@ $(ALL_OBJS) $(CFLAGS) $(LIBS)

#==============================================================================
# Targets
#==============================================================================

# Default target
.DEFAULT_GOAL := invisible/help

# Help target
invisible/help:
	@echo "Available targets:"
	@echo "  invisible/build - Build the invisible example"
	@echo "  invisible/clean - Clean the invisible example"
	@echo "  invisible/run   - Run the invisible example"

# Build target
invisible/build: $(EXAMPLE_BIN_DIR)/invisible

# Run target
invisible/run: invisible/build
	LD_LIBRARY_PATH=$(LIB_DIR):$(INVISIBLE_LIB_PATH):$$LD_LIBRARY_PATH $(EXAMPLE_BIN_DIR)/invisible

# Clean target
invisible/clean:
	@echo "Cleaning invisible example..."
	rm -rf $(EXAMPLE_BUILD_DIR)
	rm -f $(EXAMPLE_BIN_DIR)/invisible

#==============================================================================
# Phony targets
#==============================================================================

.PHONY: invisible/help invisible/build invisible/clean invisible/run
