# Compression Layer Benchmark

**Warning: This script overwrites the config.toml file in the root of the project**

This script performs comprehensive benchmarking of the compression layer implementation using the Silesia Corpus dataset. It supports configurable layer stacks, multiple iterations for statistical accuracy, and automated visualization generation.

## Overview

The benchmark tests multiple compression algorithms and levels against real-world files to evaluate:
- **Compression ratios** - How much space is saved
- **Write performance** - Time to compress and store files  
- **Read performance** - Time to decompress and retrieve files
- **Statistical reliability** - Multiple iterations with averages and standard deviations
- **Visual analysis** - Automated plot generation for performance visualization

## Architecture Options

The benchmark supports **two layer-based architectures** and **two execution modes**:

### Layer Stack Options

#### Option 1: Compression-Only Stack (default)

``` 
Application Layer (benchmark script)
↓
FUSE Layer (passthrough filesystem)
↓
Compression Layer (LZ4/ZSTD algorithms)
↓
Local Layer (actual file storage)
```

This setup tests the **real-world performance** of the layered architecture as it would be used in production applications.

#### Option 2: Block Align + Compression Stack (`--block-split`)

``` 
Application Layer (benchmark script)
↓
FUSE Layer (passthrough filesystem)
↓
Block Align Layer (configurable block size)
↓
Compression Layer (LZ4/ZSTD algorithms in sparse_block mode)
↓
Local Layer (actual file storage)
```

This setup tests compression with a **Block Align layer** that:
- Aligns all I/O operations to fixed-size blocks
- Enables **sparse_block mode** in the compression layer
- Compresses data in block-sized chunks independently
- Allows random access and partial block updates
- Tests performance with different block sizes (64KB, 256KB, 1MB, etc.)

**Use this mode when**:
- Testing block-aligned I/O patterns (common in databases, VMs)
- Evaluating compression with specific block sizes
- Benchmarking sparse block compression strategies
- Comparing block size impact on compression ratio and performance

### Execution Modes

#### FUSE Mode (default)

Uses FUSE (Filesystem in Userspace) to mount the layer stack as a virtual filesystem. Files are accessed through the mount point, providing realistic filesystem-level testing.

#### LD_PRELOAD Mode

Uses `LD_PRELOAD` to intercept POSIX file operations directly without FUSE overhead. This mode:
- **Faster startup** - No FUSE mounting required
- **No sudo required** - Direct library interception
- **Lower overhead** - Eliminates FUSE layer processing
- **Two variants**:
  - **Split mode** (`--ld-preload`): Write and read operations in separate contexts
  - **Single mode** (`--ld-preload-single`): Write and read in a single execution context

## Dataset Information

The Silesia Corpus is maintained by the data compression research community and provides standardized test files for compression algorithm evaluation. The files represent diverse data types commonly found in real-world applications, making them ideal for benchmarking compression performance across different content types.

## Prerequisites

**Required**: Download the Silesia Corpus files manually before running the benchmark.

### Download Instructions

