#!/usr/bin/env python3
"""
Goxel v14.0 Integration Test Results Validator

This script analyzes test results and performance metrics to determine
if the v14 daemon meets production requirements.
"""

import json
import sys
import os
import glob
from datetime import datetime
from typing import Dict, List, Tuple, Optional

# Performance targets
PERFORMANCE_TARGETS = {
    'latency_ms': 2.1,           # Target: <2.1ms average latency
    'throughput_ops_per_sec': 1000,  # Target: >1000 ops/s
    'memory_mb': 50,              # Target: <50MB memory usage
    'cpu_percent': 80,            # Target: <80% CPU usage
    'connection_time_ms': 10,     # Target: <10ms connection time
    'concurrent_clients': 50,     # Target: Support 50+ concurrent clients
    'success_rate': 99.0,         # Target: >99% success rate
}

# Test categories
TEST_CATEGORIES = {
    'connectivity': [
        'Daemon Startup',
        'Single Client Connection',
        'Multiple Client Connections',
        'Graceful Shutdown'
    ],
    'methods': [
        'Echo Method',
        'Version Method',
        'Status Method',
        'Create Project',
        'Add Voxel',
        'Save Project',
        'Load Project',
        'Export Model'
    ],
    'stress': [
        '10 Concurrent Clients',
        '50 Concurrent Clients',
        '1000 Operations Sequential',
        'Burst Load Test',
        'Memory Leak Test'
    ],
    'performance': [
        'Latency Benchmark',
        'Throughput Test',
        'Memory Profiling',
        'Performance Comparison'
    ]
}

