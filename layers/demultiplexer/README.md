# Demultiplexer Layer

The **demultiplexer layer** routes operations across multiple underlying layers based on configurable policies. This layer enables *parallel processing*, **load distribution**, and **redundancy** by intelligently managing multiple storage backends.

### Key Features

- **Multi-layer routing** with configurable layer selection policies
- **Passthrough operations** for optimized read/write patterns
- **Enforced layer support** ensuring critical layers receive all operations  
- **Dynamic load balancing** across available storage backends
- **Thread-safe coordination** of concurrent operations

### Configuration

```toml
[layer_name]
type = "demultiplexer"
layers = ["layer_1", "layer_2", "layer_3"]    # Array of underlying layers

[layer_name.options]
enforced_layers = ["layer_1"]                 # Layers that must receive all operations (optional)
passthrough_reads = ["layer_2"]               # Layers optimized for read operations (optional)  
passthrough_writes = ["layer_3"]              # Layers optimized for write operations (optional)
```

**Parameters:**
- `layers` (**required**): Array of layer names that this demultiplexer manages
- `enforced_layers` (*optional*): Array of layer names that must receive all operations
- `passthrough_reads` (*optional*): Array of layer names optimized for read operations
- `passthrough_writes` (*optional*): Array of layer names optimized for write operations

**Usage Notes:**
- All layer names must correspond to other defined layers in the configuration
- Layers can appear in multiple option arrays to combine behaviors
- If not specified, the first layer becomes enforced and others are optional

## Overview

The demultiplexer creates a tree-like structure where a single operation is distributed across multiple underlying layers. It supports:

- **Parallel Operations**: All operations run concurrently across configured layers
- **Enforced Layers**: Critical layers that must succeed for the operation to succeed
- **Passthrough Operations**: Skip specific operations on certain layers for optimization
- **Thread Safety**: All operations are thread-safe with proper synchronization

## Configuration Options

### Required Parameters

- **`layers`** (array of strings): List of layer names to multiplex operations across

  - Must contain at least one layer
  - Layers are referenced by their configuration section names

### Optional Parameters (in `options` table)

#### `enforced_layers` (array of strings)

- **Purpose**: Specify which layers must succeed for the operation to succeed
- **Default**: If not specified, the first layer becomes enforced and others are optional
- **Behavior**: Operation fails if any enforced layer fails

#### `passthrough_reads` (array of strings)

- **Purpose**: Skip read operations on specified layers
- **Use Case**: Write-only backup layers or layers that don't serve reads.
- **Behavior**: These layers return immediately without performing reads

#### `passthrough_writes` (array of strings)

- **Purpose**: Skip write operations on specified layers
- **Use Case**: Read-only cache layers or immutable storage
- **Behavior**: These layers return immediately without performing writes

## Operational Behavior

### Parallel Execution
All configured layers receive operations simultaneously:

- **Concurrency**: Operations execute in parallel threads
- **Synchronization**: Results are collected and aggregated
- **Performance**: Total operation time equals slowest layer time

### Error Handling
Error behavior depends on layer enforcement:

- **Enforced layers**: Any failure causes operation failure
- **Optional layers**: Failures are logged but don't fail the operation
- **Result aggregation**: Success determined by enforced layer results

### Read Operations
Read behavior with multiple layers:

- **First success**: Return data from first successful read
- **Fallback**: Try remaining layers if primary fails
- **Consistency**: No consistency guarantees between layers

### Write Operations
Write behavior with multiple layers:

- **Parallel writes**: All layers receive write operations
- **Success criteria**: Based on enforced layer configuration
- **Partial failures**: Optional layer failures don't prevent success

## Validation Rules

The demultiplexer enforces validation rules at initialization:

### Invalid Configurations

- **Both passthrough**: Layer cannot skip both reads and writes
- **All read passthrough**: At least one layer must handle reads
- **All write passthrough**: At least one layer must handle writes
- **Enforced with passthrough**: If a layer is enforced, should not have passthroughs

### Valid Configurations

- **Mixed passthrough**: Different layers can skip different operations
- **Selective enforcement**: Any subset of layers can be enforced
- **Flexible combinations**: Various layer arrangements are supported

## Use Cases

### Data Redundancy
Multiple storage backends for reliability:

- **Local + Remote**: Combine local and remote storage
- **Multi-cloud**: Distribute across different cloud providers
- **Backup strategies**: Primary storage with backup layers

### Performance Optimization
Layer-specific optimizations:

- **Cache layers**: Fast read-only caches
- **Write-through**: Immediate writes with background replication
- **Passthrough optimization**: Skip unnecessary operations

### Hybrid Architectures
Complex storage arrangements:

- **Tiered storage**: Different storage classes for different needs
- **Specialized layers**: Combine compression, encryption, and storage
- **Development/Production**: Different layer configs for different environments

## Best Practices

### Configuration Design

- **Start simple**: Begin with basic multi-layer setup
- **Enforce critical layers**: Mark essential storage as enforced
- **Optimize with passthrough**: Skip unnecessary operations
- **Document layer purposes**: Clear naming and documentation

### Performance Considerations

- **Layer ordering**: Place fastest layers first for reads
- **Enforce judiciously**: Only enforce truly critical layers
- **Monitor performance**: Track individual layer performance
- **Consider passthrough**: Skip operations that don't add value

### Error Management

- **Plan for failures**: Design for partial layer failures
- **Monitor all layers**: Track health of optional layers too
- **Graceful degradation**: Ensure system works with subset of layers
- **Alert on enforcement failures**: Monitor enforced layer health
