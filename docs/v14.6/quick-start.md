# Goxel v14.6 Quick Start Guide

## 🚀 Get Started in 5 Minutes

This guide will help you get Goxel v14.6 up and running quickly. Choose your path based on your use case.

## Installation

### macOS
```bash
# Using Homebrew
brew install goxel

# Or download binary
curl -L https://goxel.xyz/downloads/v14.6/goxel-macos-universal -o goxel
chmod +x goxel
sudo mv goxel /usr/local/bin/
```

### Linux
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install goxel

# Or download binary
curl -L https://goxel.xyz/downloads/v14.6/goxel-linux-x64 -o goxel
chmod +x goxel
sudo mv goxel /usr/local/bin/
```

### Windows
```powershell
# Using Chocolatey
choco install goxel

# Or download installer
# Visit: https://goxel.xyz/downloads/v14.6/goxel-windows-installer.exe
```

## Choose Your Mode

### 1. GUI Mode (Traditional)
```bash
# Launch GUI application
goxel
```

### 2. Headless CLI Mode (Quick Operations)
```bash
# Create a new voxel file
goxel --headless create test.gox

# Add some voxels
goxel --headless add-voxel 0 0 0 255 0 0 255
goxel --headless add-voxel 1 0 0 0 255 0 255
goxel --headless add-voxel 0 1 0 0 0 255 255

# Export to OBJ
goxel --headless export test.obj
```

### 3. Daemon Mode (High Performance)
```bash
# Start the daemon (in background)
goxel --headless --daemon --background

# Now use client commands (700% faster!)
goxel --headless create test.gox
goxel --headless add-voxel 0 0 0 255 0 0 255
goxel --headless export test.obj

# Stop daemon when done
goxel --headless --daemon stop
```

### 4. Interactive Mode (Best for Development)
```bash
# Start interactive shell
goxel --headless --interactive

# Now type commands without 'goxel --headless' prefix
goxel> create my-model.gox
Created new project: my-model.gox

goxel> add-voxel 0 0 0 255 0 0 255
Added voxel at (0, 0, 0)

goxel> list-voxels
1 voxel(s) in current layer:
  [0, 0, 0] - Color: #FF0000FF

goxel> export my-model.obj
Exported to: my-model.obj

goxel> exit
```

## Hello Voxel Example

Create your first voxel model in different languages:

### TypeScript (with daemon)
```typescript
import { GoxelClient } from '@goxel/client';

async function createCube() {
  const client = new GoxelClient();
  await client.connect();
  
  // Create new project
  await client.createProject('cube.gox');
  
  // Create a 3x3x3 cube
  for (let x = 0; x < 3; x++) {
    for (let y = 0; y < 3; y++) {
      for (let z = 0; z < 3; z++) {
        await client.addVoxel(x, y, z, [255, 100, 0, 255]);
      }
    }
  }
  
  // Export
  await client.export('cube.obj');
  await client.disconnect();
}

createCube();
```

### Python (with daemon)
```python
from goxel_client import GoxelClient

def create_pyramid():
    client = GoxelClient()
    client.connect()
    
    # Create new project
    client.create_project('pyramid.gox')
    
    # Create a pyramid
    for level in range(5):
        size = 5 - level
        for x in range(size):
            for z in range(size):
                client.add_voxel(x + level, level, z + level, 
                                [0, 255 - level * 50, 0, 255])
    
    # Export
    client.export('pyramid.obj')
    client.disconnect()

if __name__ == '__main__':
    create_pyramid()
```

### Bash Script (batch processing)
```bash
#!/bin/bash
# create_house.sh - Create a simple house

# Start daemon if not running
goxel --headless --daemon status || goxel --headless --daemon --background

# Create house
cat << 'EOF' | goxel --headless --batch -
create house.gox

# Foundation (5x5)
for x in 0 1 2 3 4; do
  for z in 0 1 2 3 4; do
    add-voxel $x 0 $z 128 128 128 255
  done
done

# Walls (4 high)
for y in 1 2 3 4; do
  # Front and back walls
  for x in 0 1 2 3 4; do
    add-voxel $x $y 0 180 100 50 255
    add-voxel $x $y 4 180 100 50 255
  done
  # Side walls
  for z in 1 2 3; do
    add-voxel 0 $y $z 180 100 50 255
    add-voxel 4 $y $z 180 100 50 255
  done
done

# Roof (triangular)
for x in 1 2 3; do
  add-voxel $x 5 2 200 50 50 255
done

export house.obj
EOF
```

## Performance Comparison

### CLI Mode (v13.4 style)
```bash
time for i in {1..10}; do
  goxel --headless create test.gox
  goxel --headless add-voxel 0 0 0 255 0 0 255
  goxel --headless export test.obj
done
# Time: ~140ms
```

### Daemon Mode (v14.6)
```bash
# Start daemon once
goxel --headless --daemon --background

time for i in {1..10}; do
  goxel --headless create test.gox
  goxel --headless add-voxel 0 0 0 255 0 0 255
  goxel --headless export test.obj
done
# Time: ~20ms (700% faster!)
```

## Common Operations

### File Management
```bash
# Create new project
goxel --headless create myproject.gox

# Open existing project
goxel --headless open myproject.gox

# Save current project
goxel --headless save

# Save as different file
goxel --headless save-as backup.gox
```

### Voxel Operations
```bash
# Add voxel (x, y, z, r, g, b, a)
goxel --headless add-voxel 10 20 30 255 128 0 255

# Remove voxel
goxel --headless remove-voxel 10 20 30

# Paint voxel (change color)
goxel --headless paint-voxel 10 20 30 0 255 0 255

# Clear all voxels
goxel --headless clear
```

### Export Formats
```bash
# Export to various formats
goxel --headless export model.obj      # Wavefront OBJ
goxel --headless export model.ply      # Stanford PLY
goxel --headless export model.stl      # STL for 3D printing
goxel --headless export model.gltf     # glTF 2.0
goxel --headless export model.vox      # MagicaVoxel
goxel --headless export model.qubicle  # Qubicle
goxel --headless export model.png      # Image slices
```

### Layer Management
```bash
# Create new layer
goxel --headless layer-create "Background"

# Switch active layer
goxel --headless layer-select 2

# Rename layer
goxel --headless layer-rename 1 "Foreground"

# Toggle layer visibility
goxel --headless layer-visible 1 false
```

## What's Next?

1. **Explore Examples**: Check out the [examples directory](examples/) for more complex projects
2. **Read the User Guide**: Learn about all features in the [User Guide](user-guide.md)
3. **API Documentation**: Build applications with the [API Reference](api-reference.md)
4. **Join the Community**: Get help on [Discord](https://discord.gg/goxel) or [Forums](https://forum.goxel.xyz)

## Need Help?

- **Documentation**: [Full Documentation](README.md)
- **Troubleshooting**: [Common Issues](troubleshooting.md)
- **GitHub Issues**: [Report Bugs](https://github.com/goxel/goxel/issues)
- **Community**: [Discord Server](https://discord.gg/goxel)

---

*Congratulations! You're now ready to create amazing voxel art with Goxel v14.6! 🎨*