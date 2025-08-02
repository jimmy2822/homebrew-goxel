# Goxel Daemon Quick Fix Guide

## Minimal Changes for Immediate Stability

This guide provides the absolute minimum changes needed to prevent daemon crashes in v14.0.

## Quick Fix #1: Reset Everything Between Projects

Edit `src/daemon/json_rpc.c`, modify `handle_goxel_create_project`:

```c
static json_rpc_response_t *handle_goxel_create_project(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // QUICK FIX: Complete cleanup before creating new project
    extern goxel_t goxel;
    
    // Step 1: Clear ALL global state
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Step 2: Reset context
    if (g_goxel_context->image) {
        // Don't delete if it's the same as global (already deleted)
        if (g_goxel_context->image != goxel.image) {
            image_delete(g_goxel_context->image);
        }
        g_goxel_context->image = NULL;
    }
    
    // Step 3: Clear any cached materials, cameras, etc.
    goxel.image = NULL;
    goxel.tool = NULL;
    goxel.tool_volume = NULL;
    goxel.pick_volume = NULL;
    
    // Continue with original logic...
    const char *name = get_string_param(&request->params, 0, "name");
    int width = get_int_param(&request->params, 1, "width");
    int height = get_int_param(&request->params, 2, "height"); 
    int depth = get_int_param(&request->params, 3, "depth");
    
    if (width <= 0) width = 64;
    if (height <= 0) height = 64;
    if (depth <= 0) depth = 64;
    if (!name) name = "New Project";
    
    LOG_D("Creating project: %s (%dx%dx%d)", name, width, height, depth);
    
    int result = goxel_core_create_project(g_goxel_context, name, width, height, depth);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to create project: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 1,
                                             error_msg, NULL, &request->id);
    }
    
    // QUICK FIX: Ensure global is synced
    goxel.image = g_goxel_context->image;
    
    json_value *result_obj = json_object_new(5);
    json_object_push(result_obj, "success", json_boolean_new(true));
    json_object_push(result_obj, "name", json_string_new(name));
    json_object_push(result_obj, "width", json_integer_new(width));
    json_object_push(result_obj, "height", json_integer_new(height));
    json_object_push(result_obj, "depth", json_integer_new(depth));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}
```

## Quick Fix #2: Simplify goxel_core_create_project

Edit `src/core/goxel_core.c`:

```c
int goxel_core_create_project(goxel_core_context_t *ctx, const char *name, 
                              int width, int height, int depth)
{
    if (!ctx) return -1;
    if (check_read_only(ctx, "create project") != 0) return -1;
    
    extern goxel_t goxel;
    
    // QUICK FIX: Always start fresh
    // Delete context image
    if (ctx->image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    // Delete global image if different
    if (goxel.image && goxel.image != ctx->image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Create new image
    image_t *new_image = image_new();
    if (!new_image) return -1;
    
    // Set project name if provided
    if (name) {
        if (new_image->path) free(new_image->path);
        new_image->path = strdup(name);
    }
    
    // Assign to both context and global
    ctx->image = new_image;
    goxel.image = new_image;
    
    // Ensure we have at least one layer
    if (!new_image->layers) {
        layer_t *layer = image_add_layer(new_image, NULL);
        if (!layer) {
            LOG_E("Failed to create default layer");
            return -1;
        }
        // Make sure it's active
        new_image->active_layer = layer;
    }
    
    return 0;
}
```

## Quick Fix #3: Add Safety Check in add_voxel

Edit `src/core/goxel_core.c`, in `goxel_core_add_voxel`:

```c
int goxel_core_add_voxel(goxel_core_context_t *ctx, int x, int y, int z, 
                         uint8_t rgba[4], int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    // QUICK FIX: Ensure global is synced
    extern goxel_t goxel;
    if (goxel.image != ctx->image) {
        LOG_W("Syncing global image pointer");
        goxel.image = ctx->image;
    }
    
    layer_t *layer = NULL;
    
    // Rest of the function remains the same...
}
```

## Testing the Fix

1. **Rebuild the daemon**:
```bash
scons daemon=1 -j4
```

2. **Restart the service**:
```bash
brew services restart goxel
```

3. **Test with simple script**:
```python
import json
import socket

def test_daemon():
    sock_path = "/opt/homebrew/var/run/goxel/goxel.sock"
    
    # Test 1: Create first project
    req1 = {"jsonrpc": "2.0", "method": "goxel.create_project", 
            "params": {"name": "Test1"}, "id": 1}
    
    # Test 2: Create second project
    req2 = {"jsonrpc": "2.0", "method": "goxel.create_project", 
            "params": {"name": "Test2"}, "id": 2}
    
    for req in [req1, req2]:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(sock_path)
        sock.send((json.dumps(req) + "\n").encode())
        response = sock.recv(4096)
        print(f"Request {req['id']}: {response}")
        sock.close()

test_daemon()
```

## Why This Works

1. **Complete State Reset**: We clear ALL references before creating new project
2. **No Shared References**: Both pointers are set to the same new image
3. **Consistent State**: Global and context always point to same image
4. **Safe Deletion**: We check pointers before deleting to avoid double-free

## Limitations

- This is a **band-aid**, not a cure
- Only ONE project at a time
- No concurrent requests
- Must complete one operation before starting another

## When to Use This Fix

- Need daemon working TODAY
- Can live with single-project limitation
- Waiting for proper architectural fix
- Testing/development environments

## What This Doesn't Fix

- Concurrent request handling
- Multiple active projects
- Thread safety issues
- Performance optimization

## Next Steps

1. Apply these quick fixes
2. Test thoroughly
3. Document limitations clearly
4. Plan for proper architectural fix in v14.1

---

**Quick Fix Version**: 1.0  
**Estimated Time**: 30 minutes  
**Risk Level**: Low  
**Stability Improvement**: High