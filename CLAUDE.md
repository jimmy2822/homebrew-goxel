# CLAUDE.md - Goxel v15 Daemon Architecture 3D Voxel Editor

## Project Overview

Goxel is a cross-platform 3D voxel editor written primarily in C99 with some C++ components. **Version 15.0.0 introduces a high-performance daemon architecture**, enabling concurrent voxel editing operations through JSON-RPC protocol for enterprise deployments, automation, and application integration.

**⚠️ v15.0 Status: BETA - Mutex Protection Applied (August 2, 2025)**
- **Core Implementation**: All JSON-RPC methods implemented and tested
- **Socket Communication**: ✅ Fixed! Daemon now accepts multiple connections properly
- **Mutex Protection**: ✅ Single-project daemon with exclusive locking implemented
- **Memory Management**: ✅ No crashes, proper state cleanup implemented
- **Automatic Cleanup**: ✅ Idle projects cleaned up after 5 minutes
- **Cross-Platform**: ✅ macOS, ✅ Linux (Ubuntu/CentOS), ✅ Windows (WSL2)
- **Enterprise Features**: systemd/launchd services, health monitoring, logging
- **Homebrew Package**: Complete formula with `brew install jimmy/goxel/goxel`
- **Known Issue**: Daemon hangs after processing requests (architectural limitation)
- **Production Status**: Not ready - requires restart between operations

**Core Features:**
- 24-bit RGB colors with alpha channel support
- Unlimited scene size and undo buffer
- Multi-layer support with visibility controls
- Marching cube rendering and procedural generation
- Ray tracing and advanced lighting
- Multiple export formats (OBJ, PLY, PNG, Magica Voxel, Qubicle, STL, etc.)

**v15.0 Daemon Architecture:**
- **JSON-RPC Server**: Unix socket-based server with full protocol compliance
- **Concurrent Processing**: Worker pool architecture for parallel operations
- **MCP Integration**: Seamless integration with Model Context Protocol tools
- **Headless Rendering**: OSMesa-based image generation with software fallback
- **Language Agnostic**: Python, JavaScript, Go, and any JSON-RPC client supported
- **Container Ready**: Optimized for Docker, Kubernetes, and microservices

**Official Website:** https://goxel.xyz  
**Documentation:** See `docs/v15/` directory  
**Platforms:** Linux, BSD, Windows, macOS + Enterprise Server Support

## Architecture Overview

### Core Data Structures
1. **Blocks** (`block_t`): Store 16³ voxels with copy-on-write mechanism
2. **Block Data** (`block_data_t`): Actual voxel data, copied only when modified
3. **Volumes** (`volume_t`): Collections of blocks, also using copy-on-write
4. **Layers** (`layer_t`): Volumes with additional attributes
5. **Images** (`image_t`): Collections of layers with undo history

### Key Systems
- **Rendering System**: Deferred rendering using `render_xxx` functions
- **Tool System**: Modular tools for different voxel operations
- **Asset System**: Assets embedded via `src/assets.inl`
- **GUI System**: Dear ImGui with custom widgets
- **File Format System**: Pluggable import/export system

## Technology Stack

### Core Languages
- **C99**: Main codebase with GNU extensions
- **C++17**: Some components (ImGui, mesh optimization)
- **GLSL**: Shaders for rendering
- **JavaScript**: Scripting support via QuickJS

### Dependencies
- **OpenGL**: 3D rendering
- **GLFW3**: Window management
- **Dear ImGui**: GUI framework
- **OSMesa**: Headless rendering
- **stb libraries**: Image I/O, fonts
- **quickjs**: JavaScript engine

## Directory Structure

```
/
├── src/                    # Main source code
│   ├── daemon/            # Daemon architecture components
│   │   ├── daemon_main.c  # Daemon entry point
│   │   ├── json_rpc.c/h   # JSON-RPC protocol  
│   │   ├── socket_server.c # Unix socket server
│   │   └── worker_pool.c  # Concurrent workers
│   ├── headless/          # Headless rendering
│   │   ├── goxel_headless_api.c # Core API
│   │   └── render_headless.c    # OSMesa rendering
│   ├── tools/             # Voxel editing tools
│   ├── gui/               # GUI panels
│   └── formats/           # Import/export handlers
├── data/                  # Assets (fonts, icons, palettes)
├── ext_src/               # External dependencies
├── docs/                  # Documentation
├── tests/                 # Test suites
└── homebrew-goxel/        # Homebrew formula
```

