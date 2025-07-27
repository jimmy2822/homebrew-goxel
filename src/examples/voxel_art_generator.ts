/**
 * Voxel Art Generator Example
 * 
 * This example demonstrates how to use the Goxel daemon client to
 * programmatically generate voxel art. It creates various patterns
 * and structures to showcase the client's capabilities.
 */

import { GoxelDaemonClient, DaemonMethod, RgbaColor } from '../index';

/**
 * Utility class for generating voxel art
 */
class VoxelArtGenerator {
  private client: GoxelDaemonClient;

  public constructor(client: GoxelDaemonClient) {
    this.client = client;
  }

  /**
   * Creates a rainbow spiral
   */
  public async createRainbowSpiral(
    centerX: number = 0,
    centerZ: number = 0,
    baseY: number = -16,
    radius: number = 5,
    height: number = 10
  ): Promise<void> {
    console.log('🌈 Creating rainbow spiral...');

    const colors = [
      { r: 255, g: 0, b: 0 },     // Red
      { r: 255, g: 165, b: 0 },   // Orange
      { r: 255, g: 255, b: 0 },   // Yellow
      { r: 0, g: 255, b: 0 },     // Green
      { r: 0, g: 0, b: 255 },     // Blue
      { r: 75, g: 0, b: 130 },    // Indigo
      { r: 238, g: 130, b: 238 }, // Violet
    ];

    const promises: Promise<unknown>[] = [];

    for (let y = 0; y < height; y++) {
      const angle = (y * Math.PI * 2) / height * 3; // 3 full rotations
      const currentRadius = radius * (1 - y / height * 0.3); // Slightly smaller at top
      
      const x = Math.round(centerX + Math.cos(angle) * currentRadius);
      const z = Math.round(centerZ + Math.sin(angle) * currentRadius);
      
      const colorIndex = y % colors.length;
      const color = { ...colors[colorIndex]!, a: 255 };

      promises.push(
        this.client.call(DaemonMethod.ADD_VOXEL, {
          x,
          y: baseY + y,
          z,
          ...color,
        })
      );
    }

    await Promise.all(promises);
    console.log('✅ Rainbow spiral created!');
  }

  /**
   * Creates a 3D checkerboard pattern
   */
  public async createCheckerboard(
    startX: number = -5,
    startY: number = -16,
    startZ: number = -5,
    size: number = 10
  ): Promise<void> {
    console.log('🏁 Creating checkerboard pattern...');

    const whiteColor: RgbaColor = { r: 255, g: 255, b: 255, a: 255 };
    const blackColor: RgbaColor = { r: 0, g: 0, b: 0, a: 255 };

    const promises: Promise<unknown>[] = [];

    for (let x = 0; x < size; x++) {
      for (let z = 0; z < size; z++) {
        const isWhite = (x + z) % 2 === 0;
        const color = isWhite ? whiteColor : blackColor;

        promises.push(
          this.client.call(DaemonMethod.ADD_VOXEL, {
            x: startX + x,
            y: startY,
            z: startZ + z,
            ...color,
          })
        );
      }
    }

    await Promise.all(promises);
    console.log('✅ Checkerboard pattern created!');
  }

  /**
   * Creates a simple house structure
   */
  public async createHouse(
    baseX: number = 10,
    baseY: number = -16,
    baseZ: number = 0
  ): Promise<void> {
    console.log('🏠 Creating house...');

    const wallColor: RgbaColor = { r: 139, g: 69, b: 19, a: 255 }; // Brown
    const roofColor: RgbaColor = { r: 178, g: 34, b: 34, a: 255 }; // Red
    const doorColor: RgbaColor = { r: 101, g: 67, b: 33, a: 255 }; // Dark brown
    const windowColor: RgbaColor = { r: 135, g: 206, b: 235, a: 255 }; // Sky blue

    const promises: Promise<unknown>[] = [];

    // House base (5x5x3)
    for (let x = 0; x < 5; x++) {
      for (let z = 0; z < 5; z++) {
        for (let y = 0; y < 3; y++) {
          // Only create walls, not interior
          if (x === 0 || x === 4 || z === 0 || z === 4) {
            let color = wallColor;
            
            // Add door
            if (x === 2 && z === 0 && (y === 0 || y === 1)) {
              color = doorColor;
            }
            
            // Add windows
            if ((x === 1 && z === 4 && y === 1) || (x === 3 && z === 4 && y === 1)) {
              color = windowColor;
            }

            promises.push(
              this.client.call(DaemonMethod.ADD_VOXEL, {
                x: baseX + x,
                y: baseY + y,
                z: baseZ + z,
                ...color,
              })
            );
          }
        }
      }
    }

    // Roof (pyramid shape)
    const roofLevels = [
      { size: 5, y: 3 },
      { size: 3, y: 4 },
      { size: 1, y: 5 },
    ];

    for (const level of roofLevels) {
      const offset = Math.floor((5 - level.size) / 2);
      
      for (let x = 0; x < level.size; x++) {
        for (let z = 0; z < level.size; z++) {
          promises.push(
            this.client.call(DaemonMethod.ADD_VOXEL, {
              x: baseX + offset + x,
              y: baseY + level.y,
              z: baseZ + offset + z,
              ...roofColor,
            })
          );
        }
      }
    }

    await Promise.all(promises);
    console.log('✅ House created!');
  }

