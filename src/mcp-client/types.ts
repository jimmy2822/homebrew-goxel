/**
 * TypeScript type definitions for Goxel v14.0 Daemon Client
 * 
 * This file defines all the types used for communication between the TypeScript
 * client and the Goxel daemon server using JSON-RPC 2.0 protocol over Unix sockets.
 */

// ============================================================================
// JSON-RPC 2.0 CORE TYPES
// ============================================================================

/**
 * JSON-RPC 2.0 request ID type
 */
export type JsonRpcId = string | number | null;

/**
 * JSON-RPC 2.0 parameter types
 */
export type JsonRpcParams = unknown[] | Record<string, unknown> | undefined;

/**
 * JSON-RPC 2.0 request structure
 */
export interface JsonRpcRequest {
  readonly jsonrpc: '2.0';
  readonly method: string;
  readonly params?: JsonRpcParams;
  readonly id?: JsonRpcId;
}

/**
 * JSON-RPC 2.0 success response structure
 */
export interface JsonRpcSuccessResponse {
  readonly jsonrpc: '2.0';
  readonly result: unknown;
  readonly id: JsonRpcId;
}

/**
 * JSON-RPC 2.0 error object structure
 */
export interface JsonRpcError {
  readonly code: number;
  readonly message: string;
  readonly data?: unknown;
}

/**
 * JSON-RPC 2.0 error response structure
 */
export interface JsonRpcErrorResponse {
  readonly jsonrpc: '2.0';
  readonly error: JsonRpcError;
  readonly id: JsonRpcId;
}

/**
 * JSON-RPC 2.0 response union type
 */
export type JsonRpcResponse = JsonRpcSuccessResponse | JsonRpcErrorResponse;

/**
 * Notification (request without ID)
 */
export interface JsonRpcNotification {
  readonly jsonrpc: '2.0';
  readonly method: string;
  readonly params?: JsonRpcParams;
}

// ============================================================================
// JSON-RPC 2.0 ERROR CODES
// ============================================================================

/**
 * Standard JSON-RPC 2.0 error codes
 */
export const enum JsonRpcErrorCode {
  PARSE_ERROR = -32700,
  INVALID_REQUEST = -32600,
  METHOD_NOT_FOUND = -32601,
  INVALID_PARAMS = -32602,
  INTERNAL_ERROR = -32603,
  
  // Server error range
  SERVER_ERROR_START = -32099,
  SERVER_ERROR_END = -32000,
  
  // Application-specific errors
  APPLICATION_ERROR = -1000,
}

// ============================================================================
// DAEMON CLIENT CONFIGURATION
// ============================================================================

/**
 * Configuration options for the daemon client
 */
export interface DaemonClientConfig {
  /** Unix socket path to connect to */
  readonly socketPath: string;
  
  /** Connection timeout in milliseconds */
  readonly timeout: number;
  
  /** Number of retry attempts for failed requests */
  readonly retryAttempts: number;
  
  /** Delay between retry attempts in milliseconds */
  readonly retryDelay: number;
  
  /** Maximum message size in bytes */
  readonly maxMessageSize: number;
  
  /** Enable debug logging */
  readonly debug: boolean;
  
  /** Auto-reconnect on connection loss */
  readonly autoReconnect: boolean;
  
  /** Reconnection interval in milliseconds */
  readonly reconnectInterval: number;
}

/**
 * Partial configuration for creating client with defaults
 */
export type PartialDaemonClientConfig = Partial<DaemonClientConfig> & {
  readonly socketPath: string;
};

// ============================================================================
// CLIENT STATE AND EVENTS
// ============================================================================

/**
 * Client connection states
 */
export const enum ClientConnectionState {
  DISCONNECTED = 'disconnected',
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
  RECONNECTING = 'reconnecting',
  ERROR = 'error',
}

/**
 * Client events
 */
