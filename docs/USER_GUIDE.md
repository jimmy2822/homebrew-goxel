# Goxel v13 Headless User Guide
**Version**: 13.0.0-phase6  
**Date**: 2025-01-23

## Table of Contents

1. [Getting Started](#getting-started)
2. [Installation](#installation)
3. [Basic Concepts](#basic-concepts)
4. [CLI Tutorials](#cli-tutorials)
5. [Programming with C API](#programming-with-c-api)
6. [File Formats](#file-formats)
7. [Troubleshooting](#troubleshooting)
8. [Advanced Usage](#advanced-usage)

## Getting Started

Goxel v13 Headless brings the power of 3D voxel editing to command-line environments and programmatic applications. Whether you're automating voxel art creation, integrating voxel editing into your application, or working in server environments, this guide will help you get started.

### What is Goxel Headless?

Goxel Headless is a version of the popular Goxel 3D voxel editor that runs without a graphical user interface. It provides:

- **Command-line interface** for scripting and automation
- **C API** for integration into other applications
- **Headless rendering** for generating images programmatically
- **Full file format support** for importing and exporting voxel data

### System Requirements

**Minimum Requirements:**
- OS: Linux, macOS, or Windows
- RAM: 512MB available memory
- Storage: 50MB disk space
- Architecture: x86_64 or ARM64

**Recommended:**
- RAM: 2GB+ for large projects
- Storage: 1GB+ for assets and temporary files
- SSD for better I/O performance

## Installation

### Pre-built Binaries

Download the latest release for your platform:

```bash
# macOS ARM64
curl -L https://github.com/goxel/goxel/releases/download/v13.0.0/goxel-cli-macos-arm64 -o goxel-cli
chmod +x goxel-cli

# Linux x86_64
curl -L https://github.com/goxel/goxel/releases/download/v13.0.0/goxel-cli-linux-x86_64 -o goxel-cli
chmod +x goxel-cli

# Windows
curl -L https://github.com/goxel/goxel/releases/download/v13.0.0/goxel-cli-windows.exe -o goxel-cli.exe
```

### Building from Source

**Prerequisites:**
- GCC or Clang compiler
- SCons build system
- GLFW development libraries
- PNG development libraries

**Build Steps:**
```bash
git clone https://github.com/goxel/goxel.git
cd goxel
git checkout v13-headless
scons headless=1 cli_tools=1
```

The `goxel-cli` executable will be created in the project root.

### Verification

Test your installation:

```bash
./goxel-cli --version
./goxel-cli --help
```

You should see version information and a list of available commands.

## Basic Concepts

### Voxels

**Voxels** (volume pixels) are 3D cubes that form the building blocks of your models. Each voxel has:
- **Position**: X, Y, Z coordinates in 3D space
- **Color**: RGBA values (Red, Green, Blue, Alpha)
- **State**: Present or absent (empty space)

### Coordinate System

Goxel uses a right-handed coordinate system:
- **X-axis**: Left (-) to Right (+)
- **Y-axis**: Down (-) to Up (+)  
- **Z-axis**: Back (-) to Front (+)

### Projects

A **project** contains:
- Multiple **layers** of voxels
- Project metadata (name, description, etc.)
- Render settings and camera positions
- Undo/redo history

### Layers

**Layers** organize voxels into separate groups:
- Each layer can be shown/hidden independently
- Layers have names and properties
- Voxels are added to the currently active layer

### File Formats

Goxel supports multiple file formats:
- **`.gox`**: Native Goxel format (recommended)
- **`.vox`**: MagicaVoxel format
- **`.obj`**: Wavefront 3D mesh (export only)
- **`.ply`**: Stanford PLY mesh (export only)
- **`.stl`**: STL mesh (export only)

## CLI Tutorials

### Tutorial 1: Your First Voxel Model

Let's create a simple 3-voxel model:

```bash
# Step 1: Create a new project
./goxel-cli create "My First Model" --output first_model.gox

# Step 2: Add three colored voxels
./goxel-cli voxel-add --pos 0,0,0 --color 255,0,0,255    # Red voxel
./goxel-cli voxel-add --pos 1,0,0 --color 0,255,0,255    # Green voxel  
./goxel-cli voxel-add --pos 2,0,0 --color 0,0,255,255    # Blue voxel

# Step 3: Save the project
./goxel-cli save first_model.gox

# Step 4: Render an image
./goxel-cli render --output first_model.png --camera isometric

echo "Your first model is complete!"
```

### Tutorial 2: Working with Layers

```bash
# Create project
./goxel-cli create "Layered Model" --output layered.gox

# Create layers for different parts
./goxel-cli layer-create "Foundation"
./goxel-cli layer-create "Walls"
./goxel-cli layer-create "Roof"

# Add foundation voxels (assuming layer 0 is "Foundation")
for x in {0..4}; do
    for z in {0..4}; do
        ./goxel-cli voxel-add --pos $x,0,$z --color 139,69,19,255 --layer 0
    done
done

# Switch to walls layer and add walls
./goxel-cli set-active-layer 1
for x in {0..4}; do
    ./goxel-cli voxel-add --pos $x,1,0 --color 200,200,200,255
    ./goxel-cli voxel-add --pos $x,1,4 --color 200,200,200,255
done

# Add roof
./goxel-cli set-active-layer 2
for x in {1..3}; do
    for z in {1..3}; do
        ./goxel-cli voxel-add --pos $x,2,$z --color 255,0,0,255
    done
done

# Render the complete model
./goxel-cli render --output house.png --camera isometric --quality high
```

### Tutorial 3: Batch Operations

For complex models, use batch operations:

```bash
# Create a CSV file with voxel data
cat > sphere_voxels.csv << EOF
x,y,z,r,g,b,a
0,0,0,255,0,0,255
1,0,0,255,128,0,255
0,1,0,255,255,0,255
-1,0,0,128,255,0,255
0,-1,0,0,255,0,255
0,0,1,0,255,128,255
0,0,-1,0,128,255,255
EOF

# Create project and load batch data
./goxel-cli create "Sphere" --output sphere.gox
./goxel-cli voxel-batch-add --file sphere_voxels.csv

# Render from multiple angles
./goxel-cli render --output sphere_front.png --camera front
./goxel-cli render --output sphere_side.png --camera right
./goxel-cli render --output sphere_iso.png --camera isometric
```

### Tutorial 4: Format Conversion

```bash
# Convert MagicaVoxel file to Goxel format
./goxel-cli open input.vox
./goxel-cli save converted.gox

# Export to various 3D formats
./goxel-cli export --format obj --output model.obj
./goxel-cli export --format ply --output model.ply
./goxel-cli export --format stl --output model.stl

# Batch convert multiple files
for file in *.vox; do
    name=$(basename "$file" .vox)
    ./goxel-cli open "$file"
    ./goxel-cli export --format obj --output "${name}.obj"
done
```

## Programming with C API

### Basic Setup

Include the header and link the library:

```c
#include "goxel_headless.h"

// Compile with: gcc -o myapp myapp.c -lgoxel_headless -lm -lpthread
```

### Simple Example

```c
#include "goxel_headless.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    goxel_error_t error;
    
    // Initialize
    goxel_context_t *ctx = goxel_create_context(&error);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }
    
    if (goxel_init_context(ctx) != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to initialize context\n");
        goxel_destroy_context(ctx);
        return 1;
    }
    
    // Create project
    if (goxel_create_project(ctx, "API Demo") != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to create project\n");
        goxel_destroy_context(ctx);
        return 1;
    }
    
    // Add voxels to create a cube
    goxel_color_t blue = {0, 100, 255, 255};
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            for (int z = 0; z < 3; z++) {
                goxel_pos_t pos = {x, y, z};
                goxel_add_voxel(ctx, pos, blue);
            }
        }
    }
    
    // Save and render
    goxel_save_project(ctx, "api_demo.gox");
    
    goxel_render_settings_t settings = {
        .width = 800,
        .height = 600,
        .camera_preset = GOXEL_CAMERA_ISOMETRIC,
        .quality = GOXEL_RENDER_QUALITY_NORMAL
    };
    goxel_render_to_file(ctx, "api_demo.png", &settings);
    
    printf("Created 3x3x3 blue cube!\n");
    
    // Cleanup
    goxel_destroy_context(ctx);
    return 0;
}
```

### Advanced Example: Procedural Generation

```c
#include "goxel_headless.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Generate a sphere of voxels
void generate_sphere(goxel_context_t *ctx, int radius, goxel_color_t color) {
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            for (int z = -radius; z <= radius; z++) {
                float distance = sqrt(x*x + y*y + z*z);
                if (distance <= radius) {
                    goxel_pos_t pos = {x, y, z};
                    goxel_add_voxel(ctx, pos, color);
                }
            }
        }
    }
}

// Generate terrain heightmap
void generate_terrain(goxel_context_t *ctx, int width, int depth) {
    goxel_color_t grass = {34, 139, 34, 255};
    goxel_color_t dirt = {139, 69, 19, 255};
    goxel_color_t stone = {128, 128, 128, 255};
    
    for (int x = 0; x < width; x++) {
        for (int z = 0; z < depth; z++) {
            // Simple heightmap using sine waves
            float height = 5 + 3 * sin(x * 0.1) * cos(z * 0.1);
            
            for (int y = 0; y < (int)height; y++) {
                goxel_pos_t pos = {x, y, z};
                goxel_color_t color;
                
                if (y == (int)height - 1) {
                    color = grass;  // Top layer
                } else if (y > (int)height - 4) {
                    color = dirt;   // Dirt layer
                } else {
                    color = stone;  // Stone base
                }
                
                goxel_add_voxel(ctx, pos, color);
            }
        }
    }
}

int main() {
    goxel_error_t error;
    goxel_context_t *ctx = goxel_create_context(&error);
    
    if (!ctx || goxel_init_context(ctx) != GOXEL_ERROR_NONE) {
        fprintf(stderr, "Failed to initialize Goxel\n");
        return 1;
    }
    
    // Create project with multiple layers
    goxel_create_project(ctx, "Procedural World");
    
    // Create terrain layer
    goxel_layer_id_t terrain_layer = goxel_create_layer(ctx, "Terrain");
    goxel_set_active_layer(ctx, terrain_layer);
    generate_terrain(ctx, 50, 50);
    
    // Create decoration layer
    goxel_layer_id_t deco_layer = goxel_create_layer(ctx, "Decorations");
    goxel_set_active_layer(ctx, deco_layer);
    
    // Add some spheres as trees/rocks
    goxel_color_t green = {0, 255, 0, 255};
    goxel_color_t brown = {139, 69, 19, 255};
    
    // Place objects at various locations
    int tree_positions[][3] = {{10, 8, 10}, {20, 6, 15}, {35, 7, 30}};
    for (int i = 0; i < 3; i++) {
        goxel_pos_t base = {tree_positions[i][0], tree_positions[i][1], tree_positions[i][2]};
        
        // Tree trunk
        for (int y = 0; y < 3; y++) {
            goxel_pos_t trunk_pos = {base.x, base.y + y, base.z};
            goxel_add_voxel(ctx, trunk_pos, brown);
        }
        
        // Tree leaves (small sphere)
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                for (int dz = -2; dz <= 2; dz++) {
                    if (dx*dx + dy*dy + dz*dz <= 4) {
                        goxel_pos_t leaf_pos = {base.x + dx, base.y + 3 + dy, base.z + dz};
                        goxel_add_voxel(ctx, leaf_pos, green);
                    }
                }
            }
        }
    }
    
    // Save project
    goxel_save_project(ctx, "procedural_world.gox");
    
    // Render multiple views
    goxel_render_settings_t settings = {
        .width = 1920,
        .height = 1080,
        .quality = GOXEL_RENDER_QUALITY_HIGH
    };
    
    settings.camera_preset = GOXEL_CAMERA_ISOMETRIC;
    goxel_render_to_file(ctx, "world_isometric.png", &settings);
    
    settings.camera_preset = GOXEL_CAMERA_TOP;
    goxel_render_to_file(ctx, "world_top.png", &settings);
    
    printf("Generated procedural world with terrain and trees!\n");
    
    goxel_destroy_context(ctx);
    return 0;
}
```

## File Formats

### Goxel Format (.gox)

The native Goxel format preserves all features:
- Multiple layers with properties
- Full undo/redo history
- Project metadata
- Compressed binary format

**Usage:**
```bash
# Save in native format
./goxel-cli save project.gox

# Load native format
./goxel-cli open project.gox
```

### MagicaVoxel Format (.vox)

Popular voxel format with good compatibility:
- Single layer support
- 256 color palette
- Widely supported

**Usage:**
```bash
# Import MagicaVoxel file
./goxel-cli open model.vox
./goxel-cli save converted.gox

# Export to MagicaVoxel (some features may be lost)
./goxel-cli export --format vox --output model.vox
```

### 3D Mesh Formats

Export voxels as 3D meshes for use in other applications:

#### Wavefront OBJ (.obj)
```bash
./goxel-cli export --format obj --output model.obj
# Creates model.obj and model.mtl (materials)
```

#### Stanford PLY (.ply)  
```bash
./goxel-cli export --format ply --output model.ply
```

#### STL (.stl)
```bash
./goxel-cli export --format stl --output model.stl
```

## Troubleshooting

### Common Issues

#### "Command not found"
**Problem**: CLI not in PATH
**Solution**: 
```bash
# Add to PATH or use full path
export PATH=$PATH:/path/to/goxel
# or
/full/path/to/goxel-cli --help
```

#### "Permission denied"
**Problem**: CLI not executable
**Solution**:
```bash
chmod +x goxel-cli
```

#### "Failed to initialize context"
**Problem**: Missing dependencies
**Solution**:
- Check OpenGL/rendering libraries
- Verify system requirements
- Try verbose mode: `./goxel-cli --verbose`

#### "File format not supported"
**Problem**: Unsupported file extension
**Solution**:
- Check supported formats: `./goxel-cli --help`
- Use explicit format: `--format obj`
- Convert through .gox format

#### "Rendering failed"
**Problem**: Headless rendering issues
**Solution**:
- Check OSMesa installation
- Verify memory availability
- Try lower resolution/quality
- Use software rendering: `--render-mode software`

### Performance Issues

#### Slow Operations
- Use batch operations for multiple voxels
- Reduce project size for testing
- Check available memory
- Use SSD storage for temporary files

#### Large File Sizes
- Use compression: `--compress`
- Remove unnecessary layers
- Optimize voxel placement
- Consider mesh export for final models

### Debug Information

Enable verbose logging:
```bash
./goxel-cli --verbose create test.gox
```

Check system information:
```bash
./goxel-cli --system-info
```

Generate debug report:
```bash
./goxel-cli --debug-report > debug.txt
```

## Advanced Usage

### Scripting and Automation

#### Bash Script Example
```bash
#!/bin/bash
# Automated voxel art generation

MODELS_DIR="./generated_models"
mkdir -p "$MODELS_DIR"

# Generate a series of models
for size in 10 20 30; do
    for color in "255,0,0" "0,255,0" "0,0,255"; do
        model_name="cube_${size}_${color//,/_}"
        
        ./goxel-cli create "$model_name" --output "$MODELS_DIR/$model_name.gox"
        
        # Fill cube with specified color
        IFS=',' read -r r g b <<< "$color"
        for ((x=0; x<size; x++)); do
            for ((y=0; y<size; y++)); do
                for ((z=0; z<size; z++)); do
                    ./goxel-cli voxel-add --pos $x,$y,$z --color $r,$g,$b,255
                done
            done
        done
        
        # Render and export
        ./goxel-cli render --output "$MODELS_DIR/$model_name.png" --camera isometric
        ./goxel-cli export --format obj --output "$MODELS_DIR/$model_name.obj"
        
        echo "Generated $model_name"
    done
done

echo "Generated $(ls $MODELS_DIR/*.gox | wc -l) models"
```

#### Python Integration
```python
#!/usr/bin/env python3
import subprocess
import json
import os

class GoxelCLI:
    def __init__(self, cli_path="./goxel-cli"):
        self.cli_path = cli_path
    
    def run_command(self, *args):
        cmd = [self.cli_path] + list(args)
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0, result.stdout, result.stderr
    
    def create_project(self, name, output=None):
        args = ["create", name]
        if output:
            args.extend(["--output", output])
        return self.run_command(*args)
    
    def add_voxel(self, x, y, z, r, g, b, a=255, layer=0):
        return self.run_command(
            "voxel-add",
            "--pos", f"{x},{y},{z}",
            "--color", f"{r},{g},{b},{a}",
            "--layer", str(layer)
        )
    
    def render(self, output, width=1920, height=1080, camera="isometric"):
        return self.run_command(
            "render",
            "--output", output,
            "--resolution", f"{width}x{height}",
            "--camera", camera
        )

# Usage example
goxel = GoxelCLI()

# Create a rainbow cube
success, _, _ = goxel.create_project("Rainbow Cube", "rainbow.gox")
if success:
    colors = [
        (255, 0, 0),    # Red
        (255, 165, 0),  # Orange
        (255, 255, 0),  # Yellow
        (0, 255, 0),    # Green
        (0, 0, 255),    # Blue
        (75, 0, 130),   # Indigo
        (238, 130, 238) # Violet
    ]
    
    for i, (r, g, b) in enumerate(colors):
        goxel.add_voxel(i, 0, 0, r, g, b)
    
    goxel.render("rainbow.png")
    print("Rainbow cube created!")
```

### Integration with Other Tools

#### Blender Import
```python
# Blender script to import OBJ files generated by Goxel
import bpy
import os

def import_goxel_models(directory):
    for filename in os.listdir(directory):
        if filename.endswith('.obj'):
            filepath = os.path.join(directory, filename)
            bpy.ops.import_scene.obj(filepath=filepath)
            
    print(f"Imported {len([f for f in os.listdir(directory) if f.endswith('.obj')])} models")

# Run in Blender
import_goxel_models("./generated_models")
```

#### 3D Printing Preparation
```bash
#!/bin/bash
# Prepare voxel models for 3D printing

INPUT_DIR="./models"
OUTPUT_DIR="./print_ready"
mkdir -p "$OUTPUT_DIR"

for gox_file in "$INPUT_DIR"/*.gox; do
    name=$(basename "$gox_file" .gox)
    
    # Export as STL for 3D printing
    ./goxel-cli open "$gox_file"
    ./goxel-cli export --format stl --output "$OUTPUT_DIR/$name.stl"
    
    # Generate preview image
    ./goxel-cli render --output "$OUTPUT_DIR/$name_preview.png" \
        --camera isometric --quality high --resolution 800x600
    
    echo "Prepared $name for 3D printing"
done
```

### Server Deployment

#### Docker Container
```dockerfile
FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libosmesa6-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy Goxel CLI
COPY goxel-cli /usr/local/bin/
RUN chmod +x /usr/local/bin/goxel-cli

# Set working directory
WORKDIR /workspace

# Default command
CMD ["goxel-cli", "--help"]
```

Build and run:
```bash
docker build -t goxel-headless .
docker run -v $(pwd):/workspace goxel-headless goxel-cli create test.gox
```

#### Web Service Integration
```python
# Flask web service for voxel generation
from flask import Flask, request, jsonify, send_file
import subprocess
import tempfile
import os

app = Flask(__name__)

@app.route('/api/voxel/create', methods=['POST'])
def create_voxel_model():
    data = request.json
    voxels = data.get('voxels', [])
    
    with tempfile.TemporaryDirectory() as temp_dir:
        project_file = os.path.join(temp_dir, 'model.gox')
        render_file = os.path.join(temp_dir, 'render.png')
        
        # Create project
        subprocess.run(['goxel-cli', 'create', 'API Model', '--output', project_file])
        
        # Add voxels
        for voxel in voxels:
            cmd = [
                'goxel-cli', 'voxel-add',
                '--pos', f"{voxel['x']},{voxel['y']},{voxel['z']}",
                '--color', f"{voxel['r']},{voxel['g']},{voxel['b']},{voxel.get('a', 255)}"
            ]
            subprocess.run(cmd)
        
        # Render
        subprocess.run(['goxel-cli', 'render', '--output', render_file])
        
        return send_file(render_file, mimetype='image/png')

if __name__ == '__main__':
    app.run(debug=True)
```

---

**User Guide Version**: 13.0.0-phase6  
**Last Updated**: 2025-01-23  
**Support**: See [API Reference](API_REFERENCE.md) and [Troubleshooting](#troubleshooting)