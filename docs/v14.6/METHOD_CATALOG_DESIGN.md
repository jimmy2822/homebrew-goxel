# Goxel v14.6 RPC Method Catalog Design

**Author**: Yuki Tanaka (Agent-3)  
**Date**: January 2025  
**Version**: 1.0  
**Status**: Draft

## Executive Summary

This document provides a comprehensive catalog of all JSON-RPC methods for Goxel v14.6's daemon interface. It maps existing v13.4 CLI commands to RPC methods, introduces new daemon-specific functionality, and defines batch operation support with standardized error handling.

## Table of Contents

1. [Method Organization](#method-organization)
2. [System Methods](#system-methods)
3. [Project Methods](#project-methods)
4. [Voxel Methods](#voxel-methods)
5. [Layer Methods](#layer-methods)
6. [Render Methods](#render-methods)
7. [Export Methods](#export-methods)
8. [Import Methods](#import-methods)
9. [Script Methods](#script-methods)
10. [Batch Operations](#batch-operations)
11. [Daemon-Specific Methods](#daemon-specific-methods)
12. [Error Code Reference](#error-code-reference)

## Method Organization

### Namespace Convention

All methods follow the pattern: `namespace.action[.detail]`

**Core Namespaces**:
- `system.*` - Daemon control and monitoring
- `project.*` - Project lifecycle management
- `voxel.*` - Voxel manipulation operations
- `layer.*` - Layer management
- `render.*` - Rendering operations
- `export.*` - Export functionality
- `import.*` - Import functionality
- `script.*` - Scripting interface
- `batch.*` - Batch processing utilities

### Method Categories

1. **Query Methods**: Return information without modifying state
2. **Mutation Methods**: Modify project state
3. **Notification Methods**: One-way messages (no response)
4. **Stream Methods**: Return data progressively

## System Methods

### system.get_version
**CLI Equivalent**: `goxel --headless --version`  
**Description**: Returns version information about the daemon and protocol

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "system.get_version",
  "id": 1
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "daemon_version": "14.6.0",
    "protocol_version": "2.0",
    "api_version": "1.0",
    "goxel_core_version": "14.6.0",
    "build_date": "2025-01-27",
    "build_commit": "abc123def",
    "features": ["batch", "compression", "scripting", "streaming"],
    "platform": "darwin-arm64"
  },
  "id": 1
}
```

### system.get_status
**CLI Equivalent**: None (new daemon feature)  
**Description**: Returns current daemon status and resource usage

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "system.get_status",
  "id": 2
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "uptime": 3600,
    "memory": {
      "used": 104857600,
      "heap_total": 134217728,
      "heap_used": 104857600,
      "external": 2097152,
      "array_buffers": 1048576
    },
    "connections": {
      "active": 3,
      "total": 10,
      "rejected": 0
    },
    "projects": {
      "open": 2,
      "total_created": 15
    },
    "performance": {
      "cpu_usage": 2.5,
      "requests_per_second": 45.2,
      "average_response_time": 1.2
    }
  },
  "id": 2
}
```

### system.ping
**CLI Equivalent**: None  
**Description**: Health check endpoint for connection monitoring

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "system.ping",
  "params": {
    "timestamp": 1706356800000
  },
  "id": 3
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "pong": true,
    "timestamp": 1706356800001,
    "latency": 1
  },
  "id": 3
}
```

### system.shutdown
**CLI Equivalent**: None  
**Description**: Gracefully shuts down the daemon

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "system.shutdown",
  "params": {
    "force": false,
    "timeout": 30000
  },
  "id": 4
}
```

### system.list_methods
**CLI Equivalent**: `goxel --headless --help`  
**Description**: Returns list of all available RPC methods

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "system.list_methods",
  "params": {
    "namespace": "voxel",
    "include_descriptions": true
  },
  "id": 5
}
```

## Project Methods

### project.create
**CLI Equivalent**: `goxel --headless create <filename>`  
**Description**: Creates a new project

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.create",
  "params": {
    "name": "MyProject",
    "path": "/tmp/myproject.gox",
    "dimensions": {
      "width": 64,
      "height": 64,
      "depth": 64
    },
    "options": {
      "background_color": [0, 0, 0, 0],
      "grid_size": 16,
      "auto_save": true
    }
  },
  "id": 10
}
```

### project.open
**CLI Equivalent**: `goxel --headless open <filename>`  
**Description**: Opens an existing project file

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.open",
  "params": {
    "path": "/tmp/existing.gox",
    "read_only": false
  },
  "id": 11
}
```

### project.save
**CLI Equivalent**: `goxel --headless save`  
**Description**: Saves the current project

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.save",
  "params": {
    "path": "/tmp/saved.gox",
    "create_backup": true,
    "compress": true
  },
  "id": 12
}
```

### project.get_info
**CLI Equivalent**: None  
**Description**: Returns information about the current project

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.get_info",
  "id": 13
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "name": "MyProject",
    "path": "/tmp/myproject.gox",
    "created": "2025-01-27T10:00:00Z",
    "modified": "2025-01-27T11:30:00Z",
    "size": 45678,
    "dimensions": {
      "width": 64,
      "height": 64,
      "depth": 64
    },
    "statistics": {
      "total_voxels": 1234,
      "layers": 3,
      "colors_used": 15
    },
    "is_modified": true
  },
  "id": 13
}
```

### project.close
**CLI Equivalent**: None  
**Description**: Closes the current project

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.close",
  "params": {
    "save_changes": true
  },
  "id": 14
}
```

### project.get_history
**CLI Equivalent**: None  
**Description**: Returns undo/redo history information

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.get_history",
  "params": {
    "limit": 10
  },
  "id": 15
}
```

## Voxel Methods

### voxel.add
**CLI Equivalent**: `goxel --headless add-voxel <x> <y> <z> <r> <g> <b> <a>`  
**Description**: Adds a single voxel at specified coordinates

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add",
  "params": {
    "x": 0,
    "y": -16,
    "z": 0,
    "color": {
      "r": 255,
      "g": 0,
      "b": 0,
      "a": 255
    },
    "layer_id": "current"
  },
  "id": 20
}
```

### voxel.remove
**CLI Equivalent**: `goxel --headless remove-voxel <x> <y> <z>`  
**Description**: Removes a voxel at specified coordinates

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.remove",
  "params": {
    "x": 0,
    "y": -16,
    "z": 0,
    "layer_id": "current"
  },
  "id": 21
}
```

### voxel.paint
**CLI Equivalent**: `goxel --headless paint-voxel <x> <y> <z> <r> <g> <b> <a>`  
**Description**: Changes color of existing voxel

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.paint",
  "params": {
    "x": 0,
    "y": -16,
    "z": 0,
    "color": {
      "r": 0,
      "g": 255,
      "b": 0,
      "a": 255
    }
  },
  "id": 22
}
```

### voxel.get
**CLI Equivalent**: None  
**Description**: Gets voxel information at coordinates

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.get",
  "params": {
    "x": 0,
    "y": -16,
    "z": 0,
    "layer_id": "all"
  },
  "id": 23
}
```

### voxel.add_batch
**CLI Equivalent**: `goxel --headless batch` (with voxel commands)  
**Description**: Adds multiple voxels efficiently

**Request (JSON Format)**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add_batch",
  "params": {
    "voxels": [
      {"x": 0, "y": 0, "z": 0, "color": {"r": 255, "g": 0, "b": 0, "a": 255}},
      {"x": 1, "y": 0, "z": 0, "color": {"r": 0, "g": 255, "b": 0, "a": 255}},
      {"x": 2, "y": 0, "z": 0, "color": {"r": 0, "g": 0, "b": 255, "a": 255}}
    ],
    "merge_mode": "replace"
  },
  "id": 24
}
```

**Request (Binary Format)**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add_batch",
  "params": {
    "format": "packed",
    "compression": "zlib",
    "count": 10000,
    "data": "eJwLz0jNyclXKM8vykkBAB0aBMQ...",
    "checksum": "sha256:abcd1234..."
  },
  "id": 25
}
```

### voxel.clear
**CLI Equivalent**: None  
**Description**: Clears all voxels in current layer or project

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.clear",
  "params": {
    "scope": "layer",
    "layer_id": "current"
  },
  "id": 26
}
```

