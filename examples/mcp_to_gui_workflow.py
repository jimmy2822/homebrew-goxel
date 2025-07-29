#!/usr/bin/env python3
"""
MCP to GUI Workflow Demonstration

This script demonstrates the complete workflow:
1. Create 3D models using MCP/JSON-RPC (daemon)
2. Save models to files
3. Open in GUI version for visual editing

Prerequisites:
- goxel-daemon running: ./goxel-daemon --foreground --socket /tmp/goxel.sock
- GUI version built: ./goxel
"""

import json
import socket
import sys
import subprocess
import os
import time


class GoxelMCPWorkflow:
    def __init__(self, socket_path='/tmp/goxel.sock'):
        self.socket_path = socket_path
        self.sock = None
        self.request_id = 0
    
    def connect_daemon(self):
        """Connect to Goxel daemon for headless operations."""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            self.sock.connect(self.socket_path)
            print(f"✓ Connected to Goxel daemon at {self.socket_path}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect to daemon: {e}")
            print("  Make sure to start: ./goxel-daemon --foreground --socket /tmp/goxel.sock")
            return False
    
    def call_daemon(self, method, params=None):
        """Make JSON-RPC call to daemon."""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params or {}
        }
        
        request_str = json.dumps(request) + '\n'
        self.sock.send(request_str.encode('utf-8'))
        
        response_data = self.sock.recv(65536).decode('utf-8')
        response = json.loads(response_data)
        
        if 'error' in response:
            raise Exception(f"Daemon RPC Error: {response['error']}")
        
        return response.get('result')
    
    def create_sample_model(self, model_name="mcp_demo"):
        """Create a sample 3D model using daemon API (simulating MCP)."""
        print(f"\n🎯 Creating 3D model '{model_name}' using daemon...")
        
        # Step 1: Create new project
        self.call_daemon('goxel.create_project', {'name': model_name})
        print("  ✓ Project created")
        
        # Step 2: Create a colorful tower
        colors = [
            {'r': 255, 'g': 0, 'b': 0, 'a': 255},    # Red base
            {'r': 0, 'g': 255, 'b': 0, 'a': 255},    # Green middle
            {'r': 0, 'g': 0, 'b': 255, 'a': 255},    # Blue top
        ]
        
        # Create 3-layer tower (on the visual grid at Y=-16)
        for layer, color in enumerate(colors):
            # Create base layer (3x3)
            for x in range(-1, 2):
                for z in range(-1, 2):
                    self.call_daemon('goxel.add_voxel', {
                        'x': x, 'y': -16 + layer, 'z': z,
                        **color
                    })
        
        print(f"  ✓ Created colorful tower with {len(colors)} layers")
        
        # Step 3: Save as Goxel native format
        gox_file = f"{model_name}.gox"
        self.call_daemon('goxel.save_project', {'path': gox_file})
        print(f"  ✓ Saved as {gox_file}")
        
        # Step 4: Also export as OBJ for compatibility
        obj_file = f"{model_name}.obj"
        self.call_daemon('goxel.export_model', {
            'format': 'obj',
            'path': obj_file
        })
        print(f"  ✓ Exported as {obj_file}")
        
        return gox_file, obj_file
    
    def disconnect_daemon(self):
        """Disconnect from daemon."""
        if self.sock:
            self.sock.close()
            self.sock = None
    
    def open_in_gui(self, model_file):
        """Open the created model in GUI version."""
        print(f"\n🖼️  Opening model in GUI: {model_file}")
        
        if not os.path.exists(model_file):
            print(f"✗ Model file not found: {model_file}")
            return False
        
        gui_executable = "./goxel"
        if not os.path.exists(gui_executable):
            print(f"✗ GUI executable not found: {gui_executable}")
            print("  Make sure to build with: make")
            return False
        
        try:
            # Open GUI with the model file
            subprocess.Popen([gui_executable, model_file])
            print("  ✓ GUI launched successfully!")
            print("  → You can now visually edit the model in the GUI")
            return True
        except Exception as e:
            print(f"✗ Failed to launch GUI: {e}")
            return False
    
    def run_complete_workflow(self):
        """Run the complete MCP → Daemon → GUI workflow."""
        print("🚀 MCP to GUI Workflow Demonstration")
        print("="*50)
        
        # Step 1: Connect to daemon
        if not self.connect_daemon():
            return False
        
        try:
            # Step 2: Create model using daemon (simulating MCP tools)
            gox_file, obj_file = self.create_sample_model("mcp_demo_tower")
            
            # Step 3: Disconnect from daemon
            self.disconnect_daemon()
            
            # Step 4: Open in GUI
            success = self.open_in_gui(gox_file)
            
            if success:
                print("\n🎉 Workflow completed successfully!")
                print("="*50)
                print("Files created:")
                print(f"  - {gox_file} (Goxel project - open in GUI)")
                print(f"  - {obj_file} (3D mesh - for other applications)")
                print("\nNext steps:")
                print("  1. Edit the model in the GUI")
                print("  2. Add more details and colors")
                print("  3. Export to other formats if needed")
                print("  4. Save your changes")
                
                return True
            else:
                return False
                
        except Exception as e:
            print(f"✗ Workflow failed: {e}")
            self.disconnect_daemon()
            return False


def check_prerequisites():
    """Check if daemon and GUI are available."""
    print("🔍 Checking prerequisites...")
    
    # Check if daemon socket exists
    socket_path = '/tmp/goxel.sock'
    if not os.path.exists(socket_path):
        print(f"⚠️  Daemon socket not found: {socket_path}")
        print("   Start daemon with: ./goxel-daemon --foreground --socket /tmp/goxel.sock")
        return False
    
    # Check if GUI executable exists
    gui_exe = './goxel'
    if not os.path.exists(gui_exe):
        print(f"⚠️  GUI executable not found: {gui_exe}")
        print("   Build GUI with: make")
        return False
    
    print("✓ All prerequisites satisfied")
    return True


def main():
    """Main entry point."""
    print("🤖 Goxel MCP → Daemon → GUI Workflow")
    print("This demonstrates the complete integration workflow\n")
    
    # Check prerequisites
    if not check_prerequisites():
        print("\n❌ Prerequisites not met. Please set up daemon and GUI first.")
        return 1
    
    # Run workflow
    workflow = GoxelMCPWorkflow()
    success = workflow.run_complete_workflow()
    
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())