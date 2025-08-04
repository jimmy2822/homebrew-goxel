# CLAUDE.md - Goxel-Daemon v15.0 JSON-RPC Server

## Project Overview

Goxel-daemon is a Unix socket JSON-RPC server for the Goxel voxel editor, enabling programmatic control and automation. Written in C99.

**⚠️ v15.0.1 Status: BETA - Export/Render Functions Fixed**
- **JSON-RPC**: ✅ All 15 methods implemented and functional
- **TDD Tests**: ✅ 271 total tests (267 passing, 4 integration test failures)
- **GitLab CI**: ✅ Automated TDD testing on every push
- **Memory Safety**: ✅ Fixed double-free bug in JSON serialization
- **First Request**: ✅ Works correctly 
- **Connection Reuse**: ⚠️ Not supported - one request per connection
- **Workaround**: ✅ Create new connection for each request (standard usage)
- **Export Formats**: ⚠️ Only .gox format supported in daemon mode (both save_project and export_model)
- **Rendering**: ✅ Fixed in PR #6 - now produces correct images with OSMesa

**Recent Fixes (v15.0.1):**
- Fixed TDD test method names (save_file → save_project, export_file → export_model) in PR #5
- Fixed daemon render functionality to produce actual images instead of gray output in PR #6
- Added real file operation integration tests that verify actual functionality
- Simplified export implementation to properly support .gox format

**Daemon Features:**
- Unix socket communication
- JSON-RPC 2.0 protocol
- Voxel manipulation API
- Layer management
- File import/export automation

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
brew install jimmy/goxel/goxel  # Installs daemon-enabled version
brew services start goxel        # Starts daemon at /opt/homebrew/var/run/goxel/goxel.sock
```

**Note:** The Homebrew package includes the daemon functionality. The daemon socket is created at `/opt/homebrew/var/run/goxel/goxel.sock`.

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
- `goxel.save_project` - Save project: [path] (⚠️ only .gox format in daemon mode)
- `goxel.export_model` - Export to format: [path, format] (⚠️ only .gox format in daemon mode)
- `goxel.get_project_info` - Get project metadata: []

**Voxel Operations**
- `goxel.add_voxel` - Add single voxel: [x, y, z, r, g, b, a]
- `goxel.remove_voxel` - Remove single voxel: [x, y, z]
- `goxel.paint_voxel` - Change voxel color: [x, y, z, r, g, b, a]
- `goxel.get_voxel` - Query voxel: [x, y, z]
- `goxel.fill_selection` - Fill selection with color: [r, g, b, a]
- `goxel.render_scene` - Render to image: [output_path, width, height]

**Layer Management**
- `goxel.list_layers` - Get all layers: []
- `goxel.create_layer` - Create layer: [name]
- `goxel.delete_layer` - Delete layer: [id]
- `goxel.set_active_layer` - Switch layer: [id]

### Example
```python
import json
import socket

# Default socket path when installed via Homebrew
SOCKET_PATH = "/opt/homebrew/var/run/goxel/goxel.sock"

# For manual testing or custom installations
# SOCKET_PATH = "/tmp/goxel.sock"

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(SOCKET_PATH)

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
# Standard usage - new connection per request
SOCKET_PATH = "/opt/homebrew/var/run/goxel/goxel.sock"  # Homebrew default
# SOCKET_PATH = "/tmp/goxel.sock"  # For manual testing

for i in range(10):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(SOCKET_PATH)
    # ... send request ...
    sock.close()
```

### Export Format Limitations
In daemon mode, export functionality is limited:

1. **save_project**: Only supports .gox format (native Goxel format)
2. **export_model**: Only supports .gox format in daemon mode (other formats return error)
3. **Rendering**: Works correctly with OSMesa dependency (fixed in v15.0.1)

### Test Coverage Issues
- **Method Names**: TDD tests previously used wrong method names (fixed in v15.0.1)
- **Mock vs Real**: TDD tests use mock implementations, not actual file operations
- **Integration Tests**: Real file operation tests in `tests/test_daemon_file_operations.c`
- **Connection Reuse**: 4 integration tests expect connection reuse which daemon doesn't support

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

### Branch Strategy
- **main**: Primary development branch (used on both GitHub and GitLab)
- **develop**: GitLab CI/CD testing branch (optional)

To push daemon changes:
```bash
git push origin main    # Push to GitHub
git push gitlab main    # Push to GitLab (triggers CI)
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
- **Test Coverage**: 271 total tests across 3 test suites
  - `example_voxel_tdd`: 19 tests (100% passing)
  - `test_daemon_jsonrpc_tdd`: 219 tests (100% passing) - Method names fixed in v15.0.1
  - `test_daemon_integration_tdd`: 33 tests (87.9% passing, 4 known failures)
  - `test_daemon_file_operations`: 12 tests (100% passing) - Real file operations
- **Memory Safety**: Fixed use-after-free bugs
- **Global State**: Centralized management
- **Known Issues**: 
  - 4 integration tests fail because they expect connection reuse (daemon design: one request per connection)
  - TDD tests use mock implementations - real tests in `tests/test_daemon_file_operations.c`

### Testing
```bash
# Run TDD tests (required before commits)
cd tests/tdd && make all && ./test_daemon_jsonrpc_tdd

# Run integration tests for file operations
cd tests && ./run_file_ops_test.sh

# Run daemon manually
./goxel-daemon --foreground --socket /tmp/test.sock

# Check if working (adjust socket path as needed)
python3 -c "
import socket, json
SOCKET = '/tmp/test.sock'  # or '/opt/homebrew/var/run/goxel/goxel.sock' for Homebrew
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(SOCKET)
s.send(json.dumps({'jsonrpc':'2.0','method':'goxel.create_project','params':['Test',16,16,16],'id':1}).encode()+b'\\n')
print(s.recv(4096))
s.close()  # Important: close after each request
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

**Version**: 15.0.1  
**Updated**: January 2025  
**Status**: Beta - Core Functionality Working

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