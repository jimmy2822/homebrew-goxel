/**
 * Goxel v14.0 Daemon Client Implementation
 * 
 * This module provides a comprehensive TypeScript client for communicating
 * with the Goxel daemon server using JSON-RPC 2.0 over Unix sockets.
 * 
 * Key features:
 * - Full JSON-RPC 2.0 compliance
 * - Unix socket communication
 * - Automatic reconnection handling
 * - Request timeout and retry logic
 * - Type-safe method calls
 * - Event-driven architecture
 * - Comprehensive error handling
 */

import * as net from 'net';
import { EventEmitter } from 'events';
import {
  PooledDaemonClientConfig,
  PartialPooledDaemonClientConfig,
  DEFAULT_POOLED_CLIENT_CONFIG,
  JsonRpcRequest,
  JsonRpcResponse,
  JsonRpcNotification,
  JsonRpcId,
  JsonRpcParams,
  PendingRequest,
  CallOptions,
  ClientConnectionState,
  ClientEvent,
  ClientStatistics,
  ConnectionEventData,
  ErrorEventData,
  MessageEventData,
  DaemonClientError,
  ConnectionError,
  TimeoutError,
  JsonRpcClientError,
  isJsonRpcSuccessResponse,
  isJsonRpcErrorResponse,
} from './types';
import { ConnectionPool } from './connection_pool';
import { HealthMonitor } from './health_monitor';

/**
 * Main Goxel daemon client class
 * 
 * Provides a high-level interface for communicating with the Goxel daemon
 * server using JSON-RPC 2.0 protocol over Unix sockets. Supports both
 * single connection and connection pooling modes for high-performance scenarios.
 * 
 * @example
 * ```typescript
 * // Single connection mode
 * const client = new GoxelDaemonClient({
 *   socketPath: '/tmp/goxel_daemon.sock',
 *   timeout: 5000,
 * });
 * 
 * await client.connect();
 * const version = await client.call('get_version');
 * await client.disconnect();
 * 
 * // Connection pool mode
 * const pooledClient = new GoxelDaemonClient({
 *   socketPath: '/tmp/goxel_daemon.sock',
 *   enablePooling: true,
 *   pool: {
 *     minConnections: 3,
 *     maxConnections: 10,
 *   }
 * });
 * 
 * await pooledClient.connect();
 * const result = await pooledClient.call('get_version');
 * await pooledClient.disconnect();
 * ```
 */
export class GoxelDaemonClient extends EventEmitter {
  private readonly config: PooledDaemonClientConfig;
  private socket: net.Socket | null = null;
  private connectionState: ClientConnectionState = ClientConnectionState.DISCONNECTED;
  private requestId: number = 0;
  private readonly pendingRequests = new Map<JsonRpcId, PendingRequest>();
  private receiveBuffer = '';
  private readonly statistics: ClientStatistics;
  private reconnectTimeout: NodeJS.Timeout | null = null;
  private connectionStartTime: Date | null = null;
  
  // Connection pooling support
  private connectionPool: ConnectionPool | null = null;
  private healthMonitor: HealthMonitor | null = null;

  /**
   * Creates a new daemon client instance
   * 
   * @param config - Client configuration options (supports pooling)
   */
  public constructor(config: PartialPooledDaemonClientConfig) {
    super();
    
    // Merge with defaults
    this.config = { ...DEFAULT_POOLED_CLIENT_CONFIG, ...config };
    
    // Initialize statistics
    this.statistics = {
      connectionAttempts: 0,
      successfulConnections: 0,
      failedConnections: 0,
      requestsSent: 0,
      responsesReceived: 0,
      errorsReceived: 0,
      bytesTransmitted: 0,
      bytesReceived: 0,
      averageResponseTime: 0,
      uptime: 0,
    };

    // Set max listeners to prevent warnings
    this.setMaxListeners(100);
  }

  // ========================================================================
  // CONNECTION MANAGEMENT
  // ========================================================================

