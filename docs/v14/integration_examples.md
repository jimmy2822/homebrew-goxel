# Goxel v14.0 Integration Examples

## 🎯 Overview

This guide provides step-by-step integration examples for the Goxel v14.0 Daemon Architecture. Each example demonstrates real-world integration scenarios with complete code samples, error handling, and best practices.

**Integration Scenarios Covered:**
- **MCP Server Integration**: Enhanced Claude AI integration
- **Web Application Integration**: Browser-based voxel editors
- **CLI Tool Integration**: Command-line utilities and scripts
- **Game Engine Integration**: Unity and Unreal Engine plugins
- **API Server Integration**: RESTful API wrapper services
- **Container Deployment**: Docker and Kubernetes integration

## 🚀 Quick Start Integration

### 1. Basic MCP Integration

This example shows how to integrate Goxel daemon with an MCP (Model Context Protocol) server for Claude AI.

```typescript
// mcp-server-goxel-daemon.ts
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { GoxelDaemonClient } from '@goxel/daemon-client';

export class GoxelMCPServer {
  private server: Server;
  private client: GoxelDaemonClient;
  private currentProject: string | null = null;

  constructor() {
    this.server = new Server(
      {
        name: 'goxel-daemon-server',
        version: '14.0.0',
      },
      {
        capabilities: {
          tools: {},
        },
      }
    );

    this.client = new GoxelDaemonClient({
      socketPath: '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 30000,
    });

    this.setupTools();
    this.setupEventHandlers();
  }

  private setupTools() {
    // Create project tool
    this.server.setRequestHandler('tools/call', async (request) => {
      const { name, arguments: args } = request.params;

      switch (name) {
        case 'create_project':
          return await this.handleCreateProject(args);
        case 'add_voxel':
          return await this.handleAddVoxel(args);
        case 'add_voxel_batch':
          return await this.handleAddVoxelBatch(args);
        case 'render_image':
          return await this.handleRenderImage(args);
        case 'export_model':
          return await this.handleExportModel(args);
        case 'save_project':
          return await this.handleSaveProject(args);
        default:
          throw new Error(`Unknown tool: ${name}`);
      }
    });

    // List available tools
    this.server.setRequestHandler('tools/list', async () => {
      return {
        tools: [
          {
            name: 'create_project',
            description: 'Create a new voxel project',
            inputSchema: {
              type: 'object',
              properties: {
                name: { type: 'string', description: 'Project name' },
                template: { 
                  type: 'string', 
                  enum: ['empty', 'cube', 'sphere'],
                  description: 'Project template'
                }
              },
              required: ['name']
            }
          },
          {
            name: 'add_voxel',
            description: 'Add a single voxel to the project',
            inputSchema: {
              type: 'object',
              properties: {
                x: { type: 'number', description: 'X coordinate' },
                y: { type: 'number', description: 'Y coordinate' },
                z: { type: 'number', description: 'Z coordinate' },
                r: { type: 'number', minimum: 0, maximum: 255, description: 'Red component' },
                g: { type: 'number', minimum: 0, maximum: 255, description: 'Green component' },
                b: { type: 'number', minimum: 0, maximum: 255, description: 'Blue component' },
                a: { type: 'number', minimum: 0, maximum: 255, description: 'Alpha component', default: 255 }
              },
              required: ['x', 'y', 'z', 'r', 'g', 'b']
            }
          },
          {
            name: 'add_voxel_batch',
            description: 'Add multiple voxels efficiently',
            inputSchema: {
              type: 'object',
              properties: {
                voxels: {
                  type: 'array',
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
            name: 'render_image',
            description: 'Render the project to an image',
            inputSchema: {
              type: 'object',
              properties: {
                width: { type: 'number', default: 512 },
                height: { type: 'number', default: 512 },
                camera_distance: { type: 'number', default: 30 },
                format: { type: 'string', enum: ['png', 'jpg'], default: 'png' }
              }
            }
          },
          {
            name: 'export_model',
            description: 'Export the project to a 3D model file',
            inputSchema: {
              type: 'object',
              properties: {
                file_path: { type: 'string', description: 'Output file path' },
                format: { 
                  type: 'string', 
                  enum: ['obj', 'ply', 'stl', 'gltf'],
                  description: 'Export format'
                }
              },
              required: ['file_path', 'format']
            }
          },
          {
            name: 'save_project',
            description: 'Save the current project',
            inputSchema: {
              type: 'object',
              properties: {
                file_path: { type: 'string', description: 'Save file path' }
              },
              required: ['file_path']
            }
          }
        ]
      };
    });
  }

  private async handleCreateProject(args: any) {
    try {
      await this.ensureConnected();
      
      const project = await this.client.createProject({
        name: args.name,
        template: args.template || 'empty'
      });
      
      this.currentProject = project.project_id;
      
      return {
        content: [
          {
            type: 'text',
            text: `✅ Created project "${project.name}" with ID: ${project.project_id}`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to create project: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async handleAddVoxel(args: any) {
    try {
      await this.ensureConnected();
      await this.ensureProject();
      
      const result = await this.client.addVoxel({
        position: [args.x, args.y, args.z],
        color: [args.r, args.g, args.b, args.a || 255]
      });
      
      return {
        content: [
          {
            type: 'text',
            text: `✅ Added voxel at (${args.x}, ${args.y}, ${args.z}) with color RGB(${args.r}, ${args.g}, ${args.b})`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to add voxel: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async handleAddVoxelBatch(args: any) {
    try {
      await this.ensureConnected();
      await this.ensureProject();
      
      const voxels = args.voxels.map(v => ({
        position: [v.x, v.y, v.z],
        color: [v.r, v.g, v.b, v.a || 255]
      }));
      
      const result = await this.client.addVoxelBatch({ voxels });
      
      return {
        content: [
          {
            type: 'text',
            text: `✅ Added ${result.processed_count} voxels to the project`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to add voxel batch: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async handleRenderImage(args: any) {
    try {
      await this.ensureConnected();
      await this.ensureProject();
      
      const distance = args.camera_distance || 30;
      const result = await this.client.renderImage({
        width: args.width || 512,
        height: args.height || 512,
        camera: {
          position: [distance, distance, distance],
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
            text: `✅ Rendered ${result.width}x${result.height} ${result.format} image in ${result.render_time_ms}ms`
          },
          {
            type: 'image',
            data: result.image_data,
            mimeType: `image/${result.format}`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to render image: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async handleExportModel(args: any) {
    try {
      await this.ensureConnected();
      await this.ensureProject();
      
      await this.client.exportModel({
        file_path: args.file_path,
        format: args.format
      });
      
      return {
        content: [
          {
            type: 'text',
            text: `✅ Exported project to ${args.file_path} in ${args.format} format`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to export model: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async handleSaveProject(args: any) {
    try {
      await this.ensureConnected();
      await this.ensureProject();
      
      await this.client.saveProject({
        file_path: args.file_path
      });
      
      return {
        content: [
          {
            type: 'text',
            text: `✅ Saved project to ${args.file_path}`
          }
        ]
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: `❌ Failed to save project: ${error.message}`
          }
        ],
        isError: true
      };
    }
  }

  private async ensureConnected() {
    if (!this.client.isConnected()) {
      await this.client.connect();
    }
  }

  private async ensureProject() {
    if (!this.currentProject) {
      throw new Error('No project loaded. Please create a project first.');
    }
  }

  private setupEventHandlers() {
    this.client.on('disconnect', () => {
      console.log('Daemon connection lost');
    });

    this.client.on('reconnect', () => {
      console.log('Daemon connection restored');
    });
  }

  async start() {
    const transport = new StdioServerTransport();
    await this.server.connect(transport);
    console.log('Goxel MCP Server started');
  }

  async stop() {
    await this.client.disconnect();
    console.log('Goxel MCP Server stopped');
  }
}

// Start the server
if (require.main === module) {
  const server = new GoxelMCPServer();
  server.start().catch(console.error);
  
  process.on('SIGINT', async () => {
    await server.stop();
    process.exit(0);
  });
}
```

