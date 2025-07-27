# Goxel v14.6 Client Architecture Design Document

**Author**: Yuki Tanaka (Agent-3)  
**Date**: January 2025  
**Version**: 1.0  
**Status**: Draft

## Executive Summary

This document presents the comprehensive client library architecture for Goxel v14.6, designed to provide robust, performant communication with the Goxel daemon server. The architecture emphasizes modularity, reliability, and developer experience while supporting advanced features like connection pooling, automatic retry mechanisms, and real-time notifications.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Components](#core-components)
3. [Connection Management](#connection-management)
4. [Request/Response Pipeline](#requestresponse-pipeline)
5. [Error Handling & Retry Strategy](#error-handling--retry-strategy)
6. [Connection Pooling Design](#connection-pooling-design)
7. [Event System](#event-system)
8. [Client API Design](#client-api-design)
9. [Performance Considerations](#performance-considerations)
10. [Implementation Plan](#implementation-plan)

## Architecture Overview

### Design Principles

1. **Separation of Concerns**: Clear boundaries between transport, protocol, and application layers
2. **Async-First**: Non-blocking operations with Promise-based APIs
3. **Resilience**: Automatic recovery from transient failures
4. **Developer Experience**: Intuitive APIs with TypeScript support
5. **Performance**: Optimized for high-throughput voxel operations

### Component Hierarchy

```
┌─────────────────────────────────────────────────────┐
│                  Client Application                  │
├─────────────────────────────────────────────────────┤
│                   High-Level API                     │
│         (GoxelClient, ProjectAPI, VoxelAPI)         │
├─────────────────────────────────────────────────────┤
│                Connection Pool Manager               │
│          (Load Balancing, Health Checks)            │
├─────────────────────────────────────────────────────┤
│              Core Connection Handler                 │
│         (Retry Logic, Queue Management)             │
├─────────────────────────────────────────────────────┤
│               JSON-RPC 2.0 Protocol                  │
│        (Serialization, Validation, Batching)        │
├─────────────────────────────────────────────────────┤
│                 Transport Layer                      │
│          (Unix Socket, TCP Socket, TLS)             │
└─────────────────────────────────────────────────────┘
```

## Core Components

### 1. Transport Layer

**Responsibilities**:
- Socket connection establishment
- Raw data transmission/reception
- Connection state management
- TLS encryption (TCP only)

**Implementation**:
```typescript
interface ITransport {
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  send(data: Buffer): Promise<void>;
  onData(callback: (data: Buffer) => void): void;
  onError(callback: (error: Error) => void): void;
  onClose(callback: () => void): void;
  isConnected(): boolean;
}

class UnixSocketTransport implements ITransport {
  private socket: net.Socket;
  private socketPath: string;
  // Implementation details...
}

class TcpSocketTransport implements ITransport {
  private socket: net.Socket;
  private host: string;
  private port: number;
  private useTls: boolean;
  // Implementation details...
}
```

### 2. Protocol Layer

**Responsibilities**:
- JSON-RPC 2.0 message formatting
- Request/response correlation
- Batch request handling
- Binary data encoding/decoding

**Key Classes**:
```typescript
class JsonRpcProtocol {
  createRequest(method: string, params?: any, id?: string | number): JsonRpcRequest;
  createNotification(method: string, params?: any): JsonRpcNotification;
  createBatchRequest(requests: JsonRpcRequest[]): JsonRpcRequest[];
  parseResponse(data: string): JsonRpcResponse | JsonRpcResponse[];
  validateMessage(message: any): boolean;
}

class BinaryDataHandler {
  encodeVoxelData(voxels: Voxel[], compression?: CompressionType): string;
  decodeVoxelData(data: string, format: VoxelFormat): Voxel[];
  calculateChecksum(data: Buffer): string;
}
```

### 3. Connection Handler

**Responsibilities**:
- Message queue management
- Request timeout handling
- Correlation of requests/responses
- Automatic reconnection

**Architecture**:
```typescript
class ConnectionHandler {
  private transport: ITransport;
  private protocol: JsonRpcProtocol;
  private pendingRequests: Map<string | number, PendingRequest>;
  private messageQueue: Queue<QueuedMessage>;
  private reconnectTimer?: NodeJS.Timeout;
  
  async send(method: string, params?: any, options?: RequestOptions): Promise<any>;
  async sendBatch(requests: BatchRequest[]): Promise<any[]>;
  notify(method: string, params?: any): void;
  
  private handleResponse(response: JsonRpcResponse): void;
  private handleReconnect(): Promise<void>;
  private processMessageQueue(): void;
}
```

### 4. High-Level APIs

**Modular API Structure**:
```typescript
class GoxelClient {
  readonly system: SystemAPI;
  readonly project: ProjectAPI;
  readonly voxel: VoxelAPI;
  readonly layer: LayerAPI;
  readonly render: RenderAPI;
  readonly export: ExportAPI;
  
  constructor(config: ClientConfig) {
    const handler = new ConnectionHandler(config);
    this.system = new SystemAPI(handler);
    this.project = new ProjectAPI(handler);
    // ... initialize other APIs
  }
}

class ProjectAPI {
  async create(options?: ProjectOptions): Promise<Project>;
  async open(path: string): Promise<Project>;
  async save(path?: string): Promise<void>;
  async getInfo(): Promise<ProjectInfo>;
}

class VoxelAPI {
  async add(x: number, y: number, z: number, color: Color): Promise<void>;
  async addBatch(voxels: Voxel[]): Promise<void>;
  async remove(x: number, y: number, z: number): Promise<void>;
  async paint(x: number, y: number, z: number, color: Color): Promise<void>;
  async clear(): Promise<void>;
}
```

## Connection Management

### Connection States

```typescript
enum ConnectionState {
  DISCONNECTED = 'disconnected',
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
  RECONNECTING = 'reconnecting',
  ERROR = 'error',
  CLOSING = 'closing'
}
```

### State Transitions

```
DISCONNECTED ─┬─→ CONNECTING ──→ CONNECTED
              │                      │
              │                      ↓
              └─← ERROR ←── RECONNECTING
                    ↑                │
                    └────────────────┘
```

### Connection Lifecycle Management

```typescript
class ConnectionManager {
  private state: ConnectionState = ConnectionState.DISCONNECTED;
  private stateListeners: Set<StateListener>;
  
  async connect(): Promise<void> {
    this.setState(ConnectionState.CONNECTING);
    try {
      await this.transport.connect();
      await this.authenticate();
      this.setState(ConnectionState.CONNECTED);
      this.startHeartbeat();
    } catch (error) {
      this.setState(ConnectionState.ERROR);
      if (this.config.autoReconnect) {
        this.scheduleReconnect();
      }
      throw error;
    }
  }
  
  private async authenticate(): Promise<void> {
    if (this.config.authToken) {
      await this.send('system.authenticate', {
        token: this.config.authToken
      });
    }
  }
  
  private startHeartbeat(): void {
    this.heartbeatTimer = setInterval(async () => {
      try {
        await this.send('system.ping', {}, { timeout: 5000 });
      } catch {
        this.handleDisconnection();
      }
    }, 30000);
  }
}
```

## Request/Response Pipeline

### Request Flow

1. **API Call**: High-level method invoked
2. **Validation**: Parameters validated
3. **Serialization**: Convert to JSON-RPC format
4. **Queueing**: Add to message queue
5. **Transmission**: Send over transport
6. **Correlation**: Track pending request
7. **Timeout**: Set timeout timer

### Response Flow

1. **Reception**: Receive data from transport
2. **Parsing**: Parse JSON-RPC response
3. **Correlation**: Match with pending request
4. **Validation**: Validate response format
5. **Deserialization**: Convert to domain objects
6. **Callback**: Resolve promise/call handler

### Pipeline Implementation

```typescript
class RequestPipeline {
  private requestInterceptors: RequestInterceptor[] = [];
  private responseInterceptors: ResponseInterceptor[] = [];
  
  async execute(request: Request): Promise<Response> {
    // Apply request interceptors
    let processedRequest = request;
    for (const interceptor of this.requestInterceptors) {
      processedRequest = await interceptor.process(processedRequest);
    }
    
    // Send request
    const response = await this.transport.send(processedRequest);
    
    // Apply response interceptors
    let processedResponse = response;
    for (const interceptor of this.responseInterceptors) {
      processedResponse = await interceptor.process(processedResponse);
    }
    
    return processedResponse;
  }
}
```

## Error Handling & Retry Strategy

### Error Classification

```typescript
enum ErrorType {
  NETWORK = 'network',           // Connection failures
  PROTOCOL = 'protocol',         // JSON-RPC errors
  TIMEOUT = 'timeout',          // Request timeouts
  APPLICATION = 'application',   // Goxel-specific errors
  AUTHENTICATION = 'auth',       // Auth failures
  RATE_LIMIT = 'rate_limit'     // Rate limiting
}

class ClientError extends Error {
  constructor(
    message: string,
    public type: ErrorType,
    public code?: number,
    public retryable: boolean = false
  ) {
    super(message);
  }
}
```

### Retry Strategy

```typescript
interface RetryStrategy {
  shouldRetry(error: ClientError, attempt: number): boolean;
  getDelay(attempt: number): number;
}

class ExponentialBackoffStrategy implements RetryStrategy {
  constructor(
    private maxAttempts: number = 3,
    private baseDelay: number = 1000,
    private maxDelay: number = 30000,
    private jitter: number = 0.1
  ) {}
  
  shouldRetry(error: ClientError, attempt: number): boolean {
    return error.retryable && attempt < this.maxAttempts;
  }
  
  getDelay(attempt: number): number {
    const exponentialDelay = this.baseDelay * Math.pow(2, attempt);
    const delay = Math.min(exponentialDelay, this.maxDelay);
    const jitterAmount = delay * this.jitter * (Math.random() * 2 - 1);
    return delay + jitterAmount;
  }
}
```

### Circuit Breaker

```typescript
class CircuitBreaker {
  private failures: number = 0;
  private lastFailureTime?: Date;
  private state: 'closed' | 'open' | 'half-open' = 'closed';
  
  constructor(
    private threshold: number = 5,
    private timeout: number = 60000
  ) {}
  
  async execute<T>(operation: () => Promise<T>): Promise<T> {
    if (this.state === 'open') {
      if (Date.now() - this.lastFailureTime!.getTime() > this.timeout) {
        this.state = 'half-open';
      } else {
        throw new ClientError('Circuit breaker is open', ErrorType.NETWORK);
      }
    }
    
    try {
      const result = await operation();
      this.onSuccess();
      return result;
    } catch (error) {
      this.onFailure();
      throw error;
    }
  }
}
```

## Connection Pooling Design

### Pool Architecture

```typescript
class ConnectionPool {
  private connections: PooledConnection[] = [];
  private available: Set<string> = new Set();
  private waitQueue: Queue<PoolRequest> = new Queue();
  
  constructor(private config: PoolConfig) {
    this.initializePool();
  }
  
  async acquire(): Promise<PooledConnection> {
    // Try to get available connection
    const connectionId = this.selectConnection();
    if (connectionId) {
      return this.connections.find(c => c.id === connectionId)!;
    }
    
    // Create new connection if under limit
    if (this.connections.length < this.config.maxConnections) {
      return this.createConnection();
    }
    
    // Wait for available connection
    return this.waitForConnection();
  }
  
  release(connection: PooledConnection): void {
    if (connection.requestCount > this.config.maxRequestsPerConnection) {
      this.replaceConnection(connection);
    } else {
      this.available.add(connection.id);
      this.processWaitQueue();
    }
  }
}
```

### Load Balancing

```typescript
class LoadBalancer {
  private strategies: Map<string, LoadBalancingStrategy> = new Map([
    ['round-robin', new RoundRobinStrategy()],
    ['least-connections', new LeastConnectionsStrategy()],
    ['random', new RandomStrategy()],
    ['weighted', new WeightedStrategy()]
  ]);
  
  selectConnection(
    connections: PooledConnection[],
    strategy: string = 'least-connections'
  ): PooledConnection | null {
    const availableConnections = connections.filter(c => c.isAvailable);
    if (availableConnections.length === 0) return null;
    
    const selector = this.strategies.get(strategy)!;
    return selector.select(availableConnections);
  }
}
```

### Health Monitoring

```typescript
class HealthMonitor {
  private healthChecks: Map<string, HealthCheck> = new Map();
  
  async checkHealth(connection: PooledConnection): Promise<HealthStatus> {
    const start = Date.now();
    try {
      await connection.client.send('system.ping', {}, { timeout: 5000 });
      const latency = Date.now() - start;
      
      return {
        healthy: true,
        latency,
        timestamp: new Date()
      };
    } catch (error) {
      return {
        healthy: false,
        error,
        timestamp: new Date()
      };
    }
  }
  
  startMonitoring(pool: ConnectionPool): void {
    setInterval(async () => {
      for (const connection of pool.getConnections()) {
        const health = await this.checkHealth(connection);
        this.updateConnectionHealth(connection, health);
      }
    }, this.config.checkInterval);
  }
}
```

## Event System

### Event Architecture

```typescript
interface EventEmitter<T extends Record<string, any>> {
  on<K extends keyof T>(event: K, handler: (data: T[K]) => void): void;
  off<K extends keyof T>(event: K, handler: (data: T[K]) => void): void;
  emit<K extends keyof T>(event: K, data: T[K]): void;
}

interface ClientEvents {
  connected: { timestamp: Date };
  disconnected: { timestamp: Date; reason?: string };
  error: { error: Error; context: string };
  notification: { method: string; params: any };
  stateChange: { from: ConnectionState; to: ConnectionState };
}

class EventManager implements EventEmitter<ClientEvents> {
  private listeners = new Map<keyof ClientEvents, Set<Function>>();
  
  on<K extends keyof ClientEvents>(
    event: K,
    handler: (data: ClientEvents[K]) => void
  ): void {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)!.add(handler);
  }
  
  emit<K extends keyof ClientEvents>(event: K, data: ClientEvents[K]): void {
    const handlers = this.listeners.get(event);
    if (handlers) {
      handlers.forEach(handler => handler(data));
    }
  }
}
```

### Notification Handling

```typescript
class NotificationHandler {
  private handlers = new Map<string, NotificationCallback>();
  
  register(method: string, callback: NotificationCallback): void {
    this.handlers.set(method, callback);
  }
  
  async handle(notification: JsonRpcNotification): Promise<void> {
    const handler = this.handlers.get(notification.method);
    if (handler) {
      try {
        await handler(notification.params);
      } catch (error) {
        console.error(`Error handling notification ${notification.method}:`, error);
      }
    }
  }
}
```

## Client API Design

### Fluent API Design

```typescript
// Project creation with fluent interface
const project = await client.project
  .create()
  .withName("MyVoxelArt")
  .withDimensions(64, 64, 64)
  .withBackgroundColor(0, 0, 0, 255)
  .execute();

// Batch voxel operations
await client.voxel
  .batch()
  .add(0, 0, 0, { r: 255, g: 0, b: 0, a: 255 })
  .add(1, 0, 0, { r: 0, g: 255, b: 0, a: 255 })
  .add(2, 0, 0, { r: 0, g: 0, b: 255, a: 255 })
  .remove(3, 0, 0)
  .execute();

// Layer operations with chaining
await client.layer
  .create("Background")
  .setVisible(true)
  .setOpacity(0.8)
  .activate()
  .execute();
```

### Builder Pattern for Complex Operations

```typescript
class RenderBuilder {
  private options: RenderOptions = {
    width: 1920,
    height: 1080,
    format: 'png'
  };
  
  resolution(width: number, height: number): this {
    this.options.width = width;
    this.options.height = height;
    return this;
  }
  
  camera(position: Vec3, target: Vec3, fov: number): this {
    this.options.camera = { position, target, fov };
    return this;
  }
  
  enableShadows(): this {
    this.options.shadows = true;
    return this;
  }
  
  async execute(): Promise<RenderResult> {
    return this.client.render.image(this.options);
  }
}
```

### Async Iterator Support

```typescript
// Iterate over layers
for await (const layer of client.layer.list()) {
  console.log(`Layer ${layer.name}: ${layer.voxelCount} voxels`);
}

// Stream voxel data
const voxelStream = client.voxel.stream({ 
  chunk: 1000,
  format: 'packed' 
});

for await (const chunk of voxelStream) {
  processVoxelChunk(chunk);
}
```

## Performance Considerations

### Request Batching

```typescript
class RequestBatcher {
  private pendingRequests: BatchableRequest[] = [];
  private batchTimer?: NodeJS.Timeout;
  
  constructor(
    private maxBatchSize: number = 100,
    private batchDelay: number = 10
  ) {}
  
  add(request: BatchableRequest): Promise<any> {
    return new Promise((resolve, reject) => {
      this.pendingRequests.push({ ...request, resolve, reject });
      
      if (this.pendingRequests.length >= this.maxBatchSize) {
        this.flush();
      } else if (!this.batchTimer) {
        this.batchTimer = setTimeout(() => this.flush(), this.batchDelay);
      }
    });
  }
  
  private async flush(): Promise<void> {
    const batch = this.pendingRequests.splice(0);
    if (batch.length === 0) return;
    
    try {
      const results = await this.sendBatch(batch);
      batch.forEach((req, i) => req.resolve(results[i]));
    } catch (error) {
      batch.forEach(req => req.reject(error));
    }
  }
}
```

### Response Caching

```typescript
class ResponseCache {
  private cache = new LRUCache<string, CachedResponse>({
    max: 1000,
    ttl: 60000 // 1 minute
  });
  
  getCacheKey(method: string, params: any): string {
    return `${method}:${JSON.stringify(params)}`;
  }
  
  async execute(
    method: string,
    params: any,
    executor: () => Promise<any>
  ): Promise<any> {
    const key = this.getCacheKey(method, params);
    const cached = this.cache.get(key);
    
    if (cached && !this.isStale(cached)) {
      return cached.data;
    }
    
    const result = await executor();
    this.cache.set(key, {
      data: result,
      timestamp: Date.now()
    });
    
    return result;
  }
}
```

### Memory Management

```typescript
class MemoryManager {
  private memoryLimit: number;
  private currentUsage: number = 0;
  
  constructor(limitMB: number = 100) {
    this.memoryLimit = limitMB * 1024 * 1024;
  }
  
  canAllocate(bytes: number): boolean {
    return this.currentUsage + bytes <= this.memoryLimit;
  }
  
  allocate(bytes: number): void {
    if (!this.canAllocate(bytes)) {
      this.performGarbageCollection();
      if (!this.canAllocate(bytes)) {
        throw new Error('Memory limit exceeded');
      }
    }
    this.currentUsage += bytes;
  }
  
  private performGarbageCollection(): void {
    // Clear caches, release unused connections, etc.
    global.gc?.();
  }
}
```

## Implementation Plan

### Phase 1: Core Infrastructure (Week 1-2)
- Transport layer implementation (Unix/TCP sockets)
- JSON-RPC 2.0 protocol handler
- Basic connection management
- Error handling framework

### Phase 2: Connection Management (Week 3-4)
- Retry mechanisms
- Circuit breaker implementation
- Connection state management
- Authentication support

### Phase 3: High-Level APIs (Week 5-6)
- Domain-specific API modules
- Fluent interface implementation
- Builder patterns
- TypeScript definitions

### Phase 4: Advanced Features (Week 7-8)
- Connection pooling
- Load balancing
- Health monitoring
- Request batching

### Phase 5: Optimization & Polish (Week 9-10)
- Performance profiling
- Memory optimization
- Response caching
- Documentation & examples

## Conclusion

This client architecture provides a robust foundation for Goxel v14.6 daemon communication, balancing performance, reliability, and developer experience. The modular design allows for progressive enhancement while maintaining backward compatibility.

Key benefits:
- **Resilient**: Automatic retry and reconnection handling
- **Performant**: Connection pooling and request batching
- **Developer-friendly**: Intuitive APIs with TypeScript support
- **Extensible**: Plugin architecture for custom behaviors
- **Production-ready**: Comprehensive error handling and monitoring

The implementation will focus on delivering core functionality early while building toward the full feature set outlined in this document.