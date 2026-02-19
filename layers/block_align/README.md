# Block Align Layer

The **block align layer** provides block-aligned I/O operations for the Modular IO Library. It ensures that all read and write operations are aligned to configurable *block boundaries*, optimizing performance for block-based storage systems.

## Overview

The block align layer offers:

- **Configurable block sizes** for optimal alignment
- **Automatic padding** and alignment handling  
- **Transparent operation** for applications
- **Performance optimization** for block-based storage systems
- **Hardware compatibility** for systems requiring aligned I/O

## Block Align Layer

The **block align layer** ensures data operations are aligned to specific *block size boundaries*, optimizing performance for storage systems that benefit from **aligned I/O operations**. This layer acts as an alignment adapter between layers with different block size requirements.

### Key Features

- **Configurable block alignment** with customizable block sizes
- **Automatic padding and alignment** for optimal storage performance
- **Transparent operation** maintaining data integrity during alignment
- **Performance optimization** for block-oriented storage systems

### Configuration

To configure a **block align layer** in your `config.toml`, define it as follows:

```toml
[layer_name]
type = "block_align"
next = "underlying_layer"    # Name of the next layer in the chain
block_size = 4096           # Block size in bytes (optional, defaults to 4096)
```

**Configuration Parameters:**

- `next` (**required**): Name of the underlying layer to forward aligned operations to
- `block_size` (*optional*): Block size in bytes for alignment operations (defaults to 4096 if not specified)

**Usage Notes:**

- Block sizes should typically be powers of 2 for optimal performance
- Common block sizes include 512, 1024, 4096, and 8192 bytes
- The `next` layer must be defined elsewhere in the configuration
- Alignment improves performance for certain storage backends and filesystems

## Features

- **Block Alignment**: Ensures all I/O operations align to block boundaries
- **Configurable Block Size**: Supports various block sizes (512, 1024, 4096, etc.)
- **Padding Handling**: Automatically handles padding for non-aligned data
- **Read Optimization**: Reads full blocks and returns requested portions
- **Write Optimization**: Handles partial block writes efficiently
- **Transparent Interface**: Applications unaware of underlying alignment

## Architecture

### Block Alignment Concept
```
Application Request: Read 100 bytes at offset 50
Block Size: 512 bytes

Block-Aligned Operation:
┌─────────┬─────────┬─────────┐
│ Block 0 │ Block 1 │ Block 2 │
│ 512 B   │ 512 B   │ 512 B   │
└─────────┴─────────┴─────────┘
     ↑ offset 50
     └─ Read full block, return bytes 50-149
```

### Design Principles

- **Alignment Strategy**: All I/O operations aligned to block boundaries
- **Buffer Management**: Block-sized buffers for efficient operations
- **Transparency**: Applications see standard file operations
- **Performance Focus**: Optimizes for block-based storage characteristics

## Configuration

### TOML Configuration
```toml
[block_aligned_layer]
type = "block_align"
block_size = 4096    # Block size in bytes
```

### Parameters

#### Required Parameters

- **`block_size`** (integer): Size of blocks in bytes
  - **Common values**: 512, 1024, 2048, 4096, 8192
  - **Requirement**: Should be power of 2 for optimal performance
  - **Consideration**: Should match underlying storage characteristics

### Block Size Selection Guidelines

| Block Size | Use Case | Performance | Memory |
|------------|----------|-------------|---------|
| **512** | Legacy systems, small files | Good | Low |
| **1024** | General purpose | Good | Low |
| **4096** | Modern SSDs, filesystems | Optimal | Moderate |
| **8192** | High-performance storage | Best | High |

## Operational Behavior

### Read Operations

1. **Alignment Calculation**: Determine which blocks contain requested data
2. **Block Boundary Adjustment**: Expand read to include full blocks
3. **Underlying Read**: Read aligned blocks from next layer
4. **Data Extraction**: Extract requested portion from aligned data
5. **Result Return**: Return only the requested data to application

### Write Operations

1. **Partial Block Handling**: Handle writes not aligned to block boundaries
2. **Read-Modify-Write**: Read existing blocks for partial modifications
3. **Data Merging**: Merge new data with existing block content
4. **Block Write**: Write complete modified blocks to next layer
5. **Optimization**: Full block writes bypass read-modify-write cycle

### File Size Management

- **Size Tracking**: Maintains actual file sizes separate from block-aligned storage
- **Truncation Handling**: Manages truncation to non-aligned sizes
- **Size Queries**: Returns actual file size (not block-aligned size)

## Performance Characteristics

### Performance Benefits

- **Storage Optimization**: Aligns with underlying storage block sizes
- **Hardware Efficiency**: Reduces partial block operations
- **Cache Efficiency**: Better alignment with system page sizes
- **I/O Reduction**: Full block operations are more efficient

### Performance Costs

- **Read Amplification**: May read more data than requested
- **Write Amplification**: Read-modify-write for partial blocks
- **Memory Usage**: Requires block-sized buffers
- **CPU Overhead**: Additional data copying and alignment calculations

### Optimization Strategies

**Read-Heavy Workloads**:

- Use larger block sizes (4096-8192 bytes)
- Match system page sizes for cache efficiency
- Consider access patterns when selecting block size

**Write-Heavy Workloads**:  