### 2. Web Application Integration

React-based web application that communicates with Goxel daemon via WebSocket proxy.

```typescript
// web-voxel-editor/src/GoxelClient.ts
import { GoxelDaemonClient } from '@goxel/daemon-client';

export class WebGoxelClient {
  private client: GoxelDaemonClient;
  private wsProxy: WebSocket | null = null;
  
  constructor(private wsUrl: string = 'ws://localhost:8081') {
    // Note: Web applications can't directly connect to Unix sockets
    // A WebSocket proxy is needed to bridge the connection
    this.client = new GoxelDaemonClient({
      tcp: { host: 'localhost', port: 8080 }, // TCP daemon connection
      autoReconnect: true
    });
  }
  
  async connect(): Promise<void> {
    // Connect to WebSocket proxy that forwards to daemon
    this.wsProxy = new WebSocket(this.wsUrl);
    
    return new Promise((resolve, reject) => {
      this.wsProxy!.onopen = () => resolve();
      this.wsProxy!.onerror = (error) => reject(error);
      this.wsProxy!.onmessage = this.handleMessage.bind(this);
    });
  }
  
  private handleMessage(event: MessageEvent) {
    const response = JSON.parse(event.data);
    // Handle daemon responses
    console.log('Daemon response:', response);
  }
  
  async createVoxelModel(voxels: Array<{x: number, y: number, z: number, color: string}>) {
    const project = await this.client.createProject({
      name: 'Web Created Model',
      template: 'empty'
    });
    
    // Convert hex colors to RGBA
    const voxelData = voxels.map(v => ({
      position: [v.x, v.y, v.z] as [number, number, number],
      color: this.hexToRgba(v.color)
    }));
    
    await this.client.addVoxelBatch({ voxels: voxelData });
    
    return project;
  }
  
  private hexToRgba(hex: string): [number, number, number, number] {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return [r, g, b, 255];
  }
}

// web-voxel-editor/src/VoxelEditor.tsx
import React, { useState, useEffect } from 'react';
import { WebGoxelClient } from './GoxelClient';

interface Voxel {
  x: number;
  y: number;
  z: number;
  color: string;
}

export const VoxelEditor: React.FC = () => {
  const [client, setClient] = useState<WebGoxelClient | null>(null);
  const [voxels, setVoxels] = useState<Voxel[]>([]);
  const [selectedColor, setSelectedColor] = useState('#ff0000');
  const [isConnected, setIsConnected] = useState(false);
  
  useEffect(() => {
    const goxelClient = new WebGoxelClient();
    setClient(goxelClient);
    
    goxelClient.connect()
      .then(() => setIsConnected(true))
      .catch(console.error);
  }, []);
  
  const addVoxel = (x: number, y: number, z: number) => {
    const newVoxel = { x, y, z, color: selectedColor };
    setVoxels(prev => [...prev, newVoxel]);
  };
  
  const generateModel = async () => {
    if (!client || !isConnected) return;
    
    try {
      const project = await client.createVoxelModel(voxels);
      console.log('Model created:', project);
      
      // Render preview
      const render = await client.renderImage({
        width: 400,
        height: 400,
        camera: {
          position: [20, 20, 20],
          target: [0, -16, 0],
          up: [0, 1, 0],
          fov: 45
        }
      });
      
      // Display rendered image
      const img = document.getElementById('preview') as HTMLImageElement;
      if (img) {
        img.src = `data:image/png;base64,${render.image_data}`;
      }
    } catch (error) {
      console.error('Failed to generate model:', error);
    }
  };
  
  return (
    <div className="voxel-editor">
      <h1>Web Voxel Editor</h1>
      <div className="status">
        Status: {isConnected ? '✅ Connected' : '❌ Disconnected'}
      </div>
      
      <div className="controls">
        <input
          type="color"
          value={selectedColor}
          onChange={(e) => setSelectedColor(e.target.value)}
        />
        <button onClick={() => addVoxel(0, -16, 0)}>Add Voxel at Origin</button>
        <button onClick={generateModel}>Generate 3D Model</button>
      </div>
      
      <div className="voxel-list">
        <h3>Voxels ({voxels.length})</h3>
        {voxels.map((voxel, index) => (
          <div key={index} className="voxel-item">
            <span style={{ backgroundColor: voxel.color, width: 20, height: 20, display: 'inline-block' }}></span>
            Position: ({voxel.x}, {voxel.y}, {voxel.z})
          </div>
        ))}
      </div>
      
      <div className="preview">
        <h3>Preview</h3>
        <img id="preview" alt="Voxel model preview" />
      </div>
    </div>
  );
};
```

