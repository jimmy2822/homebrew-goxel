# Goxel v14.6 JSON-RPC 2.0 Protocol Design Document

**Author**: Yuki Tanaka (Agent-3)  
**Date**: January 2025  
**Version**: 1.0  
**Status**: Draft

## Executive Summary

This document outlines the comprehensive JSON-RPC 2.0 protocol implementation for Goxel v14.6's daemon communication system. The protocol enables efficient, standardized communication between clients and the Goxel daemon server, supporting both Unix domain sockets and TCP connections with extensions for binary voxel data handling.

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Method Naming Conventions](#method-naming-conventions)
3. [Binary Data Extensions](#binary-data-extensions)
4. [Core Protocol Methods](#core-protocol-methods)
5. [Error Code Standards](#error-code-standards)
6. [Batch Operation Support](#batch-operation-support)
7. [Connection Management](#connection-management)
8. [Security Considerations](#security-considerations)
9. [Performance Optimizations](#performance-optimizations)
10. [Implementation Roadmap](#implementation-roadmap)

## Protocol Overview

### JSON-RPC 2.0 Compliance

The Goxel daemon implements full JSON-RPC 2.0 specification compliance with:
- Request-response pattern for all operations
- Notification support for one-way messages
- Batch request processing
- Standard error codes and custom extensions
- Structured parameter passing (both positional and named)

### Transport Layers

1. **Unix Domain Sockets** (Primary)
   - Path: `/tmp/goxel.sock` (configurable)
   - Low latency local communication
   - Automatic cleanup on daemon shutdown

2. **TCP Sockets** (Secondary)
   - Default port: 7890 (configurable)
   - Remote access capability
   - Token-based authentication for security

### Message Format

```json
{
  "jsonrpc": "2.0",
  "method": "namespace.action",
  "params": {
    "key": "value"
  },
  "id": 1
}
```

## Method Naming Conventions

### Namespace Structure

Methods follow a hierarchical namespace pattern: `namespace.action[.detail]`

**Core Namespaces**:
- `system.*` - Daemon system operations
- `project.*` - Project management
- `voxel.*` - Voxel manipulation
- `layer.*` - Layer operations
- `render.*` - Rendering operations
- `export.*` - Export functionality
- `import.*` - Import functionality
- `script.*` - Scripting operations
- `batch.*` - Batch processing

### Naming Rules

1. **Lowercase with underscores**: `get_project_info`
2. **Action verbs first**: `create`, `delete`, `get`, `set`, `update`
3. **Descriptive suffixes**: `_info`, `_list`, `_batch`
4. **Boolean queries**: `is_*`, `has_*`, `can_*`

### Method Examples

```
system.get_version
system.get_status
system.shutdown

project.create
project.open
project.save
project.get_info
project.close

voxel.add
voxel.remove
voxel.paint
voxel.get
voxel.clear
voxel.add_batch

layer.create
layer.delete
layer.rename
layer.set_visibility
layer.merge
layer.get_list
layer.set_active
```

## Binary Data Extensions

### Base64 Encoding for Voxel Data

Large voxel data sets are encoded using Base64 to maintain JSON compatibility:

```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add_batch",
  "params": {
    "format": "packed",
    "compression": "zlib",
    "data": "eJwLz0jNyclXKM8vykkBAB0aBMQ="
  },
  "id": 1
}
```

### Packed Voxel Format

Binary voxel data uses a packed format for efficiency:

```c
struct PackedVoxel {
    int16_t x, y, z;    // 6 bytes: coordinates
    uint8_t r, g, b, a; // 4 bytes: RGBA color
};  // Total: 10 bytes per voxel
```

### Compression Options

1. **None**: Raw packed data
2. **Zlib**: Standard compression (default)
3. **LZ4**: Fast compression for real-time operations
4. **Brotli**: High compression for network transfer

### Binary Response Format

```json
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "voxel_count": 1000,
    "data": {
      "format": "packed",
      "compression": "zlib",
      "size_uncompressed": 10000,
      "size_compressed": 3245,
      "checksum": "sha256:abcd1234...",
      "content": "eJwLz0jNyclXKM8vykkBAB0aBMQ..."
    }
  },
  "id": 1
}
```

## Core Protocol Methods

### System Methods

#### system.get_version
Returns daemon version and protocol information.

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
    "build_date": "2025-01-27",
    "features": ["batch", "compression", "scripting"]
  },
  "id": 1
}
```

#### system.get_status
Returns current daemon status and statistics.

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
    "memory_usage": 104857600,
    "active_connections": 3,
    "current_project": "test.gox",
    "cpu_usage": 2.5,
    "pending_operations": 0
  },
  "id": 2
}
```

### Project Methods

#### project.create
Creates a new project with optional parameters.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "project.create",
  "params": {
    "name": "MyProject",
    "dimensions": {
      "width": 64,
      "height": 64,
      "depth": 64
    },
    "background_color": [0, 0, 0, 255]
  },
  "id": 3
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "project_id": "proj_12345",
    "name": "MyProject",
    "created_at": "2025-01-27T10:00:00Z"
  },
  "id": 3
}
```

### Voxel Methods

#### voxel.add
Adds a single voxel at specified coordinates.

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
    }
  },
  "id": 4
}
```

#### voxel.add_batch
Adds multiple voxels in a single operation.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add_batch",
  "params": {
    "voxels": [
      {"x": 0, "y": 0, "z": 0, "color": {"r": 255, "g": 0, "b": 0, "a": 255}},
      {"x": 1, "y": 0, "z": 0, "color": {"r": 0, "g": 255, "b": 0, "a": 255}},
      {"x": 2, "y": 0, "z": 0, "color": {"r": 0, "g": 0, "b": 255, "a": 255}}
    ]
  },
  "id": 5
}
```

**Alternative Binary Format**:
```json
{
  "jsonrpc": "2.0",
  "method": "voxel.add_batch",
  "params": {
    "format": "packed",
    "compression": "zlib",
    "count": 1000,
    "data": "eJwLz0jNyclXKM8vykkBAB0aBMQ..."
  },
  "id": 6
}
```

### Layer Methods

#### layer.create
Creates a new layer with specified properties.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.create",
  "params": {
    "name": "Background",
    "visible": true,
    "opacity": 1.0
  },
  "id": 7
}
```

#### layer.get_list
Returns list of all layers in current project.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "layer.get_list",
  "id": 8
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
        "voxel_count": 150,
        "order": 0
      },
      {
        "id": "layer_2",
        "name": "Background",
        "visible": true,
        "voxel_count": 0,
        "order": 1
      }
    ],
    "active_layer": "layer_1"
  },
  "id": 8
}
```

### Render Methods

#### render.image
Renders current project to an image file.

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
      "fov": 45
    },
    "options": {
      "antialiasing": true,
      "shadows": true,
      "ambient_occlusion": true
    }
  },
  "id": 9
}
```

