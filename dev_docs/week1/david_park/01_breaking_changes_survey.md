# Breaking Changes Survey - Migration to Simplified Architecture

**Author**: David Park, Compatibility & Migration Tools Developer  
**Date**: January 29, 2025  
**Document Version**: 1.0

## Executive Summary

This document identifies all breaking changes that will affect existing users during the migration from the current 4-layer architecture to the simplified 2-layer architecture. The analysis covers API changes, protocol differences, deployment impacts, and third-party integrations.

## Affected User Categories

### 1. MCP Server Users (High Impact)
**Current Setup**: `AI → MCP Server (Node.js) → TypeScript Client → Goxel Daemon`  
**New Setup**: `AI → Goxel MCP-Daemon (Direct)`

#### Breaking Changes:
- **Connection Method**: Unix socket path changes from MCP server to daemon
- **Process Management**: Single daemon process instead of Node.js + daemon
- **Configuration**: New daemon config format, MCP-specific settings
- **Tool Names**: Some tool names may change for consistency
- **Error Responses**: Different error format and codes

### 2. TypeScript Client Users (Critical Impact)
**Current Setup**: `Application → TypeScript Client → Goxel Daemon`  
**New Setup**: `Application → ??? (Needs migration path)`

#### Breaking Changes:
- **Library Deprecation**: Current TypeScript client becomes obsolete
- **API Changes**: Direct daemon API differs from client wrapper
- **Connection Pooling**: New pooling mechanism incompatible
- **Event System**: Different event names and payloads
- **Type Definitions**: New TypeScript types needed

### 3. Direct Daemon Users (Low Impact)
**Current Setup**: `Application → JSON-RPC → Goxel Daemon`  
**New Setup**: `Application → JSON-RPC → Goxel MCP-Daemon`

#### Breaking Changes:
- **Protocol Detection**: Must specify protocol or rely on auto-detection
- **New Methods**: Additional MCP-specific methods mixed in
- **Performance**: Different worker pool behavior with MCP

### 4. Production Deployments (Medium Impact)

#### Breaking Changes:
- **Service Files**: systemd/launchd configs need updates
- **Monitoring**: Different health check endpoints
- **Logging**: New log format with MCP context
- **Resource Usage**: Different memory/CPU patterns

## API Breaking Changes

### Method Name Changes
| Current | New | Impact |
|---------|-----|--------|
| `goxel.load_project` | `goxel.open_file` (MCP mode) | TypeScript clients break |
| `goxel.export_model` | `goxel.export_file` (MCP mode) | TypeScript clients break |
| `goxel.add_voxel` | `goxel.add_voxels` (batch support) | Parameter structure change |

### Parameter Structure Changes

#### Current Add Voxel
```json
{
  "method": "goxel.add_voxel",
  "params": {
    "x": 10, "y": 20, "z": 30,
    "rgba": [255, 0, 0, 255]
  }
}
```

#### New MCP Format
```json
{
  "tool": "goxel_add_voxels",
  "arguments": {
    "position": { "x": 10, "y": 20, "z": 30 },
    "color": { "r": 255, "g": 0, "b": 0, "a": 255 },
    "brush": { "shape": "cube", "size": 1 }
  }
}
```

### Response Format Changes

#### Current JSON-RPC Response
```json
{
  "jsonrpc": "2.0",
  "result": { "success": true, "data": {...} },
  "id": 1
}
```

#### New MCP Response
```json
{
  "success": true,
  "data": {...},
  "metadata": { "tool": "goxel_add_voxels", "duration": 5 }
}
```

## Connection Breaking Changes

### Unix Socket Path
- **Current MCP**: `/tmp/mcp-server.sock` (Node.js)
- **Current Daemon**: `/tmp/goxel-daemon.sock`
- **New Unified**: `/tmp/goxel-mcp-daemon.sock`

### Connection Options
- **Current**: Separate options for MCP server and daemon client
- **New**: Unified options with protocol selection
- **Breaking**: Connection pooling API completely different

### Protocol Negotiation
- **Current**: Fixed protocol per connection type
- **New**: Auto-detection or explicit selection required
- **Breaking**: Old clients won't understand new handshake

## Deployment Breaking Changes

