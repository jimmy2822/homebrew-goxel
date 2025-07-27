/**
 * Advanced Goxel v14.0 Daemon Client Example
 * 
 * This example demonstrates advanced features including:
 * - Connection pooling and management
 * - Performance optimization techniques
 * - Error recovery and resilience
 * - Real-time monitoring and metrics
 * - Concurrent operations and batch processing
 */

import { GoxelDaemonClient } from '@goxel/daemon-client';
import { EventEmitter } from 'events';

interface AdvancedConfig {
  poolSize: number;
  batchSize: number;
  enableMetrics: boolean;
  enableCaching: boolean;
  retryAttempts: number;
}

interface PerformanceMetrics {
  operationsPerSecond: number;
  averageLatency: number;
  totalOperations: number;
  errorRate: number;
  memoryUsage: number;
}

interface CacheEntry<T> {
  data: T;
  timestamp: number;
  ttl: number;
}

/**
 * Advanced connection pool for managing multiple daemon connections
 */
class DaemonConnectionPool extends EventEmitter {
  private available: GoxelDaemonClient[] = [];
  private inUse: Set<GoxelDaemonClient> = new Set();
  private readonly maxSize: number;
  private readonly minSize: number;
  private connectionAttempts = 0;

  constructor(maxSize = 5, minSize = 2) {
    super();
    this.maxSize = maxSize;
    this.minSize = minSize;
  }

  async initialize(): Promise<void> {
    console.log(`🔧 Initializing connection pool (${this.minSize}-${this.maxSize} connections)`);
    
    // Create minimum number of connections
    for (let i = 0; i < this.minSize; i++) {
      await this.createConnection();
    }
    
    console.log(`✅ Connection pool initialized with ${this.available.length} connections`);
  }

  private async createConnection(): Promise<GoxelDaemonClient> {
    const client = new GoxelDaemonClient({
      socketPath: '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 30000,
      enableDebugLogging: false
    });

    await client.connect();
    
    // Set up connection event handlers
    client.on('disconnect', () => {
      this.emit('connectionLost', client);
      this.handleConnectionLoss(client);
    });

    client.on('error', (error) => {
      this.emit('connectionError', client, error);
    });

    this.available.push(client);
    this.connectionAttempts++;
    
    return client;
  }

  async acquire(): Promise<GoxelDaemonClient> {
    // Return available connection if exists
    if (this.available.length > 0) {
      const client = this.available.pop()!;
      this.inUse.add(client);
      return client;
    }

    // Create new connection if under max size
    if (this.inUse.size < this.maxSize) {
      const client = await this.createConnection();
      this.available.pop(); // Remove from available
      this.inUse.add(client);
      return client;
    }

    // Wait for connection to become available
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('Connection pool timeout'));
      }, 5000);

      const onRelease = (client: GoxelDaemonClient) => {
        clearTimeout(timeout);
        this.off('connectionReleased', onRelease);
        resolve(client);
      };

      this.on('connectionReleased', onRelease);
    });
  }

  release(client: GoxelDaemonClient): void {
    if (this.inUse.has(client)) {
      this.inUse.delete(client);
      this.available.push(client);
      this.emit('connectionReleased', client);
    }
  }

  private handleConnectionLoss(client: GoxelDaemonClient): void {
    this.inUse.delete(client);
    const index = this.available.indexOf(client);
    if (index !== -1) {
      this.available.splice(index, 1);
    }

    // Attempt to maintain minimum pool size
    if (this.available.length + this.inUse.size < this.minSize) {
      this.createConnection().catch(error => {
        console.error('Failed to recreate lost connection:', error.message);
      });
    }
  }

  getStats(): { available: number; inUse: number; total: number } {
    return {
      available: this.available.length,
      inUse: this.inUse.size,
      total: this.available.length + this.inUse.size
    };
  }

  async shutdown(): Promise<void> {
    console.log('🛑 Shutting down connection pool...');
    
    const allClients = [...this.available, ...this.inUse];
    await Promise.all(allClients.map(client => client.disconnect()));
    
    this.available = [];
    this.inUse.clear();
    
    console.log('✅ Connection pool shut down');
  }
}

