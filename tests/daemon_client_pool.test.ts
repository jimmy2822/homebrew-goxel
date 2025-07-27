/**
 * Comprehensive Tests for Connection Pool and Health Monitoring
 * 
 * This test suite validates the connection pool implementation, health monitoring,
 * load balancing, failover scenarios, and performance characteristics.
 */

import * as net from 'net';
import { EventEmitter } from 'events';
import { GoxelDaemonClient } from '../src/mcp-client/daemon_client';
import {
  ClientConnectionState,
  ClientEvent,
  DaemonMethod,
  JsonRpcErrorResponse,
  JsonRpcSuccessResponse,
  JsonRpcErrorCode,
  ConnectionError,
} from '../src/mcp-client/types';

// Test constants
const TEST_SOCKET_PATH = '/tmp/goxel_test_pool.sock';
const TEST_TIMEOUT = 2000;

// Advanced mock server for pool testing
class AdvancedMockServer extends EventEmitter {
  private server: net.Server | null = null;
  private clients: net.Socket[] = [];
  private messageHandlers = new Map<string, (params: unknown) => unknown>();
  private latencySimulation = 0;
  private errorRate = 0;
  private connectionLimit = 50;
  private requestCount = 0;

  public constructor() {
    super();
  }

  public async start(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // Clean up existing socket file
        try {
          require('fs').unlinkSync(TEST_SOCKET_PATH);
        } catch {
          // Ignore if file doesn't exist
        }

        this.server = net.createServer((socket) => {
          if (this.clients.length >= this.connectionLimit) {
            socket.destroy();
            return;
          }

          this.clients.push(socket);
          this.emit('client_connected', socket);

          socket.on('data', (data) => {
            this.handleClientMessage(socket, data);
          });

          socket.on('close', () => {
            const index = this.clients.indexOf(socket);
            if (index !== -1) {
              this.clients.splice(index, 1);
            }
            this.emit('client_disconnected', socket);
          });

          socket.on('error', (error) => {
            this.emit('client_error', socket, error);
          });
        });

        this.server.listen(TEST_SOCKET_PATH, () => {
          resolve();
        });

        this.server.on('error', reject);

      } catch (error) {
        reject(error);
      }
    });
  }

  public async stop(): Promise<void> {
    if (this.server) {
      // Close all client connections
      for (const client of this.clients) {
        client.destroy();
      }
      this.clients = [];

      // Close server
      return new Promise((resolve) => {
        this.server!.close(() => {
          this.server = null;
          resolve();
        });
      });
    }
  }

  public setLatencySimulation(latency: number): void {
    this.latencySimulation = latency;
  }

  public setErrorRate(rate: number): void {
    this.errorRate = Math.max(0, Math.min(1, rate));
  }

  public setConnectionLimit(limit: number): void {
    this.connectionLimit = limit;
  }

  public getClientCount(): number {
    return this.clients.length;
  }

  public getRequestCount(): number {
    return this.requestCount;
  }

  public resetRequestCount(): void {
    this.requestCount = 0;
  }

  public disconnectRandomClient(): void {
    if (this.clients.length > 0) {
      const randomIndex = Math.floor(Math.random() * this.clients.length);
      const client = this.clients[randomIndex];
      if (client) {
        client.destroy();
      }
    }
  }

  public setMessageHandler(method: string, handler: (params: unknown) => unknown): void {
    this.messageHandlers.set(method, handler);
  }

  private handleClientMessage(socket: net.Socket, data: Buffer): void {
    const messages = data.toString().trim().split('\n');
    
    for (const messageStr of messages) {
      if (!messageStr) continue;
      this.requestCount++;

      // Simulate errors
      if (Math.random() < this.errorRate) {
        const errorResponse = {
          jsonrpc: '2.0',
          error: {
            code: JsonRpcErrorCode.INTERNAL_ERROR,
            message: 'Simulated server error',
          },
          id: null,
        };
        socket.write(JSON.stringify(errorResponse) + '\n');
        continue;
      }

      try {
        const message = JSON.parse(messageStr);
        
        // Handle request with latency simulation
        if ('method' in message && 'id' in message) {
          setTimeout(() => {
            this.handleRequest(socket, message);
          }, this.latencySimulation);
        }
        
      } catch (error) {
        // Send parse error response
        const errorResponse = {
          jsonrpc: '2.0',
          error: {
            code: JsonRpcErrorCode.PARSE_ERROR,
            message: 'Parse error',
          },
          id: null,
        };
        socket.write(JSON.stringify(errorResponse) + '\n');
      }
    }
  }

  private handleRequest(socket: net.Socket, request: any): void {
    const { method, params, id } = request;

    // Check for custom handler
    const handler = this.messageHandlers.get(method);
    if (handler) {
      try {
        const result = handler(params);
        const response: JsonRpcSuccessResponse = {
          jsonrpc: '2.0',
          result,
          id,
        };
        socket.write(JSON.stringify(response) + '\n');
        return;
      } catch (error) {
        const errorResponse: JsonRpcErrorResponse = {
          jsonrpc: '2.0',
          error: {
            code: JsonRpcErrorCode.INTERNAL_ERROR,
            message: error instanceof Error ? error.message : 'Internal error',
          },
          id,
        };
        socket.write(JSON.stringify(errorResponse) + '\n');
        return;
      }
    }

    // Default handlers
    let result: unknown;
    switch (method) {
      case DaemonMethod.GET_VERSION:
        result = { version: '14.0.0', build: 'pool-test', timestamp: Date.now() };
        break;

      case DaemonMethod.GET_STATUS:
        result = { 
          status: 'running', 
          uptime: Date.now(),
          connections: this.clients.length,
          requests: this.requestCount,
        };
        break;

      case 'ping':
        result = { pong: Date.now() };
        break;

      case 'echo':
        result = { echo: params };
        break;

      case 'slow_operation':
        // Simulate slow operation
        setTimeout(() => {
          const response: JsonRpcSuccessResponse = {
            jsonrpc: '2.0',
            result: { completed: Date.now() },
            id,
          };
          socket.write(JSON.stringify(response) + '\n');
        }, 1000);
        return;

      default:
        const errorResponse: JsonRpcErrorResponse = {
          jsonrpc: '2.0',
          error: {
            code: JsonRpcErrorCode.METHOD_NOT_FOUND,
            message: `Method '${method}' not found`,
          },
          id,
        };
        socket.write(JSON.stringify(errorResponse) + '\n');
        return;
    }

    // Send success response
    const response: JsonRpcSuccessResponse = {
      jsonrpc: '2.0',
      result,
      id,
    };
    socket.write(JSON.stringify(response) + '\n');
  }
}

