#!/usr/bin/env python3
"""
Goxel v14.0 Performance Benchmark Report Generator

This tool generates detailed comparison reports from benchmark results,
specifically comparing v13.4 CLI performance against v14.0 daemon performance.

Features:
- Loads and analyzes benchmark result files
- Calculates performance improvement ratios
- Identifies performance regressions
- Generates visual charts and graphs
- Exports reports in multiple formats
"""

import json
import sys
import argparse
import datetime
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
import statistics

# Try to import plotting libraries (optional)
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not installed. Charts will not be generated.")
    print("Install with: pip install matplotlib")

# ============================================================================
# DATA STRUCTURES
# ============================================================================

class BenchmarkResult:
    """Represents a single benchmark result."""
    def __init__(self, data: Dict[str, Any]):
        self.name = data.get("name", "Unknown")
        self.success = data.get("success", False)
        self.metrics = data.get("metrics", {})
        self.duration = data.get("duration", 0)
        self.timestamp = data.get("start_time", "")
        self.error = data.get("error", None)

class PerformanceComparison:
    """Compares performance between CLI and daemon modes."""
    def __init__(self, cli_result: BenchmarkResult, daemon_result: BenchmarkResult):
        self.cli = cli_result
        self.daemon = daemon_result
        self.improvement_ratio = None
        self.regression = False
        
        # Calculate improvement ratio if both have metrics
        if self.cli.metrics and self.daemon.metrics:
            # For latency, lower is better
            if "avg_latency_ms" in self.cli.metrics and "avg_latency_ms" in self.daemon.metrics:
                cli_latency = self.cli.metrics["avg_latency_ms"]
                daemon_latency = self.daemon.metrics["avg_latency_ms"]
                if daemon_latency > 0:
                    self.improvement_ratio = cli_latency / daemon_latency
                    self.regression = daemon_latency > cli_latency
            
            # For throughput, higher is better
            elif "throughput_ops_sec" in self.cli.metrics and "throughput_ops_sec" in self.daemon.metrics:
                cli_throughput = self.cli.metrics["throughput_ops_sec"]
                daemon_throughput = self.daemon.metrics["throughput_ops_sec"]
                if cli_throughput > 0:
                    self.improvement_ratio = daemon_throughput / cli_throughput
                    self.regression = daemon_throughput < cli_throughput

# ============================================================================
# REPORT GENERATION
# ============================================================================

def load_benchmark_results(file_path: Path) -> List[BenchmarkResult]:
    """Load benchmark results from a JSON file."""
    try:
        with open(file_path, 'r') as f:
            data = json.load(f)
        
        results = []
        if "results" in data:
            for result_data in data["results"]:
                results.append(BenchmarkResult(result_data))
        
        return results
    except Exception as e:
        print(f"Error loading {file_path}: {e}")
        return []

def find_matching_results(cli_results: List[BenchmarkResult], 
                         daemon_results: List[BenchmarkResult]) -> List[PerformanceComparison]:
    """Match CLI and daemon results by test name."""
    comparisons = []
    
    # Create lookup dictionary for daemon results
    daemon_by_name = {r.name: r for r in daemon_results}
    
    for cli_result in cli_results:
        if cli_result.name in daemon_by_name:
            daemon_result = daemon_by_name[cli_result.name]
            comparisons.append(PerformanceComparison(cli_result, daemon_result))
    
    return comparisons

def calculate_summary_statistics(comparisons: List[PerformanceComparison]) -> Dict[str, Any]:
    """Calculate summary statistics from comparisons."""
    improvement_ratios = [c.improvement_ratio for c in comparisons 
                         if c.improvement_ratio is not None]
    
    if not improvement_ratios:
        return {
            "avg_improvement": 0,
            "min_improvement": 0,
            "max_improvement": 0,
            "median_improvement": 0,
            "regression_count": 0,
            "improvement_count": 0
        }
    
    return {
        "avg_improvement": statistics.mean(improvement_ratios),
        "min_improvement": min(improvement_ratios),
        "max_improvement": max(improvement_ratios),
        "median_improvement": statistics.median(improvement_ratios),
        "regression_count": sum(1 for c in comparisons if c.regression),
        "improvement_count": sum(1 for c in comparisons if c.improvement_ratio and c.improvement_ratio > 1)
    }

