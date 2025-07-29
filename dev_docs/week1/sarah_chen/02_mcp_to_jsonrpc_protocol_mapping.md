# MCP to JSON-RPC Protocol Mapping

**Author**: Sarah Chen, Lead MCP Protocol Integration Specialist  
**Date**: January 29, 2025  
**Document Version**: 1.0

## Executive Summary

This document provides a comprehensive mapping between MCP tool definitions and JSON-RPC methods in the Goxel daemon. The analysis reveals opportunities for direct protocol translation that will enable the simplified 2-layer architecture.

## Protocol Comparison

### MCP Protocol Structure
```json
{
  "tool": "tool_name",
  "arguments": {
    "param1": "value1",
    "param2": "value2"
  }
}
```

### JSON-RPC 2.0 Structure
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

## Current Method Mappings

### File Operations

| MCP Tool | JSON-RPC Method | Parameter Mapping | Notes |
|----------|-----------------|-------------------|-------|
| `goxel_create_project` | `goxel.create_project` | `name` → `name`<br>`path` → `path` | Direct mapping |
| `goxel_open_file` | `goxel.load_project` | `path` → `path`<br>`format` → `format` | Rename: open→load |
| `goxel_save_file` | `goxel.save_project` | `path` → `path` | Direct mapping |
| `goxel_export_file` | `goxel.export_model` | `path` → `path`<br>`format` → `format`<br>`options` → `options` | Rename: file→model |

### Voxel Operations

| MCP Tool | JSON-RPC Method | Parameter Mapping | Notes |
|----------|-----------------|-------------------|-------|
| `goxel_add_voxels` | `goxel.add_voxel` | `position` → `{x,y,z}`<br>`color` → `rgba`<br>`brush` → N/A | Need batch support |
| `goxel_remove_voxels` | `goxel.remove_voxel` | `position` → `{x,y,z}`<br>`brush` → N/A | Need brush support |
| `goxel_paint_voxels` | N/A | - | Not implemented |
| `goxel_get_voxel` | `goxel.get_voxel` | `position` → `{x,y,z}` | Direct mapping |

### Layer Operations

| MCP Tool | JSON-RPC Method | Parameter Mapping | Notes |
|----------|-----------------|-------------------|-------|
| `goxel_new_layer` | `goxel.create_layer` | `name` → `name`<br>`active` → `active` | Direct mapping |
| `goxel_delete_layer` | N/A | - | Not implemented |
| `goxel_list_layers` | `goxel.list_layers` | None | Direct mapping |
| `goxel_rename_layer` | N/A | - | Not implemented |
| `goxel_set_layer_visibility` | N/A | - | Not implemented |

### Advanced Operations

| MCP Tool | JSON-RPC Method | Parameter Mapping | Notes |
|----------|-----------------|-------------------|-------|
| `goxel_batch_voxel_operations` | N/A | - | Critical for performance |
| `goxel_procedural_shape` | N/A | - | Not implemented |
| `goxel_smart_selection` | N/A | - | Not implemented |
| `goxel_flood_fill` | N/A | - | Not implemented |

### System Operations

| MCP Tool | JSON-RPC Method | Parameter Mapping | Notes |
|----------|-----------------|-------------------|-------|
| N/A | `echo` | Test method | |
| N/A | `version` | System info | |
| N/A | `status` | Daemon status | |
| N/A | `ping` | Health check | |
| N/A | `list_methods` | Discovery | |

## Gap Analysis

### Missing JSON-RPC Methods
1. **Layer Management**
   - `goxel.delete_layer`
   - `goxel.rename_layer`
   - `goxel.set_layer_visibility`
   - `goxel.duplicate_layer`
   - `goxel.merge_layers`

2. **Advanced Voxel Operations**
   - `goxel.paint_voxels` (color only)
   - `goxel.batch_operations` (critical)
   - `goxel.procedural_shape`
   - `goxel.flood_fill`

3. **Selection Tools**
   - `goxel.select_region`
   - `goxel.select_by_color`
   - `goxel.clear_selection`

