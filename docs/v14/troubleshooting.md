# Goxel v14.0 Troubleshooting Guide

## 🎯 Overview

This comprehensive troubleshooting guide helps diagnose and resolve common issues with the Goxel v14.0 Daemon Architecture. Find solutions for connection problems, performance issues, error conditions, and deployment challenges.

**Common Issue Categories:**
- **Connection Issues**: Socket problems, timeouts, authentication
- **Performance Problems**: High latency, low throughput, memory issues
- **API Errors**: Invalid requests, method failures, data corruption
- **Deployment Issues**: Container problems, permission errors, service failures
- **Integration Problems**: Client library issues, MCP integration, platform-specific bugs

## 🔍 Diagnostic Tools

### Health Check Commands
```bash
# Check daemon status
curl -s http://localhost:8080/health | jq

# Verify Unix socket connection
echo '{"jsonrpc":"2.0","method":"goxel.get_daemon_status","id":1}' | nc -U /tmp/goxel-daemon.sock

# Test TCP connection
echo '{"jsonrpc":"2.0","method":"goxel.get_daemon_status","id":1}' | nc localhost 8080

# Check daemon process
ps aux | grep goxel-daemon
netstat -tulpn | grep goxel
```

### Log Analysis Tools
```bash
# View daemon logs with filtering
journalctl -u goxel-daemon --follow --grep ERROR
tail -f /var/log/goxel-daemon.log | grep -E "(ERROR|WARN|FATAL)"

# Performance monitoring
htop -p $(pgrep goxel-daemon)
iostat -x 1
vmstat 1
```

### Debug Mode Configuration
```json
{
  "daemon": {
    "log_level": "debug",
    "enable_request_logging": true,
    "enable_performance_logging": true,
    "slow_request_threshold_ms": 5
  },
  "debug": {
    "enable_memory_tracking": true,
    "enable_connection_tracing": true,
    "dump_requests": true,
    "dump_responses": true
  }
}
```

## 🚨 Common Issues & Solutions

### 1. Connection Issues

#### Problem: "Connection refused" or "Socket not found"
```
Error: connect ECONNREFUSED /tmp/goxel-daemon.sock
Error: ENOENT: no such file or directory, connect '/tmp/goxel-daemon.sock'
```

**Diagnosis:**
```bash
# Check if daemon is running
ps aux | grep goxel-daemon

# Check socket file existence and permissions
ls -la /tmp/goxel-daemon.sock

# Check daemon logs
journalctl -u goxel-daemon --lines=50
```

**Solutions:**
```bash
# 1. Start the daemon
./goxel-daemon --daemon --socket=/tmp/goxel-daemon.sock

# 2. Check socket permissions
sudo chmod 666 /tmp/goxel-daemon.sock

# 3. Use TCP instead of Unix socket
export GOXEL_DAEMON_TCP=true
export GOXEL_DAEMON_HOST=localhost
export GOXEL_DAEMON_PORT=8080

# 4. Restart daemon with proper permissions
sudo systemctl restart goxel-daemon
```

**Prevention:**
```typescript
// Robust connection with fallback
class RobustGoxelClient {
  private async connectWithFallback() {
    const connectionConfigs = [
      { socketPath: '/tmp/goxel-daemon.sock' },
      { tcp: { host: 'localhost', port: 8080 } },
      { socketPath: '/var/run/goxel/daemon.sock' }
    ];
    
    for (const config of connectionConfigs) {
      try {
        const client = new GoxelDaemonClient(config);
        await client.connect();
        return client;
      } catch (error) {
        console.warn(`Connection failed with config ${JSON.stringify(config)}: ${error.message}`);
      }
    }
    
    throw new Error('All connection methods failed');
  }
}
```

#### Problem: "Connection timeout" or "Request timeout"
```
Error: Request timeout after 30000ms
Error: Connection timeout
```

**Diagnosis:**
```bash
# Check network connectivity
ping localhost
telnet localhost 8080

# Monitor daemon responsiveness
time echo '{"jsonrpc":"2.0","method":"goxel.ping","id":1}' | nc localhost 8080

# Check daemon load
top -p $(pgrep goxel-daemon)
```

