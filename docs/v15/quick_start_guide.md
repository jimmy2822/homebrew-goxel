# Goxel v15.0 Quick Start Guide

## Overview

Goxel v15.0 introduces a production-ready daemon architecture that resolves the limitations of v14.0, providing true multi-request support and enhanced stability.

## Installation

### Homebrew (Recommended)
```bash
# Install Goxel v15.0
brew tap jimmy/goxel
brew install jimmy/goxel/goxel

# Start daemon service
brew services start goxel
```

### Manual Build
```bash
# Clone repository
git clone https://github.com/guillaumechereau/goxel.git
cd goxel

# Build daemon
scons daemon=1

# Run daemon
./goxel-daemon --foreground --socket /tmp/goxel.sock
```

## Basic Usage

### Starting the Daemon
```bash
# Foreground mode (for testing)
goxel-daemon --foreground --socket /tmp/goxel.sock

# Background mode (production)
goxel-daemon --socket /tmp/goxel.sock --logfile /var/log/goxel-daemon.log
```

### Python Client Example
```python
import socket
import json

def send_request(method, params=None):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect('/tmp/goxel.sock')
    
    request = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params or {},
        "id": 1
    }
    
    sock.send(json.dumps(request).encode() + b'\n')
    response = sock.recv(4096).decode()
    sock.close()
    
    return json.loads(response)

# Create a new project
result = send_request("goxel.create_project", {
    "width": 256,
    "height": 256,
    "depth": 256
})
print(f"Project created: {result}")

# Add voxels
result = send_request("goxel.add_voxels", {
    "voxels": [[128, 128, 128]],
    "color": [255, 0, 0, 255]
})
print(f"Voxels added: {result}")
```

## Key Improvements in v15.0

1. **Multi-Request Support**: Process multiple requests without daemon hangs
2. **Enhanced Stability**: Improved memory management and thread safety
3. **Better Performance**: Optimized request handling and processing
4. **Production Ready**: Suitable for enterprise deployments

## Next Steps

- Read the [API Reference](daemon_api_reference.md) for complete method documentation
- See [Migration Guide](migration_guide.md) for upgrading from v14.x
- Check [Deployment Guide](deployment_guide.md) for production setup

## Support

For issues or questions:
- GitHub Issues: https://github.com/guillaumechereau/goxel/issues
- Documentation: https://goxel.xyz/docs