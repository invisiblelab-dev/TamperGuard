# Local Layer

The **local layer** provides direct access to the local filesystem, serving as a fundamental storage backend for the modular I/O system.

## Key Features

- **Direct filesystem access** with native performance
- **POSIX-compliant operations** supporting standard file system semantics
- **Terminal layer** - does not delegate to other layers
- **Thread-safe** with OS-level guarantees
- **Minimal overhead** over direct system calls

## Configuration

```toml
[local_layer]
type = "local"
# No additional parameters required
```

The local layer has **no configuration parameters** - it uses the filesystem directly.

## Operations

**File Management**: Open, close, size query, truncate
**I/O Operations**: Positioned read/write at specific file offsets

## Performance & Behavior

**Performance**: Minimal overhead, relies on OS buffer cache
**Error Handling**: Returns system call error codes directly
**Memory**: Low memory usage, simple cleanup
**Thread Safety**: Stateless design with OS-level guarantees

## Use Cases

- **Simple Applications**: Direct local file access without complexity
- **Terminal Layer**: End point for complex layer architectures
- **Development/Testing**: Simple backend for development and testing
- **Backup Storage**: Local backup destination in multi-backend setups
- **Cache Implementation**: Local caching for remote operations

## Building and Testing

```bash
# Build all components
make build

# Run tests
make tests/run
```

**Test Coverage**: File lifecycle operations, error handling, concurrent access, large file operations

## Best Practices

- **File Permissions**: Ensure proper filesystem permissions
- **Path Validation**: Validate file paths before operations
- **Error Handling**: Always check return codes from operations
- **Resource Management**: Properly manage file descriptors
- **Buffer Sizing**: Use appropriate buffer sizes for I/O operations
- **System Limits**: Be aware of OS file descriptor limits

## Limitations

- **Local Only**: Cannot access remote filesystems
- **No Advanced Features**: No compression, encryption, or caching
- **OS Dependent**: Limited by operating system capabilities
- **Basic Operations**: Provides only fundamental file operations

## Troubleshooting

**Debugging**: Enable debug logging, check error codes, verify paths/permissions, monitor system limits 
