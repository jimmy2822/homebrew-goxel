# CLAUDE.md - Goxel v15.0 Daemon Architecture 3D Voxel Editor

## Project Overview

Goxel is a cross-platform 3D voxel editor written primarily in C99. **Version 15.0 introduces a daemon architecture** with JSON-RPC protocol support for automation and integration.

**⚠️ v15.0 Status: DEVELOPMENT - Single Operation Per Session**
- **JSON-RPC**: ✅ All 15 methods implemented and functional
- **First Request**: ✅ Works correctly 
- **Subsequent Requests**: ❌ Daemon hangs (architectural limitation)
- **Production Ready**: ❌ Requires architectural refactor

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

### Project Management
- `goxel.create_project` - params: [name, width, height, depth]
- `goxel.open_file` - params: [path]
- `goxel.save_file` - params: [path]
- `goxel.export_file` - params: [path, format]

### Voxel Operations  
- `goxel.add_voxels` - params: {voxels: [{position, color}]}
- `goxel.remove_voxels` - params: {voxels: [{position}]}
- `goxel.paint_voxels` - params: {voxels: [{position, color}]}

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

### Single Operation Per Session
The daemon can only handle one request per session due to architectural constraints:

1. **First request**: ✅ Processes correctly
2. **Second request**: ❌ Daemon becomes unresponsive
3. **Workaround**: Restart daemon between operations

**This is a fundamental limitation that requires architectural changes to fix.**

### Documentation
- Memory architecture analysis: `docs/daemon-memory-architecture-analysis.md`
- Implementation guide: `docs/daemon-memory-fix-implementation.md` 
- Quick fixes: `docs/daemon-quick-fix-guide.md`

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
**Updated**: January 2025  
**Status**: Development - Not Production Ready

## Development Philosophy

### TDD is Mandatory
All new features and bug fixes MUST be developed using Test-Driven Development. This ensures:
- Clear goals before coding
- No wasted time on unnecessary features  
- High confidence in code quality
- Built-in regression testing

**No code will be merged without accompanying tests.**