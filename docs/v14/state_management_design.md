# Goxel v14.6 State Management Design

## Overview

This document outlines the state management architecture for the Goxel v14.6 daemon, focusing on thread safety, consistency, and performance for concurrent operations.

## State Architecture

### Global State Structure

```c
typedef struct daemon_global_state {
    // Core components
    daemon_context_t *daemon_ctx;          // Main daemon context
    goxel_t *goxel_instance;              // Shared Goxel instance
    
    // Thread synchronization primitives
    struct {
        pthread_mutex_t goxel_mutex;       // Protects Goxel operations
        pthread_rwlock_t config_lock;      // Config read-write lock
        pthread_mutex_t stats_mutex;       // Statistics updates
        pthread_spinlock_t counter_lock;   // Fast counter updates
    } locks;
    
    // Connection management
    struct {
        connection_t *pool[MAX_CONNECTIONS];
        int active_count;
        pthread_mutex_t pool_mutex;
        pthread_cond_t pool_cond;
    } connections;
    
    // Request tracking
    struct {
        uint64_t next_request_id;
        request_t *active_requests;
        pthread_mutex_t request_mutex;
    } requests;
    
    // Performance metrics
    struct {
        atomic_uint_fast64_t total_requests;
        atomic_uint_fast64_t bytes_received;
        atomic_uint_fast64_t bytes_sent;
        time_t start_time;
    } metrics;
    
} daemon_global_state_t;
```

## Thread Safety Strategies

### 1. Goxel Instance Protection

Since Goxel is not thread-safe, we use a single mutex for all Goxel operations:

```c
// Goxel operation wrapper
typedef struct goxel_operation {
    enum {
        GOXEL_OP_CREATE_PROJECT,
        GOXEL_OP_LOAD_FILE,
        GOXEL_OP_SAVE_FILE,
        GOXEL_OP_ADD_VOXELS,
        GOXEL_OP_EXPORT,
        // ... other operations
    } type;
    
    union {
        struct { char *path; } file_op;
        struct { vec3_t pos; uint32_t color; } voxel_op;
        struct { char *format; char *path; } export_op;
    } params;
    
    json_t *result;
    daemon_error_t error;
} goxel_operation_t;

// Thread-safe Goxel operation execution
daemon_error_t execute_goxel_operation(daemon_global_state_t *state,
                                      goxel_operation_t *op) {
    pthread_mutex_lock(&state->locks.goxel_mutex);
    
    // Set operation context for error handling
    set_current_operation(op);
    
    // Execute operation
    switch (op->type) {
        case GOXEL_OP_CREATE_PROJECT:
            op->result = goxel_create_project(state->goxel_instance);
            break;
            
        case GOXEL_OP_ADD_VOXELS:
            op->result = goxel_add_voxel(state->goxel_instance,
                                        op->params.voxel_op.pos,
                                        op->params.voxel_op.color);
            break;
            
        // ... handle other operations
    }
    
    // Clear operation context
    clear_current_operation();
    
    pthread_mutex_unlock(&state->locks.goxel_mutex);
    
    return op->error;
}
```

### 2. Configuration Management

Configuration uses read-write locks for efficient concurrent reads:

```c
// Configuration access patterns
typedef struct config_accessor {
    daemon_full_config_t *config;
    pthread_rwlock_t *lock;
} config_accessor_t;

// Read configuration value
const char* config_read_value(config_accessor_t *accessor,
                             const char *path) {
    pthread_rwlock_rdlock(accessor->lock);
    const char *value = daemon_config_get(accessor->config, path);
    pthread_rwlock_unlock(accessor->lock);
    return value;
}

// Update configuration (e.g., from SIGHUP)
daemon_error_t config_update(config_accessor_t *accessor,
                            daemon_full_config_t *new_config) {
    pthread_rwlock_wrlock(accessor->lock);
    
    // Swap configurations
    daemon_full_config_t *old = accessor->config;
    accessor->config = new_config;
    
    pthread_rwlock_unlock(accessor->lock);
    
    // Free old config after all readers complete
    daemon_config_free(old);
    
    return DAEMON_SUCCESS;
}
```

