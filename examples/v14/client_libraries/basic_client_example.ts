/**
 * Basic Goxel v14.0 Daemon Client Example
 * 
 * This example demonstrates the fundamental usage of the Goxel daemon client
 * including connection management, basic voxel operations, and error handling.
 */

import { GoxelDaemonClient } from '@goxel/daemon-client';

interface VoxelArtConfig {
  name: string;
  size: [number, number, number];
  colors: Array<[number, number, number, number]>;
}

class BasicVoxelArtist {
  private client: GoxelDaemonClient;
  private currentProject: string | null = null;

  constructor() {
    this.client = new GoxelDaemonClient({
      socketPath: '/tmp/goxel-daemon.sock',
      autoReconnect: true,
      timeout: 30000,
      enableDebugLogging: false
    });
  }

  /**
   * Initialize connection to daemon
   */
  async connect(): Promise<void> {
    try {
      await this.client.connect();
      console.log('✅ Connected to Goxel daemon');
    } catch (error) {
      console.error('❌ Failed to connect to daemon:', error.message);
      throw error;
    }
  }

  /**
   * Create a new voxel art project
   */
  async createProject(config: VoxelArtConfig): Promise<string> {
    try {
      const project = await this.client.createProject({
        name: config.name,
        template: 'empty',
        size: config.size
      });

      this.currentProject = project.project_id;
      console.log(`📦 Created project: ${project.name} (${project.project_id})`);
      
      return project.project_id;
    } catch (error) {
      console.error('❌ Failed to create project:', error.message);
      throw error;
    }
  }

  /**
   * Add a single voxel with error handling
   */
  async addVoxel(x: number, y: number, z: number, color: [number, number, number, number]): Promise<void> {
    try {
      await this.client.addVoxel({
        position: [x, y, z],
        color: color
      });
      
      console.log(`🎯 Added voxel at (${x}, ${y}, ${z}) with color RGBA(${color.join(', ')})`);
    } catch (error) {
      if (error.code === -32003) {
        console.warn(`⚠️  Position (${x}, ${y}, ${z}) is out of bounds, skipping`);
      } else {
        console.error(`❌ Failed to add voxel at (${x}, ${y}, ${z}):`, error.message);
        throw error;
      }
    }
  }

  /**
   * Create a simple 3x3x3 cube
   */
  async createCube(centerX: number, centerY: number, centerZ: number, color: [number, number, number, number]): Promise<void> {
    console.log(`🎲 Creating 3x3x3 cube at center (${centerX}, ${centerY}, ${centerZ})`);
    
    const voxels = [];
    
    for (let x = centerX - 1; x <= centerX + 1; x++) {
      for (let y = centerY - 1; y <= centerY + 1; y++) {
        for (let z = centerZ - 1; z <= centerZ + 1; z++) {
          voxels.push({
            position: [x, y, z] as [number, number, number],
            color: color
          });
        }
      }
    }

    try {
      const result = await this.client.addVoxelBatch({ voxels });
      console.log(`✅ Added ${result.processed_count} voxels for cube`);
    } catch (error) {
      console.error('❌ Failed to create cube:', error.message);
      throw error;
    }
  }

  /**
   * Create a simple house structure
   */
  async createHouse(): Promise<void> {
    console.log('🏠 Creating house structure...');

    const structures = [
      { name: 'Foundation', voxels: this.generateFoundation(), color: [139, 69, 19, 255] as [number, number, number, number] },
      { name: 'Walls', voxels: this.generateWalls(), color: [200, 200, 200, 255] as [number, number, number, number] },
      { name: 'Roof', voxels: this.generateRoof(), color: [139, 0, 0, 255] as [number, number, number, number] }
    ];

    for (const structure of structures) {
      console.log(`🔨 Building ${structure.name}...`);
      
      const voxels = structure.voxels.map(pos => ({
        position: pos,
        color: structure.color
      }));

      try {
        const result = await this.client.addVoxelBatch({ voxels });
        console.log(`✅ ${structure.name}: ${result.processed_count} voxels added`);
      } catch (error) {
        console.error(`❌ Failed to build ${structure.name}:`, error.message);
        throw error;
      }
    }

    console.log('🎉 House completed!');
  }

  /**
   * Generate foundation voxel positions
   */
  private generateFoundation(): Array<[number, number, number]> {
    const positions: Array<[number, number, number]> = [];
    
    for (let x = -5; x <= 5; x++) {
      for (let z = -5; z <= 5; z++) {
        positions.push([x, -17, z]);
      }
    }
    
    return positions;
  }

  /**
   * Generate wall voxel positions
   */
  private generateWalls(): Array<[number, number, number]> {
    const positions: Array<[number, number, number]> = [];
    
    for (let y = -16; y <= -12; y++) {
      // Front and back walls
      for (let x = -5; x <= 5; x++) {
        positions.push([x, y, -5]); // Front wall
        positions.push([x, y, 5]);  // Back wall
      }
      
      // Side walls (excluding corners to avoid duplicates)
      for (let z = -4; z <= 4; z++) {
        positions.push([-5, y, z]); // Left wall
        positions.push([5, y, z]);  // Right wall
      }
    }
    
    return positions;
  }