  /**
   * Establishes connection to the daemon server
   * 
   * In pooling mode, initializes the connection pool. In single connection mode,
   * establishes a direct socket connection.
   * 
   * @returns Promise that resolves when connected
   * @throws {ConnectionError} If connection fails
   */
  public async connect(): Promise<void> {
    if (this.connectionState === ClientConnectionState.CONNECTED) {
      return;
    }

    if (this.connectionState === ClientConnectionState.CONNECTING) {
      throw new ConnectionError('Connection already in progress');
    }

    this.setConnectionState(ClientConnectionState.CONNECTING);
    this.statistics.connectionAttempts++;
    this.connectionStartTime = new Date();

    // Use connection pooling if enabled
    if (this.config.enablePooling) {
      return this.connectWithPool();
    }

    // Use single connection mode
    return this.connectSingle();
  }

  /**
   * Disconnects from the daemon server
   * 
   * In pooling mode, shuts down the connection pool. In single connection mode,
   * closes the direct socket connection.
   * 
   * @returns Promise that resolves when disconnected
   */
  public async disconnect(): Promise<void> {
    if (this.connectionState === ClientConnectionState.DISCONNECTED) {
      return;
    }

    // Clear reconnect timeout
    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    // Disconnect pooling components
    if (this.config.enablePooling) {
      return this.disconnectFromPool();
    }

    // Disconnect single connection
    return this.disconnectSingle();
  }

  /**
   * Checks if client is currently connected
   */
  public isConnected(): boolean {
    return this.connectionState === ClientConnectionState.CONNECTED;
  }

  /**
   * Gets current connection state
   */
  public getConnectionState(): ClientConnectionState {
    return this.connectionState;
  }

  // ========================================================================
  // METHOD CALLS
  // ========================================================================

  /**
   * Makes a JSON-RPC method call to the daemon
   * 
   * In pooling mode, uses the connection pool for load balancing and failover.
   * In single connection mode, uses the direct socket connection.
   * 
   * @param method - Method name to call
   * @param params - Method parameters (optional)
   * @param options - Call options (optional)
   * @returns Promise with method result
   * @throws {ConnectionError} If not connected
   * @throws {TimeoutError} If request times out
   * @throws {JsonRpcClientError} If server returns error
   */
  public async call(
    method: string,
    params?: JsonRpcParams,
    options: CallOptions = {}
  ): Promise<unknown> {
    if (!this.isConnected()) {
      throw new ConnectionError('Not connected to daemon');
    }

    // Use connection pool if enabled
    if (this.config.enablePooling && this.connectionPool) {
      return this.connectionPool.execute(method, params, options);
    }

    // Use single connection mode
    return this.callSingle(method, params, options);
  }

  /**
   * Sends a notification (request without expecting response)
   * 
   * @param method - Method name
   * @param params - Method parameters (optional)
   */
  public async sendNotification(method: string, params?: JsonRpcParams): Promise<void> {
    if (!this.isConnected()) {
      throw new ConnectionError('Not connected to daemon');
    }

    const notification: JsonRpcNotification = {
      jsonrpc: '2.0',
      method,
      params,
    };

    await this.sendMessage(notification);
  }

  // ========================================================================
  // STATISTICS AND MONITORING
  // ========================================================================

  /**
   * Gets client statistics
   * 
   * In pooling mode, includes aggregated pool statistics.
   */
  public getStatistics(): ClientStatistics {
    const uptime = this.connectionStartTime 
      ? Date.now() - this.connectionStartTime.getTime()
      : 0;

    let stats = {
      ...this.statistics,
      uptime,
    };

    // Include pool statistics if pooling is enabled
    if (this.config.enablePooling && this.connectionPool) {
      const poolStats = this.connectionPool.getStatistics();
      stats = {
        ...stats,
        // Merge pool statistics
        requestsSent: stats.requestsSent + poolStats.totalRequestsServed,
        averageResponseTime: poolStats.averageRequestTime || stats.averageResponseTime,
      };
    }

    return stats;
  }

  /**
   * Resets statistics counters
   */
  public resetStatistics(): void {
    Object.assign(this.statistics, {
      connectionAttempts: 0,
      successfulConnections: 0,
      failedConnections: 0,
      requestsSent: 0,
      responsesReceived: 0,
      errorsReceived: 0,
      bytesTransmitted: 0,
      bytesReceived: 0,
      averageResponseTime: 0,
      uptime: 0,
      lastConnected: undefined,
      lastDisconnected: undefined,
      lastError: undefined,
    });
  }

  /**
   * Gets client configuration
   */
  public getConfig(): Readonly<PooledDaemonClientConfig> {
    return { ...this.config };
  }

