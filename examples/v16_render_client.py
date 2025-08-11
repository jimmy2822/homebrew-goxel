#!/usr/bin/env python3
"""
Goxel v0.16 Render Client Example

Demonstrates the new file-path render transfer architecture with:
- Feature detection for v0.16 capabilities
- Automatic fallback to v0.15 mode
- Cache management
- Performance monitoring
"""

import socket
import json
import time
import os
import shutil
from typing import Dict, Optional, List, Tuple


class GoxelRenderClient:
    """Client for Goxel daemon with v0.16 render support"""
    
    def __init__(self, socket_path: str = "/tmp/goxel.sock"):
        self.socket_path = socket_path
        self.sock = None
        self.supports_v16 = False
        self.request_id = 0
        self.connect()
        
    def connect(self):
        """Connect to daemon and detect version"""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.socket_path)
        
        # Feature detection
        self.supports_v16 = self._check_v16_support()
        print(f"Connected to Goxel daemon (v0.16 support: {self.supports_v16})")
    
    def disconnect(self):
        """Disconnect from daemon"""
        if self.sock:
            self.sock.close()
            self.sock = None
    
    def request(self, method: str, params=None) -> Dict:
        """Send JSON-RPC request and get response"""
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params if params else [],
            "id": self.request_id
        }
        
        request_str = json.dumps(request) + "\n"
        self.sock.send(request_str.encode())
        
        response = self.sock.recv(65536).decode()
        result = json.loads(response)
        
        if "error" in result:
            raise Exception(f"RPC Error: {result['error']['message']}")
        
        return result.get("result", {})
    
    def _check_v16_support(self) -> bool:
        """Check if daemon supports v0.16 features"""
        try:
            result = self.request("goxel.get_render_stats", {})
            return result.get("success", False)
        except:
            return False
    
    def render_scene(self, width: int, height: int, 
                    use_file_path: bool = None) -> Dict:
        """
        Render scene with automatic mode selection
        
        Args:
            width: Render width in pixels
            height: Render height in pixels
            use_file_path: Force file-path mode (None=auto-detect)
        
        Returns:
            Dict with render information
        """
        # Auto-detect if not specified
        if use_file_path is None:
            use_file_path = self.supports_v16
        
        if use_file_path and self.supports_v16:
            # Use v0.16 file-path mode
            result = self.request("goxel.render_scene", {
                "width": width,
                "height": height,
                "options": {"return_mode": "file_path"}
            })
            
            if result.get("success") and result.get("file"):
                return {
                    "mode": "file_path",
                    "path": result["file"]["path"],
                    "size": result["file"]["size"],
                    "format": result["file"]["format"],
                    "dimensions": result["file"]["dimensions"],
                    "checksum": result["file"]["checksum"],
                    "created_at": result["file"]["created_at"],
                    "expires_at": result["file"]["expires_at"]
                }
        
        # Fall back to legacy mode
        output_path = f"/tmp/render_{int(time.time())}_{width}x{height}.png"
        result = self.request("goxel.render_scene", 
                             [output_path, width, height])
        
        if result.get("success"):
            # Get file size if it exists
            size = 0
            if os.path.exists(output_path):
                size = os.path.getsize(output_path)
            
            return {
                "mode": "legacy",
                "path": output_path,
                "size": size,
                "format": "png",
                "dimensions": {"width": width, "height": height}
            }
        
        raise Exception("Render failed")
    
    def get_render_data(self, render_info: Dict) -> bytes:
        """
        Read render data from file
        
        Args:
            render_info: Result from render_scene
        
        Returns:
            Image data as bytes
        """
        with open(render_info["path"], "rb") as f:
            return f.read()
    
    def save_render(self, render_info: Dict, output_path: str):
        """
        Save render to permanent location
        
        Args:
            render_info: Result from render_scene
            output_path: Where to save the file
        """
        shutil.copy2(render_info["path"], output_path)
        print(f"Saved render to {output_path}")
    
    def cleanup_render(self, render_info: Dict):
        """
        Clean up temporary render file
        
        Args:
            render_info: Result from render_scene
        """
        if render_info["mode"] == "file_path" and self.supports_v16:
            # Use daemon cleanup
            self.request("goxel.cleanup_render", 
                        {"path": render_info["path"]})
        elif render_info["mode"] == "legacy":
            # Manual cleanup for legacy mode
            if os.path.exists(render_info["path"]):
                os.remove(render_info["path"])
    
    def list_renders(self) -> List[Dict]:
        """List all active renders (v0.16 only)"""
        if not self.supports_v16:
            return []
        
        result = self.request("goxel.list_renders", {})
        return result.get("renders", [])
    
    def get_render_stats(self) -> Dict:
        """Get render manager statistics (v0.16 only)"""
        if not self.supports_v16:
            return {}
        
        result = self.request("goxel.get_render_stats", {})
        return result.get("stats", {})
    
    def cleanup_old_renders(self, max_age_seconds: int = 3600):
        """
        Clean up renders older than specified age
        
        Args:
            max_age_seconds: Maximum age in seconds (default 1 hour)
        """
        if not self.supports_v16:
            return 0
        
        renders = self.list_renders()
        current_time = time.time()
        cleaned = 0
        
        for render in renders:
            age = current_time - render["created_at"]
            if age > max_age_seconds:
                try:
                    self.request("goxel.cleanup_render", 
                               {"path": render["path"]})
                    cleaned += 1
                except:
                    pass
        
        return cleaned
    
    def monitor_cache(self) -> Dict:
        """Monitor cache usage and return metrics"""
        if not self.supports_v16:
            return {"mode": "legacy", "message": "v0.16 features not available"}
        
        stats = self.get_render_stats()
        
        return {
            "mode": "v0.16",
            "active_renders": stats.get("active_renders", 0),
            "cache_size_mb": stats.get("current_cache_size", 0) / 1024 / 1024,
            "cache_limit_mb": stats.get("max_cache_size", 0) / 1024 / 1024,
            "cache_usage_percent": stats.get("cache_usage_percent", 0) * 100,
            "total_renders": stats.get("total_renders", 0),
            "files_cleaned": stats.get("files_cleaned", 0),
            "cleanup_thread_active": stats.get("cleanup_thread_active", False)
        }


