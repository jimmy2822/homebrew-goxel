#!/usr/bin/env python3
"""
Goxel v13.4 Performance Baseline Benchmarking
Author: James O'Brien (Agent-4)
Date: January 2025

This script establishes performance baselines for v13.4 headless mode
to compare against v14.6 daemon architecture improvements.
"""

import json
import os
import subprocess
import time
import statistics
import argparse
import sys
from datetime import datetime
import psutil

class V134Benchmarker:
    def __init__(self, cli_path, output_file):
        self.cli_path = cli_path
        self.output_file = output_file
        self.results = {
            "timestamp": datetime.utcnow().isoformat(),
            "version": "13.4",
            "benchmarks": {}
        }
        
    def run_command(self, args):
        """Execute a CLI command and measure its performance."""
        cmd = [self.cli_path] + args
        
        # Measure startup + execution time
        start_time = time.perf_counter()
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, 
                                 stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
        end_time = time.perf_counter()
        
        elapsed_ms = (end_time - start_time) * 1000
        
        return {
            "elapsed_ms": elapsed_ms,
            "exit_code": process.returncode,
            "stdout": stdout.decode('utf-8'),
            "stderr": stderr.decode('utf-8')
        }
    
    def benchmark_startup(self, iterations=100):
        """Benchmark CLI startup time."""
        print(f"Benchmarking startup time ({iterations} iterations)...")
        
        times = []
        for i in range(iterations):
            result = self.run_command(["--version"])
            if result["exit_code"] == 0:
                times.append(result["elapsed_ms"])
            
            if (i + 1) % 10 == 0:
                print(f"  Progress: {i + 1}/{iterations}", end='\r')
        
        print()
        
        self.results["benchmarks"]["startup"] = {
            "iterations": len(times),
            "min_ms": min(times),
            "max_ms": max(times),
            "avg_ms": statistics.mean(times),
            "median_ms": statistics.median(times),
            "stddev_ms": statistics.stdev(times) if len(times) > 1 else 0,
            "p95_ms": statistics.quantiles(times, n=20)[18],  # 95th percentile
            "p99_ms": statistics.quantiles(times, n=100)[98]  # 99th percentile
        }
        
        print(f"  Average: {self.results['benchmarks']['startup']['avg_ms']:.2f} ms")
        
    def benchmark_create_operation(self, iterations=50):
        """Benchmark file creation operation."""
        print(f"Benchmarking create operation ({iterations} iterations)...")
        
        times = []
        for i in range(iterations):
            temp_file = f"/tmp/test_create_{i}.gox"
            result = self.run_command(["create", temp_file])
            
            if result["exit_code"] == 0:
                times.append(result["elapsed_ms"])
                # Cleanup
                if os.path.exists(temp_file):
                    os.remove(temp_file)
            
            if (i + 1) % 10 == 0:
                print(f"  Progress: {i + 1}/{iterations}", end='\r')
        
        print()
        
        self.results["benchmarks"]["create_operation"] = {
            "iterations": len(times),
            "min_ms": min(times),
            "max_ms": max(times),
            "avg_ms": statistics.mean(times),
            "median_ms": statistics.median(times),
            "stddev_ms": statistics.stdev(times) if len(times) > 1 else 0
        }
        
        print(f"  Average: {self.results['benchmarks']['create_operation']['avg_ms']:.2f} ms")
    
    def benchmark_voxel_operations(self, iterations=50):
        """Benchmark voxel manipulation operations."""
        print(f"Benchmarking voxel operations ({iterations} iterations)...")
        
        # Create a test file
        test_file = "/tmp/test_voxels.gox"
        self.run_command(["create", test_file])
        
        times = []
        for i in range(iterations):
            # Add voxel at different positions
            x, y, z = i % 10, -16, i % 10
            result = self.run_command([
                "add-voxel", str(x), str(y), str(z), 
                "255", "0", "0", "255", test_file
            ])
            
            if result["exit_code"] == 0:
                times.append(result["elapsed_ms"])
            
            if (i + 1) % 10 == 0:
                print(f"  Progress: {i + 1}/{iterations}", end='\r')
        
        print()
        
        # Cleanup
        if os.path.exists(test_file):
            os.remove(test_file)
        
        self.results["benchmarks"]["voxel_operations"] = {
            "iterations": len(times),
            "min_ms": min(times),
            "max_ms": max(times),
            "avg_ms": statistics.mean(times),
            "median_ms": statistics.median(times),
            "stddev_ms": statistics.stdev(times) if len(times) > 1 else 0
        }
        
        print(f"  Average: {self.results['benchmarks']['voxel_operations']['avg_ms']:.2f} ms")
    
    def benchmark_export_operations(self, iterations=20):
        """Benchmark export operations."""
        print(f"Benchmarking export operations ({iterations} iterations)...")
        
        # Create a test file with some voxels
        test_file = "/tmp/test_export.gox"
        self.run_command(["create", test_file])
        
        # Add some voxels
        for i in range(10):
            self.run_command([
                "add-voxel", str(i), "-16", "0",
                "255", "0", "0", "255", test_file
            ])
        
        times = {}
        formats = ["obj", "ply", "vox"]
        
        for fmt in formats:
            format_times = []
            for i in range(iterations):
                output_file = f"/tmp/test_export_{i}.{fmt}"
                result = self.run_command(["export", test_file, output_file])
                
                if result["exit_code"] == 0:
                    format_times.append(result["elapsed_ms"])
                    # Cleanup
                    if os.path.exists(output_file):
                        os.remove(output_file)
                
                if (i + 1) % 5 == 0:
                    print(f"  Progress ({fmt}): {i + 1}/{iterations}", end='\r')
            
            print()
            
            if format_times:
                times[fmt] = {
                    "iterations": len(format_times),
                    "min_ms": min(format_times),
                    "max_ms": max(format_times),
                    "avg_ms": statistics.mean(format_times),
                    "median_ms": statistics.median(format_times)
                }
                print(f"  {fmt.upper()} average: {times[fmt]['avg_ms']:.2f} ms")
        
        # Cleanup
        if os.path.exists(test_file):
            os.remove(test_file)
        
        self.results["benchmarks"]["export_operations"] = times
    
    def benchmark_batch_operations(self, operations=5):
        """Benchmark sequential batch operations (simulating v13.4 usage)."""
        print(f"Benchmarking batch operations ({operations} commands)...")
        
        start_time = time.perf_counter()
        
        # Simulate typical workflow
        test_file = "/tmp/test_batch.gox"
        
        # 1. Create file
        self.run_command(["create", test_file])
        
        # 2-4. Add voxels
        for i in range(3):
            self.run_command([
                "add-voxel", str(i), "-16", "0",
                "255", str(i * 50), "0", "255", test_file
            ])
        
        # 5. Export
        self.run_command(["export", test_file, "/tmp/test_batch.obj"])
        
        end_time = time.perf_counter()
        total_ms = (end_time - start_time) * 1000
        
        # Cleanup
        for f in [test_file, "/tmp/test_batch.obj"]:
            if os.path.exists(f):
                os.remove(f)
        
        self.results["benchmarks"]["batch_operations"] = {
            "operations": operations,
            "total_ms": total_ms,
            "avg_per_operation_ms": total_ms / operations
        }
        
        print(f"  Total time: {total_ms:.2f} ms")
        print(f"  Average per operation: {total_ms / operations:.2f} ms")
    
    def measure_memory_usage(self):
        """Measure memory usage of CLI operations."""
        print("Measuring memory usage...")
        
        # Get baseline memory
        process = psutil.Process()
        baseline_memory = process.memory_info().rss / 1024 / 1024  # MB
        
        # Run a command and measure peak memory
        test_file = "/tmp/test_memory.gox"
        proc = subprocess.Popen([self.cli_path, "create", test_file],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        peak_memory = baseline_memory
        while proc.poll() is None:
            try:
                cli_proc = psutil.Process(proc.pid)
                current_memory = cli_proc.memory_info().rss / 1024 / 1024
                peak_memory = max(peak_memory, current_memory)
            except:
                pass
            time.sleep(0.001)
        
        proc.wait()
        
        # Cleanup
        if os.path.exists(test_file):
            os.remove(test_file)
        
        self.results["benchmarks"]["memory_usage"] = {
            "baseline_mb": baseline_memory,
            "peak_mb": peak_memory,
            "overhead_mb": peak_memory - baseline_memory
        }
        
        print(f"  Peak memory: {peak_memory:.2f} MB")
    
    def get_binary_size(self):
        """Get the size of the CLI binary."""
        if os.path.exists(self.cli_path):
            size_bytes = os.path.getsize(self.cli_path)
            size_mb = size_bytes / 1024 / 1024
            
            self.results["binary_size_mb"] = size_mb
            print(f"Binary size: {size_mb:.2f} MB")
    
    def save_results(self):
        """Save benchmark results to JSON file."""
        with open(self.output_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"\nResults saved to: {self.output_file}")
    
    def print_summary(self):
        """Print a summary of key metrics."""
        print("\n" + "="*50)
        print("v13.4 Performance Baseline Summary")
        print("="*50)
        
        if "startup" in self.results["benchmarks"]:
            print(f"Startup time: {self.results['benchmarks']['startup']['avg_ms']:.2f} ms")
        
        if "create_operation" in self.results["benchmarks"]:
            print(f"Create operation: {self.results['benchmarks']['create_operation']['avg_ms']:.2f} ms")
        
        if "voxel_operations" in self.results["benchmarks"]:
            print(f"Voxel operation: {self.results['benchmarks']['voxel_operations']['avg_ms']:.2f} ms")
        
        if "batch_operations" in self.results["benchmarks"]:
            batch = self.results["benchmarks"]["batch_operations"]
            print(f"Batch operations ({batch['operations']} cmds): {batch['total_ms']:.2f} ms total")
            print(f"  Per command average: {batch['avg_per_operation_ms']:.2f} ms")
        
        if "binary_size_mb" in self.results:
            print(f"Binary size: {self.results['binary_size_mb']:.2f} MB")
        
        print("="*50)

def main():
    parser = argparse.ArgumentParser(description="Benchmark Goxel v13.4 performance")
    parser.add_argument("--cli-path", default="../../goxel-headless",
                      help="Path to goxel-headless CLI")
    parser.add_argument("--output", default="results/v134_baseline.json",
                      help="Output JSON file for results")
    parser.add_argument("--quick", action="store_true",
                      help="Run quick benchmark with fewer iterations")
    
    args = parser.parse_args()
    
    # Check if CLI exists
    if not os.path.exists(args.cli_path):
        print(f"Error: CLI not found at {args.cli_path}")
        print("Please build with: scons headless=1 cli_tools=1")
        sys.exit(1)
    
    # Create output directory
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    
    # Run benchmarks
    benchmarker = V134Benchmarker(args.cli_path, args.output)
    
    print("Starting v13.4 performance baseline benchmarking...")
    print(f"CLI path: {args.cli_path}")
    print()
    
    # Get binary info
    benchmarker.get_binary_size()
    print()
    
    # Run benchmarks
    if args.quick:
        benchmarker.benchmark_startup(iterations=20)
        benchmarker.benchmark_create_operation(iterations=10)
        benchmarker.benchmark_voxel_operations(iterations=10)
        benchmarker.benchmark_export_operations(iterations=5)
    else:
        benchmarker.benchmark_startup(iterations=100)
        benchmarker.benchmark_create_operation(iterations=50)
        benchmarker.benchmark_voxel_operations(iterations=50)
        benchmarker.benchmark_export_operations(iterations=20)
    
    benchmarker.benchmark_batch_operations()
    benchmarker.measure_memory_usage()
    
    # Save results
    benchmarker.save_results()
    benchmarker.print_summary()

if __name__ == "__main__":
    main()