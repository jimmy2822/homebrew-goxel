# Goxel v14.0 Quick Start Guide

**Version**: 14.0.0-beta  
**Last Updated**: January 28, 2025

## 🚀 Getting Started in 5 Minutes

This guide will help you get Goxel v14.0 daemon up and running quickly. The daemon architecture provides **683% performance improvement** over the traditional CLI, with optimizations underway to exceed 700%.

## Prerequisites

- Unix-like system (macOS, Linux, BSD)
- Basic terminal knowledge
- Node.js 16+ (for TypeScript client)
- Git (for source installation)

## Installation

### Option 1: Pre-built Binary (Recommended)

```bash
# Download the latest release
curl -L https://github.com/goxel/goxel/releases/download/v14.0.0/goxel-daemon-<platform> -o goxel-daemon
chmod +x goxel-daemon

# Start the daemon
./goxel-daemon --socket /tmp/goxel.sock --foreground
```

### Option 2: Build from Source

```bash
# Clone the repository
git clone https://github.com/goxel/goxel.git
cd goxel

# Build the daemon
scons daemon=1 mode=release

# The daemon binary will be at ./goxel-daemon
```

## Quick Test

### 1. Start the Daemon

```bash
# Start in foreground mode for testing
./goxel-daemon --socket /tmp/goxel.sock --foreground

# You should see:
# Goxel Daemon v14.0.0
# Listening on socket: /tmp/goxel.sock
# Worker pool: 4 threads ready
```

### 2. Send a Test Command

In another terminal:

```bash
# Test echo method
echo '{"jsonrpc":"2.0","method":"echo","params":{"message":"Hello Goxel!"},"id":1}' | nc -U /tmp/goxel.sock

# Expected response:
# {"jsonrpc":"2.0","result":{"message":"Hello Goxel!"},"id":1}
```

### 3. Create Your First Voxel

```bash
# Create a project and add a red voxel
cat << EOF | nc -U /tmp/goxel.sock
{"jsonrpc":"2.0","method":"create_project","params":{"name":"quickstart"},"id":1}
{"jsonrpc":"2.0","method":"add_voxel","params":{"x":0,"y":0,"z":0,"color":[255,0,0,255]},"id":2}
{"jsonrpc":"2.0","method":"save_file","params":{"path":"quickstart.gox"},"id":3}
EOF
```

## Using the TypeScript Client

### Install the Client

```bash
# Install the Goxel daemon client
npm install @goxel/daemon-client

# Or use the local version
cd goxel/src/mcp-client
npm install
npm link
```

### Basic Example

```typescript
import { GoxelDaemonClient } from '@goxel/daemon-client';

async function quickStart() {
  // Connect to the daemon
  const client = new GoxelDaemonClient('/tmp/goxel.sock');
  await client.connect();
  
  // Create a new project
  await client.call('create_project', { name: 'my-project' });
  
  // Add some voxels
  await client.call('add_voxel', { 
    x: 0, y: 0, z: 0, 
    color: [255, 0, 0, 255] // Red
  });
  
  await client.call('add_voxel', { 
    x: 1, y: 0, z: 0, 
    color: [0, 255, 0, 255] // Green
  });
  
  // Export as OBJ
  await client.call('export_model', {
    path: 'my-model.obj',
    format: 'obj'
  });
  
  // Clean up
  await client.disconnect();
}

quickStart().catch(console.error);
```

## Common Operations

### Creating a Simple Structure

```javascript
// Create a 3x3x3 cube
async function createCube(client, size = 3, color = [255, 255, 255, 255]) {
  const voxels = [];
  for (let x = 0; x < size; x++) {
    for (let y = 0; y < size; y++) {
      for (let z = 0; z < size; z++) {
        voxels.push({ x, y, z, color });
      }
    }
  }
  
  // Batch add for performance
  await client.call('add_voxels', { voxels });
}
```

### Working with Layers

```javascript
// Create multiple layers
await client.call('create_layer', { name: 'Base' });
await client.call('create_layer', { name: 'Details' });

// Switch to a layer
await client.call('set_active_layer', { id: 1 });

// List all layers
const layers = await client.call('list_layers');
console.log('Layers:', layers);
```

### Exporting Models

```javascript
// Export in various formats
const formats = ['obj', 'ply', 'stl', 'gltf'];

for (const format of formats) {
  await client.call('export_model', {
    path: `model.${format}`,
    format: format
  });
}
```

## Performance Tips

### 1. Use Batch Operations

```javascript
// ❌ Slow: Individual calls
for (const voxel of voxels) {
  await client.call('add_voxel', voxel);
}

// ✅ Fast: Batch call
await client.call('add_voxels', { voxels });
```

### 2. Connection Pooling

```javascript
// The TypeScript client automatically manages connection pooling
const client = new GoxelDaemonClient('/tmp/goxel.sock', {
  maxConnections: 10,  // Default: 10
  minConnections: 2    // Default: 2
});
```

### 3. Keep Daemon Running

The daemon's 683% performance improvement comes from avoiding process startup overhead. Keep it running for best results:

```bash
# Start as a service (Linux with systemd)
sudo systemctl start goxel-daemon
sudo systemctl enable goxel-daemon

# Or use a process manager
pm2 start goxel-daemon -- --socket /tmp/goxel.sock
```

## Migrating from CLI

### CLI Command → Daemon Method

| CLI Command | Daemon Method | Example |
|-------------|---------------|---------|
| `goxel-headless create file.gox` | `create_project` | `{"method":"create_project","params":{"name":"file"}}` |
| `goxel-headless add-voxel x y z r g b a` | `add_voxel` | `{"method":"add_voxel","params":{"x":0,"y":0,"z":0,"color":[255,0,0,255]}}` |
| `goxel-headless export file.obj` | `export_model` | `{"method":"export_model","params":{"path":"file.obj","format":"obj"}}` |

### Using the Compatibility Script

```bash
# Drop-in replacement for CLI (when using MCP)
export GOXEL_USE_DAEMON=1
goxel-headless create test.gox  # Automatically uses daemon if available
```

## Troubleshooting

### Daemon Won't Start

```bash
# Check if socket already exists
ls -la /tmp/goxel.sock
# If it exists, remove it
rm /tmp/goxel.sock

# Check permissions
ls -la /tmp | grep goxel

# Start with debug logging
./goxel-daemon --socket /tmp/goxel.sock --log-level debug --foreground
```

### Connection Refused

```bash
# Verify daemon is running
ps aux | grep goxel-daemon

# Check socket exists
test -S /tmp/goxel.sock && echo "Socket exists" || echo "Socket missing"

# Test with netcat
echo '{"jsonrpc":"2.0","method":"ping","id":1}' | nc -U /tmp/goxel.sock
```

### Performance Not as Expected

Current benchmarks show 683% improvement. To achieve the full 700%+:

```bash
# Increase worker threads (default: 4)
./goxel-daemon --workers 8

# Use batch operations in your code
# Enable connection pooling in clients
# Keep the daemon warm with periodic pings
```

## Next Steps

- **Read the API Reference**: [daemon_api_reference.md](daemon_api_reference.md)
- **View Integration Examples**: [integration_guide.md](integration_guide.md)
- **Deploy to Production**: [deployment.md](deployment.md)
- **Optimize Performance**: See performance tips above

## Getting Help

- **GitHub Issues**: https://github.com/goxel/goxel/issues
- **Documentation**: https://goxel.xyz/docs/v14
- **Community Forum**: https://forum.goxel.xyz

---

Welcome to Goxel v14.0! The daemon architecture provides enterprise-grade performance for all your voxel editing needs. Start with the examples above and explore the full API for advanced features.