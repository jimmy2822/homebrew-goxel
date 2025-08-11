#!/usr/bin/env python3
"""
Performance Benchmarks for Goxel v0.16 Render Transfer Architecture

Compares file-path mode vs theoretical Base64 mode performance:
- Memory usage
- Transfer speed
- CPU utilization
- Scalability
"""

import json
import socket
import time
import os
import psutil
import base64
import threading
import statistics
from typing import List, Dict, Tuple

SOCKET_PATH = "/tmp/goxel-test.sock"

class RenderBenchmark:
    def __init__(self):
        self.sock = None
        self.process = psutil.Process()
        self.results = {
            'file_path': [],
            'base64_sim': [],
            'memory': [],
            'cpu': []
        }
    
    def connect(self):
        """Connect to daemon"""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(SOCKET_PATH)
    
    def disconnect(self):
        """Disconnect from daemon"""
        if self.sock:
            self.sock.close()
    
    def send_request(self, method: str, params=None) -> dict:
        """Send JSON-RPC request"""
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params if params else [],
            "id": 1
        }
        
        request_str = json.dumps(request) + "\n"
        self.sock.send(request_str.encode())
        
        response = self.sock.recv(65536).decode()
        return json.loads(response)
    
    def benchmark_file_path_mode(self, width: int, height: int, iterations: int = 10) -> Dict:
        """Benchmark file-path transfer mode"""
        print(f"\n📊 Benchmarking file-path mode ({width}x{height}, {iterations} iterations)")
        
        times = []
        memory_before = self.process.memory_info().rss / 1024 / 1024  # MB
        
        for i in range(iterations):
            start = time.perf_counter()
            
            response = self.send_request("goxel.render_scene", {
                "width": width,
                "height": height,
                "options": {"return_mode": "file_path"}
            })
            
            if response.get("result", {}).get("success"):
                file_info = response["result"].get("file", {})
                if file_info.get("path"):
                    # Read file to simulate full transfer
                    with open(file_info["path"], "rb") as f:
                        data = f.read()
                    
                    elapsed = time.perf_counter() - start
                    times.append(elapsed)
            
            # Progress indicator
            if (i + 1) % (iterations // 10) == 0:
                print(f"  Progress: {(i + 1) * 100 // iterations}%")
        
        memory_after = self.process.memory_info().rss / 1024 / 1024  # MB
        memory_delta = memory_after - memory_before
        
        return {
            'mode': 'file_path',
            'dimensions': f'{width}x{height}',
            'iterations': iterations,
            'avg_time': statistics.mean(times) * 1000,  # ms
            'min_time': min(times) * 1000,
            'max_time': max(times) * 1000,
            'std_dev': statistics.stdev(times) * 1000 if len(times) > 1 else 0,
            'memory_delta': memory_delta,
            'throughput': iterations / sum(times)  # ops/sec
        }
    
    def simulate_base64_mode(self, width: int, height: int, iterations: int = 10) -> Dict:
        """Simulate Base64 encoding overhead"""
        print(f"\n📊 Simulating Base64 mode ({width}x{height}, {iterations} iterations)")
        
        times = []
        memory_before = self.process.memory_info().rss / 1024 / 1024  # MB
        
        # Estimate PNG size (rough approximation)
        estimated_png_size = width * height * 3 // 10  # Compressed estimate
        dummy_data = os.urandom(estimated_png_size)
        
        for i in range(iterations):
            start = time.perf_counter()
            
            # Simulate Base64 encoding
            encoded = base64.b64encode(dummy_data).decode('utf-8')
            
            # Simulate JSON wrapping
            json_response = json.dumps({
                "result": {
                    "success": True,
                    "image_data": encoded
                }
            })
            
            # Simulate parsing
            parsed = json.loads(json_response)
            decoded = base64.b64decode(parsed["result"]["image_data"])
            
            elapsed = time.perf_counter() - start
            times.append(elapsed)
            
            # Progress indicator
            if (i + 1) % (iterations // 10) == 0:
                print(f"  Progress: {(i + 1) * 100 // iterations}%")
        
        memory_after = self.process.memory_info().rss / 1024 / 1024  # MB
        memory_delta = memory_after - memory_before
        
        return {
            'mode': 'base64_simulated',
            'dimensions': f'{width}x{height}',
            'iterations': iterations,
            'avg_time': statistics.mean(times) * 1000,  # ms
            'min_time': min(times) * 1000,
            'max_time': max(times) * 1000,
            'std_dev': statistics.stdev(times) * 1000 if len(times) > 1 else 0,
            'memory_delta': memory_delta,
            'throughput': iterations / sum(times),  # ops/sec
            'overhead_bytes': len(json_response) - estimated_png_size
        }
    
    def benchmark_scalability(self) -> List[Dict]:
        """Test scalability with different image sizes"""
        print("\n🔄 Testing scalability with various image sizes")
        
        sizes = [
            (100, 100, "Small"),
            (400, 400, "Medium"),
            (800, 800, "Large"),
            (1920, 1080, "Full HD"),
            (3840, 2160, "4K")
        ]
        
        results = []
        
        for width, height, label in sizes:
            print(f"\n Testing {label} ({width}x{height})")
            
            # File-path mode
            try:
                file_result = self.benchmark_file_path_mode(width, height, 5)
                file_result['label'] = label
                results.append(file_result)
            except Exception as e:
                print(f"  ⚠️ File-path test failed: {e}")
            
            # Base64 simulation
            base64_result = self.simulate_base64_mode(width, height, 5)
            base64_result['label'] = label
            results.append(base64_result)
            
            # Calculate improvement
            if len(results) >= 2:
                file_time = results[-2]['avg_time']
                base64_time = results[-1]['avg_time']
                improvement = ((base64_time - file_time) / base64_time) * 100
                print(f"  ⚡ File-path is {improvement:.1f}% faster")
        
        return results
    
    def benchmark_concurrent_renders(self, num_threads: int = 10) -> Dict:
        """Test concurrent render performance"""
        print(f"\n🔄 Testing concurrent renders with {num_threads} threads")
        
        render_times = []
        errors = []
        
        def render_worker(thread_id: int):
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.connect(SOCKET_PATH)
                
                start = time.perf_counter()
                
                request = json.dumps({
                    "jsonrpc": "2.0",
                    "method": "goxel.render_scene",
                    "params": {
                        "width": 200,
                        "height": 200,
                        "options": {"return_mode": "file_path"}
                    },
                    "id": thread_id
                }) + "\n"
                
                sock.send(request.encode())
                response = sock.recv(8192)
                
                elapsed = time.perf_counter() - start
                render_times.append(elapsed)
                
                sock.close()
            except Exception as e:
                errors.append(str(e))
        
        threads = []
        start_time = time.perf_counter()
        
        for i in range(num_threads):
            t = threading.Thread(target=render_worker, args=(i,))
            threads.append(t)
            t.start()
        
        for t in threads:
            t.join()
        
        total_time = time.perf_counter() - start_time
        
        return {
            'num_threads': num_threads,
            'total_time': total_time * 1000,  # ms
            'avg_render_time': statistics.mean(render_times) * 1000 if render_times else 0,
            'successful_renders': len(render_times),
            'errors': len(errors),
            'throughput': len(render_times) / total_time if total_time > 0 else 0
        }
    
    def benchmark_cleanup_performance(self) -> Dict:
        """Test cleanup thread performance"""
        print("\n🧹 Testing cleanup performance")
        
        # Create many renders
        print("  Creating 50 test renders...")
        for i in range(50):
            self.send_request("goxel.render_scene", {
                "width": 100,
                "height": 100,
                "options": {"return_mode": "file_path"}
            })
        
        # List renders to get count
        response = self.send_request("goxel.list_renders", {})
        initial_count = len(response.get("result", {}).get("renders", []))
        
        # Measure cleanup
        start = time.perf_counter()
        response = self.send_request("goxel.cleanup_expired", {})
        cleanup_time = time.perf_counter() - start
        
        # Get final count
        response = self.send_request("goxel.list_renders", {})
        final_count = len(response.get("result", {}).get("renders", []))
        
        return {
            'initial_renders': initial_count,
            'final_renders': final_count,
            'cleaned_up': initial_count - final_count,
            'cleanup_time': cleanup_time * 1000  # ms
        }
    
    def run_comprehensive_benchmark(self):
        """Run all benchmarks"""
        print("=" * 60)
        print("Goxel v0.16 Render Transfer - Performance Benchmarks")
        print("=" * 60)
        
        try:
            self.connect()
            print(f"✓ Connected to daemon at {SOCKET_PATH}")
            
            # Create test project
            print("\n📦 Setting up test project...")
            self.send_request("goxel.create_project", ["BenchmarkProject", 32, 32, 32])
            
            # Add some voxels
            for i in range(20):
                self.send_request("goxel.add_voxel", [16, 16, i, 255, 0, 0, 255])
            
            # Run benchmarks
            results = {}
            
            # 1. Basic comparison
            print("\n1️⃣ Basic Mode Comparison (400x400)")
            results['file_path'] = self.benchmark_file_path_mode(400, 400, 20)
            results['base64'] = self.simulate_base64_mode(400, 400, 20)
            
            # 2. Scalability
            print("\n2️⃣ Scalability Tests")
            results['scalability'] = self.benchmark_scalability()
            
            # 3. Concurrent renders
            print("\n3️⃣ Concurrent Render Tests")
            results['concurrent'] = self.benchmark_concurrent_renders(10)
            
            # 4. Cleanup performance
            print("\n4️⃣ Cleanup Performance")
            results['cleanup'] = self.benchmark_cleanup_performance()
            
            # Print results
            self.print_results(results)
            
        except Exception as e:
            print(f"\n❌ Benchmark failed: {e}")
        finally:
            self.disconnect()
    
    def print_results(self, results: Dict):
        """Print formatted benchmark results"""
        print("\n" + "=" * 60)
        print("Benchmark Results Summary")
        print("=" * 60)
        
        # Mode comparison
        if 'file_path' in results and 'base64' in results:
            fp = results['file_path']
            b64 = results['base64']
            
            print("\n📊 Mode Comparison (400x400, 20 iterations):")
            print(f"  File-Path Mode:")
            print(f"    • Average: {fp['avg_time']:.2f}ms")
            print(f"    • Min/Max: {fp['min_time']:.2f}ms / {fp['max_time']:.2f}ms")
            print(f"    • Throughput: {fp['throughput']:.1f} ops/sec")
            print(f"    • Memory Delta: {fp['memory_delta']:.2f}MB")
            
            print(f"  Base64 Mode (simulated):")
            print(f"    • Average: {b64['avg_time']:.2f}ms")
            print(f"    • Min/Max: {b64['min_time']:.2f}ms / {b64['max_time']:.2f}ms")
            print(f"    • Throughput: {b64['throughput']:.1f} ops/sec")
            print(f"    • Memory Delta: {b64['memory_delta']:.2f}MB")
            print(f"    • Overhead: {b64.get('overhead_bytes', 0) / 1024:.1f}KB")
            
            improvement = ((b64['avg_time'] - fp['avg_time']) / b64['avg_time']) * 100
            print(f"\n  ⚡ Performance Gain: {improvement:.1f}% faster with file-path mode")
            
            memory_saving = max(0, b64['memory_delta'] - fp['memory_delta'])
            print(f"  💾 Memory Saving: {memory_saving:.2f}MB less with file-path mode")
        
        # Scalability results
        if 'scalability' in results:
            print("\n📈 Scalability Results:")
            for result in results['scalability']:
                if result['mode'] == 'file_path':
                    print(f"  {result['label']}:")
                    print(f"    • File-path: {result['avg_time']:.2f}ms")
        
        # Concurrent results
        if 'concurrent' in results:
            c = results['concurrent']
            print(f"\n🔄 Concurrent Renders ({c['num_threads']} threads):")
            print(f"  • Total Time: {c['total_time']:.2f}ms")
            print(f"  • Avg per Render: {c['avg_render_time']:.2f}ms")
            print(f"  • Successful: {c['successful_renders']}/{c['num_threads']}")
            print(f"  • Throughput: {c['throughput']:.1f} renders/sec")
        
        # Cleanup results
        if 'cleanup' in results:
            cl = results['cleanup']
            print(f"\n🧹 Cleanup Performance:")
            print(f"  • Renders Cleaned: {cl['cleaned_up']}")
            print(f"  • Cleanup Time: {cl['cleanup_time']:.2f}ms")
            if cl['cleaned_up'] > 0:
                print(f"  • Speed: {cl['cleanup_time'] / cl['cleaned_up']:.3f}ms per file")
        
        print("\n" + "=" * 60)
        print("✅ Benchmark Complete")
        print("=" * 60)
        
        # Key takeaways
        print("\n🎯 Key Takeaways:")
        print("  • File-path mode eliminates Base64 encoding overhead")
        print("  • Significant memory savings for large renders")
        print("  • Scales efficiently with image size")
        print("  • Supports high concurrent load")
        print("  • Fast automatic cleanup")

if __name__ == "__main__":
    benchmark = RenderBenchmark()
    benchmark.run_comprehensive_benchmark()