**Solutions:**
```typescript
// Increase timeout settings
const client = new GoxelDaemonClient({
  timeout: 60000,           // 60 second timeout
  connectionTimeout: 10000, // 10 second connection timeout
  retryAttempts: 3,        // Retry failed requests
  retryDelay: 1000         // 1 second between retries
});

// Implement request queuing
class QueuedGoxelClient {
  private requestQueue: Array<() => Promise<any>> = [];
  private processing = false;
  
  async queueRequest<T>(operation: () => Promise<T>): Promise<T> {
    return new Promise((resolve, reject) => {
      this.requestQueue.push(async () => {
        try {
          const result = await operation();
          resolve(result);
        } catch (error) {
          reject(error);
        }
      });
      
      this.processQueue();
    });
  }
  
  private async processQueue() {
    if (this.processing || this.requestQueue.length === 0) return;
    
    this.processing = true;
    while (this.requestQueue.length > 0) {
      const request = this.requestQueue.shift()!;
      await request();
    }
    this.processing = false;
  }
}
```

#### Problem: "Too many connections" or "Connection pool exhausted"
```
Error: Maximum connections reached (50/50)
Error: Connection pool exhausted
```

**Solutions:**
```bash
# Increase daemon connection limit
echo '{
  "daemon": {
    "max_connections": 100,
    "connection_pool_size": 20
  }
}' > /etc/goxel/daemon.conf

# Restart daemon
sudo systemctl restart goxel-daemon
```

```typescript
// Implement connection pooling
class ConnectionPool {
  private available: GoxelDaemonClient[] = [];
  private inUse: Set<GoxelDaemonClient> = new Set();
  private readonly maxSize: number;
  
  constructor(maxSize = 10) {
    this.maxSize = maxSize;
  }
  
  async acquire(): Promise<GoxelDaemonClient> {
    if (this.available.length > 0) {
      const client = this.available.pop()!;
      this.inUse.add(client);
      return client;
    }
    
    if (this.inUse.size < this.maxSize) {
      const client = new GoxelDaemonClient({ autoReconnect: true });
      await client.connect();
      this.inUse.add(client);
      return client;
    }
    
    throw new Error('Connection pool exhausted');
  }
  
  release(client: GoxelDaemonClient) {
    this.inUse.delete(client);
    this.available.push(client);
  }
}
```

### 2. Performance Issues

#### Problem: High latency (>10ms average response time)
**Diagnosis:**
```typescript
// Measure request latency
class LatencyDiagnostic {
  async measureOperationLatency() {
    const operations = [
      { name: 'ping', fn: () => client.ping() },
      { name: 'get_status', fn: () => client.getStatus() },
      { name: 'add_voxel', fn: () => client.addVoxel({
        position: [0, -16, 0],
        color: [255, 0, 0, 255]
      })}
    ];
    
    for (const op of operations) {
      const times: number[] = [];
      
      for (let i = 0; i < 10; i++) {
        const start = process.hrtime.bigint();
        await op.fn();
        const end = process.hrtime.bigint();
        times.push(Number(end - start) / 1_000_000);
      }
      
      const avg = times.reduce((a, b) => a + b, 0) / times.length;
      console.log(`${op.name}: ${avg.toFixed(2)}ms average`);
    }
  }
}
```

**Solutions:**
```typescript
// Use batch operations
const optimizedClient = {
  async addManyVoxels(voxels: VoxelData[]) {
    // ❌ Slow: Individual requests
    // for (const voxel of voxels) {
    //   await client.addVoxel(voxel);
    // }
    
    // ✅ Fast: Batch request
    const batchSize = 1000;
    for (let i = 0; i < voxels.length; i += batchSize) {
      const batch = voxels.slice(i, i + batchSize);
      await client.addVoxelBatch({ voxels: batch });
    }
  }
};

// Enable compression for large payloads
const client = new GoxelDaemonClient({
  enableCompression: true,
  compressionThreshold: 1024 // Compress payloads > 1KB
});

// Use connection pooling
const pooledClient = new PooledGoxelClient({
  poolSize: 5,
  socketPath: '/tmp/goxel-daemon.sock'
});
```

