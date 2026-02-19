# Storage Server Example

The storage server example provides a network-based storage server that works with the remote layer of the Modular IO Library. It implements a TCP server that handles remote file operations, enabling distributed storage scenarios and client-server architectures.

## Overview

The storage server example offers:
- **Network storage server** for remote file operations
- **TCP-based protocol** for client-server communication
- **Multi-client support** with concurrent connection handling
- **Standard file operations** over network
- **Independent operation** from the layer system (server-side implementation)

## Features

- **TCP Server**: Listens for client connections on configurable port
- **Multi-client**: Handles multiple concurrent client connections
- **Full File Operations**: Supports read, write, open, close, stat operations
- **Binary Protocol**: Efficient binary message protocol
- **Error Handling**: Proper error propagation to clients
- **Logging**: Comprehensive operation logging

## Architecture

### Server-Client Model
```
┌─────────────────┐    TCP/IP    ┌─────────────────┐
│   Clients       │ ◄─────────► │ Storage Server  │
│ (Remote Layer)  │   Protocol   │   (storserver)  │
└─────────────────┘              └─────────────────┘
                                          │
                                 ┌─────────────────┐
                                 │ Local Storage   │
                                 │   (Server-side) │
                                 └─────────────────┘
```

### Protocol Implementation
The server uses the same message protocol as the remote layer:

```c
typedef struct msg {
    int op;                    // Operation type
    char path[PSIZE];         // File path
    char buffer[BSIZE];       // Data buffer
    int flags;                // Open flags  
    off_t offset;             // File offset
    size_t size;              // Operation size
    ssize_t res;              // Result code
    int fd;                   // File descriptor
    mode_t mode;              // File permissions
    struct stat st;           // File status
} MSG;
```

### Supported Operations
- **READ (0)**: Read data from file
- **WRITE (1)**: Write data to file
- **STAT (2)**: Get file status information
- **OPEN (3)**: Open file with specified flags
- **UNLINK (4)**: Delete file
- **CLOSE (5)**: Close file descriptor

## Building and Running

### Build the Server
```bash
# Build the storage server example
make examples/storserver/build
```

### Run the Server
```bash
# Run the storage server
make examples/storserver/run
```

### Clean Build Artifacts
```bash
# Clean the storage server example
make examples/storserver/clean
```

## Server Configuration

### Default Configuration
The server uses hardcoded configuration (defined in `remote.h`):
```c
#define PORT 5000          // Server port
#define IP "127.0.0.1"     // Server bind address
#define LISTEN_BACKLOG 50  // Connection queue size
```

### Buffer Sizes
```c
#define BSIZE 4096    // Data buffer size
#define PSIZE 512     // Path string size
```

## Usage Examples

### Basic Server Operation
```bash
# Terminal 1: Start the server
make examples/storserver/run

# Terminal 2: Test with a client using remote layer
# Configure config.toml with remote layer
root = "remote_storage"
[remote_storage]
type = "remote"

# Run application that uses remote layer
./your_application
```

### Multi-Client Testing
```bash
# Start server
make examples/storserver/run &

# Multiple clients can connect simultaneously
# Each client gets independent file operations
```

### Integration with Examples
```bash
# Start storage server
make examples/storserver/run &

# Use FUSE example with remote backend
# Configure FUSE to use remote layer
# Files written to FUSE will be stored on server
```

## Implementation Details

### Server Initialization
```c
int main() {
    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // Bind to address and port
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(IP),
        .sin_port = htons(PORT)
    };
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    // Listen for connections
    listen(server_socket, LISTEN_BACKLOG);
    
    // Accept and handle client connections
    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        handle_client(client_socket);
    }
}
```

### Client Handler
Each client connection is handled by processing messages:

```c
void handle_client(int client_socket) {
    MSG msg;
    
    while (1) {
        // Receive message from client
        if (recv(client_socket, &msg, sizeof(MSG), 0) <= 0) {
            break;  // Client disconnected
        }
        
        // Process operation
        switch (msg.op) {
            case READ:
                handle_read(&msg);
                break;
            case WRITE:
                handle_write(&msg);
                break;
            case OPEN:
                handle_open(&msg);
                break;
            // ... other operations
        }
        
        // Send response back to client
        send(client_socket, &msg, sizeof(MSG), 0);
    }
    
    close(client_socket);
}
```

### File Operation Handlers

#### Read Operation
```c
void handle_read(MSG *msg) {
    ssize_t bytes_read = pread(msg->fd, msg->buffer, msg->size, msg->offset);
    msg->res = bytes_read;
}
```

#### Write Operation
```c
void handle_write(MSG *msg) {
    ssize_t bytes_written = pwrite(msg->fd, msg->buffer, msg->size, msg->offset);
    msg->res = bytes_written;
}
```

#### Open Operation
```c
void handle_open(MSG *msg) {
    int fd = open(msg->path, msg->flags, msg->mode);
    msg->res = fd;
}
```

## Protocol Details

### Message Flow
1. **Client Request**: Client sends MSG structure with operation details
2. **Server Processing**: Server processes the requested operation
3. **Server Response**: Server sends modified MSG back with results
4. **Error Handling**: Errors communicated via negative result codes