### 3. CLI Tool Integration

Command-line utility that uses the daemon for batch processing.

```typescript
#!/usr/bin/env node
// voxel-cli-tool.ts

import { Command } from 'commander';
import { GoxelDaemonClient } from '@goxel/daemon-client';
import * as fs from 'fs';
import * as path from 'path';

class VoxelCLI {
  private client: GoxelDaemonClient;
  
  constructor() {
    this.client = new GoxelDaemonClient({
      socketPath: process.env.GOXEL_DAEMON_SOCKET || '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 60000 // Longer timeout for CLI operations
    });
  }
  
  async ensureConnected() {
    if (!this.client.isConnected()) {
      console.log('Connecting to Goxel daemon...');
      await this.client.connect();
      console.log('✅ Connected to daemon');
    }
  }
  
  async generateFromCSV(csvFile: string, outputFile: string) {
    await this.ensureConnected();
    
    console.log(`Reading voxel data from ${csvFile}...`);
    const csvContent = fs.readFileSync(csvFile, 'utf-8');
    const lines = csvContent.trim().split('\n');
    const headers = lines[0].split(',');
    
    // Parse CSV data
    const voxels = lines.slice(1).map(line => {
      const values = line.split(',');
      return {
        position: [
          parseInt(values[0]), // x
          parseInt(values[1]), // y
          parseInt(values[2])  // z
        ] as [number, number, number],
        color: [
          parseInt(values[3]), // r
          parseInt(values[4]), // g
          parseInt(values[5]), // b
          parseInt(values[6]) || 255 // a (optional)
        ] as [number, number, number, number]
      };
    });
    
    console.log(`Found ${voxels.length} voxels to process`);
    
    // Create project
    const project = await this.client.createProject({
      name: path.basename(csvFile, '.csv'),
      template: 'empty'
    });
    
    console.log(`Created project: ${project.name}`);
    
    // Add voxels in batches
    const batchSize = 1000;
    for (let i = 0; i < voxels.length; i += batchSize) {
      const batch = voxels.slice(i, i + batchSize);
      await this.client.addVoxelBatch({ voxels: batch });
      
      const progress = Math.round(((i + batch.length) / voxels.length) * 100);
      process.stdout.write(`\rProgress: ${progress}%`);
    }
    
    console.log('\n✅ All voxels added');
    
    // Save project
    await this.client.saveProject({ file_path: outputFile });
    console.log(`✅ Saved project to ${outputFile}`);
    
    // Get project info
    const info = await this.client.getProjectInfo();
    console.log(`Final project: ${info.total_voxels} voxels, ${info.memory_usage.total_mb}MB`);
  }
  
  async renderMultiAngle(projectFile: string, outputDir: string) {
    await this.ensureConnected();
    
    console.log(`Loading project from ${projectFile}...`);
    await this.client.loadProject({ file_path: projectFile });
    
    const angles = [
      { name: 'front', position: [0, 0, 30] },
      { name: 'back', position: [0, 0, -30] },
      { name: 'left', position: [-30, 0, 0] },
      { name: 'right', position: [30, 0, 0] },
      { name: 'top', position: [0, 30, 0] },
      { name: 'bottom', position: [0, -30, 0] },
      { name: 'iso', position: [20, 20, 20] }
    ];
    
    fs.mkdirSync(outputDir, { recursive: true });
    
    console.log(`Rendering ${angles.length} views...`);
    
    for (let i = 0; i < angles.length; i++) {
      const angle = angles[i];
      
      const result = await this.client.renderImage({
        width: 1024,
        height: 1024,
        camera: {
          position: angle.position as [number, number, number],
          target: [0, -16, 0],
          up: [0, 1, 0],
          fov: 45
        },
        format: 'png'
      });
      
      const outputPath = path.join(outputDir, `${angle.name}.png`);
      const buffer = Buffer.from(result.image_data, 'base64');
      fs.writeFileSync(outputPath, buffer);
      
      const progress = Math.round(((i + 1) / angles.length) * 100);
      console.log(`✅ Rendered ${angle.name} view (${progress}%)`);
    }
    
    console.log(`✅ All renders saved to ${outputDir}`);
  }
  
  async batchExport(projectFile: string, formats: string[], outputDir: string) {
    await this.ensureConnected();
    
    console.log(`Loading project from ${projectFile}...`);
    await this.client.loadProject({ file_path: projectFile });
    
    fs.mkdirSync(outputDir, { recursive: true });
    
    const projectName = path.basename(projectFile, path.extname(projectFile));
    
    for (const format of formats) {
      console.log(`Exporting to ${format.toUpperCase()}...`);
      
      const outputPath = path.join(outputDir, `${projectName}.${format}`);
      
      await this.client.exportModel({
        file_path: outputPath,
        format: format as any
      });
      
      console.log(`✅ Exported to ${outputPath}`);
    }
  }
  
  async disconnect() {
    await this.client.disconnect();
  }
}

// CLI setup
const program = new Command();
const cli = new VoxelCLI();

program
  .name('voxel-cli')
  .description('Command-line tool for Goxel voxel operations')
  .version('1.0.0');

program
  .command('generate-from-csv')
  .description('Generate voxel model from CSV file')
  .argument('<csv-file>', 'CSV file with voxel data (x,y,z,r,g,b,a)')
  .argument('<output-file>', 'Output .gox file')
  .action(async (csvFile, outputFile) => {
    try {
      await cli.generateFromCSV(csvFile, outputFile);
    } catch (error) {
      console.error('❌ Error:', error.message);
      process.exit(1);
    } finally {
      await cli.disconnect();
    }
  });

program
  .command('render-multi')
  .description('Render project from multiple angles')
  .argument('<project-file>', 'Project file (.gox)')
  .argument('<output-dir>', 'Output directory for renders')
  .action(async (projectFile, outputDir) => {
    try {
      await cli.renderMultiAngle(projectFile, outputDir);
    } catch (error) {
      console.error('❌ Error:', error.message);
      process.exit(1);
    } finally {
      await cli.disconnect();
    }
  });

program
  .command('batch-export')
  .description('Export project to multiple formats')
  .argument('<project-file>', 'Project file (.gox)')
  .option('-f, --formats <formats>', 'Export formats (comma-separated)', 'obj,ply,stl')
  .argument('<output-dir>', 'Output directory')
  .action(async (projectFile, options, outputDir) => {
    try {
      const formats = options.formats.split(',');
      await cli.batchExport(projectFile, formats, outputDir);
    } catch (error) {
      console.error('❌ Error:', error.message);
      process.exit(1);
    } finally {
      await cli.disconnect();
    }
  });

program.parse();
```

