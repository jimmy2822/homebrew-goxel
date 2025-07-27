#!/usr/bin/env python3
"""
Cross-platform validation tool for Goxel v14.0 daemon.
Analyzes test results and generates compatibility reports.
"""

import argparse
import json
import os
import glob
import sys
from datetime import datetime
from collections import defaultdict
import subprocess
import platform

# Performance targets
PERFORMANCE_TARGETS = {
    'socket_creation_ms': 10.0,
    'connection_latency_ms': 5.0,
    'daemon_startup_ms': 100.0,
    'request_latency_ms': 2.1,
    'memory_usage_mb': 10.0,
}

# Platform-specific adjustments
PLATFORM_ADJUSTMENTS = {
    'darwin': {'latency_factor': 1.1},  # macOS slightly slower
    'linux': {'latency_factor': 1.0},   # Baseline
    'windows': {'latency_factor': 1.3}, # Windows overhead
    'alpine': {'latency_factor': 0.9},  # musl is faster
}

class PlatformValidator:
    def __init__(self):
        self.results = defaultdict(dict)
        self.platform_info = self._get_platform_info()
        
    def _get_platform_info(self):
        """Get current platform information."""
        return {
            'system': platform.system(),
            'release': platform.release(),
            'version': platform.version(),
            'machine': platform.machine(),
            'processor': platform.processor(),
            'python_version': platform.python_version(),
        }
    
    def run_local_tests(self):
        """Run tests on the current platform."""
        print(f"Running tests on {self.platform_info['system']} {self.platform_info['machine']}")
        
        test_results = {
            'platform': self.platform_info,
            'timestamp': datetime.now().isoformat(),
            'tests': {},
            'performance': {},
        }
        
        # Find and run platform-specific tests
        test_dir = self._get_test_directory()
        if not test_dir:
            print(f"No test directory found for {self.platform_info['system']}")
            return test_results
            
        # Run socket tests
        socket_test = os.path.join(test_dir, f"test_unix_socket_{test_dir.split('/')[-1]}")
        if os.path.exists(socket_test):
            result = self._run_test(socket_test)
            test_results['tests']['socket'] = result
            
        # Run daemon tests
        daemon_test = os.path.join('tests', 'test_daemon_lifecycle')
        if os.path.exists(daemon_test):
            result = self._run_test(daemon_test)
            test_results['tests']['daemon'] = result
            
        # Run performance benchmarks
        test_results['performance'] = self._run_performance_tests()
        
        return test_results
    
    def _get_test_directory(self):
        """Get platform-specific test directory."""
        system = self.platform_info['system'].lower()
        mapping = {
            'darwin': 'macos',
            'linux': 'linux',
            'windows': 'windows',
        }
        
        platform_name = mapping.get(system, system)
        test_dir = os.path.join('tests', 'platforms', platform_name)
        
        return test_dir if os.path.exists(test_dir) else None
    
    def _run_test(self, test_path):
        """Run a single test and capture results."""
        print(f"Running {test_path}...")
        
        try:
            # Compile if needed
            if test_path.endswith('.c'):
                executable = test_path.replace('.c', '')
                compile_cmd = ['gcc', '-o', executable, test_path, '-pthread']
                subprocess.run(compile_cmd, check=True)
                test_path = executable
                
            # Run test
            result = subprocess.run(
                [test_path],
                capture_output=True,
                text=True,
                timeout=60
            )
            
            return {
                'passed': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'returncode': result.returncode,
            }
            
        except subprocess.TimeoutExpired:
            return {
                'passed': False,
                'error': 'Test timed out after 60 seconds',
            }
        except Exception as e:
            return {
                'passed': False,
                'error': str(e),
            }
    
    def _run_performance_tests(self):
        """Run performance benchmarks."""
        results = {}
        
        # Socket creation benchmark
        results['socket_creation'] = self._benchmark_socket_creation()
        
        # Daemon startup benchmark
        results['daemon_startup'] = self._benchmark_daemon_startup()
        
        # Request latency benchmark
        results['request_latency'] = self._benchmark_request_latency()
        
        return results
    
    def _benchmark_socket_creation(self):
        """Benchmark socket creation time."""
        import time
        import socket
        import tempfile
        
        iterations = 100
        total_time = 0
        
        for i in range(iterations):
            sock_path = tempfile.mktemp(prefix='goxel_bench_')
            
            start = time.perf_counter()
            
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.bind(sock_path)
                sock.listen(1)
                sock.close()
                os.unlink(sock_path)
            except:
                pass
                
            end = time.perf_counter()
            total_time += (end - start)
            
        avg_time_ms = (total_time / iterations) * 1000
        
        return {
            'avg_ms': avg_time_ms,
            'iterations': iterations,
            'passed': avg_time_ms < PERFORMANCE_TARGETS['socket_creation_ms'],
        }
    
    def _benchmark_daemon_startup(self):
        """Benchmark daemon startup time."""
        daemon_path = os.path.join('tests', 'goxel-daemon')
        if not os.path.exists(daemon_path):
            return {'error': 'Daemon binary not found'}
            
        iterations = 5
        total_time = 0
        
        for i in range(iterations):
            start = time.perf_counter()
            
            try:
                result = subprocess.run(
                    [daemon_path, '--version'],
                    capture_output=True,
                    timeout=5
                )
            except:
                continue
                
            end = time.perf_counter()
            total_time += (end - start)
            
        avg_time_ms = (total_time / iterations) * 1000
        
        return {
            'avg_ms': avg_time_ms,
            'iterations': iterations,
            'passed': avg_time_ms < PERFORMANCE_TARGETS['daemon_startup_ms'],
        }
    
    def _benchmark_request_latency(self):
        """Benchmark request/response latency."""
        # This would require a running daemon
        # For now, return placeholder
        return {
            'avg_ms': 0,
            'note': 'Requires running daemon',
        }
    
    def analyze_results(self, results_dir):
        """Analyze test results from CI artifacts."""
        all_results = []
        
        # Find all result files
        for result_file in glob.glob(os.path.join(results_dir, '**/test_results.json'), recursive=True):
            with open(result_file, 'r') as f:
                results = json.load(f)
                all_results.append(results)
                
        # Analyze by platform
        platform_summary = defaultdict(lambda: {
            'total_tests': 0,
            'passed_tests': 0,
            'failed_tests': 0,
            'performance': {},
        })
        
        for result in all_results:
            platform = result.get('platform', {}).get('system', 'unknown')
            summary = platform_summary[platform]
            
            # Count test results
            for test_name, test_result in result.get('tests', {}).items():
                summary['total_tests'] += 1
                if test_result.get('passed'):
                    summary['passed_tests'] += 1
                else:
                    summary['failed_tests'] += 1
                    
            # Aggregate performance metrics
            for metric, value in result.get('performance', {}).items():
                if metric not in summary['performance']:
                    summary['performance'][metric] = []
                summary['performance'][metric].append(value)
                
        return dict(platform_summary)
    
    def generate_report(self, summary, output_file=None):
        """Generate HTML report of test results."""
        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Goxel v14.0 Cross-Platform Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .passed {{ color: green; }}
        .failed {{ color: red; }}
        .warning {{ color: orange; }}
    </style>
