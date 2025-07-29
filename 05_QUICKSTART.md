# 05_QUICKSTART - Goxel Quick Start Guide

## 5 Minutes to Your First Voxel

### 1. Install Dependencies

**macOS:**
```bash
brew install scons glfw tre
```

**Linux:**
```bash
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev
```

### 2. Build Goxel Daemon

```bash
# Clone repository
git clone https://github.com/guillaumechereau/goxel.git
cd goxel

# Build daemon
scons daemon=1

# Verify build
./goxel-daemon --version
```

### 3. Start the Daemon

```bash
# Start in foreground mode
./goxel-daemon --foreground --socket /tmp/goxel.sock
```

### 4. Create Your First Voxel

**Using TypeScript Client:**

```bash
# Install client
cd src/mcp-client
npm install

# Create example script
cat > first_voxel.ts << 'EOF'
import { GoxelClient } from './index';

async function main() {
  const client = new GoxelClient('/tmp/goxel.sock');
  
  // Create new project
  await client.createProject({ 
    name: 'my_first_voxel',
    size: [32, 32, 32] 
  });
  
  // Add a red cube
  await client.addVoxels([
    { x: 16, y: 16, z: 16, color: [255, 0, 0, 255] }
  ]);
  
  // Save project
  await client.saveProject({ path: 'first_voxel.gox' });
  
  // Export as OBJ
  await client.export({ 
    format: 'obj', 
    path: 'first_voxel.obj' 
  });
  
  console.log('Created first_voxel.gox and first_voxel.obj');
}

main().catch(console.error);
EOF

# Run it
npx ts-node first_voxel.ts
```

### 5. Quick Examples

**Create a 3x3x3 Cube:**
```typescript
const positions = [];
for (let x = 0; x < 3; x++) {
  for (let y = 0; y < 3; y++) {
    for (let z = 0; z < 3; z++) {
      positions.push({ x, y, z, color: [255, 128, 0, 255] });
    }
  }
}
await client.addVoxels(positions);
```

**Render Preview:**
```typescript
const image = await client.render({
  width: 800,
  height: 600,
  camera: {
    position: [30, 30, 30],
    target: [16, 16, 16]
  }
});
// Returns base64 PNG image
```

## GUI Alternative

For visual editing, use the GUI version:
```bash
# Build GUI
scons

# Run
./goxel
```

## Next Steps

- Read the [API Reference](04_API.md) for all methods
- Check [Architecture](01_ARCHITECTURE.md) for system design
- See [Build Guide](03_BUILD.md) for platform-specific notes

## Troubleshooting

**Socket Error?**
```bash
# Check if daemon is running
ps aux | grep goxel-daemon

# Check socket exists
ls -la /tmp/goxel.sock
```

**Permission Denied?**
```bash
# Fix socket permissions
chmod 755 /tmp
```

**Build Failed?**
```bash
# Clean and rebuild
scons -c
scons daemon=1
```