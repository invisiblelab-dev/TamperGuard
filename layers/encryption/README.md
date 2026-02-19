# Encryption Layer

The **encryption layer** provides transparent data encryption and decryption. It ensures *data confidentiality* by encrypting data before passing it to subsequent layers.

## Overview

The encryption layer features:

- **Transparent encryption/decryption** on read/write operations
- **Block-based encryption** with configurable block sizes
- **Random access support** for efficient I/O at any offset
- **Extensible cipher support** - currently implements AES-256-XTS only
- **Layer-agnostic design** working with any underlying storage
- **Unique IVs per block** for enhanced security, making it difficult to find inter-block patterns

## Configuration

The encryption layer supports two methods for providing the encryption key:

### Method 1: Direct Key Configuration

Provide the encryption key directly in the configuration file:

```toml
[layer_name]
type = "encryption"
next = "next_layer_name"              # Name of the next layer in the chain
block_size = 4096                     # Block size in bytes for encryption operations (default: 4096, minimum: 16)
encryption_key = "your-64-char-hex-key-here"  # 64-character hexadecimal key (256 bits)
```

### Method 2: Vault/OpenBao Integration (Recommended)

Fetch the encryption key from a HashiCorp Vault or OpenBao server at initialization:

```toml
[layer_name]
type = "encryption"
next = "next_layer_name"              # Name of the next layer in the chain
block_size = 4096                     # Block size in bytes
api_key = "your-vault-token"          # Vault authentication token
vault_addr = "http://localhost:8200"  # Vault server address
secret_path = "v1/secret/data/myapp/encryption"  # Path to the secret in Vault
```

**Vault Configuration Details:**

- **api_key**: The Vault token used for authentication (sent as `X-Vault-Token` header)
- **vault_addr**: The base URL of your Vault server
- **secret_path**: Path to the secret in Vault (e.g., `v1/secret/data/myapp/encryption`)

> **OpenBao Integration**: This feature has been tested and verified to work with OpenBao. See [OPENBAO.md](OPENBAO.md) for a complete guide on deploying OpenBao and configuring it to provide encryption keys.

The Vault server should return a JSON response with the key in the following format:

```json
{
  "data": {
    "data": {
      "key": "7da46e98f9643f34e8a4c68079816ec1ca9bbf4c68a3e50f842808848df50119"
    }
  }
}
```

**Example Vault/OpenBao Setup:**

```bash
# Store encryption key in Vault/OpenBao
vault kv put secret/myapp/encryption key="7da46e98f9643f34e8a4c68079816ec1ca9bbf4c68a3e50f842808848df50119"

# Or using OpenBao (bao CLI)
bin/bao kv put secret/myapp/encryption key="7da46e98f9643f34e8a4c68079816ec1ca9bbf4c68a3e50f842808848df50119"

# Test retrieval
curl -H "X-Vault-Token: $VAULT_TOKEN" \
     "$VAULT_ADDR/v1/secret/data/myapp/encryption"
```

> **Security Note**: Using Vault/OpenBao integration is the recommended approach for production environments as it centralizes key management and avoids storing sensitive keys in configuration files.

## Block Encryption Example
```
Data: 10KB write to file, Block Size: 4096 bytes

┌────────┬─────────┬────────┐
│ Block 0  │ Block 1  │ Block 2  │
│ 4096 B   │ 4096 B   │ 1808 B   │
│ IV=0     │ IV=1     │ IV=2     │
└────────┴─────────┴────────┘
     ↓          ↓          ↓
  Encrypt    Encrypt    Encrypt
     ↓          ↓          ↓
┌────────┬─────────┬────────┐
│Encrypted │Encrypted │Encrypted │
│ 4096 B   │ 4096 B   │ 1808 B   │
└────────┴─────────┴────────┘
```

## Supported Ciphers

### AES-256-XTS

- **Algorithm**: AES-256 in XTS mode (XEX-based tweaked-codebook mode with ciphertext stealing)
- **Key Size**: 512 bits total (two 256-bit keys for XTS)
- **Block Size**: Minimum 16 bytes (AES block size)
- **Characteristics**:
  - Designed specifically for disk/block encryption
  - No ciphertext expansion (output size = input size)
  - Supports independent block encryption/decryption (random access)
  - Handles partial blocks ≥ 16 bytes
  - NIST SP 800-38E compliant

**Why XTS Mode?** \
XTS is specifically designed for storage encryption, providing strong security without ciphertext expansion, which is ideal for block-based storage systems requiring random access patterns.

**XTS Mode Limitations**
   - **No authentication**: Does not provide integrity protection or tampering detection
   - **Minimum block size**: Cannot encrypt data smaller than 16 bytes (AES block size)
   - **Pattern leakage**: If not used properly, it is susceptible to traffic analysis and replay attacks
   - **Tweak dependency**: Security relies on unique tweaks (IVs) per block

## Operational Behavior

### Encryption Process (Write Operations)

1. Divide data into block-sized chunks
2. For each block, generate unique IV from block counter
3. Encrypt block using configured cipher
4. Write encrypted blocks to underlying layer

### Read Operations (Decryption)

1. Read encrypted data from underlying layer
2. Divide encrypted data into blocks based on the block size
3. For each block, generate matching IV from block counter
4. Decrypt block and return plaintext to application

### IV Generation

Each block uses a unique initialization vector derived from its position:

```c
uint64_t block_counter = 0;
unsigned char iv[16] = {0};
memcpy(iv, &block_counter, sizeof(uint64_t));
block_counter++;
```

## Security Limitations

**NOT SUITABLE FOR PRODUCTION**:

1. **No Integrity Protection**:
   - Provides confidentiality only (AES-XTS does not authenticate data)
   - Encrypted data can be modified or corrupted without detection ([Anti-Tampering Layer](../anti_tampering/README.md) can be used to add integrity protection)

2. **Hardcoded Key**:
   - All files encrypted with the same key
   - Key is visible in source code

3. **Weak IV Generation**:
   - IVs derived solely from block counters
   - May not provide sufficient randomness for high-security

## Building and Testing

### Build Commands
```bash
# Build all components
make build

# Build shared components only
make shared/build
```

### Dependencies

- **OpenSSL**: Required for cryptographic operations
- **libcurl**: Required for Vault integration (fetching keys from remote server)

```bash
# Debian/Ubuntu
sudo apt install libssl-dev libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install openssl-devel libcurl-devel

# macOS
brew install openssl curl
```

## Future Work

- Key management integration
- Better IV generation
- Support any size by adding padding
- Key rotation support
- Support for additional ciphers and modes
- Parallel block processing