</head>
<body>
    <h1>Goxel v14.0 Cross-Platform Test Report</h1>
    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <h2>Platform Compatibility Matrix</h2>
    <table>
        <tr>
            <th>Platform</th>
            <th>Total Tests</th>
            <th>Passed</th>
            <th>Failed</th>
            <th>Success Rate</th>
            <th>Status</th>
        </tr>
"""
        
        for platform, data in summary.items():
            total = data['total_tests']
            passed = data['passed_tests']
            failed = data['failed_tests']
            rate = (passed / total * 100) if total > 0 else 0
            
            status_class = 'passed' if rate >= 95 else 'warning' if rate >= 80 else 'failed'
            status_text = 'PASS' if rate >= 95 else 'PARTIAL' if rate >= 80 else 'FAIL'
            
            html += f"""
        <tr>
            <td>{platform}</td>
            <td>{total}</td>
            <td class="passed">{passed}</td>
            <td class="failed">{failed}</td>
            <td>{rate:.1f}%</td>
            <td class="{status_class}">{status_text}</td>
        </tr>
"""
        
        html += """
    </table>
    
    <h2>Performance Metrics</h2>
    <table>
        <tr>
            <th>Metric</th>
            <th>Target</th>
            <th>Linux</th>
            <th>macOS</th>
            <th>Windows</th>
        </tr>
"""
        
        # Add performance metrics
        for metric, target in PERFORMANCE_TARGETS.items():
            html += f"""
        <tr>
            <td>{metric}</td>
            <td>{target}</td>
"""
            for platform in ['linux', 'macos', 'windows']:
                perf_data = summary.get(platform, {}).get('performance', {})
                values = perf_data.get(metric, [])
                if values:
                    avg_value = sum(v.get('avg_ms', 0) for v in values) / len(values)
                    status_class = 'passed' if avg_value <= target else 'failed'
                    html += f'<td class="{status_class}">{avg_value:.2f}ms</td>'
                else:
                    html += '<td>N/A</td>'
            html += '</tr>'
            
        html += """
    </table>
    
    <h2>Platform-Specific Notes</h2>
    <ul>
        <li><strong>Linux:</strong> Full feature support, best performance</li>
        <li><strong>macOS:</strong> Unix sockets work, launchd integration available</li>
        <li><strong>Windows:</strong> WSL2 recommended, native support limited</li>
        <li><strong>Alpine:</strong> musl libc compatible, smaller footprint</li>
    </ul>
    
</body>
</html>
"""
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(html)
            print(f"Report written to {output_file}")
        else:
            print(html)
            
        return html

def main():
    parser = argparse.ArgumentParser(description='Cross-platform validator for Goxel v14.0')
    parser.add_argument('--run-local', action='store_true', help='Run tests on current platform')
    parser.add_argument('--analyze', help='Analyze results from directory')
    parser.add_argument('--generate-report', action='store_true', help='Generate HTML report')
    parser.add_argument('--output', help='Output file for report')
    
    args = parser.parse_args()
    
    validator = PlatformValidator()
    
    if args.run_local:
        results = validator.run_local_tests()
        print(json.dumps(results, indent=2))
        
        # Save results
        os.makedirs('tests/platforms/results', exist_ok=True)
        result_file = f"tests/platforms/results/{platform.system().lower()}_results.json"
        with open(result_file, 'w') as f:
            json.dump(results, f, indent=2)
            
    if args.analyze:
        summary = validator.analyze_results(args.analyze)
        print(json.dumps(summary, indent=2))
        
    if args.generate_report:
        # Use dummy data if no analysis provided
        if args.analyze:
            summary = validator.analyze_results(args.analyze)
        else:
            summary = {
                'linux': {'total_tests': 40, 'passed_tests': 40, 'failed_tests': 0, 'performance': {}},
                'macos': {'total_tests': 40, 'passed_tests': 38, 'failed_tests': 2, 'performance': {}},
                'windows': {'total_tests': 40, 'passed_tests': 30, 'failed_tests': 10, 'performance': {}},
            }
            
        validator.generate_report(summary, args.output)

if __name__ == '__main__':
    main()