export const enum ClientEvent {
  CONNECTED = 'connected',
  DISCONNECTED = 'disconnected',
  ERROR = 'error',
  RECONNECTING = 'reconnecting',
  MESSAGE = 'message',
}

/**
 * Event listener type
 */
export type EventListener<T = unknown> = (data: T) => void;

/**
 * Connection event data
 */
export interface ConnectionEventData {
  readonly timestamp: Date;
  readonly state: ClientConnectionState;
}

/**
 * Error event data
 */
export interface ErrorEventData {
  readonly error: Error;
  readonly timestamp: Date;
  readonly context?: string;
}

/**
 * Message event data
 */
export interface MessageEventData {
  readonly message: JsonRpcResponse | JsonRpcNotification;
  readonly timestamp: Date;
}

// ============================================================================
// REQUEST/RESPONSE HANDLING
// ============================================================================

/**
 * Pending request information
 */
export interface PendingRequest {
  readonly id: JsonRpcId;
  readonly method: string;
  readonly timestamp: Date;
  readonly timeout: number;
  readonly resolve: (result: unknown) => void;
  readonly reject: (error: Error) => void;
  timeoutHandle?: NodeJS.Timeout;
}

/**
 * Request options
 */
export interface RequestOptions {
  /** Request timeout in milliseconds (overrides client default) */
  readonly timeout?: number;
  
  /** Number of retry attempts (overrides client default) */
  readonly retries?: number;
  
  /** Whether this is a notification (no response expected) */
  readonly notification?: boolean;
}

/**
 * Client method call options
 */
export interface CallOptions extends RequestOptions {
  /** Custom request ID (generated if not provided) */
  readonly id?: JsonRpcId;
}

// ============================================================================
// CLIENT STATISTICS
// ============================================================================

/**
 * Client statistics information
 */
export interface ClientStatistics {
  connectionAttempts: number;
  successfulConnections: number;
  failedConnections: number;
  requestsSent: number;
  responsesReceived: number;
  errorsReceived: number;
  bytesTransmitted: number;
  bytesReceived: number;
  averageResponseTime: number;
  uptime: number;
  lastConnected?: Date;
  lastDisconnected?: Date;
  lastError?: Date;
}

// ============================================================================
// GOXEL-SPECIFIC TYPES
// ============================================================================

/**
 * Goxel voxel coordinates
 */
export interface VoxelCoordinates {
  readonly x: number;
  readonly y: number;
  readonly z: number;
}

/**
 * Goxel RGBA color
 */
export interface RgbaColor {
  readonly r: number; // 0-255
  readonly g: number; // 0-255
  readonly b: number; // 0-255
  readonly a: number; // 0-255
}

/**
 * Goxel project information
 */
export interface ProjectInfo {
  readonly name: string;
  readonly path: string;
  readonly layerCount: number;
  readonly voxelCount: number;
  readonly modified: boolean;
}

/**
 * Goxel layer information
 */
export interface LayerInfo {
  readonly id: number;
  readonly name: string;
  readonly visible: boolean;
  readonly voxelCount: number;
}

/**
 * Export options for different formats
 */
export interface ExportOptions {
  readonly format: 'obj' | 'ply' | 'gox' | 'vox' | 'png' | 'gltf';
  readonly path: string;
  readonly options?: Record<string, unknown>;
}

/**
 * Render options for image generation
 */
export interface RenderOptions {
  readonly width: number;
  readonly height: number;
  readonly path: string;
  readonly format?: 'png' | 'jpg';
  readonly quality?: number;
}

// ============================================================================
// DAEMON METHODS
// ============================================================================

/**
 * Available daemon methods
 */