4. **Camera/View**
   - `goxel.set_camera`
   - `goxel.get_camera`
   - `goxel.render_view`

### Missing MCP Tools
1. **Project Management**
   - Recent projects
   - Project metadata
   - Auto-save management

2. **Import/Export**
   - Batch export
   - Format conversion
   - Texture management

## Protocol Translation Strategy

### 1. Direct Mapping (70% of methods)
```typescript
// MCP Request
{ tool: "goxel_create_project", arguments: { name: "test" } }

// Direct Translation
{
  jsonrpc: "2.0",
  method: "goxel.create_project",
  params: { name: "test" },
  id: generateId()
}
```

### 2. Parameter Transformation (20% of methods)
```typescript
// MCP Request
{ tool: "goxel_add_voxels", arguments: { 
  position: { x: 10, y: 20, z: 30 },
  color: { r: 255, g: 0, b: 0, a: 255 }
}}

// Transform to
{
  jsonrpc: "2.0",
  method: "goxel.add_voxel",
  params: {
    x: 10, y: 20, z: 30,
    rgba: [255, 0, 0, 255]
  },
  id: generateId()
}
```

### 3. Method Composition (10% of methods)
```typescript
// MCP Request (batch operation)
{ tool: "goxel_batch_voxel_operations", arguments: {
  operations: [
    { type: "add", position: {x:1,y:1,z:1}, color: {...} },
    { type: "add", position: {x:2,y:2,z:2}, color: {...} }
  ]
}}

// Compose multiple JSON-RPC calls
[
  { jsonrpc: "2.0", method: "goxel.add_voxel", params: {...}, id: 1 },
  { jsonrpc: "2.0", method: "goxel.add_voxel", params: {...}, id: 2 }
]
```

## MCP Handler Requirements

### Core Functions Needed
1. **Protocol Translation**
   - `mcp_to_jsonrpc(mcp_request) → jsonrpc_request`
   - `jsonrpc_to_mcp(jsonrpc_response) → mcp_response`

2. **Parameter Mapping**
   - `map_mcp_params(tool_name, mcp_params) → jsonrpc_params`
   - `map_jsonrpc_result(method_name, result) → mcp_result`

3. **Error Mapping**
   - `map_jsonrpc_error(error_code) → mcp_error`
   - Preserve error context and details

4. **Batch Support**
   - `compose_batch_request(operations) → jsonrpc_batch`
   - `decompose_batch_response(responses) → mcp_result`

## Performance Optimizations

### 1. Method Name Caching
```c
// Pre-compute method name mappings at startup
static struct {
    const char *mcp_tool;
    const char *jsonrpc_method;
    param_mapper_fn mapper;
} method_map[] = {
    {"goxel_create_project", "goxel.create_project", NULL},
    {"goxel_open_file", "goxel.load_project", map_open_file_params},
    // ...
};
```

### 2. Zero-Copy Parameter Passing
- Use JSON DOM manipulation instead of serialization
- Pass pointers to existing JSON nodes
- Minimize string allocations

### 3. Connection Reuse
- Single persistent daemon connection
- Request pipelining support
- Automatic reconnection

## Implementation Priority

### Phase 1: Core Methods (Week 2)
1. File operations (create, open, save, export)
2. Basic voxel operations (add, remove, get)
3. Layer listing and creation

### Phase 2: Extended Methods (Week 3)
1. Batch voxel operations
2. Complete layer management
3. Selection tools

### Phase 3: Advanced Features (Future)
1. Procedural generation
2. Advanced selections
3. Render operations

## Backward Compatibility

### MCP Client Compatibility
- All existing MCP tools continue to work
- New tools added incrementally
- Version negotiation support

### TypeScript Client Migration
- Gradual deprecation path
- Compatibility shim available
- Performance metrics comparison

## Next Steps

1. Design `mcp_handler.h` interface (Task 3)
2. Implement core translation functions
3. Create parameter mapping tables
4. Write comprehensive tests

---

**Document Status**: Complete  
**Review Required**: Yes  
**Distribution**: David Park (migration planning), Alex Kumar (test design)