# Goxel v14.6 Daemon Architecture Design

## Executive Summary

This document outlines the enhanced daemon architecture for Goxel v14.6, building upon the existing v14.0 foundation. The design focuses on reliability, performance, and maintainability for long-running server processes with socket-based communication.

## Architecture Overview

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                    Goxel Daemon Process                       │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────┐    ┌──────────────────┐                │
│  │ Process Manager │    │ Signal Handler   │                 │
│  │ - PID file      │    │ - SIGTERM       │                 │
│  │ - Daemonization │    │ - SIGINT        │                 │
│  │ - Privileges    │    │ - SIGHUP        │                 │
│  └─────────────────┘    └──────────────────┘                │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Socket Communication Layer               │    │
│  ├─────────────────────────────────────────────────────┤    │
│  │  ┌─────────────────┐    ┌────────────────────┐     │    │
│  │  │ Unix Domain     │    │ TCP Socket         │     │    │
│  │  │ /tmp/goxel.sock │    │ Port 7890         │     │    │
│  │  └─────────────────┘    └────────────────────┘     │    │
│  │                                                      │    │
│  │  ┌──────────────────────────────────────────┐      │    │
│  │  │        Connection Multiplexing            │      │    │
│  │  │    epoll (Linux) / kqueue (macOS)       │      │    │
│  │  └──────────────────────────────────────────┘      │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │            Request Processing Layer                   │    │
│  ├─────────────────────────────────────────────────────┤    │
│  │  ┌─────────────┐    ┌─────────────┐    ┌────────┐  │    │
│  │  │ Worker Pool │    │Request Queue│    │ JSON   │  │    │
│  │  │ (N threads) │◄───│   (FIFO)   │◄───│  RPC   │  │    │
│  │  └─────────────┘    └─────────────┘    └────────┘  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Core Goxel Integration                   │    │
│  ├─────────────────────────────────────────────────────┤    │
│  │  ┌─────────────┐    ┌─────────────┐    ┌────────┐  │    │
│  │  │Goxel Engine │    │ State Mgmt │    │ Health │  │    │
│  │  │  Instance   │    │   (Mutex)  │    │Monitor │  │    │
│  │  └─────────────┘    └─────────────┘    └────────┘  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### Process Lifecycle Management

#### 1. Startup Sequence

```c
// Startup flow
daemon_startup() {
    1. Parse configuration
    2. Validate environment
    3. Create PID file (with lock)
    4. Setup signal handlers
    5. Initialize logging
    6. Daemonize if requested
    7. Drop privileges if configured
    8. Initialize Goxel core
    9. Create socket(s)
    10. Start worker threads
    11. Enter main event loop
}
```

#### 2. Shutdown Sequence

```c
// Graceful shutdown flow
daemon_shutdown() {
    1. Set shutdown flag
    2. Stop accepting new connections
    3. Send shutdown signal to workers
    4. Wait for active requests (timeout)
    5. Close all client connections
    6. Shutdown Goxel instance
    7. Clean up sockets
    8. Remove PID file
    9. Exit cleanly
}
```

#### 3. Signal Handling Strategy

- **SIGTERM**: Initiate graceful shutdown
- **SIGINT**: Initiate graceful shutdown (Ctrl+C)
- **SIGHUP**: Reload configuration without restart
- **SIGUSR1**: Dump current state/statistics
- **SIGUSR2**: Toggle debug logging
- **SIGPIPE**: Ignore (handle in socket layer)

### Socket Communication Strategy

#### 1. Unix Domain Socket (Primary)

```c
// Configuration
#define DEFAULT_SOCKET_PATH "/tmp/goxel.sock"
#define SOCKET_BACKLOG 128
#define MAX_CONNECTIONS 10

// Socket options
- SO_REUSEADDR: Allow quick restart
- SO_KEEPALIVE: Detect dead connections
- SO_RCVTIMEO: Receive timeout (30s)
- SO_SNDTIMEO: Send timeout (30s)
```

#### 2. TCP Socket (Optional)

```c
// TCP configuration
#define DEFAULT_TCP_PORT 7890
#define TCP_BIND_ADDR "127.0.0.1"  // localhost only by default

// Security considerations
- Bind to localhost only by default
- Optional SSL/TLS support
- Authentication token support
- Connection rate limiting
```

#### 3. Connection Multiplexing

**Linux: epoll**
```c
// High-performance event notification
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
struct epoll_event events[MAX_EVENTS];

// Edge-triggered mode for efficiency
event.events = EPOLLIN | EPOLLET;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
```

**macOS/BSD: kqueue**
```c
// Kernel event notification
int kq = kqueue();
struct kevent events[MAX_EVENTS];

// Register socket for read events
EV_SET(&event, socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
kevent(kq, &event, 1, NULL, 0, NULL);
```

### Daemon Configuration Schema