- Use smaller block sizes to reduce read-modify-write overhead
- Align application writes to block boundaries when possible
- Monitor write amplification ratios

**Sequential Access**:

- Use larger blocks for sequential throughput
- Consider underlying storage characteristics
- Balance memory usage with performance gains

## Thread Safety

The block align layer ensures thread safety through:

- **Stateless Operations**: Each operation is independent
- **Read-Only Configuration**: Block size is immutable after initialization
- **Buffer Isolation**: Each operation uses separate buffers
- **Layer Delegation**: Thread safety inherited from underlying layers

## Memory Management

### Buffer Usage

- **Temporary Buffers**: Allocated per operation for alignment
- **Block-Sized Allocations**: Buffers sized to block boundaries
- **Automatic Cleanup**: Buffers freed after operations complete
- **Memory Efficiency**: Minimal persistent memory usage

### Memory Optimization

- **Aligned Allocation**: Uses aligned memory allocation for efficiency
- **Buffer Reuse**: Minimizes allocation overhead where possible
- **Size Calculation**: Optimizes buffer sizes for operations

## Error Handling

### Alignment Errors

- **Invalid Block Sizes**: Validation during layer initialization
- **Alignment Failures**: Graceful fallback mechanisms
- **Buffer Allocation Failures**: Proper error handling and recovery

### Storage Errors

- **Layer Propagation**: Underlying layer errors passed to application
- **Partial Operations**: Maintains consistency on operation failures
- **State Management**: Proper error state handling

## Hardware Considerations

### SSD Optimization

- **Page Size Alignment**: *4KB blocks* match SSD page sizes
- **Write Amplification**: Minimizes SSD write amplification
- **Trim Support**: Better interaction with SSD trim operations

### HDD Optimization  

- **Sector Alignment**: *512-byte alignment* for traditional HDDs
- **Track Alignment**: Consider track sizes for sequential access
- **Seek Reduction**: Minimize head movement operations

### Network Storage

- **Packet Alignment**: Align with network packet sizes
- **Protocol Efficiency**: Reduce network protocol overhead
- **Bandwidth Utilization**: Optimize network resource usage

## Use Cases

### Primary Applications

- **Database Storage**: Align with database page sizes
- **Virtual Machine Disks**: Optimize VM disk I/O performance
- **Media Streaming**: Align with media block sizes
- **High-Performance Computing**: Optimize for parallel I/O operations

### Integration Scenarios

- **Storage Stack Optimization**: As part of layered storage architectures
- **Hardware Compatibility**: Meeting hardware alignment requirements
- **Performance Tuning**: Optimizing for specific storage characteristics
- **System Integration**: Aligning with system memory and cache sizes

## Building and Testing

### Build Commands
```bash
# Build all components (includes block align layer)
make build

# Or build just shared components (includes block align layer)
make shared/build

# Run block align layer tests
make tests/run
```

### Test Coverage

- Various block sizes and alignment scenarios
- Partial block read/write operations
- File size and truncation handling
- Error condition testing and recovery
- Performance benchmarking and analysis

## Best Practices

### Configuration Guidelines

- **Block Size Selection**: Match to underlying storage characteristics
- **Access Pattern Analysis**: Align configuration with usage patterns
- **Memory Constraints**: Balance performance vs memory usage
- **Hardware Matching**: Configure for target hardware platform

### Performance Tuning

- **Monitoring**: Track read/write amplification ratios
- **Profiling**: Monitor memory usage patterns
- **Benchmarking**: Test different block sizes for your workload
- **Comparison**: Benchmark against non-aligned operations

## Troubleshooting

### Common Issues

- **Performance Degradation**: Check block size vs access patterns alignment
- **Memory Usage**: Monitor buffer allocation and usage patterns
- **Alignment Mismatches**: Verify block size configuration appropriateness
- **Compatibility Issues**: Test with underlying storage systems

### Debugging Strategies

1. Enable *debug logging*: `log_mode = "debug"`
2. Monitor read/write amplification ratios
3. Profile memory usage and allocation patterns
4. Test different block sizes for your workload
5. Benchmark performance against non-aligned operations

### Performance Analysis

- **Access Pattern Monitoring**: Analyze application I/O patterns
- **Hardware Profiling**: Understand underlying storage characteristics
- **Memory Analysis**: Monitor buffer usage and allocation efficiency
- **Throughput Testing**: Measure performance under various configurations

## Limitations

### Current Limitations

- **Fixed Block Size**: Block size set at initialization time
- **Memory Overhead**: Requires additional memory for alignment buffers
- **Read/Write Amplification**: May increase I/O operations for small requests
- **Configuration Complexity**: Requires understanding of storage characteristics
- **Append Atomicity**: O_APPEND flag can't guarantee operation atomicity to ensure block aligment

### Design Trade-offs

- **Performance vs Memory**: Larger blocks improve performance but use more memory
- **Alignment vs Flexibility**: Alignment improves performance but adds complexity
- **Hardware Optimization vs Portability**: Hardware-specific tuning reduces portability

## Future Enhancements

### Planned Improvements

- **Dynamic Block Sizing**: Adjust block size based on access patterns
- **Statistics Collection**: Track alignment efficiency and performance metrics
- **Auto-Configuration**: Detect optimal block sizes automatically
- **Advanced Optimization**: NUMA-aware alignment and adaptive algorithms 
