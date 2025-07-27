# Goxel v14.0 TypeScript Client Library Guide

## 🎯 Overview

This guide provides comprehensive documentation for the Goxel v14.0 TypeScript client library. The client library offers a high-level, promise-based API for interacting with the Goxel daemon, complete with connection management, error handling, and performance optimization.

**Key Features:**
- **Promise-based API**: Modern async/await syntax support
- **Connection Pooling**: Automatic connection management and reuse
- **Type Safety**: Full TypeScript definitions for all operations
- **Error Recovery**: Automatic reconnection and failover handling
- **Performance Monitoring**: Built-in request timing and metrics
- **Batch Operations**: Efficient bulk operation support

## 🚀 Quick Start

### Installation

```bash
# Install the client library
npm install @goxel/daemon-client

# Or using yarn
yarn add @goxel/daemon-client
```

### Basic Usage

```typescript
import { GoxelDaemonClient } from '@goxel/daemon-client';

// Create client instance
const client = new GoxelDaemonClient({
  socketPath: '/tmp/goxel-daemon.sock',  // Unix socket (default)
  // OR tcp: { host: 'localhost', port: 8080 }, // TCP connection
  autoReconnect: true,
  timeout: 30000
});

async function example() {
  try {
    // Connect to daemon
    await client.connect();
    
    // Create a new project
    const project = await client.createProject({
      name: 'My Voxel Art',
      template: 'empty'
    });
    
    // Add some voxels
    await client.addVoxel({
      position: [0, -16, 0],
      color: [255, 0, 0, 255]
    });
    
    // Save the project
    await client.saveProject({
      file_path: './my_art.gox'
    });
    
    console.log('Project created successfully!');
  } catch (error) {
    console.error('Error:', error);
  } finally {
    await client.disconnect();
  }
}

example();
```

## 🏗️ Client Architecture

### Connection Management

```typescript
export interface ConnectionConfig {
  // Unix socket configuration (recommended)
  socketPath?: string;                    // Default: '/tmp/goxel-daemon.sock'
  
  // TCP configuration (for remote access)
  tcp?: {
    host: string;                         // Daemon host
    port: number;                         // Daemon port
  };
  
  // Connection options
  autoReconnect?: boolean;                // Default: true
  maxReconnectAttempts?: number;          // Default: 5
  reconnectDelay?: number;                // Default: 1000ms
  timeout?: number;                       // Default: 30000ms
  
  // Performance options
  enableCompression?: boolean;            // Default: true
  batchSize?: number;                     // Default: 100
  connectionPoolSize?: number;            // Default: 3
  
  // Debug options
  enableDebugLogging?: boolean;           // Default: false
  performanceMonitoring?: boolean;        // Default: true
}
```

### Error Handling

```typescript
export class GoxelDaemonError extends Error {
  constructor(
    public code: number,
    message: string,
    public data?: any,
    public requestId?: string
  ) {
    super(message);
    this.name = 'GoxelDaemonError';
  }
}

export class ConnectionError extends Error {
  constructor(message: string, public cause?: Error) {
    super(message);
    this.name = 'ConnectionError';
  }
}

export class TimeoutError extends Error {
  constructor(message: string, public timeout: number) {
    super(message);
    this.name = 'TimeoutError';
  }
}
```

## 🎨 API Reference

### Client Class