/**
 * Performance monitoring and metrics collection
 */
class PerformanceMonitor {
  private metrics: PerformanceMetrics = {
    operationsPerSecond: 0,
    averageLatency: 0,
    totalOperations: 0,
    errorRate: 0,
    memoryUsage: 0
  };

  private operationTimes: number[] = [];
  private errorCount = 0;
  private startTime = Date.now();
  private lastUpdate = Date.now();

  recordOperation(latency: number, isError = false): void {
    this.operationTimes.push(latency);
    this.metrics.totalOperations++;
    
    if (isError) {
      this.errorCount++;
    }

    // Keep only recent operation times (last 1000 operations)
    if (this.operationTimes.length > 1000) {
      this.operationTimes.shift();
    }

    // Update metrics every 5 seconds
    const now = Date.now();
    if (now - this.lastUpdate > 5000) {
      this.updateMetrics();
      this.lastUpdate = now;
    }
  }

  private updateMetrics(): void {
    const now = Date.now();
    const elapsed = (now - this.startTime) / 1000; // seconds

    this.metrics.operationsPerSecond = this.metrics.totalOperations / elapsed;
    this.metrics.averageLatency = this.operationTimes.length > 0 
      ? this.operationTimes.reduce((a, b) => a + b, 0) / this.operationTimes.length 
      : 0;
    this.metrics.errorRate = this.errorCount / Math.max(this.metrics.totalOperations, 1);
    this.metrics.memoryUsage = process.memoryUsage().heapUsed / 1024 / 1024; // MB
  }

  getMetrics(): PerformanceMetrics {
    this.updateMetrics();
    return { ...this.metrics };
  }

  reset(): void {
    this.metrics = {
      operationsPerSecond: 0,
      averageLatency: 0,
      totalOperations: 0,
      errorRate: 0,
      memoryUsage: 0
    };
    this.operationTimes = [];
    this.errorCount = 0;
    this.startTime = Date.now();
    this.lastUpdate = Date.now();
  }

  printReport(): void {
    const metrics = this.getMetrics();
    console.log('\n📊 Performance Report:');
    console.log(`  Operations/sec: ${metrics.operationsPerSecond.toFixed(1)}`);
    console.log(`  Average latency: ${metrics.averageLatency.toFixed(2)}ms`);
    console.log(`  Total operations: ${metrics.totalOperations.toLocaleString()}`);
    console.log(`  Error rate: ${(metrics.errorRate * 100).toFixed(2)}%`);
    console.log(`  Memory usage: ${metrics.memoryUsage.toFixed(1)}MB\n`);
  }
}

/**
 * Simple cache implementation with TTL support
 */
class SimpleCache<T> {
  private cache = new Map<string, CacheEntry<T>>();

  set(key: string, value: T, ttl = 60000): void {
    this.cache.set(key, {
      data: value,
      timestamp: Date.now(),
      ttl
    });
  }

  get(key: string): T | null {
    const entry = this.cache.get(key);
    if (!entry) return null;

    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }

    return entry.data;
  }

  invalidate(keyPattern: string): void {
    for (const key of this.cache.keys()) {
      if (key.includes(keyPattern)) {
        this.cache.delete(key);
      }
    }
  }

  clear(): void {
    this.cache.clear();
  }

  size(): number {
    return this.cache.size;
  }
}

/**
 * Advanced Goxel client with connection pooling and performance optimization
 */
class AdvancedGoxelClient extends EventEmitter {
  private pool: DaemonConnectionPool;
  private monitor: PerformanceMonitor;
  private cache: SimpleCache<any>;
  private config: AdvancedConfig;
  private batchQueue: Array<{ operation: () => Promise<any>; resolve: Function; reject: Function }> = [];
  private batchTimeout: NodeJS.Timeout | null = null;