### Export Methods

#### export.model
Exports project to various 3D formats.

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
      "include_colors": true
    }
  },
  "id": 10
}
```

## Error Code Standards

### Standard JSON-RPC 2.0 Error Codes

```
-32700  Parse error
-32600  Invalid Request
-32601  Method not found
-32602  Invalid params
-32603  Internal error
-32000 to -32099  Server error (reserved)
```

### Goxel-Specific Error Codes

```
-1000  General application error
-1001  Project not found
-1002  Layer not found
-1003  Invalid coordinates
-1004  Invalid color values
-1005  File access error
-1006  Export format not supported
-1007  Memory allocation failed
-1008  Operation timeout
-1009  Resource locked
-1010  Invalid binary data
-1011  Compression error
-1012  Authentication failed
-1013  Permission denied
-1014  Quota exceeded
-1015  Script execution error
```

### Error Response Format

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -1003,
    "message": "Invalid coordinates",
    "data": {
      "details": "Coordinates exceed project bounds",
      "max_bounds": {"x": 64, "y": 64, "z": 64},
      "provided": {"x": 100, "y": 0, "z": 0}
    }
  },
  "id": 11
}
```

## Batch Operation Support

### Batch Request Format

Multiple operations can be sent in a single request:

```json
[
  {
    "jsonrpc": "2.0",
    "method": "voxel.add",
    "params": {"x": 0, "y": 0, "z": 0, "color": {"r": 255, "g": 0, "b": 0, "a": 255}},
    "id": 1
  },
  {
    "jsonrpc": "2.0",
    "method": "voxel.add",
    "params": {"x": 1, "y": 0, "z": 0, "color": {"r": 0, "g": 255, "b": 0, "a": 255}},
    "id": 2
  },
  {
    "jsonrpc": "2.0",
    "method": "project.save",
    "params": {"path": "/tmp/batch_result.gox"},
    "id": 3
  }
]
```

### Batch Response Format

