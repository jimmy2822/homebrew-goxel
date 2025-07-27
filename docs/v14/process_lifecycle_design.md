# Goxel v14.6 Process Lifecycle Management Design

## Overview

This document details the process lifecycle management system for the Goxel v14.6 daemon, including startup, shutdown, signal handling, and state transitions.

## State Machine Design

```
                    ┌─────────────┐
                    │   STOPPED   │
                    └──────┬──────┘
                           │ daemon_start()
                    ┌──────▼──────┐
                    │  STARTING   │
                    └──────┬──────┘
                           │ initialization complete
                    ┌──────▼──────┐
           ┌────────┤   RUNNING   ├────────┐
           │        └──────┬──────┘        │
           │               │               │
     SIGHUP│               │SIGTERM/SIGINT │
           │        ┌──────▼──────┐        │
           └────────┤  STOPPING   │        │
                    └──────┬──────┘        │
                           │               │
                    ┌──────▼──────┐        │
                    │   STOPPED   │        │
                    └─────────────┘        │
                                          │
                    ┌─────────────┐        │
                    │  RELOADING  ├────────┘
                    └─────────────┘
```

## Startup Sequence

### Phase 1: Environment Validation

```c
typedef struct startup_context {
    daemon_full_config_t *config;
    char error_buffer[4096];
    int phase;
    bool test_mode;
} startup_context_t;

daemon_error_t daemon_startup_phase1(startup_context_t *ctx) {
    // 1. Validate runtime environment
    if (!validate_system_requirements()) {
        return DAEMON_ERROR_INVALID_ENVIRONMENT;
    }
    
    // 2. Check for existing daemon
    if (daemon_already_running(ctx->config->process.pid_file)) {
        return DAEMON_ERROR_ALREADY_RUNNING;
    }
    
    // 3. Validate configuration
    if (!daemon_config_validate(ctx->config, ctx->error_buffer, 
                               sizeof(ctx->error_buffer))) {
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    // 4. Create required directories
    if (!create_runtime_directories(ctx->config)) {
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    
    return DAEMON_SUCCESS;
}
```

### Phase 2: Process Setup

```c
daemon_error_t daemon_startup_phase2(startup_context_t *ctx) {
    // 1. Set resource limits
    if (!set_resource_limits(&ctx->config->limits)) {
        return DAEMON_ERROR_RESOURCE_LIMIT_FAILED;
    }
    
    // 2. Setup signal handling (pre-fork)
    if (!setup_prefork_signals()) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    // 3. Initialize logging
    if (!logging_initialize(&ctx->config->logging)) {
        return DAEMON_ERROR_LOGGING_INIT_FAILED;
    }
    
    // 4. Create PID file (with lock)
    if (ctx->config->process.create_pid_file) {
        int pid_fd = create_pid_file(ctx->config->process.pid_file);
        if (pid_fd < 0) {
            return DAEMON_ERROR_PID_FILE_CREATE_FAILED;
        }
        // Keep fd open for lock
    }
    
    return DAEMON_SUCCESS;
}
```

### Phase 3: Daemonization

```c
daemon_error_t daemon_startup_phase3(startup_context_t *ctx) {
    if (ctx->config->process.daemonize && !ctx->test_mode) {
        // 1. First fork
        pid_t pid = fork();
        if (pid < 0) {
            return DAEMON_ERROR_FORK_FAILED;
        }
        if (pid > 0) {
            // Parent exits
            _exit(EXIT_SUCCESS);
        }
        
        // 2. Create new session
        if (setsid() < 0) {
            return DAEMON_ERROR_SETSID_FAILED;
        }
        
        // 3. Second fork (prevent TTY acquisition)
        pid = fork();
        if (pid < 0) {
            return DAEMON_ERROR_FORK_FAILED;
        }
        if (pid > 0) {
            _exit(EXIT_SUCCESS);
        }
        
        // 4. Update PID file with new PID
        update_pid_file(ctx->config->process.pid_file, getpid());
        
        // 5. Change working directory
        if (chdir(ctx->config->process.working_directory) < 0) {
            return DAEMON_ERROR_CHDIR_FAILED;
        }
        
        // 6. Redirect standard file descriptors
        redirect_stdio_to_null();
        
        // 7. Set umask
        umask(ctx->config->security.umask);
    }
    
    return DAEMON_SUCCESS;
}
```

### Phase 4: Privilege Management

