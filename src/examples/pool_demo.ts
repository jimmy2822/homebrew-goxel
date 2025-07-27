/**
 * Connection Pool Demo
 * 
 * This example demonstrates the advanced connection pooling capabilities
 * of the Goxel daemon client, including load balancing, health monitoring,
 * and performance optimization.
 */

import { GoxelDaemonClient } from '../mcp-client/daemon_client';
import { 
  PoolEvent, 
  ClientEvent,
  DaemonMethod,
} from '../mcp-client/types';
import { HealthMonitorEvent } from '../mcp-client/health_monitor';

// Configuration
const SOCKET_PATH = '/tmp/goxel_daemon.sock';

/**
 * Basic connection pool demo
 */
async function basicPoolDemo(): Promise<void> {
  console.log('\n=== Basic Connection Pool Demo ===');
  
  const client = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: true,
    debug: true,
    pool: {
      minConnections: 3,
      maxConnections: 8,
      acquireTimeout: 5000,
      idleTimeout: 60000,
      healthCheckInterval: 5000,
      maxRequestsPerConnection: 1000,
      loadBalancing: true,
      debug: true,
    },
  });

  try {
    // Connect with pooling
    console.log('Connecting with connection pool...');
    await client.connect();
    
    const pool = client.getConnectionPool()!;
    console.log(`Pool initialized with ${pool.getStatistics().totalConnections} connections`);
    
    // Make some requests to demonstrate load balancing
    console.log('\nMaking requests through pool...');
    for (let i = 0; i < 5; i++) {
      const result = await client.call(DaemonMethod.GET_VERSION);
      console.log(`Request ${i + 1}:`, result);
    }
    
    // Show pool statistics
    const stats = pool.getStatistics();
    console.log('\nPool Statistics:');
    console.log(`- Total connections: ${stats.totalConnections}`);
    console.log(`- Available connections: ${stats.availableConnections}`);
    console.log(`- Requests served: ${stats.totalRequestsServed}`);
    console.log(`- Average response time: ${stats.averageRequestTime.toFixed(2)}ms`);
    
    await client.disconnect();
    console.log('Pool disconnected');
    
  } catch (error) {
    console.error('Error in basic pool demo:', error);
  }
}

/**
 * High load demonstration with connection scaling
 */
async function highLoadDemo(): Promise<void> {
  console.log('\n=== High Load Demo ===');
  
  const client = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: true,
    pool: {
      minConnections: 2,
      maxConnections: 10,
      acquireTimeout: 5000,
      idleTimeout: 60000,
      healthCheckInterval: 30000,
      maxRequestsPerConnection: 1000,
      loadBalancing: true,
      debug: false,
    },
  });

  try {
    await client.connect();
    const pool = client.getConnectionPool()!;
    
    console.log(`Starting with ${pool.getStatistics().totalConnections} connections`);
    
    // Create high load to trigger connection scaling
    console.log('Creating high load to trigger scaling...');
    const promises = [];
    
    for (let i = 0; i < 50; i++) {
      promises.push(
        client.call(DaemonMethod.GET_STATUS).then(result => {
          console.log(`Request ${i + 1} completed`);
          return result;
        })
      );
    }
    
    // Check stats during load
    setTimeout(() => {
      const loadStats = pool.getStatistics();
      console.log(`\nDuring load - Connections: ${loadStats.totalConnections}, Queue: ${pool.getQueueSize()}`);
    }, 100);
    
    // Wait for all requests to complete
    await Promise.all(promises);
    
    const finalStats = pool.getStatistics();
    console.log('\nFinal Statistics:');
    console.log(`- Peak connections: ${finalStats.totalConnections}`);
    console.log(`- Total requests served: ${finalStats.totalRequestsServed}`);
    console.log(`- Average response time: ${finalStats.averageRequestTime.toFixed(2)}ms`);
    
    await client.disconnect();
    
  } catch (error) {
    console.error('Error in high load demo:', error);
  }
}

/**
 * Health monitoring demonstration
 */
