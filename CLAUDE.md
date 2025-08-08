# CLAUDE.md - Goxel Daemon v15.2

## 📋 Project Overview

Goxel-daemon is a high-performance Unix socket JSON-RPC server for the Goxel voxel editor, enabling programmatic control and automation of 3D voxel operations. Built with C99 for maximum performance and reliability.

**🎯 Current Status: STABLE**
- ✅ **Connection Reuse**: Full JSON-RPC persistent connections
- ✅ **25 JSON-RPC Methods**: Complete API coverage
- ✅ **Script Execution**: Full QuickJS integration with error handling
- ✅ **Integration Tests**: 17/17 passing (100% success rate)
- ✅ **OSMesa Rendering**: Complete offscreen rendering pipeline
- ✅ **File Operations**: .gox, .obj, .png, and more formats
- ✅ **Production Ready**: Memory safe, thread-safe, high performance

**🌐 Official Website**: https://goxel.xyz

---

## 🚀 Quick Start

### Installation
```bash
# macOS (Homebrew)
brew tap jimmy/goxel
brew install jimmy/goxel/goxel
brew services start goxel  # Starts daemon automatically

# Build from Source
scons daemon=1
```

### Basic Usage
```python
import socket, json

# Connect to daemon
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/opt/homebrew/var/run/goxel/goxel.sock")  # Homebrew path

# Create project and add voxel
requests = [
    {"jsonrpc": "2.0", "method": "goxel.create_project", "params": ["MyProject", 32, 32, 32], "id": 1},
    {"jsonrpc": "2.0", "method": "goxel.add_voxel", "params": [16, 16, 16, 255, 0, 0, 255], "id": 2},
    {"jsonrpc": "2.0", "method": "goxel.render_scene", "params": ["output.png", 800, 600], "id": 3}
]

for request in requests:
    sock.send(json.dumps(request).encode() + b"\n")
    response = json.loads(sock.recv(4096).decode())
    print(f"Response: {response}")

sock.close()
```

---

## 🔧 Core Features

### JSON-RPC API (25 Methods)  
Complete programmatic control of voxel operations:

| Category | Methods | Count | Description |
|----------|---------|-------|-------------|
| **Project** | `create_project`, `load_project`, `save_project`, `export_model`, `get_status` | 5 | Project lifecycle management |
| **Voxels** | `add_voxel`, `remove_voxel`, `get_voxel`, `paint_voxels`, `flood_fill`, `procedural_shape`, `batch_operations` | 7 | Individual and batch voxel operations |
| **Layers** | `list_layers`, `create_layer`, `delete_layer`, `merge_layers`, `set_layer_visibility` | 5 | Layer management and organization |
| **Analysis** | `get_voxels_region`, `get_layer_voxels`, `get_bounding_box`, `get_color_histogram`, `find_voxels_by_color`, `get_unique_colors` | 6 | Data extraction and analysis |
| **Rendering** | `render_scene` | 1 | Image generation |
| **Scripting** | `execute_script` | 1 | JavaScript automation |

### Connection Management
- **✅ Persistent Connections**: Multiple requests per connection
- **✅ High Performance**: Eliminates reconnection overhead
- **✅ Thread Safe**: Concurrent client support
- **✅ Backward Compatible**: Single-request patterns still work

### File Format Support
| Format | Import | Export | Status |
|--------|--------|--------|---------|
| `.gox` | ✅ | ✅ | Native format |
| `.obj` | ✅ | ✅ | Wavefront OBJ |
| `.vox` | ✅ | ✅ | MagicaVoxel |
| `.png` | ✅ | ✅ | Image slices |
| `.ply` | ✅ | ✅ | Stanford PLY |

### Rendering Capabilities
- **OSMesa Integration**: Complete offscreen rendering
- **Multiple Formats**: PNG, JPEG, BMP output
- **Configurable Resolution**: Any size up to system limits
- **Camera Controls**: Preset angles and custom positioning

---

## 📡 API Reference

### Project Management
```python
# Create new project
{"jsonrpc": "2.0", "method": "goxel.create_project", "params": ["ProjectName", width, height, depth], "id": 1}

# Save/Export
{"jsonrpc": "2.0", "method": "goxel.save_project", "params": ["path/to/file.gox"], "id": 2}
{"jsonrpc": "2.0", "method": "goxel.export_model", "params": ["path/to/file.obj", "obj"], "id": 3}
```

### Voxel Operations
```python
# Add voxel with RGBA color
{"jsonrpc": "2.0", "method": "goxel.add_voxel", "params": [x, y, z, r, g, b, a], "id": 4}

# Query voxel
{"jsonrpc": "2.0", "method": "goxel.get_voxel", "params": [x, y, z], "id": 5}

# Batch operations for better performance
{"jsonrpc": "2.0", "method": "goxel.batch_operations", "params": [{"operations": [...]}], "id": 6}

# Color analysis
{"jsonrpc": "2.0", "method": "goxel.get_color_histogram", "params": [], "id": 7}
```

### Layer Operations
```python
# Create and manage layers
{"jsonrpc": "2.0", "method": "goxel.create_layer", "params": ["LayerName"], "id": 8}
{"jsonrpc": "2.0", "method": "goxel.list_layers", "params": [], "id": 9}
```

### Rendering
```python
# Render scene to PNG
{"jsonrpc": "2.0", "method": "goxel.render_scene", "params": ["output.png", 800, 600], "id": 6}
```

