# Goxel v14.0 Performance Testing Framework

## Overview

The Goxel v14.0 Performance Testing Framework provides comprehensive benchmarking and analysis capabilities for the daemon architecture. This framework measures critical performance metrics, detects regressions, and validates that the daemon meets its performance targets.

## Performance Targets

### Primary Targets
- **Average Latency**: < 2.1ms per request
- **Throughput**: > 1000 voxel operations/second
- **Memory Usage**: < 50MB daemon memory usage
- **Concurrent Clients**: Support 10+ simultaneous connections
- **Performance Improvement**: > 700% vs v13.4 CLI mode

### Secondary Targets
- **95th Percentile Latency**: < 5ms
- **Memory Leak Detection**: Zero memory leaks
- **Connection Stability**: > 99% uptime under load
- **Resource Efficiency**: < 10% CPU usage at idle

## Framework Components

### Core Testing Modules

#### 1. Latency Benchmark (`latency_benchmark.c`)
Measures request-response latency for various daemon operations.

**Features:**
- Multi-scenario latency testing
- Percentile analysis (P50, P95, P99)
- Warmup phase to eliminate cold-start effects
- Socket connection overhead measurement

**Test Scenarios:**
- Ping operations (target: 0.5ms)
- Status queries (target: 1.0ms)
- Project creation (target: 2.0ms)
- Voxel operations (target: 1.5ms)
- Mesh export (target: 5.0ms)

**Usage:**
```bash
cd tests/performance
./latency_benchmark [num_samples]
```

#### 2. Throughput Test (`throughput_test.c`)
Measures operations per second under various load conditions.

**Features:**
- Multi-threaded concurrent testing
- Sustained throughput measurement
- Per-thread performance breakdown
- Different operation types

**Test Scenarios:**
- Add voxel operations (target: 1500 ops/sec)
- Get voxel queries (target: 2000 ops/sec)
- Remove voxel operations (target: 1800 ops/sec)
- Batch operations (target: 800 ops/sec)

**Usage:**
```bash
cd tests/performance
./throughput_test [duration_seconds]
```

#### 3. Memory Profiling (`memory_profiling.c`)
Analyzes memory usage patterns and detects memory leaks.

**Features:**
- Real-time memory monitoring
- Memory leak detection
- Peak usage tracking
- Cross-platform memory analysis (Linux, macOS)

**Metrics Tracked:**
- Resident Set Size (RSS)
- Virtual Memory Size (VMS)
- Memory growth patterns
- Baseline vs peak usage

**Usage:**
```bash
cd tests/performance
./memory_profiling [duration_seconds]
```

#### 4. Stress Test (`stress_test.c`)
Tests daemon behavior under extreme load conditions.

**Features:**
- High concurrency scenarios
- Resource exhaustion testing
- Connection stability testing
- Extended duration stress testing

**Test Scenarios:**
- Concurrent basic operations (10+ clients)
- Heavy operation stress (5 clients)
- Rapid-fire operations (single client, 5000 ops)
- Connection storm (20 clients, short connections)

**Usage:**
```bash
cd tests/performance
./stress_test [duration_seconds]
```

#### 5. Comparison Framework (`comparison_framework.c`)
Compares v14.0 daemon performance against v13.4 CLI mode.

**Features:**
- Side-by-side performance comparison
- Improvement ratio calculation
- Batch operation analysis
- Startup overhead comparison

**Comparison Tests:**
- Project creation
- Single voxel operations
- Voxel queries
- Project export
- Batch processing

**Usage:**
```bash
cd tests/performance
./comparison_framework
```

### Automation and Reporting

#### 6. Automated Benchmark Script (`scripts/run_benchmarks.sh`)
Comprehensive test execution and orchestration.

**Features:**
- Multiple execution modes (quick, full, custom)
- CI integration support
- Automatic test compilation
- Environment validation
- Comprehensive reporting

**Usage:**
```bash
# Quick 5-minute benchmark
./scripts/run_benchmarks.sh --quick

# Full 30-minute benchmark suite
./scripts/run_benchmarks.sh --full

# CI mode with machine-readable output
./scripts/run_benchmarks.sh --ci --output ./ci_results

# Comparison tests only
./scripts/run_benchmarks.sh --comparison

# Stress tests with custom duration
./scripts/run_benchmarks.sh --stress
```

#### 7. Performance Reporter (`tools/performance_reporter.py`)
Advanced analysis and reporting tool.

**Features:**
- HTML report generation with charts
- JSON export for CI integration
- CSV data export for analysis
- Regression detection
- Trend analysis

**Usage:**
```bash
# Generate HTML report
./tools/performance_reporter.py benchmark_results --html report.html

# Export JSON for CI
./tools/performance_reporter.py benchmark_results --json results.json

# Check for regressions
./tools/performance_reporter.py benchmark_results --check-regressions --baseline baseline.json

# Export CSV data
./tools/performance_reporter.py benchmark_results --csv metrics.csv
```

## Quick Start Guide

### 1. Environment Setup

Ensure you have the necessary dependencies:
```bash
# Install build tools
sudo apt-get install build-essential  # Linux
# or
brew install gcc  # macOS

# Optional: Install Python dependencies for enhanced reporting
pip3 install matplotlib pandas
```

### 2. Build Performance Tests

```bash
cd tests/performance
make all
```

### 3. Start Daemon (if testing daemon mode)

