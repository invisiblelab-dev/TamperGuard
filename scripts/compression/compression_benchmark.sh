#!/bin/bash

# Simple Compression Benchmark with Silesia Corpus
# Usage: ./scripts/compression_benchmark.sh

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SILESIA_DIR="$PROJECT_ROOT/datasets/silesia"

# Default to compression_only, flag switches to block_split
LAYER_CONFIG="compression_only"
# Default block size for block_align + compression (sparse_block)
BLOCK_SIZE="262144"  # 256KiB default

ITERATIONS="1"  # Default to 1 iteration
USE_LD_PRELOAD="0"  # Default: FUSE mode
LD_PRELOAD_SINGLE="0"  # Currently behaves the same as normal LD_PRELOAD mode
WRAPPER_PATH=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--block-split)
            LAYER_CONFIG="block_split_compression"
            shift
            ;;
        -l|--ld-preload)
            USE_LD_PRELOAD="1"
            shift
            ;;
        --ld-preload-single)
            USE_LD_PRELOAD="1"
            LD_PRELOAD_SINGLE="1"
            shift
            ;;
        --block-size)
            BLOCK_SIZE="$2"
            shift 2
            ;;
        --iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        -i)
            ITERATIONS="$2"
            shift 2
            ;;
        -h|--help)
            echo "This script tests the compression layer by mounting a FUSE filesystem with real-world files from the Silesia Corpus."
            echo "By default, the script will test a compression->local layer configuration."
            echo "Read the README.md file for more information on the benchmark."
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -b, --block-split           Enable block align + compression (sparse_block) layer"
            echo "  --block-size SIZE           Block size in bytes (default: 262144)"
            echo "  -l, --ld-preload            Enable LD_PRELOAD mode (no FUSE)"
            echo "      --ld-preload-single     Run write+read inside a single LD_PRELOAD context"
            echo "  -i, --iterations NUM        Number of test iterations (default: 1)"
            echo "  -h, --help                  Show this help message"
            echo ""
            echo "Examples:"
            echo "  --iterations 5              Run each test 5 times"
            echo "  -i 10                       Run each test 10 times"
            echo "  --ld-preload                Use LD_PRELOAD wrapper instead of FUSE"
            echo ""
            echo "Block size examples:"
            echo "  --block-size 65536          64KiB blocks"
            echo "  --block-size 262144         256KiB blocks (default)"
            echo "  --block-size 1048576        1MiB blocks"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Configuration mapping
case "$LAYER_CONFIG" in
    "compression_only")
        CONFIG_FILE="compression_only"
        RESULTS_SUBFOLDER="compression-local-${ITERATIONS}iterations"
        LAYER_DESCRIPTION="Compression → Local"
        ;;
    "block_split_compression")
        RESULTS_SUBFOLDER="blockalign-${BLOCK_SIZE}b-compression-local-${ITERATIONS}iterations"
        LAYER_DESCRIPTION="Block Align → Compression (sparse_block) → Local with block size ${BLOCK_SIZE} bytes"
        ;;
    *)
        error "Unknown layer configuration: $LAYER_CONFIG. Use 'compression_only' or 'block_split_compression'"
        ;;