  /**
   * Creates a tree structure
   */
  public async createTree(
    baseX: number = -10,
    baseY: number = -16,
    baseZ: number = 0
  ): Promise<void> {
    console.log('🌳 Creating tree...');

    const trunkColor: RgbaColor = { r: 101, g: 67, b: 33, a: 255 }; // Brown
    const leavesColor: RgbaColor = { r: 34, g: 139, b: 34, a: 255 }; // Forest green

    const promises: Promise<unknown>[] = [];

    // Trunk (height 4)
    for (let y = 0; y < 4; y++) {
      promises.push(
        this.client.call(DaemonMethod.ADD_VOXEL, {
          x: baseX,
          y: baseY + y,
          z: baseZ,
          ...trunkColor,
        })
      );
    }

    // Leaves (3x3x3 cube at the top)
    for (let x = -1; x <= 1; x++) {
      for (let z = -1; z <= 1; z++) {
        for (let y = 4; y < 7; y++) {
          // Skip some blocks to make it look more natural
          if (Math.random() > 0.2) { // 80% chance to place a leaf block
            promises.push(
              this.client.call(DaemonMethod.ADD_VOXEL, {
                x: baseX + x,
                y: baseY + y,
                z: baseZ + z,
                ...leavesColor,
              })
            );
          }
        }
      }
    }

    await Promise.all(promises);
    console.log('✅ Tree created!');
  }

  /**
   * Creates a gradient sphere
   */
  public async createGradientSphere(
    centerX: number = 0,
    centerY: number = -10,
    centerZ: number = 10,
    radius: number = 4
  ): Promise<void> {
    console.log('🔮 Creating gradient sphere...');

    const promises: Promise<unknown>[] = [];

    for (let x = -radius; x <= radius; x++) {
      for (let y = -radius; y <= radius; y++) {
        for (let z = -radius; z <= radius; z++) {
          const distance = Math.sqrt(x * x + y * y + z * z);
          
          if (distance <= radius) {
            // Create color gradient based on distance from center
            const intensity = 1 - (distance / radius);
            const r = Math.round(255 * intensity);
            const g = Math.round(128 * (1 - intensity));
            const b = Math.round(255 * (1 - intensity));

            promises.push(
              this.client.call(DaemonMethod.ADD_VOXEL, {
                x: centerX + x,
                y: centerY + y,
                z: centerZ + z,
                r,
                g,
                b,
                a: 255,
              })
            );
          }
        }
      }
    }

    await Promise.all(promises);
    console.log('✅ Gradient sphere created!');
  }
}

/**
 * Main function to run the voxel art generator
 */
async function main(): Promise<void> {
  console.log('🎨 Goxel Voxel Art Generator');
  console.log('=============================\n');

  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 30000, // Long timeout for complex operations
    debug: false,
  });

  try {
    // Connect to daemon
    console.log('🔗 Connecting to Goxel daemon...');
    await client.connect();
    console.log('✅ Connected!\n');

    // Create a new project
    console.log('📋 Creating new project...');
    await client.call(DaemonMethod.CREATE_PROJECT, {
      name: 'voxel_art_gallery',
    });
    console.log('✅ Project created!\n');

    // Create the art generator
    const generator = new VoxelArtGenerator(client);

    // Generate different art pieces
    await generator.createCheckerboard();
    await generator.createRainbowSpiral();
    await generator.createHouse();
    await generator.createTree();
    await generator.createGradientSphere();

    console.log('\n🎭 All art pieces created!');

    // Export the project in different formats
    console.log('\n💾 Exporting project...');
    
    const exports = [
      { format: 'obj', path: './voxel_art_gallery.obj' },
      { format: 'gox', path: './voxel_art_gallery.gox' },
      { format: 'vox', path: './voxel_art_gallery.vox' },
    ];

    for (const exportConfig of exports) {
      try {
        await client.call(DaemonMethod.EXPORT_PROJECT, exportConfig);
        console.log(`✅ Exported as ${exportConfig.format.toUpperCase()}: ${exportConfig.path}`);
      } catch (error) {
        console.warn(`⚠️  Failed to export as ${exportConfig.format.toUpperCase()}:`, error);
      }
    }

    // Show statistics
    const stats = client.getStatistics();
    console.log('\n📊 Generation Statistics:');
    console.log(`   Requests sent: ${stats.requestsSent}`);
    console.log(`   Average response time: ${stats.averageResponseTime.toFixed(2)}ms`);
    console.log(`   Data transmitted: ${(stats.bytesTransmitted / 1024).toFixed(2)}KB`);
    console.log(`   Generation time: ${(stats.uptime / 1000).toFixed(1)}s`);

    console.log('\n🎉 Voxel art generation completed successfully!');
    console.log('Check the exported files to see your creations.');

  } catch (error) {
    console.error('\n❌ Error during art generation:', error);
    process.exit(1);
  } finally {
    await client.disconnect();
    console.log('\n👋 Disconnected from daemon');
  }
}

// Run the art generator
if (require.main === module) {
  main().catch((error) => {
    console.error('Unhandled error:', error);
    process.exit(1);
  });
}

export { VoxelArtGenerator };