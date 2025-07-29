# Goxel v14.0 Installation Guide

## 🚀 Quick Start (Recommended)

### Homebrew Installation (macOS/Linux)
```bash
# Install Goxel v14.0 Enterprise Daemon Architecture
brew tap jimmy/goxel file:///path/to/goxel/homebrew-goxel
brew install jimmy/goxel/goxel

# Start daemon service
brew services start goxel

# Verify installation
goxel-daemon --version

# Test with example client
python3 /opt/homebrew/share/goxel/examples/homebrew_test_client.py
```

## 📋 Installation Options

### Option 1: Homebrew Package (Recommended)
**Best for**: Production use, easy setup, service management

✅ **Advantages**:
- One-command installation
- Automatic service management
- Built-in logging and monitoring
- Includes example clients
- Regular updates

**Installation Steps**:
1. Tap the Goxel formula repository
2. Install the package
3. Start the service
4. Test the installation

### Option 2: Build from Source
**Best for**: Development, customization, latest features

**Installation Steps**:
```bash
# Install dependencies
# macOS
brew install scons glfw tre

# Ubuntu/Debian
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev

# Build enterprise daemon
scons mode=release daemon=1

# Run daemon
./goxel-daemon --foreground --socket /tmp/goxel.sock
```

## 🏢 Production Deployment

### systemd Service (Linux)
```bash
# Create service file
sudo tee /etc/systemd/system/goxel-daemon.service > /dev/null <<EOF
[Unit]
Description=Goxel v14.0 Enterprise Daemon
After=network.target

[Service]
Type=forking
ExecStart=/usr/local/bin/goxel-daemon --daemonize --socket /var/run/goxel/goxel.sock
PIDFile=/var/run/goxel/goxel.pid
User=goxel
Group=goxel
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
sudo systemctl enable goxel-daemon
sudo systemctl start goxel-daemon
```

### launchd Service (macOS)
```bash
# Create plist file
sudo tee /Library/LaunchDaemons/com.noctua.goxel.daemon.plist > /dev/null <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.noctua.goxel.daemon</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/goxel-daemon</string>
        <string>--foreground</string>
        <string>--socket</string>
        <string>/var/run/goxel/goxel.sock</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
EOF

# Load and start service
sudo launchctl load /Library/LaunchDaemons/com.noctua.goxel.daemon.plist
```

## 🐳 Container Deployment

### Docker
```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source and build
COPY . /src
WORKDIR /src
RUN scons mode=release daemon=1

# Create runtime user
RUN useradd -r -s /bin/false goxel

# Expose socket directory
RUN mkdir -p /var/run/goxel && chown goxel:goxel /var/run/goxel

USER goxel
EXPOSE 8080

CMD ["./goxel-daemon", "--foreground", "--socket", "/var/run/goxel/goxel.sock"]
```

### Kubernetes
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: goxel-daemon
spec:
  replicas: 3
  selector:
    matchLabels:
      app: goxel-daemon
  template:
    metadata:
      labels:
        app: goxel-daemon
    spec:
      containers:
      - name: goxel-daemon
        image: goxel:14.0.0
        ports:
        - containerPort: 8080
        volumeMounts:
        - name: socket-dir
          mountPath: /var/run/goxel
      volumes:
      - name: socket-dir
        emptyDir: {}
```

## 🧪 Testing Installation

### Basic Functionality Test
```bash
# Check daemon version
goxel-daemon --version

# Test daemon startup
goxel-daemon --test-lifecycle

# Test signal handling
goxel-daemon --test-signals
```

### Integration Test
```python
#!/usr/bin/env python3
import json
import socket
import sys

def test_goxel_daemon():
    try:
        # Connect to daemon
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect('/tmp/goxel.sock')
        
        # Test version
        request = {"jsonrpc": "2.0", "method": "goxel.get_version", "id": 1}
        sock.send(json.dumps(request).encode() + b'\n')
        response = json.loads(sock.recv(1024).decode())
        
        if 'result' in response:
            print(f"✅ Goxel daemon v{response['result']['version']} is working")
            return True
        else:
            print(f"❌ Error: {response.get('error', 'Unknown error')}")
            return False
            
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False
    finally:
        sock.close()

if __name__ == "__main__":
    success = test_goxel_daemon()
    sys.exit(0 if success else 1)
```

## 🔧 Configuration

### Environment Variables
```bash
# Socket path
export GOXEL_SOCKET_PATH="/tmp/goxel.sock"

# Log level
export GOXEL_LOG_LEVEL="INFO"

# Worker threads
export GOXEL_WORKER_THREADS="8"

# Max connections
export GOXEL_MAX_CONNECTIONS="256"
```

### Configuration File
```ini
# /etc/goxel/daemon.conf
[daemon]
socket_path = /var/run/goxel/goxel.sock
pid_file = /var/run/goxel/goxel.pid
log_file = /var/log/goxel/daemon.log
log_level = INFO

[performance]
worker_threads = 8
max_connections = 256
queue_size = 1024

[security]
user = goxel
group = goxel
```

## 🔍 Troubleshooting

### Common Issues

#### Socket Permission Denied
```bash
# Fix socket permissions
sudo chmod 666 /tmp/goxel.sock
# or run daemon as current user
goxel-daemon --foreground --socket ~/goxel.sock
```

#### Daemon Won't Start
```bash
# Check for existing processes
ps aux | grep goxel-daemon

# Check socket file exists
ls -la /tmp/goxel.sock

# Run with verbose logging
goxel-daemon --foreground --verbose --socket /tmp/goxel.sock
```

#### Performance Issues
```bash
# Increase worker threads
goxel-daemon --workers 16 --socket /tmp/goxel.sock

# Monitor resource usage
top -p $(pgrep goxel-daemon)
```

### Log Analysis
```bash
# View daemon logs
tail -f /var/log/goxel/daemon.log

# Check service status
systemctl status goxel-daemon  # Linux
brew services list | grep goxel  # Homebrew
```

## 📈 Performance Tuning

### Optimal Settings
- **Worker Threads**: CPU cores × 2
- **Max Connections**: 256 (default) or higher for high-load
- **Queue Size**: 1024 (default) or higher for burst traffic

### Monitoring
```bash
# Monitor socket connections
ss -x | grep goxel

# Monitor process resources
htop -p $(pgrep goxel-daemon)

# Monitor JSON-RPC requests
tail -f /var/log/goxel/daemon.log | grep "JSON-RPC"
```

## 🚀 Next Steps

After successful installation:

1. **Read the API Documentation**: See `docs/API_REFERENCE.md`
2. **Try the Examples**: Explore client examples in `examples/`
3. **Integration**: Connect your applications via JSON-RPC
4. **Monitoring**: Set up logging and health checks
5. **Scaling**: Configure for production load

## 📞 Support

- **Documentation**: Complete guides in `docs/`
- **Examples**: Client implementations in `examples/`
- **Testing**: Use included test clients
- **Issues**: Check logs for error details

---

**Installation Guide Updated**: January 29, 2025  
**Goxel Version**: 14.0.0 (Enterprise Daemon Architecture)  
**Status**: ✅ Production Ready