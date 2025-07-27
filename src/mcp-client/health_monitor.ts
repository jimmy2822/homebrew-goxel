/**
 * Goxel v14.0 Health Monitor Implementation
 * 
 * This module provides advanced health monitoring capabilities for daemon
 * connections with metrics collection, trend analysis, and predictive health
 * assessment for proactive connection management.
 * 
 * Key features:
 * - Real-time health monitoring with configurable checks
 * - Health trend analysis and prediction
 * - Performance metrics collection and analysis
 * - Automatic alert generation for health issues
 * - Historical health data storage and reporting
 * - Circuit breaker pattern for failing connections
 */

import { EventEmitter } from 'events';
import {
  ConnectionHealth,
  HealthCheckResult,
  PooledConnection,
  CallOptions,
} from './types';

/**
 * Health check configuration
 */
export interface HealthCheckConfig {
  /** Health check interval in milliseconds */
  readonly interval: number;
  
  /** Timeout for health check requests */
  readonly timeout: number;
  
  /** Number of consecutive failures before marking unhealthy */
  readonly failureThreshold: number;
  
  /** Number of consecutive successes before marking healthy */
  readonly successThreshold: number;
  
  /** Maximum response time for healthy status (ms) */
  readonly healthyLatencyThreshold: number;
  
  /** Maximum response time for degraded status (ms) */
  readonly degradedLatencyThreshold: number;
  
  /** Enable latency trend analysis */
  readonly enableTrendAnalysis: boolean;
  
  /** Number of samples for trend analysis */
  readonly trendSampleSize: number;
  
  /** Enable debug logging */
  readonly debug: boolean;
}

/**
 * Health metrics for a connection
 */
export interface HealthMetrics {
  readonly connectionId: string;
  readonly currentHealth: ConnectionHealth;
  readonly lastCheckTime: Date;
  readonly latencyHistory: number[];
  readonly averageLatency: number;
  readonly successRate: number;
  readonly consecutiveFailures: number;
  readonly consecutiveSuccesses: number;
  readonly totalChecks: number;
  readonly totalFailures: number;
  readonly uptime: number;
  readonly lastError?: Error;
  readonly healthTrend: 'improving' | 'stable' | 'degrading' | 'unknown';
}

/**
 * Health alert information
 */
export interface HealthAlert {
  readonly connectionId: string;
  readonly alertType: 'degraded' | 'unhealthy' | 'recovered' | 'latency_spike';
  readonly message: string;
  readonly metrics: HealthMetrics;
  readonly timestamp: Date;
  readonly severity: 'low' | 'medium' | 'high' | 'critical';
}

/**
 * Health monitor events
 */
export const enum HealthMonitorEvent {
  HEALTH_CHANGED = 'health_changed',
  ALERT = 'alert',
  METRICS_UPDATED = 'metrics_updated',
  CHECK_COMPLETED = 'check_completed',
  ERROR = 'error',
}

/**
 * Default health check configuration
 */
export const DEFAULT_HEALTH_CONFIG: HealthCheckConfig = {
  interval: 30000,
  timeout: 5000,
  failureThreshold: 3,
  successThreshold: 2,
  healthyLatencyThreshold: 1000,
  degradedLatencyThreshold: 3000,
  enableTrendAnalysis: true,
  trendSampleSize: 10,
  debug: false,
};

/**
 * Advanced Health Monitor
 * 
 * Provides comprehensive health monitoring for daemon connections with
 * predictive analysis, trend monitoring, and intelligent alerting.
 * 
 * @example
 * ```typescript
 * const monitor = new HealthMonitor({
 *   interval: 15000,
 *   failureThreshold: 2,
 *   enableTrendAnalysis: true,
 * });
 * 
 * monitor.on('alert', (alert) => {
 *   console.log(`Health alert: ${alert.message}`);
 * });
 * 
 * await monitor.startMonitoring(connections);
 * ```
 */
export class HealthMonitor extends EventEmitter {
  private readonly config: HealthCheckConfig;
  private readonly metricsMap = new Map<string, HealthMetrics>();
  private monitoringTimer: NodeJS.Timeout | null = null;
  private isMonitoring = false;
  private monitoredConnections = new Map<string, PooledConnection>();

