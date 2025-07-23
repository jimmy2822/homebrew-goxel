# Goxel v13 Headless API Reference
**Version**: 13.0.0-phase6  
**Date**: 2025-01-23  
**Status**: Production Ready

## Table of Contents

1. [Overview](#overview)
2. [C API Reference](#c-api-reference)
3. [CLI Interface](#cli-interface)
4. [Error Handling](#error-handling)
5. [Examples](#examples)
6. [Performance Guidelines](#performance-guidelines)

## Overview

The Goxel v13 Headless API provides programmatic access to Goxel's 3D voxel editing capabilities without requiring a GUI. The API is available in two forms:

- **C API**: Direct library integration for native applications
- **CLI Interface**: Command-line tools for scripting and automation

### Key Features

- **Headless Operation**: No GUI dependencies required
- **Full Voxel Operations**: Add, remove, modify voxels programmatically
- **Layer Management**: Create and manage multiple voxel layers
- **File Format Support**: Import/export multiple voxel formats
- **Rendering**: Generate images from voxel scenes
- **Cross-Platform**: Linux, macOS, Windows support

## C API Reference

### Core Types

#### goxel_context_t
Main context structure for all operations.

```c
typedef struct goxel_context goxel_context_t;
```

#### goxel_error_t
Error codes returned by API functions.

```c
typedef enum {
    GOXEL_ERROR_NONE = 0,
    GOXEL_ERROR_INVALID_CONTEXT = 1,
    GOXEL_ERROR_INVALID_PARAMETER = 2,
    GOXEL_ERROR_FILE_NOT_FOUND = 3,
    GOXEL_ERROR_FILE_FORMAT = 4,
    GOXEL_ERROR_MEMORY = 5,
    GOXEL_ERROR_LAYER_NOT_FOUND = 6,
    GOXEL_ERROR_RENDER_FAILED = 7,
    GOXEL_ERROR_NOT_IMPLEMENTED = 8,
    GOXEL_ERROR_UNKNOWN = 99
} goxel_error_t;
```

#### goxel_color_t
RGBA color representation.

```c
typedef struct {
    uint8_t r, g, b, a;
} goxel_color_t;
```

#### goxel_pos_t
3D position coordinates.

```c
typedef struct {
    int x, y, z;
} goxel_pos_t;
```

### Context Management

#### goxel_create_context()
Creates a new Goxel context.

```c
goxel_context_t *goxel_create_context(goxel_error_t *error);
```

**Parameters:**
- `error`: Pointer to error code variable (output)

**Returns:**
- Pointer to new context, or NULL on error

**Example:**
```c
goxel_error_t error;
goxel_context_t *ctx = goxel_create_context(&error);
if (!ctx) {
    fprintf(stderr, "Failed to create context: %d\n", error);
    return 1;
}
```

#### goxel_init_context()
Initializes a context for use.

```c
goxel_error_t goxel_init_context(goxel_context_t *ctx);
```

**Parameters:**
- `ctx`: Context to initialize

**Returns:**
- `GOXEL_ERROR_NONE` on success, error code otherwise

#### goxel_destroy_context()
Destroys a context and frees resources.

```c
void goxel_destroy_context(goxel_context_t *ctx);
```

**Parameters:**
- `ctx`: Context to destroy

### Project Management

#### goxel_create_project()
Creates a new voxel project.

```c
goxel_error_t goxel_create_project(goxel_context_t *ctx, const char *name);
```

**Parameters:**
- `ctx`: Goxel context
- `name`: Project name

**Returns:**
- Error code

#### goxel_load_project()
Loads a project from file.

```c
goxel_error_t goxel_load_project(goxel_context_t *ctx, const char *path);
```

**Parameters:**
- `ctx`: Goxel context
- `path`: File path to load

**Returns:**
- Error code

#### goxel_save_project()
Saves current project to file.

```c
goxel_error_t goxel_save_project(goxel_context_t *ctx, const char *path);
```

**Parameters:**
- `ctx`: Goxel context
- `path`: File path to save

**Returns:**
- Error code

### Voxel Operations

#### goxel_add_voxel()
Adds a voxel at the specified position.

```c
goxel_error_t goxel_add_voxel(goxel_context_t *ctx, goxel_pos_t pos, goxel_color_t color);
```

**Parameters:**
- `ctx`: Goxel context
- `pos`: Position to add voxel
- `color`: Voxel color

**Returns:**
- Error code

**Example:**
```c
goxel_pos_t pos = {10, 20, 30};
goxel_color_t red = {255, 0, 0, 255};
goxel_error_t err = goxel_add_voxel(ctx, pos, red);
```

#### goxel_remove_voxel()
Removes a voxel at the specified position.

```c
goxel_error_t goxel_remove_voxel(goxel_context_t *ctx, goxel_pos_t pos);
```

#### goxel_get_voxel()
Gets voxel color at the specified position.

```c
goxel_error_t goxel_get_voxel(goxel_context_t *ctx, goxel_pos_t pos, goxel_color_t *color);
```

#### goxel_add_voxel_batch()
Adds multiple voxels in a single operation.

```c
typedef struct {
    goxel_pos_t pos;
    goxel_color_t color;
} goxel_voxel_batch_t;

goxel_error_t goxel_add_voxel_batch(goxel_context_t *ctx, 
                                    const goxel_voxel_batch_t *voxels, 
                                    size_t count);
```

### Layer Management

#### goxel_layer_id_t
Layer identifier type.

```c
typedef int goxel_layer_id_t;
```

#### goxel_create_layer()
Creates a new layer.

```c
goxel_layer_id_t goxel_create_layer(goxel_context_t *ctx, const char *name);
```

**Returns:**
- Layer ID (>= 0) on success, -1 on error

#### goxel_delete_layer()
Deletes a layer.

```c
goxel_error_t goxel_delete_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id);
```

#### goxel_set_active_layer()
Sets the active layer for operations.

```c
goxel_error_t goxel_set_active_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id);
```

#### goxel_set_layer_visibility()
Sets layer visibility.

```c
goxel_error_t goxel_set_layer_visibility(goxel_context_t *ctx, 
                                         goxel_layer_id_t layer_id, 
                                         bool visible);
```

### Rendering

#### goxel_render_settings_t
Rendering configuration structure.

```c
typedef enum {
    GOXEL_CAMERA_FRONT,
    GOXEL_CAMERA_BACK,
    GOXEL_CAMERA_LEFT,
    GOXEL_CAMERA_RIGHT,
    GOXEL_CAMERA_TOP,
    GOXEL_CAMERA_BOTTOM,
    GOXEL_CAMERA_ISOMETRIC
} goxel_camera_preset_t;

typedef enum {
    GOXEL_RENDER_QUALITY_LOW,
    GOXEL_RENDER_QUALITY_NORMAL,
    GOXEL_RENDER_QUALITY_HIGH
} goxel_render_quality_t;

typedef struct {
    int width;
    int height;
    goxel_camera_preset_t camera_preset;
    goxel_render_quality_t quality;
} goxel_render_settings_t;
```

#### goxel_render_to_file()
Renders the scene to an image file.

```c
goxel_error_t goxel_render_to_file(goxel_context_t *ctx, 
                                   const char *output_path,
                                   const goxel_render_settings_t *settings);
```

**Example:**
```c
goxel_render_settings_t settings = {
    .width = 1920,
    .height = 1080,
    .camera_preset = GOXEL_CAMERA_ISOMETRIC,
    .quality = GOXEL_RENDER_QUALITY_HIGH
};

goxel_error_t err = goxel_render_to_file(ctx, "output.png", &settings);
```

### Utility Functions

#### goxel_get_version()
Gets the Goxel version string.

```c
const char *goxel_get_version(void);
```

#### goxel_has_feature()
Checks if a feature is available.

```c
typedef enum {
    GOXEL_FEATURE_LAYERS,
    GOXEL_FEATURE_UNDO_REDO,
    GOXEL_FEATURE_EXPORT,
    GOXEL_FEATURE_RENDERING,
    GOXEL_FEATURE_SCRIPTING
} goxel_feature_t;

bool goxel_has_feature(goxel_feature_t feature);
```

#### goxel_get_error_string()
Gets human-readable error description.

```c
const char *goxel_get_error_string(goxel_error_t error);
```

## CLI Interface

### Global Options

```bash
goxel-cli [GLOBAL_OPTIONS] COMMAND [COMMAND_OPTIONS]
```

**Global Options:**
- `-h, --help`: Show help
- `-v, --version`: Show version
- `--verbose`: Enable verbose output
- `--quiet`: Suppress non-error output

### Commands

#### create
Creates a new voxel project.

```bash
goxel-cli create PROJECT_NAME [OPTIONS]
```

**Options:**
- `--size W,H,D`: Initial project size (default: 64,64,64)
- `--output PATH`: Output file path

**Example:**
```bash
goxel-cli create my_project --size 32,32,32 --output project.gox
```

#### open
Opens an existing project.

```bash
goxel-cli open PROJECT_FILE
```

#### save
Saves the current project.

```bash
goxel-cli save [OUTPUT_FILE]
```

#### voxel-add
Adds a voxel at specified position.

```bash
goxel-cli voxel-add --pos X,Y,Z --color R,G,B,A [OPTIONS]
```

**Options:**
- `--pos X,Y,Z`: Voxel position (required)
- `--color R,G,B,A`: Voxel color (required)
- `--layer ID`: Target layer (default: 0)

**Example:**
```bash
goxel-cli voxel-add --pos 10,20,30 --color 255,0,0,255
```

#### voxel-remove
Removes voxel(s) at specified position or area.

```bash
goxel-cli voxel-remove --pos X,Y,Z [OPTIONS]
goxel-cli voxel-remove --box X1,Y1,Z1,X2,Y2,Z2 [OPTIONS]
```

#### render
Renders the scene to an image.

```bash
goxel-cli render --output FILE [OPTIONS]
```

**Options:**
- `--output FILE`: Output image file (required)
- `--resolution WxH`: Image resolution (default: 1920x1080)
- `--camera PRESET`: Camera preset (front, back, left, right, top, bottom, isometric)
- `--quality LEVEL`: Render quality (low, normal, high)

**Example:**
```bash
goxel-cli render --output render.png --resolution 1920x1080 --camera isometric
```

#### layer-create
Creates a new layer.

```bash
goxel-cli layer-create LAYER_NAME [OPTIONS]
```

#### export
Exports project to various formats.

```bash
goxel-cli export --format FORMAT --output FILE [OPTIONS]
```

**Supported Formats:**
- `obj`: Wavefront OBJ
- `ply`: Stanford PLY
- `stl`: STL mesh
- `vox`: MagicaVoxel
- `gox`: Goxel format

## Error Handling

### Error Codes

All API functions return error codes or set error parameters. Always check return values:

```c
goxel_error_t err = goxel_add_voxel(ctx, pos, color);
if (err != GOXEL_ERROR_NONE) {
    fprintf(stderr, "Error: %s\n", goxel_get_error_string(err));
    return 1;
}
```

### Common Error Patterns

#### Context Validation
```c
if (!ctx) {
    fprintf(stderr, "Invalid context\n");
    return GOXEL_ERROR_INVALID_CONTEXT;
}
```

#### Parameter Validation
```c
if (!output_path || strlen(output_path) == 0) {
    return GOXEL_ERROR_INVALID_PARAMETER;
}
```

#### Resource Cleanup
```c
void cleanup_and_exit(goxel_context_t *ctx, int exit_code) {
    if (ctx) {
        goxel_destroy_context(ctx);
    }
    exit(exit_code);
}
```

## Examples

### Complete C Example

```c
#include "goxel_headless.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    goxel_error_t error;
    
    // Create and initialize context
    goxel_context_t *ctx = goxel_create_context(&error);
    if (!ctx) {
        fprintf(stderr, "Failed to create context: %d\n", error);
        return 1;
    }
    
    error = goxel_init_context(ctx);
    if (error != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to initialize context: %d\n", error);
        goxel_destroy_context(ctx);
        return 1;
    }
    
    // Create project
    error = goxel_create_project(ctx, "Example Project");
    if (error != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to create project: %d\n", error);
        goxel_destroy_context(ctx);
        return 1;
    }
    
    // Add some voxels
    goxel_color_t red = {255, 0, 0, 255};
    for (int x = 0; x < 10; x++) {
        goxel_pos_t pos = {x, 0, 0};
        error = goxel_add_voxel(ctx, pos, red);
        if (error != GOXEL_ERROR_NONE) {
            fprintf(stderr, "Failed to add voxel: %d\n", error);
            break;
        }
    }
    
    // Save project
    error = goxel_save_project(ctx, "example.gox");
    if (error != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to save project: %d\n", error);
    } else {
        printf("Project saved successfully!\n");
    }
    
    // Cleanup
    goxel_destroy_context(ctx);
    return 0;
}
```

### CLI Workflow Example

```bash
#!/bin/bash
# Create a simple voxel scene

# Create new project
goxel-cli create "My Scene" --output scene.gox

# Add some voxels
goxel-cli voxel-add --pos 0,0,0 --color 255,0,0,255    # Red
goxel-cli voxel-add --pos 1,0,0 --color 0,255,0,255    # Green  
goxel-cli voxel-add --pos 2,0,0 --color 0,0,255,255    # Blue

# Create a new layer
goxel-cli layer-create "Details"

# Render the scene
goxel-cli render --output scene.png --camera isometric --quality high

# Export to OBJ format
goxel-cli export --format obj --output scene.obj

echo "Scene created and rendered!"
```

## Performance Guidelines

### Best Practices

#### Batch Operations
Use batch operations for adding multiple voxels:

```c
// Preferred: Batch operation
goxel_voxel_batch_t voxels[1000];
// ... populate voxels array ...
goxel_add_voxel_batch(ctx, voxels, 1000);

// Avoid: Individual operations in loop
for (int i = 0; i < 1000; i++) {
    goxel_add_voxel(ctx, pos[i], color[i]);  // Slower
}
```

#### Memory Management
- Always destroy contexts when done
- Check return values for memory allocation failures
- Use appropriate data types for coordinates

#### Rendering Optimization
- Use lower quality for preview renders
- Consider resolution requirements
- Cache render settings for repeated use

### Performance Targets

- **Simple Operations**: <10ms per operation
- **Batch Operations**: <100ms per 1,000 voxels  
- **Project Load/Save**: <1s for typical projects
- **Rendering**: <10s for 1920x1080 high quality
- **Memory Usage**: <500MB for 100k voxel projects

---

**API Reference Version**: 13.0.0-phase6  
**Last Updated**: 2025-01-23  
**Status**: Production Ready