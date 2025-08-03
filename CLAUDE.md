# CLAUDE.md - Goxel v15.0 Daemon Architecture 3D Voxel Editor

## Project Overview

Goxel is a cross-platform 3D voxel editor written primarily in C99. **Version 15.0 introduces a daemon architecture** with JSON-RPC protocol support for automation and integration.

**⚠️ v15.0 Status: BETA - Memory Issues Fixed, Connection Reuse Partial**
- **JSON-RPC**: ✅ All 15 methods implemented and functional
- **TDD Tests**: ✅ 269 total tests (265 passing, 4 known failures in integration tests)
- **GitLab CI**: ✅ Automated TDD testing on every push
- **Memory Safety**: ✅ Fixed double-free bug in JSON serialization
- **First Request**: ✅ Works correctly 
- **Connection Reuse**: ⚠️ Intermittent - client disconnect detected after first response
- **Workaround**: ✅ Create new connection for each request (recommended)
- **Production Ready**: ❌ Connection reuse needs further investigation

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
├── homebrew-goxel/     # macOS package
└── .gitlab-ci.yml      # GitLab CI configuration
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

### Git Remotes
The project is hosted on both GitHub and GitLab:

```bash
# GitHub (main repository)
origin  git@github.com:jimmy2822/goxel.git

# GitLab (CI/CD and daemon development)
gitlab  git@ssh.raiden.me:jimmy2822/goxel-daemon.git
```

To push to both remotes:
```bash
git push origin main    # Push to GitHub
git push gitlab develop # Push to GitLab
```

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
# Run all TDD tests locally
./tests/run_tdd_tests.sh

# Run specific TDD tests
cd tests/tdd
make clean
make all
./example_voxel_tdd           # Basic voxel tests
./test_daemon_jsonrpc_tdd     # JSON-RPC protocol tests
./test_daemon_integration_tdd # Integration tests (has known failures)

# Generate JUnit report (like CI does)
./generate_junit_report.sh
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
- **Test Coverage**: 269 total tests across 3 test suites
  - `example_voxel_tdd`: 19 tests (100% passing)
  - `test_daemon_jsonrpc_tdd`: 217 tests (100% passing)
  - `test_daemon_integration_tdd`: 33 tests (87.9% passing, 4 known failures)
- **Memory Safety**: Fixed use-after-free bugs
- **Global State**: Centralized management
- **Known Issues**: Integration tests have connection lifecycle issues

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

### GitLab CI

**The project uses GitLab CI for automated testing. Every push triggers TDD tests.**

#### Checking CI Status
```bash
# Configure GitLab host (required for glab CLI)
export GITLAB_HOST=ssh.raiden.me

# List recent pipelines
glab ci list

# Check current branch CI status
glab ci status

# View specific pipeline details (use pipeline ID from list)
glab api projects/jimmy2822%2Fgoxel-daemon/pipelines/<PIPELINE_ID>

# Get job logs (use job ID from pipeline details)
glab api projects/jimmy2822%2Fgoxel-daemon/jobs/<JOB_ID>/trace

# Open pipeline in web browser
glab ci view <PIPELINE_ID> --web
```

#### CI Configuration
- **File**: `.gitlab-ci.yml`
- **Stages**: build, test
- **Tests**: TDD tests run automatically
- **Reports**: JUnit format test results
- **Documentation**: `tests/tdd/GITLAB_CI_SETUP.md`

---

**Version**: 15.0.0  
**Updated**: August 2025  
**Status**: Beta - Automated Testing via GitLab CI

## Development Philosophy

### TDD is Mandatory
All new features and bug fixes MUST be developed using Test-Driven Development. This ensures:
- Clear goals before coding
- No wasted time on unnecessary features  
- High confidence in code quality
- Built-in regression testing

**No code will be merged without accompanying tests.**

### Pre-commit Checklist
Before committing any changes:
1. **Run TDD tests**: `./tests/run_tdd_tests.sh`
2. **Run lint/typecheck** (if provided): Ask user for the command
3. **Check CI status**: Ensure GitLab CI passes
4. **Update CLAUDE.md**: If making significant changes

**IMPORTANT**: When completing tasks, always run lint and typecheck commands if they were provided. If you don't know the commands, ask the user and suggest adding them to this file.