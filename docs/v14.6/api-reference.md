# Goxel v14.6 JSON-RPC API Reference

## Overview

The Goxel v14.6 daemon exposes a JSON-RPC 2.0 API for programmatic control of voxel operations. This document provides a complete reference for all available methods.

## Connection Details

### Unix Socket (Default)
```
Socket Path: /tmp/goxel.sock
Protocol: JSON-RPC 2.0 over Unix Domain Socket
```

### TCP Socket (Optional)
```
Default Port: 7531
Protocol: JSON-RPC 2.0 over TCP
Authentication: Token-based (for remote connections)
```

## Request Format

All requests must follow JSON-RPC 2.0 specification:

```json
{
  "jsonrpc": "2.0",
  "method": "method_name",
  "params": {
    "param1": "value1",
    "param2": "value2"
  },
  "id": 1
}
```

## Response Format

### Success Response
```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "success",
    "data": {}
  },
  "id": 1
}
```

### Error Response
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32600,
    "message": "Invalid Request",
    "data": "Additional error information"
  },
  "id": 1
}
```

## API Methods

### Project Management

#### `create_project`
Create a new voxel project.

**Parameters:**
- `filename` (string): Path to save the project file
- `dimensions` (object, optional): Initial volume dimensions
  - `width` (integer): Width in voxels (default: 256)
  - `height` (integer): Height in voxels (default: 256)
  - `depth` (integer): Depth in voxels (default: 256)

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "create_project",
  "params": {
    "filename": "myproject.gox",
    "dimensions": {
      "width": 128,
      "height": 128,
      "depth": 128
    }
  },
  "id": 1
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "success",
    "project_id": "proj_123456",
    "filename": "myproject.gox"
  },
  "id": 1
}
```

#### `open_project`
Open an existing voxel project.

**Parameters:**
- `filename` (string): Path to the project file
- `readonly` (boolean, optional): Open in read-only mode (default: false)

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "open_project",
  "params": {
    "filename": "myproject.gox"
  },
  "id": 2
}
```

#### `save_project`
Save the current project.

**Parameters:**
- `filename` (string, optional): Save to a different file
- `compress` (boolean, optional): Use compression (default: true)

#### `close_project`
Close the current project.

**Parameters:**
- `save` (boolean, optional): Save before closing (default: false)

### Voxel Operations

#### `add_voxel`
Add a single voxel at specified coordinates.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate  
- `z` (integer): Z coordinate
- `color` (array): RGBA color values [r, g, b, a] (0-255)
- `layer_id` (integer, optional): Target layer (default: active layer)

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "add_voxel",
  "params": {
    "x": 10,
    "y": 20,
    "z": 30,
    "color": [255, 128, 0, 255]
  },
  "id": 3
}
```

#### `add_voxels_batch`
Add multiple voxels in a single operation (optimized).

**Parameters:**
- `voxels` (array): Array of voxel objects
  - `x` (integer): X coordinate
  - `y` (integer): Y coordinate
  - `z` (integer): Z coordinate
  - `color` (array): RGBA values [r, g, b, a]
- `layer_id` (integer, optional): Target layer

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "add_voxels_batch",
  "params": {
    "voxels": [
      {"x": 0, "y": 0, "z": 0, "color": [255, 0, 0, 255]},
      {"x": 1, "y": 0, "z": 0, "color": [0, 255, 0, 255]},
      {"x": 2, "y": 0, "z": 0, "color": [0, 0, 255, 255]}
    ]
  },
  "id": 4
}
```

#### `remove_voxel`
Remove a voxel at specified coordinates.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate
- `layer_id` (integer, optional): Target layer

#### `paint_voxel`
Change the color of an existing voxel.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate
- `color` (array): New RGBA color [r, g, b, a]
- `layer_id` (integer, optional): Target layer

#### `get_voxel`
Retrieve voxel information at specified coordinates.

**Parameters:**
- `x` (integer): X coordinate
- `y` (integer): Y coordinate
- `z` (integer): Z coordinate
- `layer_id` (integer, optional): Target layer

**Response:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "exists": true,
    "color": [255, 128, 0, 255],
    "layer_id": 1
  },
  "id": 5
}
```

### Layer Management

#### `create_layer`
Create a new layer.

**Parameters:**
- `name` (string): Layer name
- `visible` (boolean, optional): Initial visibility (default: true)
- `opacity` (number, optional): Layer opacity 0.0-1.0 (default: 1.0)

