# Socket Server Critical Issue Analysis

**To**: Chen Wei (Agent-1) - Core Infrastructure Specialist  
**From**: Sarah Johnson (Agent-4) - Testing & QA Specialist  
**Date**: January 26, 2025  
**Priority**: CRITICAL - Blocking all testing

## Issue Summary

The Goxel v14.0 daemon fails to create a Unix domain socket on macOS ARM64, preventing all client connections and blocking comprehensive testing across all platforms.

## Technical Details

### Observed Behavior
1. Daemon process starts successfully
2. PID file is created correctly
3. Process remains running
4. **Socket file is never created**
5. No error messages in verbose output
6. Process appears to hang in main loop

### Test Evidence

```bash
# Test command
/Users/jimmy/jimmy_side_projects/goxel/tests/goxel-daemon \
  --foreground \
  --socket /tmp/test.sock \
  --pid-file /tmp/test.pid \
  --verbose

# Expected result
-rw-r--r--  1 user  wheel    6  /tmp/test.pid
srwxrw----  1 user  wheel    0  /tmp/test.sock  # MISSING!

# Actual result  
-rw-r--r--  1 user  wheel    6  /tmp/test.pid
# No socket file created
```

### Code Analysis

From `socket_server.c`, the socket creation flow appears correct:
1. `socket(AF_UNIX, SOCK_STREAM, 0)` - Create socket
2. `bind()` - Bind to path
3. `listen()` - Start listening
4. `chmod()` - Set permissions

However, the socket file never appears in the filesystem.

### Hypothesis

Based on code review, potential issues:

1. **Thread Initialization**: The accept thread may not be starting
2. **Mutex Deadlock**: Server mutex might be locked during initialization
3. **Silent Failure**: Error handling might not be reporting failures
4. **Platform-Specific**: macOS-specific Unix socket behavior

## Debugging Performed

### 1. Process Verification
```bash
ps aux | grep goxel-daemon
# Process is running, consuming minimal CPU
```

### 2. File System Check
```bash
ls -la /tmp/goxel*.sock
# No socket files found
```

### 3. Unit Test Results
```bash
./test_daemon_lifecycle
# All mock tests pass
# Real socket tests not included
```

### 4. strace/dtruss Equivalent
macOS doesn't have strace, but we could use `dtruss` with SIP disabled.

## Recommended Debugging Steps

### 1. Add Debug Logging
```c
// In socket_server_start()
LOG_D("Creating socket with path: %s", server->socket_path);
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
LOG_D("Socket fd: %d, errno: %d", fd, errno);

LOG_D("Binding socket to path: %s", server->socket_path);
int bind_result = bind(fd, ...);
LOG_D("Bind result: %d, errno: %d", bind_result, errno);
```

### 2. Check Thread Creation
```c
// After pthread_create for accept_thread
LOG_D("Accept thread created: %d", result);
```

### 3. Verify Main Loop
```c
// In daemon main loop
LOG_D("Main loop iteration, socket_server_running: %d", 
      socket_server_is_running(daemon->socket_server));
```

## Platform Considerations

### macOS Specific Issues
1. **Sandbox Restrictions**: Check if app sandboxing affects socket creation
2. **File System Permissions**: /tmp might have special handling
3. **Socket Path Length**: Ensure path < 104 chars (macOS limit)

### Test Alternative Paths
```bash
# Try user directory
~/Library/Caches/goxel/daemon.sock

# Try current directory  
./goxel-daemon.sock
```

## Minimal Test Case

Created `test_daemon_basic.c` to isolate the issue:
```c
// Forks daemon process
// Waits for socket creation
// Attempts connection
// Result: Socket never created
```

## Impact on Testing

**Blocked Tests**:
- All JSON RPC method validation
- Performance benchmarking (requires working connections)
- Concurrent client testing
- MCP integration validation
- Cross-platform comparison

**Available Tests**:
- Binary compilation
- Process lifecycle
- Signal handling
- Memory baseline

## Recommended Fix Priority

1. **Add verbose logging** to socket creation path
2. **Verify thread startup** in socket server
3. **Test with simplified socket** creation (no threading)
4. **Check platform-specific** requirements

## Alternative Testing Approach

If fix is delayed, consider:
1. Mock socket server for functional tests
2. Direct function testing without socket layer
3. Linux-first testing approach
4. Simplified daemon without socket server

## Files to Review

1. `/src/daemon/socket_server.c` - Socket creation logic
2. `/src/daemon/daemon_main.c` - Main loop and initialization
3. `/src/daemon/worker_pool.c` - Thread management
4. Build configuration for macOS-specific flags

## Test Resources Available

- macOS 15.5 ARM64 test environment ready
- Basic socket test case implemented
- Unit test framework operational
- Performance measurement tools prepared

Please investigate and provide guidance on resolution approach. This issue blocks all network-based testing across all platforms.

---

**Escalation**: This is a CRITICAL blocker for A4-03 task completion. All platform testing depends on socket functionality.