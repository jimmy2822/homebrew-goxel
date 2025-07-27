/**
 * Goxel v14.0 MCP Server Integration Demo
 * 
 * This demo shows how to integrate the Goxel daemon with Claude AI
 * using the Model Context Protocol (MCP). It provides a complete
 * MCP server that exposes Goxel functionality as tools for Claude.
 */

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { ListToolsRequestSchema, CallToolRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { GoxelDaemonClient } from '@goxel/daemon-client';

interface MCPToolResult {
  content: Array<{
    type: 'text' | 'image';
    text?: string;
    data?: string;
    mimeType?: string;
  }>;
  isError?: boolean;
}

interface VoxelArtProject {
  id: string;
  name: string;
  description: string;
  voxelCount: number;
  lastModified: Date;
}

/**
 * Enhanced MCP Server for Goxel v14.0 Daemon Integration
 */
export class GoxelMCPServer {
  private server: Server;
  private client: GoxelDaemonClient;
  private currentProject: string | null = null;
  private projectHistory: VoxelArtProject[] = [];
  private isConnected = false;

  constructor() {
    this.server = new Server(
      {
        name: 'goxel-daemon-mcp-server',
        version: '14.0.0',
      },
      {
        capabilities: {
          tools: {},
          resources: {},
        },
      }
    );

    this.client = new GoxelDaemonClient({
      socketPath: process.env.GOXEL_DAEMON_SOCKET || '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 60000,
      enableDebugLogging: process.env.DEBUG === 'true'
    });

    this.setupTools();
    this.setupResources();
    this.setupEventHandlers();
  }

  /**
   * Set up all available MCP tools
   */
  private setupTools(): void {
    // List available tools
    this.server.setRequestHandler(ListToolsRequestSchema, async () => ({
      tools: [
        {
          name: 'goxel_create_project',
          description: 'Create a new voxel art project with optional template',
          inputSchema: {
            type: 'object',
            properties: {
              name: { 
                type: 'string', 
                description: 'Project name (required)' 
              },
              template: { 
                type: 'string', 
                enum: ['empty', 'cube', 'sphere'],
                description: 'Project template (optional, default: empty)'
              },
              size: {
                type: 'array',
                items: { type: 'number' },
                minItems: 3,
                maxItems: 3,
                description: 'Canvas size as [width, height, depth] (optional, default: [64, 64, 64])'
              }
            },
            required: ['name']
          }
        },
        {
          name: 'goxel_add_voxel',
          description: 'Add a single voxel to the current project',
          inputSchema: {
            type: 'object',
            properties: {
              x: { type: 'number', description: 'X coordinate' },
              y: { type: 'number', description: 'Y coordinate (use -16 for ground level)' },
              z: { type: 'number', description: 'Z coordinate' },
              r: { type: 'number', minimum: 0, maximum: 255, description: 'Red component (0-255)' },
              g: { type: 'number', minimum: 0, maximum: 255, description: 'Green component (0-255)' },
              b: { type: 'number', minimum: 0, maximum: 255, description: 'Blue component (0-255)' },
              a: { type: 'number', minimum: 0, maximum: 255, description: 'Alpha component (0-255, default: 255)' }
            },
            required: ['x', 'y', 'z', 'r', 'g', 'b']
          }
        },
        {
          name: 'goxel_add_voxel_batch',
          description: 'Add multiple voxels efficiently in a single operation',
          inputSchema: {
            type: 'object',
            properties: {
              voxels: {
                type: 'array',
                description: 'Array of voxel objects to add',
                items: {
                  type: 'object',
                  properties: {
                    x: { type: 'number' },
                    y: { type: 'number' },
                    z: { type: 'number' },
                    r: { type: 'number', minimum: 0, maximum: 255 },
                    g: { type: 'number', minimum: 0, maximum: 255 },
                    b: { type: 'number', minimum: 0, maximum: 255 },
                    a: { type: 'number', minimum: 0, maximum: 255, default: 255 }
                  },
                  required: ['x', 'y', 'z', 'r', 'g', 'b']
                }
              }
            },
            required: ['voxels']
          }
        },
        {
          name: 'goxel_create_shape',
          description: 'Create predefined 3D shapes (cube, sphere, pyramid, etc.)',
          inputSchema: {
            type: 'object',
            properties: {
              shape: {
                type: 'string',
                enum: ['cube', 'sphere', 'pyramid', 'cylinder', 'torus'],
                description: 'Type of shape to create'
              },
              center_x: { type: 'number', description: 'Center X coordinate' },
              center_y: { type: 'number', description: 'Center Y coordinate' },
              center_z: { type: 'number', description: 'Center Z coordinate' },
              size: { type: 'number', description: 'Size/radius of the shape' },
              r: { type: 'number', minimum: 0, maximum: 255, description: 'Red component' },
              g: { type: 'number', minimum: 0, maximum: 255, description: 'Green component' },
              b: { type: 'number', minimum: 0, maximum: 255, description: 'Blue component' },
              a: { type: 'number', minimum: 0, maximum: 255, description: 'Alpha component', default: 255 }
            },
            required: ['shape', 'center_x', 'center_y', 'center_z', 'size', 'r', 'g', 'b']
          }
        },
        {
          name: 'goxel_render_image',
          description: 'Render the current project to an image',
          inputSchema: {
            type: 'object',
            properties: {
              width: { type: 'number', default: 512, description: 'Image width in pixels' },
              height: { type: 'number', default: 512, description: 'Image height in pixels' },
              camera_distance: { type: 'number', default: 30, description: 'Camera distance from center' },
              camera_angle: { 
                type: 'string', 
                enum: ['iso', 'front', 'back', 'left', 'right', 'top', 'bottom'],
                default: 'iso',
                description: 'Camera viewing angle'
              },
              format: { 
                type: 'string', 
                enum: ['png', 'jpg'],
                default: 'png',
                description: 'Output image format'
              }
            }
          }
        },
        {
          name: 'goxel_get_project_info',
          description: 'Get detailed information about the current project',
          inputSchema: {
            type: 'object',
            properties: {},
            additionalProperties: false
          }
        },
        {
          name: 'goxel_save_project',
          description: 'Save the current project to a file',
          inputSchema: {
            type: 'object',
            properties: {
              file_path: { 
                type: 'string', 
                description: 'File path to save the project (should end with .gox)' 
              }
            },
            required: ['file_path']
          }
        },
        {
          name: 'goxel_load_project',
          description: 'Load a project from a file',
          inputSchema: {
            type: 'object',
            properties: {
              file_path: { 
                type: 'string', 
                description: 'File path to load the project from' 
              }
            },
            required: ['file_path']
          }
        },
        {
          name: 'goxel_export_model',
          description: 'Export the current project to various 3D model formats',
          inputSchema: {
            type: 'object',
            properties: {
              file_path: { type: 'string', description: 'Output file path' },
              format: { 
                type: 'string', 
                enum: ['obj', 'ply', 'stl', 'gltf', 'vox'],
                description: 'Export format'
              },
              include_textures: { type: 'boolean', default: true, description: 'Include texture information' },
              optimize_mesh: { type: 'boolean', default: true, description: 'Optimize the mesh for smaller file size' }
            },
            required: ['file_path', 'format']
          }
        },
        {
          name: 'goxel_create_terrain',
          description: 'Generate procedural terrain using Perlin noise',
          inputSchema: {
            type: 'object',
            properties: {
              width: { type: 'number', default: 32, description: 'Terrain width' },
              depth: { type: 'number', default: 32, description: 'Terrain depth' },
              max_height: { type: 'number', default: 16, description: 'Maximum terrain height' },
              noise_scale: { type: 'number', default: 0.1, description: 'Noise scale (smaller = smoother)' },
              base_color: {
                type: 'object',
                properties: {
                  r: { type: 'number', minimum: 0, maximum: 255 },
                  g: { type: 'number', minimum: 0, maximum: 255 },
                  b: { type: 'number', minimum: 0, maximum: 255 }
                },
                default: { r: 34, g: 139, b: 34 }
              }
            }
          }
        }
      ]
    }));

    // Handle tool calls
    this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
      const { name, arguments: args } = request.params;

      try {
        await this.ensureConnected();

        switch (name) {
          case 'goxel_create_project':
            return await this.handleCreateProject(args);
          case 'goxel_add_voxel':
            return await this.handleAddVoxel(args);
          case 'goxel_add_voxel_batch':
            return await this.handleAddVoxelBatch(args);
          case 'goxel_create_shape':
            return await this.handleCreateShape(args);
          case 'goxel_render_image':
            return await this.handleRenderImage(args);
          case 'goxel_get_project_info':
            return await this.handleGetProjectInfo(args);
          case 'goxel_save_project':
            return await this.handleSaveProject(args);
          case 'goxel_load_project':
            return await this.handleLoadProject(args);
          case 'goxel_export_model':
            return await this.handleExportModel(args);
          case 'goxel_create_terrain':
            return await this.handleCreateTerrain(args);
          default:
            throw new Error(`Unknown tool: ${name}`);
        }
      } catch (error) {
        return {
          content: [
            {
              type: 'text',
              text: `❌ Error: ${error.message}`
            }
          ],
          isError: true
        };
      }
    });
  }

  /**
   * Set up MCP resources (project files, etc.)
   */
  private setupResources(): void {
    // Implementation for resources would go here
    // This could include listing available project files, templates, etc.
  }

  /**
   * Set up event handlers for the daemon client
   */
  private setupEventHandlers(): void {
    this.client.on('disconnect', () => {
      console.log('Daemon connection lost');
      this.isConnected = false;
    });

    this.client.on('reconnect', () => {
      console.log('Daemon connection restored');
      this.isConnected = true;
    });

    this.client.on('error', (error) => {
      console.error('Daemon client error:', error);
    });
  }

  /**
   * Ensure connection to daemon
   */
  private async ensureConnected(): Promise<void> {
    if (!this.isConnected) {
      try {
        await this.client.connect();
        this.isConnected = true;
      } catch (error) {
        throw new Error(`Failed to connect to Goxel daemon: ${error.message}`);
      }
    }
  }

  /**
   * Ensure current project exists
   */
  private async ensureProject(): Promise<void> {
    if (!this.currentProject) {
      throw new Error('No project loaded. Please create a project first using goxel_create_project.');
    }
  }

  /**
   * Handle create project tool call
   */
  private async handleCreateProject(args: any): Promise<MCPToolResult> {
    const project = await this.client.createProject({
      name: args.name,
      template: args.template || 'empty',
      size: args.size || [64, 64, 64]
    });

    this.currentProject = project.project_id;
    
    // Add to project history
    this.projectHistory.push({
      id: project.project_id,
      name: project.name,
      description: `Project created with template: ${args.template || 'empty'}`,
      voxelCount: 0,
      lastModified: new Date()
    });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Created project "${project.name}" with ID: ${project.project_id}\n` +
                `Canvas size: ${project.canvas_size.join(' × ')}\n` +
                `Initial layers: ${project.layer_count}`
        }
      ]
    };
  }

  /**
   * Handle add voxel tool call
   */
  private async handleAddVoxel(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const result = await this.client.addVoxel({
      position: [args.x, args.y, args.z],
      color: [args.r, args.g, args.b, args.a || 255]
    });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Added voxel at (${args.x}, ${args.y}, ${args.z}) with color RGB(${args.r}, ${args.g}, ${args.b}${args.a ? `, ${args.a}` : ''})`
        }
      ]
    };
  }

  /**
   * Handle add voxel batch tool call
   */
  private async handleAddVoxelBatch(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const voxels = args.voxels.map((v: any) => ({
      position: [v.x, v.y, v.z],
      color: [v.r, v.g, v.b, v.a || 255]
    }));

    const result = await this.client.addVoxelBatch({ voxels });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Added ${result.processed_count} voxels to the project\n` +
                `${result.failed_count > 0 ? `⚠️ ${result.failed_count} voxels failed to add` : ''}`
        }
      ]
    };
  }

  /**
   * Handle create shape tool call
   */
  private async handleCreateShape(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const voxels = this.generateShapeVoxels(
      args.shape,
      [args.center_x, args.center_y, args.center_z],
      args.size,
      [args.r, args.g, args.b, args.a || 255]
    );

    const result = await this.client.addVoxelBatch({ voxels });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Created ${args.shape} with ${result.processed_count} voxels at center (${args.center_x}, ${args.center_y}, ${args.center_z})`
        }
      ]
    };
  }

  /**
   * Generate voxels for basic shapes
   */
  private generateShapeVoxels(
    shape: string, 
    center: [number, number, number], 
    size: number, 
    color: [number, number, number, number]
  ): Array<{ position: [number, number, number]; color: [number, number, number, number] }> {
    const voxels = [];
    const [cx, cy, cz] = center;

    switch (shape) {
      case 'cube':
        for (let x = cx - size; x <= cx + size; x++) {
          for (let y = cy - size; y <= cy + size; y++) {
            for (let z = cz - size; z <= cz + size; z++) {
              voxels.push({
                position: [x, y, z] as [number, number, number],
                color: color
              });
            }
          }
        }
        break;

      case 'sphere':
        for (let x = cx - size; x <= cx + size; x++) {
          for (let y = cy - size; y <= cy + size; y++) {
            for (let z = cz - size; z <= cz + size; z++) {
              const distance = Math.sqrt(
                Math.pow(x - cx, 2) + 
                Math.pow(y - cy, 2) + 
                Math.pow(z - cz, 2)
              );
              
              if (distance <= size) {
                voxels.push({
                  position: [x, y, z] as [number, number, number],
                  color: color
                });
              }
            }
          }
        }
        break;

      case 'pyramid':
        for (let y = cy; y <= cy + size; y++) {
          const level = y - cy;
          const levelSize = size - level;
          
          for (let x = cx - levelSize; x <= cx + levelSize; x++) {
            for (let z = cz - levelSize; z <= cz + levelSize; z++) {
              voxels.push({
                position: [x, y, z] as [number, number, number],
                color: color
              });
            }
          }
        }
        break;

      case 'cylinder':
        for (let y = cy - size; y <= cy + size; y++) {
          for (let x = cx - size; x <= cx + size; x++) {
            for (let z = cz - size; z <= cz + size; z++) {
              const distance = Math.sqrt(
                Math.pow(x - cx, 2) + 
                Math.pow(z - cz, 2)
              );
              
              if (distance <= size) {
                voxels.push({
                  position: [x, y, z] as [number, number, number],
                  color: color
                });
              }
            }
          }
        }
        break;

      default:
        throw new Error(`Unknown shape: ${shape}`);
    }

    return voxels;
  }

  /**
   * Handle render image tool call
   */
  private async handleRenderImage(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const cameraPositions = {
      iso: [25, 25, 25],
      front: [0, 0, 30],
      back: [0, 0, -30],
      left: [-30, 0, 0],
      right: [30, 0, 0],
      top: [0, 30, 0],
      bottom: [0, -30, 0]
    };

    const distance = args.camera_distance || 30;
    const angle = args.camera_angle || 'iso';
    let position = cameraPositions[angle as keyof typeof cameraPositions];
    
    if (angle === 'iso') {
      position = [distance, distance, distance];
    }

    const result = await this.client.renderImage({
      width: args.width || 512,
      height: args.height || 512,
      camera: {
        position: position,
        target: [0, -16, 0],
        up: [0, 1, 0],
        fov: 45
      },
      format: args.format || 'png'
    });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Rendered ${result.width}x${result.height} ${result.format} image from ${angle} angle in ${result.render_time_ms}ms`
        },
        {
          type: 'image',
          data: result.image_data,
          mimeType: `image/${result.format}`
        }
      ]
    };
  }

  /**
   * Handle get project info tool call
   */
  private async handleGetProjectInfo(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const info = await this.client.getProjectInfo();

    return {
      content: [
        {
          type: 'text',
          text: `📊 Project Information:\n` +
                `Name: ${info.name}\n` +
                `Canvas Size: ${info.canvas_size.join(' × ')}\n` +
                `Total Voxels: ${info.total_voxels.toLocaleString()}\n` +
                `Layers: ${info.layer_count}\n` +
                `Memory Usage: ${info.memory_usage.total_mb.toFixed(1)}MB\n` +
                `Bounding Box: ${JSON.stringify(info.bounding_box)}\n` +
                `Last Modified: ${info.last_modified}`
        }
      ]
    };
  }

  /**
   * Handle save project tool call
   */
  private async handleSaveProject(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    await this.client.saveProject({
      file_path: args.file_path
    });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Project saved to ${args.file_path}`
        }
      ]
    };
  }

  /**
   * Handle load project tool call
   */
  private async handleLoadProject(args: any): Promise<MCPToolResult> {
    const project = await this.client.loadProject({
      file_path: args.file_path,
      set_active: true
    });

    this.currentProject = project.project_id;

    return {
      content: [
        {
          type: 'text',
          text: `✅ Loaded project "${project.name}" from ${args.file_path}\n` +
                `Project ID: ${project.project_id}\n` +
                `Voxels: ${project.voxel_count?.toLocaleString() || 'Unknown'}\n` +
                `Layers: ${project.layer_count}`
        }
      ]
    };
  }

  /**
   * Handle export model tool call
   */
  private async handleExportModel(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    await this.client.exportModel({
      file_path: args.file_path,
      format: args.format,
      options: {
        include_textures: args.include_textures !== false,
        optimize_mesh: args.optimize_mesh !== false
      }
    });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Exported project to ${args.file_path} in ${args.format.toUpperCase()} format`
        }
      ]
    };
  }

  /**
   * Handle create terrain tool call
   */
  private async handleCreateTerrain(args: any): Promise<MCPToolResult> {
    await this.ensureProject();

    const width = args.width || 32;
    const depth = args.depth || 32;
    const maxHeight = args.max_height || 16;
    const noiseScale = args.noise_scale || 0.1;
    const baseColor = args.base_color || { r: 34, g: 139, b: 34 };

    const voxels = this.generateTerrainVoxels(width, depth, maxHeight, noiseScale, baseColor);
    const result = await this.client.addVoxelBatch({ voxels });

    return {
      content: [
        {
          type: 'text',
          text: `✅ Generated terrain with ${result.processed_count} voxels\n` +
                `Size: ${width} × ${depth}\n` +
                `Max Height: ${maxHeight}\n` +
                `Noise Scale: ${noiseScale}`
        }
      ]
    };
  }

  /**
   * Generate terrain using simple noise function
   */
  private generateTerrainVoxels(
    width: number, 
    depth: number, 
    maxHeight: number, 
    noiseScale: number,
    baseColor: { r: number; g: number; b: number }
  ): Array<{ position: [number, number, number]; color: [number, number, number, number] }> {
    const voxels = [];

    for (let x = -width/2; x < width/2; x++) {
      for (let z = -depth/2; z < depth/2; z++) {
        // Simple pseudo-noise function
        const noise = (Math.sin(x * noiseScale) + Math.cos(z * noiseScale) + 
                      Math.sin((x + z) * noiseScale * 0.5)) / 3;
        const normalizedNoise = (noise + 1) / 2; // Normalize to 0-1
        const height = Math.floor(normalizedNoise * maxHeight);

        for (let y = -20; y < -20 + height; y++) {
          // Vary color based on height
          const heightRatio = (y + 20) / maxHeight;
          let color: [number, number, number, number];

          if (heightRatio < 0.3) {
            color = [139, 69, 19, 255]; // Brown (dirt)
          } else if (heightRatio < 0.7) {
            color = [baseColor.r, baseColor.g, baseColor.b, 255]; // Base color (grass)
          } else {
            color = [128, 128, 128, 255]; // Gray (stone)
          }

          voxels.push({
            position: [x, y, z] as [number, number, number],
            color: color
          });
        }
      }
    }

    return voxels;
  }

  /**
   * Start the MCP server
   */
  async start(): Promise<void> {
    try {
      // Test daemon connection
      await this.ensureConnected();
      console.log('✅ Connected to Goxel daemon');

      const transport = new StdioServerTransport();
      await this.server.connect(transport);
      
      console.log('🚀 Goxel MCP Server started and ready for Claude integration');
    } catch (error) {
      console.error('❌ Failed to start MCP server:', error.message);
      process.exit(1);
    }
  }

  /**
   * Stop the MCP server
   */
  async stop(): Promise<void> {
    try {
      if (this.isConnected) {
        await this.client.disconnect();
      }
      console.log('👋 Goxel MCP Server stopped');
    } catch (error) {
      console.error('⚠️ Error during shutdown:', error.message);
    }
  }
}

// Start the server if this file is executed directly
if (require.main === module) {
  const server = new GoxelMCPServer();
  
  // Handle graceful shutdown
  process.on('SIGINT', async () => {
    console.log('\n🛑 Shutting down MCP server...');
    await server.stop();
    process.exit(0);
  });

  process.on('SIGTERM', async () => {
    console.log('\n🛑 Shutting down MCP server...');
    await server.stop();
    process.exit(0);
  });

  // Start the server
  server.start().catch((error) => {
    console.error('💥 Server failed to start:', error);
    process.exit(1);
  });
}

export { GoxelMCPServer };