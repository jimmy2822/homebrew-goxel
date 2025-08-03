# Daemon Connection Reuse Architecture

## Executive Summary

This document outlines the architecture for enabling connection reuse in the Goxel v15.0 daemon, addressing the current limitation where only one JSON-RPC request can be processed per connection.

## Current State Analysis

### Problem
- After processing one JSON-RPC request, connections cannot handle subsequent requests
- Each request requires a new connection, causing overhead
- No support for concurrent requests on same connection
- Limited scalability for automation workflows

### Root Cause
- JSON monitor thread exits after first request/response cycle
- No request/response correlation mechanism
- Missing connection state management
- No message boundary detection for multiple requests

## Proposed Architecture

### 1. Connection Lifecycle Management

```
┌─────────────────┐
│  Accept Thread  │
└────────┬────────┘
         │
    ┌────▼─────┐
    │  Client  │──────┐
    │Connected │      │
    └──────────┘      │
                      ▼
              ┌──────────────┐     ┌─────────────┐
              │JSON Monitor  │────▶│Request Queue│
              │   Thread     │     └─────────────┘
              └──────┬───────┘            │
                     │                    │
                     ▼                    ▼
              ┌──────────────┐     ┌─────────────┐
              │Response Queue│◀────│Worker Pool  │
              └──────────────┘     └─────────────┘
```

### 2. Connection State Machine

```
     ┌─────────┐
     │  NEW    │
     └────┬────┘
          │ accept()
     ┌────▼────┐
     │ ACTIVE  │◀─────────┐
     └────┬────┘          │
          │               │
    ┌─────┴──────┬────────┤
    │            │        │
┌───▼───┐  ┌────▼────┐   │
│ IDLE  │  │PROCESSING│───┘
└───┬───┘  └─────────┘
    │ timeout/close
┌───▼────┐
│CLOSING │
└────────┘
```

### 3. Key Components

#### Connection State Structure
```c
typedef struct {
    // Connection identity
    uint32_t connection_id;
    int fd;
    
    // Request tracking
    hash_table_t *pending_requests;  // id -> request_info
    uint32_t next_request_id;
    
    // Connection state
    enum {
        CONN_STATE_ACTIVE,
        CONN_STATE_IDLE,
        CONN_STATE_CLOSING
    } state;
    
    // Timing
    time_t last_activity;
    time_t keepalive_interval;
    
    // Buffers
    circular_buffer_t *read_buffer;
    circular_buffer_t *write_buffer;
} connection_state_t;
```

#### Global Daemon State
```c
typedef struct {
    // Connection tracking
    hash_table_t *connections;      // fd -> connection_state_t
    pthread_mutex_t conn_mutex;
    
    // Statistics
    atomic_uint32_t active_connections;
    atomic_uint32_t total_requests;
    atomic_uint32_t pending_requests;
    
    // Configuration
    size_t max_connections;
    size_t max_pending_per_conn;
    time_t request_timeout;
    time_t idle_timeout;
} daemon_state_t;
```

### 4. Enhanced JSON Monitor Thread

```c
// Pseudo-code for improved monitor thread
void *enhanced_json_monitor(void *arg) {
    connection_state_t *conn = (connection_state_t*)arg;
    
    while (conn->state != CONN_STATE_CLOSING) {
        // 1. Read available data into circular buffer
        read_available_data(conn);
        
        // 2. Try to parse complete JSON messages
        while (has_complete_message(conn->read_buffer)) {
            json_request_t *req = parse_json_request(conn->read_buffer);
            
            // 3. Track pending request
            track_pending_request(conn, req);
            
            // 4. Queue for processing
            enqueue_work_item(req, conn);
        }
        
        // 5. Check for responses to send
        while (has_pending_responses(conn)) {
            json_response_t *resp = dequeue_response(conn);
            write_json_response(conn->fd, resp);
            
            // 6. Remove from pending
            complete_pending_request(conn, resp->id);
        }
        
        // 7. Handle timeouts and keepalive
        check_request_timeouts(conn);
        send_keepalive_if_needed(conn);
    }
}
```

