# Goxel v14.0 Daemon Memory Architecture Analysis

## Executive Summary

The Goxel v14.0 daemon suffers from fundamental architectural conflicts between its original single-user GUI design and the multi-request daemon requirements. This document analyzes the root causes and proposes solutions.

## Problem Analysis

### 1. Architectural Mismatch

Goxel was originally designed as a single-user GUI application where global state is acceptable and even beneficial. The daemon mode attempts to retrofit concurrent request handling onto this architecture, creating fundamental conflicts.

**Key Issues:**
- Global `goxel_t goxel` variable used throughout the codebase
- Many functions implicitly depend on global state
- No clear separation between UI state and core voxel operations

### 2. State Management Conflicts

```c
// Global state (from goxel.c)
goxel_t goxel = {};

// Per-request context (from daemon)
goxel_core_context_t *g_goxel_context = NULL;

// Temporary synchronization during operations
goxel.image = ctx->image;  // Dangerous shared reference
```

The daemon attempts to maintain per-request contexts, but many operations require synchronization with global state, creating shared memory references.

### 3. Memory Lifecycle Issues

**Current Flow:**
1. `create_project` creates new `image_t` in context
2. Some operations temporarily sync: `goxel.image = ctx->image`
3. Next `create_project` deletes old image
4. If global still references deleted memory → crash

**Root Cause:**
- Unclear ownership model
- Multiple pointers to same memory
- No reference counting or smart pointers
- Deletion timing conflicts

### 4. Specific Problem Areas

#### Export Operations
```c
// From goxel_core_export_project()
image_t *original_image = goxel.image;
goxel.image = ctx->image;  // Temporary sync
// ... export operation ...
goxel.image = original_image;  // Restore
```

#### Script Execution
```c
// Scripts operate on global goxel.image
goxel.image = ctx->image;
script_run_from_file(script_file, 0, NULL);
if (goxel.image != ctx->image) {
    // Script created new image
    ctx->image = goxel.image;
}
```

#### Layer Operations
- Layers depend on `image->active_layer`
- Material system uses `image->active_material`
- Both require global state consistency

## Technical Deep Dive

### Memory Corruption Sequence

1. **Request 1**: Create project A
   - `ctx->image = image_new()` → Memory at 0x1000
   - `goxel.image = ctx->image` → Both point to 0x1000

2. **Request 2**: Create project B
   - Detect `goxel.image == ctx->image` (both 0x1000)
   - Create new image at 0x2000
   - Delete old image at 0x1000
   - `ctx->image = 0x2000`
   - But other code may still reference 0x1000 → use-after-free

### Why The Fix Failed

The attempted fix delayed deletion but didn't address the fundamental issue:
- Global state still exists and is used
- Other parts of code may cache image pointers
- Concurrent requests could still interfere
- Layer creation in `create_project` may fail due to missing initialization

## Proposed Solutions

### Solution 1: Thread-Local Storage (Recommended)

Replace global state with thread-local storage:

```c
__thread goxel_t *thread_goxel = NULL;

// In each worker thread
void worker_init() {
    thread_goxel = calloc(1, sizeof(goxel_t));
    thread_goxel->image = NULL;
}

// Replace all goxel. references with thread_goxel->
```

**Pros:**
- Minimal code changes
- Each worker has isolated state
- No shared memory issues

**Cons:**
- Requires thread model (not process model)
- Some overhead per thread

### Solution 2: Complete State Isolation

Remove all global state dependencies:

```c
// Pass context explicitly to all functions
int volume_set_at(volume_t *volume, goxel_context_t *ctx, ...);
int image_export(image_t *img, goxel_context_t *ctx, ...);
```

**Pros:**
- True isolation
- Supports any concurrency model
- Clean architecture

**Cons:**
- Massive refactoring required
- API changes throughout codebase
- Risk of breaking existing functionality

### Solution 3: Single-Project Daemon (Pragmatic)

Accept the limitation and make daemon single-project:

```c
// Serialize all requests
pthread_mutex_lock(&global_project_mutex);
// Process request
pthread_mutex_unlock(&global_project_mutex);
```

**Pros:**
- Simple to implement
- Works with existing architecture
- Low risk

**Cons:**
- No concurrent project handling
- Performance limitations
- Not truly "daemon" architecture

### Solution 4: Process Isolation

Fork a new process for each project:

```c
pid_t pid = fork();
if (pid == 0) {
    // Child process handles one project
    handle_project_lifecycle();
    exit(0);
}
```

**Pros:**
- Complete isolation
- Crash resilience
- Works with existing code

**Cons:**
- Higher resource usage
- IPC complexity
- Platform-specific code

## Recommendations

### Short Term (v14.0.x)
1. Implement Solution 3 (Single-Project Daemon)
2. Add clear documentation about limitations
3. Ensure proper cleanup between projects
4. Add health checks and auto-restart

### Medium Term (v14.1)
1. Implement Solution 1 (Thread-Local Storage)
2. Gradually reduce global state usage
3. Add reference counting to image_t
4. Improve error handling

### Long Term (v15.0)
1. Full architectural refactor (Solution 2)
2. Clear separation of concerns
3. Proper dependency injection
4. Modern C++ with RAII

## Implementation Roadmap

### Phase 1: Stabilization (1-2 weeks)
- Add mutex protection around project operations
- Fix immediate crash issues
- Document current limitations
- Add automated tests

### Phase 2: Thread-Local Migration (2-4 weeks)
- Identify all global state usage
- Implement thread-local storage
- Update worker pool architecture
- Extensive testing

### Phase 3: Architecture Cleanup (1-2 months)
- Refactor core APIs
- Remove implicit state dependencies
- Add proper lifecycle management
- Performance optimization

## Conclusion

The current memory management issues stem from a fundamental architectural mismatch. While quick fixes can provide temporary stability, a proper solution requires accepting either architectural limitations (single-project daemon) or undertaking significant refactoring (state isolation).

The recommended approach is to stabilize with a single-project daemon model for v14.0, then gradually migrate to a thread-local architecture for v14.1, with a long-term goal of complete architectural refactoring for v15.0.

---

**Document Version**: 1.0  
**Date**: 2025-08-02  
**Author**: Technical Analysis Team