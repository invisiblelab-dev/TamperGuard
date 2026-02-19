# Configuration System

The Modular IO Library uses a TOML-based configuration system that allows you to define layer architectures and configure system behavior.

## Overview

The configuration system enables you to:

- Define layer hierarchies and relationships
- Configure individual layer parameters
- Set up logging levels and destinations
- Create multi-backend storage architectures

## Configuration Files

### Primary Configuration

- **`config.toml`** - Main configuration file (copy from `config.toml.example`)
- **`zlog.conf`** - Logging destinations and formats

## TOML Configuration Structure

### Basic Structure
```toml
root = "layer_name"           # Entry point layer
log_mode = "info"            # Global logging level

[layer_name]
type = "layer_type"          # Layer implementation
# layer-specific parameters...
```

### Supported Layer Types

- `local` - Local filesystem storage
- `remote` - Network-based remote storage  
- `anti_tampering` - Data integrity verification
- `compression` - Data compression (LZ4/ZSTD)
- `block_align` - Block-aligned I/O operations
- `demultiplexer` - Parallel multi-backend operations
- `invisible_storage` - Invisible storage integration

## Configuration Patterns

### Simple Configurations
Basic single-layer setups for straightforward use cases.

### Layered Architectures
Multi-layer configurations that combine different functionalities:

- Data integrity layers with storage backends
- Compression layers with multiple storage targets
- Block alignment with various storage types

### Multi-Backend Setups
Complex configurations using the demultiplexer layer:

- Parallel writes to multiple storage systems
- Enforced vs optional storage backends
- Read/write passthrough configurations

## Logging Configuration

### TOML Log Levels
Configure logging in your `config.toml`:

```toml
log_mode = "debug"  # Options: disabled, screen, error, warn, info, debug
```

**Log Level Hierarchy** (each level includes higher priority levels):

- **`disabled`**: No logging output
- **`screen`**: Terminal/console output only
- **`error`**: Error conditions + screen output  
- **`warn`**: Warning messages + error + screen output
- **`info`**: General information + warn + error + screen output
- **`debug`**: Detailed diagnostics + info + warn + error + screen output

### ZLog Configuration (`zlog.conf`)
Configure output destinations and formats:

```ini
[global]
file perms = 0666

[formats]  
simple="[%d] - %m%n"

[rules]
modular_lib.DEBUG     "%E(PWD)/logs/debug.log"; simple
modular_lib.INFO      "%E(PWD)/logs/info.log"; simple
modular_lib.WARN      "%E(PWD)/logs/warn.log"; simple
modular_lib.ERROR     "%E(PWD)/logs/error.log"; simple
modular_lib.*         >stdout; simple
```

**Environment Variables**: Use `%E(VAR_NAME)` for dynamic paths

- `%E(PWD)` - Current working directory
- `%E(HOME)` - User home directory

### Log Output
Generated log files:

- `logs/debug.log` - Debug level and above
- `logs/error.log` - Error level only  
- `logs/info.log` - Info level and above
- `logs/warn.log` - Warning level and above
- Console output - All enabled levels

## Configuration Validation

The system validates configurations at startup:

- **Layer dependencies** - Referenced layers must exist
- **Type compatibility** - Layer types must be implemented
- **Parameter validation** - Required parameters must be provided
- **Circular references** - Prevents infinite layer loops

## Best Practices

### Configuration Design

- **Start Simple** - Begin with basic configurations and add complexity
- **Use Descriptive Names** - Layer names should reflect their purpose
- **Document Configurations** - Comment complex setups
- **Version Control** - Keep configuration files in version control

### Testing and Deployment

- **Test Configurations** - Validate with simple operations before production
- **Backup Configurations** - Keep copies of working configurations
- **Environment-specific** - Use different configs for dev/staging/production
- **Monitor Logs** - Check log output after configuration changes

## Troubleshooting

### Common Issues

- **Missing layer reference**: Check layer names match exactly
- **Invalid layer type**: Verify layer type is implemented
- **Permission errors**: Check file paths and permissions
- **Logging not working**: Verify `zlog.conf` paths exist

### Debug Strategies

- Set `log_mode = "debug"` for verbose output
- Check log files for detailed error messages
- Validate TOML syntax with online validators
- Test with minimal configurations first
- Use layer-specific documentation for parameter details 
