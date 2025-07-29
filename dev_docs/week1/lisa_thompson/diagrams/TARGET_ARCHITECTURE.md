# Target Architecture Diagrams

## Simplified System Overview

```mermaid
graph TB
    subgraph "External Clients"
        MC1[Claude Desktop]
        MC2[VS Code Extension]
        MC3[Custom MCP Tools]
    end
    
    subgraph "Unified MCP-Daemon"
        direction TB
        MCP_HANDLER[MCP Protocol Handler]
        ROUTER[Protocol Router]
        WORKERS[Worker Pool]
        API[Direct API Layer]
        CORE[Goxel Core Engine]
        
        MCP_HANDLER --> ROUTER
        ROUTER --> WORKERS
        WORKERS --> API
        API --> CORE
    end
    
    MC1 --> MCP_HANDLER
    MC2 --> MCP_HANDLER
    MC3 --> MCP_HANDLER
    
    style MCP_HANDLER fill:#99ff99,stroke:#333,stroke-width:3px
    style CORE fill:#99ff99,stroke:#333,stroke-width:2px
```

## Direct Request Flow

```mermaid
sequenceDiagram
    participant User
    participant Daemon as MCP-Daemon
    participant Worker
    participant Core
    
    User->>Daemon: MCP Request
    activate Daemon
    Daemon->>Daemon: Parse MCP Protocol
    Daemon->>Worker: Dispatch Direct
    deactivate Daemon
    
    activate Worker
    Worker->>Core: Native API Call
    activate Core
    Core->>Core: Execute Operation
    Core-->>Worker: Return Result
    deactivate Core
    Worker-->>Daemon: Complete
    deactivate Worker
    
    activate Daemon
    Daemon-->>User: MCP Response
    deactivate Daemon
```

## Performance Improvements

```mermaid
graph LR
    subgraph "Before: 4 Layers"
        B1[5-10ms Latency]
        B2[4x Serialization]
        B3[125MB Memory]
        B4[3 Network Hops]
    end
    
    subgraph "After: Direct Integration"
        A1[<1ms Latency]
        A2[1x Serialization]
        A3[50MB Memory]
        A4[0 Network Hops]
    end
    
    B1 --> |80% Reduction| A1
    B2 --> |Native Only| A2
    B3 --> |60% Reduction| A3
    B4 --> |Direct Socket| A4
```

## Unified Process Architecture

```mermaid
flowchart TD
    subgraph "Single Process"
        SOCKET[MCP Socket Listener]
        PROTOCOL[Protocol Detector]
        
        subgraph "Handlers"
            MCP_H[MCP Handler]
            JSON_H[JSON-RPC Handler]
        end
        
        subgraph "Execution"
            QUEUE[Work Queue]
            POOL[Thread Pool]
            API[API Layer]
        end
        
        subgraph "Core"
            ENGINE[Voxel Engine]
            VOLUME[Volume Manager]
            RENDER[Renderer]
        end
        
        SOCKET --> PROTOCOL
        PROTOCOL --> MCP_H
        PROTOCOL --> JSON_H
        MCP_H --> QUEUE
        JSON_H --> QUEUE
        QUEUE --> POOL
        POOL --> API
        API --> ENGINE
        ENGINE --> VOLUME
        ENGINE --> RENDER
    end
```

## Memory Efficiency

```mermaid
pie title Optimized Memory Distribution (Total: ~50MB)
    "Daemon Process" : 30
    "Core Engine" : 15
    "Worker Threads" : 3
    "Protocol Buffers" : 2
```

## Dual-Mode Operation (Transition)

```mermaid
graph TD
    subgraph "Protocol Router"
        INPUT[Incoming Connection]
        DETECT{Detect Protocol}
        
        INPUT --> DETECT
        DETECT -->|MCP Magic| MCP_PATH[MCP Handler]
        DETECT -->|JSON-RPC| JSON_PATH[Legacy Handler]
        
        MCP_PATH --> UNIFIED[Unified Core API]
        JSON_PATH --> UNIFIED
    end
    
    style DETECT fill:#ffff99,stroke:#333,stroke-width:2px
    style UNIFIED fill:#99ff99,stroke:#333,stroke-width:2px
```

## Zero-Copy Data Flow

```mermaid
graph LR
    subgraph "Efficient Data Handling"
        SOCKET[Socket Buffer] --> PARSE[In-Place Parse]
        PARSE --> PTR[Pointer References]
        PTR --> API[Direct API Args]
        API --> RESULT[Result Buffer]
        RESULT --> SEND[Direct Send]
    end
    
    SOCKET -.-> |No Copy| PARSE
    PARSE -.-> |No Copy| PTR
    PTR -.-> |No Copy| API
    API -.-> |Single Alloc| RESULT
    RESULT -.-> |No Copy| SEND
```

## Deployment Simplicity

```mermaid
graph TB
    subgraph "Single Service"
        BINARY[goxel-mcp-daemon]
        CONFIG[daemon.conf]
        SERVICE[systemd/launchd]
    end
    
    subgraph "Management"
        START[Start Service]
        MONITOR[Monitor Health]
        LOG[Unified Logging]
    end
    
    BINARY --> SERVICE
    CONFIG --> SERVICE
    SERVICE --> START
    START --> MONITOR
    MONITOR --> LOG
    
    style BINARY fill:#99ff99,stroke:#333,stroke-width:3px
```

## Performance Characteristics

```mermaid
graph TD
    subgraph "Request Processing"
        REQ[MCP Request] -->|<0.1ms| PARSE[Parse]
        PARSE -->|<0.1ms| ROUTE[Route]
        ROUTE -->|<0.1ms| EXEC[Execute]
        EXEC -->|<0.5ms| RESP[Response]
    end
    
    subgraph "Metrics"
        LAT[Total Latency: <1ms]
        THROUGH[Throughput: >10K ops/sec]
        MEM[Memory: <50MB]
        CPU[CPU: Efficient]
    end
```

## Migration Path

```mermaid
stateDiagram-v2
    [*] --> Current: v14.0
    Current --> DualMode: Enable MCP Handler
    DualMode --> Testing: Validate MCP Path
    Testing --> Migration: Move Clients
    Migration --> MCPOnly: Disable Legacy
    MCPOnly --> [*]: v15.0
    
    Current: 4-Layer Architecture
    DualMode: Both Protocols
    Testing: A/B Testing
    Migration: Client Updates
    MCPOnly: MCP Native
```

## Monitoring Dashboard

```mermaid
graph LR
    subgraph "Single Dashboard"
        METRICS[Performance Metrics]
        HEALTH[Health Status]
        LOGS[Unified Logs]
        ALERTS[Alert System]
    end
    
    subgraph "Key Indicators"
        RPS[Requests/sec]
        LAT[Latency p99]
        ERR[Error Rate]
        MEM[Memory Usage]
    end
    
    METRICS --> RPS
    METRICS --> LAT
    HEALTH --> ERR
    HEALTH --> MEM
```

---

**Benefits Summary**:

1. **Performance**: 80% latency reduction, 60% memory savings
2. **Simplicity**: Single process, unified logging, one service
3. **Efficiency**: Zero-copy paths, direct API calls
4. **Flexibility**: Dual-mode support during transition
5. **Operations**: Simplified deployment and monitoring

**Next**: [Implementation Roadmap](../wiki/03_IMPLEMENTATION_GUIDE.md) →

**Last Updated**: January 29, 2025  
**Version**: 1.0.0  
**Author**: Lisa Thompson