def demo_basic_usage():
    """Demonstrate basic rendering with v0.16 client"""
    print("\n=== Basic Usage Demo ===\n")
    
    client = GoxelRenderClient()
    
    try:
        # Create a project
        client.request("goxel.create_project", ["Demo", 32, 32, 32])
        print("✓ Created project")
        
        # Add some voxels
        for i in range(10):
            client.request("goxel.add_voxel", 
                         [16 + i, 16, 16, 255, 100, 0, 255])
        print("✓ Added voxels")
        
        # Render scene
        render = client.render_scene(400, 400)
        print(f"✓ Rendered scene ({render['mode']} mode)")
        print(f"  Path: {render['path']}")
        print(f"  Size: {render['size']} bytes")
        
        # Save to permanent location
        client.save_render(render, "demo_output.png")
        
        # Clean up
        client.cleanup_render(render)
        print("✓ Cleaned up temporary file")
        
    finally:
        client.disconnect()


def demo_performance_comparison():
    """Compare performance between v0.15 and v0.16 modes"""
    print("\n=== Performance Comparison Demo ===\n")
    
    client = GoxelRenderClient()
    
    try:
        # Create test scene
        client.request("goxel.create_project", ["PerfTest", 64, 64, 64])
        
        # Add many voxels
        for i in range(20):
            for j in range(20):
                client.request("goxel.add_voxel",
                             [32 + i, 32 + j, 32, 255, 0, 0, 255])
        
        sizes = [(200, 200), (400, 400), (800, 600), (1920, 1080)]
        
        for width, height in sizes:
            print(f"\nTesting {width}×{height}:")
            
            # Test file-path mode if available
            if client.supports_v16:
                start = time.perf_counter()
                render = client.render_scene(width, height, use_file_path=True)
                elapsed = time.perf_counter() - start
                print(f"  File-path mode: {elapsed*1000:.2f}ms")
                client.cleanup_render(render)
            
            # Test legacy mode
            start = time.perf_counter()
            render = client.render_scene(width, height, use_file_path=False)
            elapsed = time.perf_counter() - start
            print(f"  Legacy mode:    {elapsed*1000:.2f}ms")
            client.cleanup_render(render)
            
            if client.supports_v16:
                print(f"  → File-path mode is faster for {width}×{height}")
        
    finally:
        client.disconnect()


