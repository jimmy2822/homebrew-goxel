/**
 * Basic Connection Pool Tests
 * 
 * Simplified tests to validate basic connection pool functionality
 * without the complex mock server setup.
 */

import { GoxelDaemonClient } from '../src/mcp-client/daemon_client';
import { ConnectionPool } from '../src/mcp-client/connection_pool';
import { HealthMonitor } from '../src/mcp-client/health_monitor';
import {
  ClientConnectionState,
  ConnectionPoolConfig,
  DEFAULT_POOL_CONFIG,
} from '../src/mcp-client/types';

// Test utilities
const createBasicPoolConfig = (): ConnectionPoolConfig => ({
  minConnections: 2,
  maxConnections: 5,
  acquireTimeout: 3000,
  idleTimeout: 60000,
  healthCheckInterval: 10000,
  maxRequestsPerConnection: 1000,
  loadBalancing: true,
  debug: false,
});

describe('Connection Pool Basic Tests', () => {
  
  // ========================================================================
  // CONFIGURATION TESTS
  // ========================================================================

  describe('Configuration', () => {
    test('should create pooled client with default config', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: false, // Start with disabled pooling
      });
      
      expect(client).toBeInstanceOf(GoxelDaemonClient);
      expect(client.getConnectionState()).toBe(ClientConnectionState.DISCONNECTED);
      expect(client.isConnected()).toBe(false);
    });

    test('should create pooled client with pool enabled', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
      });
      
      expect(client).toBeInstanceOf(GoxelDaemonClient);
      expect(client.getConnectionPool()).toBe(null); // Not connected yet
      expect(client.getHealthMonitor()).toBe(null); // Not connected yet
    });

    test('should merge pool config with defaults', () => {
      const customConfig = {
        minConnections: 3,
        maxConnections: 10,
      };
      
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: {
          ...DEFAULT_POOL_CONFIG,
          ...customConfig,
        },
      });
      
      const config = client.getConfig();
      expect(config.pool?.minConnections).toBe(3);
      expect(config.pool?.maxConnections).toBe(10);
      expect(config.pool?.loadBalancing).toBe(DEFAULT_POOL_CONFIG.loadBalancing);
    });
  });

  // ========================================================================
  // POOLING VS SINGLE CONNECTION MODE TESTS
  // ========================================================================

  describe('Connection Mode Selection', () => {
    test('should use single connection mode when pooling disabled', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: false,
      });
      
      const config = client.getConfig();
      expect(config.enablePooling).toBe(false);
    });

    test('should use pool mode when pooling enabled', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
      });
      
      const config = client.getConfig();
      expect(config.enablePooling).toBe(true);
      expect(config.pool).toBeDefined();
    });
  });

  // ========================================================================
  // POOL INSTANCE TESTS
  // ========================================================================

  describe('Pool Instance Creation', () => {
    test('should create connection pool instance', () => {
      const poolConfig = createBasicPoolConfig();
      const clientConfig = {
        socketPath: '/tmp/test.sock',
        timeout: 5000,
        retryAttempts: 3,
        retryDelay: 1000,
        maxMessageSize: 1024 * 1024,
        debug: false,
        autoReconnect: true,
        reconnectInterval: 2000,
      };
      
      const pool = new ConnectionPool(clientConfig, poolConfig);
      
      expect(pool).toBeInstanceOf(ConnectionPool);
      expect(pool.isReady()).toBe(false); // Not initialized yet
      expect(pool.getPoolState()).toBe('initializing');
    });

    test('should create health monitor instance', () => {
      const monitor = new HealthMonitor({
        interval: 5000,
        timeout: 2000,
        failureThreshold: 3,
        successThreshold: 2,
        healthyLatencyThreshold: 1000,
        degradedLatencyThreshold: 3000,
        enableTrendAnalysis: true,
        trendSampleSize: 10,
        debug: false,
      });
      
      expect(monitor).toBeInstanceOf(HealthMonitor);
    });
  });

  // ========================================================================
  // STATISTICS TESTS
  // ========================================================================

  describe('Statistics', () => {
    test('should provide pool statistics', () => {
      const poolConfig = createBasicPoolConfig();
      const clientConfig = {
        socketPath: '/tmp/test.sock',
        timeout: 5000,
        retryAttempts: 3,
        retryDelay: 1000,
        maxMessageSize: 1024 * 1024,
        debug: false,
        autoReconnect: true,
        reconnectInterval: 2000,
      };
      
      const pool = new ConnectionPool(clientConfig, poolConfig);
      const stats = pool.getStatistics();
      
      expect(stats).toBeDefined();
      expect(stats.totalConnections).toBe(0);
      expect(stats.availableConnections).toBe(0);
      expect(stats.busyConnections).toBe(0);
      expect(stats.healthyConnections).toBe(0);
      expect(stats.totalRequestsServed).toBe(0);
      expect(stats.averageRequestTime).toBe(0);
      expect(stats.poolUptime).toBe(0);
      expect(stats.connectionFailures).toBe(0);
    });

    test('should provide health monitor statistics', () => {
      const monitor = new HealthMonitor();
      const stats = monitor.getAggregatedStats();
      
      expect(stats).toBeDefined();
      expect(stats.totalConnections).toBe(0);
      expect(stats.healthyConnections).toBe(0);
      expect(stats.degradedConnections).toBe(0);
      expect(stats.unhealthyConnections).toBe(0);
      expect(stats.averageLatency).toBe(0);
      expect(stats.averageSuccessRate).toBe(0);
    });
  });

  // ========================================================================
  // ERROR HANDLING TESTS
  // ========================================================================

  describe('Error Handling', () => {
    test('should handle connection errors gracefully', async () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/nonexistent.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
        timeout: 100, // Short timeout for quick test
      });
      
      // Add error listener to prevent unhandled rejection
      client.on('error', () => {
        // Ignore errors for this test
      });
      
      // Should fail to connect to nonexistent socket
      await expect(client.connect()).rejects.toThrow();
      expect(client.isConnected()).toBe(false);
    }, 2000); // 2 second timeout

    test('should handle invalid pool configuration', () => {
      // This should work with type checking ensuring valid config
      expect(() => {
        new GoxelDaemonClient({
          socketPath: '/tmp/test.sock',
          enablePooling: true,
          pool: createBasicPoolConfig(),
        });
      }).not.toThrow();
    });
  });

  // ========================================================================
  // BATCH OPERATIONS TESTS (WITHOUT CONNECTION)
  // ========================================================================

  describe('Batch Operations Interface', () => {
    test('should provide batch execution interface', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
      });
      
      // Should have the method available
      expect(typeof client.executeBatch).toBe('function');
    });

    test('should reject batch operations when not connected', async () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
      });
      
      const batchRequests = [
        { method: 'get_version' },
        { method: 'get_status' },
      ];
      
      await expect(client.executeBatch(batchRequests)).rejects.toThrow();
    });
  });

  // ========================================================================
  // TYPE SAFETY TESTS
  // ========================================================================

  describe('Type Safety', () => {
    test('should enforce correct pool configuration types', () => {
      // Valid configuration
      const validConfig = createBasicPoolConfig();
      
      expect(validConfig.minConnections).toBeGreaterThan(0);
      expect(validConfig.maxConnections).toBeGreaterThan(validConfig.minConnections);
      expect(validConfig.acquireTimeout).toBeGreaterThan(0);
      expect(validConfig.healthCheckInterval).toBeGreaterThan(0);
      expect(typeof validConfig.loadBalancing).toBe('boolean');
    });

    test('should provide correct return types', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        pool: createBasicPoolConfig(),
      });
      
      expect(typeof client.isConnected()).toBe('boolean');
      expect(typeof client.getConnectionState()).toBe('string');
      expect(typeof client.getStatistics()).toBe('object');
      expect(typeof client.getConfig()).toBe('object');
    });
  });

  // ========================================================================
  // DEFAULT VALUES TESTS
  // ========================================================================

  describe('Default Values', () => {
    test('should use correct default pool configuration', () => {
      const defaults = DEFAULT_POOL_CONFIG;
      
      expect(defaults.minConnections).toBe(2);
      expect(defaults.maxConnections).toBe(10);
      expect(defaults.acquireTimeout).toBe(5000);
      expect(defaults.idleTimeout).toBe(60000);
      expect(defaults.healthCheckInterval).toBe(30000);
      expect(defaults.maxRequestsPerConnection).toBe(1000);
      expect(defaults.loadBalancing).toBe(true);
      expect(defaults.debug).toBe(false);
    });

    test('should apply default configuration when not specified', () => {
      const client = new GoxelDaemonClient({
        socketPath: '/tmp/test.sock',
        enablePooling: true,
        // No pool config specified - should use defaults
      });
      
      const config = client.getConfig();
      expect(config.pool?.minConnections).toBe(DEFAULT_POOL_CONFIG.minConnections);
      expect(config.pool?.maxConnections).toBe(DEFAULT_POOL_CONFIG.maxConnections);
    });
  });
});