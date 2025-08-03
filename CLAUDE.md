# CLAUDE.md - Goxel v15.0 Daemon Architecture 3D Voxel Editor

## Project Overview

Goxel is a cross-platform 3D voxel editor written primarily in C99. **Version 15.0 introduces a daemon architecture** with JSON-RPC protocol support for automation and integration.

**⚠️ v15.0 Status: DEVELOPMENT - Single Request Per Connection**
- **JSON-RPC**: ✅ All 15 methods implemented and functional
- **TDD Tests**: ✅ 217 tests, 100% passing (test_daemon_jsonrpc_tdd.c)
- **Memory Safety**: ✅ Fixed use-after-free and global state issues
- **First Request**: ✅ Works correctly 
- **Subsequent Requests**: ❌ Connection reuse not supported
- **Production Ready**: ❌ Requires connection handling improvements

**Core Features:**
- 24-bit RGB colors with alpha channel
- Unlimited scene size and undo/redo
- Multi-layer support
- Multiple export formats (OBJ, PLY, PNG, Magica Voxel, STL, etc.)
- Procedural generation and ray tracing

**Official Website:** https://goxel.xyz

## Architecture

### Core Components
- **Voxel Engine**: Block-based storage with copy-on-write
- **Rendering**: OpenGL with deferred rendering pipeline
- **GUI**: Dear ImGui framework
- **Daemon**: Unix socket server with JSON-RPC 2.0

### Directory Structure
```
/
├── src/
│   ├── daemon/         # Daemon components
│   ├── tools/          # Voxel editing tools
│   ├── formats/        # Import/export formats
│   └── gui/            # UI panels
├── docs/               # Documentation
├── tests/              # Test suites
└── homebrew-goxel/     # macOS package
```

## Installation

### Homebrew (macOS)
```bash
brew tap jimmy/goxel
brew install jimmy/goxel/goxel
brew services start goxel
```

### Build from Source
```bash
# macOS
brew install scons glfw tre

# Linux
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev

# Build
scons daemon=1
```

## JSON-RPC API

The daemon supports these methods (array parameters):

### All 15 Methods Implemented

**Project Management**
- `goxel.create_project` - Create new project: [name, width, height, depth]
- `goxel.open_file` - Open file: [path] (supports .gox, .vox, .obj, .ply, .png, .stl)
- `goxel.save_file` - Save project: [path]
- `goxel.export_file` - Export to format: [path, format]
- `goxel.get_project_info` - Get project metadata: []

**Voxel Operations**
- `goxel.add_voxels` - Add voxels: {voxels: [{position, color}]}
- `goxel.remove_voxels` - Remove voxels: {voxels: [{position}]}
- `goxel.paint_voxels` - Change colors: {voxels: [{position, color}]}
- `goxel.get_voxel` - Query voxel: {position: [x, y, z]}
- `goxel.flood_fill` - Fill connected: {position, color}
- `goxel.procedural_shape` - Generate shape: {shape, size, position, color}

**Layer Management**
- `goxel.list_layers` - Get all layers: []
- `goxel.create_layer` - Create layer: [name]
- `goxel.delete_layer` - Delete layer: [id]
- `goxel.set_active_layer` - Switch layer: [id]

### Example
```python
import json
import socket

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/opt/homebrew/var/run/goxel/goxel.sock")

request = {
    "jsonrpc": "2.0",
    "method": "goxel.create_project",
    "params": ["Test", 16, 16, 16],
    "id": 1
}

sock.send(json.dumps(request).encode() + b"\n")
response = sock.recv(4096)
print(json.loads(response))
```

## Known Limitations

### Single Request Per Connection
The daemon currently only supports one request per connection:

1. **First request**: ✅ Processes correctly
2. **Second request**: ❌ Connection reuse not supported
3. **Workaround**: Create new connection for each request

```python
# Correct usage - new connection per request
for i in range(10):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect("/tmp/goxel.sock")
    # ... send request ...
    sock.close()
```

### Documentation
- Architecture improvements: `docs/daemon-architecture-improvements.md`
- Current status report: `docs/v15-daemon-status.md`
- Root cause analysis: `docs/daemon-abort-trap-fix.md`
- Memory architecture: `docs/daemon-memory-architecture-analysis.md`

## Development

### Code Style
- C99 with GNU extensions
- 4 spaces indentation
- 80 character line limit
- snake_case naming

### Test-Driven Development (TDD)

**We follow TDD best practices to avoid wasting time and ensure every line of code has a clear purpose.**

#### TDD Workflow
1. **Red** - Write a failing test first
2. **Green** - Write minimal code to pass the test
3. **Refactor** - Improve code quality while keeping tests passing

#### Quick Start
```bash
# Run all TDD tests
./tests/run_tdd_tests.sh

# Run specific TDD tests
cd tests/tdd
make
```

#### Writing New Features with TDD
```bash
# 1. Create test file
touch tests/tdd/test_new_feature.c

# 2. Write failing test
# 3. Run test to confirm it fails
# 4. Implement feature
# 5. Run test until it passes
# 6. Refactor if needed
```

#### TDD Resources
- Framework: `tests/tdd/tdd_framework.h`
- Examples: `tests/tdd/example_voxel_tdd.c`, `tests/tdd/test_daemon_jsonrpc_tdd.c`
- Guide: `tests/tdd/TDD_WORKFLOW.md`
- Quick Start: `tests/tdd/README.md`

#### TDD Implementation Status
- **JSON-RPC Methods**: All 15 methods implemented with full TDD
- **Test Coverage**: 217 tests, 100% passing
- **Memory Safety**: Fixed use-after-free bugs
- **Global State**: Centralized management
- **Stability**: All critical issues resolved

### Testing
```bash
# Run TDD tests (required before commits)
./tests/run_tdd_tests.sh

# Run daemon manually
./goxel-daemon --foreground --socket /tmp/test.sock

# Check if working
python3 -c "
import socket, json
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/test.sock')
s.send(json.dumps({'jsonrpc':'2.0','method':'goxel.create_project','params':['Test',16,16,16],'id':1}).encode()+b'\\n')
print(s.recv(4096))
"
```

---

**Version**: 15.0.0-dev  
**Updated**: February 2025  
**Status**: Development - Not Production Ready

## Development Philosophy

### TDD is Mandatory
All new features and bug fixes MUST be developed using Test-Driven Development. This ensures:
- Clear goals before coding
- No wasted time on unnecessary features  
- High confidence in code quality
- Built-in regression testing

**No code will be merged without accompanying tests.**