  /**
   * Gets connection pool instance (if pooling is enabled)
   */
  public getConnectionPool(): ConnectionPool | null {
    return this.connectionPool;
  }

  /**
   * Gets health monitor instance (if pooling is enabled)
   */
  public getHealthMonitor(): HealthMonitor | null {
    return this.healthMonitor;
  }

  /**
   * Executes multiple requests concurrently (uses pool if available)
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
    if (this.config.enablePooling && this.connectionPool) {
      return this.connectionPool.executeBatch(requests);
    }

    // Fallback to sequential execution for single connection mode
    const results: unknown[] = [];
    for (const request of requests) {
      const result = await this.call(request.method, request.params, request.options);
      results.push(result);
    }
    return results;
  }

  // ========================================================================
  // CONNECTION POOLING METHODS
  // ========================================================================

  /**
   * Connects using connection pool
   */
  private async connectWithPool(): Promise<void> {
    try {
      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.log('[GoxelDaemonClient] Initializing connection pool...');
      }

      // Create connection pool
      this.connectionPool = new ConnectionPool(
        {
          socketPath: this.config.socketPath,
          timeout: this.config.timeout,
          retryAttempts: this.config.retryAttempts,
          retryDelay: this.config.retryDelay,
          maxMessageSize: this.config.maxMessageSize,
          debug: this.config.debug,
          autoReconnect: this.config.autoReconnect,
          reconnectInterval: this.config.reconnectInterval,
        },
        this.config.pool
      );

      // Create health monitor
      this.healthMonitor = new HealthMonitor({
        interval: this.config.pool?.healthCheckInterval || 30000,
        debug: this.config.debug,
      });

      // Setup pool event handlers
      this.setupPoolEventHandlers();

      // Initialize pool
      await this.connectionPool.initialize();

      this.handleConnectionSuccess();

      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.log('[GoxelDaemonClient] Connection pool initialized successfully');
      }

    } catch (error) {
      const connectionError = new ConnectionError(
        `Failed to initialize connection pool: ${error instanceof Error ? error.message : String(error)}`
      );
      this.handleConnectionError(connectionError);
      throw connectionError;
    }
  }

  /**
   * Disconnects from connection pool
   */
  private async disconnectFromPool(): Promise<void> {
    try {
      // Stop health monitoring
      if (this.healthMonitor) {
        this.healthMonitor.stopMonitoring();
        this.healthMonitor = null;
      }

      // Shutdown connection pool
      if (this.connectionPool) {
        await this.connectionPool.shutdown();
        this.connectionPool = null;
      }

      this.setConnectionState(ClientConnectionState.DISCONNECTED);

      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.log('[GoxelDaemonClient] Connection pool disconnected');
      }

    } catch (error) {
      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.error('[GoxelDaemonClient] Error during pool disconnect:', error);
      }
      throw error;
    }
  }

  /**
   * Sets up connection pool event handlers
   */
  private setupPoolEventHandlers(): void {
    if (!this.connectionPool || !this.healthMonitor) {
      return;
    }

    // Pool events
    this.connectionPool.on('ready', () => {
      this.emit(ClientEvent.CONNECTED, {
        timestamp: new Date(),
        state: this.connectionState,
      } as ConnectionEventData);
    });

    this.connectionPool.on('error', (error: Error) => {
      this.emit(ClientEvent.ERROR, {
        error,
        timestamp: new Date(),
        context: 'connection_pool',
      } as ErrorEventData);
    });

    // Health monitor events
    this.healthMonitor.on('alert', (alert: any) => {
      this.emit(ClientEvent.ERROR, {
        error: new DaemonClientError(`Health alert: ${alert.message}`),
        timestamp: new Date(),
        context: 'health_monitor',
      } as ErrorEventData);
    });
  }

  // ========================================================================
  // SINGLE CONNECTION METHODS
  // ========================================================================

  /**
   * Connects using single connection mode
   */
  private async connectSingle(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        this.socket = new net.Socket();
        
        // Set socket options
        this.socket.setKeepAlive(true);
        this.socket.setNoDelay(true);
        
        // Connection timeout
        const connectionTimeout = setTimeout(() => {
          this.socket?.destroy();
          this.handleConnectionError(new ConnectionError('Connection timeout'));
          reject(new ConnectionError('Connection timeout'));
        }, this.config.timeout);

        // Connection established
        this.socket.once('connect', () => {
          clearTimeout(connectionTimeout);
          this.handleConnectionSuccess();
          resolve();
        });

        // Connection error
        this.socket.once('error', (error: Error) => {
          clearTimeout(connectionTimeout);
          const connectionError = new ConnectionError(
            `Failed to connect: ${error.message}`,
            'socket_error'
          );
          this.handleConnectionError(connectionError);
          reject(connectionError);
        });

        // Setup event handlers
        this.setupSocketEventHandlers();

        // Attempt connection
        this.socket.connect({ path: this.config.socketPath });

        if (this.config.debug) {
          // eslint-disable-next-line no-console
          console.log(`[GoxelDaemonClient] Connecting to ${this.config.socketPath}`);
        }

      } catch (error) {
        const connectionError = new ConnectionError(
          `Connection setup failed: ${error instanceof Error ? error.message : String(error)}`
        );
        this.handleConnectionError(connectionError);
        reject(connectionError);
      }
    });
  }

  /**
   * Disconnects single connection
   */
  private async disconnectSingle(): Promise<void> {
    // Reject all pending requests
    this.rejectAllPendingRequests(new ConnectionError('Client disconnected'));

    // Close socket
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }

    this.setConnectionState(ClientConnectionState.DISCONNECTED);

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.log('[GoxelDaemonClient] Disconnected');
    }
  }

  /**
   * Makes a call using single connection mode
   */
  private async callSingle(
    method: string,
    params?: JsonRpcParams,
    options: CallOptions = {}
  ): Promise<unknown> {
    // Handle notifications (no response expected)
    if (options.notification) {
      await this.sendNotification(method, params);
      return;
    }

    const requestId = options.id ?? this.generateRequestId();
    const timeout = options.timeout ?? this.config.timeout;
    const request: JsonRpcRequest = {
      jsonrpc: '2.0',
      method,
      params,
      id: requestId,
    };

    return this.sendRequestWithRetry(request, timeout, options.retries ?? this.config.retryAttempts);
  }

  // ========================================================================
  // PRIVATE METHODS
  // ========================================================================

  /**
   * Sets up socket event handlers
   */
  private setupSocketEventHandlers(): void {
    if (!this.socket) {
      return;
    }

    // Data received
    this.socket.on('data', (data: Buffer) => {
      this.handleReceivedData(data);
    });

    // Socket closed
    this.socket.on('close', (hadError: boolean) => {
      this.handleSocketClosed(hadError);
    });

    // Socket error
    this.socket.on('error', (error: Error) => {
      this.handleSocketError(error);
    });
  }

  /**
   * Handles successful connection
   */
  private handleConnectionSuccess(): void {
    this.statistics.successfulConnections++;
    this.statistics.lastConnected = new Date();
    this.setConnectionState(ClientConnectionState.CONNECTED);

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.log('[GoxelDaemonClient] Connected successfully');
    }

    this.emit(ClientEvent.CONNECTED, {
      timestamp: new Date(),
      state: this.connectionState,
    } as ConnectionEventData);
  }

  /**
   * Handles connection error
   */
  private handleConnectionError(error: Error): void {
    this.statistics.failedConnections++;
    this.statistics.lastError = new Date();
    this.setConnectionState(ClientConnectionState.ERROR);

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.error('[GoxelDaemonClient] Connection error:', error.message);
    }

    this.emit(ClientEvent.ERROR, {
      error,
      timestamp: new Date(),
      context: 'connection',
    } as ErrorEventData);

    // Auto-reconnect if enabled
    if (this.config.autoReconnect && this.connectionState !== ClientConnectionState.DISCONNECTED) {
      this.scheduleReconnect();
    }
  }

  /**
   * Handles socket closure
   */
  private handleSocketClosed(hadError: boolean): void {
    const wasConnected = this.connectionState === ClientConnectionState.CONNECTED;
    
    this.statistics.lastDisconnected = new Date();
    this.setConnectionState(ClientConnectionState.DISCONNECTED);

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.log(`[GoxelDaemonClient] Socket closed (hadError: ${hadError})`);
    }

    // Reject pending requests
    this.rejectAllPendingRequests(new ConnectionError('Connection closed'));

    this.emit(ClientEvent.DISCONNECTED, {
      timestamp: new Date(),
      state: this.connectionState,
    } as ConnectionEventData);

    // Auto-reconnect if we were previously connected and auto-reconnect is enabled
    if (wasConnected && this.config.autoReconnect) {
      this.scheduleReconnect();
    }
  }

  /**
   * Handles socket error
   */
  private handleSocketError(error: Error): void {
    this.statistics.lastError = new Date();

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.error('[GoxelDaemonClient] Socket error:', error.message);
    }

    this.emit(ClientEvent.ERROR, {
      error,
      timestamp: new Date(),
      context: 'socket',
    } as ErrorEventData);
  }

  /**
   * Schedules reconnection attempt
   */
  private scheduleReconnect(): void {
    if (this.reconnectTimeout) {
      return; // Already scheduled
    }

    this.setConnectionState(ClientConnectionState.RECONNECTING);

    this.reconnectTimeout = setTimeout(() => {
      this.reconnectTimeout = null;
      
      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.log('[GoxelDaemonClient] Attempting to reconnect...');
      }

      this.emit(ClientEvent.RECONNECTING, {
        timestamp: new Date(),
        state: this.connectionState,
      } as ConnectionEventData);

      this.connect().catch((error: Error) => {
        if (this.config.debug) {
          // eslint-disable-next-line no-console
          console.error('[GoxelDaemonClient] Reconnection failed:', error.message);
        }
        
        // Schedule another reconnect attempt
        if (this.config.autoReconnect) {
          this.scheduleReconnect();
        }
      });
    }, this.config.reconnectInterval);
  }

  /**
   * Handles received data from socket
   */
  private handleReceivedData(data: Buffer): void {
    this.statistics.bytesReceived += data.length;
    this.receiveBuffer += data.toString('utf8');

    // Process complete messages (newline-delimited JSON)
    let newlineIndex: number;
    while ((newlineIndex = this.receiveBuffer.indexOf('\n')) !== -1) {
      const messageStr = this.receiveBuffer.slice(0, newlineIndex).trim();
      this.receiveBuffer = this.receiveBuffer.slice(newlineIndex + 1);

      if (messageStr.length > 0) {
        this.processReceivedMessage(messageStr);
      }
    }
  }

  /**
   * Processes a received JSON message
   */
  private processReceivedMessage(messageStr: string): void {
    try {
      // Clean up the message - daemon might send binary data before JSON
      const jsonStart = messageStr.indexOf('{');
      const jsonEnd = messageStr.lastIndexOf('}') + 1;
      
      let cleanedMessage = messageStr;
      if (jsonStart !== -1 && jsonEnd > jsonStart && jsonStart > 0) {
        cleanedMessage = messageStr.substring(jsonStart, jsonEnd);
        
        if (this.config.debug) {
          // eslint-disable-next-line no-console
          console.log('[GoxelDaemonClient] Cleaned message:', cleanedMessage);
        }
      }
      
      const message = JSON.parse(cleanedMessage) as JsonRpcResponse | JsonRpcNotification;
      this.statistics.responsesReceived++;

      // Emit message event
      this.emit(ClientEvent.MESSAGE, {
        message,
        timestamp: new Date(),
      } as MessageEventData);

      // Handle response to pending request
      if ('id' in message && message.id !== null) {
        this.handleResponse(message as JsonRpcResponse);
      }

    } catch (error) {
      this.statistics.errorsReceived++;
      
      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.error('[GoxelDaemonClient] Failed to parse message:', messageStr, error);
      }

      this.emit(ClientEvent.ERROR, {
        error: new DaemonClientError(`Invalid JSON message: ${messageStr}`),
        timestamp: new Date(),
        context: 'message_parsing',
      } as ErrorEventData);
    }
  }

  /**
   * Handles JSON-RPC response
   */
  private handleResponse(response: JsonRpcResponse): void {
    const pendingRequest = this.pendingRequests.get(response.id);
    if (!pendingRequest) {
      if (this.config.debug) {
        // eslint-disable-next-line no-console
        console.warn('[GoxelDaemonClient] Received response for unknown request ID:', response.id);
      }
      return;
    }

    // Clear timeout
    if (pendingRequest.timeoutHandle) {
      clearTimeout(pendingRequest.timeoutHandle);
    }

    // Remove from pending requests
    this.pendingRequests.delete(response.id);

    if (isJsonRpcSuccessResponse(response)) {
      pendingRequest.resolve(response.result);
    } else if (isJsonRpcErrorResponse(response)) {
      const error = new JsonRpcClientError(
        response.error.message,
        response.error.code,
        `method: ${pendingRequest.method}`
      );
      pendingRequest.reject(error);
    }
  }

  /**
   * Sends a request with retry logic
   */
  private async sendRequestWithRetry(
    request: JsonRpcRequest,
    timeout: number,
    maxRetries: number,
    currentAttempt = 0
  ): Promise<unknown> {
    try {
      return await this.sendRequest(request, timeout);
    } catch (error) {
      if (currentAttempt < maxRetries && error instanceof TimeoutError) {
        if (this.config.debug) {
          // eslint-disable-next-line no-console
          console.warn(`[GoxelDaemonClient] Request timeout, retrying (${currentAttempt + 1}/${maxRetries})`);
        }

        // Wait before retry
        await new Promise(resolve => setTimeout(resolve, this.config.retryDelay));
        
        return this.sendRequestWithRetry(request, timeout, maxRetries, currentAttempt + 1);
      }
      throw error;
    }
  }

  /**
   * Sends a JSON-RPC request and waits for response
   */
  private async sendRequest(request: JsonRpcRequest, timeout: number): Promise<unknown> {
    const requestId = request.id!;
    const startTime = Date.now();

    return new Promise((resolve, reject) => {
      // Create pending request
      const pendingRequest: PendingRequest = {
        id: requestId,
        method: request.method,
        timestamp: new Date(),
        timeout,
        resolve: (result: unknown) => {
          // Update response time statistics
          const responseTime = Date.now() - startTime;
          this.updateAverageResponseTime(responseTime);
          resolve(result);
        },
        reject,
      };

      // Set timeout
      pendingRequest.timeoutHandle = setTimeout(() => {
        this.pendingRequests.delete(requestId);
        reject(new TimeoutError(`Request timeout after ${timeout}ms`, request.method));
      }, timeout);

      // Store pending request
      this.pendingRequests.set(requestId, pendingRequest);

      // Send request
      this.sendMessage(request).catch(reject);
    });
  }

  /**
   * Sends a message over the socket
   */
  private async sendMessage(message: JsonRpcRequest | JsonRpcNotification): Promise<void> {
    if (!this.socket || !this.isConnected()) {
      throw new ConnectionError('Not connected');
    }

    const messageStr = JSON.stringify(message) + '\n';
    const messageBuffer = Buffer.from(messageStr, 'utf8');

    // Check message size
    if (messageBuffer.length > this.config.maxMessageSize) {
      throw new DaemonClientError(
        `Message too large: ${messageBuffer.length} bytes (max: ${this.config.maxMessageSize})`
      );
    }

    return new Promise((resolve, reject) => {
      this.socket!.write(messageBuffer, (error?: Error | null) => {
        if (error) {
          reject(new ConnectionError(`Failed to send message: ${error.message}`));
        } else {
          this.statistics.bytesTransmitted += messageBuffer.length;
          this.statistics.requestsSent++;
          resolve();
        }
      });
    });
  }

  /**
   * Updates connection state and emits events
   */
  private setConnectionState(state: ClientConnectionState): void {
    if (this.connectionState === state) {
      return;
    }

    const previousState = this.connectionState;
    this.connectionState = state;

    if (this.config.debug) {
      // eslint-disable-next-line no-console
      console.log(`[GoxelDaemonClient] State: ${previousState} -> ${state}`);
    }
  }

  /**
   * Generates a unique request ID
   */
  private generateRequestId(): number {
    return ++this.requestId;
  }

  /**
   * Rejects all pending requests with given error
   */
  private rejectAllPendingRequests(error: Error): void {
    for (const pendingRequest of this.pendingRequests.values()) {
      if (pendingRequest.timeoutHandle) {
        clearTimeout(pendingRequest.timeoutHandle);
      }
      pendingRequest.reject(error);
    }
    this.pendingRequests.clear();
  }

  /**
   * Updates average response time statistic
   */
  private updateAverageResponseTime(responseTime: number): void {
    const currentAvg = this.statistics.averageResponseTime;
    const totalResponses = this.statistics.responsesReceived;
    
    // Calculate new average using incremental formula
    this.statistics.averageResponseTime = 
      (currentAvg * (totalResponses - 1) + responseTime) / totalResponses;
  }
}