```typescript
export class GoxelDaemonClient {
  constructor(config?: ConnectionConfig);
  
  // Connection management
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  isConnected(): boolean;
  
  // Health and status
  getStatus(): Promise<DaemonStatus>;
  ping(): Promise<number>;                // Returns latency in ms
  
  // Project operations
  createProject(params: CreateProjectParams): Promise<ProjectInfo>;
  loadProject(params: LoadProjectParams): Promise<ProjectInfo>;
  saveProject(params: SaveProjectParams): Promise<SaveResult>;
  getProjectInfo(projectId?: string): Promise<ProjectInfo>;
  
  // Voxel operations
  addVoxel(params: AddVoxelParams): Promise<VoxelResult>;
  addVoxelBatch(params: AddVoxelBatchParams): Promise<BatchResult>;
  removeVoxel(params: RemoveVoxelParams): Promise<VoxelResult>;
  getVoxel(params: GetVoxelParams): Promise<VoxelInfo>;
  
  // Layer management
  createLayer(params: CreateLayerParams): Promise<LayerInfo>;
  listLayers(projectId?: string): Promise<LayerList>;
  setActiveLayer(params: SetActiveLayerParams): Promise<void>;
  
  // Drawing tools
  brushStroke(params: BrushStrokeParams): Promise<DrawResult>;
  fillRegion(params: FillRegionParams): Promise<FillResult>;
  selectRegion(params: SelectRegionParams): Promise<SelectionResult>;
  
  // Rendering
  renderImage(params: RenderImageParams): Promise<RenderResult>;
  exportModel(params: ExportModelParams): Promise<ExportResult>;
  
  // Configuration
  getConfig(section?: string): Promise<ConfigData>;
  setConfig(config: Partial<ConfigData>): Promise<void>;
  
  // Performance monitoring
  getPerformanceMetrics(timeRangeMinutes?: number): Promise<PerformanceMetrics>;
  
  // Events
  on(event: string, listener: Function): void;
  off(event: string, listener: Function): void;
}
```

### Type Definitions

```typescript
// Basic types
export type Position3D = [number, number, number];
export type Color = [number, number, number, number]; // RGBA
export type BoundingBox = {
  min: Position3D;
  max: Position3D;
};

// Project types
export interface ProjectInfo {
  project_id: string;
  name: string;
  canvas_size: Position3D;
  layer_count: number;
  total_voxels: number;
  bounding_box: BoundingBox;
  memory_usage: {
    total_mb: number;
    layers_mb: number;
    history_mb: number;
  };
  last_modified: string;
  file_path?: string;
}

export interface CreateProjectParams {
  name?: string;
  template?: 'empty' | 'cube' | 'sphere';
  size?: Position3D;
}

export interface LoadProjectParams {
  file_path: string;
  set_active?: boolean;
}

export interface SaveProjectParams {
  project_id?: string;
  file_path: string;
  format?: 'gox' | 'vox' | 'obj' | 'ply' | 'stl';
  options?: {
    compress?: boolean;
    include_history?: boolean;
  };
}

// Voxel types
export interface VoxelInfo {
  exists: boolean;
  position: Position3D;
  color: Color;
  material_id: string;
  layer_id: string;
}

export interface AddVoxelParams {
  position: Position3D;
  color: Color;
  project_id?: string;
  layer_id?: string;
}

export interface AddVoxelBatchParams {
  voxels: Array<{
    position: Position3D;
    color: Color;
  }>;
  project_id?: string;
  layer_id?: string;
}

export interface VoxelResult {
  success: boolean;
  voxel_count: number;
  affected_blocks?: string[];
}

export interface BatchResult {
  success: boolean;
  processed_count: number;
  failed_count: number;
  errors?: Array<{
    index: number;
    error: string;
  }>;
}

// Layer types
export interface LayerInfo {
  id: string;
  name: string;
  visible: boolean;
  opacity: number;
  voxel_count: number;
  is_active: boolean;
}

export interface LayerList {
  layers: LayerInfo[];
  active_layer: string;
  total_layers: number;
}

export interface CreateLayerParams {
  name: string;
  visible?: boolean;
  opacity?: number;
  project_id?: string;
}

// Drawing types
export interface BrushStrokeParams {
  start_position: Position3D;
  end_position: Position3D;
  brush_size: number;
  color: Color;
  mode: 'add' | 'remove' | 'paint';
  smoothing?: number;
  project_id?: string;
}

export interface FillRegionParams {
  seed_position: Position3D;
  color: Color;
  tolerance?: number;
  mode: 'replace' | 'add' | 'remove';
  project_id?: string;
}

// Rendering types
export interface Camera {
  position: Position3D;
  target: Position3D;
  up: Position3D;
  fov: number;
}

export interface Lighting {
  ambient: number;
  diffuse: number;
  sun_direction: Position3D;
}

export interface RenderImageParams {
  width: number;
  height: number;
  camera: Camera;
  lighting?: Lighting;
  format?: 'png' | 'jpg' | 'bmp';
  quality?: number;
  project_id?: string;
}

export interface RenderResult {
  image_data: string;           // Base64 encoded
  format: string;
  width: number;
  height: number;
  render_time_ms: number;
}

// Status and monitoring types
export interface DaemonStatus {
  daemon_version: string;
  uptime_seconds: number;
  active_connections: number;
  total_requests: number;
  average_response_time_ms: number;
  memory_usage: {
    daemon_mb: number;
    goxel_core_mb: number;
    total_mb: number;
  };
  active_projects: number;
  performance_stats: {
    requests_per_second: number;
    cache_hit_rate: number;
    errors_per_hour: number;
  };
}

export interface PerformanceMetrics {
  request_metrics: {
    total_requests: number;
    requests_per_minute: number[];
    average_latency_ms: number;
    p95_latency_ms: number;
    p99_latency_ms: number;
  };
  resource_metrics: {
    memory_usage_mb: number[];
    cpu_usage_percent: number[];
    active_connections: number[];
  };
  error_metrics: {
    total_errors: number;
    error_rate: number;
    errors_by_code: Record<string, number>;
  };
}
```