  /**
   * Generate roof voxel positions
   */
  private generateRoof(): Array<[number, number, number]> {
    const positions: Array<[number, number, number]> = [];
    
    for (let x = -6; x <= 6; x++) {
      for (let z = -6; z <= 6; z++) {
        positions.push([x, -11, z]);
      }
    }
    
    return positions;
  }

  /**
   * Render the current project
   */
  async renderPreview(outputPath?: string): Promise<string> {
    try {
      console.log('📸 Rendering project preview...');
      
      const result = await this.client.renderImage({
        width: 512,
        height: 512,
        camera: {
          position: [25, 25, 25],
          target: [0, -16, 0],
          up: [0, 1, 0],
          fov: 45
        },
        lighting: {
          ambient: 0.3,
          diffuse: 0.7,
          sun_direction: [-0.5, -1, -0.5]
        },
        format: 'png'
      });

      console.log(`✅ Rendered ${result.width}x${result.height} image in ${result.render_time_ms}ms`);

      // Optionally save to file
      if (outputPath) {
        const fs = require('fs');
        const buffer = Buffer.from(result.image_data, 'base64');
        fs.writeFileSync(outputPath, buffer);
        console.log(`💾 Saved render to ${outputPath}`);
      }

      return result.image_data;
    } catch (error) {
      console.error('❌ Failed to render preview:', error.message);
      throw error;
    }
  }

  /**
   * Save the current project
   */
  async saveProject(filePath: string): Promise<void> {
    try {
      await this.client.saveProject({
        file_path: filePath,
        format: 'gox'
      });
      
      console.log(`💾 Project saved to ${filePath}`);
    } catch (error) {
      console.error('❌ Failed to save project:', error.message);
      throw error;
    }
  }

  /**
   * Export project to different formats
   */
  async exportProject(basePath: string, formats: string[] = ['obj', 'ply']): Promise<void> {
    console.log(`📤 Exporting project to ${formats.length} formats...`);

    for (const format of formats) {
      try {
        const outputPath = `${basePath}.${format}`;
        
        await this.client.exportModel({
          file_path: outputPath,
          format: format as any,
          options: {
            include_textures: true,
            optimize_mesh: true,
            scale: 1.0
          }
        });
        
        console.log(`✅ Exported to ${outputPath}`);
      } catch (error) {
        console.error(`❌ Failed to export ${format}:`, error.message);
      }
    }
  }

  /**
   * Get project information
   */
  async getProjectInfo(): Promise<void> {
    try {
      const info = await this.client.getProjectInfo();
      
      console.log('\n📊 Project Information:');
      console.log(`  Name: ${info.name}`);
      console.log(`  Canvas Size: ${info.canvas_size.join(' × ')}`);
      console.log(`  Total Voxels: ${info.total_voxels.toLocaleString()}`);
      console.log(`  Layers: ${info.layer_count}`);
      console.log(`  Memory Usage: ${info.memory_usage.total_mb.toFixed(1)}MB`);
      console.log(`  Bounding Box: ${JSON.stringify(info.bounding_box)}\n`);
    } catch (error) {
      console.error('❌ Failed to get project info:', error.message);
    }
  }

  /**
   * Disconnect from daemon
   */
  async disconnect(): Promise<void> {
    try {
      await this.client.disconnect();
      console.log('👋 Disconnected from daemon');
    } catch (error) {
      console.error('⚠️  Error during disconnect:', error.message);
    }
  }
}

// Example usage function
async function runBasicExample(): Promise<void> {
  const artist = new BasicVoxelArtist();
  
  try {
    // Connect to daemon
    await artist.connect();
    
    // Create a new project
    await artist.createProject({
      name: 'Basic Example Project',
      size: [64, 64, 64],
      colors: [
        [255, 0, 0, 255],   // Red
        [0, 255, 0, 255],   // Green
        [0, 0, 255, 255],   // Blue
        [255, 255, 0, 255]  // Yellow
      ]
    });
    
    // Add some individual voxels
    await artist.addVoxel(0, -16, 0, [255, 0, 0, 255]);   // Red voxel at origin
    await artist.addVoxel(1, -16, 0, [0, 255, 0, 255]);   // Green voxel next to it
    await artist.addVoxel(2, -16, 0, [0, 0, 255, 255]);   // Blue voxel
    
    // Create a cube
    await artist.createCube(5, -14, 5, [255, 255, 0, 255]);
    
    // Create a house
    await artist.createHouse();
    
    // Get project information
    await artist.getProjectInfo();
    
    // Render preview
    await artist.renderPreview('./basic_example_preview.png');
    
    // Save project
    await artist.saveProject('./basic_example.gox');
    
    // Export to multiple formats
    await artist.exportProject('./basic_example', ['obj', 'ply', 'stl']);
    
    console.log('\n🎉 Basic example completed successfully!');
    
  } catch (error) {
    console.error('\n💥 Example failed:', error.message);
    process.exit(1);
  } finally {
    await artist.disconnect();
  }
}

// Run the example if this file is executed directly
if (require.main === module) {
  runBasicExample().catch(console.error);
}

export { BasicVoxelArtist, runBasicExample };