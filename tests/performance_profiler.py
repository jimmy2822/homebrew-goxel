#!/usr/bin/env python3
"""
Goxel v13 Performance Profiler
Phase 6.3: Performance Tuning

Profiles Goxel headless operations to identify bottlenecks and measure
compliance with performance targets:
- Simple Operations: <10ms (add/remove single voxel)
- Batch Operations: <100ms (1,000 voxels)
- Project Load/Save: <1s (typical project)
- Rendering: <10s (1920x1080, high quality)
- Memory Usage: <500MB (100k voxel project)
"""

import os
import sys
import time
import subprocess
import json
import statistics
import psutil
from typing import Dict, List, Tuple, Optional

class PerformanceProfiler:
    def __init__(self, goxel_cli_path: str = "./goxel-cli"):
        self.goxel_cli_path = goxel_cli_path
        self.results = {
            "timestamp": time.time(),
            "system_info": self._get_system_info(),
            "benchmarks": {},
            "compliance": {},
            "recommendations": []
        }
        
    def _get_system_info(self) -> Dict:
        """Get system information for benchmarking context"""
        return {
            "platform": sys.platform,
            "cpu_count": psutil.cpu_count(logical=False),
            "cpu_count_logical": psutil.cpu_count(logical=True),
            "memory_total": psutil.virtual_memory().total,
            "memory_available": psutil.virtual_memory().available,
            "architecture": os.uname().machine if hasattr(os, 'uname') else "unknown"
        }
        
    def _run_command_timed(self, cmd: List[str], timeout: float = 30.0) -> Tuple[float, int, str, str]:
        """Run a command and return (elapsed_time, exit_code, stdout, stderr)"""
        start_time = time.time()
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            elapsed = time.time() - start_time
            return elapsed, result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            elapsed = time.time() - start_time
            return elapsed, -1, "", f"Command timed out after {timeout}s"
        except Exception as e:
            elapsed = time.time() - start_time
            return elapsed, -1, "", str(e)
            
    def _run_multiple_timed(self, cmd: List[str], iterations: int = 5) -> Dict:
        """Run a command multiple times and return statistics"""
        times = []
        exit_codes = []
        
        for i in range(iterations):
            elapsed, exit_code, stdout, stderr = self._run_command_timed(cmd)
            times.append(elapsed * 1000)  # Convert to milliseconds
            exit_codes.append(exit_code)
            
        return {
            "iterations": iterations,
            "times_ms": times,
            "mean_ms": statistics.mean(times),
            "median_ms": statistics.median(times),
            "min_ms": min(times),
            "max_ms": max(times),
            "std_dev_ms": statistics.stdev(times) if len(times) > 1 else 0,
            "success_rate": sum(1 for code in exit_codes if code == 0) / len(exit_codes),
            "all_succeeded": all(code == 0 for code in exit_codes)
        }
        
    def benchmark_project_operations(self):
        """Benchmark project creation, save, and load operations"""
        print("Benchmarking project operations...")
        
        # Test project creation
        create_cmd = [self.goxel_cli_path, "create", "/tmp/perf_test_project.gox"]
        create_stats = self._run_multiple_timed(create_cmd, iterations=10)
        
        # Test project save (after adding some voxels)
        # Note: This would need the project to exist first
        save_stats = {"mean_ms": 0, "note": "Save testing requires existing project"}
        
        # Test project load
        # Note: This would need a sample project file
        load_stats = {"mean_ms": 0, "note": "Load testing requires sample project"}
        
        self.results["benchmarks"]["project_operations"] = {
            "create": create_stats,
            "save": save_stats,
            "load": load_stats
        }
        
    def benchmark_voxel_operations(self):
        """Benchmark single voxel operations"""
        print("Benchmarking voxel operations...")
        
        # First create a project
        create_cmd = [self.goxel_cli_path, "create", "/tmp/perf_voxel_test.gox"]
        self._run_command_timed(create_cmd)
        
        # Test single voxel add
        add_cmd = [self.goxel_cli_path, "voxel-add", "--pos", "0,0,0", "--color", "255,0,0,255"]
        add_stats = self._run_multiple_timed(add_cmd, iterations=20)
        
        # Test voxel remove  
        remove_cmd = [self.goxel_cli_path, "voxel-remove", "--pos", "0,0,0"]
        remove_stats = self._run_multiple_timed(remove_cmd, iterations=20)
        
        self.results["benchmarks"]["voxel_operations"] = {
            "add_single": add_stats,
            "remove_single": remove_stats
        }
        
    def benchmark_rendering(self):
        """Benchmark rendering operations"""
        print("Benchmarking rendering operations...")
        
        # Create a project with some voxels
        create_cmd = [self.goxel_cli_path, "create", "/tmp/perf_render_test.gox"]
        self._run_command_timed(create_cmd)
        
        # Add a few voxels for rendering
        for i in range(10):
            add_cmd = [self.goxel_cli_path, "voxel-add", "--pos", f"{i},0,0", "--color", "255,128,64,255"]
            self._run_command_timed(add_cmd)
        
        # Test rendering at different resolutions
        resolutions = [
            ("640x480", "small"),
            ("1920x1080", "large")
        ]
        
        render_results = {}
        
        for resolution, label in resolutions:
            render_cmd = [
                self.goxel_cli_path, "render", 
                "--output", f"/tmp/perf_render_{label}.png",
                "--resolution", resolution
            ]
            
            # Only run a few iterations for rendering due to time
            render_stats = self._run_multiple_timed(render_cmd, iterations=3)
            render_results[label] = render_stats
            
            # Clean up
            try:
                os.remove(f"/tmp/perf_render_{label}.png")
            except:
                pass
                
        self.results["benchmarks"]["rendering"] = render_results
        
    def benchmark_memory_usage(self):
        """Benchmark memory usage patterns"""
        print("Benchmarking memory usage...")
        
        def get_process_memory():
            """Get memory usage of goxel-cli processes"""
            total_memory = 0
            for proc in psutil.process_iter(['pid', 'name', 'memory_info']):
                try:
                    if 'goxel' in proc.info['name'].lower():
                        total_memory += proc.info['memory_info'].rss
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
            return total_memory
            
        # Baseline memory
        baseline_memory = psutil.virtual_memory().used
        
        # Create project and measure memory growth
        create_cmd = [self.goxel_cli_path, "create", "/tmp/perf_memory_test.gox"]
        elapsed, exit_code, stdout, stderr = self._run_command_timed(create_cmd)
        
        # This is a simplified memory test since we can't easily track
        # memory of short-lived CLI processes
        self.results["benchmarks"]["memory"] = {
            "baseline_mb": baseline_memory / (1024 * 1024),
            "note": "Memory testing limited for CLI processes",
            "project_create_time_ms": elapsed * 1000
        }
        
    def check_compliance(self):
        """Check if performance meets Phase 6 targets"""
        print("Checking performance compliance...")
        
        compliance = {
            "simple_operations_under_10ms": False,
            "batch_operations_under_100ms": False,
            "project_ops_under_1s": False,
            "rendering_under_10s": False,
            "memory_under_500mb": False
        }
        
        # Check simple operations (voxel add/remove)
        voxel_ops = self.results["benchmarks"].get("voxel_operations", {})
        if voxel_ops:
            add_time = voxel_ops.get("add_single", {}).get("mean_ms", 1000)
            remove_time = voxel_ops.get("remove_single", {}).get("mean_ms", 1000)
            if add_time < 10 and remove_time < 10:
                compliance["simple_operations_under_10ms"] = True
                
        # Check project operations
        project_ops = self.results["benchmarks"].get("project_operations", {})
        if project_ops:
            create_time = project_ops.get("create", {}).get("mean_ms", 10000)
            if create_time < 1000:
                compliance["project_ops_under_1s"] = True
                
        # Check rendering
        rendering = self.results["benchmarks"].get("rendering", {})
        if rendering:
            large_render = rendering.get("large", {}).get("mean_ms", 100000)
            if large_render < 10000:
                compliance["rendering_under_10s"] = True
                
        # Memory check (simplified)
        memory = self.results["benchmarks"].get("memory", {})
        if memory:
            baseline_mb = memory.get("baseline_mb", 1000)
            if baseline_mb < 500:  # This is a rough approximation
                compliance["memory_under_500mb"] = True
                
        compliance_score = sum(compliance.values()) / len(compliance)
        
        self.results["compliance"] = {
            "requirements": compliance,
            "score": compliance_score,
            "percentage": compliance_score * 100,
            "meets_targets": compliance_score >= 0.8  # 80% compliance
        }
        
    def generate_recommendations(self):
        """Generate performance optimization recommendations"""
        recommendations = []
        
        compliance = self.results["compliance"]["requirements"]
        
        if not compliance["simple_operations_under_10ms"]:
            recommendations.append({
                "category": "Voxel Operations",
                "issue": "Single voxel operations taking >10ms",
                "recommendation": "Optimize voxel data structure access patterns",
                "priority": "HIGH"
            })
            
        if not compliance["batch_operations_under_100ms"]:
            recommendations.append({
                "category": "Batch Operations",
                "issue": "Batch operations not meeting 100ms target",
                "recommendation": "Implement vectorized batch processing",
                "priority": "MEDIUM"
            })
            
        if not compliance["project_ops_under_1s"]:
            recommendations.append({
                "category": "Project I/O",
                "issue": "Project operations taking >1s",
                "recommendation": "Optimize file I/O and compression",
                "priority": "MEDIUM"
            })
            
        if not compliance["rendering_under_10s"]:
            recommendations.append({
                "category": "Rendering",
                "issue": "Rendering taking >10s for 1920x1080",
                "recommendation": "Optimize mesh generation and GPU utilization",
                "priority": "LOW"
            })
            
        if not compliance["memory_under_500mb"]:
            recommendations.append({
                "category": "Memory Usage",
                "issue": "Memory usage exceeding 500MB",
                "recommendation": "Implement memory pooling and compression",
                "priority": "MEDIUM"
            })
            
        # Add general recommendations
        recommendations.extend([
            {
                "category": "General",
                "issue": "CLI startup overhead",
                "recommendation": "Consider daemon mode for repeated operations",
                "priority": "LOW"
            },
            {
                "category": "General", 
                "issue": "Error handling performance",
                "recommendation": "Cache error messages and reduce string operations",
                "priority": "LOW"
            }
        ])
        
        self.results["recommendations"] = recommendations
        
    def run_full_benchmark(self):
        """Run complete performance benchmark suite"""
        print("Starting Goxel v13 Performance Profiling...")
        print("=" * 50)
        
        if not os.path.exists(self.goxel_cli_path):
            print(f"Error: {self.goxel_cli_path} not found")
            return False
            
        try:
            self.benchmark_project_operations()
            self.benchmark_voxel_operations()
            self.benchmark_rendering()
            self.benchmark_memory_usage()
            self.check_compliance()
            self.generate_recommendations()
            
            return True
        except Exception as e:
            print(f"Benchmarking failed: {e}")
            return False
            
    def save_results(self, output_file: str = "performance_results.json"):
        """Save benchmark results to JSON file"""
        with open(output_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"Performance results saved to: {output_file}")
        
    def print_summary(self):
        """Print benchmark summary"""
        print("\n" + "=" * 50)
        print("PERFORMANCE BENCHMARK SUMMARY")
        print("=" * 50)
        
        compliance = self.results["compliance"]
        print(f"Overall Compliance: {compliance['percentage']:.1f}%")
        print(f"Meets Performance Targets: {'YES' if compliance['meets_targets'] else 'NO'}")
        print()
        
        print("Requirement Status:")
        for req, status in compliance["requirements"].items():
            status_icon = "✅" if status else "❌"
            print(f"  {status_icon} {req.replace('_', ' ').title()}")
        print()
        
        # Show key metrics
        benchmarks = self.results["benchmarks"]
        
        if "voxel_operations" in benchmarks:
            voxel = benchmarks["voxel_operations"]
            if "add_single" in voxel:
                print(f"Single Voxel Add: {voxel['add_single']['mean_ms']:.2f}ms (target: <10ms)")
            if "remove_single" in voxel:
                print(f"Single Voxel Remove: {voxel['remove_single']['mean_ms']:.2f}ms (target: <10ms)")
                
        if "project_operations" in benchmarks:
            project = benchmarks["project_operations"]
            if "create" in project:
                print(f"Project Create: {project['create']['mean_ms']:.2f}ms (target: <1000ms)")
                
        if "rendering" in benchmarks:
            rendering = benchmarks["rendering"]
            for res, data in rendering.items():
                if isinstance(data, dict) and "mean_ms" in data:
                    print(f"Rendering ({res}): {data['mean_ms']:.2f}ms")
                    
        print()
        
        # Show top recommendations
        print("Top Performance Recommendations:")
        high_priority = [r for r in self.results["recommendations"] if r["priority"] == "HIGH"]
        for i, rec in enumerate(high_priority[:3], 1):
            print(f"  {i}. {rec['category']}: {rec['recommendation']}")
            
        if not high_priority:
            print("  No high-priority issues found!")

def main():
    if len(sys.argv) > 1:
        goxel_cli_path = sys.argv[1]
    else:
        goxel_cli_path = "./goxel-cli"
        
    profiler = PerformanceProfiler(goxel_cli_path)
    
    if profiler.run_full_benchmark():
        profiler.save_results()
        profiler.print_summary()
        
        # Exit with error code if performance targets not met
        if not profiler.results["compliance"]["meets_targets"]:
            print("\nWARNING: Performance targets not met!")
            return 1
    else:
        print("Benchmarking failed!")
        return 1
        
    return 0

if __name__ == "__main__":
    sys.exit(main())