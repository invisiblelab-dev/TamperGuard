# PostgreSQL pgbench Workload Script

A comprehensive bash script for running PostgreSQL pgbench performance tests with automatic database setup, disk usage monitoring, and detailed metrics reporting.

## Overview

This script automates the process of:
- Creating a PostgreSQL database
- Initializing pgbench with a scale factor of 50
- Running multiple pgbench test iterations
- Monitoring disk usage at different stages
- Calculating and reporting average latency and TPS metrics

## Prerequisites

- PostgreSQL server running and accessible
- `pgbench` utility installed
- `psql` and `createdb` utilities available
- `bc` calculator for metric calculations
- Appropriate database user permissions

## Setup

Before running the pgbench workload script, you must initialize the PostgreSQL data directory and start the server.

> **Note**: PostgreSQL binaries are typically located in:
> - `/usr/bin/` (default on most Linux distributions)
> - `/usr/lib/postgresql/16/bin/` (Ubuntu/Debian specific version)
> - `/lib/postgresql/16/bin/` (alternative location)
>
> Adjust the paths below according to your PostgreSQL installation.

### 1. Initialize the Database

Initialize a new PostgreSQL database cluster:

```bash
initdb -D examples/fuse/mount_point/

# Example with full path:
# /usr/lib/postgresql/16/bin/initdb -D examples/fuse/mount_point/
```

This creates a new PostgreSQL data directory at `examples/fuse/mount_point/`.

> **Important**: `initdb` creates the initial cluster superuser. If you don't pass
> `--username`, that superuser is named after the **OS user** running `initdb`
> (e.g. `dev`), not necessarily `postgres`.
>
> If you want the default database superuser to be `postgres`, initialize like:
>
> ```bash
> initdb -D examples/fuse/mount_point/ --username=postgres
> ```
>
> If you already initialized with (say) `dev`, just run the workload script with
> `--db-user dev` (or `--db-user $(whoami)`).

### 2. Start the PostgreSQL Server

If this step isn't working, remember to change the `unix_socket_directories` path to use `/tmp`, and create the folder with:
```bash
mkdir -p /tmp/postgresql && chmod 1777 /tmp/postgresql
```

Start the PostgreSQL server using the initialized data directory:

```bash
# Example: Start server in the background
postgres -D examples/fuse/mount_point/ &

# Or use pg_ctl for better control
pg_ctl -D examples/fuse/mount_point/ start

# Example with full path:
# /usr/lib/postgresql/16/bin/pg_ctl -D examples/fuse/mount_point/ start
```

**Note**: Adjust the data directory location according to your desired configuration.

### 3. Verify Server is Running

Check that the server is accepting connections:

```bash
pg_isready -h 127.0.0.1
```

Once the server is running and accessible, you can proceed to run the pgbench workload script.

## Usage

```bash
./pgbench_workload.sh --data-dir DIR --postgres-bin DIR [OPTIONS]
```

## Stop server

```bash
pg_ctl -D examples/fuse/mount_point/ -l logfile stop

# Example with full path:
# /usr/lib/postgresql/16/bin/pg_ctl -D examples/fuse/mount_point/ -l logfile stop
```

## Required Parameters

| Option | Description |
|--------|-------------|
| `--data-dir DIR` | **REQUIRED** PostgreSQL data directory to monitor |
| `--postgres-bin DIR` | **REQUIRED** Directory containing PostgreSQL binaries |

## Optional Parameters

| Option | Description | Default |
|--------|-------------|---------|
| `--db-name NAME` | Database name to create/use | `mydb` |
| `--db-user USER` | PostgreSQL user for connections | `postgres` |
| `--host HOST` | Database host address | `127.0.0.1` |
| `--pgbench-bin PATH` | Full path to pgbench binary | `/usr/bin/pgbench` |
| `--repeats N` | Number of test runs to execute | `3` |
| `--duration SECONDS` | Duration of each test run in seconds | `300` |
| `--read-only [0\|1\|true\|false]` | Enable read-only mode (adds `-S` flag) | `1` (enabled) |
| `-h, --help` | Display help message | - |

## Examples

> **Note**: All examples require `--data-dir` and `--postgres-bin` parameters. These are mandatory and have no default values.

### Basic Usage

Run with all default settings (required parameters must be provided):
```bash
./pgbench_workload.sh \
    --data-dir /home/admin/Modular-IO-Lib/examples/fuse/mount_point \
    --postgres-bin /usr/bin
```

This will:
- Create database `mydb` (or use existing)
- Run 3 test iterations
- Each test runs for 300 seconds (5 minutes)
- Use read-only mode (`-S` flag)

### Custom Database and Duration

Run with a custom database name and longer test duration:
```bash
./pgbench_workload.sh \
    --data-dir /home/admin/Modular-IO-Lib/examples/fuse/mount_point \
    --postgres-bin /usr/bin \
    --db-name testdb \
    --duration 600
```

### Read-Write Mode