### Script Execution
```python
# Execute JavaScript code
{"jsonrpc": "2.0", "method": "goxel.execute_script", "params": {"script": "2 + 2"}, "id": 10}

# Execute script with error handling
{"jsonrpc": "2.0", "method": "goxel.execute_script", "params": {"script": "throw new Error('test')"}, "id": 11}
# Returns: {"success": false, "code": -1, "message": "Script execution failed with code -1"}

# Execute script from file
{"jsonrpc": "2.0", "method": "goxel.execute_script", "params": {"path": "/path/to/script.js"}, "id": 12}

# Execute with custom timeout
{"jsonrpc": "2.0", "method": "goxel.execute_script", "params": {"script": "/* long running */", "timeout_ms": 5000}, "id": 13}
```

### Connection Patterns

**✅ Recommended: Persistent Connection**
```python
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(SOCKET_PATH)

for i in range(100):  # Multiple operations on same connection
    request = {"jsonrpc": "2.0", "method": "goxel.add_voxel", "params": [i, i, i, 255, 0, 0, 255], "id": i}
    sock.send(json.dumps(request).encode() + b"\n")
    response = sock.recv(4096)

sock.close()
```

**Legacy: New Connection Per Request**
```python
for i in range(100):  # Still supported but inefficient
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(SOCKET_PATH)
    # ... single request ...
    sock.close()
```

---

## 💾 Installation & Setup

### Build Requirements

**macOS:**
```bash
brew install scons glfw tre
```

**Linux:**
```bash
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev
```

### OSMesa Setup (Required for Rendering)

**Automated Installation (Recommended):**
```bash
chmod +x scripts/install_osmesa.sh
./scripts/install_osmesa.sh

# Verify
PKG_CONFIG_PATH="/opt/homebrew/opt/osmesa/lib/pkgconfig:$PKG_CONFIG_PATH" pkg-config --exists osmesa
```

**Build with OSMesa:**
```bash
PKG_CONFIG_PATH="/opt/homebrew/opt/osmesa/lib/pkgconfig:$PKG_CONFIG_PATH" scons daemon=1
```

### Deployment Options

**Development:**
```bash
./goxel-daemon --foreground --socket /tmp/goxel.sock
```

**Production (macOS Homebrew):**
```bash
brew services start goxel
# Socket created at: /opt/homebrew/var/run/goxel/goxel.sock
```

**Custom Configuration:**
```bash
./goxel-daemon --help  # See all options
```

---

## 🧪 Development

### Test-Driven Development
All features are developed using TDD methodology:

```bash
# Run all tests
./tests/run_tdd_tests.sh

# Integration tests
./tests/run_file_ops_test.sh
```

**Test Coverage:**
- **TDD Tests**: 271 tests across 3 suites
- **Integration Tests**: 12/12 passing (file operations)
- **CI/CD**: Automated testing on GitLab

### Git Workflow
```bash
# GitHub (main repository)
git remote add origin git@github.com:jimmy2822/goxel.git

# GitLab (CI/CD)
git remote add gitlab git@ssh.raiden.me:jimmy2822/goxel-daemon.git

# Push to both
git push origin main
git push gitlab main  # Triggers CI
```

### Code Standards
- **Language**: C99 with GNU extensions
- **Style**: 4 spaces, 80 chars, snake_case
- **Testing**: TDD mandatory for all new features
- **Memory**: Leak-free, thread-safe design

---

## 🔍 Troubleshooting

### Common Issues

**OSMesa Not Found:**
```bash
# Problem: Build fails with OSMesa warnings
# Solution: Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH="/opt/homebrew/opt/osmesa/lib/pkgconfig:$PKG_CONFIG_PATH"
```

**Connection Timeout:**
```bash
# Problem: Client connections timeout
# Solution: Check socket path and daemon status
ls -la /opt/homebrew/var/run/goxel/goxel.sock  # Homebrew
ps aux | grep goxel-daemon                      # Check if running
```

**Rendering Issues:**
```bash
# Problem: PNG files are gray/empty
# Solution: Verify OSMesa installation
./goxel-daemon --test-render  # Test rendering pipeline
```

### Getting Help
- **Issues**: https://github.com/jimmy2822/goxel/issues
- **Documentation**: Check `docs/` directory
- **CI Status**: GitLab pipeline logs

---

## 📝 Version Information

**Version**: 15.2  
**Release Date**: August 4, 2025  
**Status**: Stable Production Release

### Recent Improvements (v15.2)
- **🎉 Connection Reuse**: Full persistent connection support
- **⚡ Performance**: 10-100x improvement for batch operations
- **🔧 Compatibility**: Maintains backward compatibility
- **✅ Stability**: All integration tests passing

### System Requirements
- **Runtime**: OSMesa 8.0+, libpng 1.6+
- **Build**: C99 compiler, SCons, pkg-config
- **Platform**: macOS 10.15+, Linux (Ubuntu 18.04+)

### Dependencies
```bash
# Runtime libraries (automatically linked)
- libOSMesa.8.dylib     # Offscreen OpenGL rendering (OSMesa 8.0+)
- libpng16.16.dylib     # PNG image generation  
- OpenGL.framework      # OpenGL context (macOS)
- libSystem.B.dylib     # System calls
- libc++.1.dylib        # C++ standard library
- libobjc.A.dylib       # Objective-C runtime (macOS)
```

---

**🚀 Goxel Daemon v15.2 - High-Performance Voxel Automation**  
*Empowering developers to build amazing voxel applications*