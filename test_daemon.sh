#!/bin/bash
# Quick Daemon Test for goxel-daemon
# Based on v14.6 test script

echo "=========================================="
echo "Goxel v14.0 Daemon Test"
echo "=========================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Paths
DAEMON="./goxel-daemon"
SOCKET="/tmp/goxel-daemon.sock"
PID_FILE="/tmp/goxel-daemon.pid"

# Cleanup function
cleanup() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        kill $PID 2>/dev/null
        sleep 1
    fi
    pkill -f goxel-daemon 2>/dev/null
    rm -f "$SOCKET" "$PID_FILE"
}

# Test 1: Daemon startup
echo -e "\n${YELLOW}Test 1: Daemon Startup${NC}"
cleanup
$DAEMON --foreground --verbose > daemon.log 2>&1 &
DAEMON_PID=$!
echo "Started daemon with PID: $DAEMON_PID"

# Wait for socket
echo "Waiting for socket..."
for i in {1..50}; do
    if [ -S "$SOCKET" ]; then
        echo -e "${GREEN}✓ Daemon started successfully${NC}"
        break
    fi
    sleep 0.1
done

if [ ! -S "$SOCKET" ]; then
    echo -e "${RED}✗ Daemon failed to create socket${NC}"
    echo "Daemon log:"
    cat daemon.log
    kill $DAEMON_PID 2>/dev/null
    exit 1
fi

# Test 2: Socket permissions
echo -e "\n${YELLOW}Test 2: Socket Permissions${NC}"
ls -la "$SOCKET"

# Test 3: Simple JSON-RPC echo test
echo -e "\n${YELLOW}Test 3: JSON-RPC Echo Test${NC}"
if command -v nc >/dev/null 2>&1; then
    echo '{"jsonrpc":"2.0","id":1,"method":"echo","params":{"message":"Hello daemon"}}' | nc -U "$SOCKET" -w 1 > /tmp/response.txt 2>&1
    if [ -s /tmp/response.txt ]; then
        echo -e "${GREEN}✓ Socket connection successful${NC}"
        echo "Response: $(cat /tmp/response.txt)"
    else
        echo -e "${YELLOW}⚠ No response from daemon (echo may not be implemented)${NC}"
    fi
    rm -f /tmp/response.txt
else
    echo -e "${YELLOW}⚠ nc not available, trying Python${NC}"
    python3 -c "
import socket
import json

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    sock.connect('$SOCKET')
    request = {'jsonrpc': '2.0', 'id': 1, 'method': 'echo', 'params': {'message': 'Hello daemon'}}
    sock.send(json.dumps(request).encode() + b'\n')
    response = sock.recv(4096)
    print('Response:', response.decode())
except Exception as e:
    print('Connection error:', e)
finally:
    sock.close()
"
fi

# Test 4: Daemon status check
echo -e "\n${YELLOW}Test 4: Daemon Status Check${NC}"
if kill -0 $DAEMON_PID 2>/dev/null; then
    echo -e "${GREEN}✓ Daemon is running${NC}"
else
    echo -e "${RED}✗ Daemon crashed${NC}"
    echo "Daemon log:"
    tail -20 daemon.log
fi

# Test 5: Graceful shutdown
echo -e "\n${YELLOW}Test 5: Graceful Shutdown${NC}"
kill -TERM $DAEMON_PID

# Wait for shutdown
for i in {1..20}; do
    if ! kill -0 $DAEMON_PID 2>/dev/null; then
        echo -e "${GREEN}✓ Daemon shutdown gracefully${NC}"
        break
    fi
    sleep 0.1
done

if [ ! -S "$SOCKET" ]; then
    echo -e "${GREEN}✓ Socket cleaned up${NC}"
else
    echo -e "${YELLOW}⚠ Socket still exists${NC}"
fi

# Show daemon log tail
echo -e "\n${YELLOW}Daemon log (last 10 lines):${NC}"
tail -10 daemon.log

# Final cleanup
cleanup
rm -f daemon.log

echo -e "\n=========================================="
echo -e "${GREEN}Daemon test complete!${NC}"
echo "=========================================="