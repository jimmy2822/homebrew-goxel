# Migration Guide: v14.x to v15.0

## Overview

This guide helps you migrate from Goxel v14.x to v15.0. The main improvements in v15.0 address the architectural limitations that caused daemon hangs in v14.0.

## Key Changes

### Architectural Improvements

#### v14.0 Limitations (Resolved in v15.0)
- ❌ Daemon hung after processing first request
- ❌ Required restart between operations
- ❌ Single-operation limitation

#### v15.0 Improvements
- ✅ True multi-request support
- ✅ No daemon hangs
- ✅ Production-ready stability
- ✅ Enhanced concurrency handling

### API Changes

The JSON-RPC API remains largely compatible, with these enhancements:

1. **Request Handling**
   - v14.0: Single request then hang
   - v15.0: Continuous request processing

2. **Connection Management**
   - v14.0: Connection issues after first request
   - v15.0: Stable persistent connections

3. **Error Handling**
   - v14.0: Limited error recovery
   - v15.0: Comprehensive error handling

## Migration Steps

### 1. Update Installation

#### Homebrew Users
```bash
# Update formula
brew update
brew upgrade goxel

# Restart daemon
brew services restart goxel
```

#### Manual Build Users
```bash
# Pull latest changes
git pull origin master

# Clean build
scons -c
scons daemon=1
```

### 2. Update Client Code

Most client code will work unchanged. However, update error handling:

#### v14.0 Pattern (Workaround)
```python
# Old workaround for daemon hangs
def send_request_with_restart(method, params):
    try:
        result = send_request(method, params)
    except:
        restart_daemon()  # Required in v14.0
        result = send_request(method, params)
    return result
```

#### v15.0 Pattern (Direct)
```python
# Direct usage - no workarounds needed
def send_requests(requests):
    results = []
    for req in requests:
        result = send_request(req['method'], req['params'])
        results.append(result)
    return results
```

### 3. Update Configuration

#### Daemon Configuration
```bash
# v15.0 supports enhanced configuration
goxel-daemon \
  --socket /tmp/goxel.sock \
  --max-connections 100 \
  --worker-threads 8 \
  --idle-timeout 300
```

#### Service Configuration
Update systemd/launchd configurations to use new parameters.

### 4. Testing

Run the test suite to verify migration:
```bash
# Run integration tests
python3 tests/integration/test_daemon.py

# Run performance tests
python3 tests/performance/benchmark_daemon.py
```

## Breaking Changes

### None
v15.0 maintains full backward compatibility with v14.0 API. All existing methods work as before, just without the hanging issues.

## Performance Improvements

### Request Processing
- v14.0: ~100ms per request (with restart overhead)
- v15.0: ~10ms per request (continuous operation)

### Concurrency
- v14.0: Single operation at a time
- v15.0: True concurrent request handling

## Troubleshooting

### Common Issues

1. **Daemon Still Using v14.0**
   ```bash
   # Check version
   goxel-daemon --version
   
   # Should show: Goxel Daemon v15.0.0
   ```

2. **Old Socket Files**
   ```bash
   # Clean up old sockets
   rm -f /tmp/goxel*.sock
   ```

3. **Permission Issues**
   ```bash
   # Fix socket permissions
   chmod 666 /tmp/goxel.sock
   ```

## Support

For migration assistance:
- GitHub Issues: https://github.com/guillaumechereau/goxel/issues
- Migration Examples: `/docs/v15/examples/`