### 3. Connection State Management

Each connection has its own state with minimal shared data:

```c
typedef struct connection_state {
    int fd;                          // Socket file descriptor
    uint32_t id;                     // Unique connection ID
    
    // Connection metadata
    struct {
        time_t connect_time;
        struct sockaddr_storage addr;
        pid_t client_pid;
        uid_t client_uid;
    } info;
    
    // I/O buffers (per-connection)
    struct {
        char *recv_buffer;
        size_t recv_size;
        size_t recv_capacity;
        
        char *send_buffer;
        size_t send_size;
        size_t send_capacity;
    } buffers;
    
    // Request processing
    struct {
        request_t *current_request;
        time_t request_start_time;
        bool processing;
    } request;
    
    // Statistics (lock-free)
    atomic_uint_fast64_t bytes_received;
    atomic_uint_fast64_t bytes_sent;
    atomic_uint_fast32_t requests_processed;
    
} connection_state_t;

// Connection pool operations
connection_state_t* connection_pool_acquire(daemon_global_state_t *state) {
    pthread_mutex_lock(&state->connections.pool_mutex);
    
    // Wait for available connection slot
    while (state->connections.active_count >= MAX_CONNECTIONS) {
        pthread_cond_wait(&state->connections.pool_cond,
                         &state->connections.pool_mutex);
    }
    
    // Find free slot
    connection_state_t *conn = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (state->connections.pool[i] == NULL) {
            conn = connection_create();
            state->connections.pool[i] = conn;
            state->connections.active_count++;
            break;
        }
    }
    
    pthread_mutex_unlock(&state->connections.pool_mutex);
    
    return conn;
}

void connection_pool_release(daemon_global_state_t *state,
                           connection_state_t *conn) {
    pthread_mutex_lock(&state->connections.pool_mutex);
    
    // Find and release connection
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (state->connections.pool[i] == conn) {
            state->connections.pool[i] = NULL;
            state->connections.active_count--;
            connection_destroy(conn);
            break;
        }
    }
    
    // Signal waiting threads
    pthread_cond_signal(&state->connections.pool_cond);
    
    pthread_mutex_unlock(&state->connections.pool_mutex);
}
```

## Lock Ordering and Deadlock Prevention

### Lock Hierarchy

To prevent deadlocks, locks must be acquired in this order:

```
1. config_lock (read or write)
2. pool_mutex
3. goxel_mutex
4. request_mutex
5. stats_mutex
6. counter_lock
```

### Lock Acquisition Helper

```c
typedef struct lock_set {
    bool need_config;
    bool need_pool;
    bool need_goxel;
    bool need_stats;
} lock_set_t;

daemon_error_t acquire_locks(daemon_global_state_t *state,
                            lock_set_t *locks) {
    // Acquire in hierarchy order
    if (locks->need_config) {
        pthread_rwlock_rdlock(&state->locks.config_lock);
    }
    
    if (locks->need_pool) {
        pthread_mutex_lock(&state->connections.pool_mutex);
    }
    
    if (locks->need_goxel) {
        pthread_mutex_lock(&state->locks.goxel_mutex);
    }
    
    if (locks->need_stats) {
        pthread_mutex_lock(&state->locks.stats_mutex);
    }
    
    return DAEMON_SUCCESS;
}

void release_locks(daemon_global_state_t *state, lock_set_t *locks) {
    // Release in reverse order
    if (locks->need_stats) {
        pthread_mutex_unlock(&state->locks.stats_mutex);
    }
    
    if (locks->need_goxel) {
        pthread_mutex_unlock(&state->locks.goxel_mutex);
    }
    
    if (locks->need_pool) {
        pthread_mutex_unlock(&state->connections.pool_mutex);
    }
    
    if (locks->need_config) {
        pthread_rwlock_unlock(&state->locks.config_lock);
    }
}
```