1. **Find**: A reliable source of the Silesia Corpus files (https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia or https://github.com/MiloszKrajewski/SilesiaCorpus?tab=readme-ov-file might be useful, please check reliability before downloading)
2. **Extract** the uncompressed files to: `datasets/silesia/`
3. **Ensure** these files exist:
   - `datasets/silesia/dickens`
   - `datasets/silesia/mozilla`
   - `datasets/silesia/mr`
   - `datasets/silesia/nci`
   - `datasets/silesia/xml`

### Required Files

Ensure these files exist in `datasets/silesia/`:
- **dickens** (10.2MB) - English literature text
- **mozilla** (51.2MB) - Executable binary archive  
- **mr** (10.0MB) - Medical resonance image
- **nci** (33.6MB) - Chemical database
- **xml** (5.3MB) - XML document collection

## Usage

### Basic Usage (Compression-Only)

```bash
# Run from project root (after downloading Silesia files)
# Default: FUSE mode, compression-only stack
./scripts/compression/compression_benchmark.sh

# Quick benchmark with LD_PRELOAD (no FUSE, faster)
./scripts/compression/compression_benchmark.sh --ld-preload
```

### Block Split Mode

Test compression with block-aligned I/O:

```bash
# Enable block align layer (default 256KB blocks)
./scripts/compression/compression_benchmark.sh --block-split

# Custom block size: 512KB blocks
./scripts/compression/compression_benchmark.sh -b --block-size 524288

# Small blocks: 64KB for fine-grained compression
./scripts/compression/compression_benchmark.sh -b --block-size 65536

# Large blocks: 1MB for better compression ratios
./scripts/compression/compression_benchmark.sh -b --block-size 1048576

# Combined: block split with multiple iterations and LD_PRELOAD
./scripts/compression/compression_benchmark.sh -b --block-size 262144 --ld-preload -i 5
```

**When to use `--block-split`**:
- Testing database-style random access workloads
- Evaluating impact of different block sizes on compression
- Benchmarking VM disk image compression
- Comparing sparse_block mode vs standard compression

### Choosing Execution Mode

**Use FUSE Mode (default)** when:
- Testing realistic filesystem-level performance
- Evaluating FUSE overhead impact
- Testing with applications that require actual filesystem access

**Use LD_PRELOAD Mode** (`--ld-preload`) when:
- Quick benchmarking without sudo
- Focusing on layer performance without FUSE overhead
- Running on systems where FUSE is unavailable
- Testing raw library performance

### Command Line Options

- `-b, --block-split`: Enable block align layer before compression (changes to sparse_block mode)
- `--block-size <bytes>`: Block size in bytes when using `--block-split` (default: 262144 = 256 KiB)
- `-i, --iterations <num>`: Number of test iterations (default: 1)
- `-l, --ld-preload`: Use LD_PRELOAD mode instead of FUSE (split write/read)
- `--ld-preload-single`: Use LD_PRELOAD mode with single execution context
- `-h, --help`: Show help message

### Understanding `--block-split`

**Without `--block-split` (default)**:
- Files are compressed as a whole
- Layer stack: Compression → Local Storage
- Standard compression mode
- Best for sequential access patterns

**With `--block-split`**:
- Files are split into fixed-size blocks before compression
- Layer stack: Block Align → Compression (sparse_block mode) → Local Storage
- Each block is compressed independently
- Blocks can be accessed/modified individually
- Best for random access patterns (databases, VM images)
- Block size is configurable via `--block-size`

**Trade-offs**:
- ✅ **Pros**: Random access support, individual block updates, parallelizable compression
- ⚠️ **Cons**: Potentially lower compression ratios (smaller units), more metadata overhead

### Examples

```bash
# Test compression-only stack with 5 iterations
./scripts/compression/compression_benchmark.sh -i 5

# Test block split (128 KiB blocks) + compression with 3 iterations  
./scripts/compression/compression_benchmark.sh -b --block-size 131072 -i 3

# Test block split (1 MiB blocks) + compression, single iteration
./scripts/compression/compression_benchmark.sh -b --block-size 1048576

# Use LD_PRELOAD mode (no FUSE, faster startup)
./scripts/compression/compression_benchmark.sh --ld-preload -i 5

# LD_PRELOAD single context mode
./scripts/compression/compression_benchmark.sh --ld-preload-single -i 3

# Block split with LD_PRELOAD mode
./scripts/compression/compression_benchmark.sh -b --block-size 262144 --ld-preload -i 5
```

## File Verification

The script automatically verifies that all required Silesia Corpus files are present in `datasets/silesia/` before starting the benchmark. If any files are missing, it will display clear instructions on what to download.

## What It Tests

### Compression Algorithms

The script tests **7 different configurations**:

1. **No Compression** - Direct local storage (baseline)
2. **LZ4 Ultra Fast** (level -3) - Fastest standard compression
3. **LZ4 Default** (level 0) - Balanced LZ4 compression  
4. **LZ4 High** (level 9) - Best LZ4 compression ratio
5. **ZSTD Ultra Fast** (level -5) - Fastest ZSTD compression
6. **ZSTD Default** (level 3) - Balanced ZSTD compression
7. **ZSTD High** (level 15) - High ZSTD compression ratio

### Test Files

Uses files from the **[Silesia Corpus](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia)** with different characteristics:
- **dickens** (10MB) - English literature text (highly compressible)
- **mozilla** (49MB) - Executable binary (low compressibility)
- **xml** (5MB) - Structured XML data (medium compressible)
- **mr** (10MB) - Medical images (mixed compressibility)
- **nci** (32MB) - Database content (varies)

## How It Works

### Sequential Testing Architecture

To avoid resource contention and ensure accurate measurements:

1. **Compile C helper** (`io_bench.c`) for precise timing measurements
2. **Verify Silesia Corpus** files are present
3. **Generate configurations** based on layer stack choice
4. **Start single FUSE instance** for one compression configuration (or configure LD_PRELOAD)
5. **Test all files** on that instance using the C helper binary (multiple iterations if specified)
6. **Calculate statistics** (averages, standard deviations)
7. **Stop and cleanup** the instance (FUSE mode only)
8. **Repeat** for next configuration

This approach prevents:
- CPU contention between multiple compression processes
- Memory pressure from running multiple FUSE instances simultaneously
- I/O bottlenecks from concurrent disk operations

### C Helper Binary (`io_bench`)

The benchmark uses a dedicated C helper program for accurate timing:
- **Minimal overhead**: Direct POSIX I/O calls without shell overhead
- **Precise timing**: Measures only file I/O operations
- **Three modes**:
  - `write`: Write file and measure time
  - `read`: Read file and measure time
  - `full`: Write and read in single execution (LD_PRELOAD single mode)
- **Automatic compilation**: Built automatically before benchmark runs

### Performance Measurement

For each file, configuration, and iteration:

1. **Setup**: Mount FUSE filesystem (or configure LD_PRELOAD) with specified layer stack
2. **Write test**: Stream file through layers using C helper binary, measure write time
3. **Read test**: Stream file back, measure read time
4. **Size analysis**: 
   - Uses `du --block-size=1` to get **actual disk usage** (handles sparse files correctly)
   - Compares original vs compressed/stored file sizes
   - Calculates compression ratios based on real allocated space
5. **Statistical analysis**: Calculate averages and standard deviations across iterations
6. **Cleanup**: Unmount and prepare for next test (FUSE mode only)

### Multiple Iterations & Statistics

When running multiple iterations (`-i <num>`):
- Each test file is written and read `<num>` times per configuration
- Results are averaged to reduce measurement noise
- Standard deviations are calculated to show result reliability
- More iterations provide more accurate performance measurements

### Accurate Size Measurement

The benchmark uses `du` (disk usage) utilities to measure **actual allocated space** on disk:
- **Handles sparse files correctly**: Measures real disk blocks, not apparent file size
- **Accurate compression ratios**: Reflects true storage savings after compression
- **Block-level precision**: Uses `du --block-size=1` for byte-accurate measurements
- **Human-readable output**: Also provides `du -h` for easy interpretation

This is important because compressed files may appear larger or smaller than their actual disk usage depending on filesystem block allocation and sparse file handling.

## Output

### Results Directory Structure

Results are stored in timestamped directories to prevent overwrites:

```
benchmark_results/compression_layer/
├── dickens_no_compression.results
├── dickens_lz4_ultra_fast.results
├── dickens_lz4_default.results
├── dickens_lz4_high.results
├── dickens_zstd_ultra_fast.results
├── dickens_zstd_default.results
├── dickens_zstd_high.results
└── ... (similar for each file)
```

For block split configurations:
```
benchmark_results/
└── blocksplit-262144b-compression-local-3iterations_20250828_142301/
    ├── benchmark_results.csv
    ├── benchmark_report.md
    ├── plots/
    └── *.results
```

### CSV Results File

The `benchmark_results.csv` contains detailed data for analysis:

| Column | Description |
|--------|-------------|
| File | Test file name |
| Config | Compression algorithm |
| Block_Size_Bytes | Block size (if using block split) |
| Iterations | Number of test iterations |
| Original_Size_MB | Original file size in MB |
| Stored_Size_MB | Compressed file size in MB |
| Compression_Ratio | Original size / stored size |
| Write_Time_s | Average write time in seconds |
| Read_Time_s | Average read time in seconds |
| Write_Std_s | Standard deviation of write times |
| Read_Std_s | Standard deviation of read times |

### Benchmark Report

Consolidated markdown report: `benchmark_report.md`

Contains:
- Configuration summary (layer stack, block size, iterations)
- Results table with averages and standard deviations
- Compression ratios for each algorithm/file combination
- Performance metrics with statistical confidence
- Algorithm recommendations

### Visualization Plots

Automated plot generation using `plot_results.py`:

#### Individual File Plots
- **Write time vs compression ratio** for each file
- **Read time vs compression ratio** for each file
- Shows trade-offs between compression efficiency and performance

#### Summary Plots
- **All files combined** - write time vs compression ratio
- **All files combined** - read time vs compression ratio
- Color-coded by file type, shaped by compression algorithm
- Dual legends for easy interpretation

#### Plot Features
- **Raw execution times** (seconds) on x-axis
- **Compression ratios** on y-axis
- **Algorithm identification** via colors and shapes
- **High-resolution PNG** output (300 DPI)

## Visualization

### Python Plotting Script

Generate performance visualization plots:

```bash
# After running benchmark
python3 scripts/compression/plot_results.py benchmark_results.csv
```

#### Generated Plots
- **Scatter plots**: Compression ratio vs execution time
- **Per-file analysis**: Individual plots for each test file
- **Summary plots**: All files combined for comparison
- **Dual legends**: File types (colors) and algorithms (shapes)

## Sample Results

| File | Config | Original Size | Stored Size | Compression Ratio | Write Time (s) | Read Time (s) |
|------|--------|---------------|-------------|-------------------|----------------|---------------|
| mozilla | no_compression | 48.8MB | 48.8MB | 1.00:1 | 0.15 | 0.11 |
| mozilla | lz4_default | 48.8MB | 25.2MB | 1.93:1 | 21.8 | 2.97 |
| mozilla | zstd_default | 48.8MB | 17.3MB | 2.81:1 | 185.1 | 16.8 |

## Requirements

- **Build tools**: `make`, `gcc`
- **FUSE support**: `/dev/fuse` device available (only required for FUSE mode)
- **Sudo access**: Required for FUSE mounting operations (not needed for LD_PRELOAD mode)
- **Silesia Corpus**: Manual download required from [official source](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia) or [Milosz Krajewski mirror](https://github.com/MiloszKrajewski/SilesiaCorpus?tab=readme-ov-file). Please check reliability before downloading.
- **Python (for plots)**: `python3` with `pandas`, `matplotlib`, `numpy`
- **Disk space**: 
  - ~200MB for Silesia Corpus files
  - ~500MB additional for benchmark results and temporary files
- **Memory**: ~1GB recommended for larger files

### Mode-Specific Requirements

**FUSE Mode (default)**:
- FUSE device (`/dev/fuse`)
- Root/sudo permissions for mounting
- FUSE libraries installed

**LD_PRELOAD Mode** (`--ld-preload`):
- No FUSE required
- No sudo required
- Direct library interception via `LD_PRELOAD`

## Performance Notes

- **Sequential execution** prevents resource contention
- **Automatic cleanup** handles interrupted benchmarks
- **Timestamped results** prevent data overwrites
- **Sync operations** ensure accurate timing measurements
- **Statistical iterations** improve measurement reliability
- **Decompression cache** dramatically improves read performance
- **Block splitting** may improve or hurt performance depending on file characteristics
- **Manual file setup** - Download Silesia files once, use repeatedly
- **Sparse file handling** - Uses `du` to measure actual disk usage, correctly handling sparse files and compression
- **LD_PRELOAD mode** - Faster execution without FUSE overhead, ideal for quick benchmarks