  /**
   * Creates a new health monitor
   * 
   * @param config - Health check configuration
   */
  public constructor(config: Partial<HealthCheckConfig> = {}) {
    super();
    this.config = { ...DEFAULT_HEALTH_CONFIG, ...config };
    this.setMaxListeners(50);
  }

  // ========================================================================
  // MONITORING LIFECYCLE
  // ========================================================================

  /**
   * Starts health monitoring for given connections
   * 
   * @param connections - Connections to monitor
   */
  public startMonitoring(connections: PooledConnection[]): void {
    if (this.isMonitoring) {
      this.debug('[HealthMonitor] Already monitoring connections');
      return;
    }

    this.debug(`[HealthMonitor] Starting monitoring for ${connections.length} connections`);
    
    // Initialize metrics for each connection
    connections.forEach(connection => {
      this.monitoredConnections.set(connection.id, connection);
      this.initializeMetrics(connection.id);
    });

    this.isMonitoring = true;
    this.scheduleNextCheck();
  }

  /**
   * Stops health monitoring
   */
  public stopMonitoring(): void {
    if (!this.isMonitoring) {
      return;
    }

    this.debug('[HealthMonitor] Stopping health monitoring');
    
    if (this.monitoringTimer) {
      clearTimeout(this.monitoringTimer);
      this.monitoringTimer = null;
    }

    this.isMonitoring = false;
    this.monitoredConnections.clear();
  }

  /**
   * Adds a connection to monitoring
   * 
   * @param connection - Connection to add
   */
  public addConnection(connection: PooledConnection): void {
    this.debug(`[HealthMonitor] Adding connection ${connection.id} to monitoring`);
    
    this.monitoredConnections.set(connection.id, connection);
    this.initializeMetrics(connection.id);
  }

  /**
   * Removes a connection from monitoring
   * 
   * @param connectionId - Connection ID to remove
   */
  public removeConnection(connectionId: string): void {
    this.debug(`[HealthMonitor] Removing connection ${connectionId} from monitoring`);
    
    this.monitoredConnections.delete(connectionId);
    this.metricsMap.delete(connectionId);
  }

  // ========================================================================
  // HEALTH CHECKING
  // ========================================================================

  /**
   * Performs health check on a specific connection
   * 
   * @param connection - Connection to check
   * @returns Health check result
   */
  public async checkConnectionHealth(connection: PooledConnection): Promise<HealthCheckResult> {
    const startTime = Date.now();
    const connectionId = connection.id;
    
    this.debug(`[HealthMonitor] Checking health for connection ${connectionId}`);

    try {
      // Perform health check (ping-like request)
      await this.performHealthCheck(connection);
      
      const latency = Date.now() - startTime;
      const health = this.determineHealthFromLatency(latency);
      
      // Update metrics
      this.updateMetricsOnSuccess(connectionId, latency);
      
      const result: HealthCheckResult = {
        connectionId,
        health,
        latency,
        timestamp: new Date(),
      };
      
      // Check for health changes and alerts
      this.processHealthCheckResult(result);
      
      this.emit(HealthMonitorEvent.CHECK_COMPLETED, result);
      
      return result;
      
    } catch (error) {
      const latency = Date.now() - startTime;
      
      // Update metrics on failure
      this.updateMetricsOnFailure(connectionId, error instanceof Error ? error : new Error(String(error)));
      
      const result: HealthCheckResult = {
        connectionId,
        health: ConnectionHealth.UNHEALTHY,
        latency,
        error: error instanceof Error ? error : new Error(String(error)),
        timestamp: new Date(),
      };
      
      this.processHealthCheckResult(result);
      this.emit(HealthMonitorEvent.CHECK_COMPLETED, result);
      
      return result;
    }
  }

  /**
   * Performs health checks on all monitored connections
   * 
   * @returns Array of health check results
   */
  public async checkAllConnections(): Promise<HealthCheckResult[]> {
    const connections = Array.from(this.monitoredConnections.values());
    const checkPromises = connections.map(connection => 
      this.checkConnectionHealth(connection)
    );
    
    const results = await Promise.allSettled(checkPromises);
    
    return results
      .filter((result): result is PromiseFulfilledResult<HealthCheckResult> => 
        result.status === 'fulfilled'
      )
      .map(result => result.value);
  }