## 💡 Usage Examples

### Basic Voxel Art Creation

```typescript
import { GoxelDaemonClient } from '@goxel/daemon-client';

class VoxelArtist {
  private client: GoxelDaemonClient;
  
  constructor() {
    this.client = new GoxelDaemonClient({
      autoReconnect: true,
      enableDebugLogging: false
    });
  }
  
  async createSimpleStructure() {
    await this.client.connect();
    
    // Create project
    const project = await this.client.createProject({
      name: 'Simple House',
      template: 'empty'
    });
    
    console.log(`Created project: ${project.name}`);
    
    // Create foundation (batch operation for efficiency)
    const foundationVoxels = [];
    for (let x = -5; x <= 5; x++) {
      for (let z = -5; z <= 5; z++) {
        foundationVoxels.push({
          position: [x, -17, z] as Position3D,
          color: [139, 69, 19, 255] as Color  // Brown
        });
      }
    }
    
    await this.client.addVoxelBatch({
      voxels: foundationVoxels
    });
    
    // Create walls
    const wallVoxels = [];
    for (let y = -16; y <= -12; y++) {
      // Front and back walls
      for (let x = -5; x <= 5; x++) {
        wallVoxels.push(
          { position: [x, y, -5] as Position3D, color: [200, 200, 200, 255] as Color },
          { position: [x, y, 5] as Position3D, color: [200, 200, 200, 255] as Color }
        );
      }
      // Side walls
      for (let z = -4; z <= 4; z++) {
        wallVoxels.push(
          { position: [-5, y, z] as Position3D, color: [200, 200, 200, 255] as Color },
          { position: [5, y, z] as Position3D, color: [200, 200, 200, 255] as Color }
        );
      }
    }
    
    await this.client.addVoxelBatch({
      voxels: wallVoxels
    });
    
    // Create roof
    const roofVoxels = [];
    for (let x = -6; x <= 6; x++) {
      for (let z = -6; z <= 6; z++) {
        roofVoxels.push({
          position: [x, -11, z] as Position3D,
          color: [139, 0, 0, 255] as Color  // Dark red
        });
      }
    }
    
    await this.client.addVoxelBatch({
      voxels: roofVoxels
    });
    
    // Save the project
    await this.client.saveProject({
      file_path: './simple_house.gox'
    });
    
    console.log('Simple house created and saved!');
    
    await this.client.disconnect();
  }
}

// Usage
const artist = new VoxelArtist();
artist.createSimpleStructure().catch(console.error);
```