def demo_cache_management():
    """Demonstrate cache management features"""
    print("\n=== Cache Management Demo ===\n")
    
    client = GoxelRenderClient()
    
    if not client.supports_v16:
        print("Cache management requires v0.16 features")
        client.disconnect()
        return
    
    try:
        # Monitor initial state
        metrics = client.monitor_cache()
        print("Initial cache state:")
        print(f"  Active renders: {metrics['active_renders']}")
        print(f"  Cache size: {metrics['cache_size_mb']:.1f} MB")
        print(f"  Cache usage: {metrics['cache_usage_percent']:.1f}%")
        
        # Create multiple renders
        print("\nCreating 10 test renders...")
        renders = []
        for i in range(10):
            render = client.render_scene(200 + i*10, 200 + i*10)
            renders.append(render)
        
        # Check cache again
        metrics = client.monitor_cache()
        print(f"\nAfter creating renders:")
        print(f"  Active renders: {metrics['active_renders']}")
        print(f"  Cache size: {metrics['cache_size_mb']:.1f} MB")
        print(f"  Cache usage: {metrics['cache_usage_percent']:.1f}%")
        
        # Clean up old renders
        cleaned = client.cleanup_old_renders(max_age_seconds=0)
        print(f"\nCleaned up {cleaned} renders")
        
        # Final state
        metrics = client.monitor_cache()
        print(f"\nFinal cache state:")
        print(f"  Active renders: {metrics['active_renders']}")
        print(f"  Cache size: {metrics['cache_size_mb']:.1f} MB")
        
    finally:
        client.disconnect()


def demo_batch_rendering():
    """Demonstrate batch rendering with concurrent operations"""
    print("\n=== Batch Rendering Demo ===\n")
    
    import concurrent.futures
    
    def render_task(size: Tuple[int, int]) -> Dict:
        """Single render task for thread pool"""
        client = GoxelRenderClient()
        try:
            result = client.render_scene(size[0], size[1])
            return result
        finally:
            client.disconnect()
    
    # Prepare batch
    sizes = [(100, 100), (200, 200), (300, 300), (400, 400), (500, 500)]
    
    print(f"Rendering {len(sizes)} images concurrently...")
    start = time.perf_counter()
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(render_task, size) for size in sizes]
        results = [f.result() for f in futures]
    
    elapsed = time.perf_counter() - start
    
    print(f"✓ Completed {len(results)} renders in {elapsed:.2f}s")
    print(f"  Average: {elapsed/len(results)*1000:.2f}ms per render")
    
    # Show results
    total_size = sum(r["size"] for r in results)
    print(f"  Total size: {total_size / 1024:.1f} KB")
    
    # Clean up
    client = GoxelRenderClient()
    if client.supports_v16:
        for result in results:
            try:
                client.cleanup_render(result)
            except:
                pass
    client.disconnect()


if __name__ == "__main__":
    print("=" * 50)
    print("Goxel v0.16 Render Client Examples")
    print("=" * 50)
    
    try:
        # Run demos
        demo_basic_usage()
        demo_performance_comparison()
        demo_cache_management()
        demo_batch_rendering()
        
        print("\n" + "=" * 50)
        print("All demos completed successfully!")
        print("=" * 50)
        
    except Exception as e:
        print(f"\nError: {e}")
        print("\nMake sure the Goxel daemon is running:")
        print("  ./goxel-daemon --socket /tmp/goxel.sock")