#!/usr/bin/env python3
"""
Goxel v13 Test Report Generator
Phase 6: Production Ready - Test Coverage Analysis

Generates comprehensive test reports including:
- Test coverage statistics
- Performance benchmarks
- Memory usage analysis
- Success/failure rates
- Compliance verification
"""

import os
import sys
import json
import subprocess
import re
from datetime import datetime
from typing import Dict, List, Tuple, Optional

class TestReportGenerator:
    def __init__(self, test_dir: str = "."):
        self.test_dir = os.path.abspath(test_dir)
        self.report_data = {
            "timestamp": datetime.now().isoformat(),
            "test_suites": {},
            "performance": {},
            "coverage": {},
            "compliance": {},
            "summary": {}
        }
        
    def run_command(self, cmd: List[str], cwd: Optional[str] = None) -> Tuple[int, str, str]:
        """Run a command and return exit code, stdout, stderr"""
        try:
            result = subprocess.run(
                cmd, 
                cwd=cwd or self.test_dir,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timed out"
        except Exception as e:
            return -1, "", str(e)
    
    def parse_test_output(self, output: str) -> Dict:
        """Parse test output to extract statistics"""
        stats = {
            "tests_run": 0,
            "tests_passed": 0,
            "tests_failed": 0,
            "pass_rate": 0.0,
            "details": []
        }
        
        # Look for test summary patterns
        run_match = re.search(r"Tests run:\s*(\d+)", output)
        passed_match = re.search(r"Tests passed:\s*(\d+)", output)
        failed_match = re.search(r"Tests failed:\s*(\d+)", output)
        
        if run_match:
            stats["tests_run"] = int(run_match.group(1))
        if passed_match:
            stats["tests_passed"] = int(passed_match.group(1))
        if failed_match:
            stats["tests_failed"] = int(failed_match.group(1))
            
        if stats["tests_run"] > 0:
            stats["pass_rate"] = (stats["tests_passed"] / stats["tests_run"]) * 100
            
        # Extract individual test results
        test_lines = re.findall(r"Running test: ([^.]+)\.\.\..*?(PASS|FAIL)", output)
        for test_name, result in test_lines:
            stats["details"].append({
                "name": test_name.strip(),
                "status": result,
                "passed": result == "PASS"
            })
            
        return stats
    
    def run_test_suite(self, test_name: str, executable: str) -> Dict:
        """Run a test suite and collect results"""
        print(f"Running {test_name}...")
        
        if not os.path.exists(os.path.join(self.test_dir, executable)):
            return {
                "status": "SKIPPED",
                "reason": "Executable not found",
                "stats": {}
            }
            
        exit_code, stdout, stderr = self.run_command([f"./{executable}"])
        
        stats = self.parse_test_output(stdout)
        
        return {
            "status": "PASSED" if exit_code == 0 else "FAILED",
            "exit_code": exit_code,
            "stats": stats,
            "stdout": stdout,
            "stderr": stderr
        }
    
    def run_all_tests(self):
        """Run all test suites"""
        test_suites = [
            ("Core API Tests", "test_core"),
            ("Headless Rendering Tests", "test_rendering"),
            ("CLI Interface Tests", "test_cli"),
            ("File Format Tests", "test_formats"),
            ("Memory & Performance Tests", "test_memory_perf"),
            ("End-to-End Integration Tests", "test_e2e_integration")
        ]
        
        print("Building test executables...")
        build_exit, build_out, build_err = self.run_command(["make", "clean"])
        build_exit, build_out, build_err = self.run_command(["make", "all"])
        
        if build_exit != 0:
            print(f"Build failed: {build_err}")
            self.report_data["build_status"] = "FAILED"
            self.report_data["build_error"] = build_err
            return
        else:
            self.report_data["build_status"] = "SUCCESS"
        
        for suite_name, executable in test_suites:
            result = self.run_test_suite(suite_name, executable)
            self.report_data["test_suites"][suite_name] = result
    
    def run_performance_analysis(self):
        """Run performance benchmarks"""
        print("Running performance analysis...")
        
        if os.path.exists(os.path.join(self.test_dir, "test_memory_perf")):
            exit_code, stdout, stderr = self.run_command(["./test_memory_perf"])
            
            # Parse performance metrics
            perf_data = {
                "memory_usage": {},
                "timing": {},
                "raw_output": stdout
            }
            
            # Extract timing information
            timing_matches = re.findall(r"(\d+(?:\.\d+)?)\s*ms", stdout)
            if timing_matches:
                perf_data["timing"]["average_ms"] = sum(float(t) for t in timing_matches) / len(timing_matches)
                perf_data["timing"]["max_ms"] = max(float(t) for t in timing_matches)
                perf_data["timing"]["min_ms"] = min(float(t) for t in timing_matches)
            
            # Extract memory information
            memory_matches = re.findall(r"(\d+(?:\.\d+)?)\s*(?:KB|MB|bytes)", stdout)
            if memory_matches:
                perf_data["memory_usage"]["samples"] = len(memory_matches)
            
            self.report_data["performance"] = perf_data
    
    def run_coverage_analysis(self):
        """Run code coverage analysis if gcov available"""
        print("Running coverage analysis...")
        
        # Check if gcov is available
        exit_code, _, _ = self.run_command(["which", "gcov"])
        if exit_code != 0:
            self.report_data["coverage"] = {"status": "SKIPPED", "reason": "gcov not available"}
            return
        
        # Run coverage build and tests
        exit_code, stdout, stderr = self.run_command(["make", "coverage"])
        
        if exit_code == 0:
            # Parse gcov output for coverage statistics
            coverage_files = []
            for root, dirs, files in os.walk(self.test_dir):
                coverage_files.extend([f for f in files if f.endswith('.gcov')])
            
            total_lines = 0
            covered_lines = 0
            
            for gcov_file in coverage_files[:10]:  # Limit to first 10 files
                try:
                    with open(os.path.join(self.test_dir, gcov_file), 'r') as f:
                        for line in f:
                            if line.strip() and not line.startswith('##'):
                                total_lines += 1
                                if not line.strip().startswith('-:'):
                                    covered_lines += 1
                except Exception:
                    continue
            
            coverage_percent = (covered_lines / total_lines * 100) if total_lines > 0 else 0
            
            self.report_data["coverage"] = {
                "status": "SUCCESS",
                "total_lines": total_lines,
                "covered_lines": covered_lines,
                "coverage_percent": coverage_percent,
                "files_analyzed": len(coverage_files)
            }
        else:
            self.report_data["coverage"] = {"status": "FAILED", "error": stderr}
    
    def check_compliance(self):
        """Check compliance with Phase 6 requirements"""
        print("Checking compliance requirements...")
        
        requirements = {
            "build_success": False,
            "test_coverage_90_percent": False,
            "performance_under_100ms": False,
            "memory_under_500mb": False,
            "all_tests_pass": False
        }
        
        # Check build success
        requirements["build_success"] = self.report_data.get("build_status") == "SUCCESS"
        
        # Check test coverage
        coverage = self.report_data.get("coverage", {})
        if coverage.get("coverage_percent", 0) >= 90:
            requirements["test_coverage_90_percent"] = True
        
        # Check performance
        perf = self.report_data.get("performance", {})
        timing = perf.get("timing", {})
        if timing.get("average_ms", 1000) < 100:
            requirements["performance_under_100ms"] = True
        
        # Check if all tests pass
        all_pass = True
        for suite_name, suite_data in self.report_data.get("test_suites", {}).items():
            if suite_data.get("status") != "PASSED":
                all_pass = False
                break
        requirements["all_tests_pass"] = all_pass
        
        self.report_data["compliance"] = {
            "requirements": requirements,
            "compliance_score": sum(requirements.values()) / len(requirements) * 100,
            "ready_for_production": all(requirements.values())
        }
    
    def generate_summary(self):
        """Generate test summary statistics"""
        total_suites = len(self.report_data["test_suites"])
        passed_suites = sum(1 for s in self.report_data["test_suites"].values() 
                           if s.get("status") == "PASSED")
        failed_suites = sum(1 for s in self.report_data["test_suites"].values() 
                           if s.get("status") == "FAILED")
        skipped_suites = sum(1 for s in self.report_data["test_suites"].values() 
                            if s.get("status") == "SKIPPED")
        
        total_tests = sum(s.get("stats", {}).get("tests_run", 0) 
                         for s in self.report_data["test_suites"].values())
        passed_tests = sum(s.get("stats", {}).get("tests_passed", 0) 
                          for s in self.report_data["test_suites"].values())
        
        self.report_data["summary"] = {
            "total_test_suites": total_suites,
            "passed_suites": passed_suites,
            "failed_suites": failed_suites,
            "skipped_suites": skipped_suites,
            "total_individual_tests": total_tests,
            "passed_individual_tests": passed_tests,
            "overall_pass_rate": (passed_tests / total_tests * 100) if total_tests > 0 else 0,
            "suite_pass_rate": (passed_suites / total_suites * 100) if total_suites > 0 else 0
        }
    
    def generate_html_report(self) -> str:
        """Generate HTML test report"""
        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Goxel v13 Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .header {{ background: #f0f0f0; padding: 20px; border-radius: 5px; }}
        .summary {{ display: flex; gap: 20px; margin: 20px 0; }}
        .metric {{ background: #e8f4f8; padding: 15px; border-radius: 5px; flex: 1; text-align: center; }}
        .pass {{ background: #d4edda; }}
        .fail {{ background: #f8d7da; }}
        .skip {{ background: #fff3cd; }}
        .suite {{ margin: 20px 0; border: 1px solid #ddd; border-radius: 5px; }}
        .suite-header {{ background: #f8f9fa; padding: 10px; font-weight: bold; }}
        .suite-body {{ padding: 15px; }}
        .test-detail {{ margin: 5px 0; padding: 5px; border-left: 3px solid #ccc; }}
        .compliance {{ margin: 20px 0; }}
        .requirement {{ padding: 5px; margin: 2px 0; }}
        pre {{ background: #f8f9fa; padding: 10px; border-radius: 3px; overflow-x: auto; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>Goxel v13 Headless API - Test Report</h1>
        <p><strong>Generated:</strong> {self.report_data['timestamp']}</p>
        <p><strong>Phase:</strong> 6 - Production Ready</p>
    </div>
    
    <div class="summary">
        <div class="metric {'pass' if self.report_data['summary']['suite_pass_rate'] == 100 else 'fail'}">
            <h3>Test Suites</h3>
            <div style="font-size: 24px;">{self.report_data['summary']['passed_suites']}/{self.report_data['summary']['total_test_suites']}</div>
            <div>{self.report_data['summary']['suite_pass_rate']:.1f}% Pass Rate</div>
        </div>
        <div class="metric {'pass' if self.report_data['summary']['overall_pass_rate'] >= 90 else 'fail'}">
            <h3>Individual Tests</h3>
            <div style="font-size: 24px;">{self.report_data['summary']['passed_individual_tests']}/{self.report_data['summary']['total_individual_tests']}</div>
            <div>{self.report_data['summary']['overall_pass_rate']:.1f}% Pass Rate</div>
        </div>
        <div class="metric {'pass' if self.report_data['compliance']['ready_for_production'] else 'fail'}">
            <h3>Production Ready</h3>
            <div style="font-size: 24px;">{'✅' if self.report_data['compliance']['ready_for_production'] else '❌'}</div>
            <div>{self.report_data['compliance']['compliance_score']:.1f}% Compliant</div>
        </div>
    </div>
    
    <div class="compliance">
        <h2>Compliance Check</h2>
        <div class="suite-body">
        """
        
        for req, status in self.report_data["compliance"]["requirements"].items():
            status_icon = "✅" if status else "❌"
            css_class = "pass" if status else "fail"
            html += f'<div class="requirement {css_class}">{status_icon} {req.replace("_", " ").title()}</div>'
        
        html += """
        </div>
    </div>
    
    <h2>Test Suite Results</h2>
    """
        
        for suite_name, suite_data in self.report_data["test_suites"].items():
            status = suite_data.get("status", "UNKNOWN")
            css_class = {"PASSED": "pass", "FAILED": "fail", "SKIPPED": "skip"}.get(status, "")
            stats = suite_data.get("stats", {})
            
            html += f"""
    <div class="suite">
        <div class="suite-header {css_class}">{suite_name} - {status}</div>
        <div class="suite-body">
            <p><strong>Tests Run:</strong> {stats.get('tests_run', 0)}</p>
            <p><strong>Tests Passed:</strong> {stats.get('tests_passed', 0)}</p>
            <p><strong>Pass Rate:</strong> {stats.get('pass_rate', 0):.1f}%</p>
            """
            
            for test_detail in stats.get("details", []):
                test_css = "pass" if test_detail["passed"] else "fail"
                html += f'<div class="test-detail {test_css}">{test_detail["status"]} - {test_detail["name"]}</div>'
            
            html += "</div></div>"
        
        html += """
    </body>
</html>
        """
        
        return html
    
    def save_reports(self):
        """Save JSON and HTML reports"""
        # Save JSON report
        json_file = os.path.join(self.test_dir, "test_report.json")
        with open(json_file, 'w') as f:
            json.dump(self.report_data, f, indent=2)
        print(f"JSON report saved to: {json_file}")
        
        # Save HTML report
        html_content = self.generate_html_report()
        html_file = os.path.join(self.test_dir, "test_report.html")
        with open(html_file, 'w') as f:
            f.write(html_content)
        print(f"HTML report saved to: {html_file}")
        
        return json_file, html_file
    
    def run_full_analysis(self):
        """Run complete test analysis"""
        print("Starting Goxel v13 Test Analysis...")
        print("=" * 50)
        
        self.run_all_tests()
        self.run_performance_analysis()
        self.run_coverage_analysis()
        self.check_compliance()
        self.generate_summary()
        
        json_file, html_file = self.save_reports()
        
        print("\n" + "=" * 50)
        print("TEST ANALYSIS COMPLETE")
        print("=" * 50)
        print(f"Overall Pass Rate: {self.report_data['summary']['overall_pass_rate']:.1f}%")
        print(f"Production Ready: {'YES' if self.report_data['compliance']['ready_for_production'] else 'NO'}")
        print(f"Reports: {json_file}, {html_file}")
        
        return self.report_data["compliance"]["ready_for_production"]

def main():
    if len(sys.argv) > 1:
        test_dir = sys.argv[1]
    else:
        test_dir = "."
    
    generator = TestReportGenerator(test_dir)
    success = generator.run_full_analysis()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()