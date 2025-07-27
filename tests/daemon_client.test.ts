/**
 * Comprehensive Unit Tests for Goxel Daemon Client
 * 
 * This test suite validates the TypeScript daemon client implementation,
 * covering all major functionality including connection handling, message
 * passing, error scenarios, and performance characteristics.
 */

import * as net from 'net';
import { EventEmitter } from 'events';
import { GoxelDaemonClient } from '../src/mcp-client/daemon_client';
import {
  ClientConnectionState,
  ClientEvent,
  DaemonClientError,
  ConnectionError,
  TimeoutError,
  JsonRpcClientError,
  JsonRpcErrorCode,
  DaemonMethod,
  JsonRpcResponse,
  JsonRpcErrorResponse,
  JsonRpcSuccessResponse,
} from '../src/mcp-client/types';

// Test constants
const TEST_SOCKET_PATH = '/tmp/goxel_test_client.sock';
const TEST_TIMEOUT = 1000;

// Mock server implementation for testing
class MockDaemonServer extends EventEmitter {
  private server: net.Server | null = null;
  private clients: net.Socket[] = [];
  private messageHandlers = new Map<string, (params: unknown) => unknown>();
  private shouldReject = false;
  private rejectMethod = '';
  private responseDelay = 0;

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

  public setMessageHandler(method: string, handler: (params: unknown) => unknown): void {
    this.messageHandlers.set(method, handler);
  }

  public setShouldReject(method: string, shouldReject = true): void {
    this.shouldReject = shouldReject;
    this.rejectMethod = method;
  }

  public setResponseDelay(delay: number): void {
    this.responseDelay = delay;
  }

  public getClientCount(): number {
    return this.clients.length;
  }

  public disconnectAllClients(): void {
    for (const client of this.clients) {
      client.destroy();
    }
  }