  constructor(config: Partial<AdvancedConfig> = {}) {
    super();
    
    this.config = {
      poolSize: 5,
      batchSize: 100,
      enableMetrics: true,
      enableCaching: true,
      retryAttempts: 3,
      ...config
    };

    this.pool = new DaemonConnectionPool(this.config.poolSize);
    this.monitor = new PerformanceMonitor();
    this.cache = new SimpleCache();

    // Set up pool event handlers
    this.pool.on('connectionLost', (client) => {
      console.warn('⚠️  Connection lost, pool will attempt to recover');
    });

    this.pool.on('connectionError', (client, error) => {
      console.error('Connection error:', error.message);
    });
  }

  async initialize(): Promise<void> {
    console.log('🚀 Initializing advanced Goxel client...');
    await this.pool.initialize();
    console.log('✅ Advanced client ready');
  }

  /**
   * Execute operation with connection pooling and error handling
   */
  private async executeWithRetry<T>(operation: (client: GoxelDaemonClient) => Promise<T>): Promise<T> {
    let lastError: Error;
    
    for (let attempt = 1; attempt <= this.config.retryAttempts; attempt++) {
      const client = await this.pool.acquire();
      const startTime = Date.now();
      
      try {
        const result = await operation(client);
        
        if (this.config.enableMetrics) {
          this.monitor.recordOperation(Date.now() - startTime, false);
        }
        
        return result;
      } catch (error) {
        lastError = error;
        
        if (this.config.enableMetrics) {
          this.monitor.recordOperation(Date.now() - startTime, true);
        }
        
        console.warn(`⚠️  Operation failed (attempt ${attempt}/${this.config.retryAttempts}): ${error.message}`);
        
        if (attempt === this.config.retryAttempts) {
          throw lastError;
        }
        
        // Wait before retry
        await new Promise(resolve => setTimeout(resolve, 1000 * attempt));
      } finally {
        this.pool.release(client);
      }
    }
    
    throw lastError!;
  }

  /**
   * Create project with caching
   */
  async createProject(params: { name: string; template?: string; size?: [number, number, number] }): Promise<any> {
    return this.executeWithRetry(async (client) => {
      const project = await client.createProject(params);
      
      if (this.config.enableCaching) {
        this.cache.set(`project_${project.project_id}`, project, 300000); // 5 minutes
      }
      
      this.emit('projectCreated', project);
      return project;
    });
  }

  /**
   * Optimized batch voxel addition with intelligent batching
   */
  async addVoxelsBatch(voxels: Array<{ position: [number, number, number]; color: [number, number, number, number] }>): Promise<void> {
    console.log(`🎯 Adding ${voxels.length} voxels using optimized batching...`);
    
    const batchSize = this.config.batchSize;
    const batches = [];
    
    for (let i = 0; i < voxels.length; i += batchSize) {
      batches.push(voxels.slice(i, i + batchSize));
    }
    
    console.log(`📦 Split into ${batches.length} batches of ${batchSize} voxels each`);
    
    // Process batches with limited concurrency
    const maxConcurrency = Math.min(3, this.config.poolSize);
    let processed = 0;
    
    for (let i = 0; i < batches.length; i += maxConcurrency) {
      const concurrentBatches = batches.slice(i, i + maxConcurrency);
      
      const promises = concurrentBatches.map(async (batch) => {
        return this.executeWithRetry(async (client) => {
          const result = await client.addVoxelBatch({ voxels: batch });
          processed += batch.length;
          
          const progress = Math.round((processed / voxels.length) * 100);
          process.stdout.write(`\rProgress: ${progress}% (${processed}/${voxels.length})`);
          
          return result;
        });
      });
      
      await Promise.all(promises);
    }
    
    console.log('\n✅ All voxels added successfully');
    
    // Invalidate related caches
    if (this.config.enableCaching) {
      this.cache.invalidate('project_info');
      this.cache.invalidate('layer_list');
    }
  }

  /**
   * Cached project info retrieval
   */
  async getProjectInfo(projectId?: string): Promise<any> {
    const cacheKey = `project_info_${projectId || 'current'}`;
    
    if (this.config.enableCaching) {
      const cached = this.cache.get(cacheKey);
      if (cached) {
        return cached;
      }
    }
    
    return this.executeWithRetry(async (client) => {
      const info = await client.getProjectInfo(projectId);
      
      if (this.config.enableCaching) {
        this.cache.set(cacheKey, info, 60000); // 1 minute cache
      }
      
      return info;
    });
  }