### Advanced Layer Management

```typescript
class LayerManager {
  constructor(private client: GoxelDaemonClient) {}
  
  async createLayeredModel() {
    // Create base layer
    await this.client.createLayer({
      name: 'Foundation',
      opacity: 1.0
    });
    
    // Create detail layer
    const detailLayer = await this.client.createLayer({
      name: 'Details',
      opacity: 0.8
    });
    
    // Work on foundation
    await this.client.setActiveLayer({
      layer_id: detailLayer.id
    });
    
    // Add foundation voxels...
    await this.client.addVoxelBatch({
      voxels: [
        { position: [0, -16, 0], color: [100, 100, 100, 255] },
        { position: [1, -16, 0], color: [100, 100, 100, 255] }
      ]
    });
    
    // Switch to details layer
    await this.client.setActiveLayer({
      layer_id: detailLayer.id
    });
    
    // Add detail voxels...
    await this.client.addVoxel({
      position: [0, -15, 0],
      color: [255, 0, 0, 255]
    });
    
    // List all layers
    const layers = await this.client.listLayers();
    console.log(`Project has ${layers.total_layers} layers`);
    
    layers.layers.forEach(layer => {
      console.log(`Layer: ${layer.name}, Voxels: ${layer.voxel_count}`);
    });
  }
}
```

### Real-time Rendering

```typescript
class RealTimeRenderer {
  constructor(private client: GoxelDaemonClient) {}
  
  async renderFromMultipleAngles() {
    const angles = [
      { position: [20, 20, 20], name: 'iso' },
      { position: [0, 0, 30], name: 'front' },
      { position: [30, 0, 0], name: 'side' },
      { position: [0, 30, 0], name: 'top' }
    ];
    
    const renderPromises = angles.map(async (angle) => {
      const result = await this.client.renderImage({
        width: 512,
        height: 512,
        camera: {
          position: angle.position as Position3D,
          target: [0, -16, 0],
          up: [0, 1, 0],
          fov: 45
        },
        lighting: {
          ambient: 0.3,
          diffuse: 0.7,
          sun_direction: [-0.5, -1, -0.5]
        },
        format: 'png'
      });
      
      return { ...result, angle: angle.name };
    });
    
    const renders = await Promise.all(renderPromises);
    
    renders.forEach(render => {
      console.log(`Rendered ${render.angle} view in ${render.render_time_ms}ms`);
      // Save image data (base64 to file)
      const fs = require('fs');
      const buffer = Buffer.from(render.image_data, 'base64');
      fs.writeFileSync(`./render_${render.angle}.png`, buffer);
    });
  }
}
```

### Error Handling Best Practices

```typescript
class RobustClient {
  private client: GoxelDaemonClient;
  private retryCount = 0;
  private maxRetries = 3;
  
  constructor() {
    this.client = new GoxelDaemonClient({
      autoReconnect: true,
      maxReconnectAttempts: 5,
      timeout: 30000
    });
    
    // Set up event listeners
    this.client.on('disconnect', () => {
      console.log('Connection lost, attempting to reconnect...');
    });
    
    this.client.on('reconnect', () => {
      console.log('Reconnected successfully');
      this.retryCount = 0; // Reset retry count
    });
    
    this.client.on('error', (error) => {
      console.error('Client error:', error);
    });
  }
  
  async safeOperation<T>(operation: () => Promise<T>): Promise<T> {
    while (this.retryCount < this.maxRetries) {
      try {
        return await operation();
      } catch (error) {
        if (error instanceof GoxelDaemonError) {
          // Handle specific daemon errors
          switch (error.code) {
            case -32001: // Project not found
              throw new Error(`Project not found: ${error.data?.project_id}`);
            case -32003: // Invalid position
              throw new Error(`Invalid position: ${JSON.stringify(error.data?.position)}`);
            case -32007: // Concurrent access
              // Retry after a brief delay
              await this.delay(1000);
              this.retryCount++;
              continue;
            default:
              throw error;
          }
        } else if (error instanceof ConnectionError) {
          // Connection issues - wait and retry
          await this.delay(2000);
          this.retryCount++;
          continue;
        } else {
          // Unknown error - don't retry
          throw error;
        }
      }
    }
    
    throw new Error(`Operation failed after ${this.maxRetries} attempts`);
  }
  
  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
  
  async createProjectSafely(params: CreateProjectParams): Promise<ProjectInfo> {
    return this.safeOperation(() => this.client.createProject(params));
  }
}
```