```yaml
# /etc/goxel/daemon.conf
daemon:
  # Process management
  pid_file: /var/run/goxel/goxel.pid
  working_directory: /var/lib/goxel
  user: goxel
  group: goxel
  
  # Socket configuration
  sockets:
    unix:
      enabled: true
      path: /tmp/goxel.sock
      permissions: 0666
    tcp:
      enabled: false
      address: 127.0.0.1
      port: 7890
      
  # Worker configuration
  workers:
    count: 4  # Number of worker threads
    queue_size: 1000
    
  # Performance tuning
  performance:
    max_connections: 10
    request_timeout_ms: 30000
    shutdown_timeout_ms: 10000
    
  # Health monitoring
  health:
    check_interval_ms: 5000
    restart_on_failure: true
    max_restart_attempts: 3
    restart_delay_ms: 5000
    
  # Logging
  logging:
    level: info
    file: /var/log/goxel/daemon.log
    max_size_mb: 100
    max_files: 10
    syslog: true
```

### State Management Design

#### 1. Global State Protection

```c
typedef struct daemon_state {
    // Core state
    daemon_context_t *context;
    goxel_t *goxel_instance;
    
    // Thread synchronization
    pthread_mutex_t goxel_mutex;      // Protects Goxel instance
    pthread_rwlock_t config_lock;     // Protects configuration
    pthread_mutex_t stats_mutex;      // Protects statistics
    
    // Connection tracking
    struct {
        int count;
        connection_t *list[MAX_CONNECTIONS];
        pthread_mutex_t mutex;
    } connections;
    
    // Health monitoring
    struct {
        time_t last_heartbeat;
        uint64_t total_requests;
        uint64_t failed_requests;
        bool is_healthy;
    } health;
} daemon_state_t;
```

#### 2. Thread Safety Strategy

- **Goxel Instance**: Single mutex for all Goxel operations
- **Configuration**: Read-write lock (many readers, single writer)
- **Statistics**: Atomic operations where possible, mutex for complex updates
- **Connections**: Per-connection state with minimal shared data

### Health Monitoring Implementation

#### 1. Heartbeat Mechanism

```c
void* health_monitor_thread(void* arg) {
    daemon_state_t *state = (daemon_state_t*)arg;
    
    while (!state->shutdown_requested) {
        // Check daemon health
        bool healthy = check_daemon_health(state);
        
        // Update health status
        pthread_mutex_lock(&state->stats_mutex);
        state->health.is_healthy = healthy;
        state->health.last_heartbeat = time(NULL);
        pthread_mutex_unlock(&state->stats_mutex);
        
        // Log health status
        if (!healthy) {
            log_error("Daemon health check failed");
            trigger_recovery(state);
        }
        
        sleep_ms(state->config.health.check_interval_ms);
    }
    
    return NULL;
}
```

#### 2. Auto-Restart Logic

```c
typedef struct restart_policy {
    bool enabled;
    int max_attempts;
    int current_attempts;
    int delay_ms;
    time_t last_restart;
} restart_policy_t;

void trigger_recovery(daemon_state_t *state) {
    if (!state->restart_policy.enabled) return;
    
    if (state->restart_policy.current_attempts >= 
        state->restart_policy.max_attempts) {
        log_error("Max restart attempts reached, giving up");
        exit(EXIT_FAILURE);
    }
    
    // Attempt graceful restart
    daemon_restart(state);
}
```

#### 3. Health Check Endpoints

```c
// Health check JSON RPC methods
json_t* handle_health_check(json_t *params) {
    return json_pack("{s:b, s:i, s:i}", 
        "healthy", state->health.is_healthy,
        "uptime", time(NULL) - state->start_time,
        "requests", state->health.total_requests
    );
}

json_t* handle_health_stats(json_t *params) {
    return json_pack("{s:i, s:i, s:i, s:f}",
        "total_requests", state->health.total_requests,
        "failed_requests", state->health.failed_requests,
        "active_connections", state->connections.count,
        "memory_usage_mb", get_memory_usage() / 1024.0 / 1024.0
    );
}
```

### Logging Infrastructure

#### 1. Structured Logging

```c
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

typedef struct log_entry {
    log_level_t level;
    time_t timestamp;
    pid_t pid;
    pthread_t thread_id;
    const char *file;
    int line;
    const char *function;
    char message[1024];
} log_entry_t;

// Structured logging with context
#define LOG_INFO(fmt, ...) \
    daemon_log(LOG_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
```

#### 2. Log Rotation

