# Goxel v14 Daemon Script Execution

## Overview

The Goxel v14 daemon now supports non-blocking JavaScript script execution through the `goxel.execute_script` JSON-RPC method. This feature allows you to run complex voxel manipulation scripts asynchronously without blocking other daemon operations.

## Features

- **Non-blocking execution**: Scripts run in separate worker threads
- **Timeout support**: Configurable execution timeouts to prevent runaway scripts
- **Multiple execution modes**: Execute from string or file
- **Thread-safe**: Multiple scripts can run concurrently
- **Resource limits**: Sandboxed execution with resource constraints
- **QuickJS integration**: Full JavaScript support with Goxel API bindings

## API Reference

### Method: `goxel.execute_script`

Execute JavaScript code asynchronously in the Goxel environment.

#### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `script` | string | No* | JavaScript code to execute |
| `path` | string | No* | Path to JavaScript file to execute |
| `name` | string | No | Source name for debugging (default: "<inline>" or filename) |
| `timeout_ms` | integer | No | Execution timeout in milliseconds (default: 30000, max: 300000) |

*Note: Either `script` or `path` must be provided, but not both.

#### Response

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "success": true,
    "code": 0,
    "message": "Script executed successfully"
  }
}
```

#### Error Response

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": -32603,
    "message": "Script execution timeout"
  }
}
```

## Examples

### Execute Script from String

```python
import json
import socket

# Connect to daemon
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/goxel.sock")

# JavaScript code to create a cube
script_code = """
// Create a 5x5x5 red cube at origin
for (var x = 0; x < 5; x++) {
    for (var y = 0; y < 5; y++) {
        for (var z = 0; z < 5; z++) {
            goxel.addVoxel(x, y, z, [255, 0, 0, 255]);
        }
    }
}
console.log("Created 5x5x5 red cube");
"""

# Send request
request = {
    "jsonrpc": "2.0",
    "method": "goxel.execute_script",
    "params": {
        "script": script_code,
        "name": "create_cube.js"
    },
    "id": 1
}

sock.sendall((json.dumps(request) + "\n").encode())
response = json.loads(sock.recv(4096).decode())
print(response)
```

### Execute Script from File

```python
request = {
    "jsonrpc": "2.0",
    "method": "goxel.execute_script",
    "params": {
        "path": "/path/to/script.js",
        "timeout_ms": 60000  # 1 minute timeout
    },
    "id": 2
}
```

### Concurrent Script Execution

Multiple scripts can run simultaneously without blocking each other:

```python
import threading

def execute_script(script_id):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect("/tmp/goxel.sock")
    
    request = {
        "jsonrpc": "2.0",
        "method": "goxel.execute_script",
        "params": {
            "script": f"console.log('Script {script_id} running');"
        },
        "id": script_id
    }
    
    sock.sendall((json.dumps(request) + "\n").encode())
    response = json.loads(sock.recv(4096).decode())
    print(f"Script {script_id}: {response}")
    sock.close()

# Start 5 scripts concurrently
threads = []
for i in range(5):
    t = threading.Thread(target=execute_script, args=(i,))
    t.start()
    threads.append(t)

# Wait for all to complete
for t in threads:
    t.join()
```

## Script Environment

Scripts have access to the full Goxel JavaScript API:

```javascript
// Available global objects and functions
goxel.addVoxel(x, y, z, [r, g, b, a])
goxel.removeVoxel(x, y, z)
goxel.getVoxel(x, y, z)
goxel.paintVoxels(positions, color)
goxel.floodFill(x, y, z, color)
// ... and more

// Standard JavaScript features
console.log("Debug output")
Math.random()
JSON.stringify(data)
// etc.
```

## Implementation Details

### Worker Pool Architecture

The script execution handler uses a dedicated worker pool separate from the main RPC worker pool:

- **Script Worker Pool**: 4 threads dedicated to script execution
- **Queue Capacity**: 100 pending script requests
- **Priority Queue**: Enabled for script prioritization
- **Thread Safety**: Each worker thread has its own QuickJS context

### Execution Flow

1. Script request received via JSON-RPC
2. Request queued in script worker pool
3. Worker thread picks up request
4. QuickJS engine initialized (if needed)
5. Script executed with timeout monitoring
6. Result returned to client
7. Resources cleaned up

### Security Considerations

- Scripts run in sandboxed QuickJS environment
- No filesystem access beyond script loading
- No network access from scripts
- Resource limits enforced (memory, CPU time)
- Timeout enforcement prevents infinite loops

## Performance

- **Startup Time**: <10ms per script
- **Throughput**: 100+ scripts/second (small scripts)
- **Concurrency**: Up to 4 scripts running simultaneously
- **Memory**: ~1MB per active script context

## Troubleshooting

### Script Execution Fails

1. Check daemon logs for errors
2. Verify script syntax
3. Ensure Goxel context is initialized
4. Check available worker threads

### Timeout Errors

- Increase `timeout_ms` parameter
- Optimize script for better performance
- Break large scripts into smaller chunks

### Worker Pool Exhausted

- Reduce concurrent script submissions
- Implement client-side queuing
- Monitor worker pool statistics

## Future Enhancements

- Script result caching
- Script library management
- Debugger integration
- Performance profiling
- Extended API bindings