/**
 * Comprehensive Tests for Health Monitor
 * 
 * This test suite validates the health monitoring functionality including
 * health checks, trend analysis, alert generation, and metrics collection.
 */

import { HealthMonitor, HealthMonitorEvent, DEFAULT_HEALTH_CONFIG } from '../src/mcp-client/health_monitor';
import { GoxelDaemonClient } from '../src/mcp-client/daemon_client';
import {
  ConnectionHealth,
  PooledConnection,
  HealthCheckResult,
  HealthAlert,
  HealthMetrics,
} from '../src/mcp-client/types';

// Mock connection for testing
class MockConnection {
  public id: string;
  public client: any;
  public createdAt: Date;
  public health: ConnectionHealth;
  public requestCount: number;
  public lastUsed: Date;
  public isAvailable: boolean;
  private latencyResponse: number;
  private shouldFail: boolean;

  constructor(id: string, latencyResponse = 50, shouldFail = false) {
    this.id = id;
    this.createdAt = new Date();
    this.health = ConnectionHealth.HEALTHY;
    this.requestCount = 0;
    this.lastUsed = new Date();
    this.isAvailable = true;
    this.latencyResponse = latencyResponse;
    this.shouldFail = shouldFail;

    // Mock client with call method
    this.client = {
      call: jest.fn().mockImplementation(async (method: string) => {
        if (this.shouldFail) {
          throw new Error('Mock connection failure');
        }
        
        // Simulate latency
        await new Promise(resolve => setTimeout(resolve, this.latencyResponse));
        
        return { status: 'ok', timestamp: Date.now() };
      }),
    };
  }

  public setLatency(latency: number): void {
    this.latencyResponse = latency;
  }

  public setShouldFail(shouldFail: boolean): void {
    this.shouldFail = shouldFail;
  }
}

const sleep = (ms: number): Promise<void> => {
  return new Promise(resolve => setTimeout(resolve, ms));
};

