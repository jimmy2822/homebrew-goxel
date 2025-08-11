# Goxel v0.16 Troubleshooting Guide

## Common Issues and Solutions

This guide helps resolve common issues with the v0.16 render transfer architecture.

---

## Render Manager Issues

### "Render manager not initialized"

**Symptoms:**
```json
{
  "error": {
    "code": -32603,
    "message": "Render manager not initialized"
  }
}
```

**Causes:**
- Daemon version is older than v0.16
- Render manager failed to initialize at startup

**Solutions:**

1. Check daemon version:
```bash
./goxel-daemon --version
# Should show 0.16.0 or higher
```

2. Check startup logs:
```bash
./goxel-daemon --foreground --socket /tmp/goxel.sock 2>&1 | grep render_manager
# Should see: "Render manager initialized successfully"
```

3. Rebuild with render manager support:
```bash
scons daemon=1 clean
scons daemon=1
```

---

### Files not being cleaned up

**Symptoms:**
- Disk space filling up
- Old render files persist beyond TTL

**Causes:**
- Cleanup thread not running
- TTL not expired yet
- Permission issues

**Solutions:**

1. Check cleanup thread status:
```python
stats = daemon.request('goxel.get_render_stats', {})
if not stats['result']['stats']['cleanup_thread_active']:
    print("WARNING: Cleanup thread is not active!")
```

2. Verify TTL settings:
```bash
echo $GOXEL_RENDER_TTL
# Default is 3600 (1 hour)
```

3. Check file permissions:
```bash
ls -la $GOXEL_RENDER_DIR
# Should show daemon user as owner
```

4. Manual cleanup:
```python
# List all renders
renders = daemon.request('goxel.list_renders', {})

# Clean up old ones
for render in renders['result']['renders']:
    if render['created_at'] < time.time() - 3600:  # Older than 1 hour
        daemon.request('goxel.cleanup_render', {'path': render['path']})
```

---

### "Cache size limit exceeded"

**Symptoms:**
```json
{
  "error": {
    "code": -32603,
    "message": "Cache size limit exceeded",
    "data": {
      "current_size": 1073741824,
      "max_size": 1073741824
    }
  }
}
```

**Solutions:**

1. Increase cache limit:
```bash
export GOXEL_RENDER_MAX_SIZE=2147483648  # 2GB
```

2. Reduce TTL to clean up faster:
```bash
export GOXEL_RENDER_TTL=1800  # 30 minutes
```

3. Clean up manually:
```python
daemon.request('goxel.cleanup_render', {'path': '/path/to/render.png'})
```

4. List and remove largest files:
```python
renders = daemon.request('goxel.list_renders', {})['result']['renders']
renders.sort(key=lambda r: r['size'], reverse=True)

# Remove largest 10%
for render in renders[:len(renders)//10]:
    daemon.request('goxel.cleanup_render', {'path': render['path']})
```

---

## File Access Issues

### "Permission denied" errors

**Symptoms:**
- Cannot read rendered files
- Cleanup fails with permission errors

**Solutions:**

1. Check directory permissions:
```bash
ls -ld $GOXEL_RENDER_DIR
# Should be writable by daemon user
```

2. Fix permissions:
```bash
sudo chown -R $(whoami) $GOXEL_RENDER_DIR
chmod 755 $GOXEL_RENDER_DIR
```

3. Use user-writable directory:
```bash
export GOXEL_RENDER_DIR=$HOME/.goxel/renders
mkdir -p $GOXEL_RENDER_DIR
```

---

### "File not found" after render

**Symptoms:**
- Render succeeds but file doesn't exist
- Path in response is invalid

**Causes:**
- File was cleaned up too quickly
- Wrong directory path
- File system issues

**Solutions:**

1. Check if file was cleaned up:
```python
info = daemon.request('goxel.get_render_info', {'path': file_path})
if not info['result']['success']:
    print("File was already cleaned up")
```

2. Increase TTL:
```bash
export GOXEL_RENDER_TTL=7200  # 2 hours
```

3. Copy file immediately after render:
```python
import shutil

result = daemon.request('goxel.render_scene', {...})
if result['result']['success']:
    src = result['result']['file']['path']
    dst = '/permanent/location/render.png'
    shutil.copy2(src, dst)
```

---

## Performance Issues

### Slow render operations

**Symptoms:**
- Renders take longer than expected
- High latency in responses

**Solutions:**

1. Check disk I/O:
```bash
iostat -x 1
# Look for high %util on disk
```

2. Use faster storage:
```bash
# Use SSD/NVMe
export GOXEL_RENDER_DIR=/fast/ssd/renders

# Use RAM disk for maximum speed
export GOXEL_RENDER_DIR=/dev/shm/goxel_renders
```

3. Reduce concurrent operations:
```python
# Limit concurrent renders
semaphore = threading.Semaphore(5)

def render_with_limit(daemon, params):
    with semaphore:
        return daemon.request('goxel.render_scene', params)
```

---

