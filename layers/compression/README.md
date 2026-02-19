# Compression Layer

The **compression layer** provides transparent data compression and decompression for the Modular IO Library. It supports multiple compression algorithms with configurable compression levels, offering a balance between *compression ratio* and *performance*.

## Overview

- **Transparent** compression and decompression on read/write operations
- **Multiple algorithms:** Supports LZ4 and ZSTD
- **Configurable compression levels**
- **Thread-safe:** Path-based locking
- **Original size tracking**
- **Layer-agnostic:** Works with any underlying storage
- **Currently only block-based mode is implemented**

---

## Supported Mode: sparse_block & Block Alignment (IMPORTANT)

⚠️ **Only the `sparse_block` mode is currently supported.** This means data is compressed and decompressed **per block of configurable size**. It will only provide compression efficiencies with file systems that support sparse files.

To work correctly, you MUST use the `block_align` layer directly above the compression layer, setting the **same `block_size`** in both. This ensures:
- Data passed to compression is always in full-size blocks
- Every block is compressed (and decompressed) independently

**Example TOML config:**
```toml
[block_align]
type = "block_align"
block_size = 262144
next = "compression"

[compression]
type = "compression"
algorithm = "lz4"  # or "zstd"
level = 1           # compression level (int)
mode = "sparse_block"
block_size = 262144
next = "underlying_layer"

[compression.options]
free_space = true
```

---

## Key Features

- **Algorithm support:** LZ4 (fast) and ZSTD (high ratio), tunable compression levels
- **Automatic operation:** Transparently compresses/decompresses blocks on all I/O
- **Size tracking:** Maintains original file/block sizes for accurate decompression
- **Thread safety:** Per-path locking and reader-writer locks
- **Memory efficiency:** Dynamically manages buffers for optimal usage

## Configuration

Define a compression layer in your `config.toml` as above. 

- `algorithm` (**required**): Compression algorithm, `lz4` or `zstd`
- **`level`** (integer): Compression level
  - **LZ4**: Levels 0-12 (0=default, 12=best compression)(Negative values enable fast acceleration mode)
  - **ZSTD**: Levels 0-22 (3=default, 22=best compression)(Negative values enable ultra-fast compression that prioritizes speed over compression ratio)
- `mode` (**required**): Always `sparse_block` (required)
- `block_size` (**required**): Must match above block_align
- `next` (**required**): Next layer
- `options`: 
    - `free_space`: Use `fallocate` (if available in the persistense layer) to punch holes in the file if the update to the block has a smaller size than before. This leads to space optimization, but may hurt the performance.

### Algorithm Characteristics
- `lz4`: Extremely fast, moderate ratio, low memory - ideal for real-time/data streaming
- `zstd`: Fast (but slower than LZ4), better compression, moderate memory - best for storage/bandwidth savings

### Mode Characteristics
- `sparse_block`: Each blocks starts in the respective offset in the file, leaving spaces between blocks if the block is compressable. In case of no compression gains, the block is stored uncompressed. Most file systems support sparse files, but make sure that your fs supports it. If not, no gains will come from compression.
---

## Architecture

### Compression Flow
```
Application Write → Compress Data (per block) → Store Compressed → Underlying Layer
Application Read  ← Decompress Block-by-Block ← Read Compressed ← Underlying Layer
```

### Core Components
- **Size Mapping:** Tracks original file sizes for decompression
- **Compressor Interface:** Pluggable algorithm support (easy future expansion)
- **Locking System:** Per-path & per-block thread safety
- **Buffer Management:** Allocates buffers dynamically for compression/decompression

---

## Operational Behavior

### Compression (Write Operations)
1. **Acquire WRITE lock** for file path
2. Compress the write buffer (per block) using the selected algorithm
3. Record the original size in the size hash table
4. Write compressed block to next layer
5. Release WRITE lock

### Decompression (Read Operations)
1. **Acquire READ lock** for file path
2. Read compressed block data from the next layer
3. Retrieve the original block size
4. Decompress to original size
5. Release READ lock

---

## Performance Considerations
- Compression adds some CPU overhead; using LZ4 gives maximum speed, ZSTD higher compression
- Matching `block_size` between `block_align` and compression is required for correctness and optimal performance
- Space savings vs. speed trade-off determined by level & algorithm

### Performance Guidelines
- Maximum speed: LZ4 with level 1
- Maximum compression: ZSTD with high level
- Balanced: ZSTD with level 6

