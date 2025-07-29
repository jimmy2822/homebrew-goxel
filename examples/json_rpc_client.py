#!/usr/bin/env python3
"""
Goxel v14.0 Daemon JSON-RPC Client Example

This example demonstrates how to connect to the Goxel daemon
and perform basic voxel operations using Python.
"""

import json
import socket
import sys
import base64


class GoxelClient:
    def __init__(self, socket_path='/tmp/goxel.sock'):
        self.socket_path = socket_path
        self.sock = None
        self.request_id = 0
    
    def connect(self):
        """Connect to the Goxel daemon."""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            self.sock.connect(self.socket_path)
            print(f"Connected to Goxel daemon at {self.socket_path}")
        except Exception as e:
            print(f"Failed to connect: {e}")
            sys.exit(1)
    
    def disconnect(self):
        """Disconnect from the daemon."""
        if self.sock:
            self.sock.close()
            self.sock = None
    
    def call(self, method, params=None):
        """Make a JSON-RPC call to the daemon."""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params or {}
        }
        
        # Send request
        request_str = json.dumps(request) + '\n'
        self.sock.send(request_str.encode('utf-8'))
        
        # Receive response
        response_data = self.sock.recv(65536).decode('utf-8')
        response = json.loads(response_data)
        
        if 'error' in response:
            raise Exception(f"RPC Error: {response['error']}")
        
        return response.get('result')


def main():
    # Create client and connect
    client = GoxelClient()
    client.connect()
    
    try:
        # Get daemon version
        version = client.call('get_version')
        print(f"Goxel daemon version: {version}")
        
        # Create a new project
        print("\nCreating new project...")
        client.call('create_project', {'name': 'python_demo'})
        
        # Add some voxels to create a small pyramid
        print("Adding voxels...")
        colors = [
            [255, 0, 0, 255],    # Red
            [0, 255, 0, 255],    # Green
            [0, 0, 255, 255],    # Blue
            [255, 255, 0, 255],  # Yellow
        ]
        
        # Base layer (3x3)
        for x in range(-1, 2):
            for z in range(-1, 2):
                client.call('add_voxel', {
                    'x': x, 'y': 0, 'z': z,
                    'r': colors[0][0], 'g': colors[0][1], 
                    'b': colors[0][2], 'a': colors[0][3]
                })
        
        # Middle layer (2x2)
        for x in range(0, 2):
            for z in range(0, 2):
                client.call('add_voxel', {
                    'x': x - 0.5, 'y': 1, 'z': z - 0.5,
                    'r': colors[1][0], 'g': colors[1][1],
                    'b': colors[1][2], 'a': colors[1][3]
                })
        
        # Top layer (1x1)
        client.call('add_voxel', {
            'x': 0, 'y': 2, 'z': 0,
            'r': colors[2][0], 'g': colors[2][1],
            'b': colors[2][2], 'a': colors[2][3]
        })
        
        # Save the project
        print("Saving project...")
        client.call('save_project', {'path': 'pyramid.gox'})
        
        # Export as OBJ
        print("Exporting as OBJ...")
        client.call('export_project', {
            'format': 'obj',
            'path': 'pyramid.obj'
        })
        
        # Render an image
        print("Rendering preview...")
        result = client.call('render_image', {
            'width': 512,
            'height': 512,
            'camera_position': [5, 5, 5],
            'camera_target': [0, 1, 0]
        })
        
        # Save rendered image
        if result and 'image' in result:
            image_data = base64.b64decode(result['image'])
            with open('pyramid_preview.png', 'wb') as f:
                f.write(image_data)
            print("Preview saved as pyramid_preview.png")
        
        print("\nDemo completed successfully!")
        print("Files created:")
        print("  - pyramid.gox (Goxel project)")
        print("  - pyramid.obj (3D model)")
        print("  - pyramid_preview.png (rendered preview)")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.disconnect()


if __name__ == '__main__':
    main()