Responses maintain order and include all results/errors:

```json
[
  {
    "jsonrpc": "2.0",
    "result": {"success": true, "voxel_id": "vox_001"},
    "id": 1
  },
  {
    "jsonrpc": "2.0",
    "result": {"success": true, "voxel_id": "vox_002"},
    "id": 2
  },
  {
    "jsonrpc": "2.0",
    "result": {"success": true, "bytes_written": 1024},
    "id": 3
  }
]
```

### Batch Optimization Features

1. **Transaction Support**: All-or-nothing batch execution
2. **Parallel Processing**: Independent operations run concurrently
3. **Result Aggregation**: Combined statistics for batch operations
4. **Progress Notifications**: Optional progress updates for long batches

## Connection Management

### Connection Lifecycle

1. **Handshake Phase**
   ```json
   {
     "jsonrpc": "2.0",
     "method": "system.connect",
     "params": {
       "client_name": "Goxel TypeScript Client",
       "client_version": "1.0.0",
       "capabilities": ["batch", "compression", "notifications"]
     },
     "id": "handshake"
   }
   ```

2. **Authentication** (TCP only)
   ```json
   {
     "jsonrpc": "2.0",
     "method": "system.authenticate",
     "params": {
       "token": "Bearer eyJhbGciOiJIUzI1NiIs..."
     },
     "id": "auth"
   }
   ```

3. **Keep-Alive**
   ```json
   {
     "jsonrpc": "2.0",
     "method": "system.ping",
     "id": "ping_12345"
   }
   ```

### Connection Pooling

- Minimum connections: 2
- Maximum connections: 10
- Idle timeout: 60 seconds
- Health check interval: 30 seconds

### Retry Strategy

1. **Exponential Backoff**: 1s, 2s, 4s, 8s...
2. **Maximum Retries**: 3 (configurable)
3. **Circuit Breaker**: Disable after 5 consecutive failures
4. **Jitter**: ±10% randomization to prevent thundering herd

## Security Considerations

### Authentication Methods

1. **Unix Socket**: File system permissions
2. **TCP Socket**: Token-based authentication
3. **TLS Support**: Optional encryption for TCP

### Authorization Levels

1. **Read-Only**: Query operations only
2. **Standard**: All operations except system
3. **Admin**: Full access including shutdown

### Rate Limiting

- Requests per second: 100 (configurable)
- Burst allowance: 200
- Per-method limits for expensive operations

## Performance Optimizations

### Protocol Optimizations

1. **Message Compression**: Gzip for messages >1KB
2. **Binary Data Caching**: Server-side cache for repeated data
3. **Request Deduplication**: Ignore duplicate requests within 100ms
4. **Response Streaming**: Large responses sent in chunks

### Recommended Practices

1. **Use Batch Operations**: Combine multiple voxel operations
2. **Binary Format**: Use packed format for >100 voxels
3. **Connection Reuse**: Maintain persistent connections
4. **Async Operations**: Use notifications for long operations

### Performance Targets

- Single voxel operation: <1ms
- 1000 voxel batch: <10ms
- Project save: <100ms
- Image render (1920x1080): <500ms

## Implementation Roadmap

### Phase 1: Core Protocol (Week 1-2)
- JSON-RPC 2.0 parser/serializer
- Basic request/response handling
- Error management system
- Method registry framework

### Phase 2: Binary Extensions (Week 3-4)
- Base64 encoding/decoding
- Packed voxel format
- Compression support
- Checksum validation

### Phase 3: Method Implementation (Week 5-6)
- System methods
- Project management
- Voxel operations
- Layer operations

### Phase 4: Advanced Features (Week 7-8)
- Batch processing
- Notifications
- Progress tracking
- Connection pooling

### Phase 5: Optimization (Week 9-10)
- Performance profiling
- Cache implementation
- Protocol refinements
- Load testing

## Conclusion

This JSON-RPC 2.0 protocol design provides a robust, extensible foundation for Goxel v14.6's daemon communication. The protocol balances standards compliance with domain-specific optimizations, enabling efficient voxel manipulation while maintaining compatibility with standard JSON-RPC clients.

Key advantages:
- **Standards-based**: Full JSON-RPC 2.0 compliance
- **Efficient**: Binary extensions for large data sets
- **Scalable**: Batch operations and connection pooling
- **Secure**: Authentication and authorization support
- **Performant**: Optimized for voxel operations

The implementation will proceed in phases, with core functionality available early and optimizations added progressively.