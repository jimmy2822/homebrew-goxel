# Goxel Daemon v0.16 Milestone: Render Transfer Architecture

**Version**: 0.16.1  
**Status**: Planning  
**Target Date**: Q1 2025  
**Priority**: High

## Executive Summary

Implement file-path reference architecture for efficient image transfer between Goxel daemon and MCP clients, replacing potential Base64 encoding bottlenecks with a robust file-based approach.

## Problem Statement

Current considerations for render transfer include:
- Base64 encoding adds 33% overhead and memory pressure
- Large renders (4K+) may exceed buffer limits  
- MCP clients need efficient access to rendered images
- Memory efficiency crucial for production deployments

## Proposed Solution: File-Path Reference Architecture

### Core Design

```json
// Request
{
  "jsonrpc": "2.0",
  "method": "goxel.render_scene",
  "params": {
    "width": 1920,
    "height": 1080,
    "options": {
      "return_mode": "file_path",  // New parameter
      "output_dir": "/tmp/goxel_renders"  // Optional custom dir
    }
  },
  "id": 1
}

// Response
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "file": {
      "path": "/tmp/goxel_renders/render_1736506542_abc123.png",
      "size": 2048576,
      "format": "png",
      "dimensions": {"width": 1920, "height": 1080},
      "checksum": "sha256:abc123...",
      "created_at": 1736506542,
      "expires_at": 1736510142  // 1 hour TTL
    }
  },
  "id": 1
}
```

### Implementation Plan

#### Phase 1: Core Infrastructure (Week 1-2)

**1.1 Render Manager Component**
```c
// src/daemon/render_manager.c
typedef struct {
    char* output_dir;
    size_t max_cache_size;
    int ttl_seconds;
    hash_table_t* active_renders;
} render_manager_t;

// Key functions
char* render_manager_create_path(render_manager_t* rm, const char* format);
int render_manager_cleanup_expired(render_manager_t* rm);
int render_manager_register(render_manager_t* rm, render_info_t* info);
```

**1.2 Directory Management**
- Default directory: `/tmp/goxel_renders/` (configurable)
- Automatic directory creation with proper permissions (0755)
- Platform-specific paths:
  - macOS: `/var/tmp/goxel_renders/`
  - Linux: `/tmp/goxel_renders/`
  - Windows: `%TEMP%\goxel_renders\`

**1.3 File Naming Convention**
```
render_[timestamp]_[session_id]_[hash].[format]
Example: render_1736506542_abc123_d7f8a9.png
```

#### Phase 2: API Enhancement (Week 2-3)

**2.1 Backward Compatibility**
```c
// Maintain existing behavior by default
if (params->return_mode == NULL) {
    // Legacy: save to specified path
    return render_to_file(params->output_path);
} else if (strcmp(params->return_mode, "file_path") == 0) {
    // New: managed temporary file
    return render_to_managed_path();
} else if (strcmp(params->return_mode, "base64") == 0) {
    // Future: inline Base64 for small images
    return render_to_base64();
}
```

**2.2 New Methods**
```json
// Get render status
{"method": "goxel.get_render_info", "params": {"path": "/tmp/..."}}

// Clean up specific render
{"method": "goxel.cleanup_render", "params": {"path": "/tmp/..."}}

// List active renders
{"method": "goxel.list_renders", "params": {}}
```

#### Phase 3: File Management (Week 3-4)

**3.1 Automatic Cleanup**
```c
// Background thread for cleanup
void* cleanup_thread(void* arg) {
    while (daemon_running) {
        sleep(300);  // Check every 5 minutes
        
        // Remove files older than TTL
        DIR* dir = opendir(render_dir);
        struct dirent* entry;
        time_t now = time(NULL);
        
        while ((entry = readdir(dir)) != NULL) {
            struct stat st;
            if (stat(entry->d_name, &st) == 0) {
                if (now - st.st_mtime > TTL_SECONDS) {
                    unlink(entry->d_name);
                }
            }
        }
    }
}
```

**3.2 Resource Limits**
- Maximum cache size: 1GB (configurable)
- Per-render size limit: 100MB
- Concurrent render limit: 100 files
- TTL: 1 hour (configurable)

**3.3 Security Considerations**
- Validate all file paths (no directory traversal)
- Use secure random tokens in filenames
- Set restrictive permissions (0600 for files)
- Optional encryption for sensitive renders

#### Phase 4: MCP Integration (Week 4)

**4.1 MCP Tool Updates**
```typescript
// goxel-mcp updates
async function renderScene(params: RenderParams): Promise<RenderResult> {
  const response = await daemon.request('goxel.render_scene', {
    ...params,
    options: { return_mode: 'file_path' }
  });
  
  // Read file from returned path
  const imagePath = response.result.file.path;
  const imageData = await fs.readFile(imagePath);
  
  return {
    data: imageData,
    metadata: response.result.file
  };
}
```

**4.2 Client Library Support**
```python
# Python client example
class GoxelClient:
    def render_scene(self, width, height, return_data=False):
        result = self.request('goxel.render_scene', {
            'width': width,
            'height': height,
            'options': {'return_mode': 'file_path'}
        })
        
        if return_data:
            with open(result['file']['path'], 'rb') as f:
                return f.read()
        return result['file']['path']
