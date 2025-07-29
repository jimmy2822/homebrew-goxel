# Dual-Mode Operation Plan
**Author**: Michael Rodriguez  
**Date**: January 29, 2025  
**Goal**: Support both JSON-RPC and MCP protocols in a single daemon  

## Executive Summary

This plan outlines how we'll modify the Goxel daemon to support both JSON-RPC (for backward compatibility) and MCP (for enhanced AI integration) protocols simultaneously. The design prioritizes performance, minimal overhead, and clean protocol separation.

## Architecture Overview

### Current State (v14)
```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │ JSON-RPC only
┌──────▼──────┐
│Socket Server│
└──────┬──────┘
       │
┌──────▼──────┐
│ JSON Parser │
└──────┬──────┘
       │
┌──────▼──────┐
│Worker Pool  │
└─────────────┘
```

### Target State (v15)
```
┌─────────────┐     ┌─────────────┐
│ JSON Client │     │ MCP Client  │
└──────┬──────┘     └──────┬──────┘
       │                    │
       └────────┬───────────┘
                │
┌───────────────▼───────────────┐
│    Protocol-Aware Socket      │
│         Multiplexer           │
└───────────────┬───────────────┘
                │
        ┌───────┴───────┐
        ▼               ▼
┌─────────────┐ ┌─────────────┐
│ JSON-RPC    │ │    MCP      │
│  Handler    │ │  Handler    │
└──────┬──────┘ └──────┬──────┘
       │               │
       └───────┬───────┘
               ▼
      ┌─────────────┐
      │Shared Worker│
      │    Pool     │
      └─────────────┘
```

## Protocol Detection

### Magic Byte Approach

```c
// Protocol identification in first 4 bytes
typedef enum {
    PROTO_UNKNOWN    = 0x00000000,
    PROTO_JSON_RPC   = 0x7B227270,  // {"rp (start of {"rpc")
    PROTO_MCP_BINARY = 0x4D435042,  // MCPB
    PROTO_MCP_TEXT   = 0x4D435054,  // MCPT
} protocol_magic_t;

// Fast protocol detection
protocol_type_t detect_protocol_fast(const uint8_t *data, size_t len) {
    if (len < 4) return PROTO_UNKNOWN;
    
    uint32_t magic = *(uint32_t*)data;
    
    // Direct magic comparison (fastest path)
    switch (magic) {
        case PROTO_MCP_BINARY:
            return PROTOCOL_MCP_BINARY;
        case PROTO_MCP_TEXT:
            return PROTOCOL_MCP_TEXT;
    }
    
    // JSON detection (check first char)
    if (data[0] == '{' || data[0] == '[') {
        return PROTOCOL_JSON_RPC;
    }
    
    return PROTO_UNKNOWN;
}
```

### Connection Lifecycle

```c
typedef struct {
    int fd;
    protocol_type_t protocol;
    union {
        json_rpc_context_t *json_ctx;
        mcp_context_t *mcp_ctx;
    } ctx;
    
    // Shared resources
    buffer_pool_entry_t *buffer;
    worker_pool_t *workers;
} dual_mode_client_t;

// Connection handling
void handle_new_connection(int client_fd) {
    dual_mode_client_t *client = client_pool_get();
    client->fd = client_fd;
    client->protocol = PROTO_UNKNOWN;
    
    // Set non-blocking
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    
    // Add to epoll with edge-triggered mode
    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLET,
        .data.ptr = client
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
}

// First data determines protocol
void handle_first_data(dual_mode_client_t *client) {
    uint8_t peek_buf[4];
    ssize_t n = recv(client->fd, peek_buf, 4, MSG_PEEK);
    
    if (n >= 4) {
        client->protocol = detect_protocol_fast(peek_buf, n);
        
        switch (client->protocol) {
            case PROTOCOL_JSON_RPC:
                client->ctx.json_ctx = json_rpc_context_create();
                break;
            case PROTOCOL_MCP_BINARY:
            case PROTOCOL_MCP_TEXT:
                client->ctx.mcp_ctx = mcp_context_create(client->protocol);
                break;
            default:
                close_client(client);
                return;
        }
    }
}
```

