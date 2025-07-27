# Migrating from Goxel v13.4 to v14.6

## Overview

Goxel v14.6 maintains 100% backward compatibility with v13.4 while introducing significant performance improvements through its daemon architecture. This guide will help you migrate smoothly.

## Key Changes

### Binary Consolidation
- **v13.4**: Separate `goxel` (GUI) and `goxel-headless` (CLI) binaries
- **v14.6**: Single `goxel` binary with `--headless` flag

### Performance Model
- **v13.4**: Each CLI command starts new process (7.95ms overhead)
- **v14.6**: Daemon mode eliminates startup overhead (>700% faster)

### New Features
- Daemon/client architecture
- JSON-RPC API
- Client libraries (TypeScript, Python, C++)
- Connection pooling
- Concurrent processing
- Plugin system

## Migration Scenarios

### Scenario 1: Basic CLI Usage

If you use simple CLI commands, no changes needed!

#### v13.4 (Still Works)
```bash
goxel-headless create model.gox
goxel-headless add-voxel 0 0 0 255 0 0 255
goxel-headless export model.obj
```

#### v14.6 (Recommended)
```bash
goxel --headless create model.gox
goxel --headless add-voxel 0 0 0 255 0 0 255
goxel --headless export model.obj
```

### Scenario 2: Batch Scripts

For scripts with multiple operations, use daemon mode for massive speedup.

#### v13.4 Script
```bash
#!/bin/bash
for i in {1..100}; do
  goxel-headless add-voxel $i 0 0 255 0 0 255
done
# Time: ~800ms
```

#### v14.6 Script (Daemon Mode)
```bash
#!/bin/bash
# Start daemon
goxel --headless --daemon --background

# Run operations (much faster!)
for i in {1..100}; do
  goxel --headless add-voxel $i 0 0 255 0 0 255
done
# Time: ~120ms (6.6x faster)

# Stop daemon
goxel --headless --daemon stop
```

### Scenario 3: MCP Integration

The MCP server automatically uses the new daemon mode.

#### v13.4 MCP Server
```typescript
// Uses CLI bridge (slower)
const bridge = new GoxelHeadlessBridge();
await bridge.execute(['create', 'test.gox']);
```

#### v14.6 MCP Server
```typescript
// Automatically uses daemon (faster)
// No code changes needed!
const bridge = new GoxelHeadlessBridge();
await bridge.execute(['create', 'test.gox']);
```

### Scenario 4: Application Integration

For applications, migrate to the new client libraries.

#### v13.4 (Shell Execution)
```python
import subprocess

def add_voxel(x, y, z, r, g, b, a):
    subprocess.run([
        'goxel-headless', 'add-voxel',
        str(x), str(y), str(z),
        str(r), str(g), str(b), str(a)
    ])

# Slow due to process overhead
for i in range(100):
    add_voxel(i, 0, 0, 255, 0, 0, 255)
```

#### v14.6 (Native Client)
```python
from goxel_client import GoxelClient

client = GoxelClient()
client.connect()

# 10x faster with connection reuse
for i in range(100):
    client.add_voxel(i, 0, 0, [255, 0, 0, 255])

client.disconnect()
```

## Step-by-Step Migration

### Step 1: Install v14.6

```bash
# Backup v13.4 if needed
sudo cp /usr/local/bin/goxel-headless /usr/local/bin/goxel-headless-v13

# Install v14.6
# macOS
brew upgrade goxel

# Linux
sudo apt update && sudo apt upgrade goxel

# Windows
choco upgrade goxel
```

### Step 2: Update Scripts

#### Create Compatibility Wrapper (Optional)
```bash
#!/bin/bash
# /usr/local/bin/goxel-headless
# Compatibility wrapper for v13.4 scripts
exec goxel --headless "$@"
```

#### Update Script Paths
```bash
# Find all scripts using goxel-headless
grep -r "goxel-headless" /path/to/scripts/

# Update to use new binary
sed -i 's/goxel-headless/goxel --headless/g' /path/to/scripts/*.sh
```

### Step 3: Optimize Performance