```

## Testing Strategy

### Unit Tests
```c
// tests/test_render_manager.c
void test_render_path_generation() {
    render_manager_t* rm = render_manager_create("/tmp/test");
    char* path = render_manager_create_path(rm, "png");
    assert(strstr(path, "/tmp/test/render_") != NULL);
    assert(strstr(path, ".png") != NULL);
}

void test_cleanup_expired() {
    // Create old files and verify cleanup
}

void test_size_limits() {
    // Test max cache size enforcement
}
```

### Integration Tests
```bash
# tests/test_render_transfer.sh
#!/bin/bash

# Test file path mode
response=$(echo '{"method":"goxel.render_scene","params":{"width":100,"height":100,"options":{"return_mode":"file_path"}}}' | nc -U /tmp/goxel.sock)

# Extract path from response
path=$(echo $response | jq -r '.result.file.path')

# Verify file exists and is valid PNG
file -b "$path" | grep -q "PNG image data"

# Test cleanup
sleep $((TTL_SECONDS + 1))
[ ! -f "$path" ] || exit 1
```

### Performance Benchmarks
```python
# Comparison: Base64 vs File Path
import time, json, socket

def benchmark_render_mode(mode, iterations=100):
    times = []
    for _ in range(iterations):
        start = time.time()
        
        request = {
            "method": "goxel.render_scene",
            "params": {
                "width": 1920, "height": 1080,
                "options": {"return_mode": mode}
            }
        }
        # ... send request, receive response ...
        
        times.append(time.time() - start)
    
    return {
        "mode": mode,
        "avg": sum(times) / len(times),
        "min": min(times),
        "max": max(times)
    }

# Expected results:
# file_path: avg=0.05s, memory=10MB
# base64:    avg=0.15s, memory=50MB
```

## Migration Guide

### For Existing Clients

**No Breaking Changes** - Existing code continues to work:
```python
# Old code still works
daemon.request('goxel.render_scene', ['output.png', 800, 600])
```

**Opt-in to New Mode**:
```python
# New optimized mode
result = daemon.request('goxel.render_scene', {
    'width': 800,
    'height': 600,
    'options': {'return_mode': 'file_path'}
})
image_path = result['file']['path']
```

### Configuration

**Daemon Configuration** (`/etc/goxel/daemon.conf`):
```ini
[render]
output_dir = /var/tmp/goxel_renders
max_cache_size = 1073741824  # 1GB
ttl_seconds = 3600  # 1 hour
cleanup_interval = 300  # 5 minutes
```

**Environment Variables**:
```bash
GOXEL_RENDER_DIR=/custom/path
GOXEL_RENDER_TTL=7200
GOXEL_RENDER_MAX_SIZE=2147483648
```

## Success Metrics

### Performance Goals
- [ ] 90% reduction in memory usage for large renders
- [ ] 50% reduction in transfer time for 4K images
- [ ] Zero buffer overflow errors
- [ ] < 0.1s overhead for file management

### Reliability Goals
- [ ] Automatic cleanup prevents disk fill
- [ ] No orphaned files after daemon restart
- [ ] Graceful degradation on disk full
- [ ] 100% backward compatibility

### User Experience
- [ ] Seamless integration with MCP
- [ ] Clear error messages
- [ ] Debugging tools (list/cleanup commands)
- [ ] Comprehensive documentation

## Risk Analysis

### Identified Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Disk space exhaustion | High | TTL, size limits, monitoring |
| File permission issues | Medium | Fallback to memory mode |
| Concurrent access conflicts | Low | Unique filenames, atomic ops |
| Platform path differences | Low | Abstract path handling |
| Security vulnerabilities | Medium | Path validation, secure random |

### Rollback Plan
- Feature flag: `--disable-file-transfer`
- Automatic fallback to direct path on errors
- Version compatibility checks in protocol

## Timeline

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 1-2 | Core infrastructure | Render manager, tests |
| 2-3 | API enhancement | New methods, compatibility |
| 3-4 | File management | Cleanup, limits, security |
| 4 | MCP integration | Client updates, examples |
| 5 | Testing & docs | Full test suite, migration guide |
| 6 | Release prep | Performance validation, RC |

## Dependencies

- OSMesa (existing)
- POSIX file operations (existing)
- Optional: libcrypto for checksums

## Open Questions

1. **Compression**: Should we support automatic PNG optimization?
2. **Streaming**: Future support for progressive rendering?
3. **Caching**: Should identical renders return cached paths?
4. **Metadata**: Include render settings in file metadata?

## Conclusion

The file-path reference architecture provides a robust, scalable solution for render transfer that:
- Eliminates memory bottlenecks
- Maintains backward compatibility
- Enables efficient large image handling
- Provides clear upgrade path

This design positions Goxel daemon for production use cases requiring high-resolution rendering and efficient resource utilization.

---

**Author**: Goxel Development Team  
**Review Status**: Draft  
**Last Updated**: 2025-01-10