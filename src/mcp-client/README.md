# Goxel v14.0 TypeScript Daemon Client

A comprehensive TypeScript client library for communicating with the Goxel daemon server using JSON-RPC 2.0 protocol over Unix sockets.

## Features

- 🚀 **High Performance**: Connection pooling and load balancing for high-throughput scenarios
- 🔄 **Automatic Reconnection**: Built-in reconnection logic with configurable intervals
- 🛡️ **Type Safety**: Full TypeScript support with comprehensive type definitions
- 📊 **Health Monitoring**: Real-time connection health checks and performance metrics
- 🔧 **Error Handling**: Typed error classes and proper error propagation
- 📡 **Event-Driven**: Monitor connection state and receive real-time updates
- 🎯 **JSON-RPC 2.0**: Full protocol compliance with request correlation

## Installation

```bash
npm install goxel-daemon-client
```

## Quick Start

```typescript
import { GoxelDaemonClient } from 'goxel-daemon-client';

const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel-daemon.sock',
  timeout: 5000,
  retryAttempts: 3
});

// Connect to daemon
await client.connect();

// Add a voxel
await client.call('add_voxel', {
  x: 0, y: -16, z: 0,
  r: 255, g: 0, b: 0, a: 255
});

// Export project
await client.call('export_project', {
  format: 'obj',
  path: './output.obj'
});

// Disconnect
await client.disconnect();
```

## Advanced Usage

### Connection Pooling

For high-performance scenarios, enable connection pooling:

```typescript
const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel-daemon.sock',
  enablePooling: true,
  pool: {
    minConnections: 3,
    maxConnections: 10,
    healthCheckInterval: 30000
  }
});

await client.connect();

// Execute multiple requests in parallel
const results = await client.executeBatch([
  { method: 'add_voxel', params: { x: 0, y: -16, z: 0, r: 255, g: 0, b: 0, a: 255 } },
  { method: 'add_voxel', params: { x: 1, y: -16, z: 0, r: 0, g: 255, b: 0, a: 255 } },
  { method: 'add_voxel', params: { x: 2, y: -16, z: 0, r: 0, g: 0, b: 255, a: 255 } }
]);
```

### Event Handling

Monitor connection state and handle events:

```typescript
client.on('connected', (data) => {
  console.log('Connected to daemon at', data.timestamp);
});

client.on('disconnected', (data) => {
  console.log('Disconnected from daemon');
});

client.on('error', (data) => {
  console.error('Client error:', data.error.message);
});

client.on('reconnecting', (data) => {
  console.log('Attempting to reconnect...');
});
```

### Error Handling

The client provides typed error classes for different scenarios:

```typescript
import { ConnectionError, TimeoutError, JsonRpcClientError } from 'goxel-daemon-client';

try {
  await client.call('some_method');
} catch (error) {
  if (error instanceof ConnectionError) {
    console.error('Connection issue:', error.message);
  } else if (error instanceof TimeoutError) {
    console.error('Request timed out:', error.message);
  } else if (error instanceof JsonRpcClientError) {
    console.error(`RPC error (${error.code}):`, error.message);
  }
}
```

### Statistics and Monitoring

Track client performance and connection health:

```typescript
const stats = client.getStatistics();
console.log({
  requestsSent: stats.requestsSent,
  responsesReceived: stats.responsesReceived,
  averageResponseTime: `${stats.averageResponseTime.toFixed(2)}ms`,
  uptime: `${(stats.uptime / 1000).toFixed(1)}s`
});
```

## Configuration Options

```typescript
interface DaemonClientConfig {
  socketPath: string;           // Unix socket path (required)
  timeout?: number;             // Request timeout in ms (default: 5000)
  retryAttempts?: number;       // Number of retries (default: 3)
  retryDelay?: number;          // Delay between retries in ms (default: 1000)
  maxMessageSize?: number;      // Max message size in bytes (default: 1MB)
  debug?: boolean;              // Enable debug logging (default: false)
  autoReconnect?: boolean;      // Auto-reconnect on disconnect (default: true)
  reconnectInterval?: number;   // Reconnect interval in ms (default: 2000)
  enablePooling?: boolean;      // Enable connection pooling (default: false)
  pool?: ConnectionPoolConfig;  // Pool configuration
}
```

## Available Methods

The client supports all Goxel daemon methods:

- **System**: `get_version`, `get_status`, `shutdown`
- **Project**: `create_project`, `load_project`, `save_project`, `get_project_info`
- **Voxels**: `add_voxel`, `remove_voxel`, `get_voxel`, `clear_voxels`
- **Layers**: `create_layer`, `delete_layer`, `get_layers`, `set_active_layer`
- **Export/Import**: `export_project`, `import_model`, `render_image`

## MCP Integration

The client is designed for seamless MCP server integration:

```typescript
// In your MCP server tool handler
import { GoxelDaemonClient } from 'goxel-daemon-client';

class GoxelTool {
  private client: GoxelDaemonClient;

  async initialize() {
    this.client = new GoxelDaemonClient({
      socketPath: '/tmp/goxel-daemon.sock'
    });
    await this.client.connect();
  }

  async addVoxel(args: any) {
    return await this.client.call('add_voxel', args);
  }
}
```

## Testing

```bash
# Run all tests
npm test

# Run specific test suite
npm run test:client
npm run test:connection-pool
npm run test:health-monitoring

# Run with coverage
npm run test:coverage
```

## Examples

See the `examples/` directory for complete examples:

- `client_demo.ts` - Basic usage patterns
- `pool_demo.ts` - Connection pooling examples
- `mcp_integration_example.ts` - MCP server integration

## Performance

- Connection establishment: < 10ms
- Average request/response: < 5ms
- Throughput: 50+ requests/second (single connection)
- Throughput: 500+ requests/second (with pooling)

## License

GPL-3.0 - See LICENSE file for details