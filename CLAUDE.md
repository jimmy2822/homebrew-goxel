# CLAUDE.md - Goxel v14 Daemon Architecture 3D Voxel Editor

## Project Overview

Goxel is a cross-platform 3D voxel editor written primarily in C99 with some C++ components. **Version 14.0.0 introduces a high-performance daemon architecture**, enabling concurrent voxel editing operations through JSON-RPC protocol for enterprise deployments, automation, and application integration.

**⚠️ v14.0 Status: BETA - Core Memory Issue Found (January 31, 2025)**
- **Core Implementation**: All JSON-RPC methods implemented and tested
- **Socket Communication**: Protocol detection and handling working correctly
- **Performance**: First request performs well, subsequent requests crash
- **Cross-Platform**: ✅ macOS, ✅ Linux (Ubuntu/CentOS), ✅ Windows (WSL2)
- **Enterprise Features**: systemd/launchd services, health monitoring, logging
- **Homebrew Package**: Complete formula with `brew install jimmy/goxel/goxel`
- **Known Issue**: Daemon crashes on second request due to Goxel core memory management bug

**Core Features:**
- 24-bit RGB colors with alpha channel support
- Unlimited scene size and undo buffer
- Multi-layer support with visibility controls
- Marching cube rendering and procedural generation
- Ray tracing and advanced lighting
- Multiple export formats (OBJ, PLY, PNG, Magica Voxel, Qubicle, STL, etc.)

**v14.0 Daemon Architecture:**
- **JSON-RPC Server**: Unix socket-based server with full protocol compliance
- **Concurrent Processing**: Worker pool architecture for parallel operations
- **MCP Integration**: Seamless integration with Model Context Protocol tools
- **Headless Rendering**: OSMesa-based image generation with software fallback
- **Language Agnostic**: Python, JavaScript, Go, and any JSON-RPC client supported
- **Container Ready**: Optimized for Docker, Kubernetes, and microservices

**Official Website:** https://goxel.xyz  
**Documentation:** See `docs/v14/` directory  
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
# Install Goxel v14.0 Daemon
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
git archive --format=tar.gz --prefix=goxel-14.0.0/ -o homebrew-goxel/goxel-14.0.0.tar.gz HEAD
shasum -a 256 homebrew-goxel/goxel-14.0.0.tar.gz

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

See `docs/v14/daemon_api_reference.md` for complete API documentation.

## GUI Coordinate System

### Visual Grid Plane
- **Plane Type**: XZ horizontal plane (ground)
- **CLI Coordinates**: Y = -16
- **GUI Display**: Y = 0
- **Mapping**: GUI_Y = CLI_Y + 16

## Recent Fixes (January 31, 2025)

### Protocol Handler Conflicts Resolved
Fixed daemon crashes after 3-4 operations by implementing proper protocol isolation:

**Implemented Solutions:**
- ✅ Protocol detection to distinguish JSON-RPC from binary protocols
- ✅ Separate handlers for binary and JSON clients
- ✅ JSON monitor thread handles all I/O independently
- ✅ Removed problematic async processing structures
- ✅ Synchronous response handling ensures stable connections

**Result**: First request now works reliably with proper response handling.

## Known Issues

### Critical: Daemon Crashes on Second Request
The daemon currently crashes when processing a second `create_project` request due to a memory management issue in the Goxel core:

**Root Cause**: In `goxel_core_create_project()`, both `ctx->image` and global `goxel.image` point to the same memory. When creating a new project, the function deletes the old image, causing a use-after-free error on subsequent calls.

**Impact**: 
- First request: ✅ Works perfectly
- Subsequent requests: ❌ Daemon crashes with SIGABRT

**Workaround**: Restart daemon between requests (not suitable for production)

**Fix Required**: The Goxel core needs proper separation between context-specific and global state management.

## Performance

When operational, v14.0 shows significant performance improvements:
- Protocol detection and routing working efficiently
- Socket communication optimized
- First request processes correctly with proper response handling
- Full performance benefits blocked by memory management issue

---

**Last Updated**: January 31, 2025  
**Version**: 14.0.0-beta  
**Status**: ⚠️ **BETA - Critical memory management issue prevents production use**