  /**
   * High-performance rendering with caching
   */
  async renderImage(params: any): Promise<any> {
    const cacheKey = `render_${JSON.stringify(params)}`;
    
    if (this.config.enableCaching) {
      const cached = this.cache.get(cacheKey);
      if (cached) {
        console.log('🎯 Using cached render result');
        return cached;
      }
    }
    
    return this.executeWithRetry(async (client) => {
      console.log('📸 Rendering image...');
      const result = await client.renderImage(params);
      
      if (this.config.enableCaching && result.render_time_ms > 1000) {
        // Cache slow renders for 5 minutes
        this.cache.set(cacheKey, result, 300000);
      }
      
      console.log(`✅ Rendered in ${result.render_time_ms}ms`);
      return result;
    });
  }

  /**
   * Generate complex procedural structures
   */
  async generateProceduralCity(size: number = 32): Promise<void> {
    console.log(`🏙️  Generating procedural city (${size}x${size})...`);
    
    const buildings = this.generateBuildingPositions(size);
    const voxels: Array<{ position: [number, number, number]; color: [number, number, number, number] }> = [];
    
    console.log(`🏗️  Generating ${buildings.length} buildings...`);
    
    for (const building of buildings) {
      const buildingVoxels = this.generateBuilding(building);
      voxels.push(...buildingVoxels);
    }
    
    console.log(`📊 Generated ${voxels.length} voxels for city`);
    
    // Add voxels using optimized batching
    await this.addVoxelsBatch(voxels);
    
    console.log('🎉 Procedural city generation completed!');
  }

  private generateBuildingPositions(citySize: number): Array<{ x: number; z: number; height: number; width: number; depth: number }> {
    const buildings = [];
    const gridSize = 8;
    
    for (let x = -citySize/2; x < citySize/2; x += gridSize) {
      for (let z = -citySize/2; z < citySize/2; z += gridSize) {
        if (Math.random() > 0.3) { // 70% chance of building
          buildings.push({
            x: x + Math.floor(Math.random() * 3) - 1,
            z: z + Math.floor(Math.random() * 3) - 1,
            height: 5 + Math.floor(Math.random() * 10),
            width: 3 + Math.floor(Math.random() * 3),
            depth: 3 + Math.floor(Math.random() * 3)
          });
        }
      }
    }
    
    return buildings;
  }

  private generateBuilding(building: { x: number; z: number; height: number; width: number; depth: number }): Array<{ position: [number, number, number]; color: [number, number, number, number] }> {
    const voxels = [];
    const colors = [
      [100, 100, 100, 255],  // Gray
      [120, 120, 120, 255],  // Light gray
      [80, 80, 80, 255],     // Dark gray
      [150, 120, 100, 255],  // Beige
      [100, 80, 60, 255]     // Brown
    ];
    
    const color = colors[Math.floor(Math.random() * colors.length)] as [number, number, number, number];
    
    for (let y = -16; y < -16 + building.height; y++) {
      for (let x = building.x; x < building.x + building.width; x++) {
        for (let z = building.z; z < building.z + building.depth; z++) {
          // Hollow building (walls only)
          if (y === -16 || // Foundation
              y === -16 + building.height - 1 || // Roof
              x === building.x || x === building.x + building.width - 1 || // Side walls
              z === building.z || z === building.z + building.depth - 1) { // Front/back walls
            
            voxels.push({
              position: [x, y, z],
              color: color
            });
          }
        }
      }
    }
    
    return voxels;
  }

