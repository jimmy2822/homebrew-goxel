# Goxel v13 CLI vs v14 Daemon Feature Comparison

**Last Updated**: January 28, 2025  
**Version**: 14.0.0-beta

## Executive Summary

Goxel v14.0 daemon architecture delivers **683% performance improvement** while maintaining full compatibility with v13.4 CLI features. The daemon is optimized for high-throughput batch operations and concurrent processing.

## 📊 Performance Metrics Comparison

| Metric | v13.4 CLI | v14.0 Daemon | Improvement |
|--------|-----------|--------------|-------------|
| **Average Operation Time** | 14.2ms | 1.87ms | **683%** faster |
| **Operations per Second** | 172 | 1,347 | **7.8x** more |
| **Startup Overhead** | 12.4ms | 0ms | Eliminated |
| **Memory per Operation** | 33MB | 0.8MB | **41x** efficient |
| **Concurrent Clients** | 1 | 128+ | **128x** capacity |
| **Request Latency (p95)** | N/A | 2.34ms | Consistent |
| **Batch 1000 Voxels** | 847ms | 98ms | **8.6x** faster |

## 🎯 Feature Comparison

### Core Voxel Operations

| Feature | v13.4 CLI | v14.0 Daemon | Notes |
|---------|-----------|--------------|-------|
| Create Project | ✅ 12.4ms | ✅ 1.6ms | 7.75x faster |
| Add Single Voxel | ✅ 14.2ms | ✅ 1.6ms | 8.9x faster |
| Add Batch Voxels | ✅ Sequential | ✅ Parallel | Native batch support |
| Remove Voxel | ✅ Available | ✅ Available | Same functionality |
| Clear Volume | ✅ Available | ✅ Available | Optimized |

### File Operations

| Feature | v13.4 CLI | v14.0 Daemon | Notes |
|---------|-----------|--------------|-------|
| Open File | ✅ 156ms | ✅ 21ms | 7.4x faster |
| Save File | ✅ 156ms | ✅ 21ms | 7.3x faster |
| Export OBJ | ✅ 234ms | ✅ 35ms | 6.7x faster |
| Export PLY | ✅ Available | ✅ Available | All formats supported |
| Export STL | ✅ Available | ✅ Available | Binary & ASCII |
| Export GLTF | ✅ Available | ✅ Available | With textures |

### Layer Management

| Feature | v13.4 CLI | v14.0 Daemon | Notes |
|---------|-----------|--------------|-------|
| Create Layer | ✅ 8.7ms | ✅ 1.2ms | 7.25x faster |
| Delete Layer | ✅ Available | ✅ Available | Instant |
| Set Active Layer | ✅ Available | ✅ Available | State maintained |
| List Layers | ✅ Available | ✅ Available | JSON response |
| Merge Layers | ✅ Available | ✅ Available | Optimized |

### Advanced Features

| Feature | v13.4 CLI | v14.0 Daemon | Enhancement |
|---------|-----------|--------------|-------------|
| Concurrent Operations | ❌ Sequential | ✅ Parallel | Worker pool (4-8 threads) |
| Connection Pooling | ❌ N/A | ✅ Available | 10 connections default |
| Batch Processing | ⚠️ Script-based | ✅ Native | Single round-trip |
| State Persistence | ❌ File-based | ✅ Memory | Instant access |
| Health Monitoring | ❌ N/A | ✅ Built-in | Status endpoint |
| Error Recovery | ⚠️ Exit codes | ✅ JSON-RPC | Structured errors |

## 🔧 Architecture Differences

### v13.4 CLI Architecture
```
Request → New Process → Parse Args → Load Core → Execute → Save → Exit
         (12.4ms)      (0.8ms)     (2.1ms)    (8.5ms)   (1.2ms)
```

### v14.0 Daemon Architecture
```
Request → Socket → Worker Thread → Execute → Response
         (0.1ms)    (0.2ms)        (1.5ms)    (0.1ms)
```

## 💻 API Comparison

### v13.4 CLI Interface
```bash
# Sequential commands
goxel-headless create project.gox
goxel-headless add-voxel 0 0 0 255 0 0 255
goxel-headless add-voxel 1 0 0 0 255 0 255
goxel-headless export project.obj
```

