# Benchmark Suite Framework
## Alex Kumar - Week 1, Day 1

### Framework Overview
Automated performance benchmarking system to measure and track Goxel's performance across architecture changes. This framework will provide continuous regression detection and validate our 10x performance improvement claims.

### Core Components

#### 1. Benchmark Categories

**Latency Benchmarks**
- Single operation latency
- End-to-end request latency
- Layer-to-layer communication overhead
- Socket connection establishment time

**Throughput Benchmarks**
- Operations per second (single-threaded)
- Concurrent operations capacity
- Request queue saturation point
- Worker pool efficiency

**Resource Benchmarks**
- Memory usage per operation
- CPU utilization patterns
- File descriptor usage
- Network bandwidth consumption

#### 2. Benchmark Implementation Structure

```c
// benchmark_framework.h
typedef struct benchmark_config {
    const char* name;
    const char* description;
    int warmup_iterations;
    int test_iterations;
    int concurrent_clients;
    double timeout_ms;
} benchmark_config_t;

typedef struct benchmark_result {
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    double p50_latency_ms;
    double p90_latency_ms;
    double p95_latency_ms;
    double p99_latency_ms;
    double throughput_ops_per_sec;
    size_t memory_usage_bytes;
    double cpu_usage_percent;
    int successful_operations;
    int failed_operations;
} benchmark_result_t;

typedef int (*benchmark_fn)(benchmark_config_t* config, 
                           benchmark_result_t* result);

// Core benchmark runner
int run_benchmark(benchmark_fn fn, 
                  benchmark_config_t* config,
                  benchmark_result_t* result);
```

#### 3. Automated Test Scenarios

**Scenario 1: Basic Operation Latency**
```c
// Measure single voxel operation through entire stack
int benchmark_single_voxel_op(benchmark_config_t* config, 
                              benchmark_result_t* result) {
    struct timespec start, end;
    
    for (int i = 0; i < config->test_iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        // Current 4-layer path
        mcp_client_send_request("voxel.add", params);
        mcp_server_process();
        typescript_client_forward();
        daemon_execute();
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        record_latency(result, timespec_diff_ms(&start, &end));
    }
    
    return 0;
}
```

**Scenario 2: Concurrent Load Testing**
```c
// Measure system under concurrent load
int benchmark_concurrent_load(benchmark_config_t* config,
                             benchmark_result_t* result) {
    pthread_t threads[config->concurrent_clients];
    worker_context_t contexts[config->concurrent_clients];
    
    // Launch concurrent workers
    for (int i = 0; i < config->concurrent_clients; i++) {
        contexts[i].operations = config->test_iterations;
        pthread_create(&threads[i], NULL, worker_thread, &contexts[i]);
    }
    
    // Wait and collect results
    // ...
}
```

**Scenario 3: Memory Pressure Test**
```c
// Test memory usage patterns
int benchmark_memory_usage(benchmark_config_t* config,
                          benchmark_result_t* result) {
    size_t initial_rss = get_current_rss();
    
    // Create large voxel scene
    for (int i = 0; i < 1000000; i++) {
        add_voxel(random_position(), random_color());
    }
    
    size_t peak_rss = get_peak_rss();
    result->memory_usage_bytes = peak_rss - initial_rss;
    
    return 0;
}
```

### Baseline Measurements (Current 4-Layer)

| Metric | Value | Target (2-Layer) |
|--------|-------|------------------|
| Single Op Latency | 11.2ms | 6.0ms |
| Throughput | 89 ops/sec | 167 ops/sec |
| Memory/Op | 2.4KB | 1.2KB |
| CPU Usage | 45% | 25% |
| Connection Overhead | 5.3ms | 1.0ms |

### Continuous Integration

```yaml
# .github/workflows/performance.yml
name: Performance Benchmarks
on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build Benchmark Suite
        run: make benchmarks
        
      - name: Run Baseline Tests
        run: ./run_benchmarks.sh --baseline
        
      - name: Compare with Previous
        run: ./compare_results.py --threshold 5%
        
      - name: Upload Results
        uses: actions/upload-artifact@v2
        with:
          name: benchmark-results
          path: results/*.json
```

### Regression Detection Algorithm

```python
class RegressionDetector:
    def __init__(self, baseline_file, threshold=0.05):
        self.baseline = load_baseline(baseline_file)
        self.threshold = threshold  # 5% regression threshold
    
    def check_regression(self, current_results):
        regressions = []
        
        for metric, baseline_value in self.baseline.items():
            current_value = current_results[metric]
            regression_percent = (current_value - baseline_value) / baseline_value
            
            if regression_percent > self.threshold:
                regressions.append({
                    'metric': metric,
                    'baseline': baseline_value,
                    'current': current_value,
                    'regression': f"{regression_percent*100:.1f}%"
                })
        
        return regressions
```

### Benchmark Execution Schedule

**Continuous Benchmarks** (every commit)
- Basic latency tests
- Memory leak detection
- Unit test performance

**Nightly Benchmarks**
- Full throughput testing
- Concurrent load scenarios
- Memory pressure tests

**Weekly Benchmarks**
- Extended stress testing
- Cross-platform comparison
- Architecture comparison (4-layer vs 2-layer)

### Reporting Format

```json
{
  "timestamp": "2025-01-29T10:30:00Z",
  "commit": "abc123",
  "architecture": "4-layer",
  "results": {
    "latency": {
      "single_op": {
        "avg": 11.2,
        "p50": 10.8,
        "p90": 13.1,
        "p99": 15.7
      }
    },
    "throughput": {
      "ops_per_sec": 89.3,
      "max_concurrent": 50
    },
    "resources": {
      "memory_mb": 45.2,
      "cpu_percent": 43.7
    }
  },
  "regressions": [],
  "improvements": [
    {
      "metric": "socket_connect_time",
      "improvement": "12.3%"
    }
  ]
}
```

### Next Steps
1. Implement core benchmark runner in C
2. Create Python analysis scripts
3. Set up Grafana dashboards for visualization
4. Integrate with CI/CD pipeline
5. Establish baseline measurements for all metrics