### Process Architecture
```
# Current
mcp-server (Node.js) → PID 1234
goxel-daemon → PID 5678

# New
goxel-mcp-daemon → PID 9012 (handles both protocols)
```

### Service Configuration
```bash
# Current systemd
[Service]
ExecStart=/usr/bin/node /opt/goxel-mcp/index.js
ExecStartPost=/usr/bin/goxel-daemon --socket /tmp/goxel-daemon.sock

# New systemd
[Service]
ExecStart=/usr/bin/goxel-mcp-daemon --protocol=auto --socket /tmp/goxel-mcp-daemon.sock
```

### Environment Variables
- **Removed**: `MCP_SERVER_PORT`, `MCP_SERVER_HOST`
- **Added**: `GOXEL_PROTOCOL_MODE`, `GOXEL_MCP_FEATURES`
- **Changed**: `GOXEL_DAEMON_SOCKET` → `GOXEL_MCP_DAEMON_SOCKET`

## Third-Party Integration Impact

### Known Integrations at Risk

1. **Claude Desktop MCP Config**
```json
// Current (will break)
{
  "mcpServers": {
    "goxel-mcp": {
      "command": "node",
      "args": ["/path/to/goxel-mcp/index.js"]
    }
  }
}

// New (required)
{
  "mcpServers": {
    "goxel-mcp": {
      "command": "goxel-mcp-daemon",
      "args": ["--protocol=mcp", "--foreground"]
    }
  }
}
```

2. **CI/CD Pipelines**
- Build scripts expecting separate components
- Test suites using TypeScript client
- Deployment scripts starting multiple services

3. **Monitoring Systems**
- Health checks pointing to Node.js MCP server
- Metrics expecting two separate processes
- Log aggregation with different formats

4. **Custom Integrations**
- Any code importing TypeScript client library
- Scripts assuming specific process names
- Tools parsing current log formats

## Data Migration Requirements

### Configuration Files
- MCP server config.json → daemon config (MCP section)
- TypeScript client options → daemon connection options
- Service discovery configs → unified endpoint

### Persistent Data
- No impact on .gox project files
- No impact on voxel data formats
- Connection state not preserved across migration

### Caches and Temporary Data
- MCP server request cache invalidated
- TypeScript client connection pool reset
- Daemon worker state cleared

## Performance Impact

### Expected Changes
- **Latency**: 80% reduction (removing network hops)
- **Throughput**: 100% increase (direct processing)
- **Memory**: 60% reduction (fewer processes)

### Behavioral Changes
- Request ordering may differ
- Error recovery timing changes
- Resource limits apply differently

## Security Considerations

### Authentication Changes
- MCP server auth tokens → daemon auth system
- Different permission model for socket access
- New attack surface with unified daemon

### Network Exposure
- Reduced attack surface (one process vs. two)
- Different firewall rules needed
- Changed audit logging

## Risk Assessment by User Type

### Critical Risk Users
1. **Production MCP deployments**: Service disruption during migration
2. **TypeScript client applications**: Complete rewrite needed
3. **Automated workflows**: Break without updates

### Medium Risk Users
1. **Development environments**: Need reconfiguration
2. **Testing infrastructure**: Update test suites
3. **Monitoring systems**: Adjust health checks

### Low Risk Users
1. **Direct JSON-RPC users**: Minimal changes
2. **New deployments**: Can start with new architecture
3. **Experimental users**: Expected to handle changes

## Recommended Migration Timeline

### Phase 1: Compatibility Mode (Weeks 1-2)
- Daemon supports both old and new protocols
- Deprecation warnings added
- Documentation updated

### Phase 2: Parallel Running (Weeks 3-4)
- Both architectures available
- Migration tools provided
- User communication campaign

### Phase 3: Deprecation (Weeks 5-6)
- Old architecture marked deprecated
- Final migration deadline set
- Support focused on migration

### Phase 4: Removal (Week 8+)
- Old components archived
- Full commitment to new architecture
- Post-migration support

## Next Steps

1. **Design compatibility layer** (Task 2)
2. **Create user impact assessment** (Task 3)
3. **Plan detailed migration timeline** (Task 4)
4. **Develop migration tools** (Week 2)

---

**Document Status**: Complete  
**Review Required**: Yes  
**Distribution**: All team members, affected users