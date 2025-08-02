# Goxel v15.0 Production Deployment Guide

## Overview

This guide covers deploying Goxel v15.0 daemon in production environments. v15.0 resolves the architectural limitations of v14.0, providing stable multi-request handling suitable for enterprise deployments.

## System Requirements

### Hardware
- **CPU**: 2+ cores recommended
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 1GB for application + project data

### Software
- **OS**: Linux (Ubuntu 18.04+, CentOS 7+), macOS 10.15+, Windows 10+ (WSL2)
- **Dependencies**: OpenGL 3.3+, GLFW3, libpng

## Installation Methods

### 1. Package Managers

#### Homebrew (macOS/Linux)
```bash
brew tap jimmy/goxel
brew install jimmy/goxel/goxel
```

#### APT (Ubuntu/Debian)
```bash
# Add repository
sudo add-apt-repository ppa:goxel/stable
sudo apt update

# Install
sudo apt install goxel-daemon
```

### 2. Docker Container
```dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y \
    goxel-daemon \
    && rm -rf /var/lib/apt/lists/*
    
EXPOSE 8080
CMD ["goxel-daemon", "--tcp", "0.0.0.0:8080"]
```

### 3. Build from Source
```bash
git clone https://github.com/guillaumechereau/goxel.git
cd goxel
scons daemon=1 production=1
sudo make install
```

## Service Configuration

### systemd (Linux)

Create `/etc/systemd/system/goxel-daemon.service`:
```ini
[Unit]
Description=Goxel v15.0 Daemon
After=network.target

[Service]
Type=forking
User=goxel
Group=goxel
ExecStart=/usr/bin/goxel-daemon \
    --socket /var/run/goxel/goxel.sock \
    --logfile /var/log/goxel/daemon.log \
    --pidfile /var/run/goxel/goxel.pid \
    --max-connections 100 \
    --worker-threads 8
    
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable goxel-daemon
sudo systemctl start goxel-daemon
```

### launchd (macOS)

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
        <string>--socket</string>
        <string>/var/run/goxel.sock</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
```

## Configuration Options

### Daemon Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--socket` | `/tmp/goxel.sock` | Unix socket path |
| `--tcp` | disabled | TCP endpoint (e.g., `0.0.0.0:8080`) |
| `--max-connections` | 50 | Maximum concurrent connections |
| `--worker-threads` | CPU cores | Number of worker threads |
| `--idle-timeout` | 300 | Project cleanup timeout (seconds) |
| `--logfile` | stdout | Log file path |
| `--loglevel` | info | Log level (debug/info/warn/error) |

### Performance Tuning

#### Linux Kernel Parameters
```bash
# /etc/sysctl.conf
net.core.somaxconn = 1024
net.ipv4.tcp_max_syn_backlog = 2048
fs.file-max = 65535
```

#### Resource Limits
```bash
# /etc/security/limits.conf
goxel soft nofile 65535
goxel hard nofile 65535
```

## Monitoring

### Health Check Endpoint
```bash
curl --unix-socket /var/run/goxel.sock \
     -X POST \
     -d '{"jsonrpc":"2.0","method":"health_check","id":1}'
```

### Prometheus Metrics
Enable metrics export:
```bash
goxel-daemon --metrics-port 9090
```

### Logging
Configure structured logging:
```bash
goxel-daemon --log-format json --logfile /var/log/goxel/daemon.json
```

## Security

### File Permissions
```bash
# Socket permissions
chmod 660 /var/run/goxel/goxel.sock
chown goxel:goxel /var/run/goxel/goxel.sock

# Log directory
mkdir -p /var/log/goxel
chown goxel:goxel /var/log/goxel
chmod 750 /var/log/goxel
```

### Network Security
For TCP deployments:
```bash
# Firewall rules (iptables)
iptables -A INPUT -p tcp --dport 8080 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 8080 -j DROP
```

### TLS Support
```bash
goxel-daemon \
    --tcp 0.0.0.0:8443 \
    --tls-cert /etc/goxel/cert.pem \
    --tls-key /etc/goxel/key.pem
```

## High Availability

### Load Balancing
Use HAProxy for multiple daemon instances:
```
backend goxel_daemons
    balance roundrobin
    server daemon1 127.0.0.1:8081 check
    server daemon2 127.0.0.1:8082 check
    server daemon3 127.0.0.1:8083 check
```

### Clustering
Configure shared storage for project data:
```bash
goxel-daemon --data-dir /mnt/shared/goxel-data
```

## Backup & Recovery

### Automated Backups
```bash
#!/bin/bash
# /etc/cron.daily/goxel-backup
BACKUP_DIR="/backup/goxel/$(date +%Y%m%d)"
mkdir -p "$BACKUP_DIR"
cp -r /var/lib/goxel/* "$BACKUP_DIR/"
find /backup/goxel -mtime +30 -delete
```

## Troubleshooting

### Common Issues

1. **Socket Permission Denied**
   ```bash
   sudo chmod 666 /var/run/goxel.sock
   ```

2. **High Memory Usage**
   ```bash
   # Adjust cache settings
   goxel-daemon --max-cache-size 1024M
   ```

3. **Performance Issues**
   ```bash
   # Enable profiling
   goxel-daemon --profile /tmp/goxel.prof
   ```

## Support

- Documentation: https://goxel.xyz/docs
- Issues: https://github.com/guillaumechereau/goxel/issues
- Enterprise Support: support@goxel.xyz