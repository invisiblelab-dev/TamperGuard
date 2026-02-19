# Tests

The Modular IO Library includes unit tests that validate functionality across the implemented layers and shared components.

## Overview

The test system provides:

- **Unit tests** for individual layers and shared components
- **Automated test execution** with make targets  
- **Mock layer infrastructure** for testing layer interactions

## Test Architecture

### Directory Structure
```
tests/
├── Makefile                   # Test build configuration
├── mock_layer.c               # Mock layer implementation for testing
├── mock_layer.h               # Mock layer interface
├── unit/                      # Unit tests
│   ├── layers/                # Layer-specific unit tests
│   │   ├── anti_tampering/    # Anti-tampering layer tests
│   │   ├── block_align/       # Block align layer tests  
│   │   ├── compression/       # Compression layer tests
│   │   ├── demultiplexer/     # Demultiplexer layer tests
│   │   └── local/             # Local layer tests
│   └── shared/                # Shared components tests
│       └── utils/             # Utility tests
│           ├── compressor/    # Compressor algorithm tests
│           └── hasher/        # Hasher algorithm tests
```

### Test Framework
The tests use a simple testing approach:

- **Standard assertions**: Uses C's `assert()` macro
- **Test organization**: Each test file contains multiple test functions
- **Main runners**: Each test file has a main() function that runs all tests
- **Output format**: Console output with pass/fail reporting

## Running Tests

### Run All Tests
```bash
# Run complete test suite
make tests/run
```

### Build Tests Only
```bash
# Build all tests without running
make tests/build
```

### Clean Test Artifacts
```bash
# Clean all test build artifacts
make tests/clean
```

### Test Dependencies
Tests require external libraries to be built:
```bash
# Ensure all dependencies are built
make build        # Build main project including external libraries
make tests/run    # Run tests
```

## Test Areas

### Hasher Tests (`tests/unit/shared/utils/hasher/`)
Tests for the cryptographic hash system:

- **Generic interface**: Common hasher functionality across algorithms
- **Algorithm-specific**: SHA-256 and SHA-512 implementations
- **Known test vectors**: Validation against expected hash outputs
- **Error handling**: Invalid inputs and edge cases

### Layer Tests (`tests/unit/layers/`)

#### Local Layer (`local/`)
Tests for local filesystem operations:

- Basic file operations and error handling
- File truncation and size management
- Permission and access control scenarios

#### Anti-Tampering Layer (`anti_tampering/`)
Tests for data integrity verification:

- Hash-based integrity checking
- File modification detection
- Hash file management and updates
- Lock handling and concurrency

#### Compression Layer (`compression/`)
Tests for data compression functionality:

- **Main layer tests**: Core compression layer operations
- **Configuration**: TOML parsing and parameter validation  
- **Compressor engine**: Low-level compression/decompression algorithms
- **Algorithm support**: LZ4 and ZSTD implementations
- **Round-trip integrity**: Data consistency through compress/decompress cycles

#### Block Align Layer (`block_align/`)
Tests for block-aligned I/O operations:

- **Core functionality**: Block boundary handling and alignment
- **Configuration**: Block size parsing and validation
- **Cross-block operations**: Data spanning multiple blocks
- **Edge cases**: Partial blocks and boundary conditions

#### Demultiplexer Layer (`demultiplexer/`)
Tests for parallel multi-backend operations:

- Multi-layer coordination and orchestration
- Enforcement policies for critical vs optional layers
- Passthrough operation handling
- Error aggregation across multiple backends

## Test Implementation

### Basic Test Structure
```c
#include <stdio.h>
#include <assert.h>

void test_basic_functionality() {
    // Test setup
    int result = some_function(test_input);
    
    // Assertions
    assert(result == expected_value);
    printf("test_basic_functionality passed\n");
}

int main() {
    printf("Running unit tests...\n\n");
    
    test_basic_functionality();
    // ... more tests ...
    
    printf("\nAll tests passed!\n");
    return 0;
}
```

### Mock Layer System
The test infrastructure includes a mock layer (`mock_layer.c`) that simulates layer behavior for testing:

- Configurable read/write operations
- Error injection capabilities
- State tracking for test verification

## Adding New Tests

### Creating Unit Tests

1. **Create test file**:
```c
// tests/unit/my_component/test_my_component.c
#include "my_component.h"
#include <assert.h>
#include <stdio.h>

void test_my_functionality() {
    // Test implementation
    assert(my_function() == expected_result);
    printf("test_my_functionality passed\n");
}

int main() {
    printf("Testing my component...\n");
    test_my_functionality();
    printf("All my component tests passed!\n");
    return 0;
}
```

2. **Update Makefile**:
Add the new test to the `UNIT_OBJS` and `UNIT_BINS` variables in `tests/Makefile` and create the appropriate build rules.

3. **Run new tests**:
```bash
make tests/build
make tests/run
```

## Troubleshooting Tests

### Common Issues

#### Test Compilation Errors
```bash
# Check dependencies are built
make build

# Clean and rebuild tests
make tests/clean
make tests/build
```

#### Test Execution Failures
```bash
# Run specific test for debugging
bin/tests/layers/local/test_local

# Enable debug logging
log_mode = "debug"  # in config.toml
```

### Debugging Failed Tests

1. **Isolate the failure**: Run specific test binaries
2. **Enable logging**: Use debug logging mode
3. **Check dependencies**: Verify all required components built
4. **Use debugger**: Run tests under gdb for detailed analysis

```bash
# Debug with gdb
gdb bin/tests/layers/anti_tampering/test_anti_tampering
(gdb) run
(gdb) bt  # if it crashes
``` 