```bash
# Start daemon in background
./goxel-daemon --socket /tmp/goxel_daemon_test.sock &
```

### 4. Run Quick Benchmark

```bash
# Run 5-minute quick benchmark
./scripts/run_benchmarks.sh --quick
```

### 5. Generate Report

```bash
# Generate HTML report
./tools/performance_reporter.py benchmark_results --html performance_report.html
```

## Advanced Usage

### Custom Test Scenarios

You can create custom test scenarios by modifying the test arrays in each C file:

```c
// Example: Add custom latency test scenario
static const test_scenario_t custom_scenarios[] = {
    {"custom_operation", "{\"method\":\"custom_op\"}", 25, 3.0},
    // ... other scenarios
};
```

### Automated CI Integration

The framework supports automated CI integration:

```yaml
# Example GitHub Actions workflow
- name: Run Performance Tests
  run: |
    ./scripts/run_benchmarks.sh --ci --output ci_results
    ./tools/performance_reporter.py ci_results --json performance.json
    
- name: Check Performance Regressions
  run: |
    ./tools/performance_reporter.py ci_results --check-regressions --baseline baseline.json
```

### Memory Profiling with Valgrind

For detailed memory analysis:

```bash
# Run daemon under Valgrind
valgrind --tool=memcheck --leak-check=full ./goxel-daemon &

# Run memory profiling tests
./tests/performance/memory_profiling 30
```

## Interpreting Results

### Latency Results
- **Average < 2.1ms**: ✅ Target achieved
- **P95 < 5ms**: ✅ Good responsiveness
- **P99 > 10ms**: ⚠️ May indicate outliers

### Throughput Results
- **> 1000 ops/sec**: ✅ Target achieved
- **Sustained performance**: Look for consistency across test duration
- **Per-thread efficiency**: Should scale linearly with thread count

### Memory Results
- **Peak < 50MB**: ✅ Target achieved
- **No memory leaks**: ✅ Stable long-term operation
- **Memory efficiency grade**: EXCELLENT > GOOD > ACCEPTABLE > POOR

### Stress Test Results
- **All scenarios pass**: ✅ Robust under load
- **95%+ success rate**: ✅ Reliable operation
- **Low connection failures**: ✅ Stable networking

## Troubleshooting

### Common Issues

#### 1. Connection Failures
```
Error: Cannot connect to daemon socket
```
**Solution:** Ensure daemon is running and socket path is correct.

#### 2. Compilation Errors
```
Error: pthread library not found
```
**Solution:** Install pthread development libraries:
```bash
sudo apt-get install libpthread-stubs0-dev  # Linux
```

#### 3. Low Performance
```
Warning: Throughput below target
```
**Solutions:**
- Check system load
- Verify daemon configuration
- Check for resource constraints

#### 4. Memory Analysis Issues
```
Warning: Could not collect memory statistics
```
**Solutions:**
- Run with appropriate permissions
- Check platform-specific implementation
- Use alternative profiling tools

### Performance Tuning Tips

1. **Optimize Socket Configuration**
   - Increase socket buffer sizes
   - Use TCP_NODELAY for low latency
   - Configure appropriate timeouts

2. **Memory Management**
   - Monitor for memory fragmentation
   - Use memory pools for frequent allocations
   - Implement proper cleanup routines

3. **Threading Optimization**
   - Balance thread count with CPU cores
   - Minimize lock contention
   - Use lock-free data structures where possible

4. **I/O Optimization**
   - Use asynchronous I/O where appropriate
   - Batch operations when possible
   - Optimize serialization/deserialization

## Framework Extension

### Adding New Tests

1. **Create Test Module**
   ```c
   // tests/performance/new_test.c
   #include <stdio.h>
   // ... implement test logic
   ```

2. **Update Makefile**
   ```makefile
   TESTS = latency_benchmark throughput_test ... new_test
   
   new_test: new_test.c
       $(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
   ```

3. **Update Benchmark Script**
   ```bash
   # Add to run_benchmarks.sh
   run_new_test() {
       log_test "Running new test..."
       # ... implementation
   }
   ```

### Custom Metrics

Add custom metrics to the performance reporter:

```python
# In performance_reporter.py
def _parse_custom_results(self):
    """Parse custom test results."""
    # ... implementation
```

## Best Practices

### Test Design
1. **Establish Baselines**: Always run baseline tests before making changes
2. **Use Representative Workloads**: Test scenarios should match real usage patterns
3. **Account for Variance**: Run multiple iterations and use statistical analysis
4. **Isolate Variables**: Test one change at a time when possible

### Monitoring
1. **Continuous Monitoring**: Integrate tests into CI/CD pipeline
2. **Alert on Regressions**: Set up automated alerts for performance degradation
3. **Track Trends**: Monitor performance trends over time
4. **Document Changes**: Link performance changes to code changes

### Analysis
1. **Statistical Significance**: Ensure sufficient sample sizes
2. **Percentile Analysis**: Don't rely solely on averages
3. **Resource Correlation**: Correlate performance with resource usage
4. **Comparative Analysis**: Always compare against previous versions

## Conclusion

The Goxel v14.0 Performance Testing Framework provides comprehensive tools for measuring, analyzing, and validating daemon performance. By following this guide and using the provided tools, you can ensure that the daemon meets its performance targets and maintains high performance over time.

For questions or issues with the performance framework, please refer to the project documentation or submit an issue in the project repository.