```c
daemon_error_t daemon_startup_phase4(startup_context_t *ctx) {
    // 1. Resolve user/group names
    if (!daemon_config_resolve_ids(ctx->config)) {
        return DAEMON_ERROR_INVALID_USER_GROUP;
    }
    
    // 2. Set supplementary groups
    if (ctx->config->process.gid != 0) {
        if (initgroups(ctx->config->process.user, 
                       ctx->config->process.gid) < 0) {
            return DAEMON_ERROR_PERMISSION_DENIED;
        }
    }
    
    // 3. Apply security settings
    if (ctx->config->security.enable_chroot) {
        if (chroot(ctx->config->security.chroot_directory) < 0) {
            return DAEMON_ERROR_CHROOT_FAILED;
        }
        if (chdir("/") < 0) {
            return DAEMON_ERROR_CHDIR_FAILED;
        }
    }
    
    // 4. Drop privileges
    daemon_error_t err = daemon_drop_privileges(
        ctx->config->process.uid,
        ctx->config->process.gid
    );
    if (err != DAEMON_SUCCESS) {
        return err;
    }
    
    // 5. Drop capabilities (Linux)
    #ifdef __linux__
    if (ctx->config->security.drop_capabilities) {
        drop_capabilities(ctx->config->security.allowed_capabilities);
    }
    #endif
    
    return DAEMON_SUCCESS;
}
```

### Phase 5: Core Initialization

```c
daemon_error_t daemon_startup_phase5(startup_context_t *ctx,
                                     daemon_context_t *daemon) {
    // 1. Setup final signal handlers
    if (!setup_daemon_signals(daemon)) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    // 2. Initialize Goxel core
    daemon->goxel_instance = goxel_headless_create();
    if (!daemon->goxel_instance) {
        return DAEMON_ERROR_GOXEL_INIT_FAILED;
    }
    
    // 3. Initialize thread pool
    daemon->thread_pool = thread_pool_create(ctx->config->workers.count);
    if (!daemon->thread_pool) {
        return DAEMON_ERROR_THREAD_POOL_INIT_FAILED;
    }
    
    // 4. Initialize request queue
    daemon->request_queue = request_queue_create(
        ctx->config->workers.queue_size
    );
    if (!daemon->request_queue) {
        return DAEMON_ERROR_QUEUE_INIT_FAILED;
    }
    
    // 5. Create sockets
    if (ctx->config->sockets.unix.enabled) {
        daemon->unix_socket = create_unix_socket(
            &ctx->config->sockets.unix
        );
        if (daemon->unix_socket < 0) {
            return DAEMON_ERROR_SOCKET_CREATE_FAILED;
        }
    }
    
    if (ctx->config->sockets.tcp.enabled) {
        daemon->tcp_socket = create_tcp_socket(
            &ctx->config->sockets.tcp
        );
        if (daemon->tcp_socket < 0) {
            return DAEMON_ERROR_SOCKET_CREATE_FAILED;
        }
    }
    
    // 6. Start health monitor
    if (!start_health_monitor(daemon, &ctx->config->health)) {
        return DAEMON_ERROR_HEALTH_MONITOR_FAILED;
    }
    
    // 7. Update state
    daemon_set_state(daemon, DAEMON_STATE_RUNNING);
    
    return DAEMON_SUCCESS;
}
```

## Shutdown Sequence

### Graceful Shutdown

```c
typedef enum {
    SHUTDOWN_PHASE_INIT,
    SHUTDOWN_PHASE_STOP_ACCEPTING,
    SHUTDOWN_PHASE_DRAIN_REQUESTS,
    SHUTDOWN_PHASE_CLOSE_CONNECTIONS,
    SHUTDOWN_PHASE_STOP_WORKERS,
    SHUTDOWN_PHASE_CLEANUP_RESOURCES,
    SHUTDOWN_PHASE_COMPLETE
} shutdown_phase_t;

daemon_error_t daemon_graceful_shutdown(daemon_context_t *daemon) {
    daemon_set_state(daemon, DAEMON_STATE_STOPPING);
    
    // Phase 1: Stop accepting new connections
    if (daemon->unix_socket >= 0) {
        shutdown(daemon->unix_socket, SHUT_RD);
    }
    if (daemon->tcp_socket >= 0) {
        shutdown(daemon->tcp_socket, SHUT_RD);
    }
    
    // Phase 2: Signal workers to finish
    thread_pool_shutdown_begin(daemon->thread_pool);
    
    // Phase 3: Wait for active requests (with timeout)
    struct timespec timeout = {
        .tv_sec = daemon->config.performance.shutdown_timeout_ms / 1000,
        .tv_nsec = (daemon->config.performance.shutdown_timeout_ms % 1000) * 1000000
    };
    
    if (!wait_for_requests_completion(daemon, &timeout)) {
        LOG_WARN("Timeout waiting for requests, forcing shutdown");
    }
    
    // Phase 4: Close all client connections
    close_all_connections(daemon);
    
    // Phase 5: Stop worker threads
    thread_pool_destroy(daemon->thread_pool);
    
    // Phase 6: Cleanup resources
    cleanup_daemon_resources(daemon);
    
    // Phase 7: Remove PID file
    if (daemon->config.process.create_pid_file) {
        unlink(daemon->config.process.pid_file);
    }
    
    daemon_set_state(daemon, DAEMON_STATE_STOPPED);
    
    return DAEMON_SUCCESS;
}
```

