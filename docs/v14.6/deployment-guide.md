# Goxel v14.6 Production Deployment Guide

## Overview

This guide covers deploying Goxel v14.6 in production environments, including server setup, monitoring, scaling, and best practices.

## Deployment Architectures

### Single Server
```
┌─────────────────────────────────────────┐
│                Production Server                │
├─────────────────────────────────────────┤
│ ┌───────────────┐  ┌────────────────────┐ │
│ │ Goxel Daemon  │  │ Application Server │ │
│ │ Port: 7531    │◄─┤ (Node.js/Python)   │ │
│ └───────────────┘  └────────────────────┘ │
└─────────────────────────────────────────┘
```

### Load Balanced
```
┌────────────────┐
│ Load Balancer  │
└──────┬─────────┘
        ├───────────────────────┐
        ├────────┐              │
┌───────▼───────┐    ┌───────▼───────┐
│ Goxel Node 1   │    │ Goxel Node 2   │
│ Port: 7531     │    │ Port: 7531     │
└────────────────┘    └────────────────┘
```

### Container Orchestration
```
┌─────────────────────────────────────────┐
│            Kubernetes Cluster                   │
├─────────────────────────────────────────┤
│ ┌─────────────────────────────────────┐ │
│ │ Goxel Deployment (3 replicas)              │ │
│ │ ┌─────────┐ ┌─────────┐ ┌─────────┐ │ │
│ │ │ Pod 1   │ │ Pod 2   │ │ Pod 3   │ │ │
│ │ └─────────┘ └─────────┘ └─────────┘ │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

## System Requirements

### Minimum Requirements
- **CPU**: 2 cores
- **RAM**: 2GB
- **Storage**: 10GB
- **OS**: Linux (Ubuntu 20.04+), macOS 12+, Windows Server 2019+

### Recommended Production
- **CPU**: 4+ cores
- **RAM**: 8GB+
- **Storage**: 50GB+ SSD
- **Network**: 1Gbps

### Scaling Guidelines
- **1 core** handles ~500 concurrent connections
- **1GB RAM** supports ~100 active projects
- **10GB storage** for ~1000 typical projects

## Installation

### Linux (systemd)

#### 1. Install Binary
```bash
# Download and install
curl -L https://goxel.xyz/downloads/v14.6/goxel-linux-x64 -o /usr/local/bin/goxel
chmod +x /usr/local/bin/goxel

# Create daemon user
useradd -r -s /bin/false goxel-daemon
mkdir -p /var/lib/goxel
chown goxel-daemon:goxel-daemon /var/lib/goxel
```

#### 2. Create systemd Service
```ini
# /etc/systemd/system/goxel-daemon.service
[Unit]
Description=Goxel Voxel Editor Daemon
After=network.target

[Service]
Type=simple
User=goxel-daemon
Group=goxel-daemon
WorkingDirectory=/var/lib/goxel
Environment="GOXEL_DAEMON_PORT=7531"
Environment="GOXEL_DAEMON_SOCKET=/var/run/goxel/goxel.sock"
ExecStartPre=/bin/mkdir -p /var/run/goxel
ExecStartPre=/bin/chown goxel-daemon:goxel-daemon /var/run/goxel
ExecStart=/usr/local/bin/goxel --headless --daemon
Restart=always
RestartSec=5
KillMode=mixed
KillSignal=SIGTERM

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/goxel /var/run/goxel

[Install]
WantedBy=multi-user.target
```

#### 3. Enable and Start
```bash
systemctl daemon-reload
systemctl enable goxel-daemon
systemctl start goxel-daemon
systemctl status goxel-daemon
```

### Docker Deployment

#### Dockerfile
```dockerfile
# Dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libglu1-mesa \
    libpng16-16 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Goxel
RUN curl -L https://goxel.xyz/downloads/v14.6/goxel-linux-x64 -o /usr/local/bin/goxel \
    && chmod +x /usr/local/bin/goxel

# Create daemon user
RUN useradd -r -s /bin/false goxel

# Setup directories
RUN mkdir -p /var/lib/goxel /var/run/goxel \
    && chown -R goxel:goxel /var/lib/goxel /var/run/goxel

USER goxel
WORKDIR /var/lib/goxel

# Expose ports
EXPOSE 7531

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD goxel --headless --daemon status || exit 1

# Start daemon
CMD ["goxel", "--headless", "--daemon", "--port", "7531"]
```

#### docker-compose.yml
```yaml
version: '3.8'

services:
  goxel:
    build: .
    container_name: goxel-daemon
    restart: unless-stopped
    ports:
      - "7531:7531"
    volumes:
      - goxel-data:/var/lib/goxel
      - goxel-run:/var/run/goxel
    environment:
      - GOXEL_DAEMON_MAX_CONNECTIONS=50
      - GOXEL_DAEMON_LOG_LEVEL=info
    healthcheck:
      test: ["CMD", "goxel", "--headless", "--daemon", "status"]
      interval: 30s
      timeout: 10s
      retries: 3
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 4G
        reservations:
          cpus: '1'
          memory: 2G