esac

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_DIR="$PROJECT_ROOT/benchmark_results/${TIMESTAMP}_${RESULTS_SUBFOLDER}"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() { echo -e "${GREEN}[$(date +'%H:%M:%S')]${NC} $1"; }
info() { echo -e "${BLUE}[INFO]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1" >&2; exit 1; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1" >&2; }

# Global FUSE mount points and PIDs
NO_COMPRESSION_MOUNT="$RESULTS_DIR/no_compression_mount"
LZ4_ULTRA_FAST_MOUNT="$RESULTS_DIR/lz4_ultra_fast_mount"
LZ4_DEFAULT_MOUNT="$RESULTS_DIR/lz4_default_mount"
LZ4_HIGH_MOUNT="$RESULTS_DIR/lz4_high_mount"
ZSTD_ULTRA_FAST_MOUNT="$RESULTS_DIR/zstd_ultra_fast_mount"
ZSTD_DEFAULT_MOUNT="$RESULTS_DIR/zstd_default_mount"
ZSTD_HIGH_MOUNT="$RESULTS_DIR/zstd_high_mount"

NO_COMPRESSION_BACKEND="$RESULTS_DIR/no_compression_backend"
LZ4_ULTRA_FAST_BACKEND="$RESULTS_DIR/lz4_ultra_fast_backend"
LZ4_DEFAULT_BACKEND="$RESULTS_DIR/lz4_default_backend"
LZ4_HIGH_BACKEND="$RESULTS_DIR/lz4_high_backend"
ZSTD_ULTRA_FAST_BACKEND="$RESULTS_DIR/zstd_ultra_fast_backend"
ZSTD_DEFAULT_BACKEND="$RESULTS_DIR/zstd_default_backend"
ZSTD_HIGH_BACKEND="$RESULTS_DIR/zstd_high_backend"

NO_COMPRESSION_PID=""
LZ4_ULTRA_FAST_PID=""
LZ4_DEFAULT_PID=""
LZ4_HIGH_PID=""
ZSTD_ULTRA_FAST_PID=""
ZSTD_DEFAULT_PID=""
ZSTD_HIGH_PID=""

# Cleanup function
cleanup_fuse() {
    log "Cleanup of any remaining FUSE instance initiated by the script..."
    
    # Only kill processes matching our specific binary and result directory
    sudo pkill -f "passthrough.*${RESULTS_DIR}" 2>/dev/null || true
    
    # Clean up our PID files
    rm -f /tmp/fuse_*_pid 2>/dev/null || true
    
    # Force unmount any of our mount points that might be stuck
    local mounts=("$NO_COMPRESSION_MOUNT" "$LZ4_ULTRA_FAST_MOUNT" "$LZ4_DEFAULT_MOUNT" "$LZ4_HIGH_MOUNT" "$ZSTD_ULTRA_FAST_MOUNT" "$ZSTD_DEFAULT_MOUNT" "$ZSTD_HIGH_MOUNT")
    for mount in "${mounts[@]}"; do
        if mountpoint -q "$mount" 2>/dev/null; then
            warn "Force unmounting stuck mount: $mount"
            sudo umount -f "$mount" 2>/dev/null || sudo fusermount3 -u "$mount" 2>/dev/null || true
        fi
    done
    
    # Clean up directories
    rm -rf "$NO_COMPRESSION_BACKEND" "$LZ4_ULTRA_FAST_BACKEND" "$LZ4_DEFAULT_BACKEND" "$LZ4_HIGH_BACKEND" "$ZSTD_ULTRA_FAST_BACKEND" "$ZSTD_DEFAULT_BACKEND" "$ZSTD_HIGH_BACKEND"
    rm -rf "$NO_COMPRESSION_MOUNT" "$LZ4_ULTRA_FAST_MOUNT" "$LZ4_DEFAULT_MOUNT" "$LZ4_HIGH_MOUNT" "$ZSTD_ULTRA_FAST_MOUNT" "$ZSTD_DEFAULT_MOUNT" "$ZSTD_HIGH_MOUNT"
    
    log "✅ Cleanup of any remaining FUSE instance completed."
}

# Set trap for cleanup
trap cleanup_fuse EXIT INT TERM

# Check Silesia corpus availability
check_silesia() {
    log "Checking Silesia Corpus availability..."
    
    # Create directory if it doesn't exist
    mkdir -p "$SILESIA_DIR"
    
    local required_files=("dickens" "mozilla" "mr" "nci" "xml")
    local missing_files=()
    local corrupted_files=()
    
    # Expected sizes in bytes (from official Silesia Corpus)
    declare -A expected_sizes=(
        ["dickens"]="10192446"
        ["mozilla"]="51220480"
        ["mr"]="9970564"
        ["nci"]="33553445"
        ["xml"]="5345280"
    )
    
    # Check each required file
    for file in "${required_files[@]}"; do
        if [ ! -f "$SILESIA_DIR/$file" ]; then
            missing_files+=("$file")
        else
            # Check file size
            actual_size=$(stat -c%s "$SILESIA_DIR/$file" 2>/dev/null)
            expected_size=${expected_sizes[$file]}
            
            if [ "$actual_size" != "$expected_size" ]; then
                corrupted_files+=("$file (expected: $(numfmt --to=iec $expected_size), actual: $(numfmt --to=iec $actual_size))")
            fi
        fi
    done
    
    # Report missing files
    if [ ${#missing_files[@]} -gt 0 ]; then
        error "Missing Silesia files: ${missing_files[*]}
        
Please download the Silesia Corpus files manually:
1. Find: A reliable source of the Silesia Corpus files (https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia or https://github.com/MiloszKrajewski/SilesiaCorpus?tab=readme-ov-file might be useful, please check reliability before downloading)
2. Extract uncompressed files to: $SILESIA_DIR/
3. Ensure these files exist:
   - $SILESIA_DIR/dickens (10.2MB)
   - $SILESIA_DIR/mozilla (51.2MB) 
   - $SILESIA_DIR/mr (9.5MB)
   - $SILESIA_DIR/nci (33.6MB)
   - $SILESIA_DIR/xml (5.3MB)"
    fi
    
    # Report corrupted files
    if [ ${#corrupted_files[@]} -gt 0 ]; then
        error "Corrupted Silesia files detected:
$(printf '  - %s\n' "${corrupted_files[@]}")

These files have incorrect sizes. Please ensure they are the official Silesia Corpus files.

Expected sizes:
  - dickens: 10.2MB (10,192,446 bytes)
  - mozilla: 51.2MB (51,220,480 bytes)
  - mr: 9.5MB (9,970,564 bytes)  
  - nci: 33.6MB (33,553,445 bytes)
  - xml: 5.3MB (5,345,280 bytes)"
    fi
    
    log "✅ Silesia Corpus ready! Available files:"
    for file in "${required_files[@]}"; do
        local size=$(ls -lh "$SILESIA_DIR/$file" | awk '{print $5}')
        echo "  - $file: $size ✓"
    done
}

create_config_file() {
    local name="$1"
    local algorithm="$2" 
    local level="$3"
    
    if [[ "$LAYER_CONFIG" == "block_split_compression" ]]; then
        cat > "$RESULTS_DIR/${name}.toml" <<EOF
root = "block_align_layer"
log_mode = "disabled"

[block_align_layer]
type = "block_align"
block_size = $BLOCK_SIZE
next = "compression_layer"

[compression_layer]
type = "compression"
algorithm = "$algorithm"
level = $level
mode = "sparse_block"
block_size = $BLOCK_SIZE
next = "local_layer"

[local_layer]
type = "local"
EOF
    else
        cat > "$RESULTS_DIR/${name}.toml" <<EOF
root = "compression_layer"
log_mode = "disabled"

[compression_layer]
type = "compression"
next = "local_layer"
algorithm = "$algorithm"
level = $level

[local_layer]
type = "local"
EOF
    fi
}

create_configs() {
    # No compression (baseline - always just local layer)
    cat > "$RESULTS_DIR/no_compression.toml" <<EOF
root = "local_layer"
log_mode = "disabled"

[local_layer]
type = "local"
EOF
    
    # Generate all compression configs
    create_config_file "lz4_ultra_fast" "lz4" "-3"
    create_config_file "lz4_default" "lz4" "0"
    create_config_file "lz4_high" "lz4" "9"
    create_config_file "zstd_ultra_fast" "zstd" "-5"
    create_config_file "zstd_default" "zstd" "3"
    create_config_file "zstd_high" "zstd" "15"
}

# Compile lib_wrapper.so for LD_PRELOAD mode
compile_wrapper() {
    log "Compiling lib_wrapper.so for LD_PRELOAD mode..."
    cd "$PROJECT_ROOT"
    if [ ! -f "lib_wrapper.c" ]; then
        error "lib_wrapper.c not found in project root"
    fi
    gcc -fPIC -shared $(pkg-config --cflags --libs glib-2.0) -I. lib_wrapper.c \
        -L"$PROJECT_ROOT/build/lib" -lmodular -Wl,-rpath="$PROJECT_ROOT/build/lib" \
        -Wl,--version-script="$PROJECT_ROOT/lib_wrapper.map" -o lib_wrapper.so
    if [ ! -f "lib_wrapper.so" ]; then
        error "Failed to compile lib_wrapper.so"
    fi
    WRAPPER_PATH="$PROJECT_ROOT/lib_wrapper.so"
    log "✅ lib_wrapper.so compiled successfully"
}

# Path to C IO benchmark binary
IO_BENCH_BIN="$PROJECT_ROOT/scripts/compression/io_bench"

# Compile C IO benchmark helper
compile_io_bench() {
    log "Compiling io_bench helper..."
    cd "$PROJECT_ROOT"
    gcc -O2 -o "$IO_BENCH_BIN" "scripts/compression/io_bench.c" || error "Failed to compile io_bench.c"
    if [ ! -x "$IO_BENCH_BIN" ]; then
        error "io_bench binary not found after compile: $IO_BENCH_BIN"
    fi
    log "✅ io_bench compiled successfully"
}


# Simplified test function (no FUSE setup/teardown)
run_test() {
    local config_name="$1"
    local mount_dir="$2"
    local backend_dir="$3"
    local test_file="$4"
    
    info "Testing: $config_name with $(basename "$test_file") ($ITERATIONS iterations)"
    
    # Arrays to store all iteration results
    local write_times=()
    local read_times=()
    local compression_ratios=()
    local original_size=0
    local stored_size=0
    
    # Run multiple iterations
    for ((iter=1; iter<=ITERATIONS; iter++)); do
        info "  Iteration $iter/$ITERATIONS"
        
        # Clean any previous test files
        rm -f "$backend_dir"/* 2>/dev/null || true
        rm -f "$mount_dir"/test_file_* 2>/dev/null || true
        
        local target_file="$mount_dir/test_file_$(basename "$test_file")_$$_iter_$iter"
        
        if [ "$USE_LD_PRELOAD" = "1" ]; then
            # LD_PRELOAD mode
            if [ "$LD_PRELOAD_SINGLE" = "1" ]; then
                # SINGLE mode: run one C helper under LD_PRELOAD that does write+read
                local output
                local exit_code
                output=$(LD_PRELOAD="$WRAPPER_PATH" "$IO_BENCH_BIN" full "$test_file" "$target_file" 2>&1) || exit_code=$?
                exit_code=${exit_code:-0}
                if [ $exit_code -ne 0 ]; then
                    error "❌ io_bench full failed for $config_name on $(basename "$test_file"): $output"
                fi
                local write_time=$(echo "$output" | grep 'WRITE_TIME_SEC:' | cut -d: -f2)
                local read_time=$(echo "$output" | grep 'READ_TIME_SEC:' | cut -d: -f2)
                if [ -z "$write_time" ] || [ -z "$read_time" ]; then
                    error "❌ Failed to parse io_bench full output: $output"
                fi
                write_times+=("$write_time")
                read_times+=("$read_time")
            else
                # SPLIT mode: run write and read separately under LD_PRELOAD
                local output
                local exit_code
                # Write
                output=$(LD_PRELOAD="$WRAPPER_PATH" "$IO_BENCH_BIN" write "$test_file" "$target_file" 2>&1) || exit_code=$?
                exit_code=${exit_code:-0}
                if [ $exit_code -ne 0 ]; then
                    error "❌ io_bench write failed for $config_name on $(basename "$test_file"): $output"
                fi
                local write_time=$(echo "$output" | grep 'WRITE_TIME_SEC:' | cut -d: -f2)
                if [ -z "$write_time" ]; then
                    error "❌ Failed to parse io_bench write output: $output"
                fi
                write_times+=("$write_time")
                # Read
                output=$(LD_PRELOAD="$WRAPPER_PATH" "$IO_BENCH_BIN" read "$target_file" 2>&1) || exit_code=$?
                exit_code=${exit_code:-0}
                if [ $exit_code -ne 0 ]; then
                    error "❌ io_bench read failed for $config_name on $(basename "$test_file"): $output"
                fi
                local read_time=$(echo "$output" | grep 'READ_TIME_SEC:' | cut -d: -f2)
                if [ -z "$read_time" ]; then
                    error "❌ Failed to parse io_bench read output: $output"
                fi
                read_times+=("$read_time")
            fi
        else
            # FUSE mode: use the C IO helper (no LD_PRELOAD)
            local output
            local exit_code
            output=$("$IO_BENCH_BIN" full "$test_file" "$target_file" 2>&1) || exit_code=$?
            exit_code=${exit_code:-0}
            if [ $exit_code -ne 0 ]; then
                error "❌ io_bench full (FUSE) failed for $config_name on $(basename "$test_file"): $output"
            fi
            local write_time=$(echo "$output" | grep 'WRITE_TIME_SEC:' | cut -d: -f2)
            local read_time=$(echo "$output" | grep 'READ_TIME_SEC:' | cut -d: -f2)
            if [ -z "$write_time" ] || [ -z "$read_time" ]; then
                error "❌ Failed to parse io_bench full (FUSE) output: $output"
            fi
            write_times+=("$write_time")
            read_times+=("$read_time")
        fi
        
        original_size=$(stat -c%s "$test_file")
        
        # Calculate compression ratio for this iteration
        sync
        stored_size=0
        stored_size_human="0"
        target_file="$mount_dir/test_file_$(basename "$test_file")_$$_iter_$iter"
        if [ "$USE_LD_PRELOAD" = "1" ]; then
            if [ -f "$target_file" ]; then
                stored_size=$(du --block-size=1 "$target_file" 2>/dev/null | awk '{print $1}')
                stored_size=${stored_size:-0}
                stored_size_human=$(du -h "$target_file" 2>/dev/null | awk '{print $1}')
                stored_size_human=${stored_size_human:-0}
            fi
        else
            if [ -d "$backend_dir" ]; then
                stored_size=$(du --block-size=1 "$backend_dir" 2>/dev/null | awk '{print $1}')
                stored_size=${stored_size:-0}
                stored_size_human=$(du -h "$backend_dir" 2>/dev/null | awk '{print $1}')
                stored_size_human=${stored_size_human:-0}
            fi
        fi
        info "    Stored size on disk: ${stored_size_human}"
        
        local compression_ratio="1.00"
        if [ "$stored_size" -gt 0 ]; then
            compression_ratio=$(echo "scale=4; $original_size / $stored_size" | bc -l)
        fi
        compression_ratios+=("$compression_ratio")
    done
    
    # Calculate averages and standard deviations
    local avg_write_time=$(printf '%s\n' "${write_times[@]}" | awk '{sum+=$1} END {print sum/NR}')
    local avg_read_time=$(printf '%s\n' "${read_times[@]}" | awk '{sum+=$1} END {print sum/NR}')
    local avg_compression_ratio=$(printf '%s\n' "${compression_ratios[@]}" | awk '{sum+=$1} END {print sum/NR}')
    
    # Calculate standard deviations 
    local write_std=$(printf '%s\n' "${write_times[@]}" | awk -v avg="$avg_write_time" '{sum+=($1-avg)^2} END {print sqrt(sum/NR)}')
    local read_std=$(printf '%s\n' "${read_times[@]}" | awk -v avg="$avg_read_time" '{sum+=($1-avg)^2} END {print sqrt(sum/NR)}')
    
    # Save primary results file (averages)
    cat > "$RESULTS_DIR/$(basename "$test_file")_${config_name}.results" <<EOF
config: $config_name
file: $(basename "$test_file")
iterations: $ITERATIONS
original_size: $original_size
stored_size: $stored_size
compression_ratio: $avg_compression_ratio
write_time: $avg_write_time
read_time: $avg_read_time
write_time_std: $write_std
read_time_std: $read_std
EOF

    info "✅ $config_name with $(basename "$test_file") completed (avg write: ${avg_write_time}s ±${write_std}s, avg read: ${avg_read_time}s ±${read_std}s)"
}

# Generate simple report
generate_report() {
    local report="$RESULTS_DIR/benchmark_report.md"
    local csv_file="$RESULTS_DIR/benchmark_results.csv"
    
    log "📊 Generating reports..."
    
    # Create CSV file with headers
    cat > "$csv_file" <<EOF
File,Config,Block_Size_Bytes,Iterations,Original_Size_MB,Stored_Size_MB,Compression_Ratio,Write_Time_s,Read_Time_s,Write_Std_s,Read_Std_s,Original_Size_Bytes,Stored_Size_Bytes
EOF
    
    # Create markdown report
    cat > "$report" <<EOF
# Compression Benchmark Results

**Configuration**: $LAYER_DESCRIPTION  
**Date**: $(date)  
**Layer Configuration**: $LAYER_CONFIG  
$(if [[ "$LAYER_CONFIG" == "block_split_compression" ]]; then echo "**Block Size**: $BLOCK_SIZE bytes"; fi)

## Results Table

| File | Config | Original Size | Stored Size | Compression Ratio | Write Time (s) | Read Time (s) |
|------|--------|---------------|-------------|-------------------|----------------|---------------|
EOF

    # Process results and add to both files
    for result_file in "$RESULTS_DIR"/*.results; do
        if [ -f "$result_file" ]; then
            local config=$(grep "config:" "$result_file" | cut -d' ' -f2)
            local file=$(grep "file:" "$result_file" | cut -d' ' -f2)
            local orig_size=$(grep "original_size:" "$result_file" | cut -d' ' -f2)
            local stored_size=$(grep "stored_size:" "$result_file" | cut -d' ' -f2)
            local ratio=$(grep "compression_ratio:" "$result_file" | cut -d' ' -f2)
            local write_time=$(grep "write_time:" "$result_file" | cut -d' ' -f2)
            local read_time=$(grep "read_time:" "$result_file" | cut -d' ' -f2)
            
            # Extract std values (must be present)
            local write_std=$(grep "write_time_std:" "$result_file" | cut -d' ' -f2)
            local read_std=$(grep "read_time_std:" "$result_file" | cut -d' ' -f2)

            if [ -z "$write_std" ] || [ -z "$read_std" ]; then
                error "Missing standard deviation data in results file: $result_file"
            fi
            
            # Format sizes for display
            local orig_mb=$(echo "scale=1; $orig_size / 1024 / 1024" | bc -l)
            local stored_mb=$(echo "scale=1; $stored_size / 1024 / 1024" | bc -l)
            
            # Determine block size for this result
            local block_size_value=""
            if [[ "$LAYER_CONFIG" == "block_split_compression" ]]; then
                block_size_value="$BLOCK_SIZE"
            else
                block_size_value="N/A"
            fi
            
            # Add to CSV (with block size)
            echo "$file,$config,$block_size_value,$ITERATIONS,$orig_mb,$stored_mb,$ratio,$write_time,$read_time,$write_std,$read_std,$orig_size,$stored_size" >> "$csv_file"
            
            # Add to markdown table (optionally include block size)
            if [[ "$LAYER_CONFIG" == "block_split_compression" ]]; then
                echo "| $file | $config (${BLOCK_SIZE}B) | ${orig_mb}MB | ${stored_mb}MB | ${ratio}:1 | $write_time | $read_time |" >> "$report"
            else
                echo "| $file | $config | ${orig_mb}MB | ${stored_mb}MB | ${ratio}:1 | $write_time | $read_time |" >> "$report"
            fi
        fi
    done
    
    cat >> "$report" <<EOF

## Summary

**CSV Data**: Available in \`benchmark_results.csv\`  
**Raw Results**: Individual .results files in this directory

### Configuration Details
- **Layer Stack**: $LAYER_DESCRIPTION
$(if [[ "$LAYER_CONFIG" == "block_split_compression" ]]; then echo "- **Block Size**: $BLOCK_SIZE bytes ($(echo "scale=1; $BLOCK_SIZE / 1024" | bc -l) KiB)"; fi)
- **Iterations**: $ITERATIONS

### Compression Algorithms:
- **No Compression**: Direct local storage
- **LZ4 Ultra Fast** (level -3): Fastest possible compression
- **LZ4 Default** (level 0): Balanced LZ4 compression
- **LZ4 High** (level 9): Best LZ4 compression ratio
- **ZSTD Ultra Fast** (level -5): Fastest ZSTD compression
- **ZSTD Default** (level 3): Balanced ZSTD compression  
- **ZSTD High** (level 15): High ZSTD compression ratio

### Key Metrics:
- **Compression Ratio**: Higher is better (how much space saved)
- **Write Time**: Time to write file through layer system
- **Read Time**: Time to read file through layer system

EOF

    log "✅ Reports generated:"
    log "   📊 CSV: $csv_file"
    log "   📋 Markdown: $report"
}

# Start a single FUSE instance
start_single_instance() {
    local config_name="$1"
    local config_file="$2"
    local mount_dir="$3"
    local backend_dir="$4"
    
    info "Starting $config_name FUSE instance..."
    
    # Clean up any existing mount
    sudo umount "$mount_dir" 2>/dev/null || true
    
    # Create directories
    mkdir -p "$mount_dir" "$backend_dir"
    
    cd "$PROJECT_ROOT"
    
    # Copy config and start instance
    cp "$RESULTS_DIR/${config_file}.toml" "$PROJECT_ROOT/config.toml"
    sudo -E nohup "./bin/examples/fuse/passthrough" "$mount_dir" \
        -omodules=subdir,subdir="$backend_dir" -oallow_other -f \
        > "$RESULTS_DIR/${config_file}.log" 2>&1 &
    
    local pid=$!
    sleep 3
    
    if ! mountpoint -q "$mount_dir"; then
        error "Failed to mount $config_name FUSE instance"
    fi
    
    info "✅ $config_name FUSE mounted (PID: $pid)"
    
    # Store PID for cleanup
    echo "$pid" > "/tmp/fuse_${config_name}_pid"
}

# Stop a single FUSE instance
stop_single_instance() {
    local config_name="$1"
    local mount_dir="$2"
    local backend_dir="$3"
    
    info "Stopping $config_name FUSE instance..."
    
    # Get PID and kill process
    if [ -f "/tmp/fuse_${config_name}_pid" ]; then
        local pid=$(cat "/tmp/fuse_${config_name}_pid")
        sudo kill "$pid" 2>/dev/null || true
        rm -f "/tmp/fuse_${config_name}_pid"
    fi
    
    # Unmount
    sudo umount "$mount_dir" 2>/dev/null || sudo fusermount3 -u "$mount_dir" 2>/dev/null || true
    
    # Verify unmounted
    if mountpoint -q "$mount_dir" 2>/dev/null; then
        warn "⚠️  Force unmounting $config_name..."
        sudo umount -f "$mount_dir" 2>/dev/null || true
    fi
    
    info "✅ $config_name instance stopped"
}

# Test configurations in sequence
test_configurations() {
    local configs=(
        "no_compression:no_compression:$NO_COMPRESSION_MOUNT:$NO_COMPRESSION_BACKEND"
        "lz4_ultra_fast:lz4_ultra_fast:$LZ4_ULTRA_FAST_MOUNT:$LZ4_ULTRA_FAST_BACKEND"
        "lz4_default:lz4_default:$LZ4_DEFAULT_MOUNT:$LZ4_DEFAULT_BACKEND"
        "lz4_high:lz4_high:$LZ4_HIGH_MOUNT:$LZ4_HIGH_BACKEND"
        "zstd_ultra_fast:zstd_ultra_fast:$ZSTD_ULTRA_FAST_MOUNT:$ZSTD_ULTRA_FAST_BACKEND"
        "zstd_default:zstd_default:$ZSTD_DEFAULT_MOUNT:$ZSTD_DEFAULT_BACKEND"
        "zstd_high:zstd_high:$ZSTD_HIGH_MOUNT:$ZSTD_HIGH_BACKEND"
    )
    
    for config in "${configs[@]}"; do
        IFS=':' read -r config_name config_file mount_dir backend_dir <<< "$config"
        
        log "🚀 Testing configuration: $config_name"
        
        if [ "$USE_LD_PRELOAD" = "1" ]; then
            # LD_PRELOAD mode: prepare directories and compile wrapper for this config
            mkdir -p "$mount_dir" "$backend_dir"
            cp "$RESULTS_DIR/${config_file}.toml" "$PROJECT_ROOT/config.toml"
            
            # Compile wrapper (only once)
            if [ -z "$WRAPPER_PATH" ]; then
                compile_wrapper
            fi
            
            export MODULAR_IO_CONFIG="$PROJECT_ROOT/config.toml"
            
            # In LD_PRELOAD mode, files are stored directly in mount_dir
            backend_dir="$mount_dir"
        else
            # Start single FUSE instance
            start_single_instance "$config_name" "$config_file" "$mount_dir" "$backend_dir"
        fi
        
        # Test all files on this instance
        for test_file in "${silesia_files[@]}"; do
            if [ -f "$test_file" ]; then
                run_test "$config_name" "$mount_dir" "$backend_dir" "$test_file"
            else
                warn "Skipping missing file: $(basename "$test_file")"
            fi
        done
        
        if [ "$USE_LD_PRELOAD" != "1" ]; then
            # Stop and cleanup this instance (FUSE only)
            stop_single_instance "$config_name" "$mount_dir" "$backend_dir"
        else
            :
        fi
        
        log "✅ Completed configuration: $config_name"
    done
}

# Main execution
main() {
    log "🚀 Starting Compression Benchmark: $LAYER_DESCRIPTION"
    log "📂 Results will be stored in: $RESULTS_DIR"
    
    # Setup
    rm -rf "$RESULTS_DIR"
    mkdir -p "$RESULTS_DIR"
    
    # Download Silesia corpus
    check_silesia
    
    # Build project
    cd "$PROJECT_ROOT"
    log "Building project..."
    make build >/dev/null || error "Build failed"
    # Compile C IO helper
    compile_io_bench
    if [ "$USE_LD_PRELOAD" != "1" ]; then
        make examples/fuse/build >/dev/null || error "FUSE build failed"
        if [ ! -f "bin/examples/fuse/passthrough" ]; then
            error "FUSE binary not found"
        fi
    fi
    
    # Create configs
    create_configs
    
    # Test each Silesia file with all configurations sequentially
    local silesia_files=(
        "$SILESIA_DIR/dickens"
        "$SILESIA_DIR/mozilla"  
        "$SILESIA_DIR/xml"
        "$SILESIA_DIR/mr"
        "$SILESIA_DIR/nci"
    )

    # Run sequential testing
    test_configurations
    
    # Generate report
    generate_report
    
    log "✅ Benchmark completed!"
    log "📁 Results in: $RESULTS_DIR"
    log "📊 View report: $RESULTS_DIR/benchmark_report.md"
}

# Check requirements
if ! command -v wget >/dev/null; then error "wget is required"; fi
if ! command -v bc >/dev/null; then error "bc is required"; fi
if [ "$USE_LD_PRELOAD" != "1" ] && [ ! -e /dev/fuse ]; then error "FUSE not available"; fi

main "$@"
