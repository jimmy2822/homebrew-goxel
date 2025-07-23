#!/usr/bin/env python3
"""
Simple Performance Test for Goxel v13
Phase 6.3: Performance Tuning

A lightweight performance test that doesn't require external dependencies.
"""

import os
import sys
import time
import subprocess
import json
import platform

class SimplePerformanceTest:
    def __init__(self, goxel_cli_path="../goxel-headless"):
        self.goxel_cli_path = goxel_cli_path
        self.results = {
            "timestamp": time.time(),
            "platform": platform.platform(),
            "architecture": platform.machine(),
            "tests": {}
        }
        
    def run_timed_command(self, cmd, timeout=30):
        """Run a command and measure execution time"""
        start_time = time.time()
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            elapsed = time.time() - start_time
            return {
                "elapsed_ms": elapsed * 1000,
                "exit_code": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "success": result.returncode == 0
            }
        except subprocess.TimeoutExpired:
            elapsed = time.time() - start_time
            return {
                "elapsed_ms": elapsed * 1000,
                "exit_code": -1,
                "stdout": "",
                "stderr": "Timeout",
                "success": False
            }
        except Exception as e:
            elapsed = time.time() - start_time
            return {
                "elapsed_ms": elapsed * 1000,
                "exit_code": -1,
                "stdout": "",
                "stderr": str(e),
                "success": False
            }
    
    def test_cli_startup(self):
        """Test CLI startup time"""
        print("Testing CLI startup performance...")
        
        times = []
        for i in range(5):
            result = self.run_timed_command([self.goxel_cli_path, "--help"])
            if result["success"] or "help" in result["stderr"].lower():
                times.append(result["elapsed_ms"])
        
        if times:
            avg_time = sum(times) / len(times)
            self.results["tests"]["cli_startup"] = {
                "average_ms": avg_time,
                "min_ms": min(times),
                "max_ms": max(times),
                "iterations": len(times),
                "target_ms": 1000,
                "passes": avg_time < 1000
            }
            print(f"  CLI startup: {avg_time:.2f}ms (target: <1000ms)")
        else:
            print("  CLI startup test failed")
            
    def test_project_creation(self):
        """Test project creation performance"""
        print("Testing project creation performance...")
        
        times = []
        for i in range(3):
            test_file = f"/tmp/perf_test_{i}.gox"
            result = self.run_timed_command([self.goxel_cli_path, "create", test_file])
            if result["success"]:
                times.append(result["elapsed_ms"])
            # Clean up
            try:
                os.remove(test_file)
            except:
                pass
        
        if times:
            avg_time = sum(times) / len(times)
            self.results["tests"]["project_creation"] = {
                "average_ms": avg_time,
                "min_ms": min(times),
                "max_ms": max(times),
                "iterations": len(times),
                "target_ms": 1000,
                "passes": avg_time < 1000
            }
            print(f"  Project creation: {avg_time:.2f}ms (target: <1000ms)")
        else:
            print("  Project creation test failed")
            
    def test_basic_operations(self):
        """Test basic CLI operations"""
        print("Testing basic operations...")
        
        # Test help command
        help_result = self.run_timed_command([self.goxel_cli_path, "--help"], timeout=10)
        
        # Test version command (if available)
        version_result = self.run_timed_command([self.goxel_cli_path, "--version"], timeout=10)
        
        self.results["tests"]["basic_operations"] = {
            "help_command": {
                "elapsed_ms": help_result["elapsed_ms"],
                "success": help_result["success"] or "help" in help_result["stderr"].lower(),
                "passes": help_result["elapsed_ms"] < 5000
            },
            "version_command": {
                "elapsed_ms": version_result["elapsed_ms"],
                "success": version_result["success"] or "version" in version_result["stderr"].lower(),
                "passes": version_result["elapsed_ms"] < 5000
            }
        }
        
        print(f"  Help command: {help_result['elapsed_ms']:.2f}ms")
        print(f"  Version command: {version_result['elapsed_ms']:.2f}ms")
        
    def test_file_size(self):
        """Test executable file size"""
        print("Testing executable size...")
        
        try:
            size = os.path.getsize(self.goxel_cli_path)
            size_mb = size / (1024 * 1024)
            
            self.results["tests"]["file_size"] = {
                "size_bytes": size,
                "size_mb": size_mb,
                "target_mb": 20,  # Reasonable target for CLI tool
                "passes": size_mb < 20
            }
            
            print(f"  Executable size: {size_mb:.2f}MB (target: <20MB)")
        except Exception as e:
            print(f"  File size test failed: {e}")
            
    def run_all_tests(self):
        """Run all performance tests"""
        print("Starting Goxel v13 Simple Performance Tests")
        print("=" * 45)
        
        if not os.path.exists(self.goxel_cli_path):
            print(f"Error: {self.goxel_cli_path} not found")
            return False
        
        try:
            self.test_file_size()
            self.test_cli_startup()
            self.test_basic_operations()
            self.test_project_creation()
            return True
        except Exception as e:
            print(f"Test execution failed: {e}")
            return False
    
    def calculate_score(self):
        """Calculate overall performance score"""
        total_tests = 0
        passed_tests = 0
        
        for test_name, test_data in self.results["tests"].items():
            if isinstance(test_data, dict):
                if "passes" in test_data:
                    total_tests += 1
                    if test_data["passes"]:
                        passed_tests += 1
                else:
                    # Handle nested test data
                    for subtest_name, subtest_data in test_data.items():
                        if isinstance(subtest_data, dict) and "passes" in subtest_data:
                            total_tests += 1
                            if subtest_data["passes"]:
                                passed_tests += 1
        
        score = (passed_tests / total_tests * 100) if total_tests > 0 else 0
        return score, passed_tests, total_tests
    
    def print_summary(self):
        """Print test summary"""
        print("\n" + "=" * 45)
        print("PERFORMANCE TEST SUMMARY")
        print("=" * 45)
        
        score, passed, total = self.calculate_score()
        
        print(f"Overall Score: {score:.1f}% ({passed}/{total} tests passed)")
        print(f"Platform: {self.results['platform']}")
        print(f"Architecture: {self.results['architecture']}")
        print()
        
        # Print individual test results
        for test_name, test_data in self.results["tests"].items():
            print(f"{test_name.replace('_', ' ').title()}:")
            if isinstance(test_data, dict):
                if "passes" in test_data:
                    status = "✅ PASS" if test_data["passes"] else "❌ FAIL"
                    print(f"  {status}")
                    if "average_ms" in test_data:
                        print(f"  Average: {test_data['average_ms']:.2f}ms")
                    elif "elapsed_ms" in test_data:
                        print(f"  Time: {test_data['elapsed_ms']:.2f}ms")
                    if "size_mb" in test_data:
                        print(f"  Size: {test_data['size_mb']:.2f}MB")
                else:
                    # Handle nested test data
                    for subtest_name, subtest_data in test_data.items():
                        if isinstance(subtest_data, dict) and "passes" in subtest_data:
                            status = "✅ PASS" if subtest_data["passes"] else "❌ FAIL"
                            print(f"  {subtest_name}: {status} ({subtest_data['elapsed_ms']:.2f}ms)")
            print()
        
        return score >= 80  # 80% pass rate for success
    
    def save_results(self, filename="simple_perf_results.json"):
        """Save results to JSON file"""
        with open(filename, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"Results saved to: {filename}")

def main():
    if len(sys.argv) > 1:
        goxel_cli_path = sys.argv[1]
    else:
        goxel_cli_path = "../goxel-headless"
    
    tester = SimplePerformanceTest(goxel_cli_path)
    
    if tester.run_all_tests():
        success = tester.print_summary()
        tester.save_results()
        return 0 if success else 1
    else:
        print("Performance testing failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())