async function healthMonitoringDemo(): Promise<void> {
  console.log('\n=== Health Monitoring Demo ===');
  
  const client = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: true,
    pool: {
      minConnections: 3,
      maxConnections: 6,
      acquireTimeout: 5000,
      idleTimeout: 60000,
      healthCheckInterval: 2000,
      maxRequestsPerConnection: 1000,
      loadBalancing: true,
      debug: false,
    },
  });

  try {
    await client.connect();
    
    const healthMonitor = client.getHealthMonitor()!;
    
    // Set up health monitoring events
    healthMonitor.on(HealthMonitorEvent.ALERT, (alert) => {
      console.log(`🚨 Health Alert: ${alert.message} (${alert.severity})`);
    });
    
    healthMonitor.on(HealthMonitorEvent.HEALTH_CHANGED, (event) => {
      console.log(`💓 Health Changed: ${event.connectionId} ${event.previousHealth} -> ${event.currentHealth}`);
    });
    
    healthMonitor.on(HealthMonitorEvent.CHECK_COMPLETED, (result) => {
      console.log(`✅ Health Check: ${result.connectionId} - ${result.health} (${result.latency}ms)`);
    });
    
    console.log('Health monitoring active. Making requests...');
    
    // Make some requests while monitoring health
    for (let i = 0; i < 10; i++) {
      await client.call(DaemonMethod.GET_VERSION);
      await new Promise(resolve => setTimeout(resolve, 500));
    }
    
    // Show health metrics
    const allMetrics = healthMonitor.getAllMetrics();
    console.log('\nHealth Metrics:');
    
    allMetrics.forEach((metrics, connectionId) => {
      console.log(`\n${connectionId}:`);
      console.log(`  - Health: ${metrics.currentHealth}`);
      console.log(`  - Average latency: ${metrics.averageLatency.toFixed(2)}ms`);
      console.log(`  - Success rate: ${metrics.successRate.toFixed(1)}%`);
      console.log(`  - Total checks: ${metrics.totalChecks}`);
      console.log(`  - Health trend: ${metrics.healthTrend}`);
    });
    
    const aggStats = healthMonitor.getAggregatedStats();
    console.log('\nAggregated Health Stats:');
    console.log(`- Healthy connections: ${aggStats.healthyConnections}/${aggStats.totalConnections}`);
    console.log(`- Average latency: ${aggStats.averageLatency.toFixed(2)}ms`);
    console.log(`- Average success rate: ${aggStats.averageSuccessRate.toFixed(1)}%`);
    
    await client.disconnect();
    
  } catch (error) {
    console.error('Error in health monitoring demo:', error);
  }
}

/**
 * Batch operations demonstration
 */
async function batchOperationsDemo(): Promise<void> {
  console.log('\n=== Batch Operations Demo ===');
  
  const client = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: true,
    pool: {
      minConnections: 4,
      maxConnections: 8,
      acquireTimeout: 5000,
      idleTimeout: 60000,
      healthCheckInterval: 30000,
      maxRequestsPerConnection: 1000,
      loadBalancing: true,
      debug: false,
    },
  });

  try {
    await client.connect();
    
    // Prepare batch requests
    const batchRequests = [
      { method: DaemonMethod.GET_VERSION },
      { method: DaemonMethod.GET_STATUS },
      { method: DaemonMethod.GET_VERSION },
      { method: DaemonMethod.GET_STATUS },
      { method: DaemonMethod.GET_VERSION },
    ];
    
    console.log(`Executing batch of ${batchRequests.length} requests...`);
    
    const startTime = Date.now();
    const results = await client.executeBatch(batchRequests);
    const duration = Date.now() - startTime;
    
    console.log(`Batch completed in ${duration}ms`);
    console.log('Results:');
    results.forEach((result, index) => {
      console.log(`  ${index + 1}: ${JSON.stringify(result).substring(0, 100)}...`);
    });
    
    // Compare with sequential execution
    console.log('\nComparing with sequential execution...');
    
    const sequentialStart = Date.now();
    for (const request of batchRequests) {
      await client.call(request.method);
    }
    const sequentialDuration = Date.now() - sequentialStart;
    
    console.log(`Sequential execution: ${sequentialDuration}ms`);
    console.log(`Performance improvement: ${((sequentialDuration - duration) / sequentialDuration * 100).toFixed(1)}%`);
    
    await client.disconnect();
    
  } catch (error) {
    console.error('Error in batch operations demo:', error);
  }
}

/**
 * Pool events demonstration
 */
