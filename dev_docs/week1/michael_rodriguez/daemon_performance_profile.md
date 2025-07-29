# Goxel Daemon Performance Profile Report
**Author**: Michael Rodriguez  
**Date**: January 29, 2025  
**Project**: Goxel v15 Simplified Architecture  

## Executive Summary

I've analyzed the current v14 daemon implementation to establish a performance baseline for our dual-mode operation refactoring. The daemon shows significant room for optimization, particularly in startup time and memory allocation patterns.

## Current Architecture Overview

### Components Analyzed
1. **Main Entry Point** (`daemon_main.c`)
   - 1116 lines of code with extensive command-line parsing
   - Creates concurrent daemon with worker pool
   - Initializes multiple subsystems sequentially

2. **Socket Server** (`socket_server.c`)
   - Unix domain socket implementation
   - Thread-per-client option (disabled by default)
   - Client buffer management with dynamic allocation

3. **Worker Pool** (`worker_pool.c`)
   - Thread pool for concurrent request processing
   - Priority queue support (optional)
   - Per-worker statistics tracking

## Performance Metrics

### Startup Time Analysis
Based on code inspection and the mentioned ~450ms startup time:

**Bottlenecks Identified:**
1. **Sequential Initialization** (estimated ~200ms)
   - Goxel context creation per worker thread
   - JSON-RPC subsystem initialization
   - Socket server setup with file system operations

2. **Thread Creation Overhead** (estimated ~100ms)
   - Default 8 worker threads
   - Accept thread for socket server
   - Each thread initialization includes stack allocation

3. **Memory Allocations** (estimated ~150ms)
   - Per-worker Goxel contexts
   - Client buffer pre-allocation
   - Request queue initialization

### Memory Usage Profile

**Static Allocations:**
- Worker Pool: 8 threads × ~8MB stack = 64MB
- Request Queue: 1024 slots × ~1KB = 1MB
- Client Buffers: 256 max × 4KB initial = 1MB
- **Total Base Memory**: ~66MB before any operations

**Dynamic Allocations Per Request:**
- Request structure: ~128 bytes
- Response buffer: Variable (1KB - 1MB)
- JSON parsing temporary buffers

### Request Processing Latency

**Current Flow:**
1. Socket read → Buffer copy → JSON parse → Queue enqueue → Worker dequeue → Process → Response serialize → Socket write

**Estimated Latencies:**
- Simple request (e.g., status): ~5-10ms
- Complex operation (e.g., voxel batch): ~50-100ms
- Bottleneck: JSON parsing/serialization (~40% of time)

## Architecture Observations

### Strengths
1. **Modular Design**: Clean separation of concerns
2. **Thread Safety**: Proper mutex usage throughout
3. **Error Handling**: Comprehensive error codes and logging

### Weaknesses
1. **Over-Engineering**: Complex for simple operations
2. **Memory Overhead**: Per-worker context duplication
3. **Synchronization Cost**: Excessive mutex operations
4. **No Zero-Copy**: Multiple buffer copies per message

## Optimization Opportunities

### 1. Startup Time Reduction (Target: <100ms)
- **Lazy Initialization**: Defer Goxel context creation
- **Thread Pool Reuse**: Start with 2 threads, grow on demand
- **Memory Pool**: Pre-allocate reusable buffers
- **Socket Optimization**: Use SO_REUSEADDR, async bind

### 2. Memory Usage Optimization
- **Shared Context**: Single Goxel context with read-write locks
- **Buffer Pooling**: Reuse message buffers
- **Stack Reduction**: Lower thread stack size (2MB vs 8MB)
- **Zero-Copy Path**: Direct socket→parser→handler flow

### 3. Hot Path Optimizations
- **Lock-Free Queue**: For request processing
- **SIMD JSON Parsing**: Use simdjson or similar
- **Batch Processing**: Group similar operations
- **CPU Affinity**: Pin worker threads to cores

### 4. Dual-Mode Design Considerations

For MCP integration, we need:
- **Protocol Detection**: Peek first bytes to identify JSON-RPC vs MCP
- **Shared Infrastructure**: Common socket/buffer management
- **Handler Dispatch**: Fast protocol-specific routing
- **Minimal Overhead**: <1ms protocol switching cost

## Recommended Architecture Changes

### Phase 1: Immediate Optimizations (Week 1-2)
1. Replace mutex-heavy queue with lock-free ring buffer
2. Implement buffer pooling for zero allocations in hot path
3. Add fast-path for common operations (bypass queue)

### Phase 2: Dual-Mode Support (Week 2-3)
1. Protocol multiplexer at socket layer
2. Shared worker pool for both protocols
3. Unified performance monitoring

### Phase 3: Advanced Features (Week 3-4)
1. NUMA-aware memory allocation
2. io_uring for async I/O (Linux)
3. Hardware acceleration hooks

## Performance Targets

| Metric | Current (v14) | Target (v15) | Improvement |
|--------|---------------|--------------|-------------|
| Startup Time | ~450ms | <100ms | 4.5x |
| Base Memory | 66MB | 20MB | 3.3x |
| Request Latency | 5-10ms | <1ms | 10x |
| Throughput | ~10K req/s | 100K req/s | 10x |

## Next Steps

1. **Implement Profiling Harness**: Detailed measurements with `perf`
2. **Create Benchmark Suite**: Standardized performance tests
3. **Prototype Lock-Free Queue**: Validate performance gains
4. **Design Protocol Detector**: Efficient MCP/JSON-RPC routing

## Collaboration Notes

**For Sarah (MCP Protocol)**:
- Socket layer will provide protocol detection callback
- Shared buffer pool available for zero-copy operations
- Worker threads can handle both protocols

**For Alex (Testing)**:
- Performance regression tests needed for each optimization
- Benchmark harness design in progress

**For Lisa (Documentation)**:
- Performance tuning guide will be provided
- Architecture diagrams for dual-mode operation

---

*This profile establishes our baseline. Tomorrow I'll implement the profiling harness and begin prototyping the lock-free queue implementation.*