  /**
   * Multi-angle rendering for comprehensive views
   */
  async renderMultipleAngles(outputDir: string = './renders'): Promise<string[]> {
    const angles = [
      { name: 'front', position: [0, 0, 30], target: [0, -10, 0] },
      { name: 'back', position: [0, 0, -30], target: [0, -10, 0] },
      { name: 'left', position: [-30, 0, 0], target: [0, -10, 0] },
      { name: 'right', position: [30, 0, 0], target: [0, -10, 0] },
      { name: 'top', position: [0, 30, 0], target: [0, -10, 0] },
      { name: 'iso', position: [25, 25, 25], target: [0, -10, 0] },
      { name: 'aerial', position: [0, 50, 0], target: [0, -10, 0] }
    ];
    
    console.log(`📸 Rendering ${angles.length} different angles...`);
    
    const fs = require('fs');
    const path = require('path');
    
    // Create output directory
    if (!fs.existsSync(outputDir)) {
      fs.mkdirSync(outputDir, { recursive: true });
    }
    
    const outputPaths: string[] = [];
    
    // Render all angles in parallel
    const renderPromises = angles.map(async (angle) => {
      const result = await this.renderImage({
        width: 1024,
        height: 1024,
        camera: {
          position: angle.position,
          target: angle.target,
          up: [0, 1, 0],
          fov: 45
        },
        format: 'png'
      });
      
      const outputPath = path.join(outputDir, `${angle.name}.png`);
      const buffer = Buffer.from(result.image_data, 'base64');
      fs.writeFileSync(outputPath, buffer);
      
      console.log(`✅ Rendered ${angle.name} view (${result.render_time_ms}ms)`);
      return outputPath;
    });
    
    const results = await Promise.all(renderPromises);
    outputPaths.push(...results);
    
    console.log(`🎉 All ${angles.length} renders completed`);
    return outputPaths;
  }

  /**
   * Get real-time performance metrics
   */
  getPerformanceMetrics(): PerformanceMetrics {
    return this.monitor.getMetrics();
  }

  /**
   * Print performance report
   */
  printPerformanceReport(): void {
    this.monitor.printReport();
    
    const poolStats = this.pool.getStats();
    console.log('🔗 Connection Pool Status:');
    console.log(`  Available: ${poolStats.available}`);
    console.log(`  In use: ${poolStats.inUse}`);
    console.log(`  Total: ${poolStats.total}\n`);
    
    if (this.config.enableCaching) {
      console.log(`💾 Cache size: ${this.cache.size()} entries\n`);
    }
  }

  /**
   * Shutdown client and cleanup resources
   */
  async shutdown(): Promise<void> {
    console.log('🛑 Shutting down advanced client...');
    
    // Clear batch queue
    if (this.batchTimeout) {
      clearTimeout(this.batchTimeout);
    }
    
    // Clear cache
    this.cache.clear();
    
    // Shutdown pool
    await this.pool.shutdown();
    
    console.log('✅ Advanced client shut down');
  }
}

// Example usage function
async function runAdvancedExample(): Promise<void> {
  const client = new AdvancedGoxelClient({
    poolSize: 5,
    batchSize: 500,
    enableMetrics: true,
    enableCaching: true,
    retryAttempts: 3
  });
  
  try {
    // Initialize advanced client
    await client.initialize();
    
    // Create project
    const project = await client.createProject({
      name: 'Advanced Procedural City',
      template: 'empty',
      size: [128, 64, 128]
    });
    
    console.log(`📦 Created project: ${project.name}`);
    
    // Generate procedural city
    await client.generateProceduralCity(64);
    
    // Get project info
    const info = await client.getProjectInfo();
    console.log(`\n📊 Final city: ${info.total_voxels.toLocaleString()} voxels`);
    
    // Render multiple angles
    await client.renderMultipleAngles('./city_renders');
    
    // Print performance report
    client.printPerformanceReport();
    
    console.log('\n🎉 Advanced example completed successfully!');
    
  } catch (error) {
    console.error('\n💥 Advanced example failed:', error.message);
    process.exit(1);
  } finally {
    await client.shutdown();
  }
}

// Run the example if this file is executed directly
if (require.main === module) {
  runAdvancedExample().catch(console.error);
}

export { AdvancedGoxelClient, DaemonConnectionPool, PerformanceMonitor, runAdvancedExample };