#### Problem: High memory usage (>200MB)
**Diagnosis:**
```bash
# Monitor daemon memory usage
watch -n 1 'ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep goxel-daemon)'

# Check for memory leaks
valgrind --tool=memcheck --leak-check=full ./goxel-daemon

# Monitor memory growth over time
while true; do
  ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep goxel-daemon)
  sleep 10
done | tee memory-usage.log
```

**Solutions:**
```bash
# Configure memory limits
echo '{
  "daemon": {
    "max_memory_mb": 128,
    "gc_threshold_mb": 100,
    "cache_size_mb": 64
  }
}' > /etc/goxel/daemon.conf

# Enable automatic garbage collection
echo '{
  "performance": {
    "auto_gc": true,
    "gc_interval_ms": 30000
  }
}' >> /etc/goxel/daemon.conf
```

```typescript
// Implement memory monitoring
class MemoryMonitor {
  private threshold = 200 * 1024 * 1024; // 200MB
  
  async monitorMemoryUsage() {
    setInterval(async () => {
      const usage = process.memoryUsage();
      
      if (usage.heapUsed > this.threshold) {
        console.warn(`High memory usage: ${usage.heapUsed / 1024 / 1024}MB`);
        
        // Trigger garbage collection
        if (global.gc) {
          global.gc();
        }
        
        // Clear caches
        await this.clearCaches();
      }
    }, 10000);
  }
  
  private async clearCaches() {
    // Clear any client-side caches
    responseCache.clear();
    objectPool.clear();
  }
}
```

#### Problem: Low throughput (<100 operations/second)
**Solutions:**
```typescript
// Implement parallel processing
class HighThroughputProcessor {
  async processVoxelsParallel(voxels: VoxelData[]) {
    const batchSize = 500;
    const maxConcurrency = 5;
    const batches = this.chunkArray(voxels, batchSize);
    
    // Process batches in parallel
    for (let i = 0; i < batches.length; i += maxConcurrency) {
      const concurrentBatches = batches.slice(i, i + maxConcurrency);
      
      await Promise.all(concurrentBatches.map(batch =>
        client.addVoxelBatch({ voxels: batch })
      ));
    }
  }
  
  // Use streaming for continuous operations
  async streamVoxelUpdates(voxelStream: AsyncIterable<VoxelData>) {
    const buffer: VoxelData[] = [];
    const flushSize = 100;
    
    for await (const voxel of voxelStream) {
      buffer.push(voxel);
      
      if (buffer.length >= flushSize) {
        await client.addVoxelBatch({ voxels: buffer });
        buffer.length = 0; // Clear buffer
      }
    }
    
    // Flush remaining voxels
    if (buffer.length > 0) {
      await client.addVoxelBatch({ voxels: buffer });
    }
  }
}
```

### 3. API Errors

#### Problem: "Invalid JSON-RPC request" (-32600)
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32600,
    "message": "Invalid Request"
  },
  "id": null
}
```

**Common Causes & Solutions:**
```typescript
// ❌ Invalid: Missing required fields
const badRequest = {
  method: "goxel.add_voxel",
  params: { position: [0, -16, 0] }
  // Missing: jsonrpc, id
};

// ✅ Valid: Complete request
const goodRequest = {
  jsonrpc: "2.0",
  method: "goxel.add_voxel",
  params: {
    position: [0, -16, 0],
    color: [255, 0, 0, 255]
  },
  id: 1
};

// Validate requests before sending
class RequestValidator {
  validateRequest(request: any): boolean {
    return (
      request.jsonrpc === "2.0" &&
      typeof request.method === "string" &&
      request.id !== undefined &&
      (request.params === undefined || typeof request.params === "object")
    );
  }
}
```

#### Problem: "Method not found" (-32601)
```json
{
  "jsonrpc": "2.0", 
  "error": {
    "code": -32601,
    "message": "Method not found"
  },
  "id": 1
}
```

**Solutions:**
```typescript
// Check available methods
const client = new GoxelDaemonClient();
const status = await client.getStatus();
console.log('Available methods:', status.supported_methods);