def generate_text_report(comparisons: List[PerformanceComparison], 
                        summary: Dict[str, Any],
                        output_file: Optional[Path] = None):
    """Generate a text-based comparison report."""
    report_lines = []
    
    # Header
    report_lines.append("="*80)
    report_lines.append("Goxel v14.0 Performance Comparison Report")
    report_lines.append("="*80)
    report_lines.append(f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    report_lines.append("")
    
    # Summary
    report_lines.append("EXECUTIVE SUMMARY")
    report_lines.append("-"*40)
    report_lines.append(f"Average Improvement: {summary['avg_improvement']:.1f}x")
    report_lines.append(f"Median Improvement: {summary['median_improvement']:.1f}x")
    report_lines.append(f"Best Improvement: {summary['max_improvement']:.1f}x")
    report_lines.append(f"Worst Improvement: {summary['min_improvement']:.1f}x")
    report_lines.append(f"Tests with Improvement: {summary['improvement_count']}")
    report_lines.append(f"Tests with Regression: {summary['regression_count']}")
    report_lines.append("")
    
    # Target evaluation
    target_improvement = 7.0  # 700%
    if summary['avg_improvement'] >= target_improvement:
        report_lines.append(f"✅ TARGET MET: {summary['avg_improvement']:.1f}x >= {target_improvement}x")
    else:
        report_lines.append(f"❌ TARGET MISSED: {summary['avg_improvement']:.1f}x < {target_improvement}x")
    report_lines.append("")
    
    # Detailed results
    report_lines.append("DETAILED COMPARISON")
    report_lines.append("-"*40)
    
    for comp in comparisons:
        report_lines.append(f"\nTest: {comp.cli.name}")
        
        if not comp.cli.success or not comp.daemon.success:
            if not comp.cli.success:
                report_lines.append("  CLI: FAILED")
            if not comp.daemon.success:
                report_lines.append("  Daemon: FAILED")
            continue
        
        # Latency comparison
        if "avg_latency_ms" in comp.cli.metrics and "avg_latency_ms" in comp.daemon.metrics:
            cli_lat = comp.cli.metrics["avg_latency_ms"]
            daemon_lat = comp.daemon.metrics["avg_latency_ms"]
            report_lines.append(f"  Latency:")
            report_lines.append(f"    CLI:    {cli_lat:.3f} ms")
            report_lines.append(f"    Daemon: {daemon_lat:.3f} ms")
            if comp.improvement_ratio:
                if comp.regression:
                    report_lines.append(f"    Result: ❌ {comp.improvement_ratio:.1f}x SLOWER")
                else:
                    report_lines.append(f"    Result: ✅ {comp.improvement_ratio:.1f}x FASTER")
        
        # Throughput comparison
        if "throughput_ops_sec" in comp.cli.metrics and "throughput_ops_sec" in comp.daemon.metrics:
            cli_thr = comp.cli.metrics["throughput_ops_sec"]
            daemon_thr = comp.daemon.metrics["throughput_ops_sec"]
            report_lines.append(f"  Throughput:")
            report_lines.append(f"    CLI:    {cli_thr:.1f} ops/sec")
            report_lines.append(f"    Daemon: {daemon_thr:.1f} ops/sec")
            if comp.improvement_ratio:
                if comp.regression:
                    report_lines.append(f"    Result: ❌ {comp.improvement_ratio:.1f}x WORSE")
                else:
                    report_lines.append(f"    Result: ✅ {comp.improvement_ratio:.1f}x BETTER")
        
        # Memory comparison
        if "memory_peak_mb" in comp.cli.metrics and "memory_peak_mb" in comp.daemon.metrics:
            cli_mem = comp.cli.metrics["memory_peak_mb"]
            daemon_mem = comp.daemon.metrics["memory_peak_mb"]
            report_lines.append(f"  Memory Usage:")
            report_lines.append(f"    CLI:    {cli_mem:.1f} MB")
            report_lines.append(f"    Daemon: {daemon_mem:.1f} MB")
            if daemon_mem < cli_mem:
                report_lines.append(f"    Result: ✅ {cli_mem - daemon_mem:.1f} MB saved")
            else:
                report_lines.append(f"    Result: ❌ {daemon_mem - cli_mem:.1f} MB more")
    
    report_lines.append("")
    report_lines.append("="*80)
    
    # Output
    report_text = "\n".join(report_lines)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(report_text)
        print(f"Text report saved to: {output_file}")
    else:
        print(report_text)
    
    return report_text

def generate_charts(comparisons: List[PerformanceComparison], output_dir: Path):
    """Generate visual charts from comparison data."""
    if not HAS_MATPLOTLIB:
        return
    
    # Prepare data
    test_names = []
    improvement_ratios = []
    colors = []
    
    for comp in comparisons:
        if comp.improvement_ratio is not None:
            test_names.append(comp.cli.name)
            improvement_ratios.append(comp.improvement_ratio)
            colors.append('red' if comp.regression else 'green')
    
    if not improvement_ratios:
        print("No data available for charts")
        return
    
    # Create improvement ratio bar chart
    plt.figure(figsize=(12, 8))
    bars = plt.bar(range(len(test_names)), improvement_ratios, color=colors)
    
    # Add target line
    plt.axhline(y=7.0, color='blue', linestyle='--', label='Target (7x)')
    plt.axhline(y=1.0, color='black', linestyle='-', alpha=0.3, label='Baseline')
    
    # Customize chart
    plt.xlabel('Test Name')
    plt.ylabel('Improvement Ratio')
    plt.title('Goxel v14.0 Performance Improvement vs v13.4 CLI')
    plt.xticks(range(len(test_names)), test_names, rotation=45, ha='right')
    
    # Add value labels on bars
    for i, (bar, ratio) in enumerate(zip(bars, improvement_ratios)):
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height,
                f'{ratio:.1f}x', ha='center', va='bottom')
    
    # Add legend
    green_patch = mpatches.Patch(color='green', label='Improvement')
    red_patch = mpatches.Patch(color='red', label='Regression')
    plt.legend(handles=[green_patch, red_patch])
    
    plt.tight_layout()
    chart_file = output_dir / "performance_comparison_chart.png"
    plt.savefig(chart_file, dpi=150)
    plt.close()
    
    print(f"Chart saved to: {chart_file}")
    
    # Create latency comparison chart
    cli_latencies = []
    daemon_latencies = []
    latency_names = []
    
    for comp in comparisons:
        if ("avg_latency_ms" in comp.cli.metrics and 
            "avg_latency_ms" in comp.daemon.metrics):
            latency_names.append(comp.cli.name)
            cli_latencies.append(comp.cli.metrics["avg_latency_ms"])
            daemon_latencies.append(comp.daemon.metrics["avg_latency_ms"])
    
    if latency_names:
        plt.figure(figsize=(12, 8))
        x = range(len(latency_names))
        width = 0.35
        
        plt.bar([i - width/2 for i in x], cli_latencies, width, label='v13.4 CLI', color='orange')
        plt.bar([i + width/2 for i in x], daemon_latencies, width, label='v14.0 Daemon', color='blue')
        
        # Add target line
        plt.axhline(y=2.1, color='green', linestyle='--', label='Target (<2.1ms)')
        
        plt.xlabel('Test Name')
        plt.ylabel('Latency (ms)')
        plt.title('Latency Comparison: v13.4 CLI vs v14.0 Daemon')
        plt.xticks(x, latency_names, rotation=45, ha='right')
        plt.legend()
        
        plt.tight_layout()
        latency_chart_file = output_dir / "latency_comparison_chart.png"
        plt.savefig(latency_chart_file, dpi=150)
        plt.close()
        
        print(f"Latency chart saved to: {latency_chart_file}")

