#!/usr/bin/env python3
"""
Goxel v14.0 Automated Performance Benchmark Runner

This script automates the execution of all performance benchmarks,
collects results, and generates comprehensive reports.

Features:
- Automatic daemon startup/shutdown
- Sequential benchmark execution
- Resource monitoring during tests
- Result aggregation and analysis
- Report generation (JSON, CSV, HTML)
- CI/CD integration support
"""

import os
import sys
import json
import time
import subprocess
import signal
import argparse
import datetime
import shutil
import tempfile
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
import psutil
import platform

# Add parent directory to path for importing benchmark modules
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ============================================================================
# CONFIGURATION
# ============================================================================

BENCHMARK_DIR = Path(__file__).parent.parent / "tests" / "performance"
DAEMON_BINARY = Path(__file__).parent.parent / "goxel-daemon"
CLI_BINARY = Path(__file__).parent.parent / "goxel-headless"
RESULTS_DIR = Path(__file__).parent.parent / "benchmark_results"
SOCKET_PATH = "/tmp/goxel_benchmark.sock"
PID_FILE = "/tmp/goxel_benchmark.pid"

# Performance targets
TARGETS = {
    "latency_ms": 2.1,
    "throughput_ops": 1000,
    "memory_mb": 50,
    "improvement_factor": 7.0,
    "startup_ms": 10
}

# Test configurations
BENCHMARK_TESTS = [
    {
        "name": "latency_benchmark",
        "description": "Single operation latency measurement",
        "executable": "latency_benchmark",
        "args": ["100"],  # iterations
        "timeout": 60
    },
    {
        "name": "throughput_test",
        "description": "Operations per second measurement",
        "executable": "throughput_test",
        "args": ["10"],  # duration in seconds
        "timeout": 30
    },
    {
        "name": "memory_profiling",
        "description": "Memory usage patterns over time",
        "executable": "memory_profiling",
        "args": ["60"],  # duration in seconds
        "timeout": 90
    },
    {
        "name": "stress_test",
        "description": "Concurrent client handling",
        "executable": "stress_test",
        "args": ["10", "1000"],  # clients, operations
        "timeout": 120
    },
    {
        "name": "comparison_framework",
        "description": "CLI vs Daemon performance comparison",
        "executable": "comparison_framework",
        "args": [],
        "timeout": 180
    }
]

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def ensure_results_dir():
    """Create results directory if it doesn't exist."""
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    return RESULTS_DIR

def get_timestamp():
    """Get formatted timestamp for file naming."""
    return datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

def check_binaries():
    """Check if required binaries exist."""
    missing = []
    
    if not CLI_BINARY.exists():
        missing.append(f"CLI binary not found: {CLI_BINARY}")
    
    if not DAEMON_BINARY.exists():
        missing.append(f"Daemon binary not found: {DAEMON_BINARY}")
    
    # Check benchmark executables
    for test in BENCHMARK_TESTS:
        exe_path = BENCHMARK_DIR / test["executable"]
        if not exe_path.exists():
            missing.append(f"Benchmark executable not found: {exe_path}")
    
    if missing:
        print("ERROR: Missing required binaries:")
        for msg in missing:
            print(f"  - {msg}")
        print("\nPlease build the project first:")
        print("  make -C tests/performance")
        return False
    
    return True

def start_daemon(socket_path=SOCKET_PATH, pid_file=PID_FILE):
    """Start the Goxel daemon for testing."""
    print(f"Starting Goxel daemon at {socket_path}...")
    
    # Clean up any existing socket
    if Path(socket_path).exists():
        os.unlink(socket_path)
    
    # Start daemon
    cmd = [str(DAEMON_BINARY), "--socket", socket_path, "--pid-file", pid_file]
    
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            preexec_fn=os.setsid
        )
        
        # Wait for daemon to start
        time.sleep(0.5)
        
        # Check if daemon started successfully
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            print(f"Daemon failed to start:")
            print(f"stdout: {stdout.decode()}")
            print(f"stderr: {stderr.decode()}")
            return None
        
        # Verify socket exists
        if not Path(socket_path).exists():
            print("Daemon started but socket not created")
            proc.terminate()
            return None
        
        print(f"Daemon started with PID {proc.pid}")
        return proc
        
    except Exception as e:
        print(f"Failed to start daemon: {e}")
        return None

def stop_daemon(proc=None, pid_file=PID_FILE):
    """Stop the Goxel daemon."""
    print("Stopping Goxel daemon...")
    
    if proc:
        # Try graceful shutdown first
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            # Force kill if needed
            proc.kill()
            proc.wait()
    
    # Clean up PID file
    if Path(pid_file).exists():
        os.unlink(pid_file)
    
    # Clean up socket
    if Path(SOCKET_PATH).exists():
        os.unlink(SOCKET_PATH)
    
    print("Daemon stopped")