## Request Lifecycle State Machine

### Request States

```c
typedef enum {
    REQUEST_STATE_NEW,          // Just created
    REQUEST_STATE_PARSING,      // Parsing JSON RPC
    REQUEST_STATE_VALIDATED,    // Validated and queued
    REQUEST_STATE_PROCESSING,   // Being processed by worker
    REQUEST_STATE_COMPLETE,     // Processing complete
    REQUEST_STATE_SENDING,      // Sending response
    REQUEST_STATE_DONE,         // Response sent
    REQUEST_STATE_ERROR         // Error occurred
} request_state_t;

typedef struct request_state_machine {
    request_t *request;
    request_state_t current_state;
    pthread_mutex_t state_mutex;
    
    // Timing information
    struct {
        time_t created;
        time_t started;
        time_t completed;
        time_t sent;
    } timestamps;
    
    // Error information
    struct {
        daemon_error_t code;
        char message[256];
    } error;
    
} request_state_machine_t;
```

### State Transitions

```c
// Valid state transitions
static const request_state_t valid_transitions[][2] = {
    {REQUEST_STATE_NEW,        REQUEST_STATE_PARSING},
    {REQUEST_STATE_PARSING,    REQUEST_STATE_VALIDATED},
    {REQUEST_STATE_PARSING,    REQUEST_STATE_ERROR},
    {REQUEST_STATE_VALIDATED,  REQUEST_STATE_PROCESSING},
    {REQUEST_STATE_PROCESSING, REQUEST_STATE_COMPLETE},
    {REQUEST_STATE_PROCESSING, REQUEST_STATE_ERROR},
    {REQUEST_STATE_COMPLETE,   REQUEST_STATE_SENDING},
    {REQUEST_STATE_SENDING,    REQUEST_STATE_DONE},
    {REQUEST_STATE_SENDING,    REQUEST_STATE_ERROR},
    {-1, -1}  // Terminator
};

daemon_error_t request_transition(request_state_machine_t *rsm,
                                 request_state_t new_state) {
    pthread_mutex_lock(&rsm->state_mutex);
    
    // Validate transition
    bool valid = false;
    for (int i = 0; valid_transitions[i][0] != -1; i++) {
        if (valid_transitions[i][0] == rsm->current_state &&
            valid_transitions[i][1] == new_state) {
            valid = true;
            break;
        }
    }
    
    if (!valid) {
        pthread_mutex_unlock(&rsm->state_mutex);
        return DAEMON_ERROR_INVALID_STATE_TRANSITION;
    }
    
    // Update state and timestamp
    rsm->current_state = new_state;
    
    switch (new_state) {
        case REQUEST_STATE_PROCESSING:
            rsm->timestamps.started = time(NULL);
            break;
        case REQUEST_STATE_COMPLETE:
            rsm->timestamps.completed = time(NULL);
            break;
        case REQUEST_STATE_DONE:
            rsm->timestamps.sent = time(NULL);
            break;
        default:
            break;
    }
    
    pthread_mutex_unlock(&rsm->state_mutex);
    
    return DAEMON_SUCCESS;
}
```

## Performance Optimization

### 1. Lock-Free Statistics

Use atomic operations for frequently updated counters:

```c
// Increment request counter (lock-free)
void increment_request_count(daemon_global_state_t *state) {
    atomic_fetch_add(&state->metrics.total_requests, 1);
}

// Update bytes transferred (lock-free)
void update_bytes_transferred(daemon_global_state_t *state,
                             size_t bytes_in, size_t bytes_out) {
    atomic_fetch_add(&state->metrics.bytes_received, bytes_in);
    atomic_fetch_add(&state->metrics.bytes_sent, bytes_out);
}

// Read statistics snapshot
void get_statistics_snapshot(daemon_global_state_t *state,
                           daemon_stats_t *stats) {
    // Atomic reads don't need locks
    stats->total_requests = atomic_load(&state->metrics.total_requests);
    stats->bytes_received = atomic_load(&state->metrics.bytes_received);
    stats->bytes_sent = atomic_load(&state->metrics.bytes_sent);
    stats->start_time = state->metrics.start_time;
    stats->uptime = time(NULL) - stats->start_time;
    
    // Connection count needs lock
    pthread_mutex_lock(&state->connections.pool_mutex);
    stats->active_connections = state->connections.active_count;
    pthread_mutex_unlock(&state->connections.pool_mutex);
}
```

### 2. RCU for Configuration Updates

Implement Read-Copy-Update pattern for configuration changes:

```c
typedef struct rcu_config {
    daemon_full_config_t *config;
    volatile int readers;
    pthread_mutex_t update_mutex;
} rcu_config_t;

// RCU read (no locks needed)
daemon_full_config_t* rcu_config_read_begin(rcu_config_t *rcu) {
    __atomic_add_fetch(&rcu->readers, 1, __ATOMIC_SEQ_CST);
    return rcu->config;
}

void rcu_config_read_end(rcu_config_t *rcu) {
    __atomic_sub_fetch(&rcu->readers, 1, __ATOMIC_SEQ_CST);
}

// RCU update (single writer)
void rcu_config_update(rcu_config_t *rcu,
                      daemon_full_config_t *new_config) {
    pthread_mutex_lock(&rcu->update_mutex);
    
    daemon_full_config_t *old = rcu->config;
    rcu->config = new_config;
    
    // Wait for readers to finish with old config
    while (__atomic_load_n(&rcu->readers, __ATOMIC_SEQ_CST) > 0) {
        usleep(1000);  // 1ms
    }
    
    // Safe to free old config
    daemon_config_free(old);
    
    pthread_mutex_unlock(&rcu->update_mutex);
}
```

### 3. Per-CPU Data Structures

Reduce contention with per-CPU structures:

```c
typedef struct per_cpu_stats {
    // Cache line aligned to prevent false sharing
    struct {
        uint64_t requests;
        uint64_t bytes_in;
        uint64_t bytes_out;
        uint64_t errors;
        char padding[64 - 4 * sizeof(uint64_t)];
    } __attribute__((aligned(64))) stats;
} per_cpu_stats_t;

typedef struct daemon_per_cpu_data {
    int num_cpus;
    per_cpu_stats_t *cpu_stats;
} daemon_per_cpu_data_t;

// Update per-CPU statistics (no locks)
void update_cpu_stats(daemon_per_cpu_data_t *data,
                     uint64_t requests, uint64_t bytes_in,
                     uint64_t bytes_out) {
    int cpu = sched_getcpu();
    if (cpu >= 0 && cpu < data->num_cpus) {
        data->cpu_stats[cpu].stats.requests += requests;
        data->cpu_stats[cpu].stats.bytes_in += bytes_in;
        data->cpu_stats[cpu].stats.bytes_out += bytes_out;
    }
}

// Aggregate statistics from all CPUs
void aggregate_cpu_stats(daemon_per_cpu_data_t *data,
                        daemon_stats_t *total) {
    memset(total, 0, sizeof(*total));
    
    for (int i = 0; i < data->num_cpus; i++) {
        total->total_requests += data->cpu_stats[i].stats.requests;
        total->bytes_received += data->cpu_stats[i].stats.bytes_in;
        total->bytes_sent += data->cpu_stats[i].stats.bytes_out;
        total->total_errors += data->cpu_stats[i].stats.errors;
    }
}
```

## Memory Management

### Object Pools

Pre-allocate frequently used objects:

```c
typedef struct object_pool {
    void **objects;
    size_t capacity;
    size_t available;
    pthread_mutex_t mutex;
    
    // Object lifecycle callbacks
    void* (*create)(void);
    void (*reset)(void *obj);
    void (*destroy)(void *obj);
} object_pool_t;

// Request object pool
static object_pool_t request_pool = {
    .capacity = 1000,
    .create = request_create,
    .reset = request_reset,
    .destroy = request_destroy
};

// Acquire object from pool
void* pool_acquire(object_pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);
    
    void *obj = NULL;
    if (pool->available > 0) {
        obj = pool->objects[--pool->available];
        pool->reset(obj);
    } else {
        obj = pool->create();
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return obj;
}

// Release object back to pool
void pool_release(object_pool_t *pool, void *obj) {
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->available < pool->capacity) {
        pool->objects[pool->available++] = obj;
    } else {
        pool->destroy(obj);
    }
    
    pthread_mutex_unlock(&pool->mutex);
}
```

## Error Recovery

### State Consistency Checks

```c
typedef struct state_validator {
    bool (*validate)(daemon_global_state_t *state);
    void (*repair)(daemon_global_state_t *state);
    const char *name;
} state_validator_t;

static state_validator_t validators[] = {
    {validate_connection_pool, repair_connection_pool, "connection_pool"},
    {validate_request_queue, repair_request_queue, "request_queue"},
    {validate_goxel_state, repair_goxel_state, "goxel_state"},
    {NULL, NULL, NULL}
};

// Periodic state validation
void validate_daemon_state(daemon_global_state_t *state) {
    for (int i = 0; validators[i].validate != NULL; i++) {
        if (!validators[i].validate(state)) {
            LOG_ERROR("State validation failed: %s", validators[i].name);
            
            if (validators[i].repair) {
                LOG_INFO("Attempting state repair: %s", validators[i].name);
                validators[i].repair(state);
            }
        }
    }
}
```

## Monitoring and Debugging

### State Dump

```c
void dump_daemon_state(daemon_global_state_t *state, FILE *output) {
    fprintf(output, "=== Daemon State Dump ===\n");
    fprintf(output, "Time: %s\n", get_timestamp_string());
    
    // Connection state
    pthread_mutex_lock(&state->connections.pool_mutex);
    fprintf(output, "\nConnections:\n");
    fprintf(output, "  Active: %d/%d\n", 
            state->connections.active_count, MAX_CONNECTIONS);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connection_state_t *conn = state->connections.pool[i];
        if (conn) {
            fprintf(output, "  [%d] fd=%d, id=%u, requests=%u\n",
                    i, conn->fd, conn->id,
                    atomic_load(&conn->requests_processed));
        }
    }
    pthread_mutex_unlock(&state->connections.pool_mutex);
    
    // Request queue state
    pthread_mutex_lock(&state->requests.request_mutex);
    fprintf(output, "\nRequests:\n");
    fprintf(output, "  Next ID: %lu\n", state->requests.next_request_id);
    fprintf(output, "  Active requests: %d\n", 
            count_active_requests(state->requests.active_requests));
    pthread_mutex_unlock(&state->requests.request_mutex);
    
    // Performance metrics
    daemon_stats_t stats;
    get_statistics_snapshot(state, &stats);
    fprintf(output, "\nMetrics:\n");
    fprintf(output, "  Total requests: %lu\n", stats.total_requests);
    fprintf(output, "  Bytes in/out: %lu/%lu\n", 
            stats.bytes_received, stats.bytes_sent);
    fprintf(output, "  Uptime: %ld seconds\n", stats.uptime);
    
    fprintf(output, "======================\n");
}
```

### Lock Contention Monitoring