### 5. Message Framing

#### Request Format
```json
{
  "jsonrpc": "2.0",
  "method": "goxel.add_voxels",
  "params": {...},
  "id": "req-12345"  // Unique correlation ID
}
```

#### Response Format
```json
{
  "jsonrpc": "2.0",
  "result": {...},
  "id": "req-12345"  // Matches request ID
}
```

### 6. Request/Response Correlation

- Each request gets a unique ID (client-provided or server-generated)
- Pending requests tracked in per-connection hash table
- Responses matched to requests by ID
- Support for out-of-order response delivery
- Timeout handling for unmatched requests

## TDD Implementation Plan

### Phase 1: Connection Lifecycle (Week 1)

**Test Files:**
- `tests/tdd/test_connection_lifecycle.c`
- `tests/tdd/test_connection_state.c`

**Key Tests:**
```c
TEST(test_connection_accepts_multiple_requests)
TEST(test_connection_state_transitions)
TEST(test_connection_cleanup_on_close)
```

### Phase 2: Request/Response Correlation (Week 2)

**Test Files:**
- `tests/tdd/test_request_correlation.c`
- `tests/tdd/test_concurrent_requests.c`

**Key Tests:**
```c
TEST(test_handles_concurrent_requests)
TEST(test_out_of_order_responses)
TEST(test_request_id_generation)
```

### Phase 3: Error Handling & Recovery (Week 3)

**Test Files:**
- `tests/tdd/test_connection_errors.c`
- `tests/tdd/test_timeout_handling.c`

**Key Tests:**
```c
TEST(test_request_timeout)
TEST(test_connection_idle_timeout)
TEST(test_malformed_request_recovery)
```

### Phase 4: Performance & Stress Testing (Week 4)

**Test Files:**
- `tests/tdd/test_connection_stress.c`
- `tests/tdd/test_memory_safety.c`

**Key Tests:**
```c
TEST(test_handles_1000_concurrent_requests)
TEST(test_no_memory_leaks_after_10000_requests)
TEST(test_connection_pool_limits)
```

## Implementation Priority

1. **Fix JSON monitor loop** (Day 1-2)
   - Prevent monitor thread from exiting after first request
   - Add proper JSON message boundary detection

2. **Add connection state tracking** (Day 3-4)
   - Implement connection_state_t structure
   - Add per-connection request tracking

3. **Implement response routing** (Day 5-6)
   - Match responses to pending requests
   - Support out-of-order responses

4. **Add timeout handling** (Day 7-8)
   - Request-level timeouts
   - Connection idle timeouts

5. **Optimize performance** (Day 9-10)
   - Implement circular buffers
   - Reduce memory allocations
   - Add connection pooling

## Backward Compatibility

The implementation maintains full backward compatibility:

1. **Single request/response continues to work** - Existing clients see no change
2. **No API changes** - All JSON-RPC methods remain the same
3. **Opt-in persistent connections** - Clients choose to reuse connections
4. **Graceful degradation** - Falls back to single-request mode on errors

## Performance Goals

- Support 100+ concurrent connections
- Handle 1000+ requests/second per connection
- Sub-millisecond request routing overhead
- Memory usage < 1KB per idle connection
- Zero memory leaks over extended operation

## Security Considerations

1. **Request flooding protection** - Limit pending requests per connection
2. **Memory exhaustion prevention** - Cap buffer sizes
3. **Timeout enforcement** - Prevent resource hoarding
4. **Connection limits** - Configurable max connections

## Monitoring & Debugging

New statistics to track:
- Requests per connection
- Connection lifetime
- Request latency percentiles
- Pending request queue depths
- Timeout rates

## Conclusion

This architecture enables efficient connection reuse while maintaining backward compatibility and following Goxel's TDD principles. The phased implementation approach ensures each component is thoroughly tested before integration.