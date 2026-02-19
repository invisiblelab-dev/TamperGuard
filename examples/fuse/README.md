# FUSE Example

The FUSE example demonstrates the Modular IO Library in a real-world filesystem context. It implements a FUSE-based passthrough filesystem that uses the layer system for all file operations.

## Overview

The FUSE example provides:

- **FUSE filesystem implementation** using the Modular IO Library
- **Passthrough functionality** that mirrors a backend directory
- **Layer integration** showing real-world usage patterns
- **Automatic directory management** with mount point and backend creation
- **Production-ready code** demonstrating best practices

## Features

- **Full FUSE Implementation**: Complete filesystem interface
- **Layer System Integration**: Uses configured layer stack for all operations
- **Automatic Setup**: Creates necessary directories automatically
- **Background/Foreground Modes**: Supports both daemon and foreground operation
- **Signal Handling**: Proper cleanup and unmounting on signals
- **Error Handling**: Comprehensive error handling and logging

## Architecture

### Integration Model
The FUSE example acts as a bridge between the FUSE kernel interface and the Modular IO Library:

```
┌─────────────────┐
│   File System   │ (User applications)
└─────────────────┘
         │
┌─────────────────┐
│ FUSE Kernel     │ (Linux FUSE kernel module)
└─────────────────┘
         │
┌─────────────────┐
│ FUSE Example    │ (Our implementation)
└─────────────────┘
         │
┌─────────────────┐
│ Modular IO Lib  │ (Configured layer stack)
└─────────────────┘
         │
┌─────────────────┐
│ Backend Storage │ (examples/fuse/backend_data/)
└─────────────────┘
```

### Directory Structure
```
examples/fuse/
├── Makefile                    # Build configuration
├── passthrough.c              # Main FUSE implementation
├── passthrough_helpers.h      # Helper functions and utilities
├── mount_point/               # FUSE mount point (auto-created)
└── backend_data/              # Backend storage directory (auto-created)
```

## Building and Running

### Build
```bash
# Build the FUSE example
make examples/fuse/build
```

### Run Options
```bash
# Foreground mode (for debugging)
make examples/fuse/run

# Background mode (daemon)
make examples/fuse/run/daemon

# Stop the filesystem
make examples/fuse/stop
```

### Configuration File

By default, the FUSE example uses `./config.toml` in the project root directory. You can specify a custom configuration file using the `MODULAR_IO_CONFIG_PATH` variable:

```bash
# Use custom config file (absolute path)
make examples/fuse/run MODULAR_IO_CONFIG_PATH=/path/to/my/config.toml

# Use custom config file (relative to project root)
make examples/fuse/run MODULAR_IO_CONFIG_PATH=configs/production.toml

# Set via environment variable
export MODULAR_IO_CONFIG_PATH=/path/to/config.toml
make examples/fuse/run

# Or inline
MODULAR_IO_CONFIG_PATH=/path/to/config.toml make examples/fuse/run
```

**Note:** Relative paths are automatically resolved relative to the project root directory (`ROOT_DIR`).

### Clean
```bash
# Clean build artifacts
make examples/fuse/clean
```

## Usage

Once running, the FUSE filesystem appears as a normal directory that can be accessed by any application. All file operations are transparently processed through the configured layer stack.

### Basic Operations
Standard filesystem operations are supported:

- **File operations**: Create, read, write, delete files
- **Directory operations**: Create, list, remove directories
- **Metadata operations**: Get/set file attributes and permissions
- **Large file support**: Handle files of arbitrary size

### Layer Configuration
The filesystem behavior can be modified by changing the layer configuration:

- **Compression**: Automatically compress stored data
- **Integrity**: Add hash-based verification
- **Multi-backend**: Replicate data across multiple storage systems
- **Block alignment**: Optimize for specific storage characteristics

## Implementation Details

### FUSE Operations
The implementation provides standard FUSE operations:

- **File I/O**: Read, write operations with offset support
- **Metadata**: File attributes, permissions, timestamps
- **Directory management**: Directory creation, listing, removal
- **Path resolution**: Proper path handling and resolution

### Error Handling
Comprehensive error handling with appropriate FUSE error codes:

- **File system errors**: ENOENT, EACCES, EIO, ENOSPC
- **Layer propagation**: Errors from underlying layers are properly translated
- **Resource management**: Proper cleanup on errors and shutdown

### Performance Considerations
Design choices for optimal performance:

- **Direct passthrough**: Minimal overhead for supported operations
- **Buffer management**: Efficient memory usage for large files
- **Concurrent access**: Thread-safe operation handling

## Testing

### Functionality Testing
Test basic filesystem operations:

- File creation, reading, writing, deletion
- Directory operations and navigation
- Large file handling
- Permission and attribute management

### Integration Testing
Verify layer integration:

- Test different layer configurations
- Verify data integrity through layers
- Performance testing with various layer stacks

### Error Scenarios
Test error handling:

- Invalid file operations
- Permission denied scenarios
- Layer failure conditions
- Resource exhaustion situations

## Requirements

### System Dependencies
- **FUSE**: libfuse3-dev or equivalent
- **Build tools**: Standard C development environment
- **Permissions**: User must have FUSE access rights

### Configuration
- Valid `config.toml` with layer configuration
- Proper logging setup via `zlog.conf`
- Sufficient permissions for mount operations

## Troubleshooting

### Common Issues

**FUSE Not Available**:

- Verify FUSE is installed on the system
- Check if FUSE kernel module is loaded
- Install development headers if building from source

**Permission Issues**:

- Add user to the `fuse` group
- Check mount point permissions
- Verify backend directory access

**Mount Failures**:

- Ensure mount point directory exists and is empty
- Check for conflicting mount points
- Verify configuration file syntax

**Layer Configuration Issues**:

- Validate TOML configuration syntax
- Check layer parameter correctness
- Verify required dependencies are built

### Debugging

- **Foreground mode**: Run with `make examples/fuse/run` for detailed output
- **Log analysis**: Check generated log files for error details
- **FUSE debugging**: Use FUSE debugging options for low-level issues
- **Layer debugging**: Enable debug logging in configuration 
