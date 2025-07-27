# Goxel v14.6 Unified Headless Design Plan

## Executive Summary

Goxel v14.6 introduces a unified binary architecture where the headless daemon functionality is integrated into the main `goxel` executable, accessible via the `--headless` flag. This design eliminates the need for separate binaries while providing superior performance through daemon/client architecture.

## Architecture Overview

### Unified Binary Design
```
goxel [options]
  ├── GUI Mode (default)
  │   └── Traditional Goxel GUI application
  └── Headless Mode (--headless)
      ├── Daemon Server Mode
      ├── CLI Client Mode  
      └── Interactive Shell Mode
```

### Command Structure
```bash
# Start daemon server
goxel --headless --daemon [options]

# CLI client commands
goxel --headless create test.gox
goxel --headless add-voxel 0 0 0 255 0 0 255
goxel --headless export test.obj

# Interactive mode
goxel --headless --interactive

# Batch processing
goxel --headless --batch script.txt
```

## Technical Architecture

### 1. Binary Integration Strategy
- **Single Executable**: Merge GUI and headless codebases
- **Runtime Mode Selection**: Early initialization based on flags
- **Conditional Compilation**: Use preprocessor directives for mode-specific code
- **Shared Core**: Common voxel engine used by both modes

### 2. Daemon Architecture
```
┌─────────────────────────────────────────────┐
│             goxel executable                 │
├─────────────────────────────────────────────┤
│         Mode Detection Layer                 │
├──────────────────┬──────────────────────────┤
│   GUI Mode       │    Headless Mode          │
│   ┌──────────┐   │   ┌────────────────┐     │
│   │   GLFW   │   │   │  Daemon Server │     │
│   │  ImGui   │   │   ├────────────────┤     │
│   │ Renderer │   │   │   JSON-RPC     │     │
│   └──────────┘   │   │  Unix Socket   │     │
│                  │   │   TCP Socket   │     │
│                  │   └────────────────┘     │
├──────────────────┴──────────────────────────┤
│           Shared Goxel Core Engine           │
│  (Volume, Layer, Export, Import, Render)     │
└─────────────────────────────────────────────┘
```

### 3. Communication Protocol
- **Primary**: Unix Domain Socket (`/tmp/goxel.sock`)
- **Secondary**: TCP Socket (configurable port)
- **Protocol**: JSON-RPC 2.0 with binary data extensions
- **Authentication**: Token-based for remote connections

### 4. Process Management
```
┌─────────────────┐     ┌─────────────────┐
│ goxel --headless│     │ goxel --headless│
│    (client)     │────▶│   --daemon      │
└─────────────────┘     └─────────────────┘
         │                       │
         │    JSON-RPC          │
         └──────────────────────┘
```

## Implementation Phases

### Phase 1: Binary Unification (Week 1-2)
- Merge headless and GUI codebases
- Implement mode detection and initialization
- Create unified build system

### Phase 2: Daemon Infrastructure (Week 3-4)
- Implement daemon server with socket listeners
- Create process management system
- Add health monitoring and auto-restart

### Phase 3: Client Communication (Week 5-6)
- Implement JSON-RPC protocol handlers
- Create client-side command dispatcher
- Add connection pooling and retry logic

### Phase 4: Feature Parity (Week 7-8)
- Ensure all v13.4 headless features work
- Add new daemon-specific features
- Implement performance optimizations

### Phase 5: Testing & Documentation (Week 9-10)
- Comprehensive testing suite
- Performance benchmarking
- User and developer documentation

## Performance Targets

### Baseline (v13.4 CLI Mode)
- Startup: 9.88ms per command
- 5 commands: 69.86ms total

### v14.6 Daemon Mode Targets
- Daemon startup: <20ms (one-time)
- Command latency: <2ms
- 5 commands: <30ms total (>230% improvement)
- Memory overhead: <10MB for daemon

## Key Features

### 1. Unified Binary Benefits
- **Single Installation**: No separate headless binary
- **Code Reuse**: Shared core engine
- **Easier Maintenance**: One codebase to maintain
- **Flexible Deployment**: Same binary for all use cases

### 2. Daemon Advantages
- **Persistent State**: Keep projects loaded in memory
- **Connection Pooling**: Reuse socket connections
- **Batch Optimization**: Process multiple commands efficiently
- **Resource Sharing**: Single OSMesa context

### 3. Backward Compatibility
- Support legacy `goxel-headless` commands via symlink
- Maintain v13.4 CLI interface
- Graceful fallback to non-daemon mode

## Success Criteria

1. **Performance**: >200% improvement over v13.4 CLI mode
2. **Compatibility**: 100% feature parity with v13.4
3. **Reliability**: 99.9% uptime for daemon process
4. **Usability**: Seamless transition for existing users
5. **Integration**: Works with existing MCP server

## Risk Mitigation

1. **Binary Size**: Use conditional compilation to minimize overhead
2. **Complexity**: Clear separation of concerns between modes
3. **Testing**: Automated test suite for both modes
4. **Migration**: Provide migration scripts and documentation

## Configuration

### Default Configuration (`~/.goxel/daemon.conf`)
```json
{
  "daemon": {
    "socket": "/tmp/goxel.sock",
    "tcp_port": 7890,
    "tcp_enabled": false,
    "max_connections": 10,
    "timeout": 300,
    "auto_restart": true,
    "log_level": "info"
  }
}
```

## Migration Path

1. **v13.4 Users**: Symlink `goxel-headless` → `goxel --headless`
2. **Scripts**: Automatic detection and adaptation
3. **MCP Server**: Update to use new unified binary
4. **Documentation**: Clear migration guides

## Conclusion

The v14.6 unified headless design provides a cleaner, more maintainable architecture while delivering significant performance improvements through the daemon model. This approach aligns with modern CLI tool design (like Docker, Git) where a single binary provides multiple operation modes.