### Performance Optimization

```typescript
class PerformanceOptimizedClient {
  private client: GoxelDaemonClient;
  private batchBuffer: AddVoxelParams[] = [];
  private batchTimeout: NodeJS.Timeout | null = null;
  
  constructor() {
    this.client = new GoxelDaemonClient({
      batchSize: 500,
      enableCompression: true,
      performanceMonitoring: true
    });
  }
  
  // Batched voxel addition with automatic flushing
  addVoxelOptimized(params: AddVoxelParams): Promise<void> {
    return new Promise((resolve, reject) => {
      this.batchBuffer.push(params);
      
      // Clear existing timeout
      if (this.batchTimeout) {
        clearTimeout(this.batchTimeout);
      }
      
      // Flush immediately if buffer is full
      if (this.batchBuffer.length >= 100) {
        this.flushBatch().then(() => resolve()).catch(reject);
      } else {
        // Set timeout to flush after 100ms
        this.batchTimeout = setTimeout(() => {
          this.flushBatch().then(() => resolve()).catch(reject);
        }, 100);
      }
    });
  }
  
  private async flushBatch(): Promise<void> {
    if (this.batchBuffer.length === 0) return;
    
    const voxels = this.batchBuffer.map(params => ({
      position: params.position,
      color: params.color
    }));
    
    await this.client.addVoxelBatch({ voxels });
    this.batchBuffer = [];
    
    if (this.batchTimeout) {
      clearTimeout(this.batchTimeout);
      this.batchTimeout = null;
    }
  }
  
  // Monitor performance
  async getPerformanceReport(): Promise<void> {
    const metrics = await this.client.getPerformanceMetrics(60);
    
    console.log('Performance Report:');
    console.log(`Average latency: ${metrics.request_metrics.average_latency_ms}ms`);
    console.log(`P95 latency: ${metrics.request_metrics.p95_latency_ms}ms`);
    console.log(`Requests/second: ${metrics.request_metrics.total_requests / 60}`);
    console.log(`Error rate: ${metrics.error_metrics.error_rate * 100}%`);
    
    if (metrics.request_metrics.average_latency_ms > 5) {
      console.warn('High latency detected - consider optimizing batch sizes');
    }
  }
}
```

## 🔧 Configuration

### Client Configuration

```typescript
const client = new GoxelDaemonClient({
  // Connection
  socketPath: '/tmp/goxel-daemon.sock',
  
  // Performance
  batchSize: 100,                  // Optimal batch size for operations
  enableCompression: true,         // Enable JSON compression
  connectionPoolSize: 3,           // Number of concurrent connections
  
  // Reliability
  autoReconnect: true,
  maxReconnectAttempts: 5,
  reconnectDelay: 1000,
  timeout: 30000,
  
  // Development
  enableDebugLogging: false,
  performanceMonitoring: true
});
```

### Environment Variables

```bash
# Connection settings
GOXEL_DAEMON_SOCKET_PATH=/tmp/goxel-daemon.sock
GOXEL_DAEMON_TCP_HOST=localhost
GOXEL_DAEMON_TCP_PORT=8080

# Performance settings
GOXEL_CLIENT_BATCH_SIZE=100
GOXEL_CLIENT_TIMEOUT=30000
GOXEL_CLIENT_POOL_SIZE=3

# Debug settings
GOXEL_CLIENT_DEBUG=false
GOXEL_CLIENT_LOG_LEVEL=info
```

## 🚨 Error Handling

### Error Types and Handling