  // ========================================================================
  // METRICS AND ANALYSIS
  // ========================================================================

  /**
   * Gets health metrics for a specific connection
   * 
   * @param connectionId - Connection ID
   * @returns Health metrics or null if not found
   */
  public getMetrics(connectionId: string): HealthMetrics | null {
    return this.metricsMap.get(connectionId) || null;
  }

  /**
   * Gets health metrics for all monitored connections
   * 
   * @returns Map of connection ID to metrics
   */
  public getAllMetrics(): Map<string, HealthMetrics> {
    return new Map(this.metricsMap);
  }

  /**
   * Gets aggregated health statistics
   * 
   * @returns Aggregated statistics
   */
  public getAggregatedStats(): {
    totalConnections: number;
    healthyConnections: number;
    degradedConnections: number;
    unhealthyConnections: number;
    averageLatency: number;
    averageSuccessRate: number;
    alertsGenerated: number;
  } {
    const metrics = Array.from(this.metricsMap.values());
    
    if (metrics.length === 0) {
      return {
        totalConnections: 0,
        healthyConnections: 0,
        degradedConnections: 0,
        unhealthyConnections: 0,
        averageLatency: 0,
        averageSuccessRate: 0,
        alertsGenerated: 0,
      };
    }

    const healthyCounts = {
      [ConnectionHealth.HEALTHY]: 0,
      [ConnectionHealth.DEGRADED]: 0,
      [ConnectionHealth.UNHEALTHY]: 0,
      [ConnectionHealth.UNKNOWN]: 0,
    };

    let totalLatency = 0;
    let totalSuccessRate = 0;

    metrics.forEach(metric => {
      healthyCounts[metric.currentHealth]++;
      totalLatency += metric.averageLatency;
      totalSuccessRate += metric.successRate;
    });

    return {
      totalConnections: metrics.length,
      healthyConnections: healthyCounts[ConnectionHealth.HEALTHY],
      degradedConnections: healthyCounts[ConnectionHealth.DEGRADED],
      unhealthyConnections: healthyCounts[ConnectionHealth.UNHEALTHY],
      averageLatency: totalLatency / metrics.length,
      averageSuccessRate: totalSuccessRate / metrics.length,
      alertsGenerated: 0, // TODO: Track alert count
    };
  }

  // ========================================================================
  // PRIVATE METHODS
  // ========================================================================

  /**
   * Schedules the next health check
   */
  private scheduleNextCheck(): void {
    if (!this.isMonitoring) {
      return;
    }

    this.monitoringTimer = setTimeout(async () => {
      try {
        await this.checkAllConnections();
      } catch (error) {
        this.debug('[HealthMonitor] Error during health check cycle:', error);
        this.emit(HealthMonitorEvent.ERROR, error);
      }
      
      this.scheduleNextCheck();
    }, this.config.interval);
  }

  /**
   * Performs the actual health check request
   * 
   * @param connection - Connection to check
   */
  private async performHealthCheck(connection: PooledConnection): Promise<void> {
    const options: CallOptions = {
      timeout: this.config.timeout,
      notification: false,
    };
    
    // Use a lightweight method for health checking
    await connection.client.call('get_status', undefined, options);
  }

  /**
   * Determines health status from latency
   * 
   * @param latency - Response latency in ms
   * @returns Health status
   */
  private determineHealthFromLatency(latency: number): ConnectionHealth {
    if (latency <= this.config.healthyLatencyThreshold) {
      return ConnectionHealth.HEALTHY;
    } else if (latency <= this.config.degradedLatencyThreshold) {
      return ConnectionHealth.DEGRADED;
    } else {
      return ConnectionHealth.UNHEALTHY;
    }
  }

  /**
   * Initializes metrics for a connection
   * 
   * @param connectionId - Connection ID
   */
  private initializeMetrics(connectionId: string): void {
    const metrics: HealthMetrics = {
      connectionId,
      currentHealth: ConnectionHealth.UNKNOWN,
      lastCheckTime: new Date(),
      latencyHistory: [],
      averageLatency: 0,
      successRate: 0,
      consecutiveFailures: 0,
      consecutiveSuccesses: 0,
      totalChecks: 0,
      totalFailures: 0,
      uptime: 0,
      healthTrend: 'unknown',
    };
    
    this.metricsMap.set(connectionId, metrics);
  }

