# Invisible Storage Layer

The **invisible storage layer** provides integration with invisible storage systems for the Modular IO Library. It enables applications to store and retrieve data using advanced *invisibility techniques*, leveraging external Rust-based invisible storage bindings.

## Invisible Storage Layers

The **invisible storage layers** provide seamless integration with *cloud storage services* and **blockchain networks**, enabling data to be stored in distributed systems while maintaining the modular I/O interface. These layers abstract complex network protocols and authentication mechanisms.

### Available Layer Types

This directory contains implementations for:

- **S3 OpenDAL Layer**: *Amazon S3-compatible* storage services
- **Solana Layer**: *Blockchain-based* storage on the Solana network
- **IPFS OpenDAL Layer**: *IPFS-compatible* storage services

### Configuration

#### S3 OpenDAL Layer

To configure an **S3 OpenDAL layer** in your `config.toml`, define it as follows:

```toml
[layer_name]
type = "s3_opendal"
endpoint = "https://s3.amazonaws.com"          # S3 service endpoint
access_key_id = "your_access_key"              # AWS access key ID
secret_access_key = "your_secret_key"          # AWS secret access key
bucket = "your_bucket_name"                    # S3 bucket name
region = "us-east-1"                           # AWS region
root = "path/prefix"                           # Root path prefix within bucket
```

**S3 OpenDAL Configuration Parameters (all required):**

- `endpoint`: S3-compatible service endpoint URL
- `access_key_id`: Authentication access key identifier
- `secret_access_key`: Authentication secret key
- `bucket`: Name of the S3 bucket for storage
- `region`: AWS region or equivalent for the service
- `root`: Path prefix for all files within the bucket

#### Solana Layer

To configure a **Solana layer** in your `config.toml`, define it as following this example:

```toml
[layer_name]
type = "solana"
rpc_url = "https://api.devnet.solana.com"    # Solana RPC endpoint
keypair_path = "/path/to/keypair.json"             # Path to Solana keypair file
```

**Solana Configuration Parameters (all required):**

| Parameter       | Description                                             | Example                                                | How to obtain                                        | Security notes                          |
|-----------------|---------------------------------------------------------|--------------------------------------------------------|------------------------------------------------------|-----------------------------------------|
| rpc_url         | Solana RPC endpoint used for on-chain operations        | https://api.devnet.solana.com                    | Public RPC (mainnet/devnet/testnet) or a private RPC | Prefer trusted/private RPC if available |
| keypair_path    | Absolute path to a Solana JSON keypair for authentication | /home/user/.config/solana/id.json                    | Create with solana-keygen or use existing keypair    | Protect file permissions (chmod 600)    |

#### IPFS OpenDAL Layer

To configure an **IPFS OpenDAL layer** in your `config.toml`, define it as follows:

```toml
[layer_name]
type = "ipfs_opendal"
api_endpoint = "http://127.0.0.1:5001"
root = "/ipfs/"
```

**IPFS OpenDAL Configuration Parameters (all required):**

- `api_endpoint`: IPFS API endpoint URL
- `root`: Path prefix for all files within the IPFS

**First time setup:**

If you don't have a keypair file, or want to create a new one, you can follow the following steps:

**I. Install Solana CLI:**