### Storage Benefits
- Reduces on-disk and over-the-wire data size (if storage/fs supports sparse files)
- Decreases I/O and network usage

### pgbench performance
We have done three steps to test the performance of sparse block compression:
- run `initdb` script, and measure the disk space used
- run data init `pgbench -i -s 50`, and measure the disk space used
- run three times a workload of read/write queries for 5 minutes each run, collect the average transaction per second (TPS), and measure the disk space used in the end of the runs

- Platform: AWS EC2 m7i.xlarge (4 vCPUs, 16 GB RAM)
- OS: AlmaLinux 9.6 (Sage Margay)
- Database: PostgreSQL (v13.22) with pgbench benchmarking tool

With this, we've gathered this data, which provides the following comparison:

| Scenario            | Average TPS   | % Difference vs No Fuse (TPS) | Size (before; after init; after runs) | % Difference vs No Fuse (Space) |
|---------------------|---------------|--------------------------------|----------------------------------------|----------------------------------|
| No Fuse             | 3810.62 tps   | -                              | 43M; 1.4G; 2.9G                        | -                                |
| Fuse with local     | 3379 tps      | -12%                           | 43M; 1.4G; 2.9G                        | -                                |
| Fuse LZ4 256kb      | 2094.69 tps   | -45%                           | 13M; 421M; 577M                        | -70%; -70%; -81%                 |
| Fuse LZ4 512kb      | 1531.36 tps   | -60%                           | 11M; 272M; 436M                        | -75%; -81%; -85%                 |
| Fuse LZ4 256kb with fallocate       | 1276.46 tps   | -66%           | 13M; 421M; 541M                        | -80%; -94%; -81.7%               |
| Fuse LZ4 512kb with fallocate       | 1037.29 tps   | -72%           | 11M; 272M; 393M                        | -75%; -81%; -87%                 |

---

## Algorithm Comparison

| Algorithm | Speed | Compression Ratio | CPU Usage | Use Case |
|-----------|-------|-------------------|-----------|----------|
| **LZ4** | Very Fast | Moderate | Low | Performance-critical applications |
| **ZSTD** | Fast | High | Moderate | Storage-optimized applications |

#### LZ4
- **Speed:** Extremely fast
- **Ratio:** 2-3x typical for text data
- **Memory:** Low requirements
- **Best for:** Real-time apps, networks, fast access

#### ZSTD
- **Speed:** Fast (slower than LZ4)
- **Ratio:** 3-5x for text data
- **Memory:** Moderate requirements
- **Best for:** Backups, archives, bandwidth-constrained/cloud use

---

## Thread Safety

- **Path-based and reader-writer locks** prevent race conditions
- **Size mapping and buffer management** are thread-safe

---

## Error Handling

### Compression Errors
- Insufficient buffer? Automatic buffer resizing
- Algorithm init failure? Configuration validated at startup
- Compression failure? Falls back to storing uncompressed data

### Decompression Errors
- Invalid compressed data? Detect and report
- Size mismatch? Validates against mapping
- Memory failures? Reports and recovers safely
- Decompression failure? Returns standard error

### Recovery
- Loss of size mapping attempts a best-effort size estimation

---

## Building & Testing

```bash
make build        # Build all, includes compression layer
make tests/run    # Run compression tests
```

### Test Coverage
- Compression/decompression round-trips
- Each algorithm, compression level
- Error condition handling and reporting
- Thread safety validation under concurrency
- Performance and buffer efficiency

---

## Memory Management

- **Buffers sized dynamically** to fit block data
- **Reuse**: Compression buffers reused where possible
- **Cleanup:** Automatic cleanup on layer destruction
- **Size mapping:** Efficient hash table for block/file mapping, auto cleanup

---

## Troubleshooting
- High CPU usage? Lower level, use LZ4
- Memory? Monitor buffer/size-map use
- Size mismatches? Check mapping integrity
- Performance? Profile compression vs. I/O trade-off
- Debug logging: `log_mode = "debug"`

### Optimization Tips
- Match algorithm to use case
- Tune compression level for speed/space trade-off
- Monitor/tune buffer sizes for your workload
- Use block_align with matching size for reliability

---

## Modular Compressor Interface

- **Uniform interface** for algorithm selection at runtime
- **Extensible by design:** Add new compression algorithms easily
- **Current implementations:** LZ4 (via LZ4 library), ZSTD (Zstandard lib)
- **Pluggable for new methods** in future

---

This extensible design enables future growth (modes and algorithms) while delivering efficient, safe, and configurable transparent compression for any storage backend. 
