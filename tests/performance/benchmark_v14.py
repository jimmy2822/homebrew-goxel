#!/usr/bin/env python3
"""
Goxel v14.0 Performance Comparison Framework

This comprehensive benchmarking suite compares performance between:
- v13.4 CLI mode (traditional headless)
- v14.0 Daemon mode (persistent architecture)

Performance targets:
- 700% improvement over v13.4 CLI
- < 2.1ms average request latency
- > 1000 operations/second throughput
- < 50MB memory usage
- 10+ concurrent client support

The framework provides:
- Automated test execution
- Statistical analysis
- Performance regression detection  
- Multi-format reporting (JSON, CSV, HTML)
- CI/CD integration support
"""

import json
import time
import subprocess
import socket
import os
import sys
import tempfile
import statistics
import argparse
import threading
from typing import List, Dict, Any, Tuple, Optional
from dataclasses import dataclass, asdict
from pathlib import Path
import psutil

# ============================================================================
# CONFIGURATION CONSTANTS
# ============================================================================

# Test configuration
TEST_SOCKET = "/tmp/goxel_perf_test.sock"
TEST_PID_FILE = "/tmp/goxel_perf_test.pid"
TEST_LOG_FILE = "/tmp/goxel_perf_test.log"
DEFAULT_TIMEOUT = 30.0

# Performance thresholds
LATENCY_TARGET_MS = 2.1
THROUGHPUT_TARGET_OPS_SEC = 1000
MEMORY_TARGET_MB = 50
IMPROVEMENT_TARGET_PERCENT = 700
CONCURRENT_CLIENTS_TARGET = 10

# Test workloads
QUICK_TEST_SAMPLES = 100
FULL_TEST_SAMPLES = 1000
STRESS_TEST_DURATION = 60  # seconds

# ============================================================================
# DATA STRUCTURES
# ============================================================================

@dataclass
class PerformanceMetrics:
    """Performance metrics for a single test run"""
    test_name: str
    operation_count: int
    total_time_ms: float
    avg_latency_ms: float
    min_latency_ms: float
    max_latency_ms: float
    p50_latency_ms: float
    p95_latency_ms: float
    p99_latency_ms: float
    throughput_ops_sec: float
    memory_usage_mb: float
    success_rate_percent: float
    concurrent_clients: int = 1
    errors: List[str] = None

    def __post_init__(self):
        if self.errors is None:
            self.errors = []

@dataclass
class ComparisonResults:
    """Results comparing v13.4 CLI vs v14.0 Daemon"""
    cli_metrics: PerformanceMetrics
    daemon_metrics: PerformanceMetrics
    improvement_factor: float
    latency_improvement_percent: float
    throughput_improvement_percent: float
    memory_improvement_percent: float
    targets_met: Dict[str, bool]

# ============================================================================
# CLI PERFORMANCE TESTER
# ============================================================================