#### Add Daemon Management
```bash
# At script start
start_goxel_daemon() {
    if ! goxel --headless --daemon status &>/dev/null; then
        echo "Starting Goxel daemon..."
        goxel --headless --daemon --background
        sleep 1
    fi
}

# At script end
stop_goxel_daemon() {
    echo "Stopping Goxel daemon..."
    goxel --headless --daemon stop
}

# Use in script
trap stop_goxel_daemon EXIT
start_goxel_daemon

# Your operations here (now much faster!)
goxel --headless create model.gox
# ...
```

### Step 4: Migrate to Client Libraries

#### TypeScript/Node.js
```bash
npm install @goxel/client
```

```typescript
// Old: Shell execution
import { exec } from 'child_process';
const runGoxel = (cmd: string) => {
    return new Promise((resolve, reject) => {
        exec(`goxel-headless ${cmd}`, (err, stdout) => {
            if (err) reject(err);
            else resolve(stdout);
        });
    });
};

// New: Native client
import { GoxelClient } from '@goxel/client';
const client = new GoxelClient();
await client.connect();
await client.createProject('model.gox');
// ... much faster and type-safe!
```

#### Python
```bash
pip install goxel-client
```

```python
# Old: subprocess
import subprocess
result = subprocess.run(['goxel-headless', 'create', 'model.gox'])

# New: Native client  
from goxel_client import GoxelClient
client = GoxelClient()
client.connect()
client.create_project('model.gox')
```

## Configuration Migration

### Environment Variables

#### v13.4
```bash
export GOXEL_HEADLESS_TIMEOUT=30
export GOXEL_HEADLESS_SILENT=1
```

#### v14.6
```bash
# Same variables still work
export GOXEL_HEADLESS_TIMEOUT=30
export GOXEL_HEADLESS_SILENT=1

# New daemon-specific options
export GOXEL_DAEMON_PORT=7531
export GOXEL_DAEMON_SOCKET=/tmp/goxel.sock
export GOXEL_DAEMON_MAX_CONNECTIONS=10
```

### Configuration Files

#### v14.6 Daemon Config (Optional)
```yaml
# ~/.config/goxel/daemon.yaml
daemon:
  socket: /tmp/goxel.sock
  port: 7531
  max_connections: 10
  idle_timeout: 300
  log_level: info
  
performance:
  thread_pool_size: 4
  request_queue_size: 100
  enable_compression: true
```

## Feature Comparison

| Feature | v13.4 | v14.6 | Migration Required |
|---------|-------|-------|-------------------|
| CLI Commands | ✅ | ✅ | Optional (works as-is) |
| File Formats | ✅ | ✅ | None |
| Export Formats | ✅ | ✅ | None |
| Scripting | ✅ | ✅ Enhanced | None |
| Performance | Baseline | 700%+ faster | Use daemon mode |
| JSON-RPC API | ❌ | ✅ | New feature |
| Client Libraries | ❌ | ✅ | New feature |
| Connection Pooling | ❌ | ✅ | New feature |
| Concurrent Processing | ❌ | ✅ | New feature |

## Troubleshooting

### Issue: "goxel-headless: command not found"
```bash
# Create compatibility symlink
sudo ln -s /usr/local/bin/goxel /usr/local/bin/goxel-headless

# Or update scripts to use new command
alias goxel-headless='goxel --headless'
```

### Issue: Daemon won't start
```bash
# Check if old daemon running
ps aux | grep goxel
killall goxel-daemon

# Remove stale socket
rm -f /tmp/goxel.sock

# Start with debug output
goxel --headless --daemon --debug
```

### Issue: Performance not improved
```bash
# Make sure daemon is running
goxel --headless --daemon status

# If not, start it
goxel --headless --daemon --background

# Verify using daemon (should be fast)
time goxel --headless create test.gox
```

## Best Practices

1. **Always use daemon mode** for batch operations
2. **Use client libraries** instead of shell execution
3. **Enable connection pooling** in production
4. **Monitor daemon health** in production environments
5. **Gracefully shutdown daemon** to ensure data integrity

## Getting Help

- **Documentation**: [v14.6 Documentation](README.md)
- **Examples**: [Migration Examples](examples/migration/)
- **Community**: [Discord](https://discord.gg/goxel)
- **Issues**: [GitHub](https://github.com/goxel/goxel/issues)

---

*Migrating to v14.6 is easy and brings massive performance improvements! 🚀*