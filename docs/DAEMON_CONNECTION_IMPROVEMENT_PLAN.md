# Daemon Connection Improvement Plan

**Date**: 2025-01-31  
**Issue**: Goxel daemon crashes after 3-4 operations due to protocol handler conflicts  
**Goal**: Achieve stable, long-running daemon connections for complex operations

## Executive Summary

The goxel-daemon v14.0 experiences connection failures after 3-4 operations due to a fundamental conflict between binary and JSON message handlers. This plan outlines immediate fixes and long-term improvements to ensure stable daemon operation for enterprise deployments.

## Root Cause Analysis

### 1. Protocol Handler Conflict
```
Binary Handler (socket_server.c)      JSON Handler (json_socket_handler.c)
├── Expects 16-byte header           ├── Expects raw JSON
├── Reads fixed-size chunks          ├── Reads character-by-character
└── Parses network byte order        └── Tracks JSON structure
```

**Problem**: Both handlers attempt to read from the same socket, causing:
- Binary parser interprets `{"method":...}` as malformed headers
- JSON characters consumed by binary parser
- Buffer corruption after ~3-4 attempts
- Daemon crash or EPIPE error

### 2. Socket Path Inconsistency
```
Initial connection:    /opt/homebrew/var/run/goxel/goxel.sock
After crash fallback: /tmp/goxel-daemon.sock
```

### 3. Missing Connection Lifecycle Management
- No health checks or keepalive
- No automatic reconnection
- No connection pooling

## Immediate Fixes (Phase 1)

### Fix 1: Protocol-Specific Client Handling
**File**: `src/daemon/socket_server.c`

```c
// Add protocol detection to client structure
struct socket_client {
    int fd;
    uint32_t id;
    protocol_mode_t protocol;  // NEW: Track client protocol
    union {
        struct {
            char *buffer;
            size_t buffer_size;
            size_t buffer_capacity;
        } binary;
        struct {
            pthread_t monitor_thread;
            bool monitor_running;
        } json;
    } handler_data;  // NEW: Protocol-specific data
};

// Modify handle_client_connection to detect protocol first
static socket_error_t handle_client_connection(socket_server_t *server, 
                                              socket_client_t *client)
{
    // Peek at first 4 bytes to detect protocol
    char magic[4];
    ssize_t peeked = recv(client->fd, magic, 4, MSG_PEEK);
    
    if (peeked >= 4 && magic[0] == '{' && magic[1] == '"') {
        client->protocol = PROTOCOL_JSON_RPC;
        return handle_json_client(server, client);
    } else {
        client->protocol = PROTOCOL_BINARY;
        return handle_binary_client(server, client);
    }
}
```

### Fix 2: Separate Message Handlers
**File**: `src/daemon/socket_server.c`

```c
// Split binary and JSON handling completely
static socket_error_t handle_binary_client(socket_server_t *server,
                                          socket_client_t *client)
{
    // Existing binary protocol code
    while (server->running) {
        socket_message_t *message = NULL;
        socket_error_t result = read_binary_message_from_client(client, &message);
        // ... existing code ...
    }
}

static socket_error_t handle_json_client(socket_server_t *server,
                                        socket_client_t *client)
{
    // Delegate entirely to JSON handler
    json_socket_client_handler(server, client, true, server->config.user_data);
    
    // Wait for monitor thread to finish
    if (client->handler_data.json.monitor_thread) {
        pthread_join(client->handler_data.json.monitor_thread, NULL);
    }
    
    return SOCKET_SUCCESS;
}
```

### Fix 3: Fix JSON Monitor Thread
**File**: `src/daemon/json_socket_handler.c`

```c
// Ensure monitor thread handles ALL I/O for JSON clients
static void *json_client_monitor_thread(void *arg)
{
    client_monitor_data_t *data = (client_monitor_data_t *)arg;
    char buffer[65536];
    
    // Set socket to non-blocking for better control
    int flags = fcntl(data->client->fd, F_GETFL, 0);
    fcntl(data->client->fd, F_SETFL, flags | O_NONBLOCK);
    
    while (data->running && data->server && 
           socket_server_is_running(data->server)) {
        
        // Read complete JSON message
        int result = read_json_line(data->client->fd, buffer, sizeof(buffer));
        
        if (result < 0) {
            if (result == -1) {  // EAGAIN/EWOULDBLOCK
                usleep(10000);  // 10ms
                continue;
            }
            break;  // Error or connection closed
        }
        
        // Process JSON message...
        // Send response directly (no binary wrapper)
        send(data->client->fd, json_response, strlen(json_response), MSG_NOSIGNAL);
        send(data->client->fd, "\n", 1, MSG_NOSIGNAL);
    }
}
```

## Connection Stability Improvements (Phase 2)

### Improvement 1: Socket Path Consistency
**File**: `src/daemon/daemon_main.c`

```c
// Add socket path validation and persistence
static const char *get_persistent_socket_path(const char *requested_path)
{
    static char persistent_path[256];
    
    if (requested_path && strlen(requested_path) > 0) {
        strncpy(persistent_path, requested_path, sizeof(persistent_path) - 1);
    } else {
        // Use Homebrew path if available
        if (access("/opt/homebrew/var/run/goxel", F_OK) == 0) {
            strcpy(persistent_path, "/opt/homebrew/var/run/goxel/goxel.sock");
        } else {
            strcpy(persistent_path, DEFAULT_SOCKET_PATH);
        }
    }
    
    return persistent_path;
}
```

### Improvement 2: Health Monitoring
**File**: `src/daemon/health_monitor.c` (NEW)

