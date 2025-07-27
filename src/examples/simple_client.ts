/**
 * Simple Goxel Daemon Client Example
 * 
 * This is a minimal example showing the basic usage of the Goxel daemon client.
 * Perfect for getting started or as a template for simple applications.
 */

import { GoxelDaemonClient, DaemonMethod } from '../index';

async function main(): Promise<void> {
  // Create client instance
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 5000,
  });

  try {
    // Connect to the daemon
    console.log('Connecting to Goxel daemon...');
    await client.connect();
    console.log('✅ Connected!');

    // Get daemon version
    const version = await client.call(DaemonMethod.GET_VERSION);
    console.log('📦 Goxel version:', version);

    // Create a new project
    console.log('🆕 Creating new project...');
    await client.call(DaemonMethod.CREATE_PROJECT, {
      name: 'simple_example',
    });

    // Add a red voxel at the origin
    console.log('🔴 Adding red voxel...');
    await client.call(DaemonMethod.ADD_VOXEL, {
      x: 0,
      y: -16, // Ground level in Goxel
      z: 0,
      r: 255, // Red
      g: 0,
      b: 0,
      a: 255, // Fully opaque
    });

    // Add a green voxel next to it
    console.log('🟢 Adding green voxel...');
    await client.call(DaemonMethod.ADD_VOXEL, {
      x: 1,
      y: -16,
      z: 0,
      r: 0,
      g: 255, // Green
      b: 0,
      a: 255,
    });

    // Add a blue voxel to complete the RGB set
    console.log('🔵 Adding blue voxel...');
    await client.call(DaemonMethod.ADD_VOXEL, {
      x: 0,
      y: -16,
      z: 1,
      r: 0,
      g: 0,
      b: 255, // Blue
      a: 255,
    });

    // Export the project as OBJ
    console.log('💾 Exporting project...');
    await client.call(DaemonMethod.EXPORT_PROJECT, {
      format: 'obj',
      path: './simple_example.obj',
    });

    console.log('✨ Project exported to simple_example.obj');

  } catch (error) {
    console.error('❌ Error:', error);
    process.exit(1);
  } finally {
    // Always disconnect when done
    await client.disconnect();
    console.log('👋 Disconnected');
  }
}

// Run the example
if (require.main === module) {
  main().catch(console.error);
}