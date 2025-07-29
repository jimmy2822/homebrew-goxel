# Architecture Overview

## Current Architecture (v14.0)

The current Goxel architecture consists of multiple layers that introduce unnecessary complexity and performance overhead.

### Component Stack

```mermaid
graph TB
    subgraph "MCP Clients"
        MC1[MCP Client 1]
        MC2[MCP Client 2]
        MCN[MCP Client N]
    end
    
    subgraph "MCP Server Layer"
        MCP[goxel-mcp<br/>Node.js Server]
    end
    
    subgraph "Client Layer"
        TSC[TypeScript Client<br/>with Connection Pool]
    end
    
    subgraph "Daemon Layer"
        DAEMON[goxel-daemon<br/>JSON-RPC Server]
    end
    
    subgraph "Core Engine"
        CORE[Goxel Core<br/>C/C++ Engine]
    end
    
    MC1 --> MCP
    MC2 --> MCP
    MCN --> MCP
    MCP --> TSC
    TSC --> DAEMON
    DAEMON --> CORE
    
    style MCP fill:#f9f,stroke:#333,stroke-width:2px
    style TSC fill:#f9f,stroke:#333,stroke-width:2px
    style DAEMON fill:#bbf,stroke:#333,stroke-width:2px
    style CORE fill:#bfb,stroke:#333,stroke-width:2px
```

### Current Data Flow

```mermaid
sequenceDiagram
    participant Client as MCP Client
    participant MCP as MCP Server
    participant TS as TypeScript Client
    participant Daemon as Goxel Daemon
    participant Core as Core Engine
    
    Client->>MCP: MCP Protocol Request
    MCP->>MCP: Parse & Validate
    MCP->>TS: HTTP/WebSocket Call
    TS->>TS: Connection Pool Logic
    TS->>Daemon: JSON-RPC Request
    Daemon->>Daemon: Parse & Route
    Daemon->>Core: Internal API Call
    Core->>Core: Execute Operation
    Core-->>Daemon: Result
    Daemon-->>TS: JSON-RPC Response
    TS-->>MCP: HTTP Response
    MCP-->>Client: MCP Response
```

### Performance Impact

| Metric | Current Value | Overhead Source |
|--------|--------------|-----------------|
| Latency | ~5-10ms | 4 network hops |
| Serialization | 4x | Multiple format conversions |
| Memory | ~125MB | Duplicate data structures |
| CPU | +40% | Protocol translations |

## Target Architecture (Simplified)

The simplified architecture eliminates intermediate layers, providing direct MCP protocol support in the daemon.

### Simplified Component Stack

```mermaid
graph TB
    subgraph "MCP Clients"
        MC1[MCP Client 1]
        MC2[MCP Client 2]
        MCN[MCP Client N]
    end
    
    subgraph "Unified Daemon"
        DAEMON[goxel-mcp-daemon<br/>Direct MCP Support]
        CORE[Goxel Core Engine]
        DAEMON --> CORE
    end
    
    MC1 --> DAEMON
    MC2 --> DAEMON
    MCN --> DAEMON
    
    style DAEMON fill:#bfb,stroke:#333,stroke-width:4px
    style CORE fill:#bfb,stroke:#333,stroke-width:2px
```

### Optimized Data Flow

```mermaid
sequenceDiagram
    participant Client as MCP Client
    participant Daemon as MCP-Daemon
    participant Core as Core Engine
    
    Client->>Daemon: MCP Protocol Request
    Daemon->>Daemon: Parse & Route
    Daemon->>Core: Direct API Call
    Core->>Core: Execute Operation
    Core-->>Daemon: Result
    Daemon-->>Client: MCP Response
```

### Expected Performance Improvements

| Metric | Target Value | Improvement |
|--------|--------------|-------------|
| Latency | <1ms | 80% reduction |
| Serialization | 1x | Native format only |
| Memory | ~50MB | 60% reduction |
| CPU | Baseline | 40% reduction |

## Architectural Benefits

### 1. **Reduced Complexity**
- Single daemon process instead of 3 services
- Direct protocol handling
- Simplified deployment

### 2. **Performance Gains**
- Eliminate intermediate network hops
- Remove redundant serialization
- Direct memory access

### 3. **Operational Simplicity**
- Single service to monitor
- Unified logging
- Simplified debugging

### 4. **Resource Efficiency**
- Lower memory footprint
- Reduced CPU usage
- Fewer network connections

## Migration Strategy

### Phase 1: Dual-Mode Support
```mermaid
graph LR
    subgraph "Transition Period"
        CLIENT[Clients] --> ROUTER{Protocol<br/>Router}
        ROUTER -->|MCP| MCP_HANDLER[MCP Handler]
        ROUTER -->|JSON-RPC| JSONRPC[JSON-RPC Handler]
        MCP_HANDLER --> CORE[Core Engine]
        JSONRPC --> CORE
    end
```

### Phase 2: MCP-Native
```mermaid
graph LR
    subgraph "Final Architecture"
        CLIENT[MCP Clients] --> MCP_DAEMON[MCP-Native Daemon]
        MCP_DAEMON --> CORE[Core Engine]
    end
```

## Technical Considerations

### 1. **Protocol Handling**
- Native MCP parser in C
- Zero-copy message processing
- Efficient buffer management

### 2. **Backward Compatibility**
- Dual-mode operation during transition
- Compatibility shim for legacy clients
- Graceful deprecation path

### 3. **Performance Optimization**
- Lock-free data structures
- Memory pool allocation
- CPU affinity for workers

### 4. **Monitoring & Debugging**
- Built-in performance metrics
- Protocol-level debugging
- Trace logging capabilities

---

**Next**: [Dependencies Documentation](02_DEPENDENCIES.md) →

**Last Updated**: January 29, 2025  
**Version**: 1.0.0  
**Author**: Lisa Thompson