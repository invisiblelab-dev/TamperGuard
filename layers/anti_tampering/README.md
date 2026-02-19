# Anti-Tampering Layer

The **anti-tampering layer** provides comprehensive data integrity verification using configurable hash algorithms with thread-safe file locking to prevent race conditions. It ensures data anti-tampering by automatically computing and verifying file hashes on file closes.

## Key Features

- **Configurable Hash Algorithms**: Supports *SHA-256* and *SHA-512* with runtime selection
- **Automatic Hash Operations**: Computes hashes on file close, verifies on file open
- **Atomic Operations**: All operations are atomic with proper locking
- **Concurrent Reader Support**: Multiple readers with exclusive writer access
- **Race Condition Prevention**: Comprehensive locking strategy prevents data corruption
- **Layer Agnostic**: Works with any underlying storage system
- **Modular Design**: Uses the modular hasher system for extensible algorithm support

## Architecture

The anti-tampering layer uses a **dual-storage approach**:

- **Data Storage**: File content stored in the underlying *data_layer*
- **Hash Storage**: Hash metadata stored in separate *hash_layer* 
- **Hash File Naming**: Hash files named using SHA-256 of file path + ".hash"

```
┌─────────────────┐
│   Application   │
└─────────────────┘
         │
┌─────────────────┐
│ Anti-Tampering  │
├─────────────────┤
│   Hash Layer    │ ──── Stores hash files (.hash)
│   Data Layer    │ ──── Stores actual file data
└─────────────────┘
```

## Thread Safety & Locking

The anti-tampering layer implements **path-based locking** using pthread reader-writer locks:

| Operation | Lock Type | Purpose |
|-----------|-----------|---------|
| **Open** | *READ lock* | Hash verification can run concurrently |
| **Read** | *READ lock* | Multiple readers can access simultaneously |
| **Write** | *WRITE lock* | Exclusive access during data modification |
| **Close** | *WRITE lock* | Exclusive access during hash computation |

**Concurrent Operations**: Multiple file opens and reads can run simultaneously.
**Exclusive Operations**: Write and close operations block all other access.

## Configuration

### TOML Configuration
```toml
[anti_tamper_layer]
type = "anti_tampering"
hashes_storage = "/path/to/hash/directory"
hash_layer = "hash_layer_name"     # Layer for storing hash files
data_layer = "data_layer_name"     # Layer for storing actual data
algorithm = "sha256"               # Hash algorithm: "sha256" or "sha512"
```

### Parameters

- **`hashes_storage`** (string): Directory path prefix for hash files
- **`hash_layer`** (string): Name of layer configuration for storing hash files
- **`data_layer`** (string): Name of layer configuration for storing actual data
- **`algorithm`** (string): Hash algorithm - *"sha256"* (default) or *"sha512"*

| Algorithm | Speed | Security | Output Size | Use Case |
|-----------|-------|----------|-------------|----------|
| **SHA256** | Fast | Good | 64 hex chars | General purpose, performance-focused |
| **SHA512** | Slower | Better | 128 hex chars | High-security requirements |


## End-to-End Blockchain Example

The following example is one you can follow along to test the anti-tampering layer with a Solana blockchain as the hash storage layer for data integrity.

First, you need to build the project with the BUILD_INVISIBLE=1 flag to include the Solana layer:

```bash
make build BUILD_INVISIBLE=1
```

Then, you need to configure the project to use the anti-tampering layer with the Solana blockchain as the hash storage layer.

**Note**: If you don't have a keypair file, you can follow the steps in the [Solana Layer README](../invisible_storage/README.md) to create one.

```toml
root = "anti_tampering"
log_mode = "error"

[anti_tampering]
type = "anti_tampering"
hashes_storage = "/home/user/Modular-IO-Lib/examples/fuse/hashes" # Note: Use your absolute path here #
hash_layer = "solana"
data_layer = "local_layer"
algorithm = "sha256"

[solana_layer]
type = "solana"
keypair_path = "/home/user/.config/solana/id.json"
rpc_url = "https://api.devnet.solana.com"

[local_layer]
type = "local"
```

Then run the FUSE example in one terminal from the root of the project:

```bash
make examples/fuse/run
```

Write a file into the mounted directory:

```bash
echo "Hello, World!" > /home/user/Modular-IO-Lib/examples/fuse/mount_point/test.txt
```

Check the transactions in a solana block explorer:

