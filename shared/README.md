# Shared Components

The `shared/` directory contains centralized type definitions, utilities, and common functionality used across all layers and examples in the Modular IO Library.

## Directory Structure

```
shared/
├── types/           # Core structure definitions
├── enums/           # Enumeration definitions  
├── utils/           # Common utility functions
│   ├── conversion/  # Data conversion utilities
│   ├── parallel/    # Parallel processing utilities  
│   ├── locking/     # Thread-safe locking utilities
│   └── hasher/      # Hash algorithm implementations
```

## Components

### Core Types (`shared/types/`)
Fundamental data structures that define the layer architecture:

- **LayerContext**: The primary structure for layer instances
- **LayerOps**: Function pointer interface for layer operations
- **Configuration structures**: Common configuration patterns

These types provide a uniform interface across all layers and enable layer composition and chaining.

### Enumerations (`shared/enums/`)
Standardized enumeration definitions used throughout the system:

- **LayerType**: Identifies different layer implementations
- **LogMode**: Defines logging levels and output modes
- **Algorithm types**: Hash algorithms, compression methods, etc.

### Utility Functions (`shared/utils/`)

#### Conversion Utilities
Data format conversion and validation functions:

- Type conversion between different data representations
- Endianness handling for cross-platform compatibility
- String and numeric conversion utilities

#### Parallel Processing Utilities  
Thread-safe operations for multi-layer processing:

- Thread pool management and coordination
- Parallel I/O operations
- Result aggregation from multiple layers

#### Locking Utilities
Path-based reader-writer locking for concurrent access:

- **Path-based locking**: Locks associated with file paths
- **Reader-writer semantics**: Multiple concurrent readers, exclusive writers
- **Automatic cleanup**: Prevents lock leaks and deadlocks
- **Efficient lookups**: Hash table-based path-to-lock mapping

#### Hasher System
Unified interface for cryptographic hash algorithms:

- **Generic interface**: Algorithm-agnostic hash operations
- **Multiple algorithms**: SHA-256, SHA-512 support
- **EVP implementation**: Shared OpenSSL operations
- **Runtime selection**: Choose algorithms dynamically

## Architecture Principles

### Thread Safety
All shared utilities are designed for concurrent use:

- Stateless operations where possible
- Thread-safe synchronization primitives
- Proper resource management in multi-threaded environments

### Consistency
Uniform patterns across the codebase:

- Consistent error codes and handling patterns
- Standardized memory management approaches
- Common naming conventions and interfaces

### Extensibility
Designed to support future enhancements:

- Plugin-style algorithm selection
- Modular utility organization
- Clear separation of concerns

## Integration

Shared components integrate with the layer ecosystem:

- **All layers**: Use core types and enumerations
- **Anti-tampering**: Uses hasher system and locking utilities
- **Demultiplexer**: Uses parallel processing utilities
- **Configuration**: Uses enumerations for validation

## Building

Build shared components:

```bash
# Build shared components
make shared/build

# Clean build artifacts
make shared/clean

# Run tests
make tests/run
```

## Usage Guidelines

### For Layer Developers

- Use shared types for consistency
- Follow established error handling patterns
- Assume multi-threaded usage scenarios
- Document any extensions to shared interfaces

### For Application Developers

- Initialize shared utilities properly
- Handle resource cleanup appropriately
- Use thread-safe operations when needed
- Check return codes for error conditions 