### voxel.fill
**CLI Equivalent**: None  
**Description**: Fills a region with voxels

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.fill",
  "params": {
    "region": {
      "min": {"x": 0, "y": 0, "z": 0},
      "max": {"x": 10, "y": 10, "z": 10}
    },
    "color": {"r": 128, "g": 128, "b": 128, "a": 255},
    "mode": "solid"
  },
  "id": 27
}
```

### voxel.query
**CLI Equivalent**: None  
**Description**: Queries voxels in a region

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.query",
  "params": {
    "region": {
      "center": {"x": 0, "y": 0, "z": 0},
      "radius": 5
    },
    "filter": {
      "has_color": true,
      "min_alpha": 128
    },
    "return_format": "coordinates"
  },
  "id": 28
}
```

## Layer Methods

### layer.create
**CLI Equivalent**: `goxel --headless layer-create <name>`  
**Description**: Creates a new layer

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.create",
  "params": {
    "name": "Background",
    "visible": true,
    "opacity": 1.0,
    "position": "above_current"
  },
  "id": 30
}
```

### layer.delete
**CLI Equivalent**: `goxel --headless layer-delete <id>`  
**Description**: Deletes a layer

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.delete",
  "params": {
    "layer_id": "layer_123",
    "merge_down": false
  },
  "id": 31
}
```

