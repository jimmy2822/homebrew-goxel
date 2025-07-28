# Goxel v14.0 Daemon Deployment Guide

**Status**: 🚧 PREMATURE - DAEMON NOT READY FOR DEPLOYMENT  
**Version**: 14.0.0-alpha  
**Last Updated**: January 27, 2025

## ⚠️ Critical Warning

**DO NOT DEPLOY TO PRODUCTION**

The v14.0 daemon is in early alpha with no functional methods. This guide documents planned deployment procedures for when the daemon is complete. Currently:

- ❌ No working voxel operations
- ❌ No client libraries  
- ❌ No monitoring capabilities
- ❌ No production testing

**Continue using v13.4 CLI for all deployments.**

## 📋 Prerequisites

### Current Requirements
- Unix-like OS (Linux, macOS, BSD)
- C compiler and build tools
- ~ 50MB disk space
- Python 3 (for SCons)

### Missing Requirements (Not Yet Implemented)
- Working JSON-RPC methods
- Client libraries
- Monitoring tools
- systemd/launchd service files

## 🔨 Building the Daemon

### Basic Build (Works but Limited)
```bash
# Clone repository
git clone https://github.com/guillaumechereau/goxel.git
cd goxel

# Build daemon
scons daemon=1

# Note: Build may fail due to missing symbols
# See V14_ACTUAL_STATUS.md for stub function workarounds
```

### Build Issues and Workarounds
```bash
# If build fails with undefined symbols, create stubs.c:
cat > src/daemon/stubs.c << 'EOF'
// Temporary stubs for missing functions
#include <stdio.h>

void proc_clear_data(void) {}
void palette_reset(void) {}
void gui_init(void) {}
// ... add other missing functions as needed
EOF

# Add to build
echo "daemon_env.Append(SOURCES=['src/daemon/stubs.c'])" >> SConstruct
```

### Verify Build
```bash
# Check binary
./goxel-daemon --version
# Output: goxel-daemon version 14.0.0-daemon

# Test startup
./goxel-daemon --test-lifecycle
# Should show: Lifecycle management tests completed successfully
```

## 🚀 Basic Deployment (Development Only)

### Manual Start (Current Option)
```bash
# Start in foreground (recommended for alpha)
./goxel-daemon --foreground --verbose

# Start as background daemon (not recommended yet)
./goxel-daemon --daemonize --pid-file /var/run/goxel-daemon.pid
```

### Configuration Options
```bash
./goxel-daemon \
  --socket /tmp/goxel-daemon.sock \     # Unix socket path
  --workers 4 \                         # Worker threads
  --queue-size 1024 \                   # Request queue size
  --max-connections 256 \               # Max concurrent clients
  --log-file /var/log/goxel-daemon.log  # Log file path
```

### Verify Daemon Status
```bash
# Check if running
./goxel-daemon --status

# Test connection (no methods work)
echo '{"jsonrpc":"2.0","method":"test","id":1}' | nc -U /tmp/goxel-daemon.sock
# Returns: Method not found error
```

## 🐧 systemd Service (PLANNED)

### Service File (NOT TESTED)
Create `/etc/systemd/system/goxel-daemon.service`:

```ini
[Unit]
Description=Goxel 3D Voxel Editor Daemon
Documentation=https://github.com/guillaumechereau/goxel
After=network.target

[Service]
Type=forking
PIDFile=/var/run/goxel-daemon.pid
ExecStart=/usr/local/bin/goxel-daemon --daemonize \
  --pid-file /var/run/goxel-daemon.pid \
  --socket /var/run/goxel-daemon.sock \
  --log-file /var/log/goxel-daemon.log
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/usr/local/bin/goxel-daemon --stop
Restart=on-failure
RestartSec=5s

# Security settings (adjust as needed)
User=goxel
Group=goxel
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/run /var/log

[Install]
WantedBy=multi-user.target
```

### systemd Management (WHEN READY)
```bash
# DO NOT USE YET - DAEMON NOT READY

# Enable service
sudo systemctl enable goxel-daemon

# Start service
sudo systemctl start goxel-daemon

# Check status
sudo systemctl status goxel-daemon

# View logs
journalctl -u goxel-daemon -f
```

## 🍎 macOS launchd (PLANNED)

### Property List (NOT TESTED)
Create `/Library/LaunchDaemons/com.goxel.daemon.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" 
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.goxel.daemon</string>
  
  <key>ProgramArguments</key>
  <array>
    <string>/usr/local/bin/goxel-daemon</string>
    <string>--foreground</string>
    <string>--socket</string>
    <string>/var/run/goxel-daemon.sock</string>
  </array>
  
  <key>RunAtLoad</key>
  <true/>
  
  <key>KeepAlive</key>
  <dict>
    <key>SuccessfulExit</key>
    <false/>
  </dict>
  
  <key>StandardErrorPath</key>
  <string>/var/log/goxel-daemon.error.log</string>
  
  <key>StandardOutPath</key>
  <string>/var/log/goxel-daemon.out.log</string>
</dict>
</plist>
```

### launchd Management (WHEN READY)
```bash
# DO NOT USE YET - DAEMON NOT READY

# Load service
sudo launchctl load /Library/LaunchDaemons/com.goxel.daemon.plist

# Start/stop
sudo launchctl start com.goxel.daemon
sudo launchctl stop com.goxel.daemon

# Check status
sudo launchctl list | grep goxel
```

