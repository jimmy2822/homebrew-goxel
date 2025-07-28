#!/usr/bin/env node

/**
 * Manual socket test to verify daemon is responding to JSON-RPC
 */

const net = require('net');

function testSocketConnection() {
  return new Promise((resolve, reject) => {
    console.log('Testing manual socket connection to daemon...\n');

    const socket = new net.Socket();
    let responseData = '';

    socket.on('connect', () => {
      console.log('✅ Socket connected to /tmp/goxel.sock');
      
      // Send a simple JSON-RPC request
      const request = {
        jsonrpc: '2.0',
        method: 'version',
        id: 1
      };
      
      const requestStr = JSON.stringify(request) + '\n';
      console.log('📤 Sending request:', request);
      
      socket.write(requestStr);
      
      // Set timeout for response
      setTimeout(() => {
        socket.destroy();
        reject(new Error('Response timeout after 10 seconds'));
      }, 10000);
    });

    socket.on('data', (data) => {
      responseData += data.toString();
      console.log('📥 Raw response data:', JSON.stringify(data.toString()));
      
      // Check if we have a complete response
      if (responseData.includes('\n')) {
        try {
          // Clean up the response - look for JSON part
          const jsonStart = responseData.indexOf('{');
          const jsonEnd = responseData.lastIndexOf('}') + 1;
          
          if (jsonStart !== -1 && jsonEnd > jsonStart) {
            const jsonData = responseData.substring(jsonStart, jsonEnd);
            console.log('🧹 Cleaned JSON:', jsonData);
            
            const response = JSON.parse(jsonData);
            console.log('✅ Parsed response:', response);
            socket.destroy();
            resolve(response);
          } else {
            throw new Error('No valid JSON found in response');
          }
        } catch (error) {
          console.log('❌ Failed to parse response:', error.message);
          console.log('Full response data:', JSON.stringify(responseData));
          socket.destroy();
          reject(error);
        }
      }
    });

    socket.on('error', (error) => {
      console.log('❌ Socket error:', error.message);
      reject(error);
    });

    socket.on('close', () => {
      console.log('🔌 Socket closed');
    });

    console.log('🔌 Connecting to /tmp/goxel.sock...');
    socket.connect('/tmp/goxel.sock');
  });
}

testSocketConnection()
  .then((response) => {
    console.log('\n🎉 Manual socket test successful!');
    process.exit(0);
  })
  .catch((error) => {
    console.error('\n❌ Manual socket test failed:', error.message);
    process.exit(1);
  });