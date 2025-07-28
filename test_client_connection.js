#!/usr/bin/env node

/**
 * Simple test script to verify TypeScript client can connect to daemon
 */

const lib = require('./dist/index.js');
const { GoxelDaemonClient } = lib;
const DaemonMethod = lib.DaemonMethod;

async function testConnection() {
  console.log('=== Goxel v14.0 TypeScript Client Connection Test ===\n');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel.sock',
    timeout: 5000,
    debug: true,
  });

  try {
    console.log('1. Connecting to daemon...');
    await client.connect();
    console.log('✅ Successfully connected to Goxel daemon\n');

    console.log('2. Testing version method...');
    const version = await client.call(DaemonMethod.VERSION);
    console.log('✅ Daemon version:', version, '\n');

    console.log('3. Testing status method...');
    const status = await client.call(DaemonMethod.STATUS);
    console.log('✅ Daemon status:', status, '\n');

    console.log('4. Testing ping method...');
    const pingResult = await client.call(DaemonMethod.PING);
    console.log('✅ Ping result:', pingResult, '\n');

    console.log('5. Testing echo method...');
    const echoResult = await client.call(DaemonMethod.ECHO, {
      message: 'Hello from TypeScript client!',
      timestamp: new Date().toISOString()
    });
    console.log('✅ Echo result:', echoResult, '\n');

    console.log('6. Testing list_methods method...');
    const methodsList = await client.call(DaemonMethod.LIST_METHODS);
    console.log('✅ Available methods:', methodsList, '\n');

    console.log('7. Getting client statistics...');
    const stats = client.getStatistics();
    console.log('✅ Client statistics:');
    console.log(`   - Requests sent: ${stats.requestsSent}`);
    console.log(`   - Responses received: ${stats.responsesReceived}`);
    console.log(`   - Average response time: ${stats.averageResponseTime.toFixed(2)}ms`);
    console.log(`   - Uptime: ${stats.uptime}ms\n`);

    console.log('8. Disconnecting...');
    await client.disconnect();
    console.log('✅ Successfully disconnected\n');

    console.log('🎉 All tests passed! TypeScript client is working correctly.');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
    if (error.code) {
      console.error('   Error code:', error.code);
    }
    if (error.context) {
      console.error('   Context:', error.context);
    }
    
    try {
      await client.disconnect();
    } catch (disconnectError) {
      console.error('   Failed to disconnect:', disconnectError.message);
    }
    
    process.exit(1);
  }
}

// Handle process signals
process.on('SIGINT', () => {
  console.log('\n\n⚠️  Received SIGINT, exiting...');
  process.exit(0);
});

process.on('SIGTERM', () => {
  console.log('\n\n⚠️  Received SIGTERM, exiting...');
  process.exit(0);
});

// Run the test
testConnection().catch(error => {
  console.error('❌ Unhandled error:', error);
  process.exit(1);
});