### Connection Management
- **Persistent Connections**: Clients maintain connections across operations
- **Connection Pooling**: Multiple clients can connect simultaneously
- **Cleanup**: Server handles client disconnections gracefully

### Data Transfer
- **Binary Protocol**: Efficient binary message format
- **Fixed Buffer Size**: BSIZE limits single operation data transfer
- **Large Files**: Large files handled via multiple operations

## Performance Characteristics

### Throughput
- **Network Limited**: Performance limited by network bandwidth
- **Concurrent Clients**: Server handles multiple clients concurrently
- **Local I/O**: Server-side I/O performance affects overall throughput

### Scalability
- **Connection Limits**: Limited by system file descriptor limits
- **Memory Usage**: Each client connection uses memory for buffers
- **CPU Usage**: Minimal CPU overhead per operation

### Optimization Opportunities
- **Threading**: Multi-threaded client handling
- **Async I/O**: Non-blocking I/O operations
- **Connection Pooling**: Reuse connections efficiently
- **Caching**: Server-side caching for frequently accessed files

## Security Considerations

### Current Limitations
- **No Authentication**: Server accepts all connections
- **No Encryption**: Data transmitted in plaintext
- **No Authorization**: No access control on files
- **Trust Model**: Assumes trusted clients and network

### Security Recommendations
- **Network Security**: Use in trusted network environments
- **Firewall Rules**: Restrict access to server port
- **VPN/Tunneling**: Use secure tunneling for untrusted networks
- **Access Control**: Implement file system permissions

### Future Security Enhancements
- **Authentication**: Client certificate or token-based auth
- **Encryption**: TLS/SSL encryption for data transmission
- **Authorization**: Per-client access control lists
- **Audit Logging**: Detailed operation audit logs

## Testing

### Unit Testing
```bash
# Test server functionality
make examples/storserver/build
make tests/run  # Includes server integration tests
```

### Integration Testing
```bash
# Start server
make examples/storserver/run &
SERVER_PID=$!

# Test with remote layer
root = "remote_test"
[remote_test]
type = "remote"

# Run client operations
./test_client

# Cleanup
kill $SERVER_PID
```

### Load Testing
```bash
# Start server
make examples/storserver/run &

# Multiple concurrent clients
for i in {1..10}; do
    ./test_client &
done
wait

# Monitor server performance
top -p $(pgrep storserver)
```

## Troubleshooting

### Common Issues

#### Server Won't Start
```bash
# Check if port is already in use
netstat -ln | grep :5000

# Kill existing process using port
sudo lsof -ti:5000 | xargs kill -9

# Check firewall settings
sudo iptables -L | grep 5000
```

#### Connection Refused
```bash
# Verify server is running
netstat -ln | grep :5000

# Test connection
telnet 127.0.0.1 5000

# Check client configuration
# Ensure remote layer configured correctly
```

#### Performance Issues
```bash
# Monitor network usage
iftop

# Monitor server resources
htop

# Check for connection limits
ulimit -n
```

### Debugging

#### Enable Debug Logging
```c
// Add debug prints to server code
printf("Handling client: %d\n", client_socket);
printf("Operation: %d, Path: %s\n", msg.op, msg.path);
```

#### Network Debugging
```bash
# Capture network traffic
sudo tcpdump -i lo port 5000

# Monitor server connections
ss -tuln | grep :5000
```

#### File System Debugging
```bash
# Monitor server-side file operations
strace -p $(pgrep storserver)

# Check file permissions
ls -la /path/to/server/files
```

## Real-World Applications

### Use Cases
1. **Distributed Storage**: Centralized file storage for multiple clients
2. **Backup Systems**: Remote backup destination
3. **Development/Testing**: Test remote layer functionality
4. **Microservices**: File storage service in microservice architecture
5. **Edge Computing**: Remote file access in edge scenarios

### Production Deployment
- **Process Management**: Use systemd or similar for service management
- **Load Balancing**: Multiple server instances with load balancer
- **Monitoring**: Health checks and performance monitoring
- **Backup**: Regular backup of server-side storage

## Future Enhancements

### Planned Features
- **Configuration File**: TOML-based server configuration
- **Authentication**: Client authentication mechanisms
- **SSL/TLS**: Encrypted communication
- **Clustering**: Multi-server clustering support
- **Statistics**: Operation statistics and monitoring

### Advanced Configuration
```toml
# Future server configuration
[server]
bind_address = "0.0.0.0"
port = 5000
max_clients = 100
buffer_size = 8192
ssl_enabled = true
ssl_cert = "/path/to/cert.pem"
ssl_key = "/path/to/key.pem"

[auth]
enabled = true
method = "token"
token_file = "/path/to/tokens"

[storage]
root_directory = "/var/lib/storserver"
max_file_size = "1GB"
temp_directory = "/tmp/storserver"
```

## Integration with Layer System

While the storage server operates independently, it integrates with the layer system through the remote layer:

### Client-Side Integration
```toml
# Client configuration using remote layer
root = "remote_with_integrity"

[remote_with_integrity]
type = "anti_tampering"
data_layer = "remote_storage"
hash_layer = "local_hashes"

[remote_storage]
type = "remote"

[local_hashes]
type = "local"
```

This configuration provides:
- **Remote storage** via the storage server
- **Local integrity** checking with anti-tampering layer
- **Transparent operation** for applications 