export const enum DaemonMethod {
  // System methods
  GET_VERSION = 'get_version',
  GET_STATUS = 'get_status',
  SHUTDOWN = 'shutdown',
  
  // Project methods
  CREATE_PROJECT = 'create_project',
  LOAD_PROJECT = 'load_project',
  SAVE_PROJECT = 'save_project',
  GET_PROJECT_INFO = 'get_project_info',
  
  // Voxel operations
  ADD_VOXEL = 'add_voxel',
  REMOVE_VOXEL = 'remove_voxel',
  GET_VOXEL = 'get_voxel',
  CLEAR_VOXELS = 'clear_voxels',
  
  // Layer operations
  CREATE_LAYER = 'create_layer',
  DELETE_LAYER = 'delete_layer',
  GET_LAYERS = 'get_layers',
  SET_ACTIVE_LAYER = 'set_active_layer',
  
  // Export/Import
  EXPORT_PROJECT = 'export_project',
  IMPORT_MODEL = 'import_model',
  RENDER_IMAGE = 'render_image',
}

// ============================================================================
// ERROR CLASSES
// ============================================================================

/**
 * Base class for daemon client errors
 */
export class DaemonClientError extends Error {
  public readonly code?: number | undefined;
  public readonly context?: string | undefined;
  
  public constructor(message: string, code?: number | undefined, context?: string | undefined) {
    super(message);
    this.name = 'DaemonClientError';
    this.code = code;
    this.context = context;
  }
}

/**
 * Connection-related errors
 */
export class ConnectionError extends DaemonClientError {
  public constructor(message: string, context?: string) {
    super(message, undefined, context);
    this.name = 'ConnectionError';
  }
}

/**
 * Request timeout errors
 */
export class TimeoutError extends DaemonClientError {
  public constructor(message: string, context?: string) {
    super(message, undefined, context);
    this.name = 'TimeoutError';
  }
}

/**
 * JSON-RPC protocol errors
 */
export class JsonRpcClientError extends DaemonClientError {
  public constructor(message: string, code: number, context?: string) {
    super(message, code, context);
    this.name = 'JsonRpcError';
  }
}

// ============================================================================
// UTILITY TYPES
// ============================================================================

/**
 * Type guard for JSON-RPC success response
 */
export function isJsonRpcSuccessResponse(response: JsonRpcResponse): response is JsonRpcSuccessResponse {
  return 'result' in response;
}

/**
 * Type guard for JSON-RPC error response
 */
export function isJsonRpcErrorResponse(response: JsonRpcResponse): response is JsonRpcErrorResponse {
  return 'error' in response;
}

/**
 * Type guard for JSON-RPC notification
 */
export function isJsonRpcNotification(message: JsonRpcRequest | JsonRpcNotification): message is JsonRpcNotification {
  return !('id' in message);
}

// ============================================================================
// CONNECTION POOL TYPES
// ============================================================================

/**
 * Connection pool configuration
 */
export interface ConnectionPoolConfig {
  /** Minimum number of connections to maintain */
  readonly minConnections: number;
  
  /** Maximum number of connections allowed */
  readonly maxConnections: number;
  
  /** How long to wait for available connection in ms */
  readonly acquireTimeout: number;
  
  /** Connection idle timeout in ms */
  readonly idleTimeout: number;
  
  /** How often to check connection health in ms */
  readonly healthCheckInterval: number;
  
  /** Maximum number of requests per connection before rotation */
  readonly maxRequestsPerConnection: number;
  
  /** Enable connection load balancing */
  readonly loadBalancing: boolean;
  
  /** Connection pool debug logging */
  readonly debug: boolean;
}

/**
 * Connection health status
 */
export const enum ConnectionHealth {
  HEALTHY = 'healthy',
  DEGRADED = 'degraded',
  UNHEALTHY = 'unhealthy',
  UNKNOWN = 'unknown',
}

/**
 * Connection pool state
 */
export const enum PoolState {
  INITIALIZING = 'initializing',
  READY = 'ready',
  DEGRADED = 'degraded',
  SHUTDOWN = 'shutdown',
}

/**
 * Connection information in pool
 */
