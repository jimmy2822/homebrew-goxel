# Goxel v0.16 Performance Guide

## Optimizing Render Transfer Performance

This guide provides best practices and optimization techniques for the v0.16 file-path render transfer architecture.

---

## Performance Characteristics

### Memory Usage

| Operation | v0.15 (Base64) | v0.16 (File-Path) | Savings |
|-----------|----------------|-------------------|---------|
| 400×400 render | 640 KB | 64 KB | 90% |
| 1920×1080 render | 8.3 MB | 830 KB | 90% |
| 3840×2160 render | 33.2 MB | 3.3 MB | 90% |

### Transfer Speed

| Image Size | v0.15 | v0.16 | Improvement |
|------------|--------|--------|-------------|
| Small (100×100) | 5ms | 2ms | 60% faster |
| Medium (400×400) | 15ms | 7ms | 53% faster |
| Large (1920×1080) | 85ms | 40ms | 53% faster |
| 4K (3840×2160) | 350ms | 160ms | 54% faster |

---

## Optimization Strategies

### 1. Use File-Path Mode for Large Images

```python
def smart_render(daemon, width, height):
    # Use file-path mode for images larger than 500×500
    if width * height > 250000:
        return daemon.request('goxel.render_scene', {
            'width': width,
            'height': height,
            'options': {'return_mode': 'file_path'}
        })
    else:
        # Small images can use legacy mode
        return daemon.request('goxel.render_scene', 
                            [f'output_{width}x{height}.png', width, height])
```

### 2. Batch Rendering

Leverage concurrent render support:

```python
import concurrent.futures

def batch_render(daemon, sizes):
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        futures = []
        for width, height in sizes:
            future = executor.submit(daemon.request, 'goxel.render_scene', {
                'width': width,
                'height': height,
                'options': {'return_mode': 'file_path'}
            })
            futures.append(future)
        
        results = [f.result() for f in futures]
    return results
```

### 3. Cache Management

Monitor and manage cache usage:

```python
def monitor_cache(daemon):
    stats = daemon.request('goxel.get_render_stats', {})
    cache_usage = stats['result']['stats']['cache_usage_percent']
    
    if cache_usage > 0.8:  # 80% full
        # Clean up old renders
        renders = daemon.request('goxel.list_renders', {})
        old_renders = sorted(renders['result']['renders'], 
                           key=lambda r: r['created_at'])
        
        # Remove oldest 25%
        for render in old_renders[:len(old_renders)//4]:
            daemon.request('goxel.cleanup_render', 
                         {'path': render['path']})
```

### 4. TTL Optimization

Configure TTL based on usage patterns:

```bash
# Short-lived renders (web service)
export GOXEL_RENDER_TTL=300  # 5 minutes

# Long-lived renders (batch processing)
export GOXEL_RENDER_TTL=7200  # 2 hours

# Permanent renders (manual cleanup)
export GOXEL_RENDER_TTL=86400  # 24 hours
```

### 5. Directory Optimization

Use fast storage for render directory:

```bash
# Use SSD/NVMe for best performance
export GOXEL_RENDER_DIR=/fast/ssd/goxel_renders

# Use tmpfs for extreme performance (RAM-backed)
export GOXEL_RENDER_DIR=/dev/shm/goxel_renders

# Use local disk to avoid network latency
export GOXEL_RENDER_DIR=/var/local/goxel_renders
```

---

## Benchmarking

### Run Performance Tests

```bash
# Basic benchmark
python3 tests/performance/benchmark_render_transfer.py

# Stress test
for i in {1..100}; do
    python3 tests/test_render_transfer_v16.py &
done
wait
```

### Measure Performance

```python
import time
import statistics

def benchmark_render(daemon, width, height, iterations=100):
    times = []
    
    for _ in range(iterations):
        start = time.perf_counter()
        
        result = daemon.request('goxel.render_scene', {
            'width': width,
            'height': height,
            'options': {'return_mode': 'file_path'}
        })
        
        # Include file read time for fair comparison
        if result['result']['success']:
            with open(result['result']['file']['path'], 'rb') as f:
                data = f.read()
        
        elapsed = time.perf_counter() - start
        times.append(elapsed)
    
    return {
        'mean': statistics.mean(times),
        'median': statistics.median(times),
        'stdev': statistics.stdev(times),
        'min': min(times),
        'max': max(times)
    }
```

---

## Configuration Tuning

### Development Settings

```bash
# Fast iteration, short TTL
export GOXEL_RENDER_DIR=/tmp/goxel_dev
export GOXEL_RENDER_TTL=300
export GOXEL_RENDER_MAX_SIZE=104857600  # 100MB
export GOXEL_RENDER_CLEANUP_INTERVAL=60
```

### Production Settings

```bash
# High volume, optimized for throughput
export GOXEL_RENDER_DIR=/var/cache/goxel
export GOXEL_RENDER_TTL=3600
export GOXEL_RENDER_MAX_SIZE=10737418240  # 10GB
export GOXEL_RENDER_CLEANUP_INTERVAL=300
```

### CI/CD Settings

```bash
# Ephemeral, aggressive cleanup
export GOXEL_RENDER_DIR=${TMPDIR}/goxel_ci
export GOXEL_RENDER_TTL=60
export GOXEL_RENDER_MAX_SIZE=52428800  # 50MB
export GOXEL_RENDER_CLEANUP_INTERVAL=30
```

---

## Monitoring

### Key Metrics to Track

```python
def get_performance_metrics(daemon):
    stats = daemon.request('goxel.get_render_stats', {})['result']['stats']
    
    return {
        'active_renders': stats['active_renders'],
        'cache_size_mb': stats['current_cache_size'] / 1024 / 1024,
        'cache_usage_%': stats['cache_usage_percent'] * 100,
        'total_renders': stats['total_renders'],
        'cleanup_efficiency': stats['files_cleaned'] / max(1, stats['total_cleanups']),
        'avg_render_size': stats['current_cache_size'] / max(1, stats['active_renders'])
    }
```

### Performance Dashboard

```python
import time

def monitor_dashboard(daemon, interval=5):
    while True:
        metrics = get_performance_metrics(daemon)
        
        print("\033[2J\033[H")  # Clear screen
        print("=== Goxel Render Performance Dashboard ===")
        print(f"Active Renders:    {metrics['active_renders']:4d}")
        print(f"Cache Size:        {metrics['cache_size_mb']:6.1f} MB")
        print(f"Cache Usage:       {metrics['cache_usage_%']:5.1f}%")
        print(f"Total Renders:     {metrics['total_renders']:6d}")
        print(f"Cleanup Efficiency:{metrics['cleanup_efficiency']:5.1f} files/cycle")
        print(f"Avg Render Size:   {metrics['avg_render_size']/1024:.1f} KB")
        
        time.sleep(interval)
```

---

## Troubleshooting Performance Issues

### High Memory Usage

1. Check cache size: `goxel.get_render_stats`
2. Reduce TTL: `export GOXEL_RENDER_TTL=600`
3. Increase cleanup frequency: `export GOXEL_RENDER_CLEANUP_INTERVAL=60`
4. Manual cleanup: `goxel.cleanup_render`

### Slow Renders

1. Check disk I/O: `iostat -x 1`
2. Use faster storage (SSD/NVMe)
3. Reduce concurrent renders
4. Check CPU usage: `top`

### Cleanup Not Working

1. Verify cleanup thread: Check `cleanup_thread_active` in stats
2. Check file permissions: `ls -la $GOXEL_RENDER_DIR`
3. Review logs for errors
4. Restart daemon if necessary

---

## Best Practices Summary

1. **Always use file-path mode for images > 500×500 pixels**
2. **Configure TTL based on your use case**
3. **Monitor cache usage and cleanup regularly**
4. **Use fast local storage for render directory**
5. **Batch renders when possible**
6. **Set appropriate cache limits**
7. **Test performance with your specific workload**

---

*For more details, see the [Architecture Document](../v16-render-transfer-milestone.md)*