Run in read-write mode (disables `-S` flag) with shorter duration:
```bash
./pgbench_workload.sh \
    --data-dir /home/admin/Modular-IO-Lib/examples/fuse/mount_point \
    --postgres-bin /usr/bin \
    --read-only 0 \
    --duration 120
```

### Multiple Test Runs

Run 10 test iterations to get better statistical averages:
```bash
./pgbench_workload.sh \
    --data-dir /home/admin/Modular-IO-Lib/examples/fuse/mount_point \
    --postgres-bin /usr/bin \
    --repeats 10 \
    --duration 300
```

### Remote Database

Connect to a remote PostgreSQL server:
```bash
./pgbench_workload.sh \
    --data-dir /var/lib/postgresql/data \
    --postgres-bin /usr/bin \
    --host 192.168.1.100 \
    --db-user myuser \
    --db-name production_test
```

### Custom PostgreSQL Installation

Use a custom PostgreSQL installation path:
```bash
./pgbench_workload.sh \
    --data-dir /var/lib/postgresql/16/data \
    --postgres-bin /usr/lib/postgresql/16/bin \
    --pgbench-bin /usr/lib/postgresql/16/bin/pgbench
```

### Full Custom Configuration

Complete example with all options customized:
```bash
./pgbench_workload.sh \
    --data-dir /var/lib/postgresql/data \
    --postgres-bin /usr/bin \
    --db-name benchmark_db \
    --db-user jnuno \
    --host localhost \
    --pgbench-bin /usr/bin/pgbench \
    --repeats 5 \
    --duration 600 \
    --read-only 1
```

### Quick Performance Test

Run a quick 1-minute test in read-write mode:
```bash
./pgbench_workload.sh \
    --data-dir /home/admin/Modular-IO-Lib/examples/fuse/mount_point \
    --postgres-bin /usr/bin \
    --duration 60 \
    --read-only 0 \
    --repeats 1
```

## Script Workflow

1. **Disk Usage Check (Before)**: Records disk usage of the data directory before database creation
2. **Database Creation**: Creates the specified database (or uses existing)
3. **pgbench Initialization**: Initializes pgbench with scale factor 50 (`-i -s 50`)
4. **Disk Usage Check (After Init)**: Records disk usage after initialization
5. **Test Execution**: Runs pgbench tests the specified number of times
   - Each test runs for the specified duration
   - 10 clients (`-c 10`) and 2 threads (`-j 2`) are used
   - Read-only mode (`-S`) is enabled by default
6. **Disk Usage Check (After Tests)**: Records final disk usage
7. **Results Summary**: Displays average latency and TPS across all runs, along with standard deviations to measure consistency

## Output Files

The script generates temporary result files:

- `/tmp/tps_results.txt`: Detailed pgbench output for each test run
- `/tmp/disk_usage_results.txt`: Disk usage measurements at different stages

## Metrics Reported

- **Latency Average**: Average transaction latency in milliseconds (across all runs)
- **Latency Standard Deviation**: Variability of transaction latency across runs
- **TPS Average**: Average transactions per second (across all runs)
- **TPS Standard Deviation**: Variability of TPS across runs
- **Disk Usage**: Before database creation, after initialization, and after all tests

## pgbench Parameters

The script uses the following pgbench parameters:
- `-c 10`: 10 concurrent clients
- `-j 2`: 2 worker threads
- `-S`: Read-only mode (when `--read-only` is enabled)
- `-T DURATION`: Test duration in seconds
- `-i -s 50`: Initialization with scale factor 50

## Read-Only Mode

The `--read-only` flag controls whether pgbench runs in read-only mode:
- `1`, `true`, `yes`, or any non-zero value: Enables read-only mode (adds `-S` flag)
- `0`, `false`, or `no`: Disables read-only mode (read-write workload)

## Error Handling

- The script uses `set -e` to exit on any error
- **Required parameters validation**: Script exits with an error if `--data-dir` or `--postgres-bin` are not provided
- Database creation failures are handled gracefully (assumes database already exists)
- Missing directories are reported but don't stop execution
- Invalid command-line options display an error and show usage

## Troubleshooting

### Connection Issues

If you encounter connection errors:
```bash
# Check if PostgreSQL is running
pg_isready -h 127.0.0.1

# Verify user permissions
psql -h 127.0.0.1 -U your_user -d postgres -c "\du"
```

### Permission Errors

Ensure your database user has:
- `CREATE DATABASE` privilege
- Access to the target database
- Appropriate permissions for pgbench operations

### Missing Dependencies

Install required tools:
```bash
# Ubuntu/Debian
sudo apt-get install postgresql-client postgresql-contrib bc

# CentOS/RHEL
sudo yum install postgresql postgresql-contrib bc
```

## Notes

- The script waits 10 seconds between test runs
- Scale factor 50 creates a moderately sized test database
- Results are written to `/tmp/` and may be overwritten by subsequent runs
- The script will attempt to create the database; if it already exists, it will continue

## See Also

- [PostgreSQL pgbench Documentation](https://www.postgresql.org/docs/current/pgbench.html)
- PostgreSQL performance tuning guides