// Test utilities
const createPooledClient = (config: Partial<any> = {}): GoxelDaemonClient => {
  const defaultPoolConfig = {
    minConnections: 2,
    maxConnections: 5,
    acquireTimeout: 3000,
    idleTimeout: 60000,
    healthCheckInterval: 1000,
    maxRequestsPerConnection: 1000,
    loadBalancing: true,
    debug: false,
  };
  
  return new GoxelDaemonClient({
    socketPath: TEST_SOCKET_PATH,
    timeout: TEST_TIMEOUT,
    debug: false,
    enablePooling: true,
    pool: defaultPoolConfig,
    ...config,
  });
};

const sleep = (ms: number): Promise<void> => {
  return new Promise(resolve => setTimeout(resolve, ms));
};

// Test suite
describe('Connection Pool and Health Monitoring', () => {
  let mockServer: AdvancedMockServer;

  beforeEach(async () => {
    mockServer = new AdvancedMockServer();
    await mockServer.start();
  });

  afterEach(async () => {
    await mockServer.stop();
  });

  // ========================================================================
  // CONNECTION POOL INITIALIZATION TESTS
  // ========================================================================

  describe('Connection Pool Initialization', () => {
    test('should initialize pool with minimum connections', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3, 
          maxConnections: 8,
          acquireTimeout: 5000,
          idleTimeout: 60000,
          healthCheckInterval: 30000,
          maxRequestsPerConnection: 1000,
          loadBalancing: true,
          debug: false,
        }
      });
      
      await client.connect();
      
      const pool = client.getConnectionPool();
      expect(pool).toBeTruthy();
      expect(pool!.isReady()).toBe(true);
      
      const stats = pool!.getStatistics();
      expect(stats.totalConnections).toBe(3);
      expect(stats.healthyConnections).toBeGreaterThanOrEqual(3);
      
      await client.disconnect();
    });

    test('should emit pool ready event', async () => {
      const client = createPooledClient();
      const connectedEvent = jest.fn();
      
      client.on(ClientEvent.CONNECTED, connectedEvent);
      
      await client.connect();
      
      expect(connectedEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          timestamp: expect.any(Date),
          state: ClientConnectionState.CONNECTED,
        })
      );
      
      await client.disconnect();
    });

    test('should handle pool initialization failure', async () => {
      // Stop server to simulate connection failure
      await mockServer.stop();
      
      const client = createPooledClient();
      
      await expect(client.connect()).rejects.toThrow(ConnectionError);
      expect(client.getConnectionPool()).toBe(null);
    });

    test('should create health monitor with pool', async () => {
      const client = createPooledClient();
      
      await client.connect();
      
      const healthMonitor = client.getHealthMonitor();
      expect(healthMonitor).toBeTruthy();
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // LOAD BALANCING TESTS
  // ========================================================================

  describe('Load Balancing', () => {
    test('should distribute requests across connections', async () => {
      const client = createPooledClient({
        pool: { minConnections: 3, maxConnections: 3, loadBalancing: true }
      });
      
      await client.connect();
      
      // Make multiple requests
      const promises = [];
      for (let i = 0; i < 15; i++) {
        promises.push(client.call('ping'));
      }
      
      const results = await Promise.all(promises);
      expect(results).toHaveLength(15);
      
      // Verify all requests completed
      results.forEach(result => {
        expect(result).toHaveProperty('pong');
      });
      
      await client.disconnect();
    });

    test('should handle concurrent requests efficiently', async () => {
      const client = createPooledClient({
        pool: { minConnections: 4, maxConnections: 8 }
      });
      
      await client.connect();
      
      const startTime = Date.now();
      const promises = [];
      
      // Create 20 concurrent requests
      for (let i = 0; i < 20; i++) {
        promises.push(client.call(DaemonMethod.GET_VERSION));
      }
      
      const results = await Promise.all(promises);
      const duration = Date.now() - startTime;
      
      expect(results).toHaveLength(20);
      expect(duration).toBeLessThan(5000); // Should complete within 5 seconds
      
      const stats = client.getStatistics();
      expect(stats.requestsSent).toBeGreaterThanOrEqual(20);
      
      await client.disconnect();
    });

    test('should execute batch requests efficiently', async () => {
      const client = createPooledClient({
        pool: { minConnections: 3, maxConnections: 6 }
      });
      
      await client.connect();
      
      const batchRequests = [
        { method: DaemonMethod.GET_VERSION },
        { method: DaemonMethod.GET_STATUS },
        { method: 'ping' },
        { method: 'echo', params: { message: 'test' } },
      ];
      
      const results = await client.executeBatch(batchRequests);
      
      expect(results).toHaveLength(4);
      expect(results[0]).toHaveProperty('version');
      expect(results[1]).toHaveProperty('status');
      expect(results[2]).toHaveProperty('pong');
      expect(results[3]).toHaveProperty('echo');
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // HEALTH MONITORING TESTS
  // ========================================================================

  describe('Health Monitoring', () => {
    test('should perform periodic health checks', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2, 
          maxConnections: 4,
          healthCheckInterval: 500,
        }
      });
      
      await client.connect();
      
      const healthMonitor = client.getHealthMonitor()!;
      const healthChangedEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.CHECK_COMPLETED, healthChangedEvent);
      
      // Wait for health checks
      await sleep(1000);
      
      expect(healthChangedEvent).toHaveBeenCalled();
      
      const stats = healthMonitor.getAggregatedStats();
      expect(stats.totalConnections).toBeGreaterThanOrEqual(2);
      expect(stats.healthyConnections).toBeGreaterThan(0);
      
      await client.disconnect();
    });

    test('should detect degraded connections', async () => {
      // Set up server with high latency
      mockServer.setLatencySimulation(2000);
      
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          healthCheckInterval: 500,
        }
      });
      
      await client.connect();
      
      const healthMonitor = client.getHealthMonitor()!;
      const alertEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Wait for health checks to detect degradation
      await sleep(1500);
      
      // Some connections should be degraded due to high latency
      const stats = healthMonitor.getAggregatedStats();
      expect(stats.averageLatency).toBeGreaterThan(1000);
      
      await client.disconnect();
    });

    test('should handle connection failures gracefully', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3,
          maxConnections: 5,
        }
      });
      
      await client.connect();
      
      // Simulate random connection failures
      for (let i = 0; i < 2; i++) {
        mockServer.disconnectRandomClient();
        await sleep(200);
      }
      
      // Pool should still be functional
      const result = await client.call(DaemonMethod.GET_VERSION);
      expect(result).toHaveProperty('version');
      
      await client.disconnect();
    });

    test('should generate health alerts', async () => {
      mockServer.setErrorRate(0.8); // High error rate
      
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          healthCheckInterval: 300,
        }
      });
      
      await client.connect();
      
      const healthMonitor = client.getHealthMonitor()!;
      const alertEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Wait for health checks to detect problems
      await sleep(800);
      
      // Should have generated some alerts due to high error rate
      expect(alertEvent).toHaveBeenCalled();
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // CONNECTION SCALING TESTS
  // ========================================================================

  describe('Connection Scaling', () => {
    test('should scale up connections under load', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 6,
          acquireTimeout: 5000,
        }
      });
      
      await client.connect();
      
      const pool = client.getConnectionPool()!;
      const initialStats = pool.getStatistics();
      expect(initialStats.totalConnections).toBe(2);
      
      // Create high load
      const promises = [];
      for (let i = 0; i < 10; i++) {
        promises.push(client.call('slow_operation'));
      }
      
      // Wait a bit for scaling
      await sleep(500);
      
      const scaledStats = pool.getStatistics();
      expect(scaledStats.totalConnections).toBeGreaterThan(initialStats.totalConnections);
      
      // Wait for all requests to complete
      await Promise.all(promises);
      
      await client.disconnect();
    });

    test('should respect maximum connection limit', async () => {
      mockServer.setConnectionLimit(3); // Server limits connections
      
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 5,
        }
      });
      
      await client.connect();
      
      // Should not exceed server connection limit
      expect(mockServer.getClientCount()).toBeLessThanOrEqual(3);
      
      await client.disconnect();
    });

    test('should queue requests when connections unavailable', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 1,
          maxConnections: 2,
          acquireTimeout: 3000,
        }
      });
      
      await client.connect();
      
      const pool = client.getConnectionPool()!;
      
      // Create requests that will exhaust connection pool
      const promises = [];
      for (let i = 0; i < 5; i++) {
        promises.push(client.call('slow_operation'));
      }
      
      // Wait for queue to build up
      await sleep(200);
      
      const queueSize = pool.getQueueSize();
      expect(queueSize).toBeGreaterThan(0);
      
      // All requests should eventually complete
      const results = await Promise.all(promises);
      expect(results).toHaveLength(5);
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // FAILOVER AND RECOVERY TESTS
  // ========================================================================

  describe('Failover and Recovery', () => {
    test('should handle server restart gracefully', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 4,
        }
      });
      
      await client.connect();
      
      // Make initial request
      await client.call(DaemonMethod.GET_VERSION);
      
      // Restart server
      await mockServer.stop();
      await sleep(100);
      await mockServer.start();
      
      // Wait for reconnection
      await sleep(1000);
      
      // Should be able to make requests again
      const result = await client.call(DaemonMethod.GET_VERSION);
      expect(result).toHaveProperty('version');
      
      await client.disconnect();
    });

    test('should maintain service during partial connection loss', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3,
          maxConnections: 5,
        }
      });
      
      await client.connect();
      
      // Disconnect some connections
      mockServer.disconnectRandomClient();
      mockServer.disconnectRandomClient();
      
      await sleep(200);
      
      // Should still be able to serve requests
      const result = await client.call(DaemonMethod.GET_STATUS);
      expect(result).toHaveProperty('status');
      
      await client.disconnect();
    });

    test('should recover from temporary server unavailability', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 4,
        }
      });
      
      await client.connect();
      
      // Simulate temporary server unavailability
      mockServer.setErrorRate(1.0); // 100% error rate
      
      // Requests should fail temporarily
      await expect(client.call(DaemonMethod.GET_VERSION)).rejects.toThrow();
      
      // Restore server
      mockServer.setErrorRate(0.0);
      await sleep(500);
      
      // Should recover and work again
      const result = await client.call(DaemonMethod.GET_VERSION);
      expect(result).toHaveProperty('version');
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // PERFORMANCE TESTS
  // ========================================================================

  describe('Performance', () => {
    test('should handle high request volume efficiently', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 4,
          maxConnections: 8,
        }
      });
      
      await client.connect();
      
      const startTime = Date.now();
      const requestCount = 100;
      const promises = [];
      
      for (let i = 0; i < requestCount; i++) {
        promises.push(client.call('ping'));
      }
      
      const results = await Promise.all(promises);
      const duration = Date.now() - startTime;
      
      expect(results).toHaveLength(requestCount);
      
      const requestsPerSecond = (requestCount / duration) * 1000;
      expect(requestsPerSecond).toBeGreaterThan(50); // At least 50 RPS
      
      const stats = client.getStatistics();
      expect(stats.averageResponseTime).toBeLessThan(100); // < 100ms average
      
      await client.disconnect();
    });

    test('should maintain low latency under moderate load', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3,
          maxConnections: 6,
        }
      });
      
      await client.connect();
      
      const latencies: number[] = [];
      
      for (let i = 0; i < 20; i++) {
        const start = Date.now();
        await client.call('ping');
        const latency = Date.now() - start;
        latencies.push(latency);
      }
      
      const averageLatency = latencies.reduce((sum, lat) => sum + lat, 0) / latencies.length;
      const maxLatency = Math.max(...latencies);
      
      expect(averageLatency).toBeLessThan(50); // < 50ms average
      expect(maxLatency).toBeLessThan(200); // < 200ms max
      
      await client.disconnect();
    });

    test('should provide better performance than single connection', async () => {
      // Test single connection client
      const singleClient = new GoxelDaemonClient({
        socketPath: TEST_SOCKET_PATH,
        timeout: TEST_TIMEOUT,
        enablePooling: false,
      });
      
      await singleClient.connect();
      
      const singleStart = Date.now();
      const singlePromises = [];
      for (let i = 0; i < 20; i++) {
        singlePromises.push(singleClient.call('ping'));
      }
      await Promise.all(singlePromises);
      const singleDuration = Date.now() - singleStart;
      
      await singleClient.disconnect();
      
      // Test pooled client
      const pooledClient = createPooledClient({
        pool: { minConnections: 4, maxConnections: 8 }
      });
      
      await pooledClient.connect();
      
      const pooledStart = Date.now();
      const pooledPromises = [];
      for (let i = 0; i < 20; i++) {
        pooledPromises.push(pooledClient.call('ping'));
      }
      await Promise.all(pooledPromises);
      const pooledDuration = Date.now() - pooledStart;
      
      await pooledClient.disconnect();
      
      // Pooled client should be faster for concurrent requests
      expect(pooledDuration).toBeLessThan(singleDuration * 0.8); // At least 20% faster
    });
  });

  // ========================================================================
  // STATISTICS AND MONITORING TESTS
  // ========================================================================

  describe('Statistics and Monitoring', () => {
    test('should provide comprehensive pool statistics', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3,
          maxConnections: 6,
        }
      });
      
      await client.connect();
      
      // Make some requests
      await client.call(DaemonMethod.GET_VERSION);
      await client.call(DaemonMethod.GET_STATUS);
      await client.call('ping');
      
      const pool = client.getConnectionPool()!;
      const stats = pool.getStatistics();
      
      expect(stats.totalConnections).toBeGreaterThanOrEqual(3);
      expect(stats.healthyConnections).toBeGreaterThan(0);
      expect(stats.totalRequestsServed).toBeGreaterThanOrEqual(3);
      expect(stats.poolUptime).toBeGreaterThan(0);
      expect(stats.averageRequestTime).toBeGreaterThan(0);
      
      await client.disconnect();
    });

    test('should provide detailed connection information', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 4,
        }
      });
      
      await client.connect();
      
      const pool = client.getConnectionPool()!;
      const connections = pool.getConnections();
      
      expect(connections).toHaveLength(2);
      
      connections.forEach(conn => {
        expect(conn.id).toBeTruthy();
        expect(conn.createdAt).toBeInstanceOf(Date);
        expect(conn.health).toBeDefined();
        expect(conn.requestCount).toBeGreaterThanOrEqual(0);
        expect(conn.lastUsed).toBeInstanceOf(Date);
      });
      
      await client.disconnect();
    });

    test('should track health metrics accurately', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          healthCheckInterval: 500,
        }
      });
      
      await client.connect();
      
      const healthMonitor = client.getHealthMonitor()!;
      
      // Wait for health checks
      await sleep(1000);
      
      const stats = healthMonitor.getAggregatedStats();
      
      expect(stats.totalConnections).toBeGreaterThanOrEqual(2);
      expect(stats.averageLatency).toBeGreaterThan(0);
      expect(stats.averageSuccessRate).toBeGreaterThanOrEqual(0);
      
      const pool = client.getConnectionPool()!;
      const connections = pool.getConnections();
      connections.forEach((conn: any) => {
        const metrics = healthMonitor.getMetrics(conn.id);
        expect(metrics).toBeTruthy();
        expect(metrics!.totalChecks).toBeGreaterThan(0);
      });
      
      await client.disconnect();
    });
  });

  // ========================================================================
  // INTEGRATION TESTS
  // ========================================================================

  describe('Integration Tests', () => {
    test('should handle complete workflow with pooling', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 2,
          maxConnections: 5,
        }
      });
      
      // Connect with pooling
      await client.connect();
      expect(client.isConnected()).toBe(true);
      
      // Verify pool is ready
      const pool = client.getConnectionPool()!;
      expect(pool.isReady()).toBe(true);
      
      // Make various requests
      const version = await client.call(DaemonMethod.GET_VERSION);
      expect(version).toBeDefined();
      
      const status = await client.call(DaemonMethod.GET_STATUS);
      expect(status).toBeDefined();
      
      // Batch requests
      const batchResults = await client.executeBatch([
        { method: 'ping' },
        { method: 'echo', params: { message: 'test' } },
      ]);
      expect(batchResults).toHaveLength(2);
      
      // Check statistics
      const stats = client.getStatistics();
      expect(stats.requestsSent).toBeGreaterThan(0);
      
      // Disconnect cleanly
      await client.disconnect();
      expect(client.isConnected()).toBe(false);
    });

    test('should maintain state consistency during stress test', async () => {
      const client = createPooledClient({
        pool: { 
          minConnections: 3,
          maxConnections: 8,
          healthCheckInterval: 200,
        }
      });
      
      await client.connect();
      
      // Create stress conditions
      const promises = [];
      
      // High request volume
      for (let i = 0; i < 50; i++) {
        promises.push(client.call('ping'));
      }
      
      // Slow operations
      for (let i = 0; i < 5; i++) {
        promises.push(client.call('slow_operation'));
      }
      
      // Simulate connection instability
      setTimeout(() => {
        mockServer.disconnectRandomClient();
      }, 500);
      
      setTimeout(() => {
        mockServer.disconnectRandomClient();
      }, 1000);
      
      // All requests should complete successfully
      const results = await Promise.all(promises);
      expect(results).toHaveLength(55);
      
      // Pool should maintain consistent state
      const pool = client.getConnectionPool()!;
      const finalStats = pool.getStatistics();
      expect(finalStats.totalConnections).toBeGreaterThan(0);
      expect(finalStats.healthyConnections).toBeGreaterThan(0);
      
      await client.disconnect();
    });
  });
});