#### `select_layer`
Set the active layer.

**Parameters:**
- `layer_id` (integer): Layer ID to activate

#### `rename_layer`
Rename a layer.

**Parameters:**
- `layer_id` (integer): Layer ID
- `name` (string): New name

#### `delete_layer`
Delete a layer.

**Parameters:**
- `layer_id` (integer): Layer ID to delete
- `merge_down` (boolean, optional): Merge with layer below (default: false)

#### `list_layers`
Get information about all layers.

**Response:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "layers": [
      {
        "id": 1,
        "name": "Base Layer",
        "visible": true,
        "opacity": 1.0,
        "voxel_count": 1523
      },
      {
        "id": 2,
        "name": "Details",
        "visible": true,
        "opacity": 0.8,
        "voxel_count": 342
      }
    ],
    "active_layer_id": 2
  },
  "id": 6
}
```

### Export Operations

#### `export`
Export the project to various formats.

**Parameters:**
- `filename` (string): Output filename
- `format` (string, optional): Export format (auto-detected from extension)
- `options` (object, optional): Format-specific options
  - `scale` (number): Scale factor (default: 1.0)
  - `optimize` (boolean): Optimize mesh (default: true)
  - `colors` (boolean): Include colors (default: true)

**Supported Formats:**
- `obj` - Wavefront OBJ
- `ply` - Stanford PLY
- `stl` - STL (3D printing)
- `gltf` / `glb` - glTF 2.0
- `vox` - MagicaVoxel
- `qubicle` - Qubicle
- `png` - Image slices
- `bvx` - BinVox

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "export",
  "params": {
    "filename": "model.gltf",
    "options": {
      "scale": 0.1,
      "optimize": true
    }
  },
  "id": 7
}
```

### Batch Operations

#### `execute_batch`
Execute multiple operations in a single transaction.

**Parameters:**
- `operations` (array): Array of operation objects
- `atomic` (boolean, optional): All-or-nothing execution (default: true)

**Example:**
```json
{
  "jsonrpc": "2.0",
  "method": "execute_batch",
  "params": {
    "operations": [
      {"method": "create_layer", "params": {"name": "Sky"}},
      {"method": "add_voxel", "params": {"x": 0, "y": 100, "z": 0, "color": [135, 206, 235, 255]}},
      {"method": "add_voxel", "params": {"x": 1, "y": 100, "z": 0, "color": [135, 206, 235, 255]}}
    ]
  },
  "id": 8
}
```

### System Operations

#### `get_status`
Get daemon status and statistics.

**Response:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "version": "14.6.0",
    "uptime": 3600,
    "memory_usage": 45678901,
    "active_connections": 3,
    "current_project": "myproject.gox",
    "total_voxels": 15234
  },
  "id": 9
}
```

#### `shutdown`
Shutdown the daemon gracefully.

**Parameters:**
- `save_all` (boolean, optional): Save all open projects (default: true)
- `timeout` (integer, optional): Shutdown timeout in seconds (default: 30)

## Error Codes

### Standard JSON-RPC Errors
- `-32700` - Parse error
- `-32600` - Invalid Request
- `-32601` - Method not found
- `-32602` - Invalid params
- `-32603` - Internal error

### Goxel-Specific Errors
- `-30001` - Project not found
- `-30002` - Invalid coordinates
- `-30003` - Layer not found
- `-30004` - Export format not supported
- `-30005` - Operation failed
- `-30006` - Resource limit exceeded
- `-30007` - Permission denied
- `-30008` - File I/O error

## Performance Tips

1. **Use Batch Operations**: Combine multiple voxel operations into `add_voxels_batch` or `execute_batch`
2. **Connection Pooling**: Reuse connections instead of creating new ones
3. **Async Processing**: Use async/await patterns in client code
4. **Compression**: Enable compression for large operations
5. **Layer Management**: Work on specific layers to reduce processing

## Client Libraries

Official client libraries are available for:
- **TypeScript/JavaScript**: `npm install @goxel/client`
- **Python**: `pip install goxel-client`
- **C++**: Include `goxel_client.hpp`
- **Go**: `go get github.com/goxel/go-client`

See [Client Libraries Documentation](client-libraries.md) for usage examples.

---

*For more examples and advanced usage, see the [Developer Guide](developer-guide.md)*