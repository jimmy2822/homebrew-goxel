#!/usr/bin/env python3
"""
Benchmark Analysis and Visualization Tool
Author: Alex Kumar
Date: January 29, 2025

Analyzes benchmark results and generates performance reports
for the Goxel architecture simplification project.
"""

import json
import sys
import os
import argparse
from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Performance targets
TARGET_4LAYER_LATENCY = 11.0  # ms
TARGET_2LAYER_LATENCY = 6.0   # ms
TARGET_IMPROVEMENT = 1.83     # 83% improvement

class BenchmarkAnalyzer:
    def __init__(self, results_file):
        self.results_file = results_file
        self.data = None
        self.load_results()
    
    def load_results(self):
        """Load benchmark results from JSON file"""
        try:
            with open(self.results_file, 'r') as f:
                self.data = json.load(f)
        except FileNotFoundError:
            print(f"Error: Results file '{self.results_file}' not found")
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON: {e}")
            sys.exit(1)
    
    def get_benchmarks_by_architecture(self, architecture):
        """Filter benchmarks by architecture type"""
        return [b for b in self.data['benchmarks'] 
                if b['architecture'] == architecture]
    
    def calculate_improvement(self):
        """Calculate performance improvement between architectures"""
        four_layer = self.get_benchmarks_by_architecture('4-layer')
        two_layer = self.get_benchmarks_by_architecture('2-layer')
        
        improvements = {}
        
        for b4 in four_layer:
            # Find matching 2-layer benchmark
            b2_match = next((b2 for b2 in two_layer 
                           if b2['name'].replace('2Layer', '4Layer') == b4['name']), None)
            
            if b2_match:
                latency_improvement = b4['results']['latency']['avg'] / b2_match['results']['latency']['avg']
                throughput_improvement = b2_match['results']['throughput'] / b4['results']['throughput']
                
                improvements[b4['name'].replace('_4Layer', '')] = {
                    'latency_improvement': latency_improvement,
                    'throughput_improvement': throughput_improvement,
                    '4layer_latency': b4['results']['latency']['avg'],
                    '2layer_latency': b2_match['results']['latency']['avg'],
                    '4layer_throughput': b4['results']['throughput'],
                    '2layer_throughput': b2_match['results']['throughput']
                }
        
        return improvements
    
    def generate_latency_comparison_chart(self):
        """Generate latency comparison bar chart"""
        improvements = self.calculate_improvement()
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        # Latency comparison
        benchmarks = list(improvements.keys())
        x = np.arange(len(benchmarks))
        width = 0.35
        
        latencies_4layer = [improvements[b]['4layer_latency'] for b in benchmarks]
        latencies_2layer = [improvements[b]['2layer_latency'] for b in benchmarks]
        
        bars1 = ax1.bar(x - width/2, latencies_4layer, width, label='4-Layer', color='#ff7f0e')
        bars2 = ax1.bar(x + width/2, latencies_2layer, width, label='2-Layer', color='#2ca02c')
        
        # Add target lines
        ax1.axhline(y=TARGET_4LAYER_LATENCY, color='#ff7f0e', linestyle='--', alpha=0.5, label='4-Layer Target')
        ax1.axhline(y=TARGET_2LAYER_LATENCY, color='#2ca02c', linestyle='--', alpha=0.5, label='2-Layer Target')
        
        ax1.set_xlabel('Benchmark')
        ax1.set_ylabel('Latency (ms)')
        ax1.set_title('Latency Comparison: 4-Layer vs 2-Layer Architecture')
        ax1.set_xticks(x)
        ax1.set_xticklabels([b.replace('_', ' ') for b in benchmarks], rotation=45)
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Add value labels on bars
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax1.annotate(f'{height:.1f}',
                           xy=(bar.get_x() + bar.get_width() / 2, height),
                           xytext=(0, 3),
                           textcoords="offset points",
                           ha='center', va='bottom', fontsize=8)
        
        # Throughput comparison
        throughput_4layer = [improvements[b]['4layer_throughput'] for b in benchmarks]
        throughput_2layer = [improvements[b]['2layer_throughput'] for b in benchmarks]
        
        bars3 = ax2.bar(x - width/2, throughput_4layer, width, label='4-Layer', color='#ff7f0e')
        bars4 = ax2.bar(x + width/2, throughput_2layer, width, label='2-Layer', color='#2ca02c')
        
        ax2.set_xlabel('Benchmark')
        ax2.set_ylabel('Throughput (ops/sec)')
        ax2.set_title('Throughput Comparison: 4-Layer vs 2-Layer Architecture')
        ax2.set_xticks(x)
        ax2.set_xticklabels([b.replace('_', ' ') for b in benchmarks], rotation=45)
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # Add value labels
        for bars in [bars3, bars4]:
            for bar in bars:
                height = bar.get_height()
                ax2.annotate(f'{height:.0f}',
                           xy=(bar.get_x() + bar.get_width() / 2, height),
                           xytext=(0, 3),
                           textcoords="offset points",
                           ha='center', va='bottom', fontsize=8)
        
        plt.tight_layout()
        plt.savefig('latency_throughput_comparison.png', dpi=150)
        plt.close()
    
    def generate_layer_breakdown_chart(self):
        """Generate layer breakdown pie chart for 4-layer architecture"""
        four_layer = self.get_benchmarks_by_architecture('4-layer')
        
        for benchmark in four_layer:
            if 'layer_breakdown' in benchmark['results']:
                breakdown = benchmark['results']['layer_breakdown']
                
                labels = ['MCP→Server', 'Server→TS', 'TS→Daemon', 'Daemon Processing']
                sizes = [
                    breakdown['mcp_to_server'],
                    breakdown['server_to_ts'],
                    breakdown['ts_to_daemon'],
                    breakdown['daemon_processing']
                ]
                colors = ['#ff9999', '#66b3ff', '#99ff99', '#ffcc99']
                
                fig, ax = plt.subplots(figsize=(8, 8))
                wedges, texts, autotexts = ax.pie(sizes, labels=labels, colors=colors,
                                                  autopct='%1.1f%%', startangle=90)
                
                # Add actual ms values to labels
                for i, (wedge, text, autotext) in enumerate(zip(wedges, texts, autotexts)):
                    text.set_text(f'{labels[i]}\n({sizes[i]:.2f}ms)')
                
                ax.set_title(f'Layer Latency Breakdown - {benchmark["name"]}')
                plt.savefig(f'layer_breakdown_{benchmark["name"]}.png', dpi=150)
                plt.close()
    
    def generate_percentile_chart(self):
        """Generate percentile comparison chart"""
        fig, ax = plt.subplots(figsize=(10, 6))
        
        percentiles = ['p50', 'p90', 'p95', 'p99']
        
        for benchmark in self.data['benchmarks']:
            if 'Single_Operation' in benchmark['name']:
                latencies = benchmark['results']['latency']
                values = [latencies[p] for p in percentiles]
                
                style = '-' if '4Layer' in benchmark['name'] else '--'
                color = '#ff7f0e' if '4Layer' in benchmark['name'] else '#2ca02c'
                label = benchmark['architecture']
                
                ax.plot(['P50', 'P90', 'P95', 'P99'], values, 
                       marker='o', linestyle=style, color=color, 
                       label=label, linewidth=2)
                
                # Add value labels
                for i, (p, v) in enumerate(zip(['P50', 'P90', 'P95', 'P99'], values)):
                    ax.annotate(f'{v:.1f}', xy=(i, v), xytext=(5, 5),
                              textcoords='offset points', fontsize=8)
        
        ax.set_xlabel('Percentile')
        ax.set_ylabel('Latency (ms)')
        ax.set_title('Latency Percentile Comparison')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig('percentile_comparison.png', dpi=150)
        plt.close()
    
    def generate_report(self):
        """Generate comprehensive performance report"""
        improvements = self.calculate_improvement()
        
        report = []
        report.append("# Goxel Performance Benchmark Report")
        report.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append(f"Results from: {self.results_file}")
        report.append("")
        
        report.append("## Executive Summary")
        
        # Calculate average improvement
        avg_latency_improvement = np.mean([imp['latency_improvement'] 
                                         for imp in improvements.values()])
        avg_throughput_improvement = np.mean([imp['throughput_improvement'] 
                                            for imp in improvements.values()])
        
        report.append(f"- **Average Latency Improvement**: {avg_latency_improvement:.2f}x "
                     f"({(avg_latency_improvement - 1) * 100:.1f}% faster)")
        report.append(f"- **Average Throughput Improvement**: {avg_throughput_improvement:.2f}x "
                     f"({(avg_throughput_improvement - 1) * 100:.1f}% higher)")
        report.append(f"- **Target Achievement**: {'✅ PASSED' if avg_latency_improvement >= TARGET_IMPROVEMENT else '❌ FAILED'}")
        report.append("")
        
        report.append("## Detailed Results")
        
        for test_name, imp in improvements.items():
            report.append(f"\n### {test_name.replace('_', ' ')}")
            report.append("| Metric | 4-Layer | 2-Layer | Improvement |")
            report.append("|--------|---------|---------|-------------|")
            report.append(f"| Latency | {imp['4layer_latency']:.2f}ms | "
                         f"{imp['2layer_latency']:.2f}ms | "
                         f"{imp['latency_improvement']:.2f}x |")
            report.append(f"| Throughput | {imp['4layer_throughput']:.0f} ops/s | "
                         f"{imp['2layer_throughput']:.0f} ops/s | "
                         f"{imp['throughput_improvement']:.2f}x |")
        
        report.append("\n## Performance Targets")
        report.append("| Architecture | Target Latency | Achieved | Status |")
        report.append("|--------------|----------------|----------|---------|")
        
        # Check targets
        single_op_4layer = next((b for b in self.data['benchmarks'] 
                               if b['name'] == 'Single_Operation_4Layer'), None)
        single_op_2layer = next((b for b in self.data['benchmarks'] 
                               if b['name'] == 'Single_Operation_2Layer'), None)
        
        if single_op_4layer:
            achieved = single_op_4layer['results']['latency']['avg']
            status = '✅' if achieved <= TARGET_4LAYER_LATENCY else '❌'
            report.append(f"| 4-Layer | {TARGET_4LAYER_LATENCY}ms | {achieved:.2f}ms | {status} |")
        
        if single_op_2layer:
            achieved = single_op_2layer['results']['latency']['avg']
            status = '✅' if achieved <= TARGET_2LAYER_LATENCY else '❌'
            report.append(f"| 2-Layer | {TARGET_2LAYER_LATENCY}ms | {achieved:.2f}ms | {status} |")
        
        report.append("\n## Recommendations")
        
        if avg_latency_improvement >= TARGET_IMPROVEMENT:
            report.append("- ✅ Performance targets achieved! Ready to proceed with 2-layer architecture.")
        else:
            report.append("- ⚠️ Performance targets not met. Further optimization needed.")
            report.append("- Consider profiling the MCP protocol handler for bottlenecks.")
            report.append("- Review daemon processing efficiency.")
        
        # Write report
        report_path = 'benchmark_report.md'
        with open(report_path, 'w') as f:
            f.write('\n'.join(report))
        
        print(f"Report generated: {report_path}")
        return report_path
    
    def check_regression(self, baseline_file):
        """Check for performance regression against baseline"""
        try:
            with open(baseline_file, 'r') as f:
                baseline = json.load(f)
        except FileNotFoundError:
            print(f"Warning: Baseline file '{baseline_file}' not found")
            return []
        
        regressions = []
        threshold = 0.05  # 5% regression threshold
        
        for current in self.data['benchmarks']:
            # Find matching baseline
            baseline_match = next((b for b in baseline['benchmarks'] 
                                 if b['name'] == current['name']), None)
            
            if baseline_match:
                current_latency = current['results']['latency']['avg']
                baseline_latency = baseline_match['results']['latency']['avg']
                
                regression_pct = (current_latency - baseline_latency) / baseline_latency
                
                if regression_pct > threshold:
                    regressions.append({
                        'benchmark': current['name'],
                        'baseline': baseline_latency,
                        'current': current_latency,
                        'regression': f"{regression_pct * 100:.1f}%"
                    })
        
        return regressions

