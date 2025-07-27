/**
 * Goxel Daemon Client Demo
 * 
 * This example demonstrates how to use the TypeScript daemon client
 * to communicate with a Goxel daemon server. It shows basic usage
 * patterns and best practices.
 */

import {
  GoxelDaemonClient,
  DaemonMethod,
  ClientEvent,
  ConnectionError,
  TimeoutError,
  JsonRpcClientError,
  VoxelCoordinates,
  RgbaColor,
} from '../index';

/**
 * Basic usage example
 */
async function basicUsageExample(): Promise<void> {
  console.log('=== Basic Usage Example ===');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 5000,
    debug: true,
  });

  try {
    // Connect to daemon
    console.log('Connecting to daemon...');
    await client.connect();
    console.log('Connected successfully!');

    // Get version information
    const version = await client.call(DaemonMethod.GET_VERSION);
    console.log('Goxel version:', version);

    // Create a new project
    const projectResult = await client.call(DaemonMethod.CREATE_PROJECT, {
      name: 'demo_project',
    });
    console.log('Project created:', projectResult);

    // Add some voxels
    const voxelColor: RgbaColor = { r: 255, g: 0, b: 0, a: 255 }; // Red
    const position: VoxelCoordinates = { x: 0, y: -16, z: 0 };

    await client.call(DaemonMethod.ADD_VOXEL, {
      ...position,
      ...voxelColor,
    });
    console.log('Voxel added at', position);

    // Export the project
    await client.call(DaemonMethod.EXPORT_PROJECT, {
      format: 'obj',
      path: './demo_output.obj',
    });
    console.log('Project exported to demo_output.obj');

    // Get project info
    const projectInfo = await client.call(DaemonMethod.GET_PROJECT_INFO);
    console.log('Project info:', projectInfo);

  } catch (error) {
    console.error('Error in basic usage example:', error);
  } finally {
    await client.disconnect();
    console.log('Disconnected from daemon');
  }
}

/**
 * Event handling example
 */
async function eventHandlingExample(): Promise<void> {
  console.log('\n=== Event Handling Example ===');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 3000,
    autoReconnect: true,
    reconnectInterval: 2000,
    debug: true,
  });

  // Set up event listeners
  client.on(ClientEvent.CONNECTED, (data) => {
    console.log('Event: Connected at', data.timestamp);
  });

  client.on(ClientEvent.DISCONNECTED, (data) => {
    console.log('Event: Disconnected at', data.timestamp);
  });

  client.on(ClientEvent.RECONNECTING, (data) => {
    console.log('Event: Reconnecting at', data.timestamp);
  });

  client.on(ClientEvent.ERROR, (data) => {
    console.log('Event: Error at', data.timestamp, '-', data.error.message);
  });

  client.on(ClientEvent.MESSAGE, (data) => {
    console.log('Event: Message received at', data.timestamp);
  });

  try {
    await client.connect();
    
    // Make some calls to trigger events
    await client.call(DaemonMethod.GET_VERSION);
    await client.call(DaemonMethod.GET_STATUS);
    
    // Send a notification (no response expected)
    await client.sendNotification('log_message', { 
      level: 'info',
      message: 'Demo notification from TypeScript client',
    });

    // Show statistics
    const stats = client.getStatistics();
    console.log('Client statistics:', {
      requestsSent: stats.requestsSent,
      responsesReceived: stats.responsesReceived,
      averageResponseTime: `${stats.averageResponseTime.toFixed(2)}ms`,
      uptime: `${(stats.uptime / 1000).toFixed(1)}s`,
    });

  } catch (error) {
    console.error('Error in event handling example:', error);
  } finally {
    await client.disconnect();
  }
}

/**
 * Error handling example
 */
async function errorHandlingExample(): Promise<void> {
  console.log('\n=== Error Handling Example ===');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 1000, // Short timeout for demo
    retryAttempts: 2,
    debug: true,
  });

  try {
    await client.connect();

    // Example 1: Method not found error
    try {
      await client.call('nonexistent_method');
    } catch (error) {
      if (error instanceof JsonRpcClientError) {
        console.log(`JSON-RPC Error: ${error.message} (code: ${error.code})`);
      }
    }

    // Example 2: Invalid parameters error
    try {
      await client.call(DaemonMethod.ADD_VOXEL, {
        x: 'invalid', // Should be number
        y: -16,
        z: 0,
        r: 255, g: 0, b: 0, a: 255,
      });
    } catch (error) {
      if (error instanceof JsonRpcClientError) {
        console.log(`Parameter Error: ${error.message}`);
      }
    }

    // Example 3: Timeout error (if server is slow)
    try {
      await client.call('slow_method', {}, { timeout: 100 });
    } catch (error) {
      if (error instanceof TimeoutError) {
        console.log(`Timeout Error: ${error.message}`);
      }
    }

  } catch (error) {
    if (error instanceof ConnectionError) {
      console.log(`Connection Error: ${error.message}`);
    } else {
      console.error('Unexpected error:', error);
    }
  } finally {
    await client.disconnect();
  }
}

/**
 * Advanced features example
 */
