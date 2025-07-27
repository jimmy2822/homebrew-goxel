# Goxel v14 Daemon Deployment Configuration

This directory contains production-grade deployment configurations for the Goxel v14 daemon architecture.

## Configuration Files

### Core Configuration

- **`daemon_config.json`** - Unified JSON configuration file with all daemon settings
- **`goxel-daemon.service`** - Systemd service definition for Linux deployments
- **`com.goxel.daemon.plist`** - LaunchDaemon configuration for macOS deployments

### Environment-Specific Settings

The configuration system supports platform-specific defaults and environment variable overrides:

#### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `GOXEL_DAEMON_BINARY` | Path to daemon executable | Platform-specific |
| `GOXEL_SOCKET_PATH` | Unix socket path | Platform-specific |
| `GOXEL_PID_FILE` | Process ID file path | Platform-specific |
| `GOXEL_LOG_FILE` | Log file path | Platform-specific |
| `GOXEL_MAX_WORKERS` | Number of worker threads | `4` |
| `GOXEL_QUEUE_SIZE` | Request queue size | `1024` |
| `GOXEL_MAX_CONNECTIONS` | Maximum concurrent connections | `256` |

## Platform Deployment

### Linux (systemd)

1. **Install daemon binary:**
   ```bash
   sudo cp goxel-daemon /usr/bin/
   sudo chmod +x /usr/bin/goxel-daemon
   ```

2. **Create user and directories:**
   ```bash
   sudo useradd -r -s /bin/false goxel
   sudo mkdir -p /var/lib/goxel /var/log /var/run
   sudo chown goxel:goxel /var/lib/goxel
   ```

3. **Install systemd service:**
   ```bash
   sudo cp config/goxel-daemon.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable goxel-daemon
   sudo systemctl start goxel-daemon
   ```

4. **Verify deployment:**
   ```bash
   sudo systemctl status goxel-daemon
   scripts/daemon_control.sh health
   ```

### macOS (LaunchDaemon)

1. **Install daemon binary:**
   ```bash
   sudo cp goxel-daemon /usr/local/bin/
   sudo chmod +x /usr/local/bin/goxel-daemon
   ```

2. **Create user and directories:**
   ```bash
   sudo dscl . -create /Users/_goxel
   sudo dscl . -create /Users/_goxel UniqueID 301
   sudo dscl . -create /Users/_goxel PrimaryGroupID 301
   sudo mkdir -p /usr/local/var/lib/goxel /usr/local/var/log /usr/local/var/run
   sudo chown _goxel:_goxel /usr/local/var/lib/goxel
   ```

3. **Install LaunchDaemon:**
   ```bash
   sudo cp config/com.goxel.daemon.plist /Library/LaunchDaemons/
   sudo launchctl load /Library/LaunchDaemons/com.goxel.daemon.plist
   ```

4. **Verify deployment:**
   ```bash
   sudo launchctl list | grep goxel
   scripts/daemon_control.sh health
   ```

### Docker Deployment

Create a `Dockerfile`:

```dockerfile
FROM debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    libc6 \
    libgcc-s1 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Create goxel user
RUN useradd -r -s /bin/false goxel

# Copy daemon binary
COPY goxel-daemon /usr/bin/
RUN chmod +x /usr/bin/goxel-daemon

# Create directories
RUN mkdir -p /var/lib/goxel /var/log /var/run && \
    chown goxel:goxel /var/lib/goxel

# Copy configuration
COPY config/daemon_config.json /etc/goxel/

USER goxel
EXPOSE 8080

CMD ["/usr/bin/goxel-daemon", "--foreground", "--config", "/etc/goxel/daemon_config.json"]
```

## MCP Server Integration

For MCP server integration (Agent-3's work), use the `mcp_daemon_manager.js` module:

```javascript
const { GoxelDaemonManager } = require('./scripts/mcp_daemon_manager.js');

// Initialize daemon manager with auto-start
const daemonManager = new GoxelDaemonManager({
    autoStart: true,
    restartOnFailure: true,
    verbose: true
});

// Start daemon for MCP server
await daemonManager.start();

// Get daemon status
const status = await daemonManager.getStatus();
console.log('Daemon status:', status);
```

### Environment Variables for MCP Integration

```bash
export GOXEL_SOCKET_PATH="/tmp/goxel-daemon.sock"
export GOXEL_MAX_WORKERS="4"
export GOXEL_STARTUP_TIMEOUT="30000"
export GOXEL_HEALTH_TIMEOUT="5000"
export MCP_SERVER_PORT="3000"
```

## Security Configuration

### File Permissions

- Socket file: `0660` (user and group read/write)
- PID file: `0644` (world readable)
- Log file: `0640` (user read/write, group read)
- Configuration: `0640` (sensitive settings)

### systemd Security Features

The systemd service includes hardening features:

- `NoNewPrivileges=true` - Prevents privilege escalation
- `PrivateTmp=true` - Private /tmp directory
- `ProtectHome=true` - No access to user home directories
- `ProtectSystem=strict` - Read-only system directories
- `RestrictSUIDSGID=true` - No SUID/SGID execution
- `SystemCallFilter=@system-service` - Restricted system calls

## Monitoring and Logging

### Log Management

Logs are structured JSON by default:

```json
{
  "timestamp": "2025-01-26T10:30:00.000Z",
  "level": "info",
  "component": "daemon-main",
  "message": "Daemon started successfully",
  "pid": 12345,
  "socket": "/var/run/goxel-daemon.sock"
}
```

### Health Monitoring

The daemon provides health check endpoints:

```bash
# Check daemon health
scripts/daemon_control.sh health

# Get detailed status
scripts/daemon_control.sh status
```

### Performance Metrics

Monitor key metrics:

- Request processing time
- Queue depth
- Worker thread utilization  
- Memory usage
- Socket connection count

## Troubleshooting

### Common Issues

1. **Permission denied on socket:**
   ```bash
   sudo chown goxel:goxel /var/run/goxel-daemon.sock
   sudo chmod 660 /var/run/goxel-daemon.sock
   ```

2. **Daemon won't start:**
   ```bash
   # Check binary permissions
   ls -la /usr/bin/goxel-daemon
   
   # Check configuration
   sudo -u goxel /usr/bin/goxel-daemon --test-lifecycle
   ```

3. **High memory usage:**
   - Adjust `max_memory_mb` in configuration
   - Enable memory pooling
   - Reduce worker thread count

4. **Socket connection refused:**
   - Verify socket path exists
   - Check daemon is running
   - Test with `daemon_control.sh health`

### Log Analysis

```bash
# View recent logs
journalctl -u goxel-daemon -f

# Search for errors
journalctl -u goxel-daemon --grep="ERROR|FATAL"

# View daemon performance metrics
tail -f /var/log/goxel-daemon.log | jq '.level, .message, .processing_time_ms'
```

## Production Deployment Checklist

- [ ] Daemon binary installed with correct permissions
- [ ] Service user created with minimal privileges
- [ ] Required directories created with proper ownership
- [ ] Configuration files deployed and validated
- [ ] Service installed and enabled for auto-start
- [ ] Health checks passing
- [ ] Log rotation configured
- [ ] Monitoring alerts configured
- [ ] Security hardening applied
- [ ] Backup procedures documented

## Support

For deployment issues or questions:

1. Check logs: `journalctl -u goxel-daemon`
2. Run health check: `scripts/daemon_control.sh health`
3. Test daemon binary: `goxel-daemon --test-lifecycle`
4. Review configuration: Validate JSON syntax in `daemon_config.json`

This deployment configuration provides enterprise-grade reliability and security for Goxel v14 daemon operations.