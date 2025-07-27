# Goxel v14.0 JSON RPC API Reference

This document describes the JSON RPC 2.0 API provided by the Goxel daemon.

## Table of Contents

1. [Overview](#overview)
2. [Connection Methods](#connection-methods)
3. [Authentication](#authentication)
4. [Request Format](#request-format)
5. [Response Format](#response-format)
6. [Error Codes](#error-codes)
7. [API Methods](#api-methods)
   - [Daemon Control](#daemon-control)
   - [Project Management](#project-management)
   - [Voxel Operations](#voxel-operations)
   - [Layer Management](#layer-management)
   - [Shape Generation](#shape-generation)
   - [Export Operations](#export-operations)
   - [Camera Control](#camera-control)
   - [Rendering](#rendering)

## Overview

The Goxel daemon exposes a JSON RPC 2.0 API over Unix domain sockets (Linux/macOS) or named pipes (Windows). The API provides programmatic access to all Goxel functionality with high performance.

### Key Features
- JSON RPC 2.0 compliant
- Batch request support
- Async notifications
- Concurrent request handling
- Type-safe parameters

## Connection Methods

### Unix Domain Socket (Default)
```javascript
// Node.js example
const net = require('net');
const client = net.createConnection('/var/run/goxel/goxel.sock');
```

### TCP Socket (Optional)
```javascript
// When TCP is enabled in config
const client = net.createConnection(7890, 'localhost');
```

## Authentication

Authentication is disabled by default. When enabled in the configuration:

```json
{
  "jsonrpc": "2.0",
  "method": "auth.login",
  "params": {
    "token": "your-shared-secret"
  },
  "id": 1
}
```

## Request Format

### Single Request
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add",
  "params": {
    "x": 0,
    "y": 0,
    "z": 0,
    "r": 255,
    "g": 0,
    "b": 0,
    "a": 255
  },
  "id": 1
}
```

### Batch Request
```json
[
  {"jsonrpc": "2.0", "method": "project.create", "params": {"path": "test.gox"}, "id": 1},
  {"jsonrpc": "2.0", "method": "voxel.add", "params": {"x": 0, "y": 0, "z": 0, "r": 255, "g": 0, "b": 0, "a": 255}, "id": 2},
  {"jsonrpc": "2.0", "method": "project.save", "id": 3}
]
```

## Response Format

### Success Response
```json
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "message": "Voxel added successfully"
  },
  "id": 1
}
```

### Error Response
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": {
      "field": "x",
      "reason": "out of range"
    }
  },
  "id": 1
}
```

## Error Codes

| Code | Message | Description |
|------|---------|-------------|
| -32700 | Parse error | Invalid JSON |
| -32600 | Invalid Request | Not a valid JSON RPC request |
| -32601 | Method not found | Unknown method |
| -32602 | Invalid params | Invalid method parameters |
| -32603 | Internal error | Internal daemon error |
| -32000 | Project error | Project-related error |
| -32001 | IO error | File I/O error |
| -32002 | Memory error | Out of memory |
| -32003 | State error | Invalid operation state |

## API Methods

### Daemon Control

#### daemon.status
Get daemon status information.

**Parameters:** None

**Returns:**
```json
{
  "version": "14.0.0",
  "uptime": 3600,
  "connections": 5,
  "requests_processed": 1234,
  "worker_threads": 4,
  "memory_usage": 45678912
}
```

#### daemon.stats
Get detailed statistics.

**Parameters:** None

**Returns:**
```json
{
  "requests": {
    "total": 10000,
    "per_second": 523,
    "average_time_ms": 1.75
  },
  "clients": {
    "active": 3,
    "total": 25
  },
  "performance": {
    "cpu_percent": 12.5,
    "memory_mb": 43.6
  }
}
```

#### daemon.shutdown
Gracefully shutdown the daemon.

**Parameters:**
- `timeout` (integer, optional): Shutdown timeout in milliseconds

**Returns:**
```json
{
  "message": "Daemon shutting down"
}
```

### Project Management

#### project.create
Create a new project.

**Parameters:**
- `path` (string, optional): Project file path

**Returns:**
```json
{
  "success": true,
  "project_id": "uuid-1234"
}
```

#### project.open
Open an existing project.

**Parameters:**
- `path` (string): Path to project file

**Returns:**
```json
{
  "success": true,
  "project_id": "uuid-1234",
  "layers": 2,
  "voxel_count": 1523
}
```

#### project.save
Save the current project.

**Parameters:**
- `path` (string, optional): Save to specific path

**Returns:**
```json
{
  "success": true,
  "bytes_written": 45678
}
```

#### project.close
Close the current project.

**Parameters:** None

**Returns:**
```json
{
  "success": true
}
```

### Voxel Operations

#### voxel.add
Add a single voxel.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate
- `r` (integer): Red (0-255)
- `g` (integer): Green (0-255)
- `b` (integer): Blue (0-255)
- `a` (integer): Alpha (0-255)

**Returns:**
```json
{
  "success": true
}
```

#### voxel.remove
Remove a voxel at position.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate

**Returns:**
```json
{
  "success": true,
  "removed": true
}
```

#### voxel.get
Get voxel at position.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate

**Returns:**
```json
{
  "exists": true,
  "color": {
    "r": 255,
    "g": 0,
    "b": 0,
    "a": 255
  }
}
```

#### voxel.fill
Fill a region with voxels.

**Parameters:**
- `start` (object): Start position {x, y, z}
- `end` (object): End position {x, y, z}
- `color` (object): Color {r, g, b, a}

**Returns:**
```json
{
  "success": true,
  "voxels_added": 125
}
```

### Layer Management

#### layer.add
Add a new layer.

**Parameters:**
- `name` (string, optional): Layer name

**Returns:**
```json
{
  "success": true,
  "layer_id": 2,
  "name": "Layer 2"
}
```

#### layer.select
Select active layer.

**Parameters:**
- `index` (integer): Layer index

**Returns:**
```json
{
  "success": true
}
```

#### layer.remove
Remove a layer.

**Parameters:**
- `index` (integer): Layer index

**Returns:**
```json
{
  "success": true
}
```

#### layer.list
List all layers.

**Parameters:** None

**Returns:**
```json
{
  "layers": [
    {"index": 0, "name": "Layer 1", "visible": true, "voxel_count": 523},
    {"index": 1, "name": "Layer 2", "visible": false, "voxel_count": 102}
  ]
}
```

### Shape Generation

#### shape.sphere
Create a sphere.

**Parameters:**
- `center` (array): Center position [x, y, z]
- `radius` (number): Sphere radius
- `color` (array): Color [r, g, b, a]
- `filled` (boolean, optional): Filled or hollow

**Returns:**
```json
{
  "success": true,
  "voxels_added": 523
}
```

#### shape.box
Create a box.

**Parameters:**
- `start` (array): Start corner [x, y, z]
- `end` (array): End corner [x, y, z]
- `color` (array): Color [r, g, b, a]
- `filled` (boolean, optional): Filled or hollow

**Returns:**
```json
{
  "success": true,
  "voxels_added": 216
}
```

#### shape.cylinder
Create a cylinder.

**Parameters:**
- `base` (array): Base center [x, y, z]
- `height` (number): Cylinder height
- `radius` (number): Cylinder radius
- `axis` (string): "x", "y", or "z"
- `color` (array): Color [r, g, b, a]

**Returns:**
```json
{
  "success": true,
  "voxels_added": 314
}
```

### Export Operations

#### export.obj
Export as OBJ file.

**Parameters:**
- `path` (string): Output file path
- `options` (object, optional):
  - `merge_vertices` (boolean): Merge duplicate vertices
  - `optimize` (boolean): Optimize mesh
  - `scale` (number): Scale factor

**Returns:**
```json
{
  "success": true,
  "vertices": 1523,
  "faces": 2456,
  "file_size": 45678
}
```

#### export.ply
Export as PLY file.

**Parameters:**
- `path` (string): Output file path
- `binary` (boolean, optional): Binary format

**Returns:**
```json
{
  "success": true,
  "file_size": 34567
}
```

#### export.vox
Export as MagicaVoxel format.

**Parameters:**
- `path` (string): Output file path

**Returns:**
```json
{
  "success": true,
  "file_size": 23456
}
```

#### export.png
Export as PNG slice.

**Parameters:**
- `path` (string): Output file path
- `axis` (string): "x", "y", or "z"
- `slice` (integer): Slice position

**Returns:**
```json
{
  "success": true,
  "width": 256,
  "height": 256
}
```

### Camera Control

#### camera.set
Set camera position and orientation.

**Parameters:**
- `position` (array): Camera position [x, y, z]
- `target` (array): Look-at target [x, y, z]
- `up` (array, optional): Up vector [x, y, z]

**Returns:**
```json
{
  "success": true
}
```

#### camera.get
Get current camera state.

**Parameters:** None

**Returns:**
```json
{
  "position": [10, 20, 30],
  "target": [0, 0, 0],
  "up": [0, 1, 0],
  "fov": 60
}
```

### Rendering

#### render.screenshot
Render a screenshot.

**Parameters:**
- `path` (string): Output image path
- `width` (integer): Image width
- `height` (integer): Image height
- `samples` (integer, optional): Anti-aliasing samples

**Returns:**
```json
{
  "success": true,
  "render_time_ms": 125
}
```

## Client Libraries

### TypeScript/JavaScript
```typescript
import { GoxelDaemonClient } from 'goxel-daemon-client';

const client = new GoxelDaemonClient();
await client.connect();

// Create project
await client.request('project.create', { path: 'mymodel.gox' });

// Add voxels
await client.request('voxel.add', {
  x: 0, y: 0, z: 0,
  r: 255, g: 0, b: 0, a: 255
});

// Export
await client.request('export.obj', { path: 'mymodel.obj' });
```

### Python (Coming in v14.1)
```python
from goxel import DaemonClient

with DaemonClient() as client:
    client.project_create('mymodel.gox')
    client.voxel_add(0, 0, 0, color=(255, 0, 0))
    client.export_obj('mymodel.obj')
```

## Performance Tips

1. **Batch Operations**: Send multiple operations in a single batch request
2. **Keep Connections Open**: Reuse connections for multiple operations
3. **Use Appropriate Methods**: Use `voxel.fill` instead of many `voxel.add` calls
4. **Monitor Stats**: Use `daemon.stats` to track performance

## Examples

See the `examples/` directory for complete working examples:
- Basic CRUD operations
- Shape generation
- Batch processing
- Performance benchmarks
- TypeScript client usage

---

For more information, visit https://goxel.xyz/docs/v14/api