volumes:
  goxel-data:
  goxel-run:
```

### Kubernetes Deployment

#### deployment.yaml
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: goxel-daemon
  labels:
    app: goxel
spec:
  replicas: 3
  selector:
    matchLabels:
      app: goxel
  template:
    metadata:
      labels:
        app: goxel
    spec:
      containers:
      - name: goxel
        image: goxel/daemon:14.6
        ports:
        - containerPort: 7531
          name: json-rpc
        env:
        - name: GOXEL_DAEMON_PORT
          value: "7531"
        - name: GOXEL_DAEMON_MAX_CONNECTIONS
          value: "100"
        resources:
          requests:
            memory: "2Gi"
            cpu: "1"
          limits:
            memory: "4Gi"
            cpu: "2"
        livenessProbe:
          exec:
            command:
            - goxel
            - --headless
            - --daemon
            - status
          initialDelaySeconds: 30
          periodSeconds: 30
        readinessProbe:
          tcpSocket:
            port: 7531
          initialDelaySeconds: 5
          periodSeconds: 10
---
apiVersion: v1
kind: Service
metadata:
  name: goxel-service
spec:
  selector:
    app: goxel
  ports:
  - protocol: TCP
    port: 7531
    targetPort: 7531
  type: LoadBalancer
```

## Configuration

### Environment Variables
```bash
# Daemon Configuration
GOXEL_DAEMON_PORT=7531              # TCP port
GOXEL_DAEMON_SOCKET=/tmp/goxel.sock # Unix socket path
GOXEL_DAEMON_MAX_CONNECTIONS=50     # Max concurrent connections
GOXEL_DAEMON_THREAD_POOL_SIZE=4     # Worker threads
GOXEL_DAEMON_REQUEST_TIMEOUT=30     # Request timeout (seconds)
GOXEL_DAEMON_IDLE_TIMEOUT=300       # Idle connection timeout

# Logging
GOXEL_DAEMON_LOG_LEVEL=info         # debug, info, warn, error
GOXEL_DAEMON_LOG_FILE=/var/log/goxel/daemon.log
GOXEL_DAEMON_LOG_MAX_SIZE=100M      # Log rotation size
GOXEL_DAEMON_LOG_MAX_FILES=10       # Keep N rotated files

# Performance
GOXEL_DAEMON_ENABLE_COMPRESSION=true
GOXEL_DAEMON_CACHE_SIZE=1000        # Project cache size
GOXEL_DAEMON_BATCH_QUEUE_SIZE=1000  # Max queued batch operations

# Security
GOXEL_DAEMON_AUTH_TOKEN=secret      # Authentication token
GOXEL_DAEMON_ALLOWED_IPS=0.0.0.0/0  # IP whitelist
GOXEL_DAEMON_TLS_CERT=/path/to/cert.pem
GOXEL_DAEMON_TLS_KEY=/path/to/key.pem
```

### Configuration File
```yaml
# /etc/goxel/daemon.yaml
daemon:
  port: 7531
  socket: /var/run/goxel/goxel.sock
  max_connections: 50
  thread_pool_size: 4
  
logging:
  level: info
  file: /var/log/goxel/daemon.log
  max_size: 100M
  max_files: 10
  format: json
  
performance:
  enable_compression: true
  cache_size: 1000
  batch_queue_size: 1000
  request_timeout: 30
  idle_timeout: 300
  
security:
  auth_enabled: true
  auth_token: "${GOXEL_AUTH_TOKEN}"
  allowed_ips:
    - 10.0.0.0/8
    - 172.16.0.0/12
    - 192.168.0.0/16
  tls:
    enabled: true
    cert: /etc/goxel/certs/server.crt
    key: /etc/goxel/certs/server.key
```

## Monitoring

### Health Checks

#### HTTP Health Endpoint
```bash
# Enable HTTP health endpoint
GOXEL_DAEMON_HEALTH_PORT=8080

# Check health
curl http://localhost:8080/health
```

#### Metrics Endpoint
```bash
# Prometheus metrics
curl http://localhost:8080/metrics
```

### Prometheus Integration
```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'goxel'
    static_configs:
      - targets: ['goxel-daemon:8080']
```

### Grafana Dashboard
```json
{
  "dashboard": {
    "title": "Goxel Daemon Monitoring",
    "panels": [
      {
        "title": "Request Rate",
        "targets": [
          {
            "expr": "rate(goxel_requests_total[5m])"
          }
        ]
      },
      {
        "title": "Active Connections",
        "targets": [
          {
            "expr": "goxel_connections_active"
          }
        ]
      },
      {
        "title": "Memory Usage",
        "targets": [
          {
            "expr": "goxel_memory_bytes"
          }
        ]
      }
    ]
  }
}
```

### Logging

#### Structured Logging
```json
{
  "timestamp": "2025-01-27T10:30:45.123Z",
  "level": "info",
  "message": "Request processed",
  "method": "add_voxel",
  "duration_ms": 1.23,
  "client_id": "client_123",
  "request_id": "req_456"
}
```