// Use method existence checking
class SafeMethodCaller {
  private supportedMethods: Set<string> = new Set();
  
  async initialize() {
    const status = await client.getStatus();
    this.supportedMethods = new Set(status.supported_methods);
  }
  
  async callMethod(method: string, params: any) {
    if (!this.supportedMethods.has(method)) {
      throw new Error(`Method ${method} not supported by daemon`);
    }
    
    return client.callRawMethod(method, params);
  }
}
```

#### Problem: "Invalid params" (-32602)
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": {
      "expected": "position: [number, number, number]",
      "received": "position: [0, -16]"
    }
  },
  "id": 1
}
```

**Solutions:**
```typescript
// Parameter validation
class ParameterValidator {
  validateAddVoxelParams(params: any): boolean {
    return (
      Array.isArray(params.position) &&
      params.position.length === 3 &&
      params.position.every(x => typeof x === 'number') &&
      Array.isArray(params.color) &&
      params.color.length >= 3 &&
      params.color.length <= 4 &&
      params.color.every(c => typeof c === 'number' && c >= 0 && c <= 255)
    );
  }
  
  async addVoxelSafe(params: AddVoxelParams) {
    if (!this.validateAddVoxelParams(params)) {
      throw new Error('Invalid voxel parameters');
    }
    
    return client.addVoxel(params);
  }
}

// Use TypeScript for compile-time validation
interface StrictVoxelParams {
  position: [number, number, number];
  color: [number, number, number] | [number, number, number, number];
}

function addVoxelTypeSafe(params: StrictVoxelParams) {
  return client.addVoxel(params);
}
```

#### Problem: "Position out of bounds" (-32003)
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32003,
    "message": "Position out of bounds",
    "data": {
      "position": [1000, -16, 0],
      "canvas_bounds": {
        "min": [-64, -32, -64],
        "max": [63, 31, 63]
      }
    }
  },
  "id": 1
}
```

**Solutions:**
```typescript
// Bounds checking
class BoundsChecker {
  private canvasBounds = {
    min: [-64, -32, -64],
    max: [63, 31, 63]
  };
  
  isValidPosition(position: [number, number, number]): boolean {
    return position.every((coord, index) => 
      coord >= this.canvasBounds.min[index] && 
      coord <= this.canvasBounds.max[index]
    );
  }
  
  clampPosition(position: [number, number, number]): [number, number, number] {
    return position.map((coord, index) => 
      Math.max(
        this.canvasBounds.min[index],
        Math.min(this.canvasBounds.max[index], coord)
      )
    ) as [number, number, number];
  }
  
  async addVoxelWithBoundsCheck(params: AddVoxelParams) {
    if (!this.isValidPosition(params.position)) {
      // Option 1: Throw error
      throw new Error(`Position ${params.position} is out of bounds`);
      
      // Option 2: Clamp to valid range
      // params.position = this.clampPosition(params.position);
    }
    
    return client.addVoxel(params);
  }
}
```

### 4. Deployment Issues

#### Problem: Docker container fails to start
```
Error: Cannot connect to daemon socket
Error: Permission denied accessing socket
```

**Diagnosis:**
```bash
# Check container logs
docker logs goxel-daemon-container

# Check socket permissions in container
docker exec goxel-daemon-container ls -la /tmp/

# Verify port binding
docker port goxel-daemon-container
```

**Solutions:**
```dockerfile
# Dockerfile fixes
FROM ubuntu:22.04

# Create proper user and directories
RUN useradd -m -s /bin/bash goxel
RUN mkdir -p /tmp/goxel && chown goxel:goxel /tmp/goxel

# Set proper permissions
COPY --chown=goxel:goxel . /app
USER goxel

