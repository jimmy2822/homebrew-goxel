#!/usr/bin/env node

/**
 * Simple working client test
 */

const { GoxelDaemonClient, DaemonMethod } = require('./dist/index.js');

async function testSimpleConnection() {
  console.log('=== Simple Client Test ===\n');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel.sock',
    timeout: 10000,
    debug: true,
    autoReconnect: false, // Disable auto-reconnect for testing
  });

  try {
    console.log('Connecting...');
    await client.connect();
    console.log('✅ Connected!\n');

    console.log('Testing version method (10s timeout)...');
    const version = await client.call(DaemonMethod.VERSION);
    console.log('✅ Version result:', JSON.stringify(version, null, 2));

  } catch (error) {
    console.error('❌ Error:', error.message);
    if (error.code) console.error('   Code:', error.code);
    if (error.context) console.error('   Context:', error.context);
  } finally {
    try {
      console.log('\nDisconnecting...');
      await client.disconnect();
      console.log('✅ Disconnected');
    } catch (e) {
      console.error('Error during disconnect:', e.message);
    }
  }
}

testSimpleConnection().catch(console.error);