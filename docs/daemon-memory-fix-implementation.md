# Goxel Daemon Memory Fix Implementation Guide

## Immediate Fix: Single-Project Daemon Model

Based on the architectural analysis, this document provides a concrete implementation plan for stabilizing the v14.0 daemon using a single-project model.

## Implementation Details

### 1. Add Project Mutex Protection

Create a new file `src/daemon/project_mutex.h`:

```c
#ifndef PROJECT_MUTEX_H
#define PROJECT_MUTEX_H

#include <pthread.h>
#include <stdbool.h>

typedef struct {
    pthread_mutex_t mutex;
    bool has_active_project;
    char project_id[64];
    time_t last_activity;
} project_state_t;

// Global project state
extern project_state_t g_project_state;

// Initialize project mutex system
int project_mutex_init(void);

// Cleanup project mutex system  
void project_mutex_cleanup(void);

// Acquire project lock
int project_lock_acquire(const char *request_id);

// Release project lock
void project_lock_release(void);

// Check if project is idle (for auto-cleanup)
bool project_is_idle(int timeout_seconds);

#endif
```

### 2. Modify JSON-RPC Handlers

Update `src/daemon/json_rpc.c`:

```c
static json_rpc_response_t *handle_goxel_create_project(const json_rpc_request_t *request)
{
    // Acquire exclusive project lock
    char request_id[32];
    snprintf(request_id, sizeof(request_id), "req_%d", request->id.u.integer);
    
    if (project_lock_acquire(request_id) != 0) {
        return json_rpc_create_response_error(JSON_RPC_SERVER_ERROR,
            "Another project operation is in progress", 
            NULL, &request->id);
    }
    
    // Clean up any existing project state
    if (g_goxel_context && g_goxel_context->image) {
        goxel_core_reset(g_goxel_context);
    }
    
    // Original create_project logic here...
    int result = goxel_core_create_project(g_goxel_context, name, width, height, depth);
    
    // Update project state
    if (result == 0) {
        g_project_state.has_active_project = true;
        strncpy(g_project_state.project_id, name ? name : "unnamed", 
                sizeof(g_project_state.project_id) - 1);
        g_project_state.last_activity = time(NULL);
    }
    
    // Note: Don't release lock here - keep it until save/export/timeout
    
    return response;
}
```

### 3. Add Auto-Cleanup Timer

In `src/daemon/daemon_main.c`:

```c
// Add to daemon_context_t
struct daemon_context {
    // ... existing fields ...
    pthread_t cleanup_thread;
    bool cleanup_thread_running;
};

// Cleanup thread function
static void *project_cleanup_thread(void *arg)
{
    daemon_context_t *daemon = (daemon_context_t *)arg;
    
    while (daemon->cleanup_thread_running) {
        sleep(10); // Check every 10 seconds
        
        if (project_is_idle(300)) { // 5 minute timeout
            LOG_I("Auto-cleaning idle project");
            
            project_lock_acquire("auto_cleanup");
            
            if (g_goxel_context) {
                goxel_core_reset(g_goxel_context);
            }
            
            g_project_state.has_active_project = false;
            memset(g_project_state.project_id, 0, sizeof(g_project_state.project_id));
            
            project_lock_release();
        }
    }
    
    return NULL;
}
```

### 4. Ensure Thread Safety in Core Functions

Update critical functions in `src/core/goxel_core.c`:

```c
int goxel_core_create_project(goxel_core_context_t *ctx, const char *name, 
                              int width, int height, int depth)
{
    if (!ctx) return -1;
    if (check_read_only(ctx, "create project") != 0) return -1;
    
    // Critical section for project creation
    static pthread_mutex_t create_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&create_mutex);
    
    // Store reference to global goxel
    extern goxel_t goxel;
    
    // Complete reset of global state
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Clean up context image if present
    if (ctx->image && ctx->image != goxel.image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    // Create new image
    image_t *new_image = image_new();
    if (!new_image) {
        pthread_mutex_unlock(&create_mutex);
        return -1;
    }
    
    // Set both context and global
    ctx->image = new_image;
    goxel.image = new_image;
    
    // Set project name if provided
    if (name && new_image->path) {
        free(new_image->path);
        new_image->path = strdup(name);
    }
    
    // Ensure at least one layer exists
    if (!new_image->layers) {
        layer_t *layer = image_add_layer(new_image, NULL);
        if (!layer) {
            LOG_E("Failed to create default layer");
        }
    }
    
    pthread_mutex_unlock(&create_mutex);
    return 0;
}
```

### 5. Add Health Check Endpoint