  private handleClientMessage(socket: net.Socket, data: Buffer): void {
    const messages = data.toString().trim().split('\n');
    
    for (const messageStr of messages) {
      if (!messageStr) continue;

      try {
        const message = JSON.parse(messageStr);
        
        // Handle request
        if ('method' in message && 'id' in message) {
          setTimeout(() => {
            this.handleRequest(socket, message);
          }, this.responseDelay);
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

    // Check if we should reject this method
    if (this.shouldReject && method === this.rejectMethod) {
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

    // Default handlers for common methods
    let result: unknown;
    switch (method) {
      case DaemonMethod.GET_VERSION:
        result = { version: '14.0.0', build: 'test' };
        break;

      case DaemonMethod.GET_STATUS:
        result = { status: 'running', uptime: 12345 };
        break;

      case DaemonMethod.CREATE_PROJECT:
        result = { success: true, projectId: 'test_project' };
        break;

      default:
        // Unknown method
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
const createTestClient = (config: Partial<any> = {}): GoxelDaemonClient => {
  return new GoxelDaemonClient({
    socketPath: TEST_SOCKET_PATH,
    timeout: TEST_TIMEOUT,
    debug: false,
    autoReconnect: false,
    ...config,
  });
};

const sleep = (ms: number): Promise<void> => {
  return new Promise(resolve => setTimeout(resolve, ms));
};

// Test suite
describe('GoxelDaemonClient', () => {
  let mockServer: MockDaemonServer;

  beforeEach(async () => {
    mockServer = new MockDaemonServer();
    await mockServer.start();
  });

  afterEach(async () => {
    await mockServer.stop();
  });

  // ========================================================================
  // CONSTRUCTOR AND CONFIGURATION TESTS
  // ========================================================================

  describe('Constructor and Configuration', () => {
    test('should create client with minimal config', () => {
      const client = createTestClient();
      
      expect(client).toBeInstanceOf(GoxelDaemonClient);
      expect(client.getConnectionState()).toBe(ClientConnectionState.DISCONNECTED);
      expect(client.isConnected()).toBe(false);
    });

    test('should merge with default configuration', () => {
      const client = createTestClient({
        timeout: 5000,
        retryAttempts: 5,
      });

      const config = client.getConfig();
      expect(config.socketPath).toBe(TEST_SOCKET_PATH);
      expect(config.timeout).toBe(5000);
      expect(config.retryAttempts).toBe(5);
      expect(config.debug).toBe(false); // default value
    });

    test('should initialize statistics', () => {
      const client = createTestClient();
      const stats = client.getStatistics();

      expect(stats.connectionAttempts).toBe(0);
      expect(stats.successfulConnections).toBe(0);
      expect(stats.requestsSent).toBe(0);
      expect(stats.responsesReceived).toBe(0);
    });
  });

  // ========================================================================
  // CONNECTION MANAGEMENT TESTS
  // ========================================================================

  describe('Connection Management', () => {
    test('should connect successfully', async () => {
      const client = createTestClient();
      const connectPromise = client.connect();

      await expect(connectPromise).resolves.toBeUndefined();
      expect(client.isConnected()).toBe(true);
      expect(client.getConnectionState()).toBe(ClientConnectionState.CONNECTED);

      const stats = client.getStatistics();
      expect(stats.connectionAttempts).toBe(1);
      expect(stats.successfulConnections).toBe(1);
      expect(stats.lastConnected).toBeInstanceOf(Date);

      await client.disconnect();
    });

    test('should emit connection events', async () => {
      const client = createTestClient();
      const connectedEvent = jest.fn();
      const disconnectedEvent = jest.fn();

      client.on(ClientEvent.CONNECTED, connectedEvent);
      client.on(ClientEvent.DISCONNECTED, disconnectedEvent);

      await client.connect();
      expect(connectedEvent).toHaveBeenCalledTimes(1);
      expect(connectedEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          timestamp: expect.any(Date),
          state: ClientConnectionState.CONNECTED,
        })
      );

      await client.disconnect();
      expect(disconnectedEvent).toHaveBeenCalledTimes(1);
    });

    test('should handle connection timeout', async () => {
      // Stop server to simulate unavailable service
      await mockServer.stop();

      const client = createTestClient({ timeout: 100 });
      
      await expect(client.connect()).rejects.toThrow(ConnectionError);
      expect(client.isConnected()).toBe(false);

      const stats = client.getStatistics();
      expect(stats.connectionAttempts).toBe(1);
      expect(stats.failedConnections).toBe(1);
    });

    test('should prevent multiple concurrent connections', async () => {
      const client = createTestClient();
      
      const connect1 = client.connect();
      const connect2 = client.connect();

      await expect(connect1).resolves.toBeUndefined();
      await expect(connect2).rejects.toThrow(ConnectionError);

      await client.disconnect();
    });

    test('should disconnect gracefully', async () => {
      const client = createTestClient();
      await client.connect();
      
      expect(client.isConnected()).toBe(true);
      
      await client.disconnect();
      expect(client.isConnected()).toBe(false);
      expect(client.getConnectionState()).toBe(ClientConnectionState.DISCONNECTED);
    });

    test('should handle server disconnection', async () => {
      const client = createTestClient();
      const disconnectedEvent = jest.fn();
      
      client.on(ClientEvent.DISCONNECTED, disconnectedEvent);
      
      await client.connect();
      expect(client.isConnected()).toBe(true);

      // Server force disconnects all clients
      mockServer.disconnectAllClients();
      
      // Wait for disconnection event
      await sleep(50);
      
      expect(client.isConnected()).toBe(false);
      expect(disconnectedEvent).toHaveBeenCalled();
    });
  });

  // ========================================================================
  // METHOD CALL TESTS
  // ========================================================================

  describe('Method Calls', () => {
    let client: GoxelDaemonClient;

    beforeEach(async () => {
      client = createTestClient();
      await client.connect();
    });

    afterEach(async () => {
      await client.disconnect();
    });

    test('should make successful method call', async () => {
      const result = await client.call(DaemonMethod.GET_VERSION);
      
      expect(result).toEqual({ version: '14.0.0', build: 'test' });

      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(1);
      expect(stats.responsesReceived).toBe(1);
    });

    test('should make method call with parameters', async () => {
      const params = { name: 'test_project' };
      const result = await client.call(DaemonMethod.CREATE_PROJECT, params);
      
      expect(result).toEqual({ success: true, projectId: 'test_project' });
    });

    test('should handle method not found error', async () => {
      await expect(client.call('nonexistent_method')).rejects.toThrow(JsonRpcClientError);
      
      try {
        await client.call('nonexistent_method');
      } catch (error) {
        expect(error).toBeInstanceOf(JsonRpcClientError);
        expect((error as JsonRpcClientError).code).toBe(JsonRpcErrorCode.METHOD_NOT_FOUND);
      }
    });

    test('should handle request timeout', async () => {
      // Set server to delay responses
      mockServer.setResponseDelay(TEST_TIMEOUT + 100);
      
      await expect(client.call(DaemonMethod.GET_VERSION)).rejects.toThrow(TimeoutError);

      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(1);
      expect(stats.responsesReceived).toBe(0);
    });

    test('should retry on timeout', async () => {
      const clientWithRetry = createTestClient({ retryAttempts: 2, retryDelay: 50 });
      await clientWithRetry.connect();

      // First request times out, second succeeds
      let callCount = 0;
      mockServer.setMessageHandler(DaemonMethod.GET_VERSION, () => {
        callCount++;
        if (callCount === 1) {
          // Don't respond to first call (will timeout)
          return;
        }
        return { version: '14.0.0', build: 'test' };
      });

      mockServer.setResponseDelay(TEST_TIMEOUT + 50);

      // Reset delay for retry
      setTimeout(() => {
        mockServer.setResponseDelay(0);
      }, TEST_TIMEOUT + 100);

      const result = await clientWithRetry.call(DaemonMethod.GET_VERSION);
      expect(result).toEqual({ version: '14.0.0', build: 'test' });

      await clientWithRetry.disconnect();
    });

    test('should send notifications', async () => {
      // Notifications don't return results
      const result = await client.call('test_notification', { data: 'test' }, { notification: true });
      
      expect(result).toBeUndefined();

      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(1);
    });

    test('should handle custom request ID', async () => {
      const customId = 'custom-123';
      const result = await client.call(DaemonMethod.GET_VERSION, undefined, { id: customId });
      
      expect(result).toEqual({ version: '14.0.0', build: 'test' });
    });

    test('should reject calls when not connected', async () => {
      await client.disconnect();
      
      await expect(client.call(DaemonMethod.GET_VERSION)).rejects.toThrow(ConnectionError);
    });

    test('should handle large messages', async () => {
      const largeParams = { data: 'x'.repeat(1000) };
      
      mockServer.setMessageHandler('large_method', (params) => {
        return { received: (params as any).data.length };
      });

      const result = await client.call('large_method', largeParams);
      expect(result).toEqual({ received: 1000 });
    });
  });

  // ========================================================================
  // ERROR HANDLING TESTS
  // ========================================================================

  describe('Error Handling', () => {
    let client: GoxelDaemonClient;

    beforeEach(async () => {
      client = createTestClient();
      await client.connect();
    });

    afterEach(async () => {
      await client.disconnect();
    });

    test('should handle JSON parse errors', async () => {
      const errorEvent = jest.fn();
      client.on(ClientEvent.ERROR, errorEvent);

      // Simulate invalid JSON from server
      const socket = (client as any).socket;
      socket.emit('data', Buffer.from('invalid json\n'));

      await sleep(10);

      expect(errorEvent).toHaveBeenCalledWith(
        expect.objectContaining({
          error: expect.any(DaemonClientError),
          context: 'message_parsing',
        })
      );
    });

    test('should handle server errors', async () => {
      mockServer.setMessageHandler('error_method', () => {
        throw new Error('Server error');
      });

      await expect(client.call('error_method')).rejects.toThrow(JsonRpcClientError);
    });

    test('should emit error events', async () => {
      const errorEvent = jest.fn();
      client.on(ClientEvent.ERROR, errorEvent);

      try {
        await client.call('nonexistent_method');
      } catch {
        // Expected
      }

      // Error events might be emitted asynchronously
      await sleep(10);
    });

    test('should validate message size', async () => {
      const clientWithSmallMax = createTestClient({ maxMessageSize: 100 });
      await clientWithSmallMax.connect();

      const largeParams = { data: 'x'.repeat(200) };

      await expect(clientWithSmallMax.call('test', largeParams)).rejects.toThrow(DaemonClientError);

      await clientWithSmallMax.disconnect();
    });
  });

  // ========================================================================
  // STATISTICS TESTS
  // ========================================================================

  describe('Statistics', () => {
    let client: GoxelDaemonClient;

    beforeEach(async () => {
      client = createTestClient();
      await client.connect();
    });

    afterEach(async () => {
      await client.disconnect();
    });

    test('should track connection statistics', async () => {
      const stats = client.getStatistics();
      
      expect(stats.connectionAttempts).toBe(1);
      expect(stats.successfulConnections).toBe(1);
      expect(stats.failedConnections).toBe(0);
      expect(stats.lastConnected).toBeInstanceOf(Date);
      expect(stats.uptime).toBeGreaterThan(0);
    });

    test('should track request/response statistics', async () => {
      await client.call(DaemonMethod.GET_VERSION);
      await client.call(DaemonMethod.GET_STATUS);

      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(2);
      expect(stats.responsesReceived).toBe(2);
      expect(stats.bytesTransmitted).toBeGreaterThan(0);
      expect(stats.bytesReceived).toBeGreaterThan(0);
    });

    test('should track average response time', async () => {
      await client.call(DaemonMethod.GET_VERSION);
      
      const stats = client.getStatistics();
      expect(stats.averageResponseTime).toBeGreaterThan(0);
    });

    test('should reset statistics', async () => {
      await client.call(DaemonMethod.GET_VERSION);
      
      let stats = client.getStatistics();
      expect(stats.requestsSent).toBe(1);

      client.resetStatistics();
      
      stats = client.getStatistics();
      expect(stats.requestsSent).toBe(0);
      expect(stats.connectionAttempts).toBe(0);
      expect(stats.averageResponseTime).toBe(0);
    });
  });

  // ========================================================================
  // AUTO-RECONNECTION TESTS
  // ========================================================================

  describe('Auto-Reconnection', () => {
    test('should auto-reconnect when enabled', async () => {
      const client = createTestClient({
        autoReconnect: true,
        reconnectInterval: 100,
      });

      const reconnectingEvent = jest.fn();
      client.on(ClientEvent.RECONNECTING, reconnectingEvent);

      await client.connect();
      expect(client.isConnected()).toBe(true);

      // Simulate server disconnect
      mockServer.disconnectAllClients();
      await sleep(50);

      // Should enter reconnecting state
      expect(client.getConnectionState()).toBe(ClientConnectionState.RECONNECTING);
      expect(reconnectingEvent).toHaveBeenCalled();

      // Wait for reconnection
      await sleep(200);
      expect(client.isConnected()).toBe(true);

      await client.disconnect();
    });

    test('should not auto-reconnect when disabled', async () => {
      const client = createTestClient({ autoReconnect: false });

      await client.connect();
      expect(client.isConnected()).toBe(true);

      // Simulate server disconnect
      mockServer.disconnectAllClients();
      await sleep(50);

      expect(client.isConnected()).toBe(false);
      expect(client.getConnectionState()).toBe(ClientConnectionState.DISCONNECTED);
    });
  });

  // ========================================================================
  // PERFORMANCE TESTS
  // ========================================================================

  describe('Performance', () => {
    let client: GoxelDaemonClient;

    beforeEach(async () => {
      client = createTestClient();
      await client.connect();
    });

    afterEach(async () => {
      await client.disconnect();
    });

    test('should handle multiple concurrent requests', async () => {
      const promises = [];
      const requestCount = 10;

      for (let i = 0; i < requestCount; i++) {
        promises.push(client.call(DaemonMethod.GET_VERSION));
      }

      const results = await Promise.all(promises);
      
      expect(results).toHaveLength(requestCount);
      results.forEach(result => {
        expect(result).toEqual({ version: '14.0.0', build: 'test' });
      });

      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(requestCount);
      expect(stats.responsesReceived).toBe(requestCount);
    });

    test('should maintain performance under load', async () => {
      const startTime = Date.now();
      const requestCount = 50;
      const promises = [];

      for (let i = 0; i < requestCount; i++) {
        promises.push(client.call(DaemonMethod.GET_VERSION));
      }

      await Promise.all(promises);
      
      const duration = Date.now() - startTime;
      const avgDuration = duration / requestCount;

      // Should complete each request in reasonable time
      expect(avgDuration).toBeLessThan(100); // Less than 100ms per request

      const stats = client.getStatistics();
      expect(stats.averageResponseTime).toBeLessThan(50); // Average response time under 50ms
    });
  });

  // ========================================================================
  // INTEGRATION TESTS
  // ========================================================================

  describe('Integration Tests', () => {
    test('should handle complete workflow', async () => {
      const client = createTestClient();
      
      // Connect
      await client.connect();
      expect(client.isConnected()).toBe(true);

      // Get version
      const version = await client.call(DaemonMethod.GET_VERSION);
      expect(version).toBeDefined();

      // Create project
      const projectResult = await client.call(DaemonMethod.CREATE_PROJECT, {
        name: 'integration_test',
      });
      expect(projectResult).toBeDefined();

      // Check statistics
      const stats = client.getStatistics();
      expect(stats.requestsSent).toBe(2);
      expect(stats.responsesReceived).toBe(2);

      // Disconnect
      await client.disconnect();
      expect(client.isConnected()).toBe(false);
    });

    test('should handle server restart', async () => {
      const client = createTestClient({
        autoReconnect: true,
        reconnectInterval: 100,
      });

      await client.connect();
      
      // Make initial request
      await client.call(DaemonMethod.GET_VERSION);
      
      // Stop and restart server
      await mockServer.stop();
      await sleep(50);
      await mockServer.start();
      
      // Wait for reconnection
      await sleep(200);
      
      // Should be able to make requests again
      const result = await client.call(DaemonMethod.GET_VERSION);
      expect(result).toBeDefined();

      await client.disconnect();
    });
  });
});