### 4. Unity Game Engine Integration

Unity plugin for runtime voxel generation using Goxel daemon.

```csharp
// Unity/Assets/Scripts/GoxelDaemonClient.cs
using System;
using System.Collections;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;
using Newtonsoft.Json;

public class GoxelDaemonClient : MonoBehaviour
{
    [Header("Connection Settings")]
    public string daemonHost = "localhost";
    public int daemonPort = 8080;
    public float connectionTimeout = 30f;
    
    [Header("Runtime Settings")]
    public bool autoConnect = true;
    public bool enableDebugLogs = true;
    
    private TcpClient tcpClient;
    private NetworkStream stream;
    private bool isConnected = false;
    private int requestIdCounter = 0;
    
    // Events
    public event Action OnConnected;
    public event Action OnDisconnected;
    public event Action<string> OnError;
    
    private void Start()
    {
        if (autoConnect)
        {
            StartCoroutine(ConnectAsync());
        }
    }
    
    public IEnumerator ConnectAsync()
    {
        if (isConnected) yield break;
        
        LogDebug($"Connecting to Goxel daemon at {daemonHost}:{daemonPort}");
        
        try
        {
            tcpClient = new TcpClient();
            var connectTask = tcpClient.ConnectAsync(daemonHost, daemonPort);
            
            float timer = 0f;
            while (!connectTask.IsCompleted && timer < connectionTimeout)
            {
                timer += Time.deltaTime;
                yield return null;
            }
            
            if (!connectTask.IsCompleted)
            {
                throw new TimeoutException($"Connection timeout after {connectionTimeout}s");
            }
            
            if (connectTask.Exception != null)
            {
                throw connectTask.Exception.InnerException;
            }
            
            stream = tcpClient.GetStream();
            isConnected = true;
            
            LogDebug("✅ Connected to Goxel daemon");
            OnConnected?.Invoke();
        }
        catch (Exception ex)
        {
            LogError($"Failed to connect: {ex.Message}");
            OnError?.Invoke(ex.Message);
        }
    }
    
    public async Task<GoxelResponse<ProjectInfo>> CreateProjectAsync(string name, string template = "empty")
    {
        var request = new GoxelRequest
        {
            jsonrpc = "2.0",
            method = "goxel.create_project",
            parameters = new
            {
                name = name,
                template = template
            },
            id = GetNextRequestId()
        };
        
        return await SendRequestAsync<ProjectInfo>(request);
    }
    
    public async Task<GoxelResponse<VoxelResult>> AddVoxelAsync(Vector3Int position, Color32 color)
    {
        var request = new GoxelRequest
        {
            jsonrpc = "2.0",
            method = "goxel.add_voxel",
            parameters = new
            {
                position = new int[] { position.x, position.y, position.z },
                color = new int[] { color.r, color.g, color.b, color.a }
            },
            id = GetNextRequestId()
        };
        
        return await SendRequestAsync<VoxelResult>(request);
    }
    
    public async Task<GoxelResponse<BatchResult>> AddVoxelBatchAsync(VoxelData[] voxels)
    {
        var voxelArray = Array.ConvertAll(voxels, v => new
        {
            position = new int[] { v.position.x, v.position.y, v.position.z },
            color = new int[] { v.color.r, v.color.g, v.color.b, v.color.a }
        });
        
        var request = new GoxelRequest
        {
            jsonrpc = "2.0",
            method = "goxel.add_voxel_batch",
            parameters = new
            {
                voxels = voxelArray
            },
            id = GetNextRequestId()
        };
        
        return await SendRequestAsync<BatchResult>(request);
    }
    
    public async Task<GoxelResponse<RenderResult>> RenderImageAsync(int width, int height, Vector3 cameraPos, Vector3 target)
    {
        var request = new GoxelRequest
        {
            jsonrpc = "2.0",
            method = "goxel.render_image",
            parameters = new
            {
                width = width,
                height = height,
                camera = new
                {
                    position = new float[] { cameraPos.x, cameraPos.y, cameraPos.z },
                    target = new float[] { target.x, target.y, target.z },
                    up = new float[] { 0f, 1f, 0f },
                    fov = 45f
                },
                format = "png"
            },
            id = GetNextRequestId()
        };
        
        return await SendRequestAsync<RenderResult>(request);
    }
    
    private async Task<GoxelResponse<T>> SendRequestAsync<T>(GoxelRequest request)
    {
        if (!isConnected)
        {
            throw new InvalidOperationException("Not connected to daemon");
        }
        
        try
        {
            string json = JsonConvert.SerializeObject(request);
            byte[] data = Encoding.UTF8.GetBytes(json + "\n");
            
            await stream.WriteAsync(data, 0, data.Length);
            
            // Read response
            byte[] buffer = new byte[4096];
            int bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length);
            string responseJson = Encoding.UTF8.GetString(buffer, 0, bytesRead);
            
            return JsonConvert.DeserializeObject<GoxelResponse<T>>(responseJson);
        }
        catch (Exception ex)
        {
            LogError($"Request failed: {ex.Message}");
            throw;
        }
    }
    
    private int GetNextRequestId()
    {
        return ++requestIdCounter;
    }
    
    private void LogDebug(string message)
    {
        if (enableDebugLogs)
        {
            Debug.Log($"[GoxelDaemon] {message}");
        }
    }
    
    private void LogError(string message)
    {
        Debug.LogError($"[GoxelDaemon] {message}");
    }
    
    private void OnDestroy()
    {
        if (isConnected)
        {
            stream?.Close();
            tcpClient?.Close();
        }
    }
}

// Data structures
[Serializable]
public class GoxelRequest
{
    public string jsonrpc;
    public string method;
    public object parameters;
    public int id;
}

[Serializable]
public class GoxelResponse<T>
{
    public string jsonrpc;
    public T result;
    public GoxelError error;
    public int id;
}

[Serializable]
public class GoxelError
{
    public int code;
    public string message;
    public object data;
}

[Serializable]
public class ProjectInfo
{
    public string project_id;
    public string name;
    public int[] canvas_size;
    public int layer_count;
    public int total_voxels;
}

[Serializable]
public class VoxelResult
{
    public bool success;
    public int voxel_count;
}

[Serializable]
public class BatchResult
{
    public bool success;
    public int processed_count;
    public int failed_count;
}

[Serializable]
public class RenderResult
{
    public string image_data;  // Base64
    public string format;
    public int width;
    public int height;
    public float render_time_ms;
}

[Serializable]
public struct VoxelData
{
    public Vector3Int position;
    public Color32 color;
    
    public VoxelData(Vector3Int pos, Color32 col)
    {
        position = pos;
        color = col;
    }
}

// Unity/Assets/Scripts/VoxelTerrain.cs
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class VoxelTerrain : MonoBehaviour
{
    [Header("Terrain Settings")]
    public int terrainSize = 64;
    public float noiseScale = 0.1f;
    public int maxHeight = 32;
    
    [Header("Rendering")]
    public Material previewMaterial;
    public GameObject previewQuad;
    
    private GoxelDaemonClient goxelClient;
    private string currentProjectId;
    
    private void Start()
    {
        goxelClient = FindObjectOfType<GoxelDaemonClient>();
        if (goxelClient == null)
        {
            Debug.LogError("GoxelDaemonClient not found!");
            return;
        }
        
        goxelClient.OnConnected += OnDaemonConnected;
    }
    
    private async void OnDaemonConnected()
    {
        Debug.Log("Daemon connected, creating terrain project...");
        
        var projectResponse = await goxelClient.CreateProjectAsync("Unity Terrain", "empty");
        if (projectResponse.error != null)
        {
            Debug.LogError($"Failed to create project: {projectResponse.error.message}");
            return;
        }
        
        currentProjectId = projectResponse.result.project_id;
        Debug.Log($"Created project: {currentProjectId}");
        
        StartCoroutine(GenerateTerrainCoroutine());
    }
    
    private IEnumerator GenerateTerrainCoroutine()
    {
        Debug.Log("Generating voxel terrain...");
        
        var voxels = new List<VoxelData>();
        
        // Generate heightmap using Perlin noise
        for (int x = -terrainSize/2; x < terrainSize/2; x++)
        {
            for (int z = -terrainSize/2; z < terrainSize/2; z++)
            {
                float noiseValue = Mathf.PerlinNoise(x * noiseScale, z * noiseScale);
                int height = Mathf.RoundToInt(noiseValue * maxHeight);
                
                for (int y = -20; y < -20 + height; y++)
                {
                    Color32 color = GetTerrainColor(y, height);
                    voxels.Add(new VoxelData(new Vector3Int(x, y, z), color));
                }
            }
            
            // Yield every few columns to prevent frame drops
            if (x % 4 == 0)
            {
                yield return null;
            }
        }
        
        Debug.Log($"Generated {voxels.Count} voxels, adding to project...");
        
        // Add voxels in batches
        const int batchSize = 1000;
        for (int i = 0; i < voxels.Count; i += batchSize)
        {
            var batch = new VoxelData[Mathf.Min(batchSize, voxels.Count - i)];
            for (int j = 0; j < batch.Length; j++)
            {
                batch[j] = voxels[i + j];
            }
            
            var result = await goxelClient.AddVoxelBatchAsync(batch);
            if (result.error != null)
            {
                Debug.LogError($"Batch failed: {result.error.message}");
                yield break;
            }
            
            float progress = (float)(i + batch.Length) / voxels.Count;
            Debug.Log($"Progress: {progress:P0}");
            
            yield return null;
        }
        
        Debug.Log("✅ Terrain generation complete!");
        
        // Render preview
        yield return StartCoroutine(RenderPreviewCoroutine());
    }
    
    private IEnumerator RenderPreviewCoroutine()
    {
        Debug.Log("Rendering terrain preview...");
        
        var renderTask = goxelClient.RenderImageAsync(512, 512, 
            new Vector3(30, 30, 30), 
            new Vector3(0, -16, 0));
        
        yield return new WaitUntil(() => renderTask.IsCompleted);
        
        if (renderTask.Exception != null)
        {
            Debug.LogError($"Render failed: {renderTask.Exception.Message}");
            yield break;
        }
        
        var renderResult = renderTask.Result;
        if (renderResult.error != null)
        {
            Debug.LogError($"Render error: {renderResult.error.message}");
            yield break;
        }
        
        // Convert base64 to texture
        byte[] imageData = System.Convert.FromBase64String(renderResult.result.image_data);
        Texture2D texture = new Texture2D(2, 2);
        texture.LoadImage(imageData);
        
        // Apply to preview quad
        if (previewQuad != null)
        {
            var renderer = previewQuad.GetComponent<Renderer>();
            if (renderer != null)
            {
                renderer.material.mainTexture = texture;
            }
        }
        
        Debug.Log($"✅ Preview rendered in {renderResult.result.render_time_ms}ms");
    }
    
    private Color32 GetTerrainColor(int y, int maxHeight)
    {
        float heightRatio = (float)y / maxHeight;
        
        if (heightRatio < 0.3f)
        {
            return new Color32(139, 69, 19, 255);  // Brown (dirt)
        }
        else if (heightRatio < 0.7f)
        {
            return new Color32(34, 139, 34, 255);  // Green (grass)
        }
        else
        {
            return new Color32(128, 128, 128, 255); // Gray (stone)
        }
    }
}
```