### Emergency Shutdown

```c
void daemon_emergency_shutdown(int exit_code) {
    // Minimal cleanup for crash scenarios
    static bool emergency_shutdown_in_progress = false;
    
    if (emergency_shutdown_in_progress) {
        // Prevent recursion
        _exit(exit_code);
    }
    emergency_shutdown_in_progress = true;
    
    // Try to log the shutdown
    LOG_ERROR("Emergency shutdown initiated, exit code: %d", exit_code);
    
    // Remove PID file if possible
    if (g_daemon_context && g_daemon_context->config.process.pid_file) {
        unlink(g_daemon_context->config.process.pid_file);
    }
    
    // Force exit
    _exit(exit_code);
}
```

## Signal Handling

### Signal Handler Implementation

```c
// Global daemon context for signal handlers
static daemon_context_t *g_daemon_context = NULL;

// Signal handler
static void daemon_signal_handler(int signum) {
    if (!g_daemon_context) return;
    
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            // Graceful shutdown
            daemon_request_shutdown(g_daemon_context);
            break;
            
        case SIGHUP:
            // Reload configuration
            daemon_request_reload(g_daemon_context);
            break;
            
        case SIGUSR1:
            // Dump statistics
            daemon_dump_statistics(g_daemon_context);
            break;
            
        case SIGUSR2:
            // Toggle debug logging
            daemon_toggle_debug_logging(g_daemon_context);
            break;
            
        case SIGPIPE:
            // Ignore broken pipe
            break;
            
        default:
            LOG_WARN("Unexpected signal received: %d", signum);
    }
}

// Setup signal handling
daemon_error_t setup_daemon_signals(daemon_context_t *daemon) {
    g_daemon_context = daemon;
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = daemon_signal_handler;
    sigemptyset(&sa.sa_mask);
    
    // Block all signals during handler execution
    sigfillset(&sa.sa_mask);
    
    // Install handlers
    struct {
        int signum;
        int flags;
    } signals[] = {
        {SIGTERM, SA_RESTART},
        {SIGINT,  SA_RESTART},
        {SIGHUP,  SA_RESTART},
        {SIGUSR1, SA_RESTART},
        {SIGUSR2, SA_RESTART},
        {SIGPIPE, SA_RESTART},
        {0, 0}
    };
    
    for (int i = 0; signals[i].signum != 0; i++) {
        sa.sa_flags = signals[i].flags;
        if (sigaction(signals[i].signum, &sa, NULL) < 0) {
            return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
        }
    }
    
    // Block SIGCHLD to prevent zombies
    signal(SIGCHLD, SIG_IGN);
    
    return DAEMON_SUCCESS;
}
```

### Configuration Reload (SIGHUP)

```c
daemon_error_t daemon_reload_configuration(daemon_context_t *daemon) {
    LOG_INFO("Configuration reload requested");
    
    // Load new configuration
    daemon_full_config_t *new_config = daemon_config_reload(
        daemon->config,
        daemon->config->config_file_path
    );
    
    if (!new_config) {
        LOG_ERROR("Failed to load new configuration");
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    // Validate new configuration
    char errors[4096];
    if (!daemon_config_validate(new_config, errors, sizeof(errors))) {
        LOG_ERROR("Invalid configuration: %s", errors);
        daemon_config_free(new_config);
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    // Find differences
    char **changes = daemon_config_diff(daemon->config, new_config);
    
    // Apply safe changes
    bool needs_restart = false;
    for (int i = 0; changes[i] != NULL; i++) {
        if (is_runtime_changeable(changes[i])) {
            apply_config_change(daemon, changes[i], new_config);
        } else {
            LOG_WARN("Change to %s requires restart", changes[i]);
            needs_restart = true;
        }
    }
    
    // Update configuration
    daemon_config_free(daemon->config);
    daemon->config = new_config;
    
    if (needs_restart) {
        LOG_INFO("Some changes require daemon restart");
    }
    
    return DAEMON_SUCCESS;
}
```

