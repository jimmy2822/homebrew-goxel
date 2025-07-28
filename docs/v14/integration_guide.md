# Goxel v14.0 Daemon Integration Guide

**Status**: 🚧 PLANNED - NOT YET FUNCTIONAL  
**Version**: 14.0.0-alpha  
**Last Updated**: January 27, 2025

## ⚠️ Important Notice

This guide documents the **planned** integration approach for the Goxel v14.0 daemon. Currently, the daemon has no working methods and the TypeScript client does not exist. Use this guide for planning purposes only.

## 📋 Current Reality

Before attempting any integration:

1. **No Working Methods**: The daemon accepts connections but has no implemented RPC methods
2. **No Client Library**: The TypeScript/JavaScript client mentioned here doesn't exist
3. **No MCP Bridge**: The MCP integration is not implemented
4. **Use v13.4 Instead**: For any real work, continue using the stable v13.4 CLI

## 🎯 Integration Overview (PLANNED)

When complete, the v14.0 daemon will support three integration approaches:

### 1. Direct Socket Connection (Partially Working)
- Connect directly to Unix domain socket
- Send raw JSON-RPC messages
- Handle responses manually

### 2. TypeScript Client Library (Not Implemented)
- Type-safe method calls
- Automatic reconnection
- Promise-based API

### 3. MCP Server Integration (Not Implemented)
- Bridge between MCP tools and daemon
- Seamless migration from v13.4

## 🔌 Direct Socket Connection

This is the only partially working approach, though no methods are implemented.

### Basic Connection Test
```bash
# Start the daemon
./goxel-daemon

# Test connection (this works)
echo '{"jsonrpc":"2.0","method":"test","id":1}' | nc -U /tmp/goxel-daemon.sock
# Response: {"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found"},"id":1}
```

### Node.js Example (Socket Works, Methods Don't)
```javascript
const net = require('net');

class GoxelDaemonClient {
  constructor(socketPath = '/tmp/goxel-daemon.sock') {
    this.socketPath = socketPath;
    this.requestId = 0;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection(this.socketPath, () => {
        console.log('Connected to Goxel daemon');
        resolve();
      });
      
      this.socket.on('error', reject);
    });
  }

  async sendRequest(method, params = {}) {
    const request = {
      jsonrpc: '2.0',
      method: method,
      params: params,
      id: ++this.requestId
    };

    return new Promise((resolve, reject) => {
      this.socket.write(JSON.stringify(request));
      
      this.socket.once('data', (data) => {
        try {
          const response = JSON.parse(data.toString());
          if (response.error) {
            reject(new Error(response.error.message));
          } else {
            resolve(response.result);
          }
        } catch (e) {
          reject(e);
        }
      });
    });
  }

  disconnect() {
    this.socket.end();
  }
}

// Usage example (will fail - no methods implemented)
async function testDaemon() {
  const client = new GoxelDaemonClient();
  
  try {
    await client.connect();
    
    // This will fail with "Method not found"
    const result = await client.sendRequest('goxel.get_status');
    console.log(result);
  } catch (error) {
    console.error('Error:', error.message);
    // Output: Error: Method not found
  } finally {
    client.disconnect();
  }
}

testDaemon();
```

## 📦 TypeScript Client Library (PLANNED)

The TypeScript client library doesn't exist yet. When implemented, it should provide:

### Planned Installation
```bash
# This package doesn't exist yet
npm install @goxel/daemon-client
```

### Planned Usage
```typescript
// THIS CODE IS CONCEPTUAL - CLIENT DOESN'T EXIST

import { GoxelDaemonClient } from '@goxel/daemon-client';

const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel-daemon.sock',
  reconnect: true,
  reconnectInterval: 5000
});

async function createVoxelModel() {
  try {
    // Connect to daemon
    await client.connect();
    
    // Create project
    const project = await client.createProject({
      name: 'My Model',
      size: [64, 64, 64]
    });
    
    // Add voxels
    await client.addVoxel({
      position: [0, -16, 0],
      color: [255, 0, 0, 255]
    });
    
    // Save project
    await client.saveProject({
      filePath: './my-model.gox'
    });
    
  } catch (error) {
    console.error('Error:', error);
  } finally {
    await client.disconnect();
  }
}
```

### Planned Type Definitions
```typescript
// THESE TYPES ARE PLANNED - NOT IMPLEMENTED

interface VoxelPosition {
  x: number;
  y: number;
  z: number;
}

interface VoxelColor {
  r: number;
  g: number;
  b: number;
  a: number;
}

interface ProjectInfo {
  id: string;
  name: string;
  canvasSize: [number, number, number];
  layerCount: number;
  voxelCount: number;
}
```

## 🌉 MCP Server Integration (PLANNED)

The MCP bridge to connect the daemon with the existing MCP server is not implemented.

### Current State
- MCP server uses v13.4 CLI commands
- Each operation spawns a new process
- No persistent state between calls

