/**
 * Goxel v14.0 Connection Pool Implementation
 * 
 * This module provides a robust connection pool for managing multiple daemon
 * connections with load balancing, health monitoring, and automatic recovery.
 * 
 * Key features:
 * - Dynamic connection scaling (min/max connections)
 * - Health monitoring with degraded connection handling
 * - Request queuing during connection issues
 * - Load balancing across healthy connections
 * - Connection lifecycle management
 * - Performance metrics and monitoring
 * - Automatic failover and recovery
 */

import { EventEmitter } from 'events';
import { v4 as uuidv4 } from 'uuid';
import { GoxelDaemonClient } from './daemon_client';
import {
  ConnectionPoolConfig,
  PooledConnection,
  PoolStatistics,
  QueuedRequest,
  HealthCheckResult,
  ConnectionHealth,
  PoolState,
  PoolEvent,
  PoolEventData,
  ConnectionEventData,
  HealthEventData,
  RequestEventData,
  DEFAULT_POOL_CONFIG,
  DaemonClientConfig,
  CallOptions,
  JsonRpcParams,
  ConnectionError,
  TimeoutError,
  DaemonClientError,
} from './types';

/**
 * Connection Pool Manager
 * 
 * Manages a pool of daemon connections with advanced features like health monitoring,
 * load balancing, and automatic recovery. Provides high-level interface for making
 * requests through the pool.
 * 
 * @example
 * ```typescript
 * const pool = new ConnectionPool({
 *   socketPath: '/tmp/goxel_daemon.sock',
 *   pool: {
 *     minConnections: 3,
 *     maxConnections: 15,
 *     healthCheckInterval: 10000,
 *   }
 * });
 * 
 * await pool.initialize();
 * const result = await pool.execute('get_version');
 * await pool.shutdown();
 * ```
 */
export class ConnectionPool extends EventEmitter {
  private readonly config: DaemonClientConfig;
  private readonly poolConfig: ConnectionPoolConfig;
  private readonly connections = new Map<string, PooledConnection>();
  private readonly requestQueue: QueuedRequest[] = [];
  private readonly busyConnections = new Set<string>();
  private readonly statistics: PoolStatistics;
  
  private poolState: PoolState = PoolState.INITIALIZING;
  private healthCheckTimer: NodeJS.Timeout | null = null;
  private requestIdCounter = 0;
  private startTime: Date | null = null;
  private shutdownPromise: Promise<void> | null = null;

  /**
   * Creates a new connection pool
   * 
   * @param clientConfig - Base client configuration
   * @param poolConfig - Pool-specific configuration
   */
  public constructor(
    clientConfig: DaemonClientConfig,
    poolConfig: Partial<ConnectionPoolConfig> = {}
  ) {
    super();
    
    this.config = { ...clientConfig };
    this.poolConfig = { ...DEFAULT_POOL_CONFIG, ...poolConfig };
    
    // Initialize statistics
    this.statistics = {
      totalConnections: 0,
      availableConnections: 0,
      busyConnections: 0,
      healthyConnections: 0,
      totalRequestsServed: 0,
      averageRequestTime: 0,
      poolUptime: 0,
      connectionFailures: 0,
      reconnectionAttempts: 0,
      lastHealthCheck: null,
    };

    // Set max listeners to prevent warnings
    this.setMaxListeners(100);
  }

  // ========================================================================
  // POOL LIFECYCLE MANAGEMENT
  // ========================================================================