### layer.rename
**CLI Equivalent**: `goxel --headless layer-rename <id> <name>`  
**Description**: Renames a layer

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.rename",
  "params": {
    "layer_id": "layer_123",
    "name": "Foreground"
  },
  "id": 32
}
```

### layer.set_visibility
**CLI Equivalent**: `goxel --headless layer-visibility <id> <visible>`  
**Description**: Shows or hides a layer

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.set_visibility",
  "params": {
    "layer_id": "layer_123",
    "visible": false
  },
  "id": 33
}
```

### layer.merge
**CLI Equivalent**: `goxel --headless layer-merge <source> <target>`  
**Description**: Merges layers together

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.merge",
  "params": {
    "source_id": "layer_123",
    "target_id": "layer_456",
    "mode": "normal",
    "delete_source": true
  },
  "id": 34
}
```

### layer.get_list
**CLI Equivalent**: None  
**Description**: Returns list of all layers

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.get_list",
  "id": 35
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "layers": [
      {
        "id": "layer_1",
        "name": "Layer 0",
        "visible": true,
        "opacity": 1.0,
        "voxel_count": 234,
        "order": 0,
        "is_active": true
      },
      {
        "id": "layer_2",
        "name": "Background",
        "visible": true,
        "opacity": 0.8,
        "voxel_count": 567,
        "order": 1,
        "is_active": false
      }
    ],
    "active_layer_id": "layer_1"
  },
  "id": 35
}
```

### layer.set_active
**CLI Equivalent**: None  
**Description**: Sets the active layer for operations

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.set_active",
  "params": {
    "layer_id": "layer_2"
  },
  "id": 36
}
```

## Render Methods

### render.image
**CLI Equivalent**: `goxel --headless render <width> <height> <output>`  
**Description**: Renders project to an image

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "render.image",
  "params": {
    "width": 1920,
    "height": 1080,
    "format": "png",
    "path": "/tmp/render.png",
    "camera": {
      "position": [100, 100, 100],
      "target": [0, 0, 0],
      "fov": 45,
      "rotation": [0, 0, 0]
    },
    "lighting": {
      "ambient": 0.2,
      "directional": {
        "intensity": 0.8,
        "direction": [-1, -1, -1]
      }
    },
    "options": {
      "antialiasing": true,
      "shadows": true,
      "ambient_occlusion": true,
      "background_color": [0, 0, 0, 0]
    }
  },
  "id": 40
}
```