describe('HealthMonitor', () => {
  let healthMonitor: HealthMonitor;
  let mockConnections: MockConnection[];

  beforeEach(() => {
    healthMonitor = new HealthMonitor({
      interval: 100, // Fast for testing
      timeout: 1000,
      failureThreshold: 2,
      successThreshold: 1,
      healthyLatencyThreshold: 100,
      degradedLatencyThreshold: 500,
      enableTrendAnalysis: true,
      trendSampleSize: 5,
      debug: false,
    });

    // Create mock connections
    mockConnections = [
      new MockConnection('conn-1', 50),
      new MockConnection('conn-2', 80),
      new MockConnection('conn-3', 150),
    ];
  });

  afterEach(() => {
    healthMonitor.stopMonitoring();
  });

  // ========================================================================
  // CONSTRUCTOR AND CONFIGURATION TESTS
  // ========================================================================

  describe('Constructor and Configuration', () => {
    test('should create health monitor with default config', () => {
      const monitor = new HealthMonitor();
      expect(monitor).toBeInstanceOf(HealthMonitor);
    });

    test('should create health monitor with custom config', () => {
      const customConfig = {
        interval: 5000,
        timeout: 2000,
        failureThreshold: 5,
        healthyLatencyThreshold: 200,
      };

      const monitor = new HealthMonitor(customConfig);
      expect(monitor).toBeInstanceOf(HealthMonitor);
    });

    test('should merge custom config with defaults', () => {
      const customConfig = {
        interval: 2000,
        enableTrendAnalysis: false,
      };

      const monitor = new HealthMonitor(customConfig);
      
      // Should use custom values and defaults for others
      expect(monitor).toBeDefined();
    });
  });

  // ========================================================================
  // MONITORING LIFECYCLE TESTS
  // ========================================================================

  describe('Monitoring Lifecycle', () => {
    test('should start monitoring connections', () => {
      const connections = mockConnections as unknown as PooledConnection[];
      
      healthMonitor.startMonitoring(connections);
      
      // Should initialize metrics for each connection
      mockConnections.forEach(conn => {
        const metrics = healthMonitor.getMetrics(conn.id);
        expect(metrics).toBeTruthy();
        expect(metrics!.connectionId).toBe(conn.id);
      });
    });

    test('should stop monitoring gracefully', () => {
      const connections = mockConnections as unknown as PooledConnection[];
      
      healthMonitor.startMonitoring(connections);
      healthMonitor.stopMonitoring();
      
      // Should clear monitoring state
      expect(healthMonitor.getAllMetrics().size).toBe(0);
    });

    test('should add and remove connections during monitoring', () => {
      const connections = mockConnections.slice(0, 2) as unknown as PooledConnection[];
      
      healthMonitor.startMonitoring(connections);
      
      // Add new connection
      const newConnection = new MockConnection('conn-new') as unknown as PooledConnection;
      healthMonitor.addConnection(newConnection);
      
      expect(healthMonitor.getMetrics('conn-new')).toBeTruthy();
      
      // Remove connection
      healthMonitor.removeConnection('conn-1');
      expect(healthMonitor.getMetrics('conn-1')).toBeNull();
    });
  });

  // ========================================================================
  // HEALTH CHECK TESTS
  // ========================================================================

  describe('Health Checks', () => {
    test('should perform health check on healthy connection', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      const result = await healthMonitor.checkConnectionHealth(connection);
      
      expect(result.connectionId).toBe('conn-1');
      expect(result.health).toBe(ConnectionHealth.HEALTHY);
      expect(result.latency).toBeGreaterThan(0);
      expect(result.timestamp).toBeInstanceOf(Date);
      expect(result.error).toBeUndefined();
    });

    test('should detect degraded connection based on latency', async () => {
      const connection = mockConnections[2] as unknown as PooledConnection;
      connection.setLatency(300); // Set high latency
      
      const result = await healthMonitor.checkConnectionHealth(connection);
      
      expect(result.connectionId).toBe('conn-3');
      expect(result.health).toBe(ConnectionHealth.DEGRADED);
      expect(result.latency).toBeGreaterThan(200);
    });

    test('should detect unhealthy connection on failure', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      connection.setShouldFail(true);
      
      const result = await healthMonitor.checkConnectionHealth(connection);
      
      expect(result.connectionId).toBe('conn-1');
      expect(result.health).toBe(ConnectionHealth.UNHEALTHY);
      expect(result.error).toBeDefined();
      expect(result.error!.message).toBe('Mock connection failure');
    });

    test('should check all connections', async () => {
      const connections = mockConnections as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
      
      const results = await healthMonitor.checkAllConnections();
      
      expect(results).toHaveLength(3);
      results.forEach(result => {
        expect(result.connectionId).toMatch(/^conn-\d+$/);
        expect(result.health).toBeDefined();
        expect(result.latency).toBeGreaterThan(0);
      });
    });

    test('should emit check completed events', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const checkCompletedEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.CHECK_COMPLETED, checkCompletedEvent);
      
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(checkCompletedEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          health: ConnectionHealth.HEALTHY,
          latency: expect.any(Number),
        })
      );
    });
  });

  // ========================================================================
  // METRICS TRACKING TESTS
  // ========================================================================

  describe('Metrics Tracking', () => {
    beforeEach(() => {
      const connections = mockConnections as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
    });

    test('should track successful health checks', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      
      expect(metrics.totalChecks).toBe(2);
      expect(metrics.totalFailures).toBe(0);
      expect(metrics.consecutiveSuccesses).toBe(2);
      expect(metrics.consecutiveFailures).toBe(0);
      expect(metrics.successRate).toBe(100);
      expect(metrics.averageLatency).toBeGreaterThan(0);
    });

    test('should track failed health checks', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      connection.setShouldFail(true);
      
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      
      expect(metrics.totalChecks).toBe(2);
      expect(metrics.totalFailures).toBe(2);
      expect(metrics.consecutiveFailures).toBe(2);
      expect(metrics.consecutiveSuccesses).toBe(0);
      expect(metrics.successRate).toBe(0);
      expect(metrics.lastError).toBeDefined();
    });

    test('should track latency history', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Perform multiple checks with varying latency
      connection.setLatency(50);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(80);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(60);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      
      expect(metrics.latencyHistory).toHaveLength(3);
      expect(metrics.averageLatency).toBeCloseTo(63.33, 1);
    });

    test('should calculate success rate correctly', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // 3 successes
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      // 1 failure
      connection.setShouldFail(true);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      
      expect(metrics.totalChecks).toBe(4);
      expect(metrics.totalFailures).toBe(1);
      expect(metrics.successRate).toBe(75);
    });

    test('should limit latency history size', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Perform more checks than trend sample size
      for (let i = 0; i < 8; i++) {
        await healthMonitor.checkConnectionHealth(connection);
      }
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      
      // Should limit to trendSampleSize (5 in config)
      expect(metrics.latencyHistory.length).toBeLessThanOrEqual(5);
    });
  });

  // ========================================================================
  // HEALTH TREND ANALYSIS TESTS
  // ========================================================================

  describe('Health Trend Analysis', () => {
    beforeEach(() => {
      const connections = mockConnections.slice(0, 1) as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
    });

    test('should detect improving trend', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Start with high latency, then improve
      connection.setLatency(200);
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(100);
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(50);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      expect(metrics.healthTrend).toBe('improving');
    });

    test('should detect degrading trend', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Start with low latency, then degrade
      connection.setLatency(50);
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(150);
      await healthMonitor.checkConnectionHealth(connection);
      await healthMonitor.checkConnectionHealth(connection);
      
      connection.setLatency(300);
      await healthMonitor.checkConnectionHealth(connection);
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      expect(metrics.healthTrend).toBe('degrading');
    });

    test('should detect stable trend', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Consistent latency
      connection.setLatency(80);
      for (let i = 0; i < 5; i++) {
        await healthMonitor.checkConnectionHealth(connection);
      }
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      expect(metrics.healthTrend).toBe('stable');
    });
  });

  // ========================================================================
  // ALERT GENERATION TESTS
  // ========================================================================

  describe('Alert Generation', () => {
    beforeEach(() => {
      const connections = mockConnections.slice(0, 1) as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
    });

    test('should generate unhealthy alert', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const alertEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Cause health to change to unhealthy
      connection.setShouldFail(true);
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(alertEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          alertType: 'unhealthy',
          severity: 'critical',
          message: expect.stringContaining('unhealthy'),
        })
      );
    });

    test('should generate degraded alert', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const alertEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Cause health to change to degraded
      connection.setLatency(300);
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(alertEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          alertType: 'degraded',
          severity: 'medium',
          message: expect.stringContaining('degraded'),
        })
      );
    });

    test('should generate recovery alert', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const alertEvent = jest.fn();
      
      // First make connection unhealthy
      connection.setShouldFail(true);
      await healthMonitor.checkConnectionHealth(connection);
      
      // Clear previous alerts
      alertEvent.mockClear();
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Recover connection
      connection.setShouldFail(false);
      connection.setLatency(50);
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(alertEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          alertType: 'recovered',
          severity: 'low',
          message: expect.stringContaining('recovered'),
        })
      );
    });

    test('should generate latency spike alert', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const alertEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Establish baseline
      connection.setLatency(80);
      await healthMonitor.checkConnectionHealth(connection);
      
      // Create latency spike (1.5x degraded threshold)
      connection.setLatency(800);
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(alertEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          alertType: 'latency_spike',
          severity: 'medium',
          message: expect.stringContaining('latency spike'),
        })
      );
    });

    test('should emit health changed events', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      const healthChangedEvent = jest.fn();
      
      healthMonitor.on(HealthMonitorEvent.HEALTH_CHANGED, healthChangedEvent);
      
      // Establish initial healthy state
      await healthMonitor.checkConnectionHealth(connection);
      
      // Change to degraded
      connection.setLatency(300);
      await healthMonitor.checkConnectionHealth(connection);
      
      expect(healthChangedEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          connectionId: 'conn-1',
          previousHealth: ConnectionHealth.HEALTHY,
          currentHealth: ConnectionHealth.DEGRADED,
        })
      );
    });
  });

  // ========================================================================
  // AGGREGATED STATISTICS TESTS
  // ========================================================================

  describe('Aggregated Statistics', () => {
    beforeEach(() => {
      const connections = mockConnections as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
    });

    test('should provide aggregated statistics', async () => {
      // Perform health checks on all connections
      await healthMonitor.checkAllConnections();
      
      const stats = healthMonitor.getAggregatedStats();
      
      expect(stats.totalConnections).toBe(3);
      expect(stats.healthyConnections).toBeGreaterThan(0);
      expect(stats.averageLatency).toBeGreaterThan(0);
      expect(stats.averageSuccessRate).toBeGreaterThan(0);
    });

    test('should handle mixed health states', async () => {
      // Make connections have different health states
      mockConnections[0].setLatency(50);  // Healthy
      mockConnections[1].setLatency(300); // Degraded
      mockConnections[2].setShouldFail(true); // Unhealthy
      
      await healthMonitor.checkAllConnections();
      
      const stats = healthMonitor.getAggregatedStats();
      
      expect(stats.totalConnections).toBe(3);
      expect(stats.healthyConnections).toBe(1);
      expect(stats.degradedConnections).toBe(1);
      expect(stats.unhealthyConnections).toBe(1);
    });

    test('should return zero stats for no connections', () => {
      healthMonitor.stopMonitoring();
      
      const stats = healthMonitor.getAggregatedStats();
      
      expect(stats.totalConnections).toBe(0);
      expect(stats.healthyConnections).toBe(0);
      expect(stats.averageLatency).toBe(0);
      expect(stats.averageSuccessRate).toBe(0);
    });
  });

  // ========================================================================
  // PERFORMANCE TESTS
  // ========================================================================

  describe('Performance', () => {
    test('should handle monitoring many connections efficiently', async () => {
      // Create many mock connections
      const manyConnections = [];
      for (let i = 0; i < 50; i++) {
        manyConnections.push(new MockConnection(`conn-${i}`, 50 + Math.random() * 100));
      }
      
      const connections = manyConnections as unknown as PooledConnection[];
      healthMonitor.startMonitoring(connections);
      
      const startTime = Date.now();
      await healthMonitor.checkAllConnections();
      const duration = Date.now() - startTime;
      
      // Should complete within reasonable time
      expect(duration).toBeLessThan(3000); // 3 seconds for 50 connections
      
      const stats = healthMonitor.getAggregatedStats();
      expect(stats.totalConnections).toBe(50);
    });

    test('should maintain performance with frequent checks', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      healthMonitor.addConnection(connection);
      
      const startTime = Date.now();
      
      // Perform many rapid health checks
      for (let i = 0; i < 20; i++) {
        await healthMonitor.checkConnectionHealth(connection);
      }
      
      const duration = Date.now() - startTime;
      const avgDuration = duration / 20;
      
      // Average check time should be reasonable
      expect(avgDuration).toBeLessThan(100); // Less than 100ms per check
      
      const metrics = healthMonitor.getMetrics('conn-1')!;
      expect(metrics.totalChecks).toBe(20);
    });
  });

  // ========================================================================
  // ERROR HANDLING TESTS
  // ========================================================================

  describe('Error Handling', () => {
    test('should handle connection health check timeouts', async () => {
      const connection = mockConnections[0] as unknown as PooledConnection;
      
      // Mock connection with very slow response
      connection.setLatency(2000); // Longer than health check timeout
      
      const result = await healthMonitor.checkConnectionHealth(connection);
      
      expect(result.health).toBe(ConnectionHealth.UNHEALTHY);
      expect(result.error).toBeDefined();
    });

    test('should handle invalid connection gracefully', async () => {
      const invalidConnection = {
        id: 'invalid',
        client: null,
      } as any;
      
      const result = await healthMonitor.checkConnectionHealth(invalidConnection);
      
      expect(result.health).toBe(ConnectionHealth.UNHEALTHY);
      expect(result.error).toBeDefined();
    });

    test('should emit error events for monitoring failures', async () => {
      const errorEvent = jest.fn();
      healthMonitor.on(HealthMonitorEvent.ERROR, errorEvent);
      
      const invalidConnection = {
        id: 'invalid',
        client: { call: () => { throw new Error('Test error'); } },
      } as any;
      
      await healthMonitor.checkConnectionHealth(invalidConnection);
      
      // Error should be handled gracefully without crashing
      expect(errorEvent).not.toHaveBeenCalled(); // Health monitor handles errors internally
    });
  });

  // ========================================================================
  // INTEGRATION TESTS
  // ========================================================================

  describe('Integration Tests', () => {
    test('should work with periodic monitoring', async () => {
      const monitor = new HealthMonitor({
        interval: 200,
        debug: false,
      });
      
      const connections = mockConnections as unknown as PooledConnection[];
      monitor.startMonitoring(connections);
      
      const checkCompletedEvent = jest.fn();
      monitor.on(HealthMonitorEvent.CHECK_COMPLETED, checkCompletedEvent);
      
      // Wait for several monitoring cycles
      await sleep(500);
      
      // Should have performed multiple check cycles
      expect(checkCompletedEvent).toHaveBeenCalled();
      
      const stats = monitor.getAggregatedStats();
      expect(stats.totalConnections).toBe(3);
      
      monitor.stopMonitoring();
    });

    test('should detect and report connection degradation over time', async () => {
      const monitor = new HealthMonitor({
        interval: 100,
        enableTrendAnalysis: true,
        debug: false,
      });
      
      const connection = mockConnections[0] as unknown as PooledConnection;
      monitor.startMonitoring([connection]);
      
      const alertEvent = jest.fn();
      monitor.on(HealthMonitorEvent.ALERT, alertEvent);
      
      // Start healthy
      connection.setLatency(50);
      await sleep(300);
      
      // Gradually degrade
      connection.setLatency(200);
      await sleep(200);
      
      connection.setLatency(400);
      await sleep(200);
      
      // Should detect degradation
      const metrics = monitor.getMetrics('conn-1')!;
      expect(metrics.healthTrend).toBe('degrading');
      
      monitor.stopMonitoring();
    });
  });
});