export interface PooledConnection {
  readonly id: string;
  readonly client: any; // GoxelDaemonClient
  readonly createdAt: Date;
  readonly health: ConnectionHealth;
  readonly requestCount: number;
  readonly lastUsed: Date;
  readonly isAvailable: boolean;
}

/**
 * Connection pool statistics
 */
export interface PoolStatistics {
  totalConnections: number;
  availableConnections: number;
  busyConnections: number;
  healthyConnections: number;
  totalRequestsServed: number;
  averageRequestTime: number;
  poolUptime: number;
  connectionFailures: number;
  reconnectionAttempts: number;
  lastHealthCheck: Date | null;
}

/**
 * Request queue entry
 */
export interface QueuedRequest {
  readonly id: string;
  readonly method: string;
  readonly params?: JsonRpcParams;
  readonly options: CallOptions;
  readonly timestamp: Date;
  readonly resolve: (result: unknown) => void;
  readonly reject: (error: Error) => void;
  timeoutHandle?: NodeJS.Timeout;
}

/**
 * Health check result
 */
export interface HealthCheckResult {
  readonly connectionId: string;
  readonly health: ConnectionHealth;
  readonly latency: number;
  readonly error?: Error;
  readonly timestamp: Date;
}

/**
 * Pool events
 */
export const enum PoolEvent {
  READY = 'ready',
  DEGRADED = 'degraded',
  CONNECTION_ADDED = 'connection_added',
  CONNECTION_REMOVED = 'connection_removed',
  CONNECTION_HEALTH_CHANGED = 'connection_health_changed',
  REQUEST_QUEUED = 'request_queued',
  REQUEST_SERVED = 'request_served',
  ERROR = 'error',
}

/**
 * Pool event data types
 */
export interface PoolEventData {
  readonly timestamp: Date;
  readonly poolState: PoolState;
}

export interface ConnectionEventData extends PoolEventData {
  readonly connection: PooledConnection;
}

export interface HealthEventData extends PoolEventData {
  readonly healthCheck: HealthCheckResult;
}

export interface RequestEventData extends PoolEventData {
  readonly request: QueuedRequest;
}

export interface PoolErrorEventData extends PoolEventData {
  readonly error: Error;
  readonly context?: string;
}

// ============================================================================
// ENHANCED CLIENT CONFIGURATION FOR POOLING
// ============================================================================

/**
 * Enhanced daemon client configuration with pool support
 */
export interface PooledDaemonClientConfig extends DaemonClientConfig {
  /** Connection pool configuration */
  readonly pool?: ConnectionPoolConfig;
  
  /** Enable connection pooling */
  readonly enablePooling: boolean;
}

/**
 * Partial pooled configuration for creating client with defaults
 */
export type PartialPooledDaemonClientConfig = Partial<PooledDaemonClientConfig> & {
  readonly socketPath: string;
};

/**
 * Default connection pool configuration
 */
export const DEFAULT_POOL_CONFIG: ConnectionPoolConfig = {
  minConnections: 2,
  maxConnections: 10,
  acquireTimeout: 5000,
  idleTimeout: 60000,
  healthCheckInterval: 30000,
  maxRequestsPerConnection: 1000,
  loadBalancing: true,
  debug: false,
} as const;

/**
 * Default client configuration
 */
export const DEFAULT_CLIENT_CONFIG: DaemonClientConfig = {
  socketPath: '/tmp/goxel_daemon.sock',
  timeout: 5000,
  retryAttempts: 3,
  retryDelay: 1000,
  maxMessageSize: 1024 * 1024, // 1MB
  debug: false,
  autoReconnect: true,
  reconnectInterval: 2000,
} as const;

/**
 * Default pooled client configuration
 */
export const DEFAULT_POOLED_CLIENT_CONFIG: PooledDaemonClientConfig = {
  ...DEFAULT_CLIENT_CONFIG,
  enablePooling: false,
  pool: DEFAULT_POOL_CONFIG,
} as const;

// Re-export health monitor events for convenience
export { HealthMonitorEvent } from './health_monitor';