#### Log Aggregation (ELK Stack)
```yaml
# filebeat.yml
filebeat.inputs:
- type: log
  enabled: true
  paths:
    - /var/log/goxel/*.log
  json.keys_under_root: true
  json.add_error_key: true
  
output.elasticsearch:
  hosts: ["elasticsearch:9200"]
```

## Performance Tuning

### Linux Kernel Parameters
```bash
# /etc/sysctl.d/99-goxel.conf
# Increase socket buffer sizes
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728

# Increase connection backlog
net.core.somaxconn = 4096
net.ipv4.tcp_max_syn_backlog = 4096

# Enable TCP keepalive
net.ipv4.tcp_keepalive_time = 60
net.ipv4.tcp_keepalive_intvl = 10
net.ipv4.tcp_keepalive_probes = 6

# Apply settings
sysctl -p /etc/sysctl.d/99-goxel.conf
```

### Resource Limits
```bash
# /etc/security/limits.d/goxel.conf
goxel-daemon soft nofile 65536
goxel-daemon hard nofile 65536
goxel-daemon soft nproc 32768
goxel-daemon hard nproc 32768
```

## Security

### TLS Configuration
```bash
# Generate self-signed certificate (for testing)
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# Production: Use Let's Encrypt
certbot certonly --standalone -d goxel.example.com
```

### Firewall Rules
```bash
# UFW (Ubuntu)
ufw allow 7531/tcp comment 'Goxel Daemon'
ufw allow from 10.0.0.0/8 to any port 7531

# iptables
iptables -A INPUT -p tcp --dport 7531 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 7531 -j DROP
```

### Authentication
```yaml
# Client configuration
client:
  auth_token: "your-secret-token"
  tls:
    verify: true
    ca_cert: /path/to/ca.crt
```

## Backup and Recovery

### Backup Strategy
```bash
#!/bin/bash
# backup-goxel.sh

BACKUP_DIR="/backup/goxel/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

# Backup data
rsync -av /var/lib/goxel/ "$BACKUP_DIR/data/"

# Backup configuration
cp -r /etc/goxel/ "$BACKUP_DIR/config/"

# Create manifest
cat > "$BACKUP_DIR/manifest.json" << EOF
{
  "version": "14.6",
  "timestamp": "$(date -Iseconds)",
  "hostname": "$(hostname)"
}
EOF

# Compress
tar -czf "$BACKUP_DIR.tar.gz" -C "$(dirname $BACKUP_DIR)" "$(basename $BACKUP_DIR)"
rm -rf "$BACKUP_DIR"
```

### Restore Procedure
```bash
#!/bin/bash
# restore-goxel.sh

BACKUP_FILE="$1"
if [ -z "$BACKUP_FILE" ]; then
  echo "Usage: $0 <backup-file.tar.gz>"
  exit 1
fi

# Stop daemon
systemctl stop goxel-daemon

# Extract backup
tar -xzf "$BACKUP_FILE" -C /tmp/

# Restore data
rsync -av /tmp/goxel_*/data/ /var/lib/goxel/

# Restore config
cp -r /tmp/goxel_*/config/* /etc/goxel/

# Fix permissions
chown -R goxel-daemon:goxel-daemon /var/lib/goxel

# Start daemon
systemctl start goxel-daemon
```

## Troubleshooting

### Common Issues

#### Daemon Won't Start
```bash
# Check logs
journalctl -u goxel-daemon -n 50

# Verify permissions
ls -la /var/lib/goxel /var/run/goxel

# Test manual start
sudo -u goxel-daemon goxel --headless --daemon --debug
```

#### High Memory Usage
```bash
# Check memory stats
goxel --headless --daemon memory-stats

# Clear cache
goxel --headless --daemon clear-cache

# Adjust limits
GOXEL_DAEMON_CACHE_SIZE=500
```

#### Connection Refused
```bash
# Check if daemon is running
systemctl status goxel-daemon
netstat -tlnp | grep 7531

# Check firewall
ufw status
iptables -L -n

# Test connection
telnet localhost 7531
```

### Debug Mode
```bash
# Enable debug logging
GOXEL_DAEMON_LOG_LEVEL=debug goxel --headless --daemon

# Enable request tracing
GOXEL_DAEMON_TRACE_REQUESTS=true goxel --headless --daemon
```

## Best Practices

1. **High Availability**
   - Deploy multiple instances behind load balancer
   - Use health checks for automatic failover
   - Implement graceful shutdown handling

2. **Security**
   - Always use TLS in production
   - Implement authentication for external access
   - Regularly update to latest version
   - Use dedicated user account

3. **Monitoring**
   - Set up alerts for high CPU/memory usage
   - Monitor request latency and error rates
   - Track disk usage for project storage

4. **Performance**
   - Tune thread pool size based on workload
   - Enable compression for large responses
   - Use connection pooling in clients

5. **Maintenance**
   - Regular backups of project data
   - Log rotation to prevent disk fill
   - Periodic cache cleanup

---

*For additional deployment scenarios and enterprise support, contact support@goxel.xyz*