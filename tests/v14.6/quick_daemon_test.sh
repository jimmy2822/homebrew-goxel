#!/bin/bash
# Quick Daemon Validation Test
# Author: James O'Brien (Agent-4)

echo "=========================================="
echo "Goxel v14.6 Quick Daemon Validation"
echo "=========================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Paths
DAEMON="../../../goxel --headless --daemon"
SOCKET="/tmp/goxel.sock"
PID_FILE="/tmp/goxel-daemon.pid"

# Cleanup function
cleanup() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        kill $PID 2>/dev/null
        sleep 1
    fi
    rm -f "$SOCKET" "$PID_FILE"
}

# Test 1: Daemon startup
echo -e "\n${YELLOW}Test 1: Daemon Startup${NC}"
cleanup
$DAEMON &
DAEMON_PID=$!

# Wait for socket
for i in {1..50}; do
    if [ -S "$SOCKET" ]; then
        echo -e "${GREEN}✓ Daemon started successfully${NC}"
        break
    fi
    sleep 0.1
done

if [ ! -S "$SOCKET" ]; then
    echo -e "${RED}✗ Daemon failed to create socket${NC}"
    exit 1
fi

# Test 2: PID file
echo -e "\n${YELLOW}Test 2: PID File${NC}"
if [ -f "$PID_FILE" ]; then
    FILE_PID=$(cat "$PID_FILE")
    echo -e "${GREEN}✓ PID file created: $FILE_PID${NC}"
else
    echo -e "${RED}✗ PID file not created${NC}"
fi

# Test 3: Socket connection
echo -e "\n${YELLOW}Test 3: Socket Connection${NC}"
if command -v nc >/dev/null 2>&1; then
    echo '{"jsonrpc":"2.0","id":1,"method":"echo","params":{"msg":"test"}}' | nc -U "$SOCKET" -w 1 > /tmp/response.txt 2>&1
    if [ -s /tmp/response.txt ]; then
        echo -e "${GREEN}✓ Socket connection successful${NC}"
        echo "Response: $(cat /tmp/response.txt)"
    else
        echo -e "${RED}✗ No response from daemon${NC}"
    fi
else
    echo -e "${YELLOW}⚠ nc not available, skipping socket test${NC}"
fi

# Test 4: Graceful shutdown
echo -e "\n${YELLOW}Test 4: Graceful Shutdown${NC}"
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    kill -TERM $PID
    
    # Wait for shutdown
    for i in {1..20}; do
        if ! kill -0 $PID 2>/dev/null; then
            echo -e "${GREEN}✓ Daemon shutdown gracefully${NC}"
            break
        fi
        sleep 0.1
    done
    
    if [ ! -S "$SOCKET" ] && [ ! -f "$PID_FILE" ]; then
        echo -e "${GREEN}✓ Cleanup completed${NC}"
    else
        echo -e "${RED}✗ Cleanup incomplete${NC}"
    fi
fi

# Final cleanup
cleanup

echo -e "\n=========================================="
echo -e "${GREEN}Quick validation complete!${NC}"
echo "=========================================="