### render.preview
**CLI Equivalent**: None  
**Description**: Generates a quick preview render

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "render.preview",
  "params": {
    "size": 256,
    "format": "base64",
    "quality": "draft"
  },
  "id": 41
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "image": "data:image/png;base64,iVBORw0KGgoAAAANS...",
    "width": 256,
    "height": 256,
    "render_time": 15
  },
  "id": 41
}
```

### render.stream
**CLI Equivalent**: None  
**Description**: Starts a render stream for real-time updates

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "render.stream",
  "params": {
    "stream_id": "stream_001",
    "width": 640,
    "height": 480,
    "fps": 30,
    "format": "mjpeg"
  },
  "id": 42
}
```

## Export Methods

### export.model
**CLI Equivalent**: `goxel --headless export <format> <output>`  
**Description**: Exports project to 3D model format

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "export.model",
  "params": {
    "format": "obj",
    "path": "/tmp/model.obj",
    "options": {
      "merge_vertices": true,
      "optimize_mesh": true,
      "include_colors": true,
      "scale": 1.0,
      "origin": "center"
    }
  },
  "id": 50
}
```

### export.formats
**CLI Equivalent**: None  
**Description**: Lists available export formats

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "export.formats",
  "id": 51
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "formats": [
      {
        "id": "obj",
        "name": "Wavefront OBJ",
        "extension": ".obj",
        "supports_colors": true,
        "supports_materials": true
      },
      {
        "id": "ply",
        "name": "Stanford PLY",
        "extension": ".ply",
        "supports_colors": true,
        "supports_materials": false
      },
      {
        "id": "gltf",
        "name": "glTF 2.0",
        "extension": ".gltf",
        "supports_colors": true,
        "supports_materials": true
      }
    ]
  },
  "id": 51
}
```

## Import Methods

### import.model
**CLI Equivalent**: None  
**Description**: Imports a 3D model as voxels

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "import.model",
  "params": {
    "path": "/tmp/model.obj",
    "options": {
      "voxel_size": 1.0,
      "fill_interior": true,
      "color_mode": "vertex_colors",
      "target_layer": "new"
    }
  },
  "id": 60
}
```

### import.image
**CLI Equivalent**: None  
**Description**: Imports an image as voxel art

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "import.image",
  "params": {
    "path": "/tmp/sprite.png",
    "options": {
      "mode": "2d",
      "depth": 1,
      "scale": 1.0,
      "threshold_alpha": 128,
      "dithering": false
    }
  },
  "id": 61
}
```

## Script Methods

### script.execute
**CLI Equivalent**: `goxel --headless script <file>`  
**Description**: Executes a JavaScript file

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "script.execute",
  "params": {
    "path": "/tmp/generate_sphere.js",
    "args": {
      "radius": 10,
      "color": [255, 0, 0, 255]
    }
  },
  "id": 70
}
```

### script.eval
**CLI Equivalent**: None  
**Description**: Evaluates JavaScript code directly

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "script.eval",
  "params": {
    "code": "for(let x = 0; x < 10; x++) { goxel.voxel.add(x, 0, 0, [255, 0, 0, 255]); }",
    "timeout": 5000
  },
  "id": 71
}
```

## Batch Operations

### batch.execute
**CLI Equivalent**: `goxel --headless batch <file>`  
**Description**: Executes multiple operations atomically

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "batch.execute",
  "params": {
    "operations": [
      {
        "method": "voxel.add",
        "params": {"x": 0, "y": 0, "z": 0, "color": {"r": 255, "g": 0, "b": 0, "a": 255}}
      },
      {
        "method": "voxel.add",
        "params": {"x": 1, "y": 0, "z": 0, "color": {"r": 0, "g": 255, "b": 0, "a": 255}}
      },
      {
        "method": "layer.create",
        "params": {"name": "New Layer"}
      }
    ],
    "atomic": true,
    "continue_on_error": false
  },
  "id": 80
}
```

### batch.validate
**CLI Equivalent**: None  
**Description**: Validates batch operations without executing

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "batch.validate",
  "params": {
    "operations": [
      {
        "method": "voxel.add",
        "params": {"x": 0, "y": 0, "z": 0}
      }
    ]
  },
  "id": 81
}
```

## Daemon-Specific Methods