## Quick Start

### 📦 Homebrew Installation (Recommended)
```bash
# Install Goxel v15.0 Daemon
brew tap jimmy/goxel file:///path/to/goxel/homebrew-goxel
brew install jimmy/goxel/goxel

# Start daemon service
brew services start goxel

# Verify installation
goxel-daemon --version
brew services status goxel

# Test with example client
python3 /opt/homebrew/share/goxel/examples/homebrew_test_client.py
```

### Homebrew Development Commands
```bash
# Package for Homebrew (creates tarball and updates formula)
git archive --format=tar.gz --prefix=goxel-15.0.0/ -o homebrew-goxel/goxel-15.0.0.tar.gz HEAD
shasum -a 256 homebrew-goxel/goxel-15.0.0.tar.gz

# Reinstall from local formula (for testing)
brew reinstall --build-from-source Formula/goxel.rb

# Link binaries if needed
brew link goxel
```

### Manual Build
```bash
# Dependencies
brew install scons glfw tre  # macOS
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev  # Ubuntu

# Build daemon
scons daemon=1

# Run daemon
./goxel-daemon --foreground --socket /tmp/goxel.sock
```

### Daemon Testing
```bash
# Start daemon with logging
./goxel-daemon --foreground --socket /tmp/goxel.sock > daemon.log 2>&1 &
DAEMON_PID=$!

# View logs
tail -f daemon.log

# Run tests
make -C tests/integration
python3 scripts/run_benchmarks.py

# Stop daemon
kill $DAEMON_PID
```

## Development Guidelines

### Code Style
- **Indentation**: 4 spaces, no tabs
- **Line width**: 80 characters maximum
- **Naming**: `snake_case` for functions and variables
- **Braces**: K&R style, except functions (next line)
- **Standards**: C99 with GNU extensions

### Git Workflow
- **Branch**: `master` is main development branch
- **Commits**: <50 chars summary, no watermarks
- **CLA**: Contributors must sign Contributor License Agreement

### Common Tasks

#### Adding a New Tool
1. Create `src/tools/new_tool.c`
2. Register in `src/tools.c`
3. Add GUI panel in `src/gui/tools_panel.c`

#### Adding Export Format
1. Create `src/formats/new_format.c`
2. Register in `src/file_format.c`
3. Add export UI in `src/gui/export_panel.c`

## JSON-RPC API

The daemon exposes 15 methods via JSON-RPC 2.0:

### Core Methods
- `create_project` - Create new voxel project
- `open_file` - Open existing project/model
- `save_file` - Save current project
- `export_file` - Export to various formats
- `render_scene` - Render to image

### Voxel Operations
- `add_voxels` - Add voxels with brush
- `remove_voxels` - Remove voxels
- `paint_voxels` - Paint existing voxels
- `flood_fill` - Fill connected voxels
- `procedural_shape` - Generate shapes

### Layer Management
- `new_layer` - Create layer
- `delete_layer` - Remove layer
- `list_layers` - Get layer info
- `merge_layers` - Combine layers
- `set_layer_visibility` - Show/hide layer

See `docs/v15/daemon_api_reference.md` for complete API documentation.

## GUI Coordinate System

### Visual Grid Plane
- **Plane Type**: XZ horizontal plane (ground)
- **CLI Coordinates**: Y = -16
- **GUI Display**: Y = 0
- **Mapping**: GUI_Y = CLI_Y + 16

## Recent Updates

### Mutex Protection Implemented (August 2, 2025)
Added comprehensive mutex protection and automatic cleanup:

**Implemented Solutions:**
- ✅ Project mutex system with exclusive locking
- ✅ Request serialization to prevent concurrent access
- ✅ Automatic cleanup thread for idle projects (5-minute timeout)
- ✅ Thread-safe project state tracking
- ✅ macOS-compatible implementation (trylock with retry)

