# Goxel v13 Headless Developer Guide
**Version**: 13.0.0-phase6  
**Date**: 2025-01-23  
**Audience**: Developers, Contributors, System Integrators

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Build System](#build-system)
3. [Core Components](#core-components)
4. [Development Workflow](#development-workflow)
5. [Testing Framework](#testing-framework)
6. [Performance Optimization](#performance-optimization)
7. [Contributing Guidelines](#contributing-guidelines)
8. [Debugging and Profiling](#debugging-and-profiling)

## Architecture Overview

### System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Goxel v13 Headless                      │
├─────────────────────────────────────────────────────────────┤
│  CLI Interface        │        C API Bridge               │
│  ┌─────────────────┐  │  ┌─────────────────────────────┐   │
│  │ Command Parser  │  │  │ Public API Functions        │   │
│  │ Argument Validation│  │ Context Management          │   │
│  │ Help System     │  │  │ Error Handling             │   │
│  │ Output Formatting│  │  │ Thread Safety              │   │
│  └─────────────────┘  │  └─────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Core Engine                             │
│  ┌─────────────────┐  ┌─────────────────┐ ┌─────────────┐   │
│  │ Volume System   │  │ Layer Manager   │ │File Formats │   │
│  │ • Block-based   │  │ • Multi-layer   │ │• GOX        │   │
│  │ • Copy-on-Write │  │ • Visibility    │ │• VOX        │   │
│  │ • Compression   │  │ • Metadata      │ │• OBJ/PLY    │   │
│  └─────────────────┘  └─────────────────┘ └─────────────┘   │
│  ┌─────────────────┐  ┌─────────────────┐ ┌─────────────┐   │
│  │ Project Mgmt    │  │ Headless Render │ │Utilities    │   │
│  │ • Load/Save     │  │ • OSMesa        │ │• Math       │   │
│  │ • Undo/Redo     │  │ • Camera System │ │• Memory     │   │
│  │ • Metadata      │  │ • Image Output  │ │• Logging    │   │
│  └─────────────────┘  └─────────────────┘ └─────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                   Platform Layer                           │
│  ┌─────────────────┐  ┌─────────────────┐ ┌─────────────┐   │
│  │ Memory Mgmt     │  │ File I/O        │ │OpenGL       │   │
│  │ Threading       │  │ Path Handling   │ │OSMesa       │   │
│  │ Error Reporting │  │ Compression     │ │Image Codecs │   │
│  └─────────────────┘  └─────────────────┘ └─────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Separation of Concerns**: Clear boundaries between CLI, API, and core engine
2. **Copy-on-Write**: Efficient memory management for voxel data
3. **Platform Independence**: Cross-platform compatibility through abstraction
4. **Thread Safety**: Safe concurrent access to contexts and operations
5. **Error Resilience**: Comprehensive error handling and recovery

### Key Data Structures

#### Volume System
```c
// Core voxel storage using hierarchical blocks
typedef struct {
    block_data_t *data;     // 16³ voxel blocks
    int ref_count;          // Reference counting for COW
    uint64_t revision;      // Version for change detection
} block_t;

typedef struct {
    volume_t *volume;       // Main voxel data
    layer_properties_t props; // Visibility, name, etc.
    bool visible;
    uint8_t color[4];       // Layer tint color
} layer_t;

typedef struct {
    layer_t *layers;        // Array of layers
    int layer_count;
    int active_layer;
    image_history_t *history; // Undo/redo system
} image_t;
```

#### Context Management
```c
typedef struct {
    image_t *image;         // Current project
    camera_t camera;        // Rendering camera
    tool_state_t tool;      // Active tool state
    render_context_t *render; // Headless rendering
    error_state_t error;    // Last error information
    settings_t settings;    // User preferences
} goxel_core_context_t;
```

## Build System

### SCons Configuration

The build system uses SCons with conditional compilation:

```python
# Key build options
opts = Variables()
opts.Add(BoolVariable('headless', 'Build headless version', False))
opts.Add(BoolVariable('gui', 'Build GUI version', True))
opts.Add(BoolVariable('cli_tools', 'Build CLI tools', False))
opts.Add(BoolVariable('c_api', 'Build C API library', False))
opts.Add(EnumVariable('mode', 'Build mode', 'debug', allowed_values=('debug', 'release', 'profile')))

# Conditional source selection
if env['headless']:
    env.Append(CPPDEFINES=['GOXEL_HEADLESS=1'])
    sources.extend(headless_sources)
    
if env['cli_tools']:
    sources.extend(cli_sources)
    env.Program('goxel-cli', sources + ['src/headless/main_cli.c'])
```

### Build Targets

```bash
# Debug build (default)
scons

# Headless with CLI
scons headless=1 cli_tools=1

# Release build
scons mode=release headless=1 cli_tools=1

# C API shared library
scons c_api=1

# Clean build
scons -c
```

### Cross-Platform Dependencies

#### Linux/BSD
```bash
# Ubuntu/Debian
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev libosmesa6-dev

# Fedora/CentOS
sudo dnf install scons glfw-devel gtk3-devel libpng-devel mesa-libOSMesa-devel

# Arch Linux
sudo pacman -S scons glfw gtk3 libpng mesa
```

#### macOS
```bash
# Homebrew
brew install scons glfw tre libpng

# MacPorts
sudo port install scons glfw tre libpng
```

#### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw \
          mingw-w64-x86_64-libtre-git scons make \
          mingw-w64-x86_64-libpng
```

## Core Components

### Volume System Implementation

#### Block-Based Storage
```c
// 16³ block size for optimal memory usage
#define BLOCK_SIZE 16
#define BLOCK_VOLUME (BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE)

typedef struct {
    uint8_t voxels[BLOCK_VOLUME][4]; // RGBA for each voxel
    uint64_t checksum;               // For change detection
    bool dirty;                      // Needs serialization
} block_data_t;

typedef struct {
    block_data_t *data;
    int ref_count;                   // Copy-on-write reference counting
    uint64_t last_access;           // For LRU cache eviction
} block_t;
```

#### Copy-on-Write Implementation
```c
// When modifying a block, create copy if ref_count > 1
static block_data_t *block_get_writable(block_t *block) {
    if (block->ref_count > 1) {
        // Create copy for modification
        block_data_t *new_data = malloc(sizeof(block_data_t));
        memcpy(new_data, block->data, sizeof(block_data_t));
        
        block->ref_count--;
        block->data = new_data;
        block->ref_count = 1;
    }
    return block->data;
}
```

### Layer Management

#### Layer Operations
```c
// Layer creation with properties
layer_t *layer_create(const char *name, uint8_t color[4], bool visible) {
    layer_t *layer = calloc(1, sizeof(layer_t));
    layer->volume = volume_new();
    strncpy(layer->props.name, name, sizeof(layer->props.name) - 1);
    memcpy(layer->color, color, 4);
    layer->visible = visible;
    return layer;
}

// Layer blending for rendering
void layer_blend(layer_t *layers, int count, volume_t *output) {
    for (int i = 0; i < count; i++) {
        if (!layers[i].visible) continue;
        
        volume_blend(output, layers[i].volume, layers[i].color);
    }
}
```

### File Format System

#### Format Handler Interface
```c
typedef struct {
    const char *name;
    const char *extension;
    format_caps_t capabilities;
    
    // Function pointers for operations
    int (*load)(const char *path, image_t *image);
    int (*save)(const char *path, const image_t *image, export_options_t *opts);
    bool (*detect)(const char *path);
} format_handler_t;

// Format registration
static format_handler_t format_handlers[] = {
    {"Goxel", ".gox", FORMAT_CAP_READ | FORMAT_CAP_WRITE | FORMAT_CAP_LAYERS, 
     gox_load, gox_save, gox_detect},
    {"MagicaVoxel", ".vox", FORMAT_CAP_READ | FORMAT_CAP_WRITE,
     vox_load, vox_save, vox_detect},
    {"Wavefront OBJ", ".obj", FORMAT_CAP_WRITE,
     NULL, obj_save, obj_detect},
    // ... more formats
};
```

### Headless Rendering

#### OSMesa Integration
```c
// Initialize headless rendering context
typedef struct {
    OSMesaContext mesa_ctx;
    void *buffer;
    int width, height;
    bool initialized;
} headless_render_context_t;

int headless_render_init(void) {
    // Create OSMesa context for software rendering
    OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!ctx) {
        LOG_ERROR("Failed to create OSMesa context");
        return -1;
    }
    
    global_render_ctx.mesa_ctx = ctx;
    global_render_ctx.initialized = true;
    return 0;
}

// Render scene to buffer
int headless_render_scene(goxel_core_context_t *ctx, uint8_t *buffer, 
                         int width, int height, camera_preset_t preset) {
    if (!OSMesaMakeCurrent(global_render_ctx.mesa_ctx, buffer, 
                          GL_UNSIGNED_BYTE, width, height)) {
        return -1;
    }
    
    // Set up camera
    camera_set_preset(&ctx->camera, preset);
    
    // Render layers
    render_begin();
    for (int i = 0; i < ctx->image->layer_count; i++) {
        if (ctx->image->layers[i].visible) {
            render_layer(&ctx->image->layers[i]);
        }
    }
    render_end();
    
    return 0;
}
```

## Development Workflow

### Setting Up Development Environment

#### 1. Clone and Setup
```bash
git clone https://github.com/goxel/goxel.git
cd goxel
git checkout v13-headless

# Create development branch
git checkout -b feature/my-improvement
```

#### 2. Configure Build
```bash
# Development build with debugging
scons mode=debug headless=1 cli_tools=1 c_api=1

# Enable all warnings and debugging
export CFLAGS="-Wall -Wextra -g -O0 -fsanitize=address"
export CXXFLAGS="-Wall -Wextra -g -O0 -fsanitize=address"
```

#### 3. Development Tools
```bash
# Install development dependencies
pip install pre-commit
pre-commit install

# Static analysis tools
sudo apt-get install cppcheck valgrind clang-format

# Code coverage
sudo apt-get install gcov lcov
```

### Code Style Guidelines

#### C Code Style
```c
// Function naming: snake_case
int volume_set_voxel(volume_t *vol, int pos[3], uint8_t color[4])
{
    // Variable declarations at top
    block_t *block;
    int block_pos[3];
    int local_pos[3];
    
    // K&R brace style for control structures
    if (!vol || !pos || !color) {
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    // Clear logic flow
    volume_get_block_pos(pos, block_pos);
    volume_get_local_pos(pos, local_pos);
    
    block = volume_get_block(vol, block_pos);
    if (!block) {
        block = volume_create_block(vol, block_pos);
    }
    
    return block_set_voxel(block, local_pos, color);
}
```

#### Header Organization
```c
/* File header with license */
#ifndef GOXEL_VOLUME_H
#define GOXEL_VOLUME_H

// System includes first
#include <stdint.h>
#include <stdbool.h>

// Project includes
#include "goxel_core.h"

// Forward declarations
typedef struct volume volume_t;
typedef struct block block_t;

// Constants
#define VOLUME_MAX_SIZE 2048
#define BLOCK_SIZE 16

// Type definitions
typedef enum {
    VOLUME_FORMAT_SPARSE,
    VOLUME_FORMAT_DENSE
} volume_format_t;

// Function declarations with documentation
/**
 * Create a new volume with specified format
 * @param format Storage format to use
 * @return New volume or NULL on error
 */
volume_t *volume_new(volume_format_t format);

#endif // GOXEL_VOLUME_H
```

### Adding New Features

#### 1. Feature Planning
- Create issue describing the feature
- Design API changes if needed
- Plan testing strategy
- Consider backward compatibility

#### 2. Implementation Steps
```bash
# Create feature branch
git checkout -b feature/new-feature

# Implement core functionality
# 1. Add data structures
# 2. Implement core logic
# 3. Add API functions
# 4. Update CLI interface
# 5. Add tests
# 6. Update documentation
```

#### 3. Example: Adding New Voxel Operation
```c
// 1. Add to core API (src/core/goxel_core.h)
int goxel_core_flood_fill(goxel_core_context_t *ctx, int pos[3], 
                         uint8_t color[4], int layer_id);

// 2. Implement in core (src/core/volume_ops.c)
int goxel_core_flood_fill(goxel_core_context_t *ctx, int pos[3], 
                         uint8_t color[4], int layer_id) {
    if (!ctx || !pos || !color) {
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    layer_t *layer = image_get_layer(ctx->image, layer_id);
    if (!layer) {
        return GOXEL_ERROR_LAYER_NOT_FOUND;
    }
    
    return volume_flood_fill(layer->volume, pos, color);
}

// 3. Add CLI command (src/headless/cli_commands.c)
static int cmd_flood_fill(cli_args_t *args) {
    goxel_core_context_t *ctx = get_global_context();
    
    if (!args->position_set || !args->color_set) {
        fprintf(stderr, "Error: --pos and --color required\n");
        return 1;
    }
    
    int ret = goxel_core_flood_fill(ctx, args->position, args->color, 
                                   args->layer_id);
    if (ret != 0) {
        fprintf(stderr, "Error: Flood fill failed: %d\n", ret);
        return 1;
    }
    
    return 0;
}

// 4. Register command (src/headless/cli_interface.c)
static cli_command_t commands[] = {
    // ... existing commands
    {"flood-fill", "Fill connected voxels with color", cmd_flood_fill},
};

// 5. Add public API (include/goxel_headless.h)
goxel_error_t goxel_flood_fill(goxel_context_t *ctx, goxel_pos_t pos, 
                              goxel_color_t color);

// 6. Implement API bridge (src/headless/goxel_headless_api.c)
goxel_error_t goxel_flood_fill(goxel_context_t *ctx, goxel_pos_t pos, 
                              goxel_color_t color) {
    if (!ctx || !ctx->core_ctx) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    int core_pos[3] = {pos.x, pos.y, pos.z};
    uint8_t core_color[4] = {color.r, color.g, color.b, color.a};
    
    int ret = goxel_core_flood_fill(ctx->core_ctx, core_pos, core_color, 
                                   ctx->active_layer);
    return map_core_error(ret);
}
```

## Testing Framework

### Test Structure

```
tests/
├── core/                   # Core functionality tests
│   ├── test_goxel_core.c
│   ├── test_volume.c
│   ├── test_layers.c
│   └── test_file_formats.c
├── headless/              # Headless-specific tests
│   ├── test_rendering.c
│   ├── test_cli.c
│   └── test_api.c
├── integration/           # End-to-end tests
│   ├── test_e2e_headless.c
│   └── test_performance.c
├── data/                  # Test data files
│   ├── sample.gox
│   ├── test.vox
│   └── reference.obj
└── tools/                 # Testing utilities
    ├── test_runner.py
    └── generate_test_data.py
```

### Writing Tests

#### Unit Test Example
```c
#include "test_framework.h"
#include "../../src/core/volume.h"

TEST(volume_creation) {
    volume_t *vol = volume_new(VOLUME_FORMAT_SPARSE);
    ASSERT_NOT_NULL(vol);
    ASSERT_EQ(volume_get_voxel_count(vol), 0);
    
    volume_delete(vol);
    return TEST_PASS;
}

TEST(volume_voxel_operations) {
    volume_t *vol = volume_new(VOLUME_FORMAT_SPARSE);
    uint8_t red[4] = {255, 0, 0, 255};
    int pos[3] = {10, 20, 30};
    
    // Test add voxel
    int ret = volume_set_voxel(vol, pos, red);
    ASSERT_EQ(ret, 0);
    
    // Test get voxel
    uint8_t retrieved[4];
    ret = volume_get_voxel(vol, pos, retrieved);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(retrieved[0], 255);
    ASSERT_EQ(retrieved[1], 0);
    ASSERT_EQ(retrieved[2], 0);
    ASSERT_EQ(retrieved[3], 255);
    
    volume_delete(vol);
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(volume_tests) {
    RUN_TEST(volume_creation);
    RUN_TEST(volume_voxel_operations);
}
```

#### Integration Test Example
```c
TEST(cli_project_workflow) {
    const char *test_project = "/tmp/test_cli_workflow.gox";
    
    // Clean up any existing test file
    remove(test_project);
    
    // Test project creation
    int ret = run_cli_command("create", "Test Project", "--output", test_project);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(file_exists(test_project));
    
    // Test voxel addition
    ret = run_cli_command("voxel-add", "--pos", "0,0,0", 
                         "--color", "255,0,0,255");
    ASSERT_EQ(ret, 0);
    
    // Test save
    ret = run_cli_command("save", test_project);
    ASSERT_EQ(ret, 0);
    
    // Test render
    const char *render_output = "/tmp/test_render.png";
    ret = run_cli_command("render", "--output", render_output, 
                         "--camera", "isometric");
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(file_exists(render_output));
    
    // Cleanup
    remove(test_project);
    remove(render_output);
    
    return TEST_PASS;
}
```

### Running Tests

```bash
# Build and run all tests
make -C tests run-all

# Run specific test suites
make -C tests run-core
make -C tests run-integration

# Run with coverage analysis
make -C tests coverage

# Run memory leak detection
make -C tests memcheck

# Performance benchmarking
make -C tests benchmark
```

## Performance Optimization

### Profiling Tools

#### 1. Built-in Profiling
```c
// Add timing macros for development
#ifdef GOXEL_PROFILE
#define PROFILE_START(name) \
    uint64_t _prof_start_##name = get_time_ns()

#define PROFILE_END(name) \
    uint64_t _prof_end_##name = get_time_ns(); \
    LOG_INFO("PROFILE: %s took %lu ns", #name, \
             _prof_end_##name - _prof_start_##name)
#else
#define PROFILE_START(name)
#define PROFILE_END(name)
#endif

// Usage in code
int volume_set_voxel(volume_t *vol, int pos[3], uint8_t color[4]) {
    PROFILE_START(volume_set_voxel);
    
    // ... implementation
    
    PROFILE_END(volume_set_voxel);
    return ret;
}
```

#### 2. External Profiling
```bash
# Valgrind profiling
valgrind --tool=callgrind ./goxel-cli create test.gox
kcachegrind callgrind.out.*

# perf profiling (Linux)
perf record -g ./goxel-cli create test.gox
perf report

# Instruments (macOS)
instruments -t "Time Profiler" ./goxel-cli create test.gox
```

### Optimization Strategies

#### 1. Memory Optimization
```c
// Block pooling to reduce allocation overhead
typedef struct {
    block_data_t *free_blocks;
    int free_count;
    int max_free;
} block_pool_t;

static block_pool_t g_block_pool = {0};

block_data_t *block_data_alloc(void) {
    if (g_block_pool.free_count > 0) {
        // Reuse from pool
        return g_block_pool.free_blocks[--g_block_pool.free_count];
    } else {
        // Allocate new
        return malloc(sizeof(block_data_t));
    }
}

void block_data_free(block_data_t *data) {
    if (g_block_pool.free_count < g_block_pool.max_free) {
        // Return to pool
        g_block_pool.free_blocks[g_block_pool.free_count++] = data;
    } else {
        // Actually free
        free(data);
    }
}
```

#### 2. SIMD Optimization
```c
#ifdef __ARM_NEON
#include <arm_neon.h>

// Vectorized voxel operations for ARM64
void volume_fill_region_neon(uint8_t *voxels, uint8_t color[4], size_t count) {
    uint8x16_t color_vec = vdupq_n_u8(0);
    color_vec = vsetq_lane_u8(color[0], color_vec, 0);
    color_vec = vsetq_lane_u8(color[1], color_vec, 1);
    color_vec = vsetq_lane_u8(color[2], color_vec, 2);
    color_vec = vsetq_lane_u8(color[3], color_vec, 3);
    
    // Replicate pattern
    color_vec = vreinterpretq_u8_u32(vdupq_n_u32(
        vgetq_lane_u32(vreinterpretq_u32_u8(color_vec), 0)));
    
    size_t vec_count = count / 16;
    for (size_t i = 0; i < vec_count; i++) {
        vst1q_u8(voxels + i * 16, color_vec);
    }
    
    // Handle remainder
    size_t remainder = count % 16;
    for (size_t i = vec_count * 16; i < count; i++) {
        voxels[i] = color[i % 4];
    }
}
#endif
```

#### 3. Cache Optimization
```c
// Cache-friendly block iteration
void volume_iterate_blocks(volume_t *vol, block_iterator_func_t func) {
    // Sort blocks by memory address for better cache locality
    block_t **sorted_blocks = malloc(vol->block_count * sizeof(block_t*));
    memcpy(sorted_blocks, vol->blocks, vol->block_count * sizeof(block_t*));
    
    qsort(sorted_blocks, vol->block_count, sizeof(block_t*), 
          compare_block_addresses);
    
    // Iterate in cache-friendly order
    for (int i = 0; i < vol->block_count; i++) {
        func(sorted_blocks[i]);
    }
    
    free(sorted_blocks);
}
```

### Performance Targets

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Single voxel add/remove | <10ms | Average of 1000 operations |
| Batch operation (1K voxels) | <100ms | Single batch call |
| Project load/save | <1s | Typical project (10K voxels) |
| Rendering (1920x1080) | <10s | High quality, complex scene |
| Memory usage | <500MB | 100K voxel project |

## Contributing Guidelines

### Code Review Process

1. **Pre-commit Checks**
   ```bash
   # Format code
   clang-format -i src/**/*.c src/**/*.h
   
   # Static analysis
   cppcheck src/
   
   # Run tests
   make -C tests run-all
   ```

2. **Pull Request Requirements**
   - All tests pass
   - Code coverage maintained (>90%)
   - Documentation updated
   - Performance impact assessed
   - No memory leaks (valgrind clean)

3. **Review Checklist**
   - [ ] Code follows style guidelines
   - [ ] Functions are well-documented
   - [ ] Error handling is comprehensive
   - [ ] Tests cover new functionality
   - [ ] No performance regressions
   - [ ] Cross-platform compatibility verified

### Documentation Standards

#### Function Documentation
```c
/**
 * Add a voxel to the specified volume at the given position
 * 
 * @param vol Volume to modify (must not be NULL)
 * @param pos Position array [x, y, z] (must not be NULL)
 * @param color RGBA color array [r, g, b, a] (must not be NULL)
 * @param layer_id Target layer ID (must be valid)
 * 
 * @return 0 on success, negative error code on failure
 * @retval GOXEL_ERROR_INVALID_PARAMETER if any parameter is invalid
 * @retval GOXEL_ERROR_MEMORY if memory allocation fails
 * @retval GOXEL_ERROR_LAYER_NOT_FOUND if layer_id is invalid
 * 
 * @note This function is thread-safe when operating on different volumes
 * @warning Modifying the same volume from multiple threads is not safe
 * 
 * @since v13.0.0
 */
int volume_set_voxel(volume_t *vol, int pos[3], uint8_t color[4], int layer_id);
```

#### API Changes
- Document all API changes in CHANGELOG.md
- Maintain backward compatibility when possible
- Use semantic versioning for releases
- Provide migration guides for breaking changes

## Debugging and Profiling

### Debug Builds

```bash
# Enable debug symbols and AddressSanitizer
export CFLAGS="-g -O0 -fsanitize=address -fsanitize=undefined"
export LDFLAGS="-fsanitize=address -fsanitize=undefined"
scons mode=debug headless=1 cli_tools=1
```

### Debugging Tools

#### 1. GDB Debugging
```bash
# Debug CLI crashes
gdb ./goxel-cli
(gdb) set args create test.gox
(gdb) run
(gdb) bt
(gdb) print variable_name
```

#### 2. Memory Debugging
```bash
# AddressSanitizer (compile-time)
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1
./goxel-cli create test.gox

# Valgrind (runtime)
valgrind --leak-check=full --show-leak-kinds=all ./goxel-cli create test.gox
```

#### 3. Custom Debugging
```c
// Debug logging macros
#ifdef DEBUG
#define DEBUG_LOG(fmt, ...) \
    fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

// Memory tracking
#ifdef TRACK_MEMORY
static size_t allocated_memory = 0;
static size_t allocation_count = 0;

void *debug_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    allocated_memory += size;
    allocation_count++;
    DEBUG_LOG("ALLOC %zu bytes at %p (%s:%d)", size, ptr, file, line);
    return ptr;
}

#define malloc(size) debug_malloc(size, __FILE__, __LINE__)
#endif
```

### Performance Analysis

#### 1. Benchmarking Framework
```c
// Micro-benchmarking
typedef struct {
    const char *name;
    void (*setup)(void);
    void (*benchmark)(void);
    void (*teardown)(void);
    int iterations;
} benchmark_t;

void run_benchmark(benchmark_t *bench) {
    if (bench->setup) bench->setup();
    
    uint64_t start = get_time_ns();
    for (int i = 0; i < bench->iterations; i++) {
        bench->benchmark();
    }
    uint64_t end = get_time_ns();
    
    if (bench->teardown) bench->teardown();
    
    double avg_time = (double)(end - start) / bench->iterations / 1000000.0;
    printf("Benchmark %s: %.3f ms/op (%d iterations)\n", 
           bench->name, avg_time, bench->iterations);
}
```

#### 2. Memory Profiling
```bash
# Heap profiling with massif
valgrind --tool=massif ./goxel-cli create large_project.gox
ms_print massif.out.*

# Memory usage monitoring
while true; do
    ps -p $(pgrep goxel-cli) -o pid,rss,vsz,pcpu,pmem
    sleep 1
done
```

### Release Process

#### 1. Pre-release Checklist
- [ ] All tests pass on all platforms
- [ ] Performance benchmarks meet targets
- [ ] Documentation is up to date
- [ ] No known memory leaks
- [ ] API compatibility verified
- [ ] Security audit completed

#### 2. Release Build
```bash
# Optimized release build
export CFLAGS="-O3 -DNDEBUG -flto"
export LDFLAGS="-flto"
scons mode=release headless=1 cli_tools=1 c_api=1

# Strip symbols for distribution
strip goxel-cli
strip libgoxel_headless.so
```

#### 3. Packaging
```bash
# Create distribution packages
tar -czf goxel-headless-v13.0.0-linux-x86_64.tar.gz \
    goxel-cli libgoxel_headless.so include/ docs/

# Generate checksums
sha256sum goxel-headless-v13.0.0-*.tar.gz > checksums.sha256
```

---

**Developer Guide Version**: 13.0.0-phase6  
**Last Updated**: 2025-01-23  
**For Contributors**: See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines