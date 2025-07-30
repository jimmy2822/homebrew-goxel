#!/usr/bin/env python3
"""
Color Analysis Demo for Goxel v14 Daemon

Demonstrates the three color analysis methods:
1. Color histogram generation
2. Finding voxels by color
3. Getting unique colors

Usage:
    python3 color_analysis_demo.py [socket_path]
"""

import json
import socket
import sys
import os

class GoxelClient:
    def __init__(self, socket_path="/tmp/goxel.sock"):
        self.socket_path = socket_path
        self.request_id = 0
        
    def request(self, method, params=None):
        """Send JSON-RPC request and return result."""
        self.request_id += 1
        
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "id": self.request_id
        }
        
        if params:
            request["params"] = params
            
        # Connect and send
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.socket_path)
        sock.sendall(json.dumps(request).encode() + b'\n')
        
        # Read response
        response_data = b""
        while b'\n' not in response_data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response_data += chunk
                
        sock.close()
        
        # Parse and return result
        response = json.loads(response_data.decode().strip())
        if "error" in response:
            raise Exception(f"RPC Error: {response['error']}")
        return response.get("result")


def create_colorful_scene(client):
    """Create a scene with various colors for demonstration."""
    print("Creating colorful test scene...")
    
    # Create new project
    client.request("goxel.create_project")
    
    # Create a rainbow cube
    colors = [
        ([255, 0, 0], "Red"),      # Red
        ([255, 127, 0], "Orange"),  # Orange  
        ([255, 255, 0], "Yellow"),  # Yellow
        ([0, 255, 0], "Green"),     # Green
        ([0, 0, 255], "Blue"),      # Blue
        ([75, 0, 130], "Indigo"),   # Indigo
        ([148, 0, 211], "Violet")   # Violet
    ]
    
    # Add voxels for each color
    for i, (color, name) in enumerate(colors):
        print(f"  Adding {name} voxels...")
        for j in range(5):  # 5 voxels per color
            client.request("goxel.add_voxel", {
                "x": i * 2,
                "y": j,
                "z": 0,
                "color": color + [255]  # Add alpha
            })
    
    # Add some scattered white voxels
    print("  Adding white accent voxels...")
    for i in range(7):
        client.request("goxel.add_voxel", {
            "x": i * 2,
            "y": 6,
            "z": 0,
            "color": [255, 255, 255, 255]
        })
    
    print("Scene created with 42 voxels in 8 colors!\n")


def demo_color_histogram(client):
    """Demonstrate color histogram analysis."""
    print("=== Color Histogram Demo ===")
    
    # Get exact color histogram
    print("\n1. Exact color histogram (no binning):")
    result = client.request("goxel.get_color_histogram", {
        "sort_by_count": True
    })
    
    print(f"   Total voxels: {result['total_voxels']}")
    print(f"   Unique colors: {result['unique_colors']}")
    print("\n   Top 5 colors:")
    for entry in result['histogram'][:5]:
        print(f"   - {entry['color']}: {entry['count']} voxels ({entry['percentage']:.1f}%)")
    
    # Get binned histogram
    print("\n2. Binned color histogram (bin_size=32):")
    result = client.request("goxel.get_color_histogram", {
        "bin_size": 32,
        "sort_by_count": True,
        "top_n": 5
    })
    
    print(f"   Colors after binning: {result['unique_colors']}")
    print("\n   Top 5 binned colors:")
    for entry in result['histogram']:
        print(f"   - {entry['color']}: {entry['count']} voxels ({entry['percentage']:.1f}%)")


def demo_find_by_color(client):
    """Demonstrate finding voxels by color."""
    print("\n\n=== Find Voxels by Color Demo ===")
    
    # Find exact red voxels
    print("\n1. Finding exact red voxels:")
    result = client.request("goxel.find_voxels_by_color", {
        "color": [255, 0, 0],
        "include_locations": True
    })
    
    print(f"   Found {result['match_count']} red voxels")
    if result['locations']:
        print("   First 3 locations:")
        for loc in result['locations'][:3]:
            print(f"   - Position: ({loc['x']}, {loc['y']}, {loc['z']})")
    
    # Find with tolerance
    print("\n2. Finding reddish voxels (with tolerance):")
    result = client.request("goxel.find_voxels_by_color", {
        "color": [255, 50, 50],
        "tolerance": 100,  # Very tolerant
        "include_locations": False
    })
    
    print(f"   Found {result['match_count']} reddish voxels")
    
    # Find white voxels in specific region
    print("\n3. Finding white voxels in upper region:")
    result = client.request("goxel.find_voxels_by_color", {
        "color": [255, 255, 255],
        "region": {
            "min": [0, 5, 0],
            "max": [20, 10, 5]
        },
        "include_locations": True
    })
    
    print(f"   Found {result['match_count']} white voxels in upper region")


def demo_unique_colors(client):
    """Demonstrate getting unique colors."""
    print("\n\n=== Unique Colors Demo ===")
    
    # Get all unique colors
    print("\n1. All unique colors:")
    result = client.request("goxel.get_unique_colors")
    
    print(f"   Found {result['count']} unique colors:")
    for color in result['colors']:
        print(f"   - {color['hex']} (RGB: {color['rgba'][:3]})")
    
    # Get with similar color merging
    print("\n2. Unique colors with merging (threshold=50):")
    result = client.request("goxel.get_unique_colors", {
        "merge_similar": True,
        "merge_threshold": 50,
        "sort_by_count": True
    })
    
    print(f"   After merging: {result['count']} color groups")
    for color in result['colors']:
        print(f"   - {color['hex']}")


def color_palette_example(client):
    """Example: Extract and save a color palette."""
    print("\n\n=== Practical Example: Color Palette Extraction ===")
    
    # Get top 10 most used colors
    result = client.request("goxel.get_color_histogram", {
        "sort_by_count": True,
        "top_n": 10
    })
    
    print("Extracted color palette (top 10 colors):")
    print("\nCSS Variables:")
    for i, entry in enumerate(result['histogram']):
        css_name = f"--color-{i+1}"
        print(f"{css_name}: {entry['color']};  /* {entry['count']} voxels */")
    
    print("\nRGB Values (for graphic design):")
    for i, entry in enumerate(result['histogram']):
        rgba = entry['rgba']
        print(f"Color {i+1}: rgb({rgba[0]}, {rgba[1]}, {rgba[2]})")


def main():
    # Parse arguments
    socket_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/goxel.sock"
    
    # Check if daemon is running
    if not os.path.exists(socket_path):
        print(f"Error: Goxel daemon not found at {socket_path}")
        print("Please start the daemon first:")
        print("  ./goxel-daemon --foreground --socket /tmp/goxel.sock")
        return 1
    
    # Create client
    client = GoxelClient(socket_path)
    
    try:
        # Create test scene
        create_colorful_scene(client)
        
        # Run demonstrations
        demo_color_histogram(client)
        demo_find_by_color(client)
        demo_unique_colors(client)
        color_palette_example(client)
        
        print("\n✅ Color analysis demo completed successfully!")
        return 0
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())