def get_system_info():
    """Collect system information for the report."""
    return {
        "platform": platform.platform(),
        "processor": platform.processor(),
        "cpu_count": psutil.cpu_count(),
        "memory_total_mb": psutil.virtual_memory().total / (1024 * 1024),
        "python_version": sys.version,
        "timestamp": datetime.datetime.now().isoformat()
    }

# ============================================================================
# BENCHMARK EXECUTION
# ============================================================================

def run_benchmark(test_config: Dict[str, Any], daemon_proc=None) -> Dict[str, Any]:
    """Run a single benchmark test."""
    print(f"\n{'='*60}")
    print(f"Running: {test_config['name']}")
    print(f"Description: {test_config['description']}")
    print(f"{'='*60}")
    
    exe_path = BENCHMARK_DIR / test_config["executable"]
    cmd = [str(exe_path)] + test_config["args"]
    
    # Set environment for test
    env = os.environ.copy()
    env["GOXEL_TEST_SOCKET"] = SOCKET_PATH
    env["GOXEL_CLI_PATH"] = str(CLI_BINARY)
    
    start_time = time.time()
    result = {
        "name": test_config["name"],
        "description": test_config["description"],
        "start_time": datetime.datetime.now().isoformat(),
        "success": False,
        "error": None,
        "duration": 0,
        "output": "",
        "metrics": {}
    }
    
    try:
        # Monitor initial resource usage
        if daemon_proc and daemon_proc.poll() is None:
            daemon_process = psutil.Process(daemon_proc.pid)
            initial_memory = daemon_process.memory_info().rss / (1024 * 1024)  # MB
            initial_cpu = daemon_process.cpu_percent(interval=0.1)
        else:
            initial_memory = 0
            initial_cpu = 0
        
        # Run the benchmark
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env
        )
        
        stdout, stderr = proc.communicate(timeout=test_config["timeout"])
        
        # Check result
        if proc.returncode == 0:
            result["success"] = True
            result["output"] = stdout.decode()
            
            # Monitor final resource usage
            if daemon_proc and daemon_proc.poll() is None:
                final_memory = daemon_process.memory_info().rss / (1024 * 1024)
                final_cpu = daemon_process.cpu_percent(interval=0.1)
                
                result["metrics"]["memory_delta_mb"] = final_memory - initial_memory
                result["metrics"]["cpu_average"] = (initial_cpu + final_cpu) / 2
                result["metrics"]["memory_peak_mb"] = final_memory
            
            # Parse test output for metrics
            parse_test_output(result)
            
        else:
            result["error"] = f"Test failed with code {proc.returncode}"
            result["output"] = stderr.decode()
            
    except subprocess.TimeoutExpired:
        result["error"] = f"Test timed out after {test_config['timeout']}s"
        proc.kill()
    except Exception as e:
        result["error"] = f"Test execution failed: {str(e)}"
    
    result["duration"] = time.time() - start_time
    result["end_time"] = datetime.datetime.now().isoformat()
    
    # Print summary
    if result["success"]:
        print(f"✅ Test completed in {result['duration']:.2f}s")
        if result["metrics"]:
            print("\nKey Metrics:")
            for key, value in result["metrics"].items():
                print(f"  {key}: {value}")
    else:
        print(f"❌ Test failed: {result['error']}")
    
    return result

def parse_test_output(result: Dict[str, Any]):
    """Parse test output to extract metrics."""
    output = result["output"]
    metrics = result["metrics"]
    
    # Common patterns to look for
    patterns = [
        (r"Average latency: ([\d.]+) ms", "avg_latency_ms"),
        (r"Min latency: ([\d.]+) ms", "min_latency_ms"),
        (r"Max latency: ([\d.]+) ms", "max_latency_ms"),
        (r"P95 latency: ([\d.]+) ms", "p95_latency_ms"),
        (r"P99 latency: ([\d.]+) ms", "p99_latency_ms"),
        (r"Throughput: ([\d.]+) ops/sec", "throughput_ops_sec"),
        (r"Success rate: ([\d.]+)%", "success_rate"),
        (r"Memory usage: ([\d.]+) MB", "memory_usage_mb"),
        (r"Improvement factor: ([\d.]+)x", "improvement_factor"),
        (r"Startup time: ([\d.]+) ms", "startup_time_ms")
    ]
    
    import re
    for pattern, metric_name in patterns:
        match = re.search(pattern, output)
        if match:
            try:
                metrics[metric_name] = float(match.group(1))
            except:
                pass

# ============================================================================
# REPORT GENERATION
# ============================================================================

