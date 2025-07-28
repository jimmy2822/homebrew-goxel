/**
 * MCP Server Integration Example
 * 
 * This example demonstrates how the MCP server can integrate with the
 * Goxel daemon using the TypeScript client library. It shows patterns
 * for tool implementation and error handling.
 */

import {
  GoxelDaemonClient,
  DaemonMethod,
  ClientEvent,
  ConnectionError,
  JsonRpcClientError,
  RgbaColor,
  ExportOptions,
} from '../index';

/**
 * Example MCP tool handler that uses the daemon client
 */
class GoxelMCPToolHandler {
  private client: GoxelDaemonClient;
  private isConnected = false;

  constructor() {
    this.client = new GoxelDaemonClient({
      socketPath: '/tmp/goxel-daemon.sock',
      timeout: 10000, // 10 seconds for MCP operations
      retryAttempts: 3,
      retryDelay: 1000,
      autoReconnect: true,
      debug: process.env['DEBUG'] === 'true',
    });

    // Setup event handlers
    this.setupEventHandlers();
  }

  /**
   * Initialize connection to daemon
   */
  async initialize(): Promise<void> {
    try {
      await this.client.connect();
      this.isConnected = true;
      console.log('Connected to Goxel daemon');

      // Verify daemon is responsive
      const version = await this.client.call(DaemonMethod.GET_VERSION);
      console.log('Goxel daemon version:', version);
    } catch (error) {
      console.error('Failed to connect to daemon:', error);
      throw error;
    }
  }

  /**
   * Cleanup and disconnect
   */
  async shutdown(): Promise<void> {
    if (this.isConnected) {
      await this.client.disconnect();
      this.isConnected = false;
      console.log('Disconnected from Goxel daemon');
    }
  }

  /**
   * MCP Tool: Create a new voxel project
   */
  async createProject(params: { name: string }): Promise<any> {
    this.ensureConnected();
    
    try {
      const result = await this.client.call(DaemonMethod.CREATE_PROJECT, params);
      return {
        success: true,
        message: `Project '${params.name}' created successfully`,
        data: result,
      };
    } catch (error) {
      return this.handleError(error, 'create_project');
    }
  }

  /**
   * MCP Tool: Add a voxel at specified coordinates
   */
  async addVoxel(params: {
    x: number;
    y: number;
    z: number;
    color: string; // Hex color like "#FF0000"
  }): Promise<any> {
    this.ensureConnected();

    try {
      // Convert hex color to RGBA
      const rgba = this.hexToRgba(params.color);
      
      const voxelParams = {
        x: params.x,
        y: params.y,
        z: params.z,
        ...rgba,
      };

      await this.client.call(DaemonMethod.ADD_VOXEL, voxelParams);
      
      return {
        success: true,
        message: `Voxel added at (${params.x}, ${params.y}, ${params.z})`,
      };
    } catch (error) {
      return this.handleError(error, 'add_voxel');
    }
  }

  /**
   * MCP Tool: Export the current project
   */
  async exportProject(params: {
    format: 'obj' | 'ply' | 'gox' | 'vox' | 'png' | 'gltf';
    path: string;
  }): Promise<any> {
    this.ensureConnected();

    try {
      const exportOptions: ExportOptions = {
        format: params.format,
        path: params.path,
      };

      await this.client.call(DaemonMethod.EXPORT_PROJECT, exportOptions as unknown as Record<string, unknown>);
      
      return {
        success: true,
        message: `Project exported to ${params.path}`,
        format: params.format,
      };
    } catch (error) {
      return this.handleError(error, 'export_project');
    }
  }

  /**
   * MCP Tool: Get project information
   */
  async getProjectInfo(): Promise<any> {
    this.ensureConnected();

    try {
      const info = await this.client.call(DaemonMethod.GET_PROJECT_INFO);
      return {
        success: true,
        data: info,
      };
    } catch (error) {
      return this.handleError(error, 'get_project_info');
    }
  }