# Use TCP instead of Unix socket in containers
EXPOSE 8080
CMD ["./goxel-daemon", "--tcp", "--port=8080", "--bind=0.0.0.0"]
```

```yaml
# docker-compose.yml fixes
version: '3.8'
services:
  goxel-daemon:
    build: .
    ports:
      - "8080:8080"
    volumes:
      - /tmp/goxel-socket:/tmp/goxel-socket
    environment:
      - GOXEL_SOCKET_PATH=/tmp/goxel-socket/daemon.sock
    user: "1000:1000"  # Use consistent UID/GID
    restart: unless-stopped
```

#### Problem: Kubernetes deployment issues
```yaml
# k8s-troubleshooting.yaml
apiVersion: v1
kind: Pod
metadata:
  name: goxel-debug
spec:
  containers:
  - name: debug
    image: goxel-daemon:latest
    command: ["/bin/bash", "-c", "sleep 3600"]
    volumeMounts:
    - name: socket-volume
      mountPath: /tmp/goxel
  volumes:
  - name: socket-volume
    emptyDir: {}
```

```bash
# Debug commands
kubectl logs deployment/goxel-daemon
kubectl exec -it goxel-debug -- /bin/bash
kubectl describe pod goxel-daemon-xyz
kubectl get events --sort-by=.metadata.creationTimestamp
```

#### Problem: systemd service fails
```ini
# /etc/systemd/system/goxel-daemon.service
[Unit]
Description=Goxel v14.0 Daemon
After=network.target

[Service]
Type=simple
User=goxel
Group=goxel
ExecStart=/usr/local/bin/goxel-daemon --daemon --socket=/tmp/goxel-daemon.sock
Restart=always
RestartSec=10
Environment=GOXEL_LOG_LEVEL=info

# Security settings
NoNewPrivileges=true
PrivateTmp=false  # Allow socket access
ProtectHome=true
ProtectSystem=strict
ReadWritePaths=/tmp

[Install]
WantedBy=multi-user.target
```

```bash
# Troubleshooting commands
sudo systemctl status goxel-daemon
sudo journalctl -u goxel-daemon --follow
sudo systemctl daemon-reload
sudo systemctl enable goxel-daemon
sudo systemctl restart goxel-daemon
```

### 5. Platform-Specific Issues

#### macOS Issues
```bash
# Socket permission issues
sudo chmod 666 /tmp/goxel-daemon.sock

# Firewall blocking TCP connections
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/goxel-daemon
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblock /path/to/goxel-daemon

# macOS security restrictions
xattr -d com.apple.quarantine /path/to/goxel-daemon
```

#### Windows Issues
```powershell
# Named pipe permissions
Get-Acl \\.\pipe\goxel-daemon

# Windows Defender exclusions
Add-MpPreference -ExclusionPath "C:\Program Files\Goxel"
Add-MpPreference -ExclusionProcess "goxel-daemon.exe"

# TCP port conflicts
netstat -an | findstr 8080
netsh interface portproxy show all
```

#### Linux Issues
```bash
# SELinux context issues
sudo setsebool -P httpd_can_network_connect 1
sudo semanage port -a -t http_port_t -p tcp 8080

# AppArmor restrictions
sudo aa-complain /usr/local/bin/goxel-daemon

# Socket file cleanup
sudo rm -f /tmp/goxel-daemon.sock
sudo systemctl restart goxel-daemon
```

## 🛠️ Debug Tools & Scripts

### Comprehensive Health Check Script
```bash
#!/bin/bash
# goxel-health-check.sh

set -e

echo "🔍 Goxel v14.0 Daemon Health Check"
echo "=================================="

# Check if daemon is running
if pgrep -f goxel-daemon > /dev/null; then
    echo "✅ Daemon process is running"
    ps aux | grep goxel-daemon | grep -v grep
else
    echo "❌ Daemon process not found"
    exit 1
fi

# Check socket file
if [[ -S /tmp/goxel-daemon.sock ]]; then
    echo "✅ Unix socket exists"
    ls -la /tmp/goxel-daemon.sock
else
    echo "⚠️  Unix socket not found"
fi

