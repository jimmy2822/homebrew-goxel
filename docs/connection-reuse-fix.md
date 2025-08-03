# JSON-RPC Connection Reuse Fix

## Problem
The Goxel daemon v15.0 was experiencing connection reuse issues where:
- First JSON-RPC request: ✅ Works correctly
- Second request on same connection: ❌ Connection detected as closed
- Workaround: Create new connection for each request (inefficient)

## Root Cause
The issue was in `json_socket_handler.c` where `recv()` returning 0 was incorrectly interpreted as a closed connection. For non-blocking sockets, `recv()` can return 0 when no data is available, not just when the connection is closed.

## Fix Applied
1. **Improved recv() handling** - Properly distinguish between "no data" and "connection closed"
2. **Added poll() checks** - Use POLLHUP and POLLERR to detect true disconnection
3. **Better error handling** - Handle EAGAIN/EWOULDBLOCK correctly for non-blocking sockets

## Testing
Use the provided test script to verify connection reuse:
```bash
# Build daemon
scons daemon=1

# Start daemon
./goxel-daemon --foreground --socket /tmp/test.sock

# In another terminal, test connection reuse
python3 test_connection_reuse.py /tmp/test.sock 10
```

## Best Practices for Robust Connection Handling

### 1. Message Framing
- Use newline delimiters (current approach)
- Consider length-prefixed messages for binary data
- Implement proper JSON boundary detection

### 2. Connection Health Monitoring
- Enable TCP keep-alive
- Implement application-level heartbeats
- Use poll() for connection state detection

### 3. Timeout Management
- Set socket timeouts for recv/send operations
- Implement idle connection cleanup
- Handle partial message timeouts

### 4. Error Recovery
- Gracefully handle EAGAIN/EWOULDBLOCK
- Implement reconnection logic in clients
- Log connection issues for debugging

## Next Steps
1. Test the fix thoroughly with multiple concurrent clients
2. Add connection statistics to monitor reuse effectiveness
3. Consider implementing connection pooling for better performance
4. Add unit tests for connection handling edge cases