async function poolEventsDemo(): Promise<void> {
  console.log('\n=== Pool Events Demo ===');
  
  const client = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: true,
    pool: {
      minConnections: 2,
      maxConnections: 5,
      acquireTimeout: 5000,
      idleTimeout: 60000,
      healthCheckInterval: 30000,
      maxRequestsPerConnection: 1000,
      loadBalancing: true,
      debug: true,
    },
  });

  try {
    // Set up event listeners
    client.on(ClientEvent.CONNECTED, () => {
      console.log('🔌 Client connected with pool');
    });
    
    client.on(ClientEvent.DISCONNECTED, () => {
      console.log('🔌 Client disconnected from pool');
    });
    
    client.on(ClientEvent.ERROR, (errorData) => {
      console.log(`❌ Client error: ${errorData.error.message}`);
    });
    
    const pool = client.getConnectionPool();
    if (pool) {
      pool.on(PoolEvent.READY, () => {
        console.log('🏊 Pool ready');
      });
      
      pool.on(PoolEvent.CONNECTION_ADDED, (event) => {
        console.log(`➕ Connection added: ${event.connection.id}`);
      });
      
      pool.on(PoolEvent.CONNECTION_REMOVED, (event) => {
        console.log(`➖ Connection removed: ${event.connection.id}`);
      });
      
      pool.on(PoolEvent.REQUEST_QUEUED, (event) => {
        console.log(`📋 Request queued: ${event.request.id}`);
      });
      
      pool.on(PoolEvent.REQUEST_SERVED, (event) => {
        console.log(`✅ Request served: ${event.request.id}`);
      });
    }
    
    await client.connect();
    
    // Make some requests to trigger events
    console.log('Making requests to trigger events...');
    for (let i = 0; i < 8; i++) {
      await client.call(DaemonMethod.GET_VERSION);
      await new Promise(resolve => setTimeout(resolve, 200));
    }
    
    await client.disconnect();
    
  } catch (error) {
    console.error('Error in pool events demo:', error);
  }
}

/**
 * Performance comparison: Single vs Pool
 */
async function performanceComparison(): Promise<void> {
  console.log('\n=== Performance Comparison: Single vs Pool ===');
  
  const testRequests = 20;
  
  // Test single connection
  console.log('Testing single connection client...');
  const singleClient = new GoxelDaemonClient({
    socketPath: SOCKET_PATH,
    enablePooling: false,
  });
  
  try {
    await singleClient.connect();
    
    const singleStart = Date.now();
    for (let i = 0; i < testRequests; i++) {
      await singleClient.call(DaemonMethod.GET_VERSION);
    }
    const singleDuration = Date.now() - singleStart;
    
    await singleClient.disconnect();
    
    console.log(`Single connection: ${singleDuration}ms for ${testRequests} requests`);
    
    // Test pooled connection
    console.log('Testing pooled connection client...');
    const pooledClient = new GoxelDaemonClient({
      socketPath: SOCKET_PATH,
      enablePooling: true,
      pool: {
        minConnections: 4,
        maxConnections: 8,
        acquireTimeout: 5000,
        idleTimeout: 60000,
        healthCheckInterval: 30000,
        maxRequestsPerConnection: 1000,
        loadBalancing: true,
        debug: false,
      },
    });
    
    await pooledClient.connect();
    
    const pooledStart = Date.now();
    const promises = [];
    for (let i = 0; i < testRequests; i++) {
      promises.push(pooledClient.call(DaemonMethod.GET_VERSION));
    }
    await Promise.all(promises);
    const pooledDuration = Date.now() - pooledStart;
    
    await pooledClient.disconnect();
    
    console.log(`Pooled connection: ${pooledDuration}ms for ${testRequests} concurrent requests`);
    
    const improvement = ((singleDuration - pooledDuration) / singleDuration) * 100;
    console.log(`Performance improvement: ${improvement.toFixed(1)}%`);
    
  } catch (error) {
    console.error('Error in performance comparison:', error);
  }
}

/**
 * Main demo runner
 */
async function main(): Promise<void> {
  console.log('🚀 Goxel Connection Pool Demo');
  console.log('==============================');
  
  try {
    await basicPoolDemo();
    await highLoadDemo();
    await healthMonitoringDemo();
    await batchOperationsDemo();
    await poolEventsDemo();
    await performanceComparison();
    
    console.log('\n✅ All demos completed successfully!');
    
  } catch (error) {
    console.error('\n❌ Demo failed:', error);
    process.exit(1);
  }
}

// Run demos if this file is executed directly
if (require.main === module) {
  main().catch(error => {
    console.error('Fatal error:', error);
    process.exit(1);
  });
}

export {
  basicPoolDemo,
  highLoadDemo,
  healthMonitoringDemo,
  batchOperationsDemo,
  poolEventsDemo,
  performanceComparison,
};