### High memory usage

**Symptoms:**
- Daemon consuming excessive memory
- System running out of RAM

**Solutions:**

1. Check current usage:
```python
stats = daemon.request('goxel.get_render_stats', {})
print(f"Cache size: {stats['result']['stats']['current_cache_size'] / 1024 / 1024:.1f} MB")
```

2. Reduce cache size:
```bash
export GOXEL_RENDER_MAX_SIZE=104857600  # 100MB only
```

3. Aggressive cleanup:
```bash
export GOXEL_RENDER_TTL=300  # 5 minutes
export GOXEL_RENDER_CLEANUP_INTERVAL=60  # Every minute
```

---

## Compatibility Issues

### Legacy code not working

**Symptoms:**
- Old render calls fail
- Unexpected parameter errors

**Solutions:**

1. Ensure backward compatibility mode:
```python
# This should always work
daemon.request('goxel.render_scene', ['output.png', 800, 600])
```

2. Check for typos in parameters:
```python
# Wrong - typo in "return_mode"
{'options': {'retrun_mode': 'file_path'}}  

# Correct
{'options': {'return_mode': 'file_path'}}
```

3. Use feature detection:
```python
def render_compatible(daemon, width, height):
    try:
        # Try new format
        return daemon.request('goxel.render_scene', {
            'width': width,
            'height': height,
            'options': {'return_mode': 'file_path'}
        })
    except:
        # Fall back to old format
        return daemon.request('goxel.render_scene', 
                            ['/tmp/output.png', width, height])
```

---

## Debugging Techniques

### Enable verbose logging

```bash
# Start daemon with verbose output
./goxel-daemon --foreground --verbose --socket /tmp/goxel.sock
```

### Monitor render manager stats

```python
import json
import time

def monitor_stats(daemon):
    while True:
        stats = daemon.request('goxel.get_render_stats', {})
        print(json.dumps(stats['result']['stats'], indent=2))
        time.sleep(5)
```

### Trace file operations

```bash
# Use strace to see file operations
strace -e open,unlink,stat ./goxel-daemon --socket /tmp/goxel.sock
```

### Check socket communication

```bash
# Monitor socket traffic
socat -v UNIX-LISTEN:/tmp/monitor.sock,fork UNIX-CONNECT:/tmp/goxel.sock
```

---

## Environment Variables Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `GOXEL_RENDER_DIR` | `/var/tmp/goxel_renders` | Render output directory |
| `GOXEL_RENDER_TTL` | `3600` | File TTL in seconds |
| `GOXEL_RENDER_MAX_SIZE` | `1073741824` | Max cache size in bytes |
| `GOXEL_RENDER_CLEANUP_INTERVAL` | `300` | Cleanup interval in seconds |

---

## Getting Help

If these solutions don't resolve your issue:

1. **Check logs**: Look for error messages in daemon output
2. **File an issue**: https://github.com/guillaumechereau/goxel/issues
3. **Include details**:
   - Daemon version: `./goxel-daemon --version`
   - Error messages
   - Minimal reproduction code
   - Environment variables
   - Operating system

---

## Quick Diagnostic Script

```python
#!/usr/bin/env python3
"""Quick diagnostic for v0.16 render issues"""

import socket
import json
import os
import time

def diagnose():
    print("Goxel v0.16 Diagnostic")
    print("=" * 40)
    
    # Check environment
    print("\nEnvironment Variables:")
    for var in ['GOXEL_RENDER_DIR', 'GOXEL_RENDER_TTL', 
                'GOXEL_RENDER_MAX_SIZE', 'GOXEL_RENDER_CLEANUP_INTERVAL']:
        value = os.environ.get(var, 'Not set')
        print(f"  {var}: {value}")
    
    # Try to connect
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.connect('/tmp/goxel.sock')
        print("\n✓ Connected to daemon")
        
        # Get stats
        request = json.dumps({
            'jsonrpc': '2.0',
            'method': 'goxel.get_render_stats',
            'params': {},
            'id': 1
        }) + '\n'
        
        sock.send(request.encode())
        response = json.loads(sock.recv(8192).decode())
        
        if 'result' in response:
            stats = response['result']['stats']
            print("\nRender Manager Stats:")
            print(f"  Active renders: {stats.get('active_renders', 'N/A')}")
            print(f"  Cache size: {stats.get('current_cache_size', 0) / 1024 / 1024:.1f} MB")
            print(f"  Cleanup thread: {'Active' if stats.get('cleanup_thread_active') else 'Inactive'}")
            print("\n✓ v0.16 features available")
        else:
            print("\n✗ v0.16 features not available")
            print("  Daemon may be running older version")
            
    except Exception as e:
        print(f"\n✗ Connection failed: {e}")
        print("  Check if daemon is running")
    finally:
        sock.close()

if __name__ == '__main__':
    diagnose()
```

---

*For additional support, see the [API Reference](API_REFERENCE.md) and [Migration Guide](MIGRATION_GUIDE.md)*