**Files Added:**
- `src/daemon/project_mutex.h` - Mutex system interface
- `src/daemon/project_mutex.c` - Mutex implementation

**Result**: Prevents concurrent project operations, automatic resource cleanup.

### Socket Fix Applied (August 2, 2025)
Fixed daemon socket handling to accept multiple connections:

**Implemented Solutions:**
- ✅ Protocol detection now happens in separate thread
- ✅ Accept thread no longer blocks after first connection
- ✅ JSON monitor threads properly handle client lifecycle
- ✅ Multiple connections now supported

**Result**: Daemon accepts and handles multiple connections correctly.

### Memory Fix Applied (August 2, 2025)
Applied fixes to prevent memory crashes:

**Implemented Solutions:**
- ✅ Complete state cleanup before creating new projects
- ✅ Simplified project creation to avoid shared references
- ✅ Added safety checks in voxel operations
- ✅ Fixed use-after-free errors
- ✅ Prevented segmentation faults

**Result**: Daemon no longer crashes with memory errors.

### Protocol Handler Improvements (January 31, 2025)
- ✅ Protocol detection to distinguish JSON-RPC from binary protocols
- ✅ Separate handlers for binary and JSON clients
- ✅ JSON monitor thread handles all I/O independently

## Known Issues

### Daemon Hangs After Processing Requests
The daemon becomes unresponsive after processing requests, requiring restart between operations.

**Root Cause**: Architectural mismatch between single-user Goxel design and multi-request daemon model. While socket, memory, and mutex issues have been fixed, the fundamental communication architecture needs refactoring.

**Impact**: 
- First request: ✅ Processes successfully
- Subsequent requests: ❌ Daemon unresponsive
- Mutex protection: ✅ Works correctly
- Memory management: ✅ No crashes
- Socket handling: ✅ Accepts multiple connections

**Status (August 2, 2025)**:
- ✅ Socket fix complete - daemon accepts multiple connections
- ✅ Memory issues resolved - no crashes
- ✅ Mutex protection implemented - prevents concurrent access
- ✅ Automatic cleanup thread - cleans idle projects
- ❌ Request handling - daemon hangs after first operation

**Current Workarounds**:
1. Restart daemon between requests (not suitable for production)
2. Use daemon for single operations only
3. Monitor with cleanup thread for automatic resource management

**Documentation**: 
- **Socket Fix Applied**: `SOCKET_FIX_REPORT.md` - Socket handling fix details
- **Memory Fix Applied**: `QUICK_FIX_FINAL_REPORT.md` - Memory fix implementation
- **Mutex Implementation**: `src/daemon/project_mutex.h/c` - Mutex protection system
- **Quick Fix Guide**: `docs/daemon-quick-fix-guide.md` - How to apply the fixes
- **Full Solution**: `docs/daemon-memory-fix-implementation.md` - Complete single-project daemon implementation
- **Technical Analysis**: `docs/daemon-memory-architecture-analysis.md` - Root cause analysis

**Recommended Approach**:
- **Immediate**: Use with understanding of single-operation limitation
- **v15.0**: ✅ Complete architectural refactor for true multi-tenant support

## Performance

v15.0 with mutex protection shows:
- ✅ Protocol detection and routing working efficiently
- ✅ Socket communication supports multiple connections
- ✅ Mutex protection prevents race conditions
- ✅ Automatic cleanup reduces memory usage over time
- ✅ First request processes at full speed
- ❌ Subsequent requests blocked by architectural issue

---

**Last Updated**: August 2, 2025  
**Version**: 15.0.0-beta  
**Status**: ⚠️ **BETA - Mutex protection implemented, single-operation limitation remains**

## Documentation Index

For comprehensive documentation, see **[docs/INDEX.md](docs/INDEX.md)** which provides:
- 🚨 Critical issues and fixes
- 📘 Core documentation and guides  
- 📁 Version-specific documentation (v15.0)
- 🔍 Analysis reports and improvement plans
- 📝 Templates and standards

Total documentation files: 66+ organized by category