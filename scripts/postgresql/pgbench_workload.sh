#!/bin/bash

# Simple PostgreSQL pgbench testing script
# Matches exact requirements with /lib/postgresql/16/bin/ paths

set -e

# Default values
DB_NAME="mydb"
# By default, use the OS user running the script. This matches how `initdb`
# chooses the initial cluster superuser when you don't pass `--username`.
# (Many local `initdb` runs end up with a superuser named like your shell user,
# not necessarily `postgres`.)
DB_USER="${USER:-postgres}"
HOST="127.0.0.1"
POSTGRES_DATA_DIR=""  # REQUIRED: Must be provided via --data-dir
POSTGRES_BIN=""       # REQUIRED: Must be provided via --postgres-bin
PGBENCH_BIN="/usr/bin/pgbench"
repeats="3"
DURATION="300"
READ_ONLY="1"

# Usage function
usage() {
    cat << EOF
Usage: $0 --data-dir DIR --postgres-bin DIR [OPTIONS]

PostgreSQL pgbench testing script

REQUIRED:
    --data-dir DIR              PostgreSQL data directory (REQUIRED)
    --postgres-bin DIR          PostgreSQL binaries directory (REQUIRED)

OPTIONS:
    --db-name NAME              Database name (default: mydb)
    --db-user USER              Database user (default: \$USER, fallback: postgres)
    --host HOST                 Database host (default: 127.0.0.1)
    --pgbench-bin PATH          Path to pgbench binary (default: /usr/bin/pgbench)
    --repeats N                 Number of test runs (default: 3)
    --duration SECONDS          Test duration in seconds (default: 300)
    --read-only [0|1|true|false] Enable read-only mode with -S flag (default: 1)
    -h, --help                  Show this help message

Examples:
    $0 --data-dir /mnt/fuse/mount --postgres-bin /usr/bin
    $0 --data-dir /mnt/fuse/mount --postgres-bin /usr/bin --duration 600
    $0 --data-dir /mnt/fuse/mount --postgres-bin /usr/bin --read-only 0 --duration 120

EOF
    exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --db-name)
            if [[ -z "$2" ]]; then
                echo "Error: --db-name requires a value"
                usage
            fi
            DB_NAME="$2"
            shift 2
            ;;
        --db-user)
            if [[ -z "$2" ]]; then
                echo "Error: --db-user requires a value"
                usage
            fi
            DB_USER="$2"
            shift 2
            ;;
        --host)
            if [[ -z "$2" ]]; then
                echo "Error: --host requires a value"
                usage
            fi
            HOST="$2"
            shift 2
            ;;
        --data-dir)
            if [[ -z "$2" ]]; then
                echo "Error: --data-dir requires a value"
                usage
            fi
            POSTGRES_DATA_DIR="$2"
            shift 2
            ;;
        --postgres-bin)
            if [[ -z "$2" ]]; then
                echo "Error: --postgres-bin requires a value"
                usage
            fi
            POSTGRES_BIN="$2"
            shift 2
            ;;
        --pgbench-bin)
            if [[ -z "$2" ]]; then
                echo "Error: --pgbench-bin requires a value"
                usage
            fi
            PGBENCH_BIN="$2"
            shift 2
            ;;
        --repeats)
            if [[ -z "$2" ]]; then
                echo "Error: --repeats requires a value"
                usage
            fi
            repeats="$2"
            shift 2
            ;;
        --duration)
            if [[ -z "$2" ]]; then
                echo "Error: --duration requires a value"
                usage
            fi
            DURATION="$2"
            shift 2
            ;;
        --read-only)
            if [[ -z "$2" ]]; then
                echo "Error: --read-only requires a value"
                usage
            fi
            READ_ONLY="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Error: Unknown option: $1"
            usage
            ;;
    esac
done

# Validate required parameters
if [[ -z "$POSTGRES_DATA_DIR" ]]; then
    echo "Error: --data-dir is required"
    echo ""
    usage
fi

if [[ -z "$POSTGRES_BIN" ]]; then
    echo "Error: --postgres-bin is required"
    echo ""
    usage
fi

echo "=== PostgreSQL pgbench Testing Script ==="

echo "Using configuration:"
echo "  DB_NAME          = $DB_NAME"
echo "  DB_USER          = $DB_USER"
echo "  HOST             = $HOST"
echo "  POSTGRES_DATA_DIR= $POSTGRES_DATA_DIR"
echo "  POSTGRES_BIN     = $POSTGRES_BIN"
echo "  PGBENCH_BIN      = $PGBENCH_BIN"
echo "  repeats          = $repeats"
echo "  duration         = $DURATION seconds"
echo "  read_only        = $READ_ONLY"
echo ""

echo "1. Checking disk usage before database creation..."
echo "=== DISK USAGE RESULTS ===" > /tmp/disk_usage_results.txt
echo "Folder monitored: $POSTGRES_DATA_DIR" >> /tmp/disk_usage_results.txt
echo "" >> /tmp/disk_usage_results.txt

if [ -d "$POSTGRES_DATA_DIR" ]; then
    usage1=$(du -sh "$POSTGRES_DATA_DIR" 2>/dev/null | awk '{print $1}')
    echo "Before database creation: $usage1"
    echo "Before database creation: $usage1" >> /tmp/disk_usage_results.txt