# Check TCP port
if nc -z localhost 8080 2>/dev/null; then
    echo "✅ TCP port 8080 is open"
else
    echo "⚠️  TCP port 8080 not accessible"
fi

# Test API connectivity
echo "🔧 Testing API connectivity..."

# Unix socket test
if echo '{"jsonrpc":"2.0","method":"goxel.get_daemon_status","id":1}' | nc -U /tmp/goxel-daemon.sock 2>/dev/null | grep -q '"result"'; then
    echo "✅ Unix socket API responds"
else
    echo "❌ Unix socket API failed"
fi

# TCP test
if echo '{"jsonrpc":"2.0","method":"goxel.get_daemon_status","id":1}' | nc localhost 8080 2>/dev/null | grep -q '"result"'; then
    echo "✅ TCP API responds"
else
    echo "❌ TCP API failed"
fi

# Performance test
echo "⚡ Running performance test..."
start_time=$(date +%s%3N)
echo '{"jsonrpc":"2.0","method":"goxel.ping","id":1}' | nc localhost 8080 > /dev/null 2>&1
end_time=$(date +%s%3N)
latency=$((end_time - start_time))

if [[ $latency -lt 10 ]]; then
    echo "✅ Low latency: ${latency}ms"
elif [[ $latency -lt 50 ]]; then
    echo "⚠️  Moderate latency: ${latency}ms"
else
    echo "❌ High latency: ${latency}ms"
fi

echo "✅ Health check completed"
```

### Connection Test Script
```typescript
#!/usr/bin/env node
// connection-test.js

const { GoxelDaemonClient } = require('@goxel/daemon-client');

async function runConnectionTests() {
  console.log('🔧 Goxel Connection Test Suite');
  console.log('=============================');
  
  const testConfigs = [
    {
      name: 'Unix Socket',
      config: { socketPath: '/tmp/goxel-daemon.sock' }
    },
    {
      name: 'TCP localhost',
      config: { tcp: { host: 'localhost', port: 8080 } }
    },
    {
      name: 'TCP 127.0.0.1',
      config: { tcp: { host: '127.0.0.1', port: 8080 } }
    }
  ];
  
  for (const test of testConfigs) {
    console.log(`\n🧪 Testing ${test.name}...`);
    
    try {
      const client = new GoxelDaemonClient(test.config);
      
      // Connection test
      const start = Date.now();
      await client.connect();
      const connectTime = Date.now() - start;
      console.log(`✅ Connected in ${connectTime}ms`);
      
      // API test
      const pingStart = Date.now();
      const latency = await client.ping();
      const pingTime = Date.now() - pingStart;
      console.log(`✅ Ping: ${latency}ms (total: ${pingTime}ms)`);
      
      // Status test
      const status = await client.getStatus();
      console.log(`✅ Status: ${status.daemon_version}, uptime: ${status.uptime_seconds}s`);
      
      await client.disconnect();
      console.log(`✅ ${test.name} test passed`);
      
    } catch (error) {
      console.log(`❌ ${test.name} test failed: ${error.message}`);
    }
  }
}

runConnectionTests().catch(console.error);
```

### Performance Benchmark Script
```typescript
#!/usr/bin/env node
// performance-benchmark.js

const { GoxelDaemonClient } = require('@goxel/daemon-client');

