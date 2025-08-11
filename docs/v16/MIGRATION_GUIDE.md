# Goxel v0.16 Migration Guide

## Upgrading to the File-Path Render Transfer Architecture

This guide helps you migrate from Goxel v0.15 or earlier to v0.16's efficient file-path based render transfer system.

---

## Table of Contents

1. [What's New](#whats-new)
2. [Breaking Changes](#breaking-changes)
3. [Migration Strategies](#migration-strategies)
4. [Code Examples](#code-examples)
5. [Testing Your Migration](#testing-your-migration)
6. [Rollback Plan](#rollback-plan)

---

## What's New

### Key Features in v0.16

| Feature | v0.15 | v0.16 | Benefit |
|---------|-------|-------|---------|
| **Render Transfer** | Direct file save only | File-path references | 90% less memory |
| **Memory Usage** | Full image in memory | Path reference only | Scales to 4K+ images |
| **Cleanup** | Manual | Automatic with TTL | No disk exhaustion |
| **Concurrency** | Limited | 100+ renders | High throughput |
| **Configuration** | Hard-coded | Environment variables | Flexible deployment |

### New JSON-RPC Methods

```javascript
// New in v0.16
goxel.get_render_info    // Get render file information
goxel.list_renders        // List all active renders
goxel.cleanup_render      // Manually clean up a render
goxel.get_render_stats    // Get render manager statistics
```

---

## Breaking Changes

### None! Full Backward Compatibility

v0.16 maintains 100% backward compatibility. All existing code continues to work unchanged:

```python
# This v0.15 code works perfectly in v0.16
response = daemon.request('goxel.render_scene', ['output.png', 800, 600])
```

However, to benefit from the new architecture, you'll want to adopt the new patterns.

---

## Migration Strategies

### Strategy 1: Gradual Migration (Recommended)

Detect v0.16 features and use them when available:

```python
class GoxelClient:
    def __init__(self, socket_path):
        self.socket_path = socket_path
        self.supports_v16 = self._check_v16_support()
    
    def _check_v16_support(self):
        """Check if daemon supports v0.16 features"""
        try:
            response = self.request('goxel.get_render_stats', {})
            return response.get('success', False)
        except:
            return False
    
    def render_scene(self, width, height, output_path=None):
        if self.supports_v16 and not output_path:
            # Use new file-path mode
            return self._render_v16(width, height)
        else:
            # Use legacy mode
            return self._render_legacy(width, height, output_path)
    
    def _render_v16(self, width, height):
        response = self.request('goxel.render_scene', {
            'width': width,
            'height': height,
            'options': {'return_mode': 'file_path'}
        })
        return response['file']['path']
    
    def _render_legacy(self, width, height, output_path):
        if not output_path:
            output_path = f'/tmp/render_{int(time.time())}.png'
        
        self.request('goxel.render_scene', [output_path, width, height])
        return output_path
```

### Strategy 2: Feature Flag Migration

Use environment variables to control migration:

```python
import os

USE_V16_FEATURES = os.getenv('GOXEL_USE_V16', 'true').lower() == 'true'

def render_scene(daemon, width, height):
    if USE_V16_FEATURES:
        try:
            # Try v0.16 mode
            response = daemon.request('goxel.render_scene', {
                'width': width,
                'height': height,
                'options': {'return_mode': 'file_path'}
            })
            return response['file']['path']
        except:
            # Fall back if not supported
            pass
    
    # Legacy mode
    output = f'/tmp/render_{uuid.uuid4()}.png'
    daemon.request('goxel.render_scene', [output, width, height])
    return output
```

### Strategy 3: Immediate Migration

If you control the daemon version, migrate immediately:

```python
# Before (v0.15)
def render_scene_old(daemon, width, height):
    output_path = f'/tmp/render_{int(time.time())}.png'
    daemon.request('goxel.render_scene', [output_path, width, height])
    
    with open(output_path, 'rb') as f:
        image_data = f.read()
    
    os.remove(output_path)  # Manual cleanup
    return image_data

# After (v0.16)
def render_scene_new(daemon, width, height):
    response = daemon.request('goxel.render_scene', {
        'width': width,
        'height': height,
        'options': {'return_mode': 'file_path'}
    })
    
    file_info = response['file']
    
    with open(file_info['path'], 'rb') as f:
        image_data = f.read()
    
    # No manual cleanup needed - TTL handles it
    return image_data
```

---

## Code Examples

### Python Migration

#### Before (v0.15)

```python
import socket
import json
import os
import base64

class GoxelRendererV15:
    def __init__(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect('/tmp/goxel.sock')
    
    def render_to_base64(self, width, height):
        # Render to file
        output_path = f'/tmp/render_{int(time.time())}.png'
        request = {
            'jsonrpc': '2.0',
            'method': 'goxel.render_scene',
            'params': [output_path, width, height],
            'id': 1
        }
        
        self.sock.send(json.dumps(request).encode() + b'\n')
        response = json.loads(self.sock.recv(8192).decode())
        
        # Read and encode
        with open(output_path, 'rb') as f:
            image_data = f.read()
        
        # Clean up
        os.remove(output_path)
        
        # Return Base64 for transport
        return base64.b64encode(image_data).decode('utf-8')
```

#### After (v0.16)

```python
import socket
import json

class GoxelRendererV16:
    def __init__(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect('/tmp/goxel.sock')
    
    def render_to_path(self, width, height):
        # Use file-path mode
        request = {
            'jsonrpc': '2.0',
            'method': 'goxel.render_scene',
            'params': {
                'width': width,
                'height': height,
                'options': {'return_mode': 'file_path'}
            },
            'id': 1
        }
        
        self.sock.send(json.dumps(request).encode() + b'\n')
        response = json.loads(self.sock.recv(8192).decode())
        
        # Return file info - no Base64 needed!
        return response['result']['file']
    
    def render_to_data(self, width, height):
        # Get file path
        file_info = self.render_to_path(width, height)
        
        # Read if needed
        with open(file_info['path'], 'rb') as f:
            return f.read()
```

### JavaScript/TypeScript Migration

#### Before (v0.15)

```typescript
// v0.15 - Using base64 encoding
class GoxelRendererV15 {
  async renderScene(width: number, height: number): Promise<string> {
    const outputPath = `/tmp/render_${Date.now()}.png`;
    
    await this.daemon.request('goxel.render_scene', [
      outputPath, width, height
    ]);
    
    // Read file and convert to base64
    const imageBuffer = await fs.readFile(outputPath);
    const base64 = imageBuffer.toString('base64');
    
    // Clean up
    await fs.unlink(outputPath);
    
    return `data:image/png;base64,${base64}`;
  }
}
```

#### After (v0.16)

```typescript
// v0.16 - Using file-path references
class GoxelRendererV16 {
  async renderScene(width: number, height: number): Promise<RenderResult> {
    const response = await this.daemon.request('goxel.render_scene', {
      width,
      height,
      options: { return_mode: 'file_path' }
    });
    
    const fileInfo = response.result.file;
    
    return {
      path: fileInfo.path,
      size: fileInfo.size,
      checksum: fileInfo.checksum,
      expiresAt: new Date(fileInfo.expires_at * 1000),
      
      // Helper to read data if needed
      async getData(): Promise<Buffer> {
        return fs.readFile(fileInfo.path);
      }
    };
  }
  
  async cleanupOldRenders(): Promise<void> {
    const response = await this.daemon.request('goxel.list_renders', {});
    const renders = response.result.renders;
    
    // Clean up renders older than 5 minutes
    const fiveMinutesAgo = Date.now() / 1000 - 300;
    
    for (const render of renders) {
      if (render.created_at < fiveMinutesAgo) {
        await this.daemon.request('goxel.cleanup_render', {
          path: render.path
        });
      }
    }
  }
}
```

### MCP Server Migration

#### Before (v0.15)

```typescript
// v0.15 MCP tool implementation
export async function goxel_render_scene(args: RenderArgs) {
  // Limited to export_model workaround
  const result = await goxelDaemonBridge.exportModel(args.outputPath);
  
  return {
    success: result.success,
    message: 'Export completed (rendering not available)',
    content: [{
      type: 'text',
      text: 'Note: This exports the model, not a rendered image.'
    }]
  };
}
```

#### After (v0.16)

```typescript
// v0.16 MCP tool implementation
export async function goxel_render_scene(args: RenderArgs) {
  const result = await goxelDaemonBridge.request('goxel.render_scene', {
    width: args.width || 800,
    height: args.height || 600,
    options: { return_mode: 'file_path' }
  });
  
  if (result.success) {
    const file = result.file;
    
    // Optionally read the image
    let imageData;
    if (args.returnData) {
      imageData = await fs.readFile(file.path);
    }
    
    return {
      success: true,
      message: `Rendered to ${file.path}`,
      data: {
        path: file.path,
        size: file.size,
        dimensions: file.dimensions,
        imageData: imageData?.toString('base64')
      },
      content: [{
        type: 'text',
        text: `✅ Rendered successfully!
Path: ${file.path}
Size: ${(file.size / 1024).toFixed(2)} KB
Dimensions: ${file.dimensions.width}x${file.dimensions.height}
TTL: ${Math.floor((file.expires_at - Date.now()/1000) / 60)} minutes`
      }]
    };
  }
}
```

---

## Testing Your Migration

### 1. Version Detection Test

```python
def test_version_detection():
    """Ensure version detection works correctly"""
    
    client = GoxelClient('/tmp/goxel.sock')
    
    # Should detect v0.16 features
    if daemon_version >= '0.16.1':
        assert client.supports_v16 == True
        print("✓ v0.16 features detected")
    else:
        assert client.supports_v16 == False
        print("✓ Correctly identified older version")
```

### 2. Backward Compatibility Test

```python
def test_backward_compatibility():
    """Ensure legacy code still works"""
    
    # This should work in both v0.15 and v0.16
    response = daemon.request('goxel.render_scene', 
                             ['/tmp/test.png', 200, 200])
    
    assert response['result']['success'] == True
    assert os.path.exists('/tmp/test.png')
    print("✓ Legacy mode works")
```

### 3. Performance Comparison Test

```python
def test_performance_improvement():
    """Compare performance between modes"""
    
    import time
    import psutil
    
    process = psutil.Process()
    
    # Test legacy mode
    mem_before = process.memory_info().rss
    start = time.time()
    
    for i in range(10):
        daemon.request('goxel.render_scene', 
                      [f'/tmp/test_{i}.png', 800, 600])
    
    legacy_time = time.time() - start
    legacy_memory = process.memory_info().rss - mem_before
    
    # Clean up
    for i in range(10):
        os.remove(f'/tmp/test_{i}.png')
    
    # Test file-path mode
    mem_before = process.memory_info().rss
    start = time.time()
    
    for i in range(10):
        daemon.request('goxel.render_scene', {
            'width': 800,
            'height': 600,
            'options': {'return_mode': 'file_path'}
        })
    
    v16_time = time.time() - start
    v16_memory = process.memory_info().rss - mem_before
    
    improvement = ((legacy_time - v16_time) / legacy_time) * 100
    memory_saving = ((legacy_memory - v16_memory) / legacy_memory) * 100
    
    print(f"✓ Performance improvement: {improvement:.1f}% faster")
    print(f"✓ Memory saving: {memory_saving:.1f}% less memory")
```

### 4. Integration Test Suite

```bash
#!/bin/bash
# Run comprehensive migration tests

echo "Running v0.16 Migration Tests"
echo "=============================="

# Test 1: Version detection
python3 -c "
import test_client
client = test_client.GoxelClient()
print('Version:', client.get_version())
print('v0.16 support:', client.supports_v16)
"

# Test 2: Backward compatibility
echo "Testing backward compatibility..."
python3 tests/test_legacy_mode.py

# Test 3: New features
echo "Testing v0.16 features..."
python3 tests/test_v16_features.py

# Test 4: Performance
echo "Running performance comparison..."
python3 tests/benchmark_render_transfer.py

echo "=============================="
echo "Migration tests complete!"
```

---

## Rollback Plan

If you need to rollback to v0.15 behavior:

### 1. Environment Variable Override

```bash
# Force legacy mode
export GOXEL_FORCE_LEGACY_MODE=true
./your-application
```

### 2. Code-Level Rollback

```python
class GoxelClient:
    def __init__(self, force_legacy=False):
        self.force_legacy = force_legacy or \
                           os.getenv('GOXEL_FORCE_LEGACY_MODE') == 'true'
    
    def render_scene(self, width, height):
        if not self.force_legacy:
            try:
                # Try v0.16
                return self._render_v16(width, height)
            except:
                pass
        
        # Always fall back to legacy
        return self._render_legacy(width, height)
```

### 3. Daemon Downgrade

```bash
# Stop v0.16 daemon
pkill goxel-daemon

# Install v0.15
brew install goxel@0.15

# Start v0.15 daemon
goxel-daemon-0.15 --socket /tmp/goxel.sock
```

---

## Migration Checklist

- [ ] **Assess Current Usage**
  - [ ] Identify all render_scene calls
  - [ ] Check for Base64 encoding usage
  - [ ] Review cleanup procedures

- [ ] **Plan Migration Strategy**
  - [ ] Choose gradual vs immediate migration
  - [ ] Set up feature detection
  - [ ] Plan testing approach

- [ ] **Update Code**
  - [ ] Add v0.16 support detection
  - [ ] Update render_scene calls
  - [ ] Remove manual cleanup code
  - [ ] Add error handling for new methods

- [ ] **Configure Environment**
  - [ ] Set GOXEL_RENDER_DIR if needed
  - [ ] Configure GOXEL_RENDER_TTL
  - [ ] Adjust GOXEL_RENDER_MAX_SIZE

- [ ] **Test Migration**
  - [ ] Run backward compatibility tests
  - [ ] Verify performance improvements
  - [ ] Test cleanup functionality
  - [ ] Load test with concurrent renders

- [ ] **Deploy**
  - [ ] Update daemon to v0.16
  - [ ] Deploy application changes
  - [ ] Monitor performance metrics
  - [ ] Verify cleanup is working

- [ ] **Document**
  - [ ] Update API documentation
  - [ ] Document configuration changes
  - [ ] Update deployment procedures

---

## Troubleshooting

### Q: How do I know if my daemon supports v0.16?

```python
def check_daemon_version(daemon):
    try:
        # Try v0.16-specific method
        response = daemon.request('goxel.get_render_stats', {})
        if response.get('result', {}).get('success'):
            return "0.16.1 or higher"
    except:
        pass
    
    return "0.15.x or lower"
```

### Q: What happens to renders when the daemon restarts?

Renders are persisted to disk, so they survive daemon restarts. The cleanup thread will resume managing them based on TTL.

### Q: Can I disable automatic cleanup?

Yes, set a very high TTL:
```bash
export GOXEL_RENDER_TTL=86400000  # ~1000 days
```

### Q: How do I migrate a large codebase gradually?

1. Add version detection to your client library
2. Update high-traffic code paths first
3. Monitor performance improvements
4. Gradually migrate remaining code
5. Remove legacy code after full migration

---

## Support

- **Issues**: https://github.com/guillaumechereau/goxel/issues
- **Documentation**: https://goxel.xyz/docs
- **Discord**: https://discord.gg/goxel

---

**Version**: 0.16.1  
**Last Updated**: 2025-01-10  
**Migration Difficulty**: Easy (Backward Compatible)