## Unified Request Processing

### Request Abstraction

```c
// Protocol-agnostic request
typedef struct {
    request_id_t id;
    method_id_t method;
    void *params;
    void (*complete)(void *result, void *error);
    void *user_data;
} unified_request_t;

// Method registry for both protocols
typedef struct {
    const char *name;
    method_id_t id;
    request_handler_fn json_handler;
    request_handler_fn mcp_handler;
    uint32_t flags;
} method_entry_t;

static method_entry_t method_registry[] = {
    {"create_project", METHOD_CREATE_PROJECT, 
     handle_create_json, handle_create_mcp, METHOD_FLAG_ASYNC},
    {"add_voxels", METHOD_ADD_VOXELS,
     handle_voxels_json, handle_voxels_mcp, METHOD_FLAG_BATCH},
    // ... more methods
};
```

### Worker Pool Integration

```c
// Enhanced worker that handles both protocols
typedef struct {
    worker_pool_t *pool;
    void *goxel_context;
    
    // Protocol handlers
    json_rpc_processor_t *json_proc;
    mcp_processor_t *mcp_proc;
    
    // Shared resources
    buffer_pool_t *buffers;
    result_cache_t *cache;
} enhanced_worker_t;

// Unified processing function
void process_request(enhanced_worker_t *worker, unified_request_t *req) {
    // Check cache first (same for both protocols)
    void *cached = cache_get(worker->cache, req->id);
    if (cached) {
        req->complete(cached, NULL);
        return;
    }
    
    // Find method handler
    method_entry_t *method = find_method(req->method);
    if (!method) {
        req->complete(NULL, "Unknown method");
        return;
    }
    
    // Execute appropriate handler
    void *result = NULL;
    if (req->user_data && IS_MCP_REQUEST(req)) {
        result = method->mcp_handler(worker->goxel_context, req->params);
    } else {
        result = method->json_handler(worker->goxel_context, req->params);
    }
    
    // Cache if applicable
    if (method->flags & METHOD_FLAG_CACHEABLE) {
        cache_put(worker->cache, req->id, result);
    }
    
    req->complete(result, NULL);
}
```

## Performance Optimizations

### 1. Zero-Cost Abstraction

```c
// Compile-time protocol dispatch
#define DISPATCH_BY_PROTOCOL(client, json_func, mcp_func) \
    ((client)->protocol == PROTOCOL_JSON_RPC ? \
     (json_func)((client)->ctx.json_ctx) : \
     (mcp_func)((client)->ctx.mcp_ctx))

// Inlined protocol checks
static inline bool is_mcp_client(const dual_mode_client_t *client) {
    return client->protocol >= PROTOCOL_MCP_BINARY;
}

static inline bool is_json_client(const dual_mode_client_t *client) {
    return client->protocol == PROTOCOL_JSON_RPC;
}
```

### 2. Memory Pool Segregation

```c
// Protocol-specific memory pools
typedef struct {
    memory_pool_t *json_small;   // 256B blocks for JSON
    memory_pool_t *json_large;   // 4KB blocks for JSON
    memory_pool_t *mcp_frames;   // 1KB blocks for MCP
    memory_pool_t *mcp_bulk;     // 64KB blocks for MCP bulk
    memory_pool_t *shared;       // Variable size, both protocols
} dual_mode_memory_t;

// Allocation by protocol
void *alloc_by_protocol(dual_mode_memory_t *mem, 
                       protocol_type_t proto, 
                       size_t size) {
    if (proto == PROTOCOL_JSON_RPC) {
        return size <= 256 ? 
               pool_alloc(mem->json_small) : 
               pool_alloc(mem->json_large);
    } else {
        return size <= 1024 ? 
               pool_alloc(mem->mcp_frames) : 
               pool_alloc(mem->mcp_bulk);
    }
}
```