```typescript
try {
  await client.addVoxel({
    position: [1000, -16, 0],  // Out of bounds
    color: [255, 0, 0, 255]
  });
} catch (error) {
  if (error instanceof GoxelDaemonError) {
    switch (error.code) {
      case -32001: // Project not found
        console.log('Please create or load a project first');
        break;
      case -32003: // Invalid position
        console.log(`Position out of bounds: ${JSON.stringify(error.data?.position)}`);
        break;
      default:
        console.log(`Daemon error ${error.code}: ${error.message}`);
    }
  } else if (error instanceof ConnectionError) {
    console.log('Connection lost - attempting to reconnect...');
    await client.connect();
  } else if (error instanceof TimeoutError) {
    console.log(`Request timed out after ${error.timeout}ms`);
  } else {
    console.log('Unexpected error:', error);
  }
}
```

## 📊 Performance Best Practices

### Batch Operations
```typescript
// ❌ Inefficient: Individual operations
for (const voxel of voxels) {
  await client.addVoxel(voxel);
}

// ✅ Efficient: Batch operation
await client.addVoxelBatch({ voxels });
```

### Connection Reuse
```typescript
// ❌ Inefficient: Reconnecting for each operation
await client.connect();
await client.addVoxel(voxel1);
await client.disconnect();

await client.connect();
await client.addVoxel(voxel2);
await client.disconnect();

// ✅ Efficient: Reuse connection
await client.connect();
await client.addVoxel(voxel1);
await client.addVoxel(voxel2);
await client.disconnect();
```

### Async Operations
```typescript
// ❌ Sequential: Slow
await client.addVoxel(voxel1);
await client.addVoxel(voxel2);
await client.addVoxel(voxel3);

// ✅ Parallel: Fast (when order doesn't matter)
await Promise.all([
  client.addVoxel(voxel1),
  client.addVoxel(voxel2),
  client.addVoxel(voxel3)
]);
```

## 🔍 Debugging

### Debug Logging
```typescript
const client = new GoxelDaemonClient({
  enableDebugLogging: true
});

// Will log:
// [DEBUG] Connecting to daemon at /tmp/goxel-daemon.sock
// [DEBUG] Sending request: {"jsonrpc":"2.0","method":"goxel.add_voxel",...}
// [DEBUG] Received response in 2.3ms
```

### Performance Monitoring
```typescript
// Monitor request performance
client.on('request', (request) => {
  console.log(`Request: ${request.method}`);
});

client.on('response', (response, duration) => {
  console.log(`Response received in ${duration}ms`);
});

// Get detailed metrics
const metrics = await client.getPerformanceMetrics();
console.log(JSON.stringify(metrics, null, 2));
```

## 🧪 Testing

### Unit Testing Example
```typescript
import { GoxelDaemonClient } from '@goxel/daemon-client';
import { jest } from '@jest/globals';

describe('GoxelDaemonClient', () => {
  let client: GoxelDaemonClient;
  
  beforeEach(() => {
    client = new GoxelDaemonClient({
      socketPath: '/tmp/test-goxel-daemon.sock'
    });
  });
  
  afterEach(async () => {
    await client.disconnect();
  });
  
  test('should create project successfully', async () => {
    const project = await client.createProject({
      name: 'Test Project',
      template: 'empty'
    });
    
    expect(project.name).toBe('Test Project');
    expect(project.project_id).toBeDefined();
    expect(project.layer_count).toBe(1);
  });
  
  test('should handle invalid positions gracefully', async () => {
    await expect(client.addVoxel({
      position: [9999, -16, 0],
      color: [255, 0, 0, 255]
    })).rejects.toThrow('Position out of bounds');
  });
});
```

---

*This TypeScript client library guide provides comprehensive documentation for integrating with the Goxel v14.0 daemon. The client library abstracts the JSON RPC communication and provides a modern, promise-based API for all voxel operations.*

**Last Updated**: January 26, 2025  
**Version**: 14.0.0-dev  
**Status**: 📋 Template Ready for Implementation