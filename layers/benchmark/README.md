# Benchmark Layer

The **benchmark layer** measures the time required to execute I/O operations on lower layers. It is ideal for performance and stress testing of `read`, `write`, `truncate` and other file operations within the modular I/O system.  
Note that `open` and `close` operations are **excluded** from benchmarking since they cannot be safely repeated multiple times without causing issues (e.g., too many open file descriptors or closing a file descriptor more than once).

## Overview

The benchmark layer provides:

- **Accurate timing measurements** for I/O operations
- **Configurable operation repetition** for more consistent results
- **Transparent behavior** to applications
- **Performance evaluation** of underlying layers
- **Easy integration** into layered I/O stacks

## Key Features

- **Configurable operation repetitions** (`reps`) to reduce measurement variability
- **High-resolution timing** using `clock_gettime(CLOCK_MONOTONIC)`
- **Direct output to `stdout`** with the total elapsed time
- **Independent measurement for reads and writes**
- **Transparent interface** — applications remain unaware of the benchmarking layer

## Usage Notes

The benchmark layer can be used to evaluate the overhead introduced by a specific layer by placing it:

- **Above** the target layer to measure the full stack including it
- **Below** the target layer to measure only what comes after

You can then compare the results to estimate the cost of that specific layer.

## Future Improvements

- **Support for measuring `open` and `close` operations**
- **Support for configurable runtime scripts** to automate consistent tests

## Configuration

To configure the benchmark layer in your `config.toml`:

```toml
[benchmark_layer]
type = "benchmark"
next = "underlying_layer"   # Name of the next layer in the chain
reps = 100                  # Number of repetitions per operation (higher = more stable)