def main():
    parser = argparse.ArgumentParser(description='Analyze Goxel benchmark results')
    parser.add_argument('results', help='Path to benchmark results JSON file')
    parser.add_argument('--baseline', help='Path to baseline results for regression detection')
    parser.add_argument('--charts', action='store_true', help='Generate visualization charts')
    
    args = parser.parse_args()
    
    analyzer = BenchmarkAnalyzer(args.results)
    
    # Generate report
    report_path = analyzer.generate_report()
    print(f"\n📊 Performance Report: {report_path}")
    
    # Generate charts if requested
    if args.charts:
        print("\n📈 Generating visualization charts...")
        analyzer.generate_latency_comparison_chart()
        analyzer.generate_layer_breakdown_chart()
        analyzer.generate_percentile_chart()
        print("Charts generated successfully!")
    
    # Check for regressions if baseline provided
    if args.baseline:
        print("\n🔍 Checking for performance regressions...")
        regressions = analyzer.check_regression(args.baseline)
        
        if regressions:
            print("⚠️ Performance regressions detected:")
            for reg in regressions:
                print(f"  - {reg['benchmark']}: {reg['baseline']:.2f}ms → "
                      f"{reg['current']:.2f}ms ({reg['regression']})")
        else:
            print("✅ No performance regressions detected!")
    
    # Print summary
    improvements = analyzer.calculate_improvement()
    avg_improvement = np.mean([imp['latency_improvement'] for imp in improvements.values()])
    
    print(f"\n🎯 Overall Performance Improvement: {avg_improvement:.2f}x "
          f"({(avg_improvement - 1) * 100:.1f}% faster)")

if __name__ == '__main__':
    main()