def generate_csv_report(comparisons: List[PerformanceComparison], output_file: Path):
    """Generate CSV format report for further analysis."""
    import csv
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Header
        writer.writerow([
            "Test Name",
            "CLI Success",
            "Daemon Success",
            "CLI Latency (ms)",
            "Daemon Latency (ms)",
            "CLI Throughput (ops/sec)",
            "Daemon Throughput (ops/sec)",
            "CLI Memory (MB)",
            "Daemon Memory (MB)",
            "Improvement Ratio",
            "Regression"
        ])
        
        # Data rows
        for comp in comparisons:
            writer.writerow([
                comp.cli.name,
                comp.cli.success,
                comp.daemon.success,
                comp.cli.metrics.get("avg_latency_ms", ""),
                comp.daemon.metrics.get("avg_latency_ms", ""),
                comp.cli.metrics.get("throughput_ops_sec", ""),
                comp.daemon.metrics.get("throughput_ops_sec", ""),
                comp.cli.metrics.get("memory_peak_mb", ""),
                comp.daemon.metrics.get("memory_peak_mb", ""),
                comp.improvement_ratio if comp.improvement_ratio else "",
                comp.regression
            ])
    
    print(f"CSV report saved to: {output_file}")

# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate performance comparison reports for Goxel v14.0"
    )
    parser.add_argument(
        "cli_results",
        type=Path,
        help="Path to v13.4 CLI benchmark results JSON"
    )
    parser.add_argument(
        "daemon_results",
        type=Path,
        help="Path to v14.0 daemon benchmark results JSON"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path.cwd(),
        help="Output directory for reports"
    )
    parser.add_argument(
        "--format",
        choices=["text", "csv", "charts", "all"],
        default="all",
        help="Output format(s) to generate"
    )
    parser.add_argument(
        "--no-charts",
        action="store_true",
        help="Skip chart generation even if matplotlib is available"
    )
    
    args = parser.parse_args()
    
    # Validate input files
    if not args.cli_results.exists():
        print(f"Error: CLI results file not found: {args.cli_results}")
        return 1
    
    if not args.daemon_results.exists():
        print(f"Error: Daemon results file not found: {args.daemon_results}")
        return 1
    
    # Create output directory
    args.output_dir.mkdir(parents=True, exist_ok=True)
    
    # Load results
    print("Loading benchmark results...")
    cli_results = load_benchmark_results(args.cli_results)
    daemon_results = load_benchmark_results(args.daemon_results)
    
    if not cli_results:
        print("Error: No CLI results found")
        return 1
    
    if not daemon_results:
        print("Error: No daemon results found")
        return 1
    
    print(f"Loaded {len(cli_results)} CLI results")
    print(f"Loaded {len(daemon_results)} daemon results")
    
    # Match results
    comparisons = find_matching_results(cli_results, daemon_results)
    print(f"Found {len(comparisons)} matching test pairs")
    
    if not comparisons:
        print("Error: No matching tests found between CLI and daemon results")
        return 1
    
    # Calculate summary
    summary = calculate_summary_statistics(comparisons)
    
    # Generate reports
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    
    if args.format in ["text", "all"]:
        text_file = args.output_dir / f"comparison_report_{timestamp}.txt"
        generate_text_report(comparisons, summary, text_file)
    
    if args.format in ["csv", "all"]:
        csv_file = args.output_dir / f"comparison_data_{timestamp}.csv"
        generate_csv_report(comparisons, csv_file)
    
    if args.format in ["charts", "all"] and not args.no_charts:
        generate_charts(comparisons, args.output_dir)
    
    # Print summary
    print("\n" + "="*60)
    print("PERFORMANCE COMPARISON SUMMARY")
    print("="*60)
    print(f"Average Improvement: {summary['avg_improvement']:.1f}x")
    print(f"Target (700%): {'✅ MET' if summary['avg_improvement'] >= 7.0 else '❌ NOT MET'}")
    print(f"Tests with regression: {summary['regression_count']}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())