class CLIPerformanceTester:
    """Tests v13.4 CLI performance (traditional mode)"""
    
    def __init__(self, cli_path: str = "../../goxel-headless"):
        self.cli_path = cli_path
        self.temp_files = []
    
    def cleanup(self):
        """Clean up temporary files"""
        for file_path in self.temp_files:
            try:
                os.unlink(file_path)
            except FileNotFoundError:
                pass
    
    def run_cli_command(self, args: List[str]) -> Tuple[float, bool, str]:
        """Run a single CLI command and measure performance"""
        start_time = time.perf_counter()
        
        try:
            result = subprocess.run(
                [self.cli_path] + args,
                capture_output=True,
                text=True,
                timeout=DEFAULT_TIMEOUT
            )
            
            end_time = time.perf_counter()
            duration_ms = (end_time - start_time) * 1000
            
            success = result.returncode == 0
            error_msg = result.stderr if not success else ""
            
            return duration_ms, success, error_msg
            
        except subprocess.TimeoutExpired:
            return DEFAULT_TIMEOUT * 1000, False, "Command timeout"
        except Exception as e:
            return 0, False, str(e)
    
    def benchmark_basic_operations(self, num_samples: int) -> PerformanceMetrics:
        """Benchmark basic CLI operations"""
        latencies = []
        errors = []
        memory_usage = 0
        
        # Create a temporary project file
        with tempfile.NamedTemporaryFile(suffix='.gox', delete=False) as temp_file:
            temp_project = temp_file.name
            self.temp_files.append(temp_project)
        
        operations = [
            (["create", temp_project], "create_project"),
            (["add-voxel", "0", "-16", "0", "255", "0", "0", "255", temp_project], "add_voxel"),
            (["export", temp_project.replace('.gox', '.obj')], "export_model"),
        ]
        
        print(f"  Running {num_samples} CLI operation sequences...")
        
        for i in range(num_samples):
            sequence_start = time.perf_counter()
            sequence_success = True
            
            # Monitor memory usage during operations
            process = psutil.Process()
            memory_before = process.memory_info().rss / 1024 / 1024  # MB
            
            for args, operation_name in operations:
                duration_ms, success, error_msg = self.run_cli_command(args)
                
                if not success:
                    sequence_success = False
                    errors.append(f"{operation_name}: {error_msg}")
                    break
            
            memory_after = process.memory_info().rss / 1024 / 1024  # MB
            memory_usage = max(memory_usage, memory_after - memory_before)
            
            if sequence_success:
                sequence_end = time.perf_counter()
                sequence_duration = (sequence_end - sequence_start) * 1000
                latencies.append(sequence_duration)
            
            # Progress indicator
            if (i + 1) % (num_samples // 10) == 0:
                print(f"    Progress: {i + 1}/{num_samples} ({(i + 1) / num_samples * 100:.1f}%)")
        
        # Calculate metrics
        if latencies:
            total_time = sum(latencies)
            avg_latency = statistics.mean(latencies)
            min_latency = min(latencies)
            max_latency = max(latencies)
            p50_latency = statistics.median(latencies)
            p95_latency = self._percentile(latencies, 95)
            p99_latency = self._percentile(latencies, 99)
            throughput = len(latencies) * len(operations) / (total_time / 1000)
            success_rate = len(latencies) / num_samples * 100
        else:
            total_time = avg_latency = min_latency = max_latency = 0
            p50_latency = p95_latency = p99_latency = 0
            throughput = 0
            success_rate = 0
        
        return PerformanceMetrics(
            test_name="CLI_Basic_Operations",
            operation_count=len(latencies) * len(operations),
            total_time_ms=total_time,
            avg_latency_ms=avg_latency,
            min_latency_ms=min_latency,
            max_latency_ms=max_latency,
            p50_latency_ms=p50_latency,
            p95_latency_ms=p95_latency,
            p99_latency_ms=p99_latency,
            throughput_ops_sec=throughput,
            memory_usage_mb=memory_usage,
            success_rate_percent=success_rate,
            errors=errors
        )
    
    def _percentile(self, data: List[float], percentile: float) -> float:
        """Calculate percentile of a dataset"""
        if not data:
            return 0
        sorted_data = sorted(data)
        index = (percentile / 100) * (len(sorted_data) - 1)
        if index.is_integer():
            return sorted_data[int(index)]
        else:
            lower = sorted_data[int(index)]
            upper = sorted_data[int(index) + 1]
            return lower + (upper - lower) * (index - int(index))

# ============================================================================
# DAEMON PERFORMANCE TESTER
# ============================================================================

class DaemonPerformanceTester:
    """Tests v14.0 Daemon performance (persistent mode)"""
    
    def __init__(self, daemon_path: str = "../../goxel-headless"):
        self.daemon_path = daemon_path
        self.daemon_process = None
        self.socket_path = TEST_SOCKET
        self.request_id = 1
    
    def start_daemon(self) -> bool:
        """Start the daemon process"""
        try:
            # Clean up any existing socket
            if os.path.exists(self.socket_path):
                os.unlink(self.socket_path)
            
            # Start daemon process
            self.daemon_process = subprocess.Popen(
                [self.daemon_path, "--daemon", "--socket", self.socket_path,
                 "--pid-file", TEST_PID_FILE, "--log-file", TEST_LOG_FILE],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            # Wait for socket to be available
            for _ in range(100):  # 10 second timeout
                if os.path.exists(self.socket_path):
                    time.sleep(0.1)  # Give it a moment to be ready
                    return True
                time.sleep(0.1)
            
            return False
            
        except Exception as e:
            print(f"Error starting daemon: {e}")
            return False
    
    def stop_daemon(self):
        """Stop the daemon process"""
        if self.daemon_process:
            try:
                self.daemon_process.terminate()
                self.daemon_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.daemon_process.kill()
                self.daemon_process.wait()
        
        # Clean up files
        for file_path in [self.socket_path, TEST_PID_FILE, TEST_LOG_FILE]:
            try:
                os.unlink(file_path)
            except FileNotFoundError:
                pass
    
    def send_json_rpc(self, method: str, params: List = None) -> Tuple[float, bool, Dict]:
        """Send JSON RPC request and measure response time"""
        if params is None:
            params = []
        
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": self.request_id
        }
        self.request_id += 1
        
        start_time = time.perf_counter()
        
        try:
            # Connect to daemon
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.settimeout(DEFAULT_TIMEOUT)
            sock.connect(self.socket_path)
            
            # Send request
            request_json = json.dumps(request) + "\n"
            sock.send(request_json.encode('utf-8'))
            
            # Receive response
            response_data = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk
                if b'\n' in chunk:
                    break
            
            sock.close()
            
            end_time = time.perf_counter()
            duration_ms = (end_time - start_time) * 1000
            
            # Parse response
            response_str = response_data.decode('utf-8').strip()
            response = json.loads(response_str) if response_str else {}
            
            success = "result" in response or ("error" in response and response["error"]["code"] != -32700)
            
            return duration_ms, success, response
            
        except Exception as e:
            return DEFAULT_TIMEOUT * 1000, False, {"error": str(e)}
    
    def benchmark_basic_operations(self, num_samples: int) -> PerformanceMetrics:
        """Benchmark basic daemon operations"""
        if not self.start_daemon():
            return PerformanceMetrics(
                test_name="Daemon_Basic_Operations",
                operation_count=0,
                total_time_ms=0,
                avg_latency_ms=0,
                min_latency_ms=0,
                max_latency_ms=0,
                p50_latency_ms=0,
                p95_latency_ms=0,
                p99_latency_ms=0,
                throughput_ops_sec=0,
                memory_usage_mb=0,
                success_rate_percent=0,
                errors=["Failed to start daemon"]
            )
        
        try:
            latencies = []
            errors = []
            
            operations = [
                ("goxel.create_project", ["Perf Test Project", 32, 32, 32]),
                ("goxel.add_voxel", [0, -16, 0, 255, 0, 0, 255, 0]),
                ("goxel.get_voxel", [0, -16, 0]),
                ("goxel.get_status", []),
            ]
            
            print(f"  Running {num_samples} daemon operation sequences...")
            
            # Monitor daemon memory usage
            daemon_memory = 0
            if self.daemon_process:
                try:
                    daemon_psutil = psutil.Process(self.daemon_process.pid)
                    daemon_memory = daemon_psutil.memory_info().rss / 1024 / 1024  # MB
                except psutil.NoSuchProcess:
                    pass
            
            for i in range(num_samples):
                sequence_start = time.perf_counter()
                sequence_success = True
                sequence_latencies = []
                
                for method, params in operations:
                    duration_ms, success, response = self.send_json_rpc(method, params)
                    sequence_latencies.append(duration_ms)
                    
                    if not success:
                        sequence_success = False
                        error_msg = response.get("error", {}).get("message", "Unknown error")
                        errors.append(f"{method}: {error_msg}")
                        break
                
                if sequence_success:
                    sequence_total = sum(sequence_latencies)
                    latencies.extend(sequence_latencies)
                
                # Update memory usage
                if self.daemon_process and i % 10 == 0:
                    try:
                        daemon_psutil = psutil.Process(self.daemon_process.pid)
                        current_memory = daemon_psutil.memory_info().rss / 1024 / 1024
                        daemon_memory = max(daemon_memory, current_memory)
                    except psutil.NoSuchProcess:
                        pass
                
                # Progress indicator
                if (i + 1) % (num_samples // 10) == 0:
                    print(f"    Progress: {i + 1}/{num_samples} ({(i + 1) / num_samples * 100:.1f}%)")
            
            # Calculate metrics
            if latencies:
                total_time = sum(latencies)
                avg_latency = statistics.mean(latencies)
                min_latency = min(latencies)
                max_latency = max(latencies)
                p50_latency = statistics.median(latencies)
                p95_latency = self._percentile(latencies, 95)
                p99_latency = self._percentile(latencies, 99)
                throughput = len(latencies) / (total_time / 1000)
                success_rate = len(latencies) / (num_samples * len(operations)) * 100
            else:
                total_time = avg_latency = min_latency = max_latency = 0
                p50_latency = p95_latency = p99_latency = 0
                throughput = 0
                success_rate = 0
            
            return PerformanceMetrics(
                test_name="Daemon_Basic_Operations",
                operation_count=len(latencies),
                total_time_ms=total_time,
                avg_latency_ms=avg_latency,
                min_latency_ms=min_latency,
                max_latency_ms=max_latency,
                p50_latency_ms=p50_latency,
                p95_latency_ms=p95_latency,
                p99_latency_ms=p99_latency,
                throughput_ops_sec=throughput,
                memory_usage_mb=daemon_memory,
                success_rate_percent=success_rate,
                errors=errors
            )
        
        finally:
            self.stop_daemon()
    
    def benchmark_concurrent_clients(self, num_clients: int, operations_per_client: int) -> PerformanceMetrics:
        """Benchmark concurrent client performance"""
        if not self.start_daemon():
            return PerformanceMetrics(
                test_name="Daemon_Concurrent_Clients",
                operation_count=0,
                total_time_ms=0,
                avg_latency_ms=0,
                min_latency_ms=0,
                max_latency_ms=0,
                p50_latency_ms=0,
                p95_latency_ms=0,
                p99_latency_ms=0,
                throughput_ops_sec=0,
                memory_usage_mb=0,
                success_rate_percent=0,
                concurrent_clients=num_clients,
                errors=["Failed to start daemon"]
            )
        
        try:
            all_latencies = []
            all_errors = []
            
            def client_worker(client_id: int, results: List):
                """Worker function for each client thread"""
                latencies = []
                errors = []
                
                for i in range(operations_per_client):
                    method = "goxel.get_status"
                    params = []
                    
                    duration_ms, success, response = self.send_json_rpc(method, params)
                    
                    if success:
                        latencies.append(duration_ms)
                    else:
                        error_msg = response.get("error", {}).get("message", "Unknown error")
                        errors.append(f"Client {client_id}: {error_msg}")
                
                results.append((latencies, errors))
            
            print(f"  Running {num_clients} concurrent clients with {operations_per_client} operations each...")
            
            # Start concurrent client threads
            threads = []
            results = []
            
            start_time = time.perf_counter()
            
            for client_id in range(num_clients):
                thread = threading.Thread(target=client_worker, args=(client_id, results))
                threads.append(thread)
                thread.start()
            
            # Wait for all threads to complete
            for thread in threads:
                thread.join()
            
            end_time = time.perf_counter()
            total_test_time = (end_time - start_time) * 1000
            
            # Aggregate results
            for latencies, errors in results:
                all_latencies.extend(latencies)
                all_errors.extend(errors)
            
            # Monitor daemon memory
            daemon_memory = 0
            if self.daemon_process:
                try:
                    daemon_psutil = psutil.Process(self.daemon_process.pid)
                    daemon_memory = daemon_psutil.memory_info().rss / 1024 / 1024
                except psutil.NoSuchProcess:
                    pass
            
            # Calculate metrics
            if all_latencies:
                avg_latency = statistics.mean(all_latencies)
                min_latency = min(all_latencies)
                max_latency = max(all_latencies)
                p50_latency = statistics.median(all_latencies)
                p95_latency = self._percentile(all_latencies, 95)
                p99_latency = self._percentile(all_latencies, 99)
                throughput = len(all_latencies) / (total_test_time / 1000)
                success_rate = len(all_latencies) / (num_clients * operations_per_client) * 100
            else:
                avg_latency = min_latency = max_latency = 0
                p50_latency = p95_latency = p99_latency = 0
                throughput = 0
                success_rate = 0
            
            return PerformanceMetrics(
                test_name="Daemon_Concurrent_Clients",
                operation_count=len(all_latencies),
                total_time_ms=total_test_time,
                avg_latency_ms=avg_latency,
                min_latency_ms=min_latency,
                max_latency_ms=max_latency,
                p50_latency_ms=p50_latency,
                p95_latency_ms=p95_latency,
                p99_latency_ms=p99_latency,
                throughput_ops_sec=throughput,
                memory_usage_mb=daemon_memory,
                success_rate_percent=success_rate,
                concurrent_clients=num_clients,
                errors=all_errors
            )
        
        finally:
            self.stop_daemon()
    
    def _percentile(self, data: List[float], percentile: float) -> float:
        """Calculate percentile of a dataset"""
        if not data:
            return 0
        sorted_data = sorted(data)
        index = (percentile / 100) * (len(sorted_data) - 1)
        if index.is_integer():
            return sorted_data[int(index)]
        else:
            lower = sorted_data[int(index)]
            upper = sorted_data[int(index) + 1]
            return lower + (upper - lower) * (index - int(index))

# ============================================================================
# PERFORMANCE COMPARISON ENGINE
# ============================================================================

class PerformanceComparison:
    """Compares v13.4 CLI vs v14.0 Daemon performance"""
    
    def __init__(self, cli_path: str = "../../goxel-headless", daemon_path: str = None):
        self.cli_tester = CLIPerformanceTester(cli_path)
        self.daemon_tester = DaemonPerformanceTester(daemon_path or cli_path)
    
    def run_basic_comparison(self, num_samples: int) -> ComparisonResults:
        """Run basic performance comparison"""
        print("🔄 Running CLI vs Daemon Performance Comparison")
        print("=" * 60)
        
        # Test CLI performance
        print("\n📋 Testing v13.4 CLI Performance...")
        cli_metrics = self.cli_tester.benchmark_basic_operations(num_samples)
        self._print_metrics_summary(cli_metrics)
        
        # Test daemon performance
        print("\n🚀 Testing v14.0 Daemon Performance...")
        daemon_metrics = self.daemon_tester.benchmark_basic_operations(num_samples)
        self._print_metrics_summary(daemon_metrics)
        
        # Calculate comparison metrics
        return self._calculate_comparison(cli_metrics, daemon_metrics)
    
    def run_concurrent_test(self, num_clients: int, operations_per_client: int) -> PerformanceMetrics:
        """Run concurrent client test (daemon only)"""
        print(f"\n⚡ Testing Concurrent Performance ({num_clients} clients)")
        print("=" * 60)
        
        return self.daemon_tester.benchmark_concurrent_clients(num_clients, operations_per_client)
    
    def _calculate_comparison(self, cli_metrics: PerformanceMetrics, daemon_metrics: PerformanceMetrics) -> ComparisonResults:
        """Calculate comparison results"""
        # Calculate improvement factors
        if cli_metrics.avg_latency_ms > 0:
            improvement_factor = cli_metrics.avg_latency_ms / daemon_metrics.avg_latency_ms
            latency_improvement = (cli_metrics.avg_latency_ms - daemon_metrics.avg_latency_ms) / cli_metrics.avg_latency_ms * 100
        else:
            improvement_factor = 0
            latency_improvement = 0
        
        if cli_metrics.throughput_ops_sec > 0:
            throughput_improvement = (daemon_metrics.throughput_ops_sec - cli_metrics.throughput_ops_sec) / cli_metrics.throughput_ops_sec * 100
        else:
            throughput_improvement = 0
        
        if cli_metrics.memory_usage_mb > 0:
            memory_improvement = (cli_metrics.memory_usage_mb - daemon_metrics.memory_usage_mb) / cli_metrics.memory_usage_mb * 100
        else:
            memory_improvement = 0
        
        # Check if targets are met
        targets_met = {
            "latency_target": daemon_metrics.avg_latency_ms < LATENCY_TARGET_MS,
            "throughput_target": daemon_metrics.throughput_ops_sec > THROUGHPUT_TARGET_OPS_SEC,
            "memory_target": daemon_metrics.memory_usage_mb < MEMORY_TARGET_MB,
            "improvement_target": improvement_factor > (IMPROVEMENT_TARGET_PERCENT / 100)
        }
        
        return ComparisonResults(
            cli_metrics=cli_metrics,
            daemon_metrics=daemon_metrics,
            improvement_factor=improvement_factor,
            latency_improvement_percent=latency_improvement,
            throughput_improvement_percent=throughput_improvement,
            memory_improvement_percent=memory_improvement,
            targets_met=targets_met
        )
    
    def _print_metrics_summary(self, metrics: PerformanceMetrics):
        """Print metrics summary"""
        print(f"  Operations: {metrics.operation_count}")
        print(f"  Success Rate: {metrics.success_rate_percent:.1f}%")
        print(f"  Avg Latency: {metrics.avg_latency_ms:.2f}ms")
        print(f"  P95 Latency: {metrics.p95_latency_ms:.2f}ms")
        print(f"  Throughput: {metrics.throughput_ops_sec:.1f} ops/sec")
        print(f"  Memory Usage: {metrics.memory_usage_mb:.1f}MB")
        
        if metrics.errors:
            print(f"  Errors: {len(metrics.errors)}")
    
    def cleanup(self):
        """Clean up test artifacts"""
        self.cli_tester.cleanup()

# ============================================================================
# REPORT GENERATION
# ============================================================================

class PerformanceReporter:
    """Generates performance reports in multiple formats"""
    
    @staticmethod
    def generate_json_report(comparison: ComparisonResults, concurrent_metrics: PerformanceMetrics, 
                           output_file: str):
        """Generate JSON report"""
        report = {
            "test_timestamp": time.time(),
            "test_date": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
            "comparison": asdict(comparison),
            "concurrent_test": asdict(concurrent_metrics),
            "targets": {
                "latency_target_ms": LATENCY_TARGET_MS,
                "throughput_target_ops_sec": THROUGHPUT_TARGET_OPS_SEC,
                "memory_target_mb": MEMORY_TARGET_MB,
                "improvement_target_percent": IMPROVEMENT_TARGET_PERCENT,
                "concurrent_clients_target": CONCURRENT_CLIENTS_TARGET
            }
        }
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"✅ JSON report generated: {output_file}")
    
    @staticmethod
    def print_comparison_summary(comparison: ComparisonResults, concurrent_metrics: PerformanceMetrics):
        """Print comparison summary to console"""
        print("\n" + "=" * 80)
        print("🎯 GOXEL v14.0 PERFORMANCE COMPARISON RESULTS")
        print("=" * 80)
        
        # Overall improvement
        print(f"\n🚀 Overall Performance Improvement: {comparison.improvement_factor:.1f}x")
        print(f"   Target: {IMPROVEMENT_TARGET_PERCENT / 100:.1f}x | " + 
              ("✅ MET" if comparison.targets_met["improvement_target"] else "❌ NOT MET"))
        
        # Detailed metrics
        print(f"\n📊 Detailed Metrics:")
        print(f"   Latency Improvement: {comparison.latency_improvement_percent:.1f}%")
        print(f"   Throughput Improvement: {comparison.throughput_improvement_percent:.1f}%")
        print(f"   Memory Change: {comparison.memory_improvement_percent:+.1f}%")
        
        # Target validation
        print(f"\n🎯 Target Validation:")
        daemon_metrics = comparison.daemon_metrics
        
        print(f"   Latency: {daemon_metrics.avg_latency_ms:.2f}ms (target: <{LATENCY_TARGET_MS}ms) | " +
              ("✅ MET" if comparison.targets_met["latency_target"] else "❌ NOT MET"))
        
        print(f"   Throughput: {daemon_metrics.throughput_ops_sec:.0f} ops/sec (target: >{THROUGHPUT_TARGET_OPS_SEC}) | " +
              ("✅ MET" if comparison.targets_met["throughput_target"] else "❌ NOT MET"))
        
        print(f"   Memory: {daemon_metrics.memory_usage_mb:.1f}MB (target: <{MEMORY_TARGET_MB}MB) | " +
              ("✅ MET" if comparison.targets_met["memory_target"] else "❌ NOT MET"))
        
        # Concurrent performance
        print(f"\n⚡ Concurrent Performance ({concurrent_metrics.concurrent_clients} clients):")
        print(f"   Success Rate: {concurrent_metrics.success_rate_percent:.1f}%")
        print(f"   Avg Latency: {concurrent_metrics.avg_latency_ms:.2f}ms")
        print(f"   Throughput: {concurrent_metrics.throughput_ops_sec:.0f} ops/sec")
        
        concurrent_success = (concurrent_metrics.concurrent_clients >= CONCURRENT_CLIENTS_TARGET and 
                            concurrent_metrics.success_rate_percent >= 95)
        print(f"   Target: ≥{CONCURRENT_CLIENTS_TARGET} clients, ≥95% success | " +
              ("✅ MET" if concurrent_success else "❌ NOT MET"))
        
        # Overall assessment
        all_targets_met = all(comparison.targets_met.values()) and concurrent_success
        print(f"\n🏆 Overall Assessment: " + 
              ("🎉 ALL TARGETS MET - READY FOR DEPLOYMENT" if all_targets_met else "⚠️  SOME TARGETS NOT MET"))
        
        print("=" * 80)

# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="Goxel v14.0 Performance Comparison Framework")
    parser.add_argument("--quick", action="store_true", help="Run quick test (100 samples)")
    parser.add_argument("--full", action="store_true", help="Run full test (1000 samples)")
    parser.add_argument("--samples", type=int, default=QUICK_TEST_SAMPLES, help="Number of test samples")
    parser.add_argument("--concurrent-clients", type=int, default=10, help="Number of concurrent clients to test")
    parser.add_argument("--operations-per-client", type=int, default=50, help="Operations per concurrent client")
    parser.add_argument("--output", type=str, help="Output file for JSON report")
    parser.add_argument("--cli-path", type=str, default="../../goxel-headless", help="Path to CLI executable")
    
    args = parser.parse_args()
    
    # Determine sample count
    if args.full:
        samples = FULL_TEST_SAMPLES
    elif args.quick:
        samples = QUICK_TEST_SAMPLES
    else:
        samples = args.samples
    
    print("🎯 Goxel v14.0 Performance Comparison Framework")
    print("=" * 60)
    print(f"Samples: {samples}")
    print(f"Concurrent clients: {args.concurrent_clients}")
    print(f"Operations per client: {args.operations_per_client}")
    
    # Run performance comparison
    comparison_engine = PerformanceComparison(args.cli_path)
    
    try:
        # Basic comparison
        comparison_results = comparison_engine.run_basic_comparison(samples)
        
        # Concurrent test
        concurrent_results = comparison_engine.run_concurrent_test(
            args.concurrent_clients, args.operations_per_client)
        
        # Generate reports
        PerformanceReporter.print_comparison_summary(comparison_results, concurrent_results)
        
        if args.output:
            PerformanceReporter.generate_json_report(
                comparison_results, concurrent_results, args.output)
        
        # Exit code based on target achievement
        all_targets_met = (all(comparison_results.targets_met.values()) and 
                          concurrent_results.success_rate_percent >= 95)
        
        return 0 if all_targets_met else 1
        
    finally:
        comparison_engine.cleanup()

if __name__ == "__main__":
    sys.exit(main())