def evaluate_results(results: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Evaluate results against performance targets."""
    evaluation = {
        "targets_met": 0,
        "targets_failed": 0,
        "details": [],
        "overall_pass": False
    }
    
    # Check each target
    for result in results:
        if not result["success"]:
            continue
        
        metrics = result["metrics"]
        
        # Latency target
        if "avg_latency_ms" in metrics:
            passed = metrics["avg_latency_ms"] <= TARGETS["latency_ms"]
            evaluation["details"].append({
                "metric": "Average Latency",
                "target": f"<{TARGETS['latency_ms']}ms",
                "actual": f"{metrics['avg_latency_ms']:.3f}ms",
                "passed": passed
            })
            if passed:
                evaluation["targets_met"] += 1
            else:
                evaluation["targets_failed"] += 1
        
        # Throughput target
        if "throughput_ops_sec" in metrics:
            passed = metrics["throughput_ops_sec"] >= TARGETS["throughput_ops"]
            evaluation["details"].append({
                "metric": "Throughput",
                "target": f">{TARGETS['throughput_ops']} ops/sec",
                "actual": f"{metrics['throughput_ops_sec']:.1f} ops/sec",
                "passed": passed
            })
            if passed:
                evaluation["targets_met"] += 1
            else:
                evaluation["targets_failed"] += 1
        
        # Memory target
        if "memory_peak_mb" in metrics:
            passed = metrics["memory_peak_mb"] <= TARGETS["memory_mb"]
            evaluation["details"].append({
                "metric": "Memory Usage",
                "target": f"<{TARGETS['memory_mb']}MB",
                "actual": f"{metrics['memory_peak_mb']:.1f}MB",
                "passed": passed
            })
            if passed:
                evaluation["targets_met"] += 1
            else:
                evaluation["targets_failed"] += 1
        
        # Performance improvement
        if "improvement_factor" in metrics:
            passed = metrics["improvement_factor"] >= TARGETS["improvement_factor"]
            evaluation["details"].append({
                "metric": "Performance Improvement",
                "target": f">{TARGETS['improvement_factor']}x",
                "actual": f"{metrics['improvement_factor']:.1f}x",
                "passed": passed
            })
            if passed:
                evaluation["targets_met"] += 1
            else:
                evaluation["targets_failed"] += 1
    
    evaluation["overall_pass"] = evaluation["targets_failed"] == 0
    return evaluation

def generate_json_report(results: List[Dict[str, Any]], output_file: Path):
    """Generate JSON format report."""
    report = {
        "test_run": {
            "timestamp": datetime.datetime.now().isoformat(),
            "system_info": get_system_info(),
            "targets": TARGETS
        },
        "results": results,
        "evaluation": evaluate_results(results)
    }
    
    with open(output_file, 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"\nJSON report saved to: {output_file}")

def generate_html_report(results: List[Dict[str, Any]], output_file: Path):
    """Generate HTML format report with visualizations."""
    evaluation = evaluate_results(results)
    system_info = get_system_info()
    
    html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Goxel v14.0 Performance Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }}
        h1 {{ color: #333; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; }}
        h2 {{ color: #555; margin-top: 30px; }}
        .summary {{ background: #f9f9f9; padding: 15px; border-radius: 5px; margin: 20px 0; }}
        .pass {{ color: #4CAF50; font-weight: bold; }}
        .fail {{ color: #f44336; font-weight: bold; }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        th, td {{ padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }}
        th {{ background: #4CAF50; color: white; }}
        tr:hover {{ background: #f5f5f5; }}
        .metric-card {{ display: inline-block; margin: 10px; padding: 20px; background: #fff; border: 1px solid #ddd; border-radius: 5px; min-width: 200px; }}
        .metric-value {{ font-size: 24px; font-weight: bold; color: #333; }}
        .metric-label {{ color: #666; font-size: 14px; }}
        .chart {{ margin: 20px 0; }}
        .test-result {{ margin: 20px 0; padding: 15px; background: #f9f9f9; border-left: 4px solid #ddd; }}
        .test-result.success {{ border-color: #4CAF50; }}
        .test-result.failed {{ border-color: #f44336; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Goxel v14.0 Performance Benchmark Report</h1>
        
        <div class="summary">
            <h2>Executive Summary</h2>
            <p><strong>Test Date:</strong> {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
            <p><strong>Platform:</strong> {system_info['platform']}</p>
            <p><strong>Overall Result:</strong> <span class="{'pass' if evaluation['overall_pass'] else 'fail'}">
                {'PASS' if evaluation['overall_pass'] else 'FAIL'}
            </span></p>
            <p><strong>Targets Met:</strong> {evaluation['targets_met']} / {evaluation['targets_met'] + evaluation['targets_failed']}</p>
        </div>
        
        <h2>Performance Targets</h2>
        <table>
            <tr>
                <th>Metric</th>
                <th>Target</th>
                <th>Actual</th>
                <th>Status</th>
            </tr>
"""
    
    for detail in evaluation["details"]:
        status_class = "pass" if detail["passed"] else "fail"
        status_text = "✅ PASS" if detail["passed"] else "❌ FAIL"
        html_content += f"""
            <tr>
                <td>{detail['metric']}</td>
                <td>{detail['target']}</td>
                <td>{detail['actual']}</td>
                <td class="{status_class}">{status_text}</td>
            </tr>
"""
    
    html_content += """
        </table>
        
        <h2>Test Results</h2>
"""
    
    for result in results:
        status_class = "success" if result["success"] else "failed"
        html_content += f"""
        <div class="test-result {status_class}">
            <h3>{result['name']}</h3>
            <p><strong>Description:</strong> {result['description']}</p>
            <p><strong>Duration:</strong> {result['duration']:.2f}s</p>
"""
        
        if result["success"] and result["metrics"]:
            html_content += "<p><strong>Metrics:</strong></p><ul>"
            for key, value in result["metrics"].items():
                html_content += f"<li>{key}: {value}</li>"
            html_content += "</ul>"
        elif result["error"]:
            html_content += f'<p class="fail">Error: {result["error"]}</p>'
        
        html_content += "</div>"
    
    html_content += """
        <h2>System Information</h2>
        <div class="summary">
            <p><strong>CPU:</strong> {cpu_count} cores</p>
            <p><strong>Memory:</strong> {memory:.1f} GB</p>
            <p><strong>Python:</strong> {python}</p>
        </div>
        
        <div style="margin-top: 40px; padding-top: 20px; border-top: 1px solid #ddd; color: #666; font-size: 12px;">
            Generated by Goxel v14.0 Performance Benchmark Suite
        </div>
    </div>
</body>
</html>
""".format(
        cpu_count=system_info['cpu_count'],
        memory=system_info['memory_total_mb'] / 1024,
        python=system_info['python_version'].split()[0]
    )
    
    with open(output_file, 'w') as f:
        f.write(html_content)
    
    print(f"HTML report saved to: {output_file}")

# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Goxel v14.0 Automated Performance Benchmark Runner"
    )
    parser.add_argument(
        "--skip-daemon",
        action="store_true",
        help="Skip daemon startup (assume it's already running)"
    )
    parser.add_argument(
        "--tests",
        nargs="+",
        choices=[t["name"] for t in BENCHMARK_TESTS],
        help="Run only specific tests"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=RESULTS_DIR,
        help="Output directory for results"
    )
    parser.add_argument(
        "--format",
        choices=["json", "html", "both"],
        default="both",
        help="Output format for reports"
    )
    parser.add_argument(
        "--ci-mode",
        action="store_true",
        help="CI/CD mode - exit with error code if targets not met"
    )
    
    args = parser.parse_args()
    
    # Check prerequisites
    if not check_binaries():
        return 1
    
    # Ensure output directory exists
    args.output_dir.mkdir(parents=True, exist_ok=True)
    
    # Start daemon if needed
    daemon_proc = None
    if not args.skip_daemon:
        daemon_proc = start_daemon()
        if not daemon_proc:
            print("Failed to start daemon")
            return 1
    
    # Select tests to run
    tests_to_run = BENCHMARK_TESTS
    if args.tests:
        tests_to_run = [t for t in BENCHMARK_TESTS if t["name"] in args.tests]
    
    # Run benchmarks
    results = []
    try:
        print(f"\nRunning {len(tests_to_run)} benchmarks...")
        for test in tests_to_run:
            result = run_benchmark(test, daemon_proc)
            results.append(result)
            time.sleep(1)  # Brief pause between tests
    
    finally:
        # Stop daemon
        if daemon_proc:
            stop_daemon(daemon_proc)
    
    # Generate reports
    timestamp = get_timestamp()
    
    if args.format in ["json", "both"]:
        json_file = args.output_dir / f"benchmark_results_{timestamp}.json"
        generate_json_report(results, json_file)
    
    if args.format in ["html", "both"]:
        html_file = args.output_dir / f"benchmark_report_{timestamp}.html"
        generate_html_report(results, html_file)
    
    # Evaluate results
    evaluation = evaluate_results(results)
    
    print("\n" + "="*60)
    print("BENCHMARK SUMMARY")
    print("="*60)
    print(f"Total tests run: {len(results)}")
    print(f"Successful tests: {sum(1 for r in results if r['success'])}")
    print(f"Failed tests: {sum(1 for r in results if not r['success'])}")
    print(f"Targets met: {evaluation['targets_met']}")
    print(f"Targets failed: {evaluation['targets_failed']}")
    print(f"Overall: {'✅ PASS' if evaluation['overall_pass'] else '❌ FAIL'}")
    
    # Exit code for CI/CD
    if args.ci_mode and not evaluation["overall_pass"]:
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())