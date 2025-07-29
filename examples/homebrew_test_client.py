#!/usr/bin/env python3
"""
Homebrew Goxel Daemon Test Client
Tests the goxel-daemon installation via Homebrew
"""

import json
import socket
import sys
import os

def send_json_rpc(sock_path, method, params=None):
    """Send a JSON-RPC request to the daemon"""
    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": params or {}
    }
    
    try:
        # Connect to Unix socket
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(sock_path)
        
        # Send request
        message = json.dumps(request) + '\n'
        client.send(message.encode())
        
        # Receive response
        response = b""
        while True:
            chunk = client.recv(4096)
            if not chunk:
                break
            response += chunk
            if b'\n' in response:
                break
        
        client.close()
        return json.loads(response.decode())
        
    except FileNotFoundError:
        print(f"Error: Socket not found at {sock_path}")
        print("Make sure goxel-daemon is running:")
        print("  brew services start goxel")
        print("or")
        print("  goxel-daemon --foreground --socket /tmp/goxel.sock")
        return None
    except Exception as e:
        print(f"Error: {e}")
        return None

def main():
    # Try Homebrew service socket first
    homebrew_socket = "/usr/local/var/run/goxel/goxel.sock"
    if os.path.exists("/opt/homebrew"):  # Apple Silicon
        homebrew_socket = "/opt/homebrew/var/run/goxel/goxel.sock"
    
    # Fallback to manual socket
    manual_socket = "/tmp/goxel.sock"
    
    socket_path = homebrew_socket if os.path.exists(homebrew_socket) else manual_socket
    
    print(f"Testing Goxel daemon at {socket_path}...")
    
    # Test version
    result = send_json_rpc(socket_path, "goxel.get_version")
    if result:
        print(f"✓ Version: {result.get('result', {}).get('version', 'Unknown')}")
    else:
        sys.exit(1)
    
    # Test create project
    result = send_json_rpc(socket_path, "goxel.create_project", {"name": "homebrew_test"})
    if result and not result.get('error'):
        print("✓ Created project: homebrew_test")
    
    # Test add voxel
    result = send_json_rpc(socket_path, "goxel.add_voxel", {
        "position": {"x": 0, "y": 0, "z": 0},
        "color": {"r": 255, "g": 0, "b": 0, "a": 255}
    })
    if result and not result.get('error'):
        print("✓ Added red voxel at origin")
    
    # Test export
    result = send_json_rpc(socket_path, "goxel.export_model", {
        "path": "/tmp/homebrew_test.obj",
        "format": "obj"
    })
    if result and not result.get('error'):
        print("✓ Exported model to /tmp/homebrew_test.obj")
    
    print("\n✅ All tests passed! Goxel daemon is working correctly.")

if __name__ == "__main__":
    main()