### 5. RESTful API Wrapper

Express.js server that provides REST API over the Goxel daemon.

```typescript
// rest-api-server/src/server.ts
import express from 'express';
import cors from 'cors';
import { GoxelDaemonClient } from '@goxel/daemon-client';
import { body, param, query, validationResult } from 'express-validator';
import rateLimit from 'express-rate-limit';

export class GoxelRESTServer {
  private app: express.Application;
  private client: GoxelDaemonClient;
  private currentProject: string | null = null;
  
  constructor(private port: number = 3000) {
    this.app = express();
    this.client = new GoxelDaemonClient({
      socketPath: process.env.GOXEL_DAEMON_SOCKET || '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 60000
    });
    
    this.setupMiddleware();
    this.setupRoutes();
  }
  
  private setupMiddleware() {
    // CORS
    this.app.use(cors());
    
    // JSON parsing
    this.app.use(express.json({ limit: '10mb' }));
    
    // Rate limiting
    const limiter = rateLimit({
      windowMs: 15 * 60 * 1000, // 15 minutes
      max: 1000 // limit each IP to 1000 requests per windowMs
    });
    this.app.use(limiter);
    
    // Request logging
    this.app.use((req, res, next) => {
      console.log(`${new Date().toISOString()} ${req.method} ${req.path}`);
      next();
    });
  }
  
  private setupRoutes() {
    // Health check
    this.app.get('/health', (req, res) => {
      res.json({ 
        status: 'ok', 
        daemon_connected: this.client.isConnected(),
        timestamp: new Date().toISOString()
      });
    });
    
    // Daemon status
    this.app.get('/api/daemon/status', async (req, res) => {
      try {
        await this.ensureConnected();
        const status = await this.client.getStatus();
        res.json(status);
      } catch (error) {
        res.status(500).json({ error: error.message });
      }
    });
    
    // Project operations
    this.app.post('/api/projects', 
      body('name').isString().isLength({ min: 1, max: 100 }),
      body('template').optional().isIn(['empty', 'cube', 'sphere']),
      async (req, res) => {
        const errors = validationResult(req);
        if (!errors.isEmpty()) {
          return res.status(400).json({ errors: errors.array() });
        }
        
        try {
          await this.ensureConnected();
          const project = await this.client.createProject(req.body);
          this.currentProject = project.project_id;
          res.status(201).json(project);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    this.app.get('/api/projects/:projectId', async (req, res) => {
      try {
        await this.ensureConnected();
        const info = await this.client.getProjectInfo(req.params.projectId);
        res.json(info);
      } catch (error) {
        res.status(500).json({ error: error.message });
      }
    });
    
    this.app.post('/api/projects/:projectId/load',
      body('file_path').isString(),
      async (req, res) => {
        try {
          await this.ensureConnected();
          const project = await this.client.loadProject({
            file_path: req.body.file_path,
            set_active: true
          });
          this.currentProject = project.project_id;
          res.json(project);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    this.app.post('/api/projects/:projectId/save',
      body('file_path').isString(),
      body('format').optional().isIn(['gox', 'vox', 'obj', 'ply', 'stl']),
      async (req, res) => {
        try {
          await this.ensureConnected();
          await this.client.saveProject({
            project_id: req.params.projectId,
            file_path: req.body.file_path,
            format: req.body.format
          });
          res.json({ success: true });
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    // Voxel operations
    this.app.post('/api/projects/:projectId/voxels',
      body('position').isArray({ min: 3, max: 3 }),
      body('position.*').isInt(),
      body('color').isArray({ min: 3, max: 4 }),
      body('color.*').isInt({ min: 0, max: 255 }),
      async (req, res) => {
        const errors = validationResult(req);
        if (!errors.isEmpty()) {
          return res.status(400).json({ errors: errors.array() });
        }
        
        try {
          await this.ensureConnected();
          const result = await this.client.addVoxel({
            position: req.body.position,
            color: req.body.color.length === 3 ? 
              [...req.body.color, 255] : req.body.color,
            project_id: req.params.projectId
          });
          res.status(201).json(result);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    this.app.post('/api/projects/:projectId/voxels/batch',
      body('voxels').isArray({ min: 1, max: 10000 }),
      body('voxels.*.position').isArray({ min: 3, max: 3 }),
      body('voxels.*.color').isArray({ min: 3, max: 4 }),
      async (req, res) => {
        const errors = validationResult(req);
        if (!errors.isEmpty()) {
          return res.status(400).json({ errors: errors.array() });
        }
        
        try {
          await this.ensureConnected();
          
          // Normalize colors to RGBA
          const voxels = req.body.voxels.map(v => ({
            position: v.position,
            color: v.color.length === 3 ? [...v.color, 255] : v.color
          }));
          
          const result = await this.client.addVoxelBatch({
            voxels,
            project_id: req.params.projectId
          });
          
          res.status(201).json(result);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    this.app.get('/api/projects/:projectId/voxels/:x/:y/:z', 
      param(['x', 'y', 'z']).isInt(),
      async (req, res) => {
        try {
          await this.ensureConnected();
          const voxel = await this.client.getVoxel({
            position: [parseInt(req.params.x), parseInt(req.params.y), parseInt(req.params.z)],
            project_id: req.params.projectId
          });
          res.json(voxel);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    // Rendering
    this.app.post('/api/projects/:projectId/render',
      body('width').optional().isInt({ min: 64, max: 4096 }),
      body('height').optional().isInt({ min: 64, max: 4096 }),
      body('camera').optional().isObject(),
      body('format').optional().isIn(['png', 'jpg']),
      async (req, res) => {
        try {
          await this.ensureConnected();
          
          const renderParams = {
            width: req.body.width || 512,
            height: req.body.height || 512,
            camera: req.body.camera || {
              position: [20, 20, 20],
              target: [0, -16, 0],
              up: [0, 1, 0],
              fov: 45
            },
            format: req.body.format || 'png',
            project_id: req.params.projectId
          };
          
          const result = await this.client.renderImage(renderParams);
          
          // Return image data with proper headers
          res.set({
            'Content-Type': `image/${result.format}`,
            'X-Render-Time': result.render_time_ms.toString(),
            'X-Image-Size': `${result.width}x${result.height}`
          });
          
          if (req.query.download === 'true') {
            res.set('Content-Disposition', `attachment; filename="render.${result.format}"`);
          }
          
          // Convert base64 to buffer and send
          const buffer = Buffer.from(result.image_data, 'base64');
          res.send(buffer);
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    // Export
    this.app.post('/api/projects/:projectId/export',
      body('format').isIn(['obj', 'ply', 'stl', 'gltf', 'vox']),
      body('file_path').isString(),
      async (req, res) => {
        try {
          await this.ensureConnected();
          
          await this.client.exportModel({
            file_path: req.body.file_path,
            format: req.body.format,
            project_id: req.params.projectId
          });
          
          res.json({ 
            success: true, 
            file_path: req.body.file_path,
            format: req.body.format
          });
        } catch (error) {
          res.status(500).json({ error: error.message });
        }
      }
    );
    
    // Error handling
    this.app.use((error, req, res, next) => {
      console.error('Unhandled error:', error);
      res.status(500).json({ 
        error: 'Internal server error',
        message: error.message 
      });
    });
    
    // 404 handler
    this.app.use((req, res) => {
      res.status(404).json({ error: 'Not found' });
    });
  }
  
  private async ensureConnected() {
    if (!this.client.isConnected()) {
      await this.client.connect();
    }
  }
  
  async start() {
    try {
      await this.client.connect();
      console.log('✅ Connected to Goxel daemon');
      
      this.app.listen(this.port, () => {
        console.log(`🚀 Goxel REST API server running on port ${this.port}`);
        console.log(`📖 API docs: http://localhost:${this.port}/health`);
      });
    } catch (error) {
      console.error('❌ Failed to start server:', error);
      process.exit(1);
    }
  }
  
  async stop() {
    await this.client.disconnect();
    console.log('Server stopped');
  }
}

