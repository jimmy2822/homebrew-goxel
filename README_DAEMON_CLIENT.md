# Goxel v14.0 TypeScript Daemon Client

A comprehensive TypeScript client library for communicating with the Goxel v14.0 daemon server using JSON-RPC 2.0 protocol over Unix sockets.

## 🚀 Features

- **Full JSON-RPC 2.0 Compliance**: Complete implementation of the JSON-RPC 2.0 specification
- **Unix Socket Communication**: Efficient local communication with the Goxel daemon
- **TypeScript Support**: Full type safety with comprehensive type definitions
- **Automatic Reconnection**: Built-in auto-reconnection with configurable intervals
- **Request Timeout & Retry**: Robust error handling with timeout and retry logic
- **Event-Driven Architecture**: Listen to connection events and messages
- **Performance Monitoring**: Built-in statistics and performance metrics
- **Concurrent Requests**: Support for multiple simultaneous requests
- **Comprehensive Testing**: >90% test coverage with Jest

## 📦 Installation

Since this is part of the Goxel project, you can build and use it directly:

```bash
# Install dependencies
npm install

# Build the TypeScript code
npm run build

# Run tests
npm test

# Run tests with coverage
npm run test:coverage

# Run linting
npm run lint
```

## 🏃 Quick Start

### Basic Usage

```typescript
import { GoxelDaemonClient, DaemonMethod } from 'goxel-daemon-client';

const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel_daemon.sock',
  timeout: 5000,
});

try {
  await client.connect();
  
  const version = await client.call(DaemonMethod.GET_VERSION);
  console.log('Goxel version:', version);
  
  await client.call(DaemonMethod.CREATE_PROJECT, { name: 'my_project' });
  await client.call(DaemonMethod.ADD_VOXEL, {
    x: 0, y: -16, z: 0,
    r: 255, g: 0, b: 0, a: 255
  });
  
  await client.call(DaemonMethod.EXPORT_PROJECT, {
    format: 'obj',
    path: './output.obj'
  });
  
} finally {
  await client.disconnect();
}
```

### Event Handling

```typescript
import { GoxelDaemonClient, ClientEvent } from 'goxel-daemon-client';

const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel_daemon.sock',
  autoReconnect: true,
});

// Listen to connection events
client.on(ClientEvent.CONNECTED, (data) => {
  console.log('Connected at', data.timestamp);
});

client.on(ClientEvent.DISCONNECTED, (data) => {
  console.log('Disconnected at', data.timestamp);
});

client.on(ClientEvent.ERROR, (data) => {
  console.error('Error:', data.error.message);
});

await client.connect();
```

## 🔧 Configuration

### Client Configuration

```typescript
const client = new GoxelDaemonClient({
  // Required: Unix socket path
  socketPath: '/tmp/goxel_daemon.sock',
  
  // Optional: Request timeout in milliseconds (default: 5000)
  timeout: 5000,
  
  // Optional: Number of retry attempts (default: 3)
  retryAttempts: 3,
  
  // Optional: Delay between retries in milliseconds (default: 1000)
  retryDelay: 1000,
  
  // Optional: Maximum message size in bytes (default: 1MB)
  maxMessageSize: 1024 * 1024,
  
  // Optional: Enable debug logging (default: false)
  debug: false,
  
  // Optional: Auto-reconnect on connection loss (default: true)
  autoReconnect: true,
  
  // Optional: Reconnection interval in milliseconds (default: 2000)
  reconnectInterval: 2000,
});
```

## 📚 API Reference

### Main Client Class

#### `GoxelDaemonClient`

**Constructor**
```typescript
constructor(config: PartialDaemonClientConfig)
```

**Connection Management**
```typescript
async connect(): Promise<void>
async disconnect(): Promise<void>
isConnected(): boolean
getConnectionState(): ClientConnectionState
```

**Method Calls**
```typescript
async call(method: string, params?: JsonRpcParams, options?: CallOptions): Promise<unknown>
async sendNotification(method: string, params?: JsonRpcParams): Promise<void>
```

**Statistics & Monitoring**
```typescript
getStatistics(): ClientStatistics
resetStatistics(): void
getConfig(): Readonly<DaemonClientConfig>
```

### Available Daemon Methods

```typescript
enum DaemonMethod {
  // System methods
  GET_VERSION = 'get_version',
  GET_STATUS = 'get_status',
  SHUTDOWN = 'shutdown',
  
  // Project methods
  CREATE_PROJECT = 'create_project',
  LOAD_PROJECT = 'load_project',
  SAVE_PROJECT = 'save_project',
  GET_PROJECT_INFO = 'get_project_info',
  
  // Voxel operations
  ADD_VOXEL = 'add_voxel',
  REMOVE_VOXEL = 'remove_voxel',
  GET_VOXEL = 'get_voxel',
  CLEAR_VOXELS = 'clear_voxels',
  
  // Layer operations
  CREATE_LAYER = 'create_layer',
  DELETE_LAYER = 'delete_layer',
  GET_LAYERS = 'get_layers',
  SET_ACTIVE_LAYER = 'set_active_layer',
  
  // Export/Import
  EXPORT_PROJECT = 'export_project',
  IMPORT_MODEL = 'import_model',
  RENDER_IMAGE = 'render_image',
}
```