## State Management

### State Transitions

```c
typedef struct state_transition {
    daemon_state_t from;
    daemon_state_t to;
    bool (*validator)(daemon_context_t *daemon);
    daemon_error_t (*handler)(daemon_context_t *daemon);
} state_transition_t;

static state_transition_t valid_transitions[] = {
    {DAEMON_STATE_STOPPED,  DAEMON_STATE_STARTING, NULL, handle_start},
    {DAEMON_STATE_STARTING, DAEMON_STATE_RUNNING,  validate_started, NULL},
    {DAEMON_STATE_RUNNING,  DAEMON_STATE_STOPPING, NULL, handle_stop},
    {DAEMON_STATE_STOPPING, DAEMON_STATE_STOPPED,  NULL, NULL},
    {DAEMON_STATE_RUNNING,  DAEMON_STATE_RELOADING, NULL, handle_reload},
    {DAEMON_STATE_RELOADING, DAEMON_STATE_RUNNING, NULL, NULL},
    {DAEMON_STATE_ANY,      DAEMON_STATE_ERROR,    NULL, handle_error},
    {0, 0, NULL, NULL}
};

daemon_error_t daemon_transition_state(daemon_context_t *daemon,
                                      daemon_state_t new_state) {
    pthread_mutex_lock(&daemon->state_mutex);
    
    daemon_state_t current = daemon->state;
    
    // Find valid transition
    state_transition_t *transition = NULL;
    for (int i = 0; valid_transitions[i].from != 0; i++) {
        if ((valid_transitions[i].from == current ||
             valid_transitions[i].from == DAEMON_STATE_ANY) &&
            valid_transitions[i].to == new_state) {
            transition = &valid_transitions[i];
            break;
        }
    }
    
    if (!transition) {
        pthread_mutex_unlock(&daemon->state_mutex);
        LOG_ERROR("Invalid state transition: %s -> %s",
                  state_to_string(current),
                  state_to_string(new_state));
        return DAEMON_ERROR_INVALID_STATE_TRANSITION;
    }
    
    // Validate transition
    if (transition->validator && !transition->validator(daemon)) {
        pthread_mutex_unlock(&daemon->state_mutex);
        return DAEMON_ERROR_TRANSITION_FAILED;
    }
    
    // Update state
    daemon->state = new_state;
    daemon->state_change_time = time(NULL);
    
    pthread_mutex_unlock(&daemon->state_mutex);
    
    // Execute transition handler
    if (transition->handler) {
        return transition->handler(daemon);
    }
    
    return DAEMON_SUCCESS;
}
```

## Process Monitoring

### Health Checks

```c
typedef struct health_status {
    bool is_healthy;
    time_t last_check;
    time_t last_success;
    int consecutive_failures;
    char last_error[256];
} health_status_t;

void* health_monitor_thread(void *arg) {
    daemon_context_t *daemon = (daemon_context_t*)arg;
    health_status_t status = {0};
    
    while (!daemon->shutdown_requested) {
        // Perform health checks
        bool healthy = true;
        
        // Check 1: Process responsiveness
        if (!check_process_responsive(daemon)) {
            healthy = false;
            snprintf(status.last_error, sizeof(status.last_error),
                    "Process not responsive");
        }
        
        // Check 2: Memory usage
        size_t mem_usage = get_memory_usage();
        if (mem_usage > daemon->config.limits.max_memory_mb * 1024 * 1024) {
            healthy = false;
            snprintf(status.last_error, sizeof(status.last_error),
                    "Memory limit exceeded: %zu MB", mem_usage / 1024 / 1024);
        }
        
        // Check 3: Request queue health
        if (request_queue_is_full(daemon->request_queue)) {
            healthy = false;
            snprintf(status.last_error, sizeof(status.last_error),
                    "Request queue full");
        }
        
        // Check 4: External health check script
        if (daemon->config.health.health_check_script) {
            if (!run_health_check_script(
                    daemon->config.health.health_check_script)) {
                healthy = false;
                snprintf(status.last_error, sizeof(status.last_error),
                        "External health check failed");
            }
        }
        
        // Update status
        status.last_check = time(NULL);
        if (healthy) {
            status.is_healthy = true;
            status.last_success = status.last_check;
            status.consecutive_failures = 0;
        } else {
            status.consecutive_failures++;
            
            if (status.consecutive_failures >= 3) {
                status.is_healthy = false;
                LOG_ERROR("Health check failed: %s", status.last_error);
                
                // Trigger recovery
                if (daemon->config.health.restart_on_failure) {
                    trigger_daemon_recovery(daemon);
                }
            }
        }
        
        // Update daemon health status
        update_daemon_health(daemon, &status);
        
        // Sleep until next check
        sleep_ms(daemon->config.health.check_interval_ms);
    }
    
    return NULL;
}
```

