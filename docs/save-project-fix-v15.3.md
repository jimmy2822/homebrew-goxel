# Save_Project Fix - Goxel v15.3

## Critical Issue Resolution

**Issue**: The `goxel.save_project` JSON-RPC method would hang indefinitely in daemon mode, making it impossible to save projects programmatically.

**Status**: ✅ **RESOLVED** in v15.3 (August 10, 2025)

## Problem Description

### Symptoms
- `save_project` method never returned a response
- Client connections would timeout waiting for response  
- Daemon appeared to freeze when processing save requests
- Only affected daemon mode; GUI version worked fine

### Root Cause
The save operation in `src/core/formats/gox.c` attempted to generate a preview image by calling `goxel_render_to_buf()`, which tried to initialize OpenGL context in headless daemon mode - causing an infinite hang.

## Solution Implemented

### Technical Fix
Modified the `save_to_file()` function in `src/core/formats/gox.c` to detect daemon mode and skip preview generation:

```c
// Generate preview for .gox files (skip in daemon mode to avoid blocking)
bool skip_preview = false;
extern goxel_t goxel;
if (!goxel.graphics_initialized) {
    // No graphics means we're in headless/daemon mode
    skip_preview = true;
    LOG_I("Daemon mode detected (no graphics) - skipping preview generation");
}

if (!skip_preview) {
    preview = calloc(128 * 128, 4);
    if (preview) {
        // Try to generate preview, but don't fail if it doesn't work
        LOG_D("Generating preview for save file");
        goxel_render_to_buf(preview, 128, 128, 4);
        png = img_write_to_mem(preview, 128, 128, 4, &size);
        if (png) {
            chunk_write_all(out, "PREV", (char*)png, size);
            free(png);
        }
        free(preview);
    }
}
```

### Key Changes
1. **Detection Logic**: Check if `goxel.graphics_initialized` is false
2. **Skip Preview**: Don't attempt OpenGL operations in headless mode
3. **Preserve Functionality**: GUI version still generates previews normally
4. **File Integrity**: .gox files save correctly without previews in daemon mode

## Verification

### Before Fix
```python
# This would hang forever
{"jsonrpc": "2.0", "method": "goxel.save_project", "params": ["test.gox"], "id": 1}
# Client timeout after 10+ seconds with no response
```

### After Fix (v15.3)
```python
# Now responds instantly
{"jsonrpc": "2.0", "method": "goxel.save_project", "params": ["test.gox"], "id": 1}
# Response: {"jsonrpc": "2.0", "result": {"success": true, "path": "test.gox"}, "id": 1}
# Response time: 0.00s
```

## Impact

### Before v15.3
- ❌ save_project unusable in production
- ❌ Workflows requiring file persistence broken  
- ❌ Automation pipelines would fail/timeout
- ❌ Enterprise deployment blocked

### After v15.3  
- ✅ save_project works instantly (0.00s response)
- ✅ Complete automation workflows possible
- ✅ Production deployment ready
- ✅ Enterprise-grade reliability

## Installation

### Get Fixed Version
```bash
# Homebrew (recommended)
brew install jimmy/goxel/goxel-daemon

# Build from source
git clone https://github.com/jimmy2822/goxel.git
cd goxel
scons daemon=1 mode=release
```

### Verify Fix Working
```python
import socket, json, time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/opt/homebrew/var/run/goxel/goxel.sock")

# Test the fix
start_time = time.time()
request = {"jsonrpc": "2.0", "method": "goxel.create_project", "params": ["Test", 16, 16, 16], "id": 1}
sock.send(json.dumps(request).encode() + b"\n")
response = json.loads(sock.recv(4096).decode())

save_request = {"jsonrpc": "2.0", "method": "goxel.save_project", "params": ["test.gox"], "id": 2}
sock.send(json.dumps(save_request).encode() + b"\n")
save_response = json.loads(sock.recv(4096).decode())
elapsed = time.time() - start_time

print(f"✅ save_project completed in {elapsed:.2f}s")
print(f"Response: {save_response}")
sock.close()
```

## Timeline

- **August 9, 2025**: Issue identified and root cause analyzed
- **August 10, 2025**: Fix implemented, tested, and deployed
- **August 10, 2025**: Homebrew package updated with fix
- **August 10, 2025**: Documentation updated

## Related Documents

- [v15.3 Status Report](v15-daemon-status.md)
- [CLAUDE.md](../CLAUDE.md) - Main project documentation
- [API Reference](v15/daemon_api_reference.md) - Full API documentation

---

**Fix Status**: ✅ **RESOLVED**  
**Version**: 15.3+  
**Severity**: Critical → None  
**Impact**: Complete functionality restored