#!/usr/bin/env python3
"""
Goxel v14.0 Daemon Architecture - Performance Reporter and Analysis Tool

This module provides comprehensive analysis and reporting of benchmark results
including trend analysis, regression detection, and visual reporting capabilities.

Features:
- Parse and analyze benchmark results
- Generate HTML reports with charts
- Detect performance regressions
- Compare results across runs
- Export data for CI integration
"""

import os
import sys
import json
import csv
import argparse
import datetime
import statistics
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import re

# Optional dependencies for enhanced reporting
try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

try:
    import pandas as pd
    HAS_PANDAS = True
except ImportError:
    HAS_PANDAS = False

class BenchmarkResult:
    """Represents a single benchmark result."""
    
    def __init__(self, name: str, timestamp: datetime.datetime):
        self.name = name
        self.timestamp = timestamp
        self.metrics = {}
        self.status = "unknown"
        self.duration_ms = 0.0
        self.notes = []
    
    def add_metric(self, key: str, value: float, unit: str = ""):
        """Add a performance metric."""
        self.metrics[key] = {
            'value': value,
            'unit': unit
        }
    
    def add_note(self, note: str):
        """Add a note or observation."""
        self.notes.append(note)
    
    def to_dict(self):
        """Convert to dictionary for JSON serialization."""
        return {
            'name': self.name,
            'timestamp': self.timestamp.isoformat(),
            'metrics': self.metrics,
            'status': self.status,
            'duration_ms': self.duration_ms,
            'notes': self.notes
        }