class TestResultValidator:
    def __init__(self, results_dir: str = 'results'):
        self.results_dir = results_dir
        self.results = {}
        self.performance_metrics = {}
        self.issues = []
        self.warnings = []
        
    def load_results(self) -> bool:
        """Load test results from result files."""
        # Find latest test report
        report_files = glob.glob(f'{self.results_dir}/integration_test_report_*.md')
        if not report_files:
            print("ERROR: No test report files found")
            return False
            
        latest_report = max(report_files)
        print(f"Loading results from: {latest_report}")
        
        # Parse markdown report
        with open(latest_report, 'r') as f:
            content = f.read()
            
        # Extract test results
        self._parse_test_results(content)
        
        # Load performance metrics if available
        perf_files = glob.glob(f'{self.results_dir}/../performance/performance_results*.json')
        if perf_files:
            with open(max(perf_files), 'r') as f:
                self.performance_metrics = json.load(f)
                
        return True
        
    def _parse_test_results(self, content: str):
        """Parse test results from markdown report."""
        lines = content.split('\n')
        
        for line in lines:
            if line.startswith('- PASS:'):
                test_name = line.replace('- PASS:', '').strip()
                self.results[test_name] = 'PASS'
            elif line.startswith('- FAIL:'):
                test_name = line.replace('- FAIL:', '').strip()
                self.results[test_name] = 'FAIL'
                
    def validate_connectivity(self) -> Tuple[bool, List[str]]:
        """Validate basic connectivity tests."""
        passed = True
        issues = []
        
        for test in TEST_CATEGORIES['connectivity']:
            if test not in self.results:
                issues.append(f"Missing test: {test}")
                passed = False
            elif self.results[test] != 'PASS':
                issues.append(f"Failed test: {test}")
                passed = False
                
        return passed, issues
        
    def validate_methods(self) -> Tuple[bool, List[str]]:
        """Validate JSON-RPC method tests."""
        passed = True
        issues = []
        
        for test in TEST_CATEGORIES['methods']:
            if test not in self.results:
                issues.append(f"Missing method test: {test}")
                passed = False
            elif self.results[test] != 'PASS':
                issues.append(f"Failed method test: {test}")
                passed = False
                
        return passed, issues
        
    def validate_performance(self) -> Tuple[bool, List[str]]:
        """Validate performance against targets."""
        passed = True
        issues = []
        
        if not self.performance_metrics:
            issues.append("No performance metrics available")
            return False, issues
            
        # Check latency
        if 'average_latency_ms' in self.performance_metrics:
            latency = self.performance_metrics['average_latency_ms']
            if latency > PERFORMANCE_TARGETS['latency_ms']:
                issues.append(
                    f"Latency {latency:.2f}ms exceeds target "
                    f"{PERFORMANCE_TARGETS['latency_ms']}ms"
                )
                passed = False
            else:
                print(f"✓ Latency: {latency:.2f}ms (target: <{PERFORMANCE_TARGETS['latency_ms']}ms)")
                
        # Check throughput
        if 'throughput_ops_per_sec' in self.performance_metrics:
            throughput = self.performance_metrics['throughput_ops_per_sec']
            if throughput < PERFORMANCE_TARGETS['throughput_ops_per_sec']:
                issues.append(
                    f"Throughput {throughput:.0f} ops/s below target "
                    f"{PERFORMANCE_TARGETS['throughput_ops_per_sec']} ops/s"
                )
                passed = False
            else:
                print(f"✓ Throughput: {throughput:.0f} ops/s (target: >{PERFORMANCE_TARGETS['throughput_ops_per_sec']} ops/s)")
                
        # Check memory usage
        if 'memory_usage_mb' in self.performance_metrics:
            memory = self.performance_metrics['memory_usage_mb']
            if memory > PERFORMANCE_TARGETS['memory_mb']:
                self.warnings.append(
                    f"Memory usage {memory:.1f}MB exceeds target "
                    f"{PERFORMANCE_TARGETS['memory_mb']}MB"
                )
            else:
                print(f"✓ Memory: {memory:.1f}MB (target: <{PERFORMANCE_TARGETS['memory_mb']}MB)")
                
        return passed, issues
        
    def validate_stress(self) -> Tuple[bool, List[str]]:
        """Validate stress test results."""
        passed = True
        issues = []
        
        # Check concurrent client support
        if '50 Concurrent Clients' in self.results:
            if self.results['50 Concurrent Clients'] != 'PASS':
                issues.append("Failed to support 50 concurrent clients")
                passed = False
        else:
            self.warnings.append("50 concurrent clients test not run")
            
        # Check memory leak test
        if 'Memory Leak Test' in self.results:
            if self.results['Memory Leak Test'] != 'PASS':
                issues.append("Memory leak detected")
                passed = False
                
        return passed, issues
        
    def generate_summary(self) -> Dict[str, any]:
        """Generate validation summary."""
        # Run all validations
        conn_passed, conn_issues = self.validate_connectivity()
        meth_passed, meth_issues = self.validate_methods()
        perf_passed, perf_issues = self.validate_performance()
        stress_passed, stress_issues = self.validate_stress()
        
        # Compile all issues
        all_issues = conn_issues + meth_issues + perf_issues + stress_issues
        
        # Calculate overall status
        all_passed = conn_passed and meth_passed and perf_passed and stress_passed
        
        return {
            'timestamp': datetime.now().isoformat(),
            'overall_status': 'PASS' if all_passed else 'FAIL',
            'connectivity': {'passed': conn_passed, 'issues': conn_issues},
            'methods': {'passed': meth_passed, 'issues': meth_issues},
            'performance': {'passed': perf_passed, 'issues': perf_issues},
            'stress': {'passed': stress_passed, 'issues': stress_issues},
            'total_issues': len(all_issues),
            'total_warnings': len(self.warnings),
            'issues': all_issues,
            'warnings': self.warnings,
            'ready_for_production': all_passed and len(all_issues) == 0
        }
        
    def print_report(self, summary: Dict[str, any]):
        """Print validation report."""
        print("\n" + "=" * 60)
        print("Goxel v14.0 Integration Test Validation Report")
        print("=" * 60)
        print(f"Timestamp: {summary['timestamp']}")
        print(f"Overall Status: {summary['overall_status']}")
        print()
        
        # Category results
        categories = ['connectivity', 'methods', 'performance', 'stress']
        for cat in categories:
            status = '✓' if summary[cat]['passed'] else '✗'
            print(f"{status} {cat.capitalize()}: ", end='')
            if summary[cat]['passed']:
                print("PASSED")
            else:
                print(f"FAILED ({len(summary[cat]['issues'])} issues)")
                
        print()
        
        # Issues
        if summary['issues']:
            print("CRITICAL ISSUES:")
            for issue in summary['issues']:
                print(f"  ✗ {issue}")
            print()
            
        # Warnings
        if summary['warnings']:
            print("WARNINGS:")
            for warning in summary['warnings']:
                print(f"  ⚠ {warning}")
            print()
            
        # Production readiness
        print("PRODUCTION READINESS:")
        if summary['ready_for_production']:
            print("  ✅ The v14 daemon is READY for production deployment!")
            print("  All tests passed and performance targets met.")
        else:
            print("  ❌ The v14 daemon is NOT ready for production.")
            print(f"  Please address {summary['total_issues']} critical issues.")
            
        print("=" * 60)
        
def main():
    validator = TestResultValidator()
    
    # Load test results
    if not validator.load_results():
        print("ERROR: Failed to load test results")
        sys.exit(1)
        
    # Generate validation summary
    summary = validator.generate_summary()
    
    # Print report
    validator.print_report(summary)
    
    # Save summary to file
    summary_file = f"{validator.results_dir}/validation_summary_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)
    print(f"\nValidation summary saved to: {summary_file}")
    
    # Exit with appropriate code
    sys.exit(0 if summary['ready_for_production'] else 1)
    
if __name__ == '__main__':
    main()