  /**
   * MCP Tool: Batch voxel operations
   */
  async batchAddVoxels(params: {
    voxels: Array<{
      x: number;
      y: number;
      z: number;
      color: string;
    }>;
  }): Promise<any> {
    this.ensureConnected();

    try {
      const results: unknown[] = [];
      const errors: Array<{
        error: boolean;
        voxel: { x: number; y: number; z: number; color: string };
        message: string;
      }> = [];

      // Process voxels in batches for better performance
      const batchSize = 50;
      for (let i = 0; i < params.voxels.length; i += batchSize) {
        const batch = params.voxels.slice(i, i + batchSize);
        
        // Create promises for parallel execution
        const promises = batch.map(voxel => {
          const rgba = this.hexToRgba(voxel.color);
          return this.client.call(DaemonMethod.ADD_VOXEL, {
            x: voxel.x,
            y: voxel.y,
            z: voxel.z,
            ...rgba,
          }).catch(error => ({
            error: true,
            voxel,
            message: error.message,
          }));
        });

        // Wait for batch to complete
        const batchResults = await Promise.all(promises);
        
        batchResults.forEach(result => {
          if (result && typeof result === 'object' && 'error' in result && 'voxel' in result && 'message' in result) {
            errors.push(result as {
              error: boolean;
              voxel: { x: number; y: number; z: number; color: string };
              message: string;
            });
          } else {
            results.push(result);
          }
        });
      }

      return {
        success: errors.length === 0,
        message: `Added ${results.length} voxels successfully`,
        successCount: results.length,
        errorCount: errors.length,
        errors: errors.length > 0 ? errors : undefined,
      };
    } catch (error) {
      return this.handleError(error, 'batch_add_voxels');
    }
  }

  /**
   * Setup event handlers for monitoring
   */
  private setupEventHandlers(): void {
    this.client.on(ClientEvent.CONNECTED, () => {
      console.log('[MCP] Daemon connected');
      this.isConnected = true;
    });

    this.client.on(ClientEvent.DISCONNECTED, () => {
      console.log('[MCP] Daemon disconnected');
      this.isConnected = false;
    });

    this.client.on(ClientEvent.RECONNECTING, () => {
      console.log('[MCP] Attempting to reconnect to daemon...');
    });

    this.client.on(ClientEvent.ERROR, (data) => {
      console.error('[MCP] Daemon client error:', data.error.message);
    });
  }

  /**
   * Ensure client is connected
   */
  private ensureConnected(): void {
    if (!this.isConnected) {
      throw new Error('Not connected to Goxel daemon');
    }
  }

  /**
   * Convert hex color to RGBA
   */
  private hexToRgba(hex: string): RgbaColor {
    // Remove # if present
    hex = hex.replace('#', '');
    
    // Parse hex values
    const r = parseInt(hex.substr(0, 2), 16);
    const g = parseInt(hex.substr(2, 2), 16);
    const b = parseInt(hex.substr(4, 2), 16);
    const a = hex.length > 6 ? parseInt(hex.substr(6, 2), 16) : 255;

    return { r, g, b, a };
  }

  /**
   * Handle errors consistently for MCP responses
   */
  private handleError(error: unknown, tool: string): any {
    if (error instanceof ConnectionError) {
      return {
        success: false,
        error: 'connection_error',
        message: `Daemon connection error: ${error.message}`,
        tool,
      };
    } else if (error instanceof JsonRpcClientError) {
      return {
        success: false,
        error: 'daemon_error',
        message: `Daemon error: ${error.message}`,
        code: error.code,
        tool,
      };
    } else {
      return {
        success: false,
        error: 'unknown_error',
        message: error instanceof Error ? error.message : 'Unknown error',
        tool,
      };
    }
  }

  /**
   * Get client statistics for monitoring
   */
  getStatistics() {
    return this.client.getStatistics();
  }
}

/**
 * Example usage in MCP server
 */
async function mcpServerExample() {
  const handler = new GoxelMCPToolHandler();

  try {
    // Initialize connection
    await handler.initialize();

    // Create a new project
    const projectResult = await handler.createProject({ name: 'mcp_demo' });
    console.log('Create project result:', projectResult);

    // Add some voxels
    const voxelResult = await handler.addVoxel({
      x: 0,
      y: -16,
      z: 0,
      color: '#FF0000',
    });
    console.log('Add voxel result:', voxelResult);

    // Batch add voxels (create a simple structure)
    const voxels = [];
    for (let x = -2; x <= 2; x++) {
      for (let z = -2; z <= 2; z++) {
        voxels.push({
          x,
          y: -16,
          z,
          color: '#00FF00',
        });
      }
    }

    const batchResult = await handler.batchAddVoxels({ voxels });
    console.log('Batch add result:', batchResult);

    // Get project info
    const infoResult = await handler.getProjectInfo();
    console.log('Project info:', infoResult);

    // Export the project
    const exportResult = await handler.exportProject({
      format: 'obj',
      path: './mcp_demo_output.obj',
    });
    console.log('Export result:', exportResult);

    // Show statistics
    const stats = handler.getStatistics();
    console.log('Client statistics:', {
      requests: stats.requestsSent,
      responses: stats.responsesReceived,
      avgResponseTime: `${stats.averageResponseTime.toFixed(2)}ms`,
    });

  } catch (error) {
    console.error('MCP server error:', error);
  } finally {
    // Cleanup
    await handler.shutdown();
  }
}

// Export for use in MCP server
export { GoxelMCPToolHandler };

// Run example if executed directly
if (require.main === module) {
  mcpServerExample().catch(console.error);
}