You can go to [Solscan](https://solscan.io/?cluster=devnet) and search for your address and you should see the transaction with the hash of the file your wrote.

Further reads of the file will compare the hash of the file with the hash stored in the Solana blockchain.

## Operational Behavior

### File Operations

**Open**: Verifies file integrity by comparing stored hash with computed hash
**Read/Write**: Standard file operations with appropriate locking
**Close**: Computes and stores file hash for future verification

All operations are atomic and thread-safe with proper locking.

## Hash File Management

Hash files are named using **SHA-256 of the original file path**:
```
Original file: /path/to/myfile.txt
Hash filename: <hashes_storage>/a1b2c3d4e5f6...789.hash
```

The hash layer can be any supported layer type (local, remote, cloud storage).

## Error Handling

### Integrity Violations
- **Hash Mismatch**: File content doesn't match stored hash
- **Missing Hash**: Hash file doesn't exist for existing data file
- **Corrupted Hash**: Hash file is malformed or unreadable

### System Errors
- **Lock Failures**: Unable to acquire required locks
- **Storage Errors**: Underlying layer errors (disk full, permissions, etc.)
- **Hash Computation**: Hasher algorithm failures

### Return Codes
- **0**: Operation completed successfully
- **-1**: File integrity violation detected
- **-2**: Unable to acquire required lock
- **-3**: Underlying storage layer error

## Performance

**Read Performance**: Hash verification adds overhead on file open, but concurrent reads are supported
**Write Performance**: Hash computation adds overhead on file close with exclusive access
**Memory Usage**: O(n) for n open files, O(m) for m unique file paths

In order to get some performance numbers, we have used the Anti-Tampering layer in the configuration while running the FUSE example on top of a PostgreSQL database. We have tested three different configurations, and also tested it agains a non-mounted directory:
- Local Layer
- Anti-Tampering with hashes stored on Local
- Anti-Tampering with hashes stored on S3 

We have used pgbench to benchmark our DB with three different workloads: Mixed read/write, read-only (SELECT), and write-only (INSERT). All results are reported in transactions per second (TPS).

Test Configuration:
- Platform: AWS EC2 m7i.xlarge (4 vCPUs, 16 GB RAM)
- OS: AlmaLinux 9.6 (Sage Margay)
- Database: PostgreSQL (v13.22) with pgbench benchmarking tool
- Scale Factor: 50
- Test Duration: 5 runs of 30 minutes each

For S3 configurations, we used a VPC Gateway Endpoint to minimize access latency.

| Configuration              | Read/Write Mix | Read-Only      | Write-Only     |
|---------------------------|----------------|----------------|----------------|
| **No Fuse**               | 3602.04 ± 14.90| 85943.15 ± 291.07| 5020.75 ± 3.62  |
| **Local**                 | 3215.36 ± 31.33| 34709.66 ± 256.66| 4141.10 ± 55.51  |
| **Anti-Tampering Local Hash** | 2159.81 ± 30.52| 35281.88 ± 237.89| 4444.91 ± 357.23 |
| **Anti-Tampering S3 Hash** | 1790.32 ± 15.17| 33607.99 ± 1506.85| 3827.04 ± 33.37  |

## Use Cases

- **Critical data protection**: Detect unauthorized modifications
- **Compliance requirements**: Meet data integrity standards
- **Corruption detection**: Identify storage-level corruption
- **Tamper detection**: Identify malicious file modifications
- **Forensic integrity**: Maintain evidence chain of custody

## Building and Testing

```bash
# Build all components
make build

# Run tests
make tests/run
```

**Test Coverage**: Hash computation/verification, concurrent access, error handling, algorithm switching

## Security Considerations

### Hash Algorithm Security
- **SHA256**: Cryptographically secure, widely trusted
- **SHA512**: More secure, recommended for sensitive data
- **Collision Resistance**: Both algorithms resist hash collisions

### Protected Attack Scenarios
- **File Tampering**: Detects unauthorized file modifications
- **Data Corruption**: Identifies storage-level corruption
- **Integrity Verification**: Ensures data authenticity

### Unprotected Attack Scenarios
- **Hash File Tampering**: If attacker can modify both data and hash
- **Time-of-Check-Time-of-Use**: Race conditions outside layer control
- **Cryptographic Attacks**: Algorithm-specific vulnerabilities

### Security Best Practices
- Store hash files on separate, secured storage
- Use *SHA512* for high-security environments
- Monitor hash verification failures
- Implement backup hash storage
- Secure hash storage layer independently

## Troubleshooting

**Common Issues**: Hash mismatches, lock timeouts, permission errors, performance issues

**Debugging**: Enable debug logging, check hash files, verify underlying layers, test simple operations

**Performance Tuning**: Choose appropriate algorithm (SHA256 vs SHA512), optimize storage layer

## Hasher Architecture

The anti-tampering layer uses a **modular hasher system** with:
- **Generic EVP Implementation**: Shared OpenSSL EVP operations for all algorithms
- **Algorithm-Specific Wrappers**: Lightweight wrappers for SHA-256 and SHA-512

This enables easy addition of new hash algorithms while maintaining consistent interfaces. 
