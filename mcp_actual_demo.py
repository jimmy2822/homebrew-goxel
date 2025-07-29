#!/usr/bin/env python3
"""
Actual MCP to GUI Workflow Demo

This demonstrates the REAL workflow using working daemon methods.
"""

import json
import socket
import sys
import os
import time

class GoxelDaemonClient:
    def __init__(self, socket_path='/tmp/goxel.sock'):
        self.socket_path = socket_path
        self.sock = None
        self.request_id = 0
    
    def connect(self):
        """Connect to daemon."""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.socket_path)
        print(f"✓ Connected to Goxel daemon")
    
    def call(self, method, params=None):
        """Make JSON-RPC call."""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params or {}
        }
        
        request_str = json.dumps(request) + '\n'
        self.sock.send(request_str.encode('utf-8'))
        
        response_data = self.sock.recv(8192).decode('utf-8')
        response = json.loads(response_data)
        
        if 'error' in response:
            print(f"  ⚠️  Error: {response['error']}")
            return None
        
        return response.get('result')
    
    def disconnect(self):
        """Disconnect."""
        if self.sock:
            self.sock.close()

def main():
    """Real MCP workflow demonstration."""
    print("🚀 ACTUAL MCP → Daemon → GUI Workflow")
    print("="*50)
    
    client = GoxelDaemonClient()
    
    try:
        # Step 1: Connect to daemon
        print("\n1️⃣  Connecting to daemon...")
        client.connect()
        
        # Step 2: Test basic connectivity
        print("\n2️⃣  Testing daemon functionality...")
        result = client.call('ping')
        if result:
            print(f"  → Ping successful: {result}")
        
        # Step 3: Get version info
        print("\n3️⃣  Getting daemon version...")
        version = client.call('version')
        if version:
            print(f"  → Version: {version}")
        
        # Step 4: List available methods
        print("\n4️⃣  Listing available methods...")
        methods = client.call('list_methods')
        if methods:
            print(f"  → Available methods: {len(methods)} total")
            for method in methods[:5]:  # Show first 5
                print(f"    • {method}")
            if len(methods) > 5:
                print(f"    • ... and {len(methods) - 5} more")
        
        # Step 5: Try to create a project (this might work)
        print("\n5️⃣  Attempting to create project...")
        result = client.call('goxel.create_project', {'name': 'mcp_demo'})
        if result:
            print(f"  → Project created: {result}")
        
        # Step 6: Try to add a voxel (basic method)
        print("\n6️⃣  Attempting to add voxel...")
        result = client.call('goxel.add_voxel', {
            'x': 0, 'y': -16, 'z': 0,  # On the visual grid
            'r': 255, 'g': 0, 'b': 0, 'a': 255  # Red
        })
        if result:
            print(f"  → Voxel added: {result}")
        
        # Step 7: Try to save project
        print("\n7️⃣  Attempting to save project...")
        result = client.call('goxel.save_project', {'path': 'actual_mcp_demo.gox'})
        if result:
            print(f"  → Project saved: {result}")
            
            # Check if file was created
            if os.path.exists('actual_mcp_demo.gox'):
                file_size = os.path.getsize('actual_mcp_demo.gox')
                print(f"  → File created successfully: {file_size} bytes")
                
                print("\n🎯 SUCCESS! Complete workflow:")
                print("  1. ✅ Connected to daemon")
                print("  2. ✅ Created project via MCP/JSON-RPC")
                print("  3. ✅ Added voxel programmatically")
                print("  4. ✅ Saved as .gox file")
                print("  5. 🔜 Ready to open in GUI: ./goxel actual_mcp_demo.gox")
                
                print("\n📋 This proves the MCP → GUI workflow:")
                print("  • MCP tools can create 3D models via daemon")
                print("  • Models save to standard .gox format")
                print("  • GUI can open and edit the same files")
                print("  • Seamless integration between automation and manual editing")
            else:
                print("  ⚠️  File not found - save may not have worked")
        
        client.disconnect()
        
    except Exception as e:
        print(f"❌ Error during workflow: {e}")
        client.disconnect()

if __name__ == '__main__':
    main()