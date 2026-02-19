# Read Cache Layer

A **read cache layer** introduces a caching layer for read operations in the modular I/O system.  
Its main goal is to **reduce redundant accesses to lower layers** (and consequently the disk or remote backends) by storing blocks of data that have already been read.  

With this approach, subsequent reads to the same block are served directly from the cache, avoiding unnecessary I/O costs and improving overall performance.  

## Overview

The read cache layer provides:

- **In-memory caching** for file blocks
- **Automatic consistency management** (invalidating or updating blocks when necessary, e.g., writes, truncates, unlinks, etc)
- **Transparency** for applications — no user code changes required
- **Easy integration** into any layer stack

## Key Features

- **Block-based caching** defined by the `block_size` parameter
- **Configurable number of blocks to cache** 
- **Configurable block size**, allowing to cache more or less data at once according to memory availability and performance needs

## Usage Notes

**This layer currently works in synergy with block align to cache entire blocks, enhancing the performance of programs that exhibit spacial locality in their read requests.**

The layer can be used to:

- **Improve performance of frequent reads** in large files
- **Avoid redundant reads** in I/O-intensive workloads

## Future Improvements

- **Configurable block replacement policy** (LRU, LFU, etc.)
- **Thread-safe handling of cache pools**
- **Local disk caching** for remote-heavy operations

## Configuration

Example configuration in `config.toml`:

```toml
[read_cache_layer]
type = "read_cache"
next = "underlying_layer"   # Name of the next layer in the stack
block_size = 4096           # Size of each block in bytes. This value needs to be the same as the block_align block_size
num_blocks = 1024           # Maximum number of blocks in cache
