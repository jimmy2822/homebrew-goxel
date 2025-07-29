# Compatibility Layer Design - Zero-Downtime Migration

**Author**: David Park, Compatibility & Migration Tools Developer  
**Date**: January 29, 2025  
**Document Version**: 1.0

## Executive Summary

This document outlines the design of a compatibility layer that ensures zero-downtime migration from the current 4-layer architecture to the simplified 2-layer architecture. The layer provides transparent protocol translation, connection proxying, and gradual migration capabilities.

## Design Principles

1. **Zero Downtime**: No service interruption during migration
2. **Transparent Operation**: Existing clients work without modification
3. **Gradual Migration**: Users migrate at their own pace
4. **Performance Neutral**: Minimal overhead during transition
5. **Full Observability**: Complete visibility into migration progress

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Existing Clients                         │
├─────────────────┬───────────────────┬─────────────────────┤
│   MCP Clients   │  TypeScript Client │  Direct JSON-RPC   │
└────────┬────────┴─────────┬─────────┴──────────┬──────────┘
         │                  │                     │
         ▼                  ▼                     ▼
┌─────────────────────────────────────────────────────────────┐
│                  Compatibility Layer                        │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │ MCP Proxy   │  │ TS Client    │  │ Protocol        │   │
│  │ Server      │  │ Adapter      │  │ Router          │   │
│  └─────────────┘  └──────────────┘  └─────────────────┘   │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
         ┌────────────────────────────────────┐
         │    Unified Goxel MCP-Daemon        │
         └────────────────────────────────────┘
```

## Component Design

### 1. MCP Proxy Server

**Purpose**: Emulate the current MCP server interface while forwarding to the new daemon

```typescript
interface MCPProxyConfig {
  // Listen on old MCP server address
  listenAddress: string;  // Default: "/tmp/mcp-server.sock"
  
  // Forward to new daemon
  daemonAddress: string;  // Default: "/tmp/goxel-mcp-daemon.sock"
  
  // Compatibility options
  emulateNodeBehavior: boolean;  // Match Node.js timing
  translateErrors: boolean;       // Convert error formats
  logCompatibilityIssues: boolean; // Track usage patterns
}
```

**Key Features**:
- Socket server listening on old MCP path
- Protocol translation (MCP → Internal MCP format)
- Response timing emulation
- Deprecation warning injection

### 2. TypeScript Client Adapter

**Purpose**: Provide drop-in replacement for existing TypeScript client

```typescript
// Old client usage (unchanged)
import { GoxelDaemonClient } from 'goxel-daemon-client';

// New adapter provides same interface
export class GoxelDaemonClient {
  constructor(config: DaemonClientConfig) {
    // Internally connects to new daemon
    this.internal = new UnifiedDaemonConnection({
      ...config,
      protocol: 'jsonrpc',
      socketPath: config.socketPath || '/tmp/goxel-mcp-daemon.sock'
    });
  }
  
  // All existing methods preserved
  async connect(): Promise<void> {
    await this.internal.connect();
    console.warn('[Deprecation] GoxelDaemonClient is deprecated. ' +
                 'Please migrate to UnifiedGoxelClient');
  }
  
  async call(method: string, params: any): Promise<any> {
    // Translate old method names to new ones
    const newMethod = this.translateMethod(method);
    return await this.internal.call(newMethod, params);
  }
}
```

### 3. Protocol Router

**Purpose**: Auto-detect and route requests to appropriate handlers

```c
typedef enum {
    PROTOCOL_UNKNOWN,
    PROTOCOL_MCP,
    PROTOCOL_JSONRPC,
    PROTOCOL_LEGACY_MCP,
    PROTOCOL_LEGACY_TS
} protocol_type_t;

typedef struct {
    protocol_type_t type;
    void* handler;
    translation_func_t translator;
} protocol_handler_t;

// Auto-detection logic
protocol_type_t detect_protocol(const char* data, size_t len) {
    json_value_t* root = json_parse(data, len);
    
    // Check for MCP structure
    if (json_has_field(root, "tool")) {
        if (json_has_field(root, "_legacy")) {
            return PROTOCOL_LEGACY_MCP;
        }
        return PROTOCOL_MCP;
    }
    
    // Check for JSON-RPC
    if (json_has_field(root, "jsonrpc") && 
        json_has_field(root, "method")) {
        return PROTOCOL_JSONRPC;
    }
    
    return PROTOCOL_UNKNOWN;
}
```

### 4. Migration Telemetry

**Purpose**: Track migration progress and identify issues

```c
typedef struct {
    uint64_t legacy_mcp_requests;
    uint64_t legacy_ts_requests;
    uint64_t native_mcp_requests;
    uint64_t native_jsonrpc_requests;
    uint64_t translation_errors;
    uint64_t deprecation_warnings_sent;
    time_t first_legacy_request;
    time_t last_legacy_request;
} migration_stats_t;

// Telemetry API
void record_request(protocol_type_t protocol, const char* method) {
    migration_stats.total_requests[protocol]++;
    
    if (is_legacy_protocol(protocol)) {
        migration_stats.last_legacy_request = time(NULL);
        
        // Send deprecation warning every N requests
        if (migration_stats.total_requests[protocol] % 100 == 0) {
            send_deprecation_notice(protocol);
        }
    }
}
```

## Translation Mappings

### Method Name Translation

```c
typedef struct {
    const char* old_name;
    const char* new_name;
    param_transform_func_t transformer;
} method_mapping_t;