## 🐳 Docker Deployment (CONCEPTUAL)

### Dockerfile (UNTESTED)
```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    scons \
    libglfw3-dev \
    libpng-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Copy source
WORKDIR /app
COPY . .

# Build daemon
RUN scons daemon=1

# Runtime
EXPOSE 8080
CMD ["./goxel-daemon", "--foreground", "--socket", "/tmp/goxel-daemon.sock"]
```

### Docker Compose (CONCEPTUAL)
```yaml
version: '3.8'

services:
  goxel-daemon:
    build: .
    volumes:
      - /tmp/goxel-daemon.sock:/tmp/goxel-daemon.sock
      - ./projects:/data/projects
    environment:
      - GOXEL_WORKERS=8
      - GOXEL_MAX_CONNECTIONS=1024
    restart: unless-stopped
```

## 📊 Monitoring (NOT AVAILABLE)

### Current Monitoring Capabilities
- ❌ No metrics endpoint
- ❌ No health checks
- ❌ No performance data
- ✅ Basic process monitoring only

### Planned Monitoring
```bash
# Health check endpoint (NOT IMPLEMENTED)
curl http://localhost:8080/health

# Metrics endpoint (NOT IMPLEMENTED)  
curl http://localhost:8080/metrics

# Status via RPC (NOT IMPLEMENTED)
echo '{"jsonrpc":"2.0","method":"goxel.get_daemon_status","id":1}' | \
  nc -U /tmp/goxel-daemon.sock
```

## 🔒 Security Considerations

### Current Security Model
- Unix socket with file system permissions
- No authentication implemented
- No encryption
- No rate limiting

### Recommended Practices (When Functional)
1. **Restrict Socket Permissions**
   ```bash
   chmod 660 /var/run/goxel-daemon.sock
   chown goxel:goxel /var/run/goxel-daemon.sock
   ```

2. **Use Dedicated User**
   ```bash
   sudo useradd -r -s /bin/false goxel
   ```

3. **Limit Resource Usage**
   ```bash
   # In systemd service
   LimitNOFILE=4096
   LimitNPROC=64
   MemoryLimit=512M
   ```

## 🚨 Troubleshooting

### Daemon Won't Start
```bash
# Check for existing process
ps aux | grep goxel-daemon

# Check socket exists
ls -la /tmp/goxel-daemon.sock

# Remove stale socket
rm -f /tmp/goxel-daemon.sock

# Check permissions
ls -la $(which goxel-daemon)
```

### Connection Refused
```bash
# Verify daemon is running
./goxel-daemon --status

# Check socket path
strace -e connect nc -U /tmp/goxel-daemon.sock < /dev/null

# Test with verbose mode
./goxel-daemon --foreground --verbose
```

### Methods Not Working
**This is expected!** No methods are implemented yet. All method calls will return:
```json
{"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found"},"id":1}
```

## 📈 Performance Tuning (PREMATURE)

Performance cannot be tuned until methods are implemented. Current settings:

### Worker Threads
```bash
# Default: 4 threads
./goxel-daemon --workers 8  # More threads (when methods exist)
```

### Queue Size
```bash
# Default: 1024 entries
./goxel-daemon --queue-size 4096  # Larger queue (when needed)
```

### Connection Limits
```bash
# Default: 256 connections
./goxel-daemon --max-connections 1024  # More connections (when useful)
```

## 🔄 Migration from v13.4 (NOT READY)

### Current State
- v13.4 CLI is stable and functional
- v14.0 daemon has no working methods
- No migration path exists

### Future Migration Steps (PLANNED)
1. Implement all v13.4 CLI commands in daemon
2. Create compatibility layer
3. Test thoroughly
4. Gradual rollout

### Stay on v13.4
```bash
# Continue using this:
./goxel-headless create test.gox
./goxel-headless add-voxel 0 -16 0 255 0 0 255

# Not this (doesn't work):
echo '{"jsonrpc":"2.0","method":"goxel.create_project","params":{"name":"test"},"id":1}' | \
  nc -U /tmp/goxel-daemon.sock
```

## 📋 Deployment Checklist

### Before Deployment (ALL ITEMS FAIL)
- [ ] ❌ Verify all RPC methods work
- [ ] ❌ Run integration tests
- [ ] ❌ Benchmark performance
- [ ] ❌ Test under load
- [ ] ❌ Verify client libraries
- [ ] ❌ Check monitoring endpoints
- [ ] ❌ Review security settings
- [ ] ❌ Document known issues

### Current Deployment Readiness: 0%

## 🎯 Recommendations

### For System Administrators
1. **Do not deploy v14.0** - It provides no functionality
2. **Continue using v13.4** - Stable and tested
3. **Wait for beta** - Check back in 2-3 months

### For Developers
1. **Help implement methods** - Core functionality needed
2. **Create client libraries** - Essential for adoption
3. **Add monitoring** - Required for production

### For Decision Makers
1. **Adjust timelines** - Add 8-12 weeks
2. **Plan gradual migration** - When ready
3. **Maintain v13.4** - It's your production system

---

*This deployment guide will be updated as v14.0 development progresses. Do not attempt production deployment of the current alpha version.*