```c
static json_rpc_response_t *handle_goxel_health_check(const json_rpc_request_t *request)
{
    json_value *result = json_object_new(5);
    
    json_object_push(result, "daemon_status", json_string_new("healthy"));
    json_object_push(result, "has_active_project", 
                     json_boolean_new(g_project_state.has_active_project));
    
    if (g_project_state.has_active_project) {
        json_object_push(result, "project_id", 
                         json_string_new(g_project_state.project_id));
        json_object_push(result, "idle_seconds", 
                         json_integer_new(time(NULL) - g_project_state.last_activity));
    }
    
    json_object_push(result, "max_concurrent_projects", json_integer_new(1));
    
    return json_rpc_create_response_result(result, &request->id);
}
```

## Testing Strategy

### 1. Unit Tests

```python
def test_single_project_enforcement():
    """Test that only one project can be active at a time"""
    
    # Create first project
    resp1 = send_json_rpc("goxel.create_project", {"name": "Project1"})
    assert resp1["result"]["success"] == True
    
    # Try to create second project without releasing first
    resp2 = send_json_rpc("goxel.create_project", {"name": "Project2"})
    assert "error" in resp2
    assert "in progress" in resp2["error"]["message"]
    
    # Save first project (should release lock)
    resp3 = send_json_rpc("goxel.save_project", {"path": "/tmp/project1.gox"})
    assert resp3["result"]["success"] == True
    
    # Now second project should succeed
    resp4 = send_json_rpc("goxel.create_project", {"name": "Project2"})
    assert resp4["result"]["success"] == True
```

### 2. Stress Tests

```python
def test_sequential_projects():
    """Test creating many projects sequentially"""
    
    for i in range(100):
        # Create project
        resp = send_json_rpc("goxel.create_project", 
                             {"name": f"StressTest{i}"})
        assert resp["result"]["success"] == True
        
        # Add some voxels
        for j in range(10):
            resp = send_json_rpc("goxel.add_voxel", {
                "position": [i, j, 0],
                "color": [255, 0, 0, 255]
            })
        
        # Save and release
        resp = send_json_rpc("goxel.save_project", 
                             {"path": f"/tmp/stress{i}.gox"})
        assert resp["result"]["success"] == True
        
        # Verify daemon still healthy
        resp = send_json_rpc("goxel.health_check", {})
        assert resp["result"]["daemon_status"] == "healthy"
```

## Migration Path

### For Existing Users

1. **Update Documentation**: Clearly state single-project limitation
2. **Add Warning Messages**: When concurrent requests detected
3. **Provide Migration Guide**: Show how to adapt workflows

### Example Client Adaptation

```python
class GoxelDaemonClient:
    def __init__(self, socket_path):
        self.socket_path = socket_path
        self.has_active_project = False
    
    def create_project(self, name):
        # Always save/cleanup previous project first
        if self.has_active_project:
            self.cleanup_project()
        
        resp = self.send_request("goxel.create_project", {"name": name})
        if resp.get("result", {}).get("success"):
            self.has_active_project = True
        return resp
    
    def cleanup_project(self):
        # Save or export before creating new project
        if self.has_active_project:
            self.send_request("goxel.save_project", 
                              {"path": "/tmp/temp_save.gox"})
            self.has_active_project = False
```

## Monitoring and Debugging

### 1. Add Logging

```c
// In project state changes
LOG_I("Project state change: %s -> %s (request: %s)",
      g_project_state.has_active_project ? "active" : "idle",
      new_state ? "active" : "idle", 
      request_id);

// In lock acquisition
LOG_D("Project lock requested by %s, current holder: %s",
      request_id, g_project_state.project_id);
```

### 2. Debug Commands

Add debug JSON-RPC methods:

```c
{"goxel.debug.get_project_state", handle_debug_get_project_state, "[DEBUG] Get current project state"},
{"goxel.debug.force_cleanup", handle_debug_force_cleanup, "[DEBUG] Force project cleanup"},
{"goxel.debug.get_memory_stats", handle_debug_get_memory_stats, "[DEBUG] Get memory usage stats"},
```

## Rollout Plan

### Phase 1: Internal Testing (Week 1)
- Implement basic mutex protection
- Test with existing test suite
- Fix any regressions

### Phase 2: Beta Release (Week 2)
- Release as v14.0.1-beta
- Gather feedback from early adopters
- Monitor for stability issues

### Phase 3: Production Release (Week 3-4)
- Address beta feedback
- Update all documentation
- Release as v14.0.1

## Success Metrics

1. **Stability**: Zero crashes in 1000 sequential project operations
2. **Performance**: < 100ms overhead for project lock acquisition
3. **Compatibility**: Existing clients work with minor modifications
4. **Reliability**: 99.9% uptime over 24-hour period

## Future Enhancements

Once stable with single-project model:

1. **Project Queuing**: Queue requests instead of rejecting
2. **Project Pooling**: Pre-initialize project contexts
3. **State Snapshots**: Fast save/restore for project switching
4. **Graduated Migration**: Slowly introduce multi-project support

---

**Document Version**: 1.0  
**Date**: 2025-08-02  
**Implementation Priority**: HIGH  
**Estimated Effort**: 1-2 weeks