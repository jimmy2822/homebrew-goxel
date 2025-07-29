# Optimization Opportunities Summary
**Author**: Michael Rodriguez  
**Date**: January 29, 2025  
**Focus**: Actionable performance improvements for v15  

## Critical Path Optimizations

### 1. Startup Time: 450ms → <100ms

**Current Bottlenecks:**
- Sequential Goxel context initialization (8 contexts × 25ms = 200ms)
- Thread creation with default 8MB stacks (8 threads × 12ms = 96ms)  
- Socket binding and file operations (50ms)
- Memory pre-allocation (100ms)

**Solutions:**
```c
// Lazy context initialization
typedef struct {
    _Atomic(bool) initialized;
    void *context;
    pthread_once_t once;
} lazy_context_t;

void ensure_context(lazy_context_t *ctx, int worker_id) {
    if (!atomic_load(&ctx->initialized)) {
        pthread_once(&ctx->once, init_goxel_context);
    }
}

// Reduced stack size
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 256 * 1024); // 256KB vs 8MB

// Async socket initialization
int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
// Bind happens in background thread
```

### 2. Memory Usage: 66MB → 20MB

**Current Waste:**
- 8 worker threads × 8MB stack = 64MB (mostly unused)
- Duplicate Goxel contexts = 8 × 2MB = 16MB
- Pre-allocated client buffers = 256 × 4KB = 1MB

**Solutions:**
```c
// Single shared context with RCU
typedef struct {
    void *goxel_ctx;
    pthread_rwlock_t lock;
    _Atomic(uint64_t) version;
} shared_context_t;

// On-demand buffer allocation
typedef struct {
    buffer_pool_t *small;  // 512B buffers
    buffer_pool_t *medium; // 4KB buffers  
    buffer_pool_t *large;  // 64KB buffers
} tiered_buffer_pool_t;

// Start with 2 workers, grow as needed
#define INITIAL_WORKERS 2
#define MAX_WORKERS 16
```

### 3. Request Latency: 5-10ms → <1ms

**Current Flow:**
1. Read syscall → Copy to buffer (100μs)
2. JSON parse → Allocate AST (500μs)
3. Enqueue → Lock contention (50μs)
4. Dequeue → More locks (50μs)
5. Process → Dispatch overhead (200μs)
6. Serialize → JSON stringify (500μs)
7. Write syscall → Copy from buffer (100μs)

**Optimized Flow:**
```c
// Zero-copy with io_uring (Linux)
struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
io_uring_prep_recv(sqe, fd, buffer, size, 0);
sqe->user_data = (uint64_t)client;

// Direct parsing without AST
typedef struct {
    const char *method;
    size_t method_len;
    const char *params;
    size_t params_len;
} json_rpc_view_t;

bool parse_json_rpc_zero_copy(const char *data, size_t len, 
                             json_rpc_view_t *view) {
    // Use SIMD to find key positions
    // No allocations, just pointers into buffer
}

// Lock-free queue with CAS
bool enqueue_lockfree(queue_t *q, void *item) {
    node_t *node = node_pool_get();
    node->data = item;
    node->next = NULL;
    
    node_t *last = atomic_load(&q->tail);
    while (!atomic_compare_exchange_weak(&last->next, NULL, node)) {
        last = atomic_load(&q->tail);
    }
    atomic_store(&q->tail, node);
}
```

### 4. Hot Path Optimizations

**Method Dispatch Table:**
```c
// Current: String comparison in loop
for (int i = 0; i < num_methods; i++) {
    if (strcmp(method, methods[i].name) == 0) {
        return methods[i].handler;
    }
}

// Optimized: Perfect hash table
typedef struct {
    uint32_t hash;
    method_handler_t handler;
} method_entry_t;

// Compile-time perfect hash generation
#define METHOD_HASH(name) (djb2_hash(name) & METHOD_MASK)

static method_entry_t method_table[] = {
    [METHOD_HASH("create_project")] = {hash_create, handle_create},
    [METHOD_HASH("add_voxels")] = {hash_voxels, handle_voxels},
    // ...
};

// O(1) lookup
method_handler_t lookup_method_fast(uint32_t hash) {
    method_entry_t *entry = &method_table[hash & METHOD_MASK];
    if (entry->hash == hash) return entry->handler;
    return NULL;
}
```