static const method_mapping_t method_mappings[] = {
    // TypeScript client -> New daemon
    {"add_voxel", "goxel.add_voxels", transform_add_voxel_params},
    {"load_project", "goxel.open_file", NULL},
    {"export_model", "goxel.export_file", transform_export_params},
    
    // Legacy MCP -> New MCP
    {"goxel_create_project", "create_project", NULL},
    {"goxel_add_voxels", "add_voxels", transform_mcp_voxel_params},
    
    {NULL, NULL, NULL}
};
```

### Parameter Transformation

```c
// Example: Transform old add_voxel params to new format
json_value_t* transform_add_voxel_params(json_value_t* old_params) {
    json_value_t* new_params = json_create_object();
    
    // Old format: {x: 10, y: 20, z: 30, rgba: [255,0,0,255]}
    // New format: {position: {x,y,z}, color: {r,g,b,a}, brush: {...}}
    
    json_value_t* position = json_create_object();
    json_set_number(position, "x", json_get_number(old_params, "x"));
    json_set_number(position, "y", json_get_number(old_params, "y"));
    json_set_number(position, "z", json_get_number(old_params, "z"));
    json_set_object(new_params, "position", position);
    
    json_array_t* rgba = json_get_array(old_params, "rgba");
    if (rgba && json_array_size(rgba) == 4) {
        json_value_t* color = json_create_object();
        json_set_number(color, "r", json_array_get_number(rgba, 0));
        json_set_number(color, "g", json_array_get_number(rgba, 1));
        json_set_number(color, "b", json_array_get_number(rgba, 2));
        json_set_number(color, "a", json_array_get_number(rgba, 3));
        json_set_object(new_params, "color", color);
    }
    
    // Add default brush
    json_value_t* brush = json_create_object();
    json_set_string(brush, "shape", "cube");
    json_set_number(brush, "size", 1);
    json_set_object(new_params, "brush", brush);
    
    return new_params;
}
```

## Deployment Strategy

### Phase 1: Silent Compatibility (Week 2)
1. Deploy new daemon with compatibility layer
2. No client changes required
3. Monitor telemetry for issues
4. Fix any translation bugs

### Phase 2: Deprecation Notices (Week 3)
1. Enable deprecation warnings
2. Provide migration guides
3. Offer migration tools
4. Support early adopters

### Phase 3: Active Migration (Week 4-5)
1. Reach out to known users
2. Assist with migration
3. Update documentation
4. Celebrate migrations

### Phase 4: Final Transition (Week 6+)
1. Set sunset date for compatibility
2. Increase warning frequency
3. Provide final migration window
4. Archive old components

## Configuration Options

```yaml
# /etc/goxel/compatibility.yaml
compatibility:
  # Enable compatibility layer
  enabled: true
  
  # Proxy servers
  mcp_proxy:
    enabled: true
    listen_path: "/tmp/mcp-server.sock"
    legacy_mode: true
    
  typescript_adapter:
    enabled: true
    listen_path: "/tmp/goxel-daemon.sock"
    emulate_pooling: true
    
  # Migration settings
  migration:
    deprecation_warnings: true
    warning_frequency: 100  # Every N requests
    telemetry_enabled: true
    telemetry_endpoint: "https://telemetry.goxel.xyz/v1/migration"
    
  # Protocol routing
  protocol_routing:
    auto_detect: true
    default_protocol: "jsonrpc"
    legacy_support: true
    
  # Performance
  performance:
    translation_cache_size: 1000
    connection_timeout: 5000
    max_legacy_connections: 100
```

## Testing Strategy

### Unit Tests
- Protocol detection accuracy
- Parameter transformation correctness
- Method name mapping
- Error translation

### Integration Tests
- Old MCP client → New daemon
- TypeScript client → New daemon
- Mixed protocol scenarios
- Performance overhead

### Compatibility Matrix

| Client Type | Version | Status | Notes |
|------------|---------|--------|-------|
| MCP Server | v13.x | ✅ Full | Via proxy |
| MCP Server | v14.x | ✅ Full | Via proxy |
| TS Client | v14.0 | ✅ Full | Via adapter |
| Direct JSON-RPC | All | ✅ Full | Native |
| Future MCP | v15.x | ✅ Full | Native |

## Performance Considerations

### Overhead Analysis
- Protocol detection: < 0.1ms
- Parameter transformation: < 0.5ms
- Method routing: < 0.1ms
- **Total overhead**: < 1ms per request

### Optimization Strategies
1. **Translation Cache**: Cache frequent transformations
2. **Connection Pooling**: Reuse daemon connections
3. **Lazy Loading**: Load translators on demand
4. **Zero-Copy**: Use JSON DOM manipulation

## Rollback Strategy

### Instant Rollback
```bash
# Disable compatibility layer
goxel-mcp-daemon --compatibility=false

# Or revert to old architecture
systemctl stop goxel-mcp-daemon
systemctl start goxel-mcp-server
systemctl start goxel-daemon
```

### Data Preservation
- No data migration required
- All project files remain compatible
- Connection state can be preserved

## Success Metrics

### Migration Progress
- % of requests using new protocol
- Number of unique clients migrated
- Reduction in legacy requests/day

### Quality Metrics
- Translation error rate < 0.01%
- Performance overhead < 1ms
- Zero data loss incidents

### User Satisfaction
- Support ticket volume
- Migration feedback scores
- Time to complete migration

## Next Steps

1. Implement core translation functions
2. Create proxy servers
3. Build telemetry system
4. Write comprehensive tests
5. Document migration process

---

**Document Status**: Complete  
**Review Required**: Yes  
**Dependencies**: Sarah's protocol mapping, Michael's daemon analysis