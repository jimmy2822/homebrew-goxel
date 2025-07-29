# 01_ARCHITECTURE - Goxel Simplified Architecture

## Current Architecture (Overcomplicated)
```
[MCP Client] → [MCP Server] → [TypeScript Client] → [Goxel Daemon]
             (goxel-mcp)    (src/mcp-client)    (JSON-RPC socket)
```

## Proposed Simplified Architecture
```
[MCP Client] → [Goxel MCP-Daemon]
             (Direct MCP protocol)
```

## Implementation Options

### Option 1: Goxel Daemon with MCP Support
- Modify `goxel-daemon` to support MCP protocol directly
- Single process handling both MCP and internal operations
- Benefits: Minimal latency, single deployment unit

### Option 2: MCP Server with Direct JSON-RPC
- MCP server directly calls Goxel daemon's JSON-RPC
- Remove intermediate TypeScript client layer
- Benefits: Keep existing daemon unchanged

## Recommended: Option 1

```c
// goxel-daemon enhanced with MCP
int main() {
    if (mcp_mode) {
        start_mcp_server();  // Handle MCP protocol
    } else {
        start_json_rpc_server();  // Legacy mode
    }
}
```

## Benefits of Simplification
- **Performance**: Remove unnecessary network hops
- **Maintenance**: Single codebase instead of 3
- **Deployment**: One service instead of multiple
- **Debugging**: Simpler stack traces

## Migration Path
1. Add MCP protocol handler to goxel-daemon
2. Test with existing MCP clients
3. Deprecate separate MCP server
4. Remove intermediate TypeScript client

## File Cleanup Plan
- Archive v13/v14 documentation to `/archive/`
- Keep only essential files with `1_` prefix
- Merge overlapping documentation