### daemon.get_config
**Description**: Returns daemon configuration

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "daemon.get_config",
  "id": 90
}
```

### daemon.set_config
**Description**: Updates daemon configuration

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "daemon.set_config",
  "params": {
    "max_connections": 20,
    "timeout": 300000,
    "log_level": "debug"
  },
  "id": 91
}
```

### daemon.get_metrics
**Description**: Returns performance metrics

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "daemon.get_metrics",
  "params": {
    "period": "last_hour",
    "include_histogram": true
  },
  "id": 92
}
```

### daemon.subscribe
**Description**: Subscribes to daemon events

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "daemon.subscribe",
  "params": {
    "events": ["project.modified", "layer.changed", "render.progress"],
    "filter": {
      "project_id": "current"
    }
  },
  "id": 93
}
```

## Error Code Reference

### Standard JSON-RPC Errors
```
-32700: Parse error
-32600: Invalid Request  
-32601: Method not found
-32602: Invalid params
-32603: Internal error
```

### Goxel Application Errors
```
-1000: General application error
-1001: Project not found
-1002: Layer not found
-1003: Invalid coordinates
-1004: Invalid color values
-1005: File access error
-1006: Export format not supported
-1007: Memory allocation failed
-1008: Operation timeout
-1009: Resource locked
-1010: Invalid binary data
-1011: Compression error
-1012: Authentication failed
-1013: Permission denied
-1014: Quota exceeded
-1015: Script execution error
-1016: Invalid project state
-1017: Operation not supported
-1018: Validation failed
-1019: Concurrent modification
-1020: Storage full
```

### Error Response Structure
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -1003,
    "message": "Invalid coordinates",
    "data": {
      "field": "x",
      "value": 1000,
      "max_allowed": 64,
      "suggestion": "Coordinates must be within project bounds"
    }
  },
  "id": 100
}
```

## Method Migration Matrix

| v13.4 CLI Command | v14.6 RPC Method | Notes |
|-------------------|------------------|-------|
| `create <file>` | `project.create` | Enhanced with options |
| `open <file>` | `project.open` | Added read-only mode |
| `save` | `project.save` | Added backup option |
| `add-voxel` | `voxel.add` | Same functionality |
| `remove-voxel` | `voxel.remove` | Same functionality |
| `paint-voxel` | `voxel.paint` | New method |
| `batch <file>` | `batch.execute` | JSON-based now |
| `layer-create` | `layer.create` | Enhanced options |
| `layer-delete` | `layer.delete` | Added merge option |
| `layer-rename` | `layer.rename` | Same functionality |
| `layer-visibility` | `layer.set_visibility` | Same functionality |
| `layer-merge` | `layer.merge` | Enhanced modes |
| `render` | `render.image` | Many new options |
| `export` | `export.model` | Format-specific options |
| `script` | `script.execute` | Added eval method |
| `--help` | `system.list_methods` | Dynamic method list |
| `--version` | `system.get_version` | More detailed info |

## Performance Considerations

### Method Categories by Cost

**Low Cost** (<1ms):
- All query methods (`get_*`)
- Single voxel operations
- Layer property changes

**Medium Cost** (1-10ms):
- Batch voxel operations (<1000 voxels)
- Project save/load
- Preview renders

**High Cost** (>10ms):
- Large batch operations (>1000 voxels)
- Full resolution renders
- Model exports
- Script execution

### Optimization Guidelines

1. **Use batch methods** for multiple voxel operations
2. **Use binary format** for >100 voxels
3. **Enable compression** for network transfers
4. **Cache query results** when possible
5. **Use streaming** for large data sets

## Conclusion

This comprehensive method catalog provides a complete mapping of v13.4 CLI functionality to v14.6 RPC methods while introducing powerful new daemon-specific features. The standardized error handling and batch operation support enable efficient, reliable communication between clients and the Goxel daemon.

Key improvements over v13.4:
- **Richer responses** with detailed status information
- **Batch operations** for atomic multi-step processes
- **Streaming support** for real-time updates
- **Enhanced error handling** with actionable error data
- **Daemon-specific features** for monitoring and configuration

The implementation will maintain backward compatibility while providing a foundation for future enhancements.