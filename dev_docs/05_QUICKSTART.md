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

**Using JSON-RPC directly (Python example):**

```python
import json
import socket

# Connect to daemon
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect('/tmp/goxel.sock')

def call_rpc(method, params=None):
    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": params or {}
    }
    sock.send(json.dumps(request).encode() + b'\n')
    response = json.loads(sock.recv(4096).decode())
    return response['result']

# Create new project
call_rpc('create_project', {'name': 'my_first_voxel'})

# Add a red cube
call_rpc('add_voxel', {
    'x': 16, 'y': 16, 'z': 16,
    'r': 255, 'g': 0, 'b': 0, 'a': 255
})

# Save project
call_rpc('save_project', {'path': 'first_voxel.gox'})

# Export as OBJ
call_rpc('export_project', {
    'format': 'obj',
    'path': 'first_voxel.obj'
})

print('Created first_voxel.gox and first_voxel.obj')
sock.close()
```

### 5. Quick Examples

**Create a 3x3x3 Cube:**
```python
# Add multiple voxels to create a cube
for x in range(3):
    for y in range(3):
        for z in range(3):
            call_rpc('add_voxel', {
                'x': x, 'y': y, 'z': z,
                'r': 255, 'g': 128, 'b': 0, 'a': 255
            })
```

**Render Preview:**
```python
# Render the scene
result = call_rpc('render_image', {
    'width': 800,
    'height': 600,
    'camera_position': [30, 30, 30],
    'camera_target': [16, 16, 16]
})
# Returns base64 PNG image in result['image']
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