```c
typedef struct {
    pthread_t thread;
    bool running;
    concurrent_daemon_t *daemon;
    int check_interval_sec;
} health_monitor_t;

static void *health_monitor_thread(void *arg)
{
    health_monitor_t *monitor = (health_monitor_t *)arg;
    
    while (monitor->running) {
        // Check daemon health
        daemon_health_t health = {
            .memory_usage = get_memory_usage(),
            .active_connections = socket_server_get_connection_count(
                monitor->daemon->socket_server),
            .worker_pool_healthy = worker_pool_is_healthy(
                monitor->daemon->worker_pool),
            .uptime_seconds = time(NULL) - monitor->daemon->start_time
        };
        
        // Log health status
        LOG_I("Daemon health: memory=%zu MB, connections=%d, workers=%s, uptime=%lds",
              health.memory_usage / 1024 / 1024,
              health.active_connections,
              health.worker_pool_healthy ? "healthy" : "degraded",
              health.uptime_seconds);
        
        // Write health file for external monitoring
        write_health_file("/opt/homebrew/var/run/goxel/health.json", &health);
        
        sleep(monitor->check_interval_sec);
    }
    
    return NULL;
}
```

### Improvement 3: Connection Pooling (MCP Side)
**File**: `goxel-mcp/src/daemon-client/connection_pool.ts`

```typescript
export class DaemonConnectionPool {
    private connections: Map<string, DaemonConnection> = new Map();
    private maxConnections: number = 5;
    private healthCheckInterval: number = 30000; // 30 seconds
    
    async getConnection(): Promise<DaemonConnection> {
        // Find healthy connection
        for (const [id, conn] of this.connections) {
            if (await conn.isHealthy()) {
                return conn;
            }
        }
        
        // Create new connection if under limit
        if (this.connections.size < this.maxConnections) {
            const conn = await this.createConnection();
            this.connections.set(conn.id, conn);
            return conn;
        }
        
        // Wait for available connection
        return this.waitForConnection();
    }
    
    private async createConnection(): Promise<DaemonConnection> {
        const socketPath = process.env.GOXEL_SOCKET_PATH || 
                          '/opt/homebrew/var/run/goxel/goxel.sock';
        
        const conn = new DaemonConnection(socketPath);
        
        // Set up error recovery
        conn.on('error', (err) => {
            if (err.code === 'EPIPE' || err.code === 'ECONNRESET') {
                this.handleConnectionError(conn);
            }
        });
        
        // Set up health monitoring
        conn.startHealthCheck(this.healthCheckInterval);
        
        await conn.connect();
        return conn;
    }
    
    private async handleConnectionError(conn: DaemonConnection) {
        this.connections.delete(conn.id);
        
        // Attempt to recreate connection
        try {
            const newConn = await this.createConnection();
            this.connections.set(newConn.id, newConn);
        } catch (err) {
            console.error('Failed to recreate connection:', err);
        }
    }
}
```

## Implementation Steps

### Phase 1: Immediate Fixes (1-2 days)
1. **Day 1**:
   - [ ] Implement protocol detection in socket_server.c
   - [ ] Split binary and JSON handlers
   - [ ] Fix JSON monitor thread to handle all I/O
   - [ ] Test with simple operations

2. **Day 2**:
   - [ ] Fix socket path consistency
   - [ ] Add connection error logging
   - [ ] Test with complex operations (1000+ voxels)
   - [ ] Update daemon documentation

### Phase 2: Stability Improvements (3-5 days)
3. **Day 3**:
   - [ ] Implement health monitoring system
   - [ ] Add daemon health endpoints
   - [ ] Create health check scripts

4. **Day 4**:
   - [ ] Implement MCP connection pooling
   - [ ] Add automatic reconnection logic
   - [ ] Add connection metrics

5. **Day 5**:
   - [ ] Integration testing
   - [ ] Performance benchmarking
   - [ ] Update deployment guides

## Testing Plan

### Unit Tests
```bash
# Test protocol detection
make test-protocol-detection

# Test connection lifecycle
make test-connection-lifecycle

# Test error recovery
make test-error-recovery
```

### Integration Tests
```bash
# Test 1000+ operation stability
python3 tests/stability/test_bulk_operations.py

# Test connection recovery
python3 tests/stability/test_connection_recovery.py

# Test concurrent connections
python3 tests/stability/test_concurrent_clients.py
```

### Load Tests
```bash
# Simulate 100 concurrent clients
./tests/load/simulate_clients.sh 100

# Long-running stability test (24 hours)
./tests/stability/long_run.sh
```

## Success Metrics

1. **Stability**: Zero crashes in 24-hour test run
2. **Performance**: Handle 1000+ operations without degradation
3. **Recovery**: Automatic recovery from connection errors within 5 seconds
4. **Scalability**: Support 100+ concurrent connections

## Rollout Plan

1. **Alpha Testing**: Internal testing with test suite
2. **Beta Testing**: Limited release to power users
3. **Production Release**: Full v14.0.1 release with fixes

## Risk Mitigation

1. **Backward Compatibility**: Maintain support for existing binary protocol
2. **Rollback Plan**: Keep v14.0.0 binaries available
3. **Monitoring**: Add comprehensive logging and metrics
4. **Documentation**: Update all integration guides

## Conclusion

These improvements will transform the goxel-daemon from an unstable prototype into a production-ready service capable of handling complex, long-running operations. The phased approach ensures we can deliver immediate fixes while building toward a robust, enterprise-grade solution.

**Estimated Timeline**: 5-7 days for full implementation  
**Priority**: CRITICAL - Blocks v14.0 adoption