**SIMD JSON Parsing:**
```c
// Find JSON delimiters with SIMD
#include <immintrin.h>

size_t find_json_end_simd(const char *data, size_t len) {
    __m256i quote = _mm256_set1_epi8('"');
    __m256i brace = _mm256_set1_epi8('}');
    __m256i escape = _mm256_set1_epi8('\\');
    
    int depth = 0;
    bool in_string = false;
    
    for (size_t i = 0; i < len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(data + i));
        __m256i quotes = _mm256_cmpeq_epi8(chunk, quote);
        __m256i braces = _mm256_cmpeq_epi8(chunk, brace);
        
        // Process matches...
    }
}
```

### 5. Cache-Friendly Data Layout

**Current:**
```c
struct client {
    int fd;               // 4 bytes
    char padding[60];     // 60 bytes waste
    // --- cache line boundary ---
    void *buffer;         // 8 bytes
    size_t buffer_size;   // 8 bytes
    // ... more fields spread across cache lines
};
```

**Optimized:**
```c
// Group hot data in one cache line
struct client_hot {
    int fd;                    // 4 bytes
    uint32_t flags;           // 4 bytes
    void *current_buffer;     // 8 bytes
    size_t buffer_pos;        // 8 bytes
    method_handler_t handler; // 8 bytes
    void *context;           // 8 bytes
    uint64_t last_active;    // 8 bytes
    // Padding to 64 bytes
    char pad[16];
} __attribute__((aligned(64)));

// Cold data in separate structure
struct client_cold {
    char address[128];
    statistics_t stats;
    // ... rarely accessed fields
};
```

### 6. Compiler Optimizations

```makefile
# Current flags
CFLAGS = -O2 -g

# Optimized flags
CFLAGS = -O3 -march=native -flto -fno-plt \
         -fomit-frame-pointer -finline-functions \
         -funroll-loops -ftree-vectorize

# Profile-guided optimization
pgo-generate:
    $(CC) $(CFLAGS) -fprofile-generate ...

pgo-use:
    $(CC) $(CFLAGS) -fprofile-use ...

# Link-time optimization
LDFLAGS = -flto -Wl,--gc-sections
```

### 7. System-Level Optimizations

**CPU Affinity:**
```c
// Pin workers to specific cores
void set_thread_affinity(pthread_t thread, int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
}

// NUMA awareness
void *alloc_numa_local(size_t size, int node) {
    return numa_alloc_onnode(size, node);
}
```

**Huge Pages:**
```c
// Use huge pages for large allocations
void *alloc_huge(size_t size) {
    void *ptr = mmap(NULL, size, 
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                    -1, 0);
    if (ptr == MAP_FAILED) {
        // Fallback to normal pages
        ptr = mmap(NULL, size, 
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    }
    return ptr;
}
```

## Quick Wins (Implement This Week)

1. **Reduce thread stack size**: 64MB → 2MB savings
2. **Lazy initialization**: 200ms startup improvement
3. **Remove unnecessary mutexes**: 20% latency reduction
4. **Buffer pooling**: 90% allocation reduction
5. **Method hash table**: 10x faster dispatch

## Medium-Term Goals (Weeks 2-3)

1. **io_uring support**: 50% syscall overhead reduction
2. **SIMD parsing**: 3x JSON parsing speed
3. **Lock-free queues**: 5x throughput increase
4. **Zero-copy paths**: 80% memory bandwidth savings

## Benchmark Targets

```c
// Micro-benchmarks to track
void benchmark_suite() {
    BENCHMARK("startup_time", measure_startup);
    BENCHMARK("first_request", measure_first_request);
    BENCHMARK("json_parse_1k", measure_json_parse_1k);
    BENCHMARK("method_dispatch", measure_dispatch);
    BENCHMARK("memory_allocate", measure_allocation);
    BENCHMARK("queue_operations", measure_queue_ops);
}

// Target results:
// startup_time: <100ms (was 450ms)
// first_request: <1ms (was 10ms)
// json_parse_1k: <10μs (was 50μs)
// method_dispatch: <100ns (was 1μs)
```

## Risk Assessment

| Optimization | Risk | Mitigation |
|--------------|------|------------|
| Lock-free queues | Complexity | Extensive testing |
| SIMD parsing | Portability | Fallback paths |
| io_uring | Linux-only | ifdef guards |
| Huge pages | Admin setup | Auto-detect |

## Next Steps

1. **Today**: Implement quick wins, measure impact
2. **Tomorrow**: Begin lock-free queue prototype
3. **This Week**: Complete benchmarking framework
4. **Next Week**: Integrate with Sarah's MCP work

---

*These optimizations will transform Goxel from a traditional daemon into a high-performance, modern architecture suitable for AI workloads.*