  /**
   * Initializes the connection pool
   * 
   * @returns Promise that resolves when pool is ready
   */
  public async initialize(): Promise<void> {
    if (this.poolState !== PoolState.INITIALIZING) {
      throw new DaemonClientError(`Pool already initialized (state: ${this.poolState})`);
    }

    this.debug('[ConnectionPool] Initializing connection pool...');
    this.startTime = new Date();

    try {
      // Create minimum number of connections
      const connectionPromises: Promise<void>[] = [];
      for (let i = 0; i < this.poolConfig.minConnections; i++) {
        connectionPromises.push(this.createConnection());
      }

      await Promise.all(connectionPromises);

      // Start health monitoring
      this.startHealthMonitoring();

      // Set pool as ready
      this.setPoolState(PoolState.READY);
      
      this.debug(`[ConnectionPool] Pool initialized with ${this.connections.size} connections`);
      
      this.emit(PoolEvent.READY, {
        timestamp: new Date(),
        poolState: this.poolState,
      } as PoolEventData);

    } catch (error) {
      this.debug('[ConnectionPool] Failed to initialize pool:', error);
      await this.cleanup();
      throw new ConnectionError(`Failed to initialize connection pool: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Shuts down the connection pool gracefully
   * 
   * @returns Promise that resolves when shutdown is complete
   */
  public async shutdown(): Promise<void> {
    if (this.shutdownPromise) {
      return this.shutdownPromise;
    }

    this.shutdownPromise = this.performShutdown();
    return this.shutdownPromise;
  }

  /**
   * Checks if the pool is ready to serve requests
   */
  public isReady(): boolean {
    return this.poolState === PoolState.READY;
  }

  /**
   * Gets current pool state
   */
  public getPoolState(): PoolState {
    return this.poolState;
  }

  // ========================================================================
  // REQUEST EXECUTION
  // ========================================================================

  /**
   * Executes a method call through the connection pool
   * 
   * @param method - Method name to call
   * @param params - Method parameters (optional)
   * @param options - Call options (optional)
   * @returns Promise with method result
   */
  public async execute(
    method: string,
    params?: JsonRpcParams,
    options: CallOptions = {}
  ): Promise<unknown> {
    if (this.poolState === PoolState.SHUTDOWN) {
      throw new ConnectionError('Pool is shutdown');
    }

    if (this.poolState !== PoolState.READY && this.poolState !== PoolState.DEGRADED) {
      throw new ConnectionError(`Pool not ready (state: ${this.poolState})`);
    }

    const startTime = Date.now();

    try {
      // Try to get available connection
      const connection = await this.acquireConnection(options.timeout ?? this.config.timeout);
      
      try {
        // Execute request
        const result = await connection.client.call(method, params, options);
        
        // Update statistics
        this.updateRequestStatistics(Date.now() - startTime);
        
        return result;
        
      } finally {
        // Release connection back to pool
        this.releaseConnection(connection.id);
      }
      
    } catch (error) {
      this.debug(`[ConnectionPool] Request failed for method '${method}':`, error);
      throw error;
    }
  }

  /**
   * Executes multiple requests concurrently through the pool
   * 
   * @param requests - Array of request specifications
   * @returns Promise with array of results
   */
  public async executeBatch(
    requests: Array<{
      method: string;
      params?: JsonRpcParams;
      options?: CallOptions;
    }>
  ): Promise<unknown[]> {
    const promises = requests.map(req => 
      this.execute(req.method, req.params, req.options)
    );
    
    return Promise.all(promises);
  }

  // ========================================================================
  // CONNECTION MANAGEMENT
  // ========================================================================

  /**
   * Acquires an available connection from the pool
   * 
   * @param timeout - Timeout for acquiring connection
   * @returns Promise with pooled connection
   */
  private async acquireConnection(timeout: number): Promise<PooledConnection> {
    const acquireStartTime = Date.now();
    
    while (Date.now() - acquireStartTime < timeout) {
      // Try to find available healthy connection
      const availableConnection = this.findAvailableConnection();
      if (availableConnection) {
        this.busyConnections.add(availableConnection.id);
        this.updateConnectionUsage(availableConnection.id);
        return availableConnection;
      }

      // Check if we can create more connections
      if (this.connections.size < this.poolConfig.maxConnections) {
        try {
          await this.createConnection();
          continue; // Try again with new connection
        } catch (error) {
          this.debug('[ConnectionPool] Failed to create new connection:', error);
        }
      }

      // Queue request if no connections available
      if (this.requestQueue.length < 100) { // Prevent memory issues
        return this.queueRequest(timeout - (Date.now() - acquireStartTime));
      }

      // Wait a bit before retrying
      await this.sleep(50);
    }

    throw new TimeoutError(`Failed to acquire connection within ${timeout}ms`);
  }

  /**
   * Releases a connection back to the pool
   * 
   * @param connectionId - Connection ID to release
   */
  private releaseConnection(connectionId: string): void {
    this.busyConnections.delete(connectionId);
    
    // Process queued requests
    this.processRequestQueue();
  }

  /**
   * Finds an available healthy connection
   * 
   * @returns Available connection or null
   */
  private findAvailableConnection(): PooledConnection | null {
    const healthyConnections = Array.from(this.connections.values())
      .filter(conn => 
        conn.health === ConnectionHealth.HEALTHY &&
        conn.isAvailable &&
        !this.busyConnections.has(conn.id)
      );

    if (healthyConnections.length === 0) {
      return null;
    }

    // Load balancing: choose connection with least requests
    if (this.poolConfig.loadBalancing) {
      return healthyConnections.reduce((least, current) => 
        current.requestCount < least.requestCount ? current : least
      ) || null;
    }

    // Round-robin: return first available
    return healthyConnections[0] || null;
  }

  /**
   * Creates a new connection and adds it to the pool
   * 
   * @returns Promise that resolves when connection is ready
   */
  private async createConnection(): Promise<void> {
    const connectionId = uuidv4();
    const client = new GoxelDaemonClient(this.config);
    
    this.debug(`[ConnectionPool] Creating connection ${connectionId}`);

    try {
      await client.connect();
      
      const connection: PooledConnection = {
        id: connectionId,
        client,
        createdAt: new Date(),
        health: ConnectionHealth.HEALTHY,
        requestCount: 0,
        lastUsed: new Date(),
        isAvailable: true,
      };

      this.connections.set(connectionId, connection);
      this.updatePoolStatistics();
      
      this.debug(`[ConnectionPool] Connection ${connectionId} created successfully`);
      
      // Setup connection event handlers
      this.setupConnectionEventHandlers(connection);
      
      this.emit(PoolEvent.CONNECTION_ADDED, {
        connection,
        timestamp: new Date(),
        poolState: this.poolState,
      } as ConnectionEventData);
      
    } catch (error) {
      this.statistics.connectionFailures++;
      this.debug(`[ConnectionPool] Failed to create connection ${connectionId}:`, error);
      throw error;
    }
  }

  /**
   * Removes a connection from the pool
   * 
   * @param connectionId - Connection ID to remove
   */
  private async removeConnection(connectionId: string): Promise<void> {
    const connection = this.connections.get(connectionId);
    if (!connection) {
      return;
    }

    this.debug(`[ConnectionPool] Removing connection ${connectionId}`);
    
    try {
      await connection.client.disconnect();
    } catch (error) {
      this.debug(`[ConnectionPool] Error disconnecting ${connectionId}:`, error);
    }

    this.connections.delete(connectionId);
    this.busyConnections.delete(connectionId);
    this.updatePoolStatistics();
    
    this.emit(PoolEvent.CONNECTION_REMOVED, {
      connection,
      timestamp: new Date(),
      poolState: this.poolState,
    } as ConnectionEventData);
    
    // Create replacement connection if needed
    if (this.connections.size < this.poolConfig.minConnections && 
        this.poolState !== PoolState.SHUTDOWN) {
      try {
        await this.createConnection();
      } catch (error) {
        this.debug('[ConnectionPool] Failed to create replacement connection:', error);
      }
    }
  }

  // ========================================================================
  // HEALTH MONITORING
  // ========================================================================

  /**
   * Starts health monitoring for all connections
   */
  private startHealthMonitoring(): void {
    if (this.healthCheckTimer) {
      return;
    }

    this.healthCheckTimer = setInterval(() => {
      this.performHealthCheck().catch(error => {
        this.debug('[ConnectionPool] Health check failed:', error);
      });
    }, this.poolConfig.healthCheckInterval);
  }

  /**
   * Performs health check on all connections
   */
  private async performHealthCheck(): Promise<void> {
    if (this.poolState === PoolState.SHUTDOWN) {
      return;
    }

    this.debug('[ConnectionPool] Performing health check...');
    
    const healthPromises = Array.from(this.connections.values())
      .map(connection => this.checkConnectionHealth(connection));

    const results = await Promise.allSettled(healthPromises);
    
    let healthyCount = 0;
    let degradedCount = 0;
    let unhealthyCount = 0;

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        switch (result.value.health) {
          case ConnectionHealth.HEALTHY:
            healthyCount++;
            break;
          case ConnectionHealth.DEGRADED:
            degradedCount++;
            break;
          case ConnectionHealth.UNHEALTHY:
            unhealthyCount++;
            break;
        }
      } else {
        unhealthyCount++;
      }
    });

    // Update pool state based on health results
    const totalConnections = this.connections.size;
    const healthyRatio = healthyCount / totalConnections;
    
    if (healthyRatio >= 0.8) {
      this.setPoolState(PoolState.READY);
    } else if (healthyRatio >= 0.5) {
      this.setPoolState(PoolState.DEGRADED);
    }

    this.statistics.lastHealthCheck = new Date();
    this.updatePoolStatistics();
    
    this.debug(`[ConnectionPool] Health check complete: ${healthyCount} healthy, ${degradedCount} degraded, ${unhealthyCount} unhealthy`);
  }

  /**
   * Checks health of a specific connection
   * 
   * @param connection - Connection to check
   * @returns Health check result
   */
  private async checkConnectionHealth(connection: PooledConnection): Promise<HealthCheckResult> {
    const startTime = Date.now();
    
    try {
      // Simple ping test
      await connection.client.call('get_status', undefined, { timeout: 2000 });
      
      const latency = Date.now() - startTime;
      const health = latency < 1000 ? ConnectionHealth.HEALTHY : ConnectionHealth.DEGRADED;
      
      // Update connection health
      this.updateConnectionHealth(connection.id, health);
      
      const result: HealthCheckResult = {
        connectionId: connection.id,
        health,
        latency,
        timestamp: new Date(),
      };
      
      this.emit(PoolEvent.CONNECTION_HEALTH_CHANGED, {
        healthCheck: result,
        timestamp: new Date(),
        poolState: this.poolState,
      } as HealthEventData);
      
      return result;
      
    } catch (error) {
      const health = ConnectionHealth.UNHEALTHY;
      this.updateConnectionHealth(connection.id, health);
      
      const result: HealthCheckResult = {
        connectionId: connection.id,
        health,
        latency: Date.now() - startTime,
        error: error instanceof Error ? error : new Error(String(error)),
        timestamp: new Date(),
      };
      
      // Remove unhealthy connections
      if (this.connections.size > this.poolConfig.minConnections) {
        this.removeConnection(connection.id).catch(err => {
          this.debug(`[ConnectionPool] Failed to remove unhealthy connection ${connection.id}:`, err);
        });
      }
      
      return result;
    }
  }

  // ========================================================================
  // REQUEST QUEUING
  // ========================================================================

  /**
   * Queues a request when no connections are available
   * 
   * @param remainingTimeout - Remaining timeout for request
   * @returns Promise with pooled connection
   */
  private async queueRequest(remainingTimeout: number): Promise<PooledConnection> {
    return new Promise((resolve, reject) => {
      const requestId = `queued_${++this.requestIdCounter}`;
      
      const queuedRequest: QueuedRequest = {
        id: requestId,
        method: '__connection_request__',
        timestamp: new Date(),
        resolve: resolve as (result: unknown) => void,
        reject,
        options: {},
      };

      // Set timeout for queued request
      if (remainingTimeout > 0) {
        queuedRequest.timeoutHandle = setTimeout(() => {
          this.removeFromQueue(requestId);
          reject(new TimeoutError(`Queued request timeout after ${remainingTimeout}ms`));
        }, remainingTimeout);
      }

      this.requestQueue.push(queuedRequest);
      
      this.emit(PoolEvent.REQUEST_QUEUED, {
        request: queuedRequest,
        timestamp: new Date(),
        poolState: this.poolState,
      } as RequestEventData);
      
      this.debug(`[ConnectionPool] Request ${requestId} queued (queue size: ${this.requestQueue.length})`);
    });
  }

  /**
   * Processes the request queue when connections become available
   */
  private processRequestQueue(): void {
    while (this.requestQueue.length > 0) {
      const availableConnection = this.findAvailableConnection();
      if (!availableConnection) {
        break;
      }

      const queuedRequest = this.requestQueue.shift()!;
      
      // Clear timeout
      if (queuedRequest.timeoutHandle) {
        clearTimeout(queuedRequest.timeoutHandle);
      }

      this.busyConnections.add(availableConnection.id);
      this.updateConnectionUsage(availableConnection.id);
      
      queuedRequest.resolve(availableConnection);
      
      this.emit(PoolEvent.REQUEST_SERVED, {
        request: queuedRequest,
        timestamp: new Date(),
        poolState: this.poolState,
      } as RequestEventData);
    }
  }

  /**
   * Removes a request from the queue
   * 
   * @param requestId - Request ID to remove
   */
  private removeFromQueue(requestId: string): void {
    const index = this.requestQueue.findIndex(req => req.id === requestId);
    if (index !== -1) {
      this.requestQueue.splice(index, 1);
    }
  }

  // ========================================================================
  // STATISTICS AND MONITORING
  // ========================================================================

  /**
   * Gets current pool statistics
   */
  public getStatistics(): PoolStatistics {
    const uptime = this.startTime ? Date.now() - this.startTime.getTime() : 0;
    
    return {
      ...this.statistics,
      poolUptime: uptime,
    };
  }

  /**
   * Gets detailed information about all connections
   */
  public getConnections(): PooledConnection[] {
    return Array.from(this.connections.values());
  }

  /**
   * Gets current request queue size
   */
  public getQueueSize(): number {
    return this.requestQueue.length;
  }

  // ========================================================================
  // PRIVATE UTILITY METHODS
  // ========================================================================

  /**
   * Sets up event handlers for a connection
   * 
   * @param connection - Connection to setup handlers for
   */
  private setupConnectionEventHandlers(connection: PooledConnection): void {
    connection.client.on('error', (error: Error) => {
      this.debug(`[ConnectionPool] Connection ${connection.id} error:`, error);
      this.updateConnectionHealth(connection.id, ConnectionHealth.UNHEALTHY);
    });

    connection.client.on('disconnected', () => {
      this.debug(`[ConnectionPool] Connection ${connection.id} disconnected`);
      this.updateConnectionHealth(connection.id, ConnectionHealth.UNHEALTHY);
    });
  }

  /**
   * Updates connection health status
   * 
   * @param connectionId - Connection ID
   * @param health - New health status
   */
  private updateConnectionHealth(connectionId: string, health: ConnectionHealth): void {
    const connection = this.connections.get(connectionId);
    if (connection) {
      (connection as any).health = health;
      this.updatePoolStatistics();
    }
  }

  /**
   * Updates connection usage statistics
   * 
   * @param connectionId - Connection ID
   */
  private updateConnectionUsage(connectionId: string): void {
    const connection = this.connections.get(connectionId);
    if (connection) {
      (connection as any).requestCount++;
      (connection as any).lastUsed = new Date();
      
      // Check if connection needs rotation
      if (connection.requestCount >= this.poolConfig.maxRequestsPerConnection) {
        this.debug(`[ConnectionPool] Connection ${connectionId} reached max requests, rotating`);
        this.removeConnection(connectionId).catch(error => {
          this.debug(`[ConnectionPool] Failed to rotate connection ${connectionId}:`, error);
        });
      }
    }
  }

  /**
   * Updates pool statistics
   */
  private updatePoolStatistics(): void {
    const connections = Array.from(this.connections.values());
    
    this.statistics.totalConnections = connections.length;
    this.statistics.availableConnections = connections.filter(c => 
      c.isAvailable && !this.busyConnections.has(c.id)
    ).length;
    this.statistics.busyConnections = this.busyConnections.size;
    this.statistics.healthyConnections = connections.filter(c => 
      c.health === ConnectionHealth.HEALTHY
    ).length;
  }

  /**
   * Updates request statistics
   * 
   * @param responseTime - Request response time in ms
   */
  private updateRequestStatistics(responseTime: number): void {
    this.statistics.totalRequestsServed++;
    
    // Update average response time using incremental formula
    const totalRequests = this.statistics.totalRequestsServed;
    this.statistics.averageRequestTime = 
      (this.statistics.averageRequestTime * (totalRequests - 1) + responseTime) / totalRequests;
  }

  /**
   * Sets pool state and emits events
   * 
   * @param state - New pool state
   */
  private setPoolState(state: PoolState): void {
    if (this.poolState === state) {
      return;
    }

    const previousState = this.poolState;
    this.poolState = state;
    
    this.debug(`[ConnectionPool] State: ${previousState} -> ${state}`);
    
    if (state === PoolState.DEGRADED && previousState !== PoolState.DEGRADED) {
      this.emit(PoolEvent.DEGRADED, {
        timestamp: new Date(),
        poolState: state,
      } as PoolEventData);
    }
  }

  /**
   * Performs shutdown cleanup
   */
  private async performShutdown(): Promise<void> {
    this.debug('[ConnectionPool] Shutting down pool...');
    this.setPoolState(PoolState.SHUTDOWN);
    
    // Stop health monitoring
    if (this.healthCheckTimer) {
      clearInterval(this.healthCheckTimer);
      this.healthCheckTimer = null;
    }

    // Reject all queued requests
    while (this.requestQueue.length > 0) {
      const request = this.requestQueue.shift()!;
      if (request.timeoutHandle) {
        clearTimeout(request.timeoutHandle);
      }
      request.reject(new ConnectionError('Pool shutdown'));
    }

    // Close all connections
    await this.cleanup();
    
    this.debug('[ConnectionPool] Pool shutdown complete');
  }

  /**
   * Cleans up all connections
   */
  private async cleanup(): Promise<void> {
    const disconnectPromises = Array.from(this.connections.values())
      .map(connection => connection.client.disconnect().catch((error: Error) => {
        this.debug(`[ConnectionPool] Error disconnecting ${connection.id}:`, error);
      }));

    await Promise.all(disconnectPromises);
    
    this.connections.clear();
    this.busyConnections.clear();
  }

  /**
   * Sleep utility function
   * 
   * @param ms - Milliseconds to sleep
   */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Debug logging utility
   * 
   * @param message - Debug message
   * @param args - Additional arguments
   */
  private debug(message: string, ...args: unknown[]): void {
    if (this.poolConfig.debug) {
      // eslint-disable-next-line no-console
      console.log(`[${new Date().toISOString()}] ${message}`, ...args);
    }
  }
}