class PerformanceReporter:
    """Main performance analysis and reporting class."""
    
    def __init__(self, results_dir: Path):
        self.results_dir = Path(results_dir)
        self.benchmark_results = []
        self.targets = {
            'latency_avg_ms': 2.1,
            'throughput_ops_sec': 1000,
            'memory_peak_mb': 50,
            'concurrent_clients': 10,
            'improvement_ratio': 7.0
        }
    
    def parse_benchmark_files(self):
        """Parse all benchmark result files in the directory."""
        if not self.results_dir.exists():
            print(f"Results directory {self.results_dir} does not exist")
            return
        
        # Parse different types of result files
        self._parse_latency_results()
        self._parse_throughput_results()
        self._parse_memory_results()
        self._parse_stress_results()
        self._parse_comparison_results()
        
        print(f"Parsed {len(self.benchmark_results)} benchmark results")
    
    def _parse_latency_results(self):
        """Parse latency benchmark results."""
        latency_file = self.results_dir / "latency_results.txt"
        if not latency_file.exists():
            return
        
        result = BenchmarkResult("latency_benchmark", datetime.datetime.now())
        
        with open(latency_file, 'r') as f:
            content = f.read()
        
        # Extract average latency
        avg_match = re.search(r'Overall Average Latency: ([\d.]+)ms', content)
        if avg_match:
            result.add_metric('avg_latency', float(avg_match.group(1)), 'ms')
        
        # Extract success rate
        success_match = re.search(r'Tests Passed: (\d+)/(\d+)', content)
        if success_match:
            passed, total = int(success_match.group(1)), int(success_match.group(2))
            result.add_metric('success_rate', (passed / total) * 100, '%')
            result.status = "pass" if passed == total else "partial"
        
        # Check if target is achieved
        target_match = re.search(r'Target Achievement: (\w+)', content)
        if target_match and target_match.group(1) == "ACHIEVED":
            result.add_note("Latency target achieved")
        
        self.benchmark_results.append(result)
    
    def _parse_throughput_results(self):
        """Parse throughput benchmark results."""
        throughput_file = self.results_dir / "throughput_results.txt"
        if not throughput_file.exists():
            return
        
        result = BenchmarkResult("throughput_benchmark", datetime.datetime.now())
        
        with open(throughput_file, 'r') as f:
            content = f.read()
        
        # Extract throughput values
        throughput_matches = re.findall(r'Throughput: ([\d.]+) ops/sec', content)
        if throughput_matches:
            throughputs = [float(t) for t in throughput_matches]
            result.add_metric('avg_throughput', statistics.mean(throughputs), 'ops/sec')
            result.add_metric('max_throughput', max(throughputs), 'ops/sec')
            result.add_metric('min_throughput', min(throughputs), 'ops/sec')
        
        # Extract overall grade
        grade_match = re.search(r'Overall Grade: (\w+)', content)
        if grade_match:
            grade = grade_match.group(1)
            result.status = "pass" if grade == "EXCELLENT" else "partial"
            result.add_note(f"Throughput grade: {grade}")
        
        self.benchmark_results.append(result)
    
    def _parse_memory_results(self):
        """Parse memory profiling results."""
        memory_file = self.results_dir / "memory_results.txt"
        if not memory_file.exists():
            return
        
        result = BenchmarkResult("memory_profiling", datetime.datetime.now())
        
        with open(memory_file, 'r') as f:
            content = f.read()
        
        # Extract memory metrics
        peak_match = re.search(r'Peak RSS: ([\d.]+) MB', content)
        if peak_match:
            result.add_metric('peak_memory', float(peak_match.group(1)), 'MB')
        
        final_match = re.search(r'Final RSS: ([\d.]+) MB', content)
        if final_match:
            result.add_metric('final_memory', float(final_match.group(1)), 'MB')
        
        # Check memory efficiency
        efficiency_match = re.search(r'Memory Efficiency: (\w+)', content)
        if efficiency_match:
            efficiency = efficiency_match.group(1)
            result.add_note(f"Memory efficiency: {efficiency}")
            result.status = "pass" if efficiency in ["EXCELLENT", "GOOD"] else "partial"
        
        # Check for memory leaks
        leak_match = re.search(r'Memory Leaks: (\w+)', content)
        if leak_match and leak_match.group(1) == "NONE":
            result.add_note("No memory leaks detected")
        elif leak_match:
            result.add_note("Memory leaks detected")
        
        self.benchmark_results.append(result)
    
    def _parse_stress_results(self):
        """Parse stress test results."""
        stress_file = self.results_dir / "stress_results.txt"
        if not stress_file.exists():
            return
        
        result = BenchmarkResult("stress_test", datetime.datetime.now())
        
        with open(stress_file, 'r') as f:
            content = f.read()
        
        # Extract concurrent client handling
        scenarios_match = re.search(r'Scenarios Passed: (\d+)/(\d+)', content)
        if scenarios_match:
            passed, total = int(scenarios_match.group(1)), int(scenarios_match.group(2))
            result.add_metric('scenarios_passed', passed, 'count')
            result.add_metric('scenarios_total', total, 'count')
            result.status = "pass" if passed >= total * 0.8 else "fail"
        
        # Extract grade
        grade_match = re.search(r'Overall Grade: (\w+)', content)
        if grade_match:
            grade = grade_match.group(1)
            result.add_note(f"Stress test grade: {grade}")
        
        self.benchmark_results.append(result)
    
    def _parse_comparison_results(self):
        """Parse performance comparison results."""
        comparison_file = self.results_dir / "comparison_results.txt"
        if not comparison_file.exists():
            return
        
        result = BenchmarkResult("performance_comparison", datetime.datetime.now())
        
        with open(comparison_file, 'r') as f:
            content = f.read()
        
        # Extract improvement ratio
        improvement_match = re.search(r'Average Improvement: ([\d.]+)x', content)
        if improvement_match:
            improvement = float(improvement_match.group(1))
            result.add_metric('improvement_ratio', improvement, 'x')
            result.status = "pass" if improvement >= self.targets['improvement_ratio'] else "partial"
        
        # Extract best and worst improvements
        best_match = re.search(r'Best Improvement: ([\d.]+)x', content)
        if best_match:
            result.add_metric('best_improvement', float(best_match.group(1)), 'x')
        
        worst_match = re.search(r'Worst Improvement: ([\d.]+)x', content)
        if worst_match:
            result.add_metric('worst_improvement', float(worst_match.group(1)), 'x')
        
        # Extract grade
        grade_match = re.search(r'Overall Grade: (\w+)', content)
        if grade_match:
            grade = grade_match.group(1)
            result.add_note(f"Comparison grade: {grade}")
        
        self.benchmark_results.append(result)
    
    def analyze_performance_trends(self, historical_data: List[Dict]) -> Dict:
        """Analyze performance trends over time."""
        if not historical_data:
            return {"trend": "no_data", "message": "No historical data available"}
        
        trends = {}
        
        # Analyze each metric type
        metric_types = ['avg_latency', 'avg_throughput', 'peak_memory', 'improvement_ratio']
        
        for metric in metric_types:
            values = []
            timestamps = []
            
            for data in historical_data:
                for result in data.get('results', []):
                    if metric in result.get('metrics', {}):
                        values.append(result['metrics'][metric]['value'])
                        timestamps.append(datetime.datetime.fromisoformat(result['timestamp']))
            
            if len(values) >= 2:
                # Simple trend analysis
                recent_avg = statistics.mean(values[-3:]) if len(values) >= 3 else values[-1]
                older_avg = statistics.mean(values[:3]) if len(values) >= 3 else values[0]
                
                if metric in ['avg_latency', 'peak_memory']:
                    # Lower is better
                    trend = "improving" if recent_avg < older_avg else "degrading"
                else:
                    # Higher is better
                    trend = "improving" if recent_avg > older_avg else "degrading"
                
                trends[metric] = {
                    'trend': trend,
                    'recent_avg': recent_avg,
                    'older_avg': older_avg,
                    'change_percent': abs((recent_avg - older_avg) / older_avg * 100)
                }
        
        return trends
    
    def detect_regressions(self, baseline: Optional[Dict] = None) -> List[Dict]:
        """Detect performance regressions compared to baseline."""
        regressions = []
        
        if not baseline:
            return regressions
        
        for result in self.benchmark_results:
            for metric_name, metric_data in result.metrics.items():
                baseline_value = None
                
                # Find baseline value for this metric
                for baseline_result in baseline.get('results', []):
                    if baseline_result['name'] == result.name and metric_name in baseline_result.get('metrics', {}):
                        baseline_value = baseline_result['metrics'][metric_name]['value']
                        break
                
                if baseline_value is None:
                    continue
                
                current_value = metric_data['value']
                threshold = 0.1  # 10% regression threshold
                
                # Check for regression based on metric type
                if metric_name in ['avg_latency', 'peak_memory']:
                    # Lower is better - regression if current > baseline
                    if (current_value - baseline_value) / baseline_value > threshold:
                        regressions.append({
                            'test': result.name,
                            'metric': metric_name,
                            'baseline': baseline_value,
                            'current': current_value,
                            'regression_percent': (current_value - baseline_value) / baseline_value * 100,
                            'unit': metric_data['unit']
                        })
                else:
                    # Higher is better - regression if current < baseline
                    if (baseline_value - current_value) / baseline_value > threshold:
                        regressions.append({
                            'test': result.name,
                            'metric': metric_name,
                            'baseline': baseline_value,
                            'current': current_value,
                            'regression_percent': (baseline_value - current_value) / baseline_value * 100,
                            'unit': metric_data['unit']
                        })
        
        return regressions
    
    def generate_html_report(self, output_file: Path):
        """Generate comprehensive HTML performance report."""
        html_content = self._create_html_template()
        
        # Generate charts if matplotlib is available
        charts = []
        if HAS_MATPLOTLIB:
            charts = self._generate_charts()
        
        # Replace placeholders in template
        html_content = html_content.replace('{{TIMESTAMP}}', datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
        html_content = html_content.replace('{{RESULTS_TABLE}}', self._generate_results_table())
        html_content = html_content.replace('{{SUMMARY_STATS}}', self._generate_summary_stats())
        html_content = html_content.replace('{{CHARTS}}', self._generate_charts_html(charts))
        
        with open(output_file, 'w') as f:
            f.write(html_content)
        
        print(f"HTML report generated: {output_file}")
    
    def _create_html_template(self) -> str:
        """Create HTML template for the report."""
        return """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Goxel v14.0 Performance Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }
        h2 { color: #34495e; margin-top: 30px; }
        .timestamp { color: #7f8c8d; font-size: 14px; }
        .summary { background: #ecf0f1; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .metric-card { display: inline-block; background: white; border: 1px solid #bdc3c7; border-radius: 5px; padding: 15px; margin: 10px; min-width: 200px; }
        .metric-value { font-size: 24px; font-weight: bold; color: #2980b9; }
        .metric-label { color: #7f8c8d; font-size: 14px; }
        .status-pass { color: #27ae60; font-weight: bold; }
        .status-fail { color: #e74c3c; font-weight: bold; }
        .status-partial { color: #f39c12; font-weight: bold; }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th, td { border: 1px solid #bdc3c7; padding: 12px; text-align: left; }
        th { background-color: #34495e; color: white; }
        tr:nth-child(even) { background-color: #f8f9fa; }
        .chart-container { margin: 20px 0; text-align: center; }
        .notes { background: #fff3cd; border: 1px solid #ffeaa7; border-radius: 5px; padding: 10px; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Goxel v14.0 Daemon Performance Report</h1>
        <div class="timestamp">Generated: {{TIMESTAMP}}</div>
        
        <div class="summary">
            {{SUMMARY_STATS}}
        </div>
        
        <h2>Performance Metrics</h2>
        {{RESULTS_TABLE}}
        
        <h2>Performance Charts</h2>
        {{CHARTS}}
        
        <div class="notes">
            <h3>Performance Targets</h3>
            <ul>
                <li>Average Latency: &lt; 2.1ms</li>
                <li>Throughput: &gt; 1000 ops/sec</li>
                <li>Memory Usage: &lt; 50MB</li>
                <li>Concurrent Clients: 10+</li>
                <li>Performance Improvement: &gt; 7.0x vs CLI</li>
            </ul>
        </div>
    </div>
</body>
</html>
"""
    
    def _generate_results_table(self) -> str:
        """Generate HTML table of benchmark results."""
        html = "<table>"
        html += "<tr><th>Test</th><th>Status</th><th>Key Metrics</th><th>Notes</th></tr>"
        
        for result in self.benchmark_results:
            # Format status
            status_class = f"status-{result.status}"
            status_text = result.status.upper()
            
            # Format key metrics
            metrics_html = ""
            for name, data in result.metrics.items():
                metrics_html += f"<div><strong>{name}:</strong> {data['value']:.2f} {data['unit']}</div>"
            
            # Format notes
            notes_html = "<br>".join(result.notes)
            
            html += f"""
            <tr>
                <td>{result.name}</td>
                <td><span class="{status_class}">{status_text}</span></td>
                <td>{metrics_html}</td>
                <td>{notes_html}</td>
            </tr>
            """
        
        html += "</table>"
        return html
    
    def _generate_summary_stats(self) -> str:
        """Generate summary statistics HTML."""
        total_tests = len(self.benchmark_results)
        passed_tests = sum(1 for r in self.benchmark_results if r.status == "pass")
        
        html = f"""
        <div class="metric-card">
            <div class="metric-value">{total_tests}</div>
            <div class="metric-label">Total Tests</div>
        </div>
        <div class="metric-card">
            <div class="metric-value">{passed_tests}</div>
            <div class="metric-label">Tests Passed</div>
        </div>
        <div class="metric-card">
            <div class="metric-value">{(passed_tests/total_tests*100):.1f}%</div>
            <div class="metric-label">Success Rate</div>
        </div>
        """
        
        # Add key performance metrics
        for result in self.benchmark_results:
            if result.name == "latency_benchmark" and 'avg_latency' in result.metrics:
                value = result.metrics['avg_latency']['value']
                status = "✓" if value < self.targets['latency_avg_ms'] else "✗"
                html += f"""
                <div class="metric-card">
                    <div class="metric-value">{value:.2f}ms {status}</div>
                    <div class="metric-label">Average Latency</div>
                </div>
                """
            
            elif result.name == "throughput_benchmark" and 'avg_throughput' in result.metrics:
                value = result.metrics['avg_throughput']['value']
                status = "✓" if value > self.targets['throughput_ops_sec'] else "✗"
                html += f"""
                <div class="metric-card">
                    <div class="metric-value">{value:.0f} ops/s {status}</div>
                    <div class="metric-label">Throughput</div>
                </div>
                """
        
        return html
    
    def _generate_charts(self) -> List[str]:
        """Generate performance charts using matplotlib."""
        if not HAS_MATPLOTLIB:
            return []
        
        charts = []
        
        # Create charts directory
        charts_dir = self.results_dir / "charts"
        charts_dir.mkdir(exist_ok=True)
        
        # Generate latency chart
        latency_chart = self._create_latency_chart(charts_dir)
        if latency_chart:
            charts.append(latency_chart)
        
        return charts
    
    def _create_latency_chart(self, charts_dir: Path) -> Optional[str]:
        """Create latency performance chart."""
        # This would create actual matplotlib charts
        # For now, return placeholder
        return None
    
    def _generate_charts_html(self, charts: List[str]) -> str:
        """Generate HTML for charts section."""
        if not charts:
            return "<p>Chart generation requires matplotlib. Install with: pip install matplotlib</p>"
        
        html = ""
        for chart in charts:
            html += f'<div class="chart-container"><img src="{chart}" alt="Performance Chart"></div>'
        
        return html
    
    def export_json(self, output_file: Path):
        """Export results as JSON for CI integration."""
        data = {
            'timestamp': datetime.datetime.now().isoformat(),
            'results': [result.to_dict() for result in self.benchmark_results],
            'summary': {
                'total_tests': len(self.benchmark_results),
                'passed_tests': sum(1 for r in self.benchmark_results if r.status == "pass"),
                'targets': self.targets
            }
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"JSON report exported: {output_file}")
    
    def export_csv(self, output_file: Path):
        """Export metrics as CSV for analysis."""
        rows = []
        
        for result in self.benchmark_results:
            base_row = {
                'test_name': result.name,
                'timestamp': result.timestamp.isoformat(),
                'status': result.status
            }
            
            for metric_name, metric_data in result.metrics.items():
                row = base_row.copy()
                row['metric_name'] = metric_name
                row['metric_value'] = metric_data['value']
                row['metric_unit'] = metric_data['unit']
                rows.append(row)
        
        if rows:
            with open(output_file, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            
            print(f"CSV data exported: {output_file}")

def main():
    parser = argparse.ArgumentParser(
        description="Goxel v14.0 Performance Reporter and Analysis Tool"
    )
    parser.add_argument(
        "results_dir",
        help="Directory containing benchmark results"
    )
    parser.add_argument(
        "--html", "-H",
        help="Generate HTML report"
    )
    parser.add_argument(
        "--json", "-j",
        help="Export JSON report"
    )
    parser.add_argument(
        "--csv", "-c",
        help="Export CSV data"
    )
    parser.add_argument(
        "--baseline",
        help="Baseline results file for regression detection"
    )
    parser.add_argument(
        "--check-regressions",
        action="store_true",
        help="Check for performance regressions"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    if not os.path.exists(args.results_dir):
        print(f"Error: Results directory {args.results_dir} does not exist")
        sys.exit(1)
    
    # Create reporter
    reporter = PerformanceReporter(Path(args.results_dir))
    
    # Parse benchmark files
    reporter.parse_benchmark_files()
    
    if not reporter.benchmark_results:
        print("No benchmark results found to analyze")
        sys.exit(1)
    
    # Generate reports
    if args.html:
        reporter.generate_html_report(Path(args.html))
    
    if args.json:
        reporter.export_json(Path(args.json))
    
    if args.csv:
        reporter.export_csv(Path(args.csv))
    
    # Check for regressions
    if args.check_regressions and args.baseline:
        with open(args.baseline, 'r') as f:
            baseline_data = json.load(f)
        
        regressions = reporter.detect_regressions(baseline_data)
        
        if regressions:
            print(f"\n⚠️  {len(regressions)} performance regressions detected:")
            for reg in regressions:
                print(f"  {reg['test']}.{reg['metric']}: "
                      f"{reg['baseline']:.2f} → {reg['current']:.2f} {reg['unit']} "
                      f"({reg['regression_percent']:.1f}% regression)")
            sys.exit(1)
        else:
            print("✅ No performance regressions detected")
    
    # Print summary
    total = len(reporter.benchmark_results)
    passed = sum(1 for r in reporter.benchmark_results if r.status == "pass")
    
    print(f"\nPerformance Analysis Summary:")
    print(f"  Total Tests: {total}")
    print(f"  Passed: {passed}")
    print(f"  Success Rate: {passed/total*100:.1f}%")
    
    if args.verbose:
        print("\nDetailed Results:")
        for result in reporter.benchmark_results:
            print(f"  {result.name}: {result.status.upper()}")
            for name, data in result.metrics.items():
                print(f"    {name}: {data['value']:.2f} {data['unit']}")

if __name__ == "__main__":
    main()