### 3. Protocol-Specific Fast Paths

```c
// JSON-RPC fast path for common methods
static inline bool try_json_fast_path(json_rpc_context_t *ctx,
                                     const char *method,
                                     json_t *params) {
    // Common methods with simple responses
    if (strcmp(method, "get_status") == 0) {
        send_json_response(ctx, "{\"status\":\"ok\",\"version\":\"15.0\"}");
        return true;
    }
    
    if (strcmp(method, "ping") == 0) {
        send_json_response(ctx, "{\"result\":\"pong\"}");
        return true;
    }
    
    return false;
}

// MCP fast path for control messages
static inline bool try_mcp_fast_path(mcp_context_t *ctx,
                                    uint16_t msg_type,
                                    const void *data) {
    switch (msg_type) {
        case MCP_MSG_HEARTBEAT:
            mcp_send_ack(ctx, MCP_ACK_HEARTBEAT);
            return true;
            
        case MCP_MSG_VERSION_CHECK:
            mcp_send_version(ctx, 15, 0, 0);
            return true;
    }
    
    return false;
}
```

## Migration Strategy

### Phase 1: Foundation (Week 1)
1. Implement protocol detection
2. Create unified client structure
3. Add dual-mode to socket server
4. Basic testing framework

### Phase 2: Integration (Week 2)
1. Unified request processing
2. Worker pool modifications
3. Memory pool optimization
4. Protocol-specific fast paths

### Phase 3: Optimization (Week 3)
1. Lock-free structures
2. Zero-copy paths
3. SIMD optimizations
4. Performance validation

### Backward Compatibility

```c
// Environment variable for protocol preference
const char *default_proto = getenv("GOXEL_DEFAULT_PROTOCOL");

// Command line option
struct config {
    bool json_only;     // --json-only
    bool mcp_only;      // --mcp-only
    bool prefer_mcp;    // --prefer-mcp
    int json_port;      // --json-port=9001
    int mcp_port;       // --mcp-port=9002
};

// Graceful protocol downgrade
if (client->protocol == PROTOCOL_MCP_BINARY && 
    !server->mcp_enabled) {
    // Try to negotiate JSON-RPC fallback
    send_protocol_error(client, "MCP not enabled, use JSON-RPC");
    client->protocol = PROTOCOL_JSON_RPC;
}
```

## Performance Targets

| Metric | JSON-RPC | MCP | Dual-Mode Overhead |
|--------|----------|-----|-------------------|
| Protocol Detection | N/A | N/A | <1μs |
| Request Routing | 5μs | 3μs | +0.5μs |
| Memory per Client | 4KB | 2KB | +256B |
| Context Switch | N/A | N/A | <100ns |

## Testing Strategy

### Unit Tests
- Protocol detection accuracy
- Client state transitions
- Memory pool efficiency
- Request routing correctness

### Integration Tests
- Mixed protocol clients
- Protocol switching
- Resource sharing
- Error handling

### Performance Tests
- Throughput comparison
- Latency measurement
- Memory usage
- CPU utilization

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Protocol confusion | High | Magic bytes + validation |
| Memory fragmentation | Medium | Segregated pools |
| Code complexity | Medium | Clean abstractions |
| Performance regression | Low | Continuous benchmarking |

## Success Criteria

1. **Seamless Operation**: Clients can connect with either protocol
2. **No Performance Loss**: <5% overhead for single-protocol use
3. **Resource Efficiency**: Shared infrastructure reduces memory
4. **Clean Architecture**: Protocol handlers remain independent

## Notes for Team

**Sarah**: Your MCP handler will plug directly into the `mcp_handler` function pointers in the method registry.

**Alex**: Need tests for protocol detection edge cases and mixed-client scenarios.

**Lisa**: Documentation should emphasize the transparent protocol support - users don't need to know about the dual-mode unless they want to.

---

*This plan provides a clear path to dual-mode operation while maintaining the performance gains we're targeting for v15.*