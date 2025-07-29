# 04_API - Goxel API Reference

## Overview

Goxel v14 provides a JSON-RPC 2.0 API for voxel editing operations through the daemon server.

**Protocol**: JSON-RPC 2.0  
**Transport**: Unix Socket (`/tmp/goxel.sock`)  
**Client Libraries**: TypeScript/JavaScript

## Core Methods

### Project Management

#### goxel.create_project
Creates a new voxel project.
```json
{
  "method": "goxel.create_project",
  "params": {
    "name": "my_project",
    "size": [128, 128, 128]
  }
}
```

#### goxel.open_project
Opens existing project file.
```json
{
  "method": "goxel.open_project",
  "params": {
    "path": "/path/to/project.gox"
  }
}
```

#### goxel.save_project
Saves current project.
```json
{
  "method": "goxel.save_project",
  "params": {
    "path": "/path/to/project.gox"
  }
}
```

### Voxel Operations

#### goxel.add_voxels
Adds voxels at specified positions.
```json
{
  "method": "goxel.add_voxels",
  "params": {
    "voxels": [
      {"x": 0, "y": 0, "z": 0, "color": [255, 0, 0, 255]}
    ]
  }
}
```

#### goxel.remove_voxels
Removes voxels from positions.
```json
{
  "method": "goxel.remove_voxels",
  "params": {
    "positions": [{"x": 0, "y": 0, "z": 0}]
  }
}
```

#### goxel.paint_voxels
Changes color of existing voxels.
```json
{
  "method": "goxel.paint_voxels",
  "params": {
    "position": {"x": 0, "y": 0, "z": 0},
    "color": [0, 255, 0, 255],
    "radius": 2
  }
}
```

### Layer Management

#### goxel.create_layer
Creates new layer.
```json
{
  "method": "goxel.create_layer",
  "params": {
    "name": "New Layer"
  }
}
```

#### goxel.list_layers
Lists all layers.
```json
{
  "method": "goxel.list_layers"
}
```

### Export Operations

#### goxel.export
Exports to various formats.
```json
{
  "method": "goxel.export",
  "params": {
    "format": "obj",
    "path": "/path/to/output.obj"
  }
}
```

Supported formats: `obj`, `ply`, `stl`, `gltf`, `vox`, `png`

### Rendering

#### goxel.render
Renders current view to image.
```json
{
  "method": "goxel.render",
  "params": {
    "width": 800,
    "height": 600,
    "camera": {
      "position": [10, 10, 10],
      "target": [0, 0, 0]
    }
  }
}
```

## TypeScript Client Example

```typescript
import { GoxelClient } from '@goxel/client';

const client = new GoxelClient('/tmp/goxel.sock');

// Create project
await client.createProject({ name: 'test', size: [64, 64, 64] });

// Add voxels
await client.addVoxels([
  { x: 0, y: 0, z: 0, color: [255, 0, 0, 255] },
  { x: 1, y: 0, z: 0, color: [0, 255, 0, 255] }
]);

// Export
await client.export({ format: 'obj', path: 'output.obj' });
```

## Error Codes

- `-32700`: Parse error
- `-32600`: Invalid request
- `-32601`: Method not found
- `-32602`: Invalid params
- `-32000`: Server error

## Performance

The daemon architecture provides:
- **Concurrent Processing**: Multiple operations in parallel
- **Connection Pooling**: Reuse connections for efficiency
- **Batch Operations**: Send multiple operations in one request
- **683% Performance Improvement**: Over sequential operations