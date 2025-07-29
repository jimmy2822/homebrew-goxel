# Current Architecture Diagrams

## System Overview

```mermaid
graph TB
    subgraph "External Clients"
        MC1[Claude Desktop]
        MC2[VS Code Extension]
        MC3[Custom MCP Tools]
    end
    
    subgraph "MCP Server" 
        direction TB
        MCPS[goxel-mcp Server<br/>Node.js Process]
        MCPH[MCP Protocol Handler]
        MCPR[Request Router]
        MCPS --> MCPH
        MCPH --> MCPR
    end
    
    subgraph "TypeScript Client"
        direction TB
        TSC[TypeScript Client Library]
        POOL[Connection Pool Manager]
        HEALTH[Health Monitor]
        TSC --> POOL
        TSC --> HEALTH
    end
    
    subgraph "Goxel Daemon"
        direction TB
        SOCK[Unix Socket Server]
        JSONRPC[JSON-RPC Handler]
        WORK[Worker Pool]
        API[Headless API]
        SOCK --> JSONRPC
        JSONRPC --> WORK
        WORK --> API
    end
    
    subgraph "Core Engine"
        direction TB
        CORE[Goxel Core]
        VOL[Volume Manager]
        LAYER[Layer System]
        RENDER[Renderer]
        CORE --> VOL
        CORE --> LAYER
        CORE --> RENDER
    end
    
    MC1 --> MCPS
    MC2 --> MCPS
    MC3 --> MCPS
    MCPR --> TSC
    POOL --> SOCK
    API --> CORE
    
    style MCPS fill:#ff9999,stroke:#333,stroke-width:2px
    style TSC fill:#ff9999,stroke:#333,stroke-width:2px
    style SOCK fill:#9999ff,stroke:#333,stroke-width:2px
    style CORE fill:#99ff99,stroke:#333,stroke-width:2px
```

## Detailed Component Interactions

```mermaid
flowchart LR
    subgraph "Layer 1: MCP Interface"
        MCP_IN[MCP Request] --> PARSE1[Protocol Parser]
        PARSE1 --> VALIDATE1[Validation]
        VALIDATE1 --> TRANSFORM1[Transform to HTTP]
    end
    
    subgraph "Layer 2: TypeScript Client"
        HTTP_IN[HTTP Request] --> QUEUE[Request Queue]
        QUEUE --> SERIALIZE1[Serialize to JSON-RPC]
        SERIALIZE1 --> SOCKET[Socket Selection]
    end
    
    subgraph "Layer 3: JSON-RPC Daemon"
        UNIX_IN[Unix Socket] --> PARSE2[JSON Parser]
        PARSE2 --> VALIDATE2[Method Validation]
        VALIDATE2 --> DISPATCH[Worker Dispatch]
    end
    
    subgraph "Layer 4: Core Engine"
        API_CALL[API Function] --> EXEC[Execute Operation]
        EXEC --> RESULT[Generate Result]
    end
    
    TRANSFORM1 --> HTTP_IN
    SOCKET --> UNIX_IN
    DISPATCH --> API_CALL
```

## Request Flow Sequence

```mermaid
sequenceDiagram
    participant User
    participant MCP as MCP Server
    participant TS as TypeScript Client
    participant Daemon
    participant Worker
    participant Core
    
    User->>MCP: Tool Request
    activate MCP
    MCP->>MCP: Parse MCP Protocol
    MCP->>MCP: Validate Parameters
    MCP->>TS: HTTP Request
    deactivate MCP
    
    activate TS
    TS->>TS: Check Connection Pool
    TS->>TS: Serialize to JSON-RPC
    TS->>Daemon: Unix Socket Write
    deactivate TS
    
    activate Daemon
    Daemon->>Daemon: Read Socket Data
    Daemon->>Daemon: Parse JSON-RPC
    Daemon->>Worker: Dispatch to Worker
    deactivate Daemon
    
    activate Worker
    Worker->>Core: Call API Function
    activate Core
    Core->>Core: Execute Operation
    Core-->>Worker: Return Result
    deactivate Core
    Worker-->>Daemon: Job Complete
    deactivate Worker
    
    activate Daemon
    Daemon-->>TS: JSON-RPC Response
    deactivate Daemon
    
    activate TS
    TS-->>MCP: HTTP Response
    deactivate TS
    
    activate MCP
    MCP-->>User: Tool Result
    deactivate MCP
```