### Auto-Recovery

```c
daemon_error_t trigger_daemon_recovery(daemon_context_t *daemon) {
    static int recovery_attempts = 0;
    static time_t last_recovery = 0;
    
    time_t now = time(NULL);
    
    // Check if we're in recovery cooldown
    if (now - last_recovery < daemon->config.health.restart_delay_ms / 1000) {
        LOG_WARN("Recovery attempted too soon, skipping");
        return DAEMON_ERROR_RECOVERY_TOO_SOON;
    }
    
    // Check max attempts
    if (recovery_attempts >= daemon->config.health.max_restart_attempts) {
        LOG_ERROR("Maximum recovery attempts reached, giving up");
        daemon_emergency_shutdown(EXIT_FAILURE);
        return DAEMON_ERROR_MAX_RECOVERY_ATTEMPTS;
    }
    
    LOG_INFO("Attempting daemon recovery (attempt %d/%d)",
             recovery_attempts + 1,
             daemon->config.health.max_restart_attempts);
    
    recovery_attempts++;
    last_recovery = now;
    
    // Try graceful restart
    daemon_error_t err = daemon_restart(daemon);
    
    if (err == DAEMON_SUCCESS) {
        LOG_INFO("Daemon recovery successful");
        recovery_attempts = 0;  // Reset counter on success
    } else {
        LOG_ERROR("Daemon recovery failed: %s", 
                  daemon_error_string(err));
    }
    
    return err;
}
```

## Integration Examples

### systemd Service

```ini
[Unit]
Description=Goxel 3D Voxel Editor Daemon
After=network.target

[Service]
Type=forking
PIDFile=/var/run/goxel/goxel.pid
ExecStart=/usr/bin/goxel-daemon --config /etc/goxel/daemon.conf
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/bin/kill -TERM $MAINPID
Restart=on-failure
RestartSec=5
User=goxel
Group=goxel

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/goxel /var/log/goxel

[Install]
WantedBy=multi-user.target
```

### launchd plist (macOS)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" 
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.goxel.daemon</string>
    
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/goxel-daemon</string>
        <string>--config</string>
        <string>/usr/local/etc/goxel/daemon.conf</string>
    </array>
    
    <key>RunAtLoad</key>
    <true/>
    
    <key>KeepAlive</key>
    <dict>
        <key>SuccessfulExit</key>
        <false/>
    </dict>
    
    <key>WorkingDirectory</key>
    <string>/var/lib/goxel</string>
    
    <key>StandardOutPath</key>
    <string>/var/log/goxel/daemon.log</string>
    
    <key>StandardErrorPath</key>
    <string>/var/log/goxel/daemon.log</string>
</dict>
</plist>
```

## Testing Strategy

### Unit Tests

```c
// Test daemon lifecycle
void test_daemon_lifecycle(void) {
    daemon_full_config_t *config = daemon_config_create_default();
    config->process.daemonize = false;  // Test mode
    config->test_mode = true;
    
    daemon_context_t *daemon = daemon_context_create(config);
    assert(daemon != NULL);
    
    // Test startup
    assert(daemon_start(daemon) == DAEMON_SUCCESS);
    assert(daemon_get_state(daemon) == DAEMON_STATE_RUNNING);
    
    // Test shutdown
    assert(daemon_shutdown(daemon) == DAEMON_SUCCESS);
    assert(daemon_get_state(daemon) == DAEMON_STATE_STOPPED);
    
    daemon_context_destroy(daemon);
    daemon_config_free(config);
}
```

### Integration Tests

```bash
#!/bin/bash
# Test daemon integration

# Start daemon
./goxel-daemon --test-mode --config test.conf &
DAEMON_PID=$!

# Wait for startup
sleep 2

# Test health check
curl -s http://localhost:7890/health | jq .

# Test signal handling
kill -HUP $DAEMON_PID
sleep 1

# Test graceful shutdown
kill -TERM $DAEMON_PID
wait $DAEMON_PID
```

## Conclusion

This process lifecycle management design provides a robust foundation for the Goxel v14.6 daemon, with comprehensive startup/shutdown sequences, signal handling, health monitoring, and recovery mechanisms. The design ensures reliable operation in production environments while maintaining flexibility for different deployment scenarios.