async function runPerformanceBenchmark() {
  console.log('⚡ Goxel Performance Benchmark');
  console.log('=============================');
  
  const client = new GoxelDaemonClient({
    socketPath: '/tmp/goxel-daemon.sock',
    timeout: 60000
  });
  
  await client.connect();
  
  try {
    // Create test project
    console.log('\n📦 Creating test project...');
    const project = await client.createProject({
      name: 'Benchmark Project',
      template: 'empty'
    });
    console.log(`✅ Project created: ${project.project_id}`);
    
    // Single voxel performance
    console.log('\n🎯 Single voxel operations...');
    const singleVoxelTimes = [];
    
    for (let i = 0; i < 100; i++) {
      const start = process.hrtime.bigint();
      await client.addVoxel({
        position: [i % 10, -16, Math.floor(i / 10)],
        color: [255, 0, 0, 255]
      });
      const end = process.hrtime.bigint();
      singleVoxelTimes.push(Number(end - start) / 1_000_000);
    }
    
    const avgSingle = singleVoxelTimes.reduce((a, b) => a + b, 0) / singleVoxelTimes.length;
    console.log(`Average single voxel time: ${avgSingle.toFixed(2)}ms`);
    console.log(`Single voxel throughput: ${(1000 / avgSingle).toFixed(0)} voxels/second`);
    
    // Batch operation performance
    console.log('\n📦 Batch operations...');
    const batchSizes = [10, 100, 500, 1000];
    
    for (const batchSize of batchSizes) {
      const voxels = Array.from({ length: batchSize }, (_, i) => ({
        position: [
          (i % 32) - 16,
          -15,
          Math.floor(i / 32) - 16
        ],
        color: [
          Math.floor(Math.random() * 256),
          Math.floor(Math.random() * 256),
          Math.floor(Math.random() * 256),
          255
        ]
      }));
      
      const start = process.hrtime.bigint();
      await client.addVoxelBatch({ voxels });
      const end = process.hrtime.bigint();
      
      const totalTime = Number(end - start) / 1_000_000;
      const timePerVoxel = totalTime / batchSize;
      const throughput = batchSize / (totalTime / 1000);
      
      console.log(`Batch ${batchSize}: ${totalTime.toFixed(2)}ms total, ${timePerVoxel.toFixed(2)}ms/voxel, ${throughput.toFixed(0)} voxels/second`);
    }
    
    // Rendering performance
    console.log('\n🖼️  Rendering performance...');
    const renderSizes = [256, 512, 1024];
    
    for (const size of renderSizes) {
      const start = process.hrtime.bigint();
      const result = await client.renderImage({
        width: size,
        height: size,
        camera: {
          position: [20, 20, 20],
          target: [0, -16, 0],
          up: [0, 1, 0],
          fov: 45
        },
        format: 'png'
      });
      const end = process.hrtime.bigint();
      
      const totalTime = Number(end - start) / 1_000_000;
      const pixelsPerSecond = (size * size) / (totalTime / 1000);
      
      console.log(`Render ${size}x${size}: ${totalTime.toFixed(2)}ms total, ${result.render_time_ms.toFixed(2)}ms daemon, ${(pixelsPerSecond / 1000).toFixed(0)}K pixels/second`);
    }
    
  } finally {
    await client.disconnect();
  }
  
  console.log('\n✅ Benchmark completed');
}

runPerformanceBenchmark().catch(console.error);
```

---

## 📋 Troubleshooting Checklist

### ✅ Before Opening Issues
- [ ] Check daemon process is running
- [ ] Verify socket/port accessibility
- [ ] Test with different connection methods
- [ ] Review daemon logs for errors
- [ ] Run health check script
- [ ] Test with minimal example code

### ✅ When Reporting Issues
- [ ] Include daemon version and platform
- [ ] Provide complete error messages
- [ ] Share relevant log excerpts
- [ ] Include configuration files
- [ ] Describe steps to reproduce
- [ ] Mention any recent changes

### ✅ Performance Issues
- [ ] Run performance benchmark
- [ ] Check system resource usage
- [ ] Monitor network latency
- [ ] Test with different batch sizes
- [ ] Verify optimal configuration
- [ ] Profile client-side code

### ✅ Deployment Issues
- [ ] Verify container/service configuration
- [ ] Check file permissions and ownership
- [ ] Test network connectivity
- [ ] Review security policies
- [ ] Validate environment variables
- [ ] Test with simplified deployment

---

*This troubleshooting guide provides comprehensive solutions for common issues in the Goxel v14.0 Daemon Architecture. Use the diagnostic tools and step-by-step solutions to quickly resolve problems and maintain optimal performance.*

**Last Updated**: January 26, 2025  
**Version**: 14.0.0-dev  
**Status**: 📋 Template Ready for Implementation