## Performance Bottlenecks

```mermaid
graph TD
    subgraph "Serialization Overhead"
        S1[MCP to JSON] --> S2[JSON to HTTP]
        S2 --> S3[HTTP to JSON-RPC]
        S3 --> S4[JSON-RPC to C Structs]
    end
    
    subgraph "Network Hops"
        N1[Client to MCP Server] --> N2[MCP to TypeScript]
        N2 --> N3[TypeScript to Daemon]
    end
    
    subgraph "Processing Delays"
        P1[MCP Validation] --> P2[TypeScript Queue]
        P2 --> P3[Worker Pool Wait]
        P3 --> P4[Response Assembly]
    end
    
    S1 -.-> |~1ms| S2
    S2 -.-> |~0.5ms| S3
    S3 -.-> |~1ms| S4
    
    N1 -.-> |~2ms| N2
    N2 -.-> |~1ms| N3
    
    P1 -.-> |~0.5ms| P2
    P2 -.-> |~1ms| P3
    P3 -.-> |~0.5ms| P4
```

## Memory Usage Profile

```mermaid
pie title Memory Distribution (Total: ~125MB)
    "MCP Server (Node.js)" : 35
    "TypeScript Client" : 25
    "Daemon Process" : 40
    "Core Engine" : 20
    "Overhead & Buffers" : 5
```

## Process Communication

```mermaid
graph LR
    subgraph "IPC Methods"
        HTTP[HTTP/REST<br/>MCP ↔ TS Client]
        UNIX[Unix Socket<br/>TS Client ↔ Daemon]
        DIRECT[Direct Call<br/>Daemon ↔ Core]
    end
    
    subgraph "Data Formats"
        MCP_FMT[MCP Protocol]
        JSON[JSON]
        JSONRPC[JSON-RPC]
        CSTRUCT[C Structures]
    end
    
    MCP_FMT --> |Transform| JSON
    JSON --> |Wrap| JSONRPC
    JSONRPC --> |Parse| CSTRUCT
```

## Deployment Complexity

```mermaid
graph TB
    subgraph "Services to Deploy"
        NODE[Node.js Runtime]
        NPM[NPM Dependencies]
        DAEMON[Goxel Daemon Binary]
        CONFIG[Configuration Files]
    end
    
    subgraph "Service Management"
        PM2[PM2 for Node.js]
        SYSTEMD[systemd for Daemon]
        NGINX[Nginx for Proxy]
    end
    
    subgraph "Monitoring"
        LOGS1[MCP Server Logs]
        LOGS2[Daemon Logs]
        METRICS[Performance Metrics]
        HEALTH[Health Checks]
    end
    
    NODE --> PM2
    DAEMON --> SYSTEMD
    PM2 --> LOGS1
    SYSTEMD --> LOGS2
    LOGS1 --> METRICS
    LOGS2 --> METRICS
```

---

**Analysis**: The current architecture introduces significant complexity through multiple layers, each adding latency and resource overhead. The 4-layer approach was designed for modularity but results in:

- **4 serialization steps** per request
- **3 network hops** (even on localhost)
- **~125MB memory overhead**
- **5-10ms total latency**
- **Complex deployment** requiring multiple services

**Next**: [Target Architecture Diagrams](TARGET_ARCHITECTURE.md) →

**Last Updated**: January 29, 2025  
**Version**: 1.0.0  
**Author**: Lisa Thompson