// Start server
if (require.main === module) {
  const server = new GoxelRESTServer(parseInt(process.env.PORT || '3000'));
  
  server.start().catch(console.error);
  
  process.on('SIGINT', async () => {
    console.log('\nShutting down...');
    await server.stop();
    process.exit(0);
  });
}
```

### 6. Docker Container Integration

Complete containerized deployment with daemon and web interface.

```dockerfile
# Dockerfile.daemon
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    scons \
    pkg-config \
    libglfw3-dev \
    libgtk-3-dev \
    libpng-dev \
    libosmesa6-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy Goxel source
COPY . /goxel
WORKDIR /goxel

# Build daemon
RUN scons mode=release headless=1 daemon=1

# Create runtime user
RUN useradd -m -s /bin/bash goxel

# Set up socket directory
RUN mkdir -p /tmp/goxel && chown goxel:goxel /tmp/goxel

USER goxel

# Expose daemon port
EXPOSE 8080

# Start daemon
CMD ["./goxel-daemon", "--tcp", "--port=8080", "--bind=0.0.0.0"]
```

```yaml
# docker-compose.yml
version: '3.8'

services:
  goxel-daemon:
    build:
      context: .
      dockerfile: Dockerfile.daemon
    ports:
      - "8080:8080"
    volumes:
      - goxel-projects:/data
    environment:
      - GOXEL_LOG_LEVEL=info
      - GOXEL_MAX_CONNECTIONS=50
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    restart: unless-stopped

  goxel-api:
    build:
      context: ./rest-api-server
      dockerfile: Dockerfile
    ports:
      - "3000:3000"
    depends_on:
      - goxel-daemon
    environment:
      - GOXEL_DAEMON_HOST=goxel-daemon
      - GOXEL_DAEMON_PORT=8080
      - PORT=3000
    volumes:
      - goxel-projects:/data
    restart: unless-stopped

  goxel-web:
    build:
      context: ./web-interface
      dockerfile: Dockerfile
    ports:
      - "80:80"
    depends_on:
      - goxel-api
    environment:
      - REACT_APP_API_URL=http://localhost:3000
    restart: unless-stopped

