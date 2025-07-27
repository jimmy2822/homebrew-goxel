/**
 * Goxel v14.0 Daemon Client Library
 * 
 * A comprehensive TypeScript client library for communicating with the
 * Goxel daemon server using JSON-RPC 2.0 protocol over Unix sockets.
 * 
 * @example
 * ```typescript
 * import { GoxelDaemonClient, DaemonMethod } from 'goxel-daemon-client';
 * 
 * const client = new GoxelDaemonClient({
 *   socketPath: '/tmp/goxel_daemon.sock',
 *   timeout: 5000,
 * });
 * 
 * try {
 *   await client.connect();
 *   
 *   const version = await client.call(DaemonMethod.GET_VERSION);
 *   console.log('Goxel version:', version);
 *   
 *   await client.call(DaemonMethod.CREATE_PROJECT, { name: 'my_project' });
 *   await client.call(DaemonMethod.ADD_VOXEL, {
 *     x: 0, y: -16, z: 0,
 *     r: 255, g: 0, b: 0, a: 255
 *   });
 *   
 *   await client.call(DaemonMethod.EXPORT_PROJECT, {
 *     format: 'obj',
 *     path: './output.obj'
 *   });
 *   
 * } finally {
 *   await client.disconnect();
 * }
 * ```
 * 
 * @packageDocumentation
 */

// Export main client class
export { GoxelDaemonClient } from './mcp-client/daemon_client';

// Export all types and interfaces
export * from './mcp-client/types';

// Export type guards and utilities
export {
  isJsonRpcSuccessResponse,
  isJsonRpcErrorResponse,
  isJsonRpcNotification,
} from './mcp-client/types';

// Export error classes
export {
  DaemonClientError,
  ConnectionError,
  TimeoutError,
  JsonRpcClientError,
} from './mcp-client/types';

// Export constants and enums
export {
  JsonRpcErrorCode,
  ClientConnectionState,
  ClientEvent,
  DaemonMethod,
  DEFAULT_CLIENT_CONFIG,
} from './mcp-client/types';