Follow the instructions [here](https://solana.com/docs/intro/installation) to install the Solana CLI. Tested with version v3.0.8.

**Note**: The next steps are to configure for devnet, but you can use the same steps for mainnet or testnet.

**II. Configure your Solana CLI to use devnet:**

Run the following command in your terminal:

```bash
solana config set --url devnet
```

**III. Get your wallet address:**

If you don't have a keypair, this command will generate one automatically:

```bash
solana address
```

**IV. Get some SOL:**

The following command should airdrop 1 SOL to your wallet:

```bash
solana airdrop 1
```

**V. Find Your Keypair File Path:**

To find the location of your keypair file, run:

```bash
solana config get
```

Look for the following line:
```
Keypair Path: /Users/youruser/.config/solana/id.json
```

This is the path to your keypair file.

**VI. Add the Keypair Path to the Project Config:**

Paste the keypair path into your project’s config file:

```toml
[solana_sdk]
type = "solana"
rpc_url = "https://api.devnet.solana.com"
keypair_path = "/Users/youruser/.config/solana/id.json"
```

**Usage Notes:**

- All S3 OpenDAL parameters are required for proper authentication and operation
- Solana requires a valid keypair file with appropriate permissions
- Network connectivity and valid credentials are required for both layer types
- These layers handle complex networking and authentication internally

## Overview

The invisible storage layer offers:

- **Invisible data storage** using advanced steganographic techniques
- **Integration** with external Rust invisible storage library
- **Covert operations** that hide data existence from casual observation
- **Security enhancement** through data concealment
- **Seamless interface** matching standard storage operations

## Features

- **Data Concealment**: Stores data in ways that hide its existence
- **Rust Integration**: Uses high-performance Rust bindings for core operations  
- **Steganographic Storage**: Advanced techniques for data invisibility
- **Security Layer**: Adds security through obscurity
- **Standard Interface**: Compatible with existing layer architecture
- **Performance**: Optimized Rust implementation for efficiency

## Architecture

### Integration Model
```
┌─────────────────┐
│   Application   │
└─────────────────┘
         │
┌─────────────────┐
│ Invisible Layer │
└─────────────────┘
         │
┌─────────────────┐
│  Rust Bindings  │ ──── External invisible storage library
└─────────────────┘
         │
┌─────────────────┐
│ Invisible Store │ ──── Hidden/steganographic storage
└─────────────────┘
```

### External Dependencies
The invisible storage layer depends on:

- **Rust invisible storage bindings**: Located in `lib/invisible-storage-bindings/`
- **External Rust library**: Provides core invisible storage functionality
- **Native interface**: C-compatible bindings for integration

## Configuration

### TOML Configuration
```toml
[invisible_layer]
type = "invisible_storage"
# Configuration parameters depend on specific invisible storage implementation
```

### Building Dependencies

#### Build External Library
```bash
# Build the Rust invisible storage library
make external/libinvisible/build

# Or use the combined build command
make libinvisible/build
```

#### Clean External Library
```bash
# Clean the external library
make libinvisible/clean
```

## Layer Implementation

### Initialization
The invisible storage layer initializes by:

1. **Loading Rust bindings**: Connects to external invisible storage library
2. **Configuration setup**: Applies invisible storage configuration
3. **Interface creation**: Creates standard layer interface
4. **Resource allocation**: Allocates necessary resources

### Operation Mapping
Standard layer operations are mapped to invisible storage:

- **Open**: Initialize invisible file access
- **Read**: Read data from invisible storage
- **Write**: Write data to invisible storage  
- **Close**: Finalize invisible file operations

## Example Application

### Running the Invisible Example
```bash
# Build the invisible storage example
make examples/invisible/build

# Run the example application
make examples/invisible/run
```

The invisible example demonstrates:

- **Integration patterns**: How to use invisible storage in applications
- **Configuration**: Proper setup for invisible storage operations
- **Operations**: Reading, writing, and managing invisible data
- **Performance**: Benchmarking invisible storage operations

## Security Considerations

### Security Benefits

- **Data concealment**: Hides data existence from unauthorized observation
- **Steganographic protection**: Uses advanced hiding techniques
- **Access control**: Controls access to invisible data
- **Covert channels**: Enables covert communication channels

### Security Limitations

- **Detection resistance**: May not resist sophisticated detection methods  
- **Performance overhead**: Concealment techniques add processing overhead
- **Dependency trust**: Relies on security of external Rust library
- **Key management**: Requires secure key management for access

### Best Practices

- **Combine with encryption**: Use alongside traditional encryption
- **Key security**: Securely manage access keys and credentials
- **Detection testing**: Test against detection tools
- **Backup strategies**: Maintain secure backup of invisible data access methods

## Performance Characteristics

### Performance Factors

- **Rust implementation**: High-performance core operations
- **Concealment overhead**: Additional processing for invisibility
- **Storage medium**: Performance depends on underlying storage
- **Access patterns**: Sequential vs random access performance

### Optimization Strategies

- **Batch operations**: Group operations for efficiency
- **Caching**: Cache frequently accessed invisible data
- **Async operations**: Use asynchronous I/O where possible
- **Memory management**: Optimize buffer usage

## Integration Testing

### Development Testing
```bash
# Run with invisible storage components  
make build                    # Build all components including invisible
make examples/invisible/run   # Test invisible storage integration
```

### Validation Steps

1. **Library loading**: Verify Rust bindings load correctly
2. **Operation testing**: Test all layer operations
3. **Data integrity**: Verify data consistency
4. **Performance testing**: Benchmark operation performance
5. **Security testing**: Validate concealment effectiveness

## External Library Management  

### Submodule Management
```bash
# Initialize invisible storage submodule
git submodule update --init --recursive

# Update to latest version
make submodules/fetch
```

### Build System Integration
The invisible storage layer integrates with the build system:

- **Automatic building**: Built as part of main build process
- **Dependency management**: Handles Rust library dependencies
- **Cross-platform**: Supports multiple platforms
- **Version tracking**: Tracks external library versions

## Error Handling

### Library Errors

- **Load failures**: External library loading issues
- **Interface errors**: Rust-C interface communication problems
- **Configuration errors**: Invalid invisible storage configuration
- **Resource errors**: Memory or resource allocation failures

### Storage Errors

- **Invisibility failures**: Data concealment operation failures
- **Detection risks**: Potential exposure of invisible data
- **Corruption**: Invisible data corruption or loss
- **Access failures**: Unable to access invisible storage

### Recovery Mechanisms

- **Fallback storage**: Fallback to visible storage on failures
- **Error logging**: Detailed error logging for debugging
- **State recovery**: Attempt recovery of invisible storage state
- **Data rescue**: Emergency data recovery procedures

## Troubleshooting

### Common Issues

- **Library not found**: External Rust library not built or installed
- **Interface failures**: Version mismatches between C and Rust code
- **Performance issues**: Excessive concealment overhead
- **Detection warnings**: Invisible data may be detectable

### Debugging Steps  

1. **Verify build**: Ensure external library built successfully
2. **Check logs**: Enable debug logging for detailed information
3. **Test isolation**: Test invisible storage layer in isolation
4. **Validate config**: Verify invisible storage configuration
5. **Performance profile**: Profile operations for bottlenecks

### Build Issues
```bash
# Common build troubleshooting
make clean/all           # Clean all components including external
make libinvisible/build  # Rebuild external library
make build               # Rebuild entire project
```