### Error Handling

The client provides specific error types for different scenarios:

```typescript
import {
  DaemonClientError,
  ConnectionError,
  TimeoutError,
  JsonRpcClientError,
} from 'goxel-daemon-client';

try {
  await client.call('some_method');
} catch (error) {
  if (error instanceof ConnectionError) {
    console.log('Connection problem:', error.message);
  } else if (error instanceof TimeoutError) {
    console.log('Request timed out:', error.message);
  } else if (error instanceof JsonRpcClientError) {
    console.log('Server error:', error.message, 'Code:', error.code);
  }
}
```

## 📖 Examples

### Running Examples

The project includes several example files:

```bash
# Simple client example
npm run dev simple_client

# Complete demo with all features
npm run dev client_demo

# Voxel art generator
npm run dev voxel_art_generator
```

### Example Files

- **`simple_client.ts`**: Basic usage example showing connection, voxel creation, and export
- **`client_demo.ts`**: Comprehensive demo covering all client features
- **`voxel_art_generator.ts`**: Advanced example creating various voxel art patterns

### Creating Voxel Art

```typescript
import { GoxelDaemonClient, DaemonMethod } from 'goxel-daemon-client';

const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel_daemon.sock'
});

await client.connect();
await client.call(DaemonMethod.CREATE_PROJECT, { name: 'art' });

// Create a red cube
for (let x = 0; x < 3; x++) {
  for (let y = 0; y < 3; y++) {
    for (let z = 0; z < 3; z++) {
      await client.call(DaemonMethod.ADD_VOXEL, {
        x, y: -16 + y, z,
        r: 255, g: 0, b: 0, a: 255
      });
    }
  }
}

await client.call(DaemonMethod.EXPORT_PROJECT, {
  format: 'obj',
  path: './red_cube.obj'
});

await client.disconnect();
```

## 🧪 Testing

The client includes comprehensive tests covering all functionality:

```bash
# Run all tests
npm test

# Run tests with coverage
npm run test:coverage

# Run tests in watch mode
npm run test:watch

# Run specific test file
npm test daemon_client.test.ts
```

### Test Coverage

The test suite achieves >90% coverage and includes:

- Connection management tests
- Method call tests  
- Error handling tests
- Event system tests
- Performance tests
- Integration tests

## 🔍 Debugging

### Enable Debug Logging

```typescript
const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel_daemon.sock',
  debug: true, // Enable debug output
});
```

### Monitor Statistics

```typescript
const stats = client.getStatistics();
console.log('Statistics:', {
  requestsSent: stats.requestsSent,
  responsesReceived: stats.responsesReceived,
  averageResponseTime: stats.averageResponseTime,
  uptime: stats.uptime,
  bytesTransmitted: stats.bytesTransmitted,
  bytesReceived: stats.bytesReceived,
});
```

## ⚡ Performance

### Concurrent Requests

The client supports multiple concurrent requests:

```typescript
const promises = [
  client.call(DaemonMethod.GET_VERSION),
  client.call(DaemonMethod.GET_STATUS),
  client.call(DaemonMethod.GET_PROJECT_INFO),
];

const results = await Promise.all(promises);
```

### Batch Operations

For bulk operations, use Promise.all for better performance:

```typescript
const voxelPromises = [];
for (let i = 0; i < 1000; i++) {
  voxelPromises.push(
    client.call(DaemonMethod.ADD_VOXEL, {
      x: i % 10,
      y: -16,
      z: Math.floor(i / 10),
      r: 255, g: 128, b: 64, a: 255,
    })
  );
}

await Promise.all(voxelPromises);
```

## 🛠️ Development

### Project Structure

```
src/
├── mcp-client/
│   ├── daemon_client.ts    # Main client implementation
│   └── types.ts           # TypeScript type definitions
├── examples/
│   ├── client_demo.ts     # Comprehensive demo
│   ├── simple_client.ts   # Basic usage example
│   └── voxel_art_generator.ts # Advanced art generation
└── index.ts              # Main export file

tests/
└── daemon_client.test.ts  # Comprehensive test suite
```

### Build Scripts

```bash
# Development build with watch
npm run build:watch

# Clean build directory
npm run clean

# Lint code
npm run lint

# Fix linting issues
npm run lint:fix
```

## 🤝 Contributing

1. Follow the existing code style and conventions
2. Add tests for new features
3. Update documentation as needed
4. Ensure all tests pass: `npm test`
5. Check linting: `npm run lint`

## 📄 License

This project is licensed under the GPL-3.0 License - see the main Goxel project for details.

## 🔗 Related

- [Goxel v14.0 Daemon Architecture](./GOXEL_V14_VERSION_PLAN.md)
- [Goxel Main Project](https://goxel.xyz)
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)