volumes:
  goxel-projects:
```

```bash
#!/bin/bash
# deploy.sh - Production deployment script

set -e

echo "🚀 Deploying Goxel v14.0 Daemon Architecture..."

# Build and start services
docker-compose build
docker-compose up -d

# Wait for services to be ready
echo "⏳ Waiting for services to start..."
sleep 10

# Health checks
echo "🔍 Checking service health..."

# Check daemon
if curl -f http://localhost:8080/health > /dev/null 2>&1; then
    echo "✅ Goxel daemon is healthy"
else
    echo "❌ Goxel daemon health check failed"
    exit 1
fi

# Check API
if curl -f http://localhost:3000/health > /dev/null 2>&1; then
    echo "✅ REST API is healthy"
else
    echo "❌ REST API health check failed"
    exit 1
fi

# Check web interface
if curl -f http://localhost/ > /dev/null 2>&1; then
    echo "✅ Web interface is healthy"
else
    echo "❌ Web interface health check failed"
    exit 1
fi

echo "🎉 Deployment successful!"
echo "📊 Dashboard: http://localhost/"
echo "🔧 API: http://localhost:3000/"
echo "⚙️ Daemon: http://localhost:8080/"

# Show logs
echo "📋 Recent logs:"
docker-compose logs --tail=20
```

---

## 📋 Integration Checklist

### ✅ MCP Integration
- [ ] Enhanced MCP server with daemon client
- [ ] All Goxel operations available as MCP tools
- [ ] Proper error handling and recovery
- [ ] Performance monitoring integration

### ✅ Web Application Integration  
- [ ] TypeScript client for browser use
- [ ] WebSocket proxy for daemon connection
- [ ] React components for voxel editing
- [ ] Real-time rendering preview

### ✅ CLI Tool Integration
- [ ] Command-line utility with daemon client
- [ ] CSV data processing
- [ ] Batch operations and multi-angle rendering
- [ ] Export to multiple formats

### ✅ Unity Game Engine Integration
- [ ] C# daemon client for Unity
- [ ] Runtime voxel generation
- [ ] Terrain generation example
- [ ] Async operation handling

### ✅ RESTful API Integration
- [ ] Express.js server with full REST API
- [ ] Input validation and rate limiting
- [ ] File upload/download support
- [ ] Comprehensive error handling

### ✅ Container Deployment
- [ ] Docker images for all components
- [ ] Docker Compose orchestration
- [ ] Health checks and monitoring
- [ ] Production deployment scripts

---

*These integration examples provide comprehensive templates for connecting various applications and platforms to the Goxel v14.0 Daemon Architecture. Each example includes complete code, error handling, and best practices for production use.*

**Last Updated**: January 26, 2025  
**Version**: 14.0.0-dev  
**Status**: 📋 Template Ready for Implementation