async function advancedFeaturesExample(): Promise<void> {
  console.log('\n=== Advanced Features Example ===');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 5000,
    maxMessageSize: 2 * 1024 * 1024, // 2MB
    debug: true,
  });

  try {
    await client.connect();

    // Custom request IDs
    const customId = `request-${Date.now()}`;
    const result = await client.call(DaemonMethod.GET_VERSION, undefined, {
      id: customId,
    });
    console.log(`Response for custom ID ${customId}:`, result);

    // Concurrent requests
    console.log('Making concurrent requests...');
    const promises = [
      client.call(DaemonMethod.GET_VERSION),
      client.call(DaemonMethod.GET_STATUS),
      client.call(DaemonMethod.GET_PROJECT_INFO),
    ];

    const results = await Promise.all(promises);
    console.log('Concurrent results:', results);

    // Request with custom timeout
    try {
      await client.call(DaemonMethod.GET_VERSION, undefined, {
        timeout: 10000, // 10 second timeout for this request
      });
    } catch (error) {
      console.log('Custom timeout request failed:', error);
    }

    // Batch operations
    console.log('Performing batch voxel operations...');
    const batchPromises = [];
    
    for (let x = 0; x < 5; x++) {
      for (let z = 0; z < 5; z++) {
        batchPromises.push(
          client.call(DaemonMethod.ADD_VOXEL, {
            x,
            y: -16,
            z,
            r: x * 51,
            g: z * 51,
            b: 128,
            a: 255,
          })
        );
      }
    }

    await Promise.all(batchPromises);
    console.log('Batch operations completed');

    // Export with custom options
    await client.call(DaemonMethod.EXPORT_PROJECT, {
      format: 'obj',
      path: './advanced_demo.obj',
      options: {
        includeNormals: true,
        includeTextures: false,
      },
    });

  } catch (error) {
    console.error('Error in advanced features example:', error);
  } finally {
    await client.disconnect();
  }
}

/**
 * Performance testing example
 */
async function performanceTestExample(): Promise<void> {
  console.log('\n=== Performance Test Example ===');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel_daemon.sock',
    timeout: 30000, // Long timeout for performance tests
    debug: false, // Disable debug for cleaner output
  });

  try {
    await client.connect();

    // Test 1: Request throughput
    console.log('Testing request throughput...');
    const requestCount = 100;
    const startTime = Date.now();

    const promises = Array.from({ length: requestCount }, (_, i) =>
      client.call(DaemonMethod.GET_VERSION, undefined, { id: `perf-${i}` })
    );

    await Promise.all(promises);
    
    const duration = Date.now() - startTime;
    const throughput = (requestCount / duration) * 1000; // requests per second
    
    console.log(`Completed ${requestCount} requests in ${duration}ms`);
    console.log(`Throughput: ${throughput.toFixed(2)} requests/second`);

    // Test 2: Large data handling
    console.log('Testing large data handling...');
    const largeData = {
      voxels: Array.from({ length: 1000 }, (_, i) => ({
        x: i % 10,
        y: -16,
        z: Math.floor(i / 10),
        r: 255,
        g: 128,
        b: 64,
        a: 255,
      })),
    };

    const largeDataStart = Date.now();
    await client.call('batch_add_voxels', largeData);
    const largeDataDuration = Date.now() - largeDataStart;
    
    console.log(`Large data request completed in ${largeDataDuration}ms`);

    // Show final statistics
    const stats = client.getStatistics();
    console.log('Final statistics:', {
      totalRequests: stats.requestsSent,
      totalResponses: stats.responsesReceived,
      averageResponseTime: `${stats.averageResponseTime.toFixed(2)}ms`,
      bytesTransmitted: `${(stats.bytesTransmitted / 1024).toFixed(2)}KB`,
      bytesReceived: `${(stats.bytesReceived / 1024).toFixed(2)}KB`,
      uptime: `${(stats.uptime / 1000).toFixed(1)}s`,
    });

  } catch (error) {
    console.error('Error in performance test example:', error);
  } finally {
    await client.disconnect();
  }
}

/**
 * Main demo function
 */
async function main(): Promise<void> {
  console.log('🎯 Goxel Daemon Client Demo');
  console.log('============================\n');

  const args = process.argv.slice(2);
  const example = args[0] || 'all';

  try {
    switch (example) {
      case 'basic':
        await basicUsageExample();
        break;
      
      case 'events':
        await eventHandlingExample();
        break;
      
      case 'errors':
        await errorHandlingExample();
        break;
      
      case 'advanced':
        await advancedFeaturesExample();
        break;
      
      case 'performance':
        await performanceTestExample();
        break;
      
      case 'all':
      default:
        await basicUsageExample();
        await eventHandlingExample();
        await errorHandlingExample();
        await advancedFeaturesExample();
        await performanceTestExample();
        break;
    }

    console.log('\n✅ Demo completed successfully!');
    
  } catch (error) {
    console.error('\n❌ Demo failed:', error);
    process.exit(1);
  }
}

// Run the demo if this file is executed directly
if (require.main === module) {
  main().catch((error) => {
    console.error('Unhandled error:', error);
    process.exit(1);
  });
}

export {
  basicUsageExample,
  eventHandlingExample,
  errorHandlingExample,
  advancedFeaturesExample,
  performanceTestExample,
};