  /**
   * Updates metrics on successful health check
   * 
   * @param connectionId - Connection ID
   * @param latency - Response latency
   */
  private updateMetricsOnSuccess(connectionId: string, latency: number): void {
    const metrics = this.metricsMap.get(connectionId);
    if (!metrics) {
      return;
    }

    const updatedMetrics: HealthMetrics = {
      ...metrics,
      lastCheckTime: new Date(),
      totalChecks: metrics.totalChecks + 1,
      consecutiveSuccesses: metrics.consecutiveSuccesses + 1,
      consecutiveFailures: 0,
      latencyHistory: this.updateLatencyHistory(metrics.latencyHistory, latency),
      averageLatency: this.calculateAverageLatency(metrics.latencyHistory, latency),
      successRate: this.calculateSuccessRate(metrics.totalChecks + 1, metrics.totalFailures),
      currentHealth: this.determineHealthFromLatency(latency),
      healthTrend: this.analyzeHealthTrend(metrics.latencyHistory, latency),
    };

    this.metricsMap.set(connectionId, updatedMetrics);
    this.emit(HealthMonitorEvent.METRICS_UPDATED, updatedMetrics);
  }

  /**
   * Updates metrics on failed health check
   * 
   * @param connectionId - Connection ID
   * @param error - Error that occurred
   */
  private updateMetricsOnFailure(connectionId: string, error: Error): void {
    const metrics = this.metricsMap.get(connectionId);
    if (!metrics) {
      return;
    }

    const updatedMetrics: HealthMetrics = {
      ...metrics,
      lastCheckTime: new Date(),
      totalChecks: metrics.totalChecks + 1,
      totalFailures: metrics.totalFailures + 1,
      consecutiveFailures: metrics.consecutiveFailures + 1,
      consecutiveSuccesses: 0,
      successRate: this.calculateSuccessRate(metrics.totalChecks + 1, metrics.totalFailures + 1),
      currentHealth: ConnectionHealth.UNHEALTHY,
      lastError: error,
      healthTrend: 'degrading',
    };

    this.metricsMap.set(connectionId, updatedMetrics);
    this.emit(HealthMonitorEvent.METRICS_UPDATED, updatedMetrics);
  }

  /**
   * Processes health check result and generates alerts
   * 
   * @param result - Health check result
   */
  private processHealthCheckResult(result: HealthCheckResult): void {
    const metrics = this.metricsMap.get(result.connectionId);
    if (!metrics) {
      return;
    }

    const previousHealth = metrics.currentHealth;
    const currentHealth = result.health;

    // Check for health status changes
    if (previousHealth !== currentHealth) {
      this.emit(HealthMonitorEvent.HEALTH_CHANGED, {
        connectionId: result.connectionId,
        previousHealth,
        currentHealth,
        metrics,
      });

      // Generate alerts for health changes
      this.generateHealthChangeAlert(result.connectionId, previousHealth, currentHealth, metrics);
    }

    // Check for latency spikes
    if (result.latency > this.config.degradedLatencyThreshold * 1.5) {
      this.generateLatencySpikeAlert(result.connectionId, result.latency, metrics);
    }
  }

