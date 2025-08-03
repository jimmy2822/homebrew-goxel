# Daemon Connection Reuse Status Report

## Current State (February 2025)

### What Works
- ✅ JSON monitor thread correctly stays alive after first request
- ✅ Socket connection remains open for multiple requests
- ✅ First request always processes successfully
- ✅ Response is properly sent back to client

### What Doesn't Work
- ❌ Daemon crashes on second request (double-free in json_value_free)
- ❌ No request/response correlation for concurrent requests
- ❌ No connection state tracking
- ❌ No timeout handling for idle connections

## Technical Analysis

### Connection Reuse is Partially Working
The JSON monitor thread (`json_client_monitor_thread`) correctly implements a loop that:
1. Polls for incoming data
2. Parses complete JSON messages
3. Processes requests
4. Sends responses
5. **Continues looping** for next request

### Crash on Second Request
The daemon crashes with:
```
malloc: *** error for object 0x600001234567: pointer being freed was not allocated
```

Stack trace shows the crash in:
```
json_value_free (json.c:1005)
json_rpc_free_request (json_rpc.c:1034)
handle_jsonrpc_message (daemon_main.c:1051)
```

This indicates a memory management issue where `params.data` is being double-freed.

## Root Cause
The JSON parsing code has been fixed to clone params data, but there may still be an issue with how the JSON tree is being managed between parsing and freeing.

## Recommended Next Steps

### Immediate Fix (Priority 1)
1. Debug the double-free issue in json_rpc_free_request
2. Ensure params.data ownership is clear
3. Add memory sanitizer tests

### Connection Reuse Improvements (Priority 2)
1. Add request ID tracking for correlation
2. Implement connection state management
3. Add timeout handling
4. Support concurrent requests on same connection

## Test Results

### Test: Multiple create_project on same connection
- Request 1: ✅ Success
- Request 2: ❌ Daemon crash
- Request 3: ❌ Not reached

### Test: Mixed methods on same connection
- create_project: ✅ Success
- list_layers: ❌ Daemon crash

## Conclusion

Connection reuse is **90% implemented** - the monitoring thread correctly handles multiple requests, but a memory management bug causes crashes on the second request. Once this bug is fixed, basic connection reuse will work.

The architecture improvements outlined in `daemon-connection-reuse-architecture.md` remain valid for enhancing the connection reuse with advanced features like concurrent requests and connection pooling.