### Planned Migration Path
1. **Phase 1**: Create daemon adapter in MCP server
2. **Phase 2**: Route commands through daemon
3. **Phase 3**: Deprecate CLI invocations
4. **Phase 4**: Full daemon-based MCP tools

### Conceptual MCP Adapter
```javascript
// THIS IS PLANNED - NOT IMPLEMENTED

class GoxelDaemonAdapter {
  constructor() {
    this.daemon = new GoxelDaemonClient();
    this.connected = false;
  }
  
  async ensureConnected() {
    if (!this.connected) {
      await this.daemon.connect();
      this.connected = true;
    }
  }
  
  async executeCommand(command, args) {
    await this.ensureConnected();
    
    // Map CLI commands to daemon methods
    switch (command) {
      case 'create':
        return await this.daemon.createProject({ 
          filePath: args[0] 
        });
        
      case 'add-voxel':
        return await this.daemon.addVoxel({
          position: [args[0], args[1], args[2]],
          color: [args[3], args[4], args[5], args[6]]
        });
        
      // ... more mappings
    }
  }
}
```

## 🚨 Common Integration Issues

### Current Issues (Alpha State)

1. **No Methods Work**
   ```bash
   # Any method call returns:
   {"error":{"code":-32601,"message":"Method not found"}}
   ```
   **Solution**: Wait for implementation or use v13.4 CLI

2. **No Client Libraries**
   ```bash
   npm install @goxel/daemon-client
   # Error: Package not found
   ```
   **Solution**: Use direct socket connection or wait

3. **Performance Can't Be Measured**
   - No working methods to benchmark
   - 700% improvement claim is theoretical
   **Solution**: Focus on functionality first

### Planned Error Handling

When methods are implemented, handle these error types:

```javascript
async function robustOperation() {
  const maxRetries = 3;
  let retries = 0;
  
  while (retries < maxRetries) {
    try {
      return await client.someMethod();
    } catch (error) {
      if (error.code === -32001) {
        // Project not found
        console.log('Creating new project...');
        await client.createProject();
      } else if (error.code === -32603) {
        // Internal error - retry
        retries++;
        await new Promise(r => setTimeout(r, 1000));
      } else {
        throw error;
      }
    }
  }
}
```

## 🔧 Development Setup

To work on implementing the missing functionality:

### 1. Build the Daemon
```bash
cd /path/to/goxel
scons daemon=1

# Note: You may need to add stub functions
# See V14_ACTUAL_STATUS.md for details
```

### 2. Implement a Test Method
Add to `src/daemon/json_rpc.c`:
```c
static json_rpc_response_t *handle_echo(const json_rpc_request_t *request) {
    json_value *result = json_object_new(1);
    json_object_push(result, "echo", json_string_new("Hello from daemon!"));
    return json_rpc_create_response_result(result, &request->id);
}

// Add to method registry
{"echo", handle_echo, "Test echo method"}
```

### 3. Test Your Implementation
```bash
./goxel-daemon

# In another terminal:
echo '{"jsonrpc":"2.0","method":"echo","id":1}' | nc -U /tmp/goxel-daemon.sock
# Should return: {"jsonrpc":"2.0","result":{"echo":"Hello from daemon!"},"id":1}
```

## 📚 Fallback Strategies

Until v14.0 is functional, use these approaches:

### 1. Continue Using v13.4 CLI
```bash
./goxel-headless create test.gox
./goxel-headless add-voxel 0 -16 0 255 0 0 255
./goxel-headless export test.obj
```

### 2. Use MCP Server with v13.4
The existing MCP server works well with v13.4:
```javascript
// This works today
const result = await server.callTool('create_voxel', {
  output_file: 'test.gox'
});
```

### 3. Batch Operations with Scripts
```bash
#!/bin/bash
# Efficient batch processing with v13.4
./goxel-headless --interactive << EOF
create test.gox
add-voxel 0 -16 0 255 0 0 255
add-voxel 1 -16 0 0 255 0 255
export test.obj
exit
EOF
```

## 🎯 Next Steps

### For Users
1. **Don't integrate yet** - Nothing works
2. **Monitor progress** - Check GitHub for updates
3. **Use v13.4** - It's stable and functional

### For Contributors
1. **Implement basic methods** - Start with echo, version, status
2. **Create client library** - Even a minimal one helps
3. **Connect Goxel core** - Replace mock contexts
4. **Add tests** - Validate each method

## 📊 Integration Readiness

| Component | Status | Ready for Integration |
|-----------|--------|----------------------|
| Daemon Infrastructure | ✅ Working | Yes (but useless alone) |
| JSON-RPC Methods | ❌ None | No |
| TypeScript Client | ❌ Doesn't exist | No |
| MCP Bridge | ❌ Not started | No |
| Documentation | ⚠️ Overly optimistic | Use with caution |

**Overall Integration Readiness: 0%**

---

*This guide will be updated as v14.0 development progresses. For now, continue using v13.4 for all production work.*