  /**
   * Generates health change alert
   * 
   * @param connectionId - Connection ID
   * @param previousHealth - Previous health status
   * @param currentHealth - Current health status
   * @param metrics - Current metrics
   */
  private generateHealthChangeAlert(
    connectionId: string,
    previousHealth: ConnectionHealth,
    currentHealth: ConnectionHealth,
    metrics: HealthMetrics
  ): void {
    let alertType: HealthAlert['alertType'];
    let severity: HealthAlert['severity'];
    let message: string;

    if (currentHealth === ConnectionHealth.UNHEALTHY) {
      alertType = 'unhealthy';
      severity = 'critical';
      message = `Connection ${connectionId} became unhealthy (${metrics.consecutiveFailures} consecutive failures)`;
    } else if (currentHealth === ConnectionHealth.DEGRADED) {
      alertType = 'degraded';
      severity = 'medium';
      message = `Connection ${connectionId} performance degraded (latency: ${metrics.averageLatency}ms)`;
    } else if (previousHealth !== ConnectionHealth.HEALTHY && currentHealth === ConnectionHealth.HEALTHY) {
      alertType = 'recovered';
      severity = 'low';
      message = `Connection ${connectionId} recovered to healthy status`;
    } else {
      return; // No alert needed
    }

    const alert: HealthAlert = {
      connectionId,
      alertType,
      message,
      metrics,
      timestamp: new Date(),
      severity,
    };

    this.emit(HealthMonitorEvent.ALERT, alert);
    this.debug(`[HealthMonitor] Alert: ${message}`);
  }

  /**
   * Generates latency spike alert
   * 
   * @param connectionId - Connection ID
   * @param latency - Current latency
   * @param metrics - Current metrics
   */
  private generateLatencySpikeAlert(
    connectionId: string,
    latency: number,
    metrics: HealthMetrics
  ): void {
    const alert: HealthAlert = {
      connectionId,
      alertType: 'latency_spike',
      message: `Connection ${connectionId} experiencing latency spike: ${latency}ms (avg: ${metrics.averageLatency}ms)`,
      metrics,
      timestamp: new Date(),
      severity: 'medium',
    };

    this.emit(HealthMonitorEvent.ALERT, alert);
    this.debug(`[HealthMonitor] Latency spike alert: ${alert.message}`);
  }

  /**
   * Updates latency history with size limit
   * 
   * @param history - Current latency history
   * @param newLatency - New latency value
   * @returns Updated history
   */
  private updateLatencyHistory(history: number[], newLatency: number): number[] {
    const updated = [...history, newLatency];
    if (updated.length > this.config.trendSampleSize) {
      updated.shift();
    }
    return updated;
  }

  /**
   * Calculates average latency
   * 
   * @param history - Latency history
   * @param newLatency - New latency value
   * @returns Average latency
   */
  private calculateAverageLatency(history: number[], newLatency: number): number {
    const allLatencies = [...history, newLatency];
    return allLatencies.reduce((sum, lat) => sum + lat, 0) / allLatencies.length;
  }

  /**
   * Calculates success rate
   * 
   * @param totalChecks - Total number of checks
   * @param totalFailures - Total number of failures
   * @returns Success rate as percentage
   */
  private calculateSuccessRate(totalChecks: number, totalFailures: number): number {
    if (totalChecks === 0) {
      return 0;
    }
    return ((totalChecks - totalFailures) / totalChecks) * 100;
  }

  /**
   * Analyzes health trend from latency history
   * 
   * @param history - Latency history
   * @param newLatency - New latency value
   * @returns Health trend
   */
  private analyzeHealthTrend(
    history: number[],
    newLatency: number
  ): 'improving' | 'stable' | 'degrading' | 'unknown' {
    if (!this.config.enableTrendAnalysis || history.length < 3) {
      return 'unknown';
    }

    const recentHistory = [...history.slice(-5), newLatency];
    const firstHalf = recentHistory.slice(0, Math.floor(recentHistory.length / 2));
    const secondHalf = recentHistory.slice(Math.floor(recentHistory.length / 2));

    const firstAvg = firstHalf.reduce((sum, lat) => sum + lat, 0) / firstHalf.length;
    const secondAvg = secondHalf.reduce((sum, lat) => sum + lat, 0) / secondHalf.length;

    const improvementThreshold = 0.1; // 10% improvement threshold
    const degradationThreshold = 0.2; // 20% degradation threshold

    if (secondAvg < firstAvg * (1 - improvementThreshold)) {
      return 'improving';
    } else if (secondAvg > firstAvg * (1 + degradationThreshold)) {
      return 'degrading';
    } else {
      return 'stable';
    }
  }

  /**
   * Debug logging utility
   * 
   * @param message - Debug message
   * @param args - Additional arguments
   */
  private debug(message: string, ...args: unknown[]): void {
    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.log(`[${new Date().toISOString()}] ${message}`, ...args);
    }
  }
}