```c
typedef struct log_config {
    char *filename;
    size_t max_size;
    int max_files;
    bool use_syslog;
    log_level_t min_level;
} log_config_t;

void rotate_logs(log_config_t *config) {
    if (get_file_size(config->filename) > config->max_size) {
        // Rotate log files
        for (int i = config->max_files - 1; i > 0; i--) {
            char old[PATH_MAX], new[PATH_MAX];
            snprintf(old, PATH_MAX, "%s.%d", config->filename, i - 1);
            snprintf(new, PATH_MAX, "%s.%d", config->filename, i);
            rename(old, new);
        }
        
        // Move current to .0
        char backup[PATH_MAX];
        snprintf(backup, PATH_MAX, "%s.0", config->filename);
        rename(config->filename, backup);
    }
}
```

## Security Considerations

### 1. Privilege Dropping

```c
daemon_error_t daemon_drop_privileges(uid_t uid, gid_t gid) {
    // Drop supplementary groups
    if (setgroups(0, NULL) != 0) {
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    
    // Change group first
    if (gid != 0 && setgid(gid) != 0) {
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    
    // Then change user
    if (uid != 0 && setuid(uid) != 0) {
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    
    // Verify we can't regain privileges
    if (setuid(0) == 0) {
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    
    return DAEMON_SUCCESS;
}
```

### 2. Socket Permissions

- Unix socket: Configurable permissions (default 0666)
- Directory permissions: Ensure socket directory is accessible
- TCP socket: Bind to localhost only by default

### 3. Resource Limits

```c
void set_resource_limits(void) {
    struct rlimit rlim;
    
    // Limit core dumps (security)
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);
    
    // Set reasonable file descriptor limit
    rlim.rlim_cur = 1024;
    rlim.rlim_max = 4096;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    // Limit memory usage
    rlim.rlim_cur = 512 * 1024 * 1024;  // 512MB
    rlim.rlim_max = 1024 * 1024 * 1024; // 1GB
    setrlimit(RLIMIT_AS, &rlim);
}
```

## Performance Optimizations

### 1. Connection Pooling

- Pre-allocated connection structures
- Object pooling for request/response buffers
- Zero-copy where possible

### 2. Lock-Free Operations

- Atomic counters for statistics
- Lock-free queue for non-critical logging
- RCU (Read-Copy-Update) for configuration updates

### 3. Memory Management

- Custom memory pools for frequent allocations
- Buffer recycling for network I/O
- Periodic memory compaction

## Integration Points

### 1. Agent-1 → Agent-2 Interface

```c
// daemon_request_handler.h
typedef struct request_handler {
    daemon_context_t *daemon_ctx;
    json_rpc_handler_t *json_handler;
    
    // Process JSON RPC request
    char* (*process_request)(struct request_handler *handler,
                            const char *request,
                            size_t request_len);
} request_handler_t;
```

### 2. Agent-1 → Agent-3 Protocol

```c
// Socket message format
typedef struct daemon_message {
    uint32_t magic;      // 0x474F5845 ('GOXE')
    uint32_t version;    // Protocol version
    uint32_t length;     // Message length
    uint32_t flags;      // Message flags
    char data[];         // JSON RPC payload
} daemon_message_t;
```

### 3. Agent-1 → Agent-4 Hooks

```c
// Performance monitoring hooks
typedef struct perf_hooks {
    void (*request_start)(uint64_t request_id);
    void (*request_end)(uint64_t request_id, int status);
    void (*connection_opened)(int client_fd);
    void (*connection_closed)(int client_fd);
} perf_hooks_t;

// Register hooks for Agent-4's performance monitoring
void daemon_register_perf_hooks(perf_hooks_t *hooks);
```

## Error Handling Strategy

### 1. Error Propagation

- Clear error codes for all operations
- Detailed error messages in logs
- JSON RPC error responses to clients

### 2. Recovery Mechanisms

- Automatic reconnection for dropped clients
- Request retry with exponential backoff
- Graceful degradation under load

### 3. Debugging Support

- Verbose logging mode
- Request tracing
- Memory leak detection in debug builds

## Testing Considerations

### 1. Unit Testing

- Mock socket operations
- Simulate signal delivery
- Test error conditions

### 2. Integration Testing

- Multi-client scenarios
- Daemon restart testing
- Load testing

### 3. Fault Injection

- Random connection drops
- Memory pressure simulation
- Signal storm testing

## Deployment Recommendations

### 1. System Integration

- systemd service file for Linux
- launchd plist for macOS
- Windows Service support (future)

### 2. Monitoring

- Prometheus metrics endpoint
- Health check URL for load balancers
- Log aggregation support

### 3. Upgrade Path

- Graceful restart without dropping connections
- Configuration migration tools
- Backward compatibility with v14.0

## Conclusion

This daemon architecture design provides a robust foundation for Goxel v14.6's persistent server mode. The design emphasizes reliability through proper process management, performance through efficient socket handling and concurrency, and maintainability through clear interfaces and comprehensive error handling.

The architecture is designed to integrate seamlessly with the work of other agents while providing the stable, high-performance foundation required for enterprise deployments.