else
    echo "Before database creation: N/A (folder not found)"
    echo "Before database creation: N/A (folder not found)" >> /tmp/disk_usage_results.txt
fi

echo "2. Creating database '$DB_NAME'..."
$POSTGRES_BIN/createdb -h $HOST -U $DB_USER $DB_NAME 2>/dev/null || echo "Database might already exist"

echo "3. Running pgbench initialization (scale factor 50)..."
$PGBENCH_BIN -i -s 50 -h $HOST -U $DB_USER $DB_NAME

echo "4. Checking disk usage after initialization..."
if [ -d "$POSTGRES_DATA_DIR" ]; then
    usage2=$(du -sh "$POSTGRES_DATA_DIR" 2>/dev/null | awk '{print $1}')
    echo "After initialization: $usage2"
    echo "After initialization: $usage2" >> /tmp/disk_usage_results.txt
else
    echo "After initialization: N/A (folder not found)"
    echo "After initialization: N/A (folder not found)" >> /tmp/disk_usage_results.txt
fi

echo "5. Running pgbench tests ($repeats times)..."
echo "=== PGBENCH TEST RESULTS ===" > /tmp/tps_results.txt

total_latency=0
total_tps=0
latency_values=()
tps_values=()

# Build pgbench command with optional read-only flag
PGBENCH_CMD="$PGBENCH_BIN -c 10 -j 2"
# Add -S flag unless explicitly disabled (0, false, or no)
if [ "$READ_ONLY" != "0" ] && [ "$READ_ONLY" != "false" ] && [ "$READ_ONLY" != "no" ]; then
    PGBENCH_CMD="$PGBENCH_CMD -S"
fi
PGBENCH_CMD="$PGBENCH_CMD -T $DURATION -h $HOST -U $DB_USER $DB_NAME"

for ((i=1; i<=repeats; i++)); do
    echo "Test $i/$repeats:"
    echo "=== Test $i ===" >> /tmp/tps_results.txt
    run_output="$($PGBENCH_CMD 2>&1)"
    echo "$run_output" >> /tmp/tps_results.txt
    echo "" >> /tmp/tps_results.txt

    latency_value=$(printf '%s\n' "$run_output" | awk '/latency average/ {print $4; exit}')
    tps_value=$(printf '%s\n' "$run_output" | awk '/^tps =/ {print $3; exit}')

    if [ -n "$latency_value" ]; then
        total_latency=$(echo "$total_latency + $latency_value" | bc -l)
        latency_values+=("$latency_value")
    fi

    if [ -n "$tps_value" ]; then
        total_tps=$(echo "$total_tps + $tps_value" | bc -l)
        tps_values+=("$tps_value")
    fi

    echo "$run_output"
    echo ""
    
    if [ $i -lt $repeats ]; then
        echo "Waiting 10 seconds..."
        sleep 10
    fi
done

echo ""
echo "6. Checking disk usage after all tests..."
if [ -d "$POSTGRES_DATA_DIR" ]; then
    usage3=$(du -sh "$POSTGRES_DATA_DIR" 2>/dev/null | awk '{print $1}')
    echo "After all tests: $usage3"
    echo "After all tests: $usage3" >> /tmp/disk_usage_results.txt
else
    echo "After all tests: N/A (folder not found)"
    echo "After all tests: N/A (folder not found)" >> /tmp/disk_usage_results.txt
fi

echo ""
echo "=== FINAL RESULTS ==="
cat /tmp/tps_results.txt
echo "================"

if [ "$repeats" -gt 0 ]; then
    avg_latency=$(echo "$total_latency / $repeats" | bc -l)
    avg_tps=$(echo "$total_tps / $repeats" | bc -l)

    # Calculate standard deviation for latency
    latency_variance=0
    if [ ${#latency_values[@]} -gt 1 ]; then
        for value in "${latency_values[@]}"; do
            diff=$(echo "$value - $avg_latency" | bc -l)
            squared=$(echo "$diff * $diff" | bc -l)
            latency_variance=$(echo "$latency_variance + $squared" | bc -l)
        done
        latency_variance=$(echo "$latency_variance / $repeats" | bc -l)
        latency_stddev=$(echo "sqrt($latency_variance)" | bc -l)
    else
        latency_stddev=0
    fi

    # Calculate standard deviation for TPS
    tps_variance=0
    if [ ${#tps_values[@]} -gt 1 ]; then
        for value in "${tps_values[@]}"; do
            diff=$(echo "$value - $avg_tps" | bc -l)
            squared=$(echo "$diff * $diff" | bc -l)
            tps_variance=$(echo "$tps_variance + $squared" | bc -l)
        done
        tps_variance=$(echo "$tps_variance / $repeats" | bc -l)
        tps_stddev=$(echo "sqrt($tps_variance)" | bc -l)
    else
        tps_stddev=0
    fi

    printf "\n=== AVERAGE METRICS ===\n"
    printf "Latency average (across runs): %.6f ms\n" "$avg_latency"
    printf "Latency standard deviation: %.6f ms\n" "$latency_stddev"
    printf "TPS average (across runs): %.6f\n" "$avg_tps"
    printf "TPS standard deviation: %.6f\n" "$tps_stddev"
fi

echo ""
echo "=== DISK USAGE SUMMARY ==="
cat /tmp/disk_usage_results.txt
echo "=========================="

echo "Script completed!"