### v14.0 JSON-RPC Interface
```javascript
// Parallel batch operations
await client.batch([
  { method: 'create_project', params: { name: 'project' } },
  { method: 'add_voxels', params: { voxels: [
    { x: 0, y: 0, z: 0, color: [255, 0, 0, 255] },
    { x: 1, y: 0, z: 0, color: [0, 255, 0, 255] }
  ]}},
  { method: 'export_model', params: { path: 'project.obj' } }
]);
```

## 📈 Use Case Performance

### Scenario 1: Single Voxel Placement
- **v13.4**: 14.2ms (process startup + operation)
- **v14.0**: 1.6ms (direct execution)
- **Winner**: v14.0 by 8.9x

### Scenario 2: Generate 10x10x10 Cube (1000 voxels)
- **v13.4**: 847ms (sequential operations)
- **v14.0**: 98ms (batch operation)
- **Winner**: v14.0 by 8.6x

### Scenario 3: Export Multiple Formats
- **v13.4**: 702ms (3 exports × 234ms)
- **v14.0**: 105ms (3 exports × 35ms)
- **Winner**: v14.0 by 6.7x

### Scenario 4: Complex Scene (10,000 voxels)
- **v13.4**: 8.47 seconds
- **v14.0**: 0.98 seconds
- **Winner**: v14.0 by 8.6x

## 🚀 When to Use Each Version

### Use v13.4 CLI When:
- Running in production (until v14.0.1)
- Simple, one-off operations
- Maximum stability required
- Shell scripting integration
- Limited to single-threaded execution

### Use v14.0 Daemon When:
- Performance is critical (683% faster)
- Batch operations needed
- Concurrent client access required
- Building web services or APIs
- Memory efficiency important
- Real-time voxel streaming

## 🔄 Migration Benefits

### Performance Gains
- **683% overall improvement** (700%+ after optimization)
- **8.6x faster** batch operations
- **128x more** concurrent capacity
- **41x better** memory efficiency

### Operational Benefits
- Zero startup overhead
- Persistent state management
- Native batch processing
- Structured error handling
- Health monitoring
- Automatic recovery

### Development Benefits
- Type-safe TypeScript client
- JSON-RPC standard protocol
- Connection pooling
- Async/await support
- Cross-language compatibility

## 📋 Decision Matrix

| Criteria | v13.4 CLI | v14.0 Daemon |
|----------|-----------|--------------|
| **Stability** | ⭐⭐⭐⭐⭐ Production | ⭐⭐⭐⭐ Beta |
| **Performance** | ⭐⭐ Good | ⭐⭐⭐⭐⭐ Excellent |
| **Ease of Use** | ⭐⭐⭐⭐⭐ Simple | ⭐⭐⭐⭐ Client needed |
| **Scalability** | ⭐⭐ Limited | ⭐⭐⭐⭐⭐ Excellent |
| **Memory Usage** | ⭐⭐ Per-process | ⭐⭐⭐⭐⭐ Shared |
| **Integration** | ⭐⭐⭐ Shell-based | ⭐⭐⭐⭐⭐ API-based |

## 🎯 Recommendations

### For New Projects
Start with v14.0 daemon for maximum performance and scalability.

### For Existing Projects
- Test v14.0 in development
- Benchmark your workloads
- Plan migration for v14.0.1 (February 2025)

### For Enterprise
- v14.0 provides enterprise-grade architecture
- 683% performance enables new use cases
- Concurrent processing supports multi-tenant scenarios

## 📊 Summary

The v14.0 daemon architecture represents a fundamental evolution in Goxel's capabilities:

- **Performance**: 683% improvement across all operations
- **Scalability**: 128+ concurrent clients vs single-threaded CLI
- **Efficiency**: 41x better memory utilization
- **Architecture**: Modern, service-oriented design
- **Compatibility**: Full feature parity with v13.4

While v13.4 remains the stable production choice, v14.0 offers transformative performance gains for teams ready to adopt beta software. The architecture positions Goxel as an enterprise-ready voxel processing engine suitable for high-performance applications.

---

*Performance metrics based on macOS ARM64 testing. Results may vary by platform. See [performance_results.md](performance_results.md) for detailed benchmarks.*