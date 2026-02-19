# Remote Layer

The **remote layer** provides access to *distributed storage systems* and **remote backends**, enabling the modular I/O system to work with networked storage infrastructures. This layer abstracts network communication protocols and manages **remote connectivity**.

### Key Features

- **Network-aware operations** with built-in connectivity management
- **Protocol abstraction** supporting various remote storage systems
- **Connection pooling** and **retry mechanisms** for robust network operations
- **Thread-safe networking** for concurrent remote access

### Configuration

To configure a **remote layer** in your `config.toml`, define it as follows:

```toml
[layer_name]
type = "remote"
```

**Configuration Parameters:**

The remote layer requires no additional configuration parameters in its current implementation. Remote connectivity details are managed internally.

**Usage Notes:**

- Designed for integration with distributed storage systems
- Network latency and reliability considerations apply
- Suitable for cloud-based and distributed storage scenarios

## Overview

The remote layer implements:

- **Network-based file operations** over TCP sockets
- **Client-server architecture** for distributed storage
- **Protocol abstraction** for remote I/O operations
- **Connection management** and error handling

## Features

- **Network Communication**: TCP socket-based client-server protocol
- **Remote I/O Operations**: Full I/O operation support over network
- **Connection Management**: Handles connection establishment and cleanup
- **Protocol Abstraction**: Hides network complexity from applications
- **Error Propagation**: Maintains error semantics across network boundaries

## Architecture

### Client-Server Model
```
┌─────────────┐    TCP/IP    ┌─────────────┐
│   Client    │ ◄─────────► │   Server    │
│ (Layer)     │   Protocol   │ (Storage)   │
└─────────────┘              └─────────────┘
```

### Communication Protocol

The remote layer uses a **custom message protocol** that provides:

- **Fixed-size messages** for efficient parsing
- **Binary protocol** for optimal performance
- **Synchronous operations** with request-response pattern
- **Error code propagation** from server to client

### Core Operations

- **File Management**: Open, close, and file metadata operations
- **Data Transfer**: Read and write operations with positioning support
- **Error Handling**: Network and filesystem error propagation
- **Connection Control**: Connection lifecycle management

## Configuration

### TOML Configuration
```toml
[remote_layer]
type = "remote"
# Server connection details configured at compile time
# Default: 127.0.0.1:5000
```

**Note**: Current implementation uses *compile-time configuration* for server endpoints.

## Operational Behavior

### Connection Management

- **Initialization**: Automatic connection establishment on layer creation
- **Persistence**: Socket connection maintained in layer state
- **Error Recovery**: Network errors propagated to application
- **Cleanup**: Connection closed during layer destruction

### I/O Operation Flow

#### Read Operations

1. **Request Construction**: Client prepares read request message
2. **Network Transmission**: Request sent to server via TCP
3. **Server Processing**: Server performs local read operation
4. **Response Return**: Server sends data and result back to client
5. **Result Delivery**: Client returns data to application

#### Write Operations

1. **Data Packaging**: Client packages write data in request message
2. **Network Transmission**: Request with data sent to server
3. **Server Processing**: Server performs local write operation
4. **Result Return**: Server confirms operation success/failure
5. **Result Delivery**: Client returns operation result to application

## Server Integration

### Server Component

The remote layer works with the **storage server** component:

- **Multi-client support**: Handles multiple client connections
- **Request processing**: Processes remote I/O requests
- **Local operations**: Performs actual file operations server-side
- **Result transmission**: Returns operation results to clients

### Server Management
```bash
# Server operations
make examples/storserver/build    # Build storage server
make examples/storserver/run      # Run storage server
```

## Performance Characteristics

### Network Considerations

- **Latency Impact**: Network round-trip time affects all operations
- **Bandwidth Utilization**: Transfer efficiency depends on network capacity
- **Protocol Overhead**: Synchronous protocol introduces latency
- **Buffer Management**: Message buffer sizes affect transfer efficiency

### Optimization Strategies

- **Connection Reuse**: Maintain persistent connections
- **Batch Operations**: Group multiple operations when possible
- **Compression**: Consider data compression for large transfers
- **Async Support**: Implement asynchronous operations for better throughput

## Security Considerations

### Current Security Model

- **Trust-based**: Assumes trusted network environment
- **No Authentication**: No built-in client authentication
- **Plaintext Communication**: Data transmitted without encryption
- **Basic Access Control**: Limited server-side access controls

### Security Recommendations

- **Network Security**: Use within secure/trusted networks
- **Access Controls**: Implement server-side access restrictions
- **Encryption**: Consider adding encryption for sensitive data
- **Authentication**: Implement client authentication mechanisms

## Error Handling

### Network-Level Errors

- **Connection failures**: Server unavailability or network issues
- **Communication errors**: Socket errors and transmission failures
- **Timeout conditions**: Operations that exceed time limits
- **Server crashes**: Handling server-side failures

### File System Errors

- **Remote filesystem errors**: Server-side file operation failures
- **Permission issues**: Access denied on remote system
- **Resource constraints**: Server-side disk space or file limits
- **Path resolution**: Remote path validation and resolution errors

## Building and Testing

### Build Commands
```bash
# Build all components (includes remote layer)
make build

# Or build just shared components (includes remote layer)
make shared/build

# Build and run server component
make examples/storserver/build
make examples/storserver/run

# Run remote layer tests (requires server)
make tests/run
```

### Test Requirements

- **Server dependency**: Tests require running storage server
- **Network connectivity**: Local network access for testing
- **Integration testing**: Multi-component testing scenarios

## Use Cases

### Distributed Storage Scenarios

- **Remote backup**: Off-site data backup and replication
- **Centralized storage**: Shared storage across multiple clients
- **Load distribution**: Distribute storage across multiple servers
- **Disaster recovery**: Remote site redundancy

### Integration Patterns

- **Multi-backend systems**: Combine with local storage for redundancy
- **Cache hierarchies**: Remote storage as backing store for local cache
- **Distributed applications**: Storage layer for distributed systems
- **Development environments**: Remote storage for development/testing