```c
#ifdef DEBUG_LOCKS
typedef struct lock_stats {
    const char *name;
    atomic_uint_fast64_t acquisitions;
    atomic_uint_fast64_t contentions;
    atomic_uint_fast64_t wait_time_us;
} lock_stats_t;

static lock_stats_t lock_stats[] = {
    {"goxel_mutex", 0, 0, 0},
    {"config_lock", 0, 0, 0},
    {"pool_mutex", 0, 0, 0},
    {"stats_mutex", 0, 0, 0},
    {NULL, 0, 0, 0}
};

// Instrumented lock acquisition
int instrumented_mutex_lock(pthread_mutex_t *mutex, const char *name) {
    lock_stats_t *stats = find_lock_stats(name);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int result = pthread_mutex_trylock(mutex);
    if (result == EBUSY) {
        atomic_fetch_add(&stats->contentions, 1);
        result = pthread_mutex_lock(mutex);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t wait_us = (end.tv_sec - start.tv_sec) * 1000000 +
                       (end.tv_nsec - start.tv_nsec) / 1000;
    
    atomic_fetch_add(&stats->acquisitions, 1);
    atomic_fetch_add(&stats->wait_time_us, wait_us);
    
    return result;
}

// Report lock statistics
void report_lock_stats(FILE *output) {
    fprintf(output, "\nLock Statistics:\n");
    fprintf(output, "%-20s %10s %10s %10s %10s\n",
            "Lock", "Acquired", "Contended", "Cont%", "Avg Wait(us)");
    
    for (int i = 0; lock_stats[i].name != NULL; i++) {
        uint64_t acq = atomic_load(&lock_stats[i].acquisitions);
        uint64_t cont = atomic_load(&lock_stats[i].contentions);
        uint64_t wait = atomic_load(&lock_stats[i].wait_time_us);
        
        double cont_pct = acq > 0 ? (double)cont / acq * 100 : 0;
        double avg_wait = acq > 0 ? (double)wait / acq : 0;
        
        fprintf(output, "%-20s %10lu %10lu %9.1f%% %10.1f\n",
                lock_stats[i].name, acq, cont, cont_pct, avg_wait);
    }
}
#endif // DEBUG_LOCKS
```

## Testing Strategies

### Concurrency Testing

```c
// Test concurrent state access
void test_concurrent_state_access(void) {
    daemon_global_state_t *state = create_test_state();
    
    #define NUM_THREADS 10
    pthread_t threads[NUM_THREADS];
    
    // Start concurrent readers
    for (int i = 0; i < NUM_THREADS / 2; i++) {
        pthread_create(&threads[i], NULL, state_reader_thread, state);
    }
    
    // Start concurrent writers
    for (int i = NUM_THREADS / 2; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, state_writer_thread, state);
    }
    
    // Let them run
    sleep(5);
    
    // Stop all threads
    set_stop_flag(true);
    
    // Wait for completion
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Validate final state
    assert(validate_final_state(state));
    
    destroy_test_state(state);
}
```

### Deadlock Detection

```c
// Deadlock detection with thread sanitizer
void test_lock_ordering(void) {
    // Compile with: -fsanitize=thread
    
    daemon_global_state_t *state = create_test_state();
    
    // Test correct ordering
    lock_set_t locks = {
        .need_config = true,
        .need_goxel = true,
        .need_stats = true
    };
    
    assert(acquire_locks(state, &locks) == DAEMON_SUCCESS);
    release_locks(state, &locks);
    
    // Test incorrect ordering (should be caught by TSAN)
    // This would cause a deadlock or TSAN warning
    // pthread_mutex_lock(&state->locks.stats_mutex);
    // pthread_mutex_lock(&state->locks.goxel_mutex);  // Wrong order!
    
    destroy_test_state(state);
}
```

## Conclusion

This state management design provides:

1. **Thread Safety**: Clear locking strategies for all shared state
2. **Performance**: Lock-free operations where possible, minimal contention
3. **Consistency**: State validation and repair mechanisms
4. **Debuggability**: Comprehensive monitoring and diagnostics
5. **Scalability**: Per-CPU structures and object pools for high load

The design ensures reliable concurrent operation while maintaining the performance targets required for the v14.6 daemon architecture.