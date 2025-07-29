#!/bin/bash
# Daemon Performance Profiling Script
# Author: Michael Rodriguez
# Date: January 29, 2025

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "Goxel Daemon Performance Profiler"
echo "======================================"

# Check if daemon binary exists
DAEMON_BIN="/Users/jimmy/jimmy_side_projects/goxel/goxel-daemon"
if [ ! -f "$DAEMON_BIN" ]; then
    echo -e "${RED}Error: Daemon binary not found at $DAEMON_BIN${NC}"
    echo "Please build with: scons daemon=1"
    exit 1
fi

# Create temp directory for profiling
PROFILE_DIR="/tmp/goxel_profile_$$"
mkdir -p "$PROFILE_DIR"

echo -e "\n${YELLOW}1. Measuring Startup Time${NC}"
echo "----------------------------------------"

# Measure startup time
START_TIME=$(date +%s%N)
$DAEMON_BIN --foreground --socket "$PROFILE_DIR/test.sock" &
DAEMON_PID=$!

# Wait for socket to be created
WAIT_COUNT=0
while [ ! -S "$PROFILE_DIR/test.sock" ] && [ $WAIT_COUNT -lt 50 ]; do
    sleep 0.01
    WAIT_COUNT=$((WAIT_COUNT + 1))
done

END_TIME=$(date +%s%N)
STARTUP_TIME=$(( (END_TIME - START_TIME) / 1000000 ))

if [ -S "$PROFILE_DIR/test.sock" ]; then
    echo -e "${GREEN}✓ Daemon started in ${STARTUP_TIME}ms${NC}"
else
    echo -e "${RED}✗ Daemon failed to start${NC}"
    kill $DAEMON_PID 2>/dev/null
    rm -rf "$PROFILE_DIR"
    exit 1
fi

echo -e "\n${YELLOW}2. Memory Usage Analysis${NC}"
echo "----------------------------------------"

# Get memory stats
if command -v pmap &> /dev/null; then
    MEMORY_KB=$(pmap $DAEMON_PID | tail -n 1 | awk '{print $2}' | sed 's/K//')
    MEMORY_MB=$((MEMORY_KB / 1024))
    echo "Total Memory: ${MEMORY_MB}MB (${MEMORY_KB}KB)"
    
    # Detailed memory breakdown
    echo -e "\nMemory Map Summary:"
    pmap -x $DAEMON_PID | grep -E "(heap|stack|goxel)" | head -10
fi

# Check thread count
THREAD_COUNT=$(ps -M $DAEMON_PID | wc -l)
echo -e "\nThread Count: $((THREAD_COUNT - 1))"

echo -e "\n${YELLOW}3. Socket Performance Test${NC}"
echo "----------------------------------------"

# Create a simple JSON-RPC request
cat > "$PROFILE_DIR/test_request.json" <<EOF
{"jsonrpc":"2.0","method":"get_daemon_info","params":{},"id":1}
EOF

# Test socket response time
if command -v nc &> /dev/null; then
    echo "Testing socket response time..."
    
    # Warm up
    nc -U "$PROFILE_DIR/test.sock" < "$PROFILE_DIR/test_request.json" > /dev/null 2>&1
    
    # Measure
    SOCKET_START=$(date +%s%N)
    for i in {1..10}; do
        nc -U "$PROFILE_DIR/test.sock" < "$PROFILE_DIR/test_request.json" > /dev/null 2>&1
    done
    SOCKET_END=$(date +%s%N)
    
    AVG_RESPONSE=$(( (SOCKET_END - SOCKET_START) / 10000000 ))
    echo -e "${GREEN}✓ Average response time: ${AVG_RESPONSE}ms${NC}"
fi

echo -e "\n${YELLOW}4. CPU Usage Pattern${NC}"
echo "----------------------------------------"

# Monitor CPU for 2 seconds
echo "Monitoring CPU usage (2 seconds)..."
CPU_SAMPLES=""
for i in {1..4}; do
    CPU=$(ps -p $DAEMON_PID -o %cpu | tail -1)
    CPU_SAMPLES="$CPU_SAMPLES $CPU"
    sleep 0.5
done
echo "CPU Usage samples:$CPU_SAMPLES %"

echo -e "\n${YELLOW}5. File Descriptor Analysis${NC}"
echo "----------------------------------------"

# Count file descriptors
FD_COUNT=$(ls /proc/$DAEMON_PID/fd 2>/dev/null | wc -l)
echo "Open file descriptors: $FD_COUNT"

# Show socket info
echo -e "\nSocket information:"
lsof -p $DAEMON_PID 2>/dev/null | grep -E "(UNIX|sock)" | head -5

echo -e "\n${YELLOW}6. Performance Summary${NC}"
echo "----------------------------------------"

# Kill daemon
kill $DAEMON_PID 2>/dev/null
wait $DAEMON_PID 2>/dev/null

# Cleanup
rm -rf "$PROFILE_DIR"

# Summary
echo -e "\n${GREEN}Performance Profile Summary:${NC}"
echo "• Startup Time: ${STARTUP_TIME}ms"
echo "• Memory Usage: ${MEMORY_MB}MB"
echo "• Thread Count: $((THREAD_COUNT - 1))"
echo "• Avg Response: ${AVG_RESPONSE}ms"
echo "• FD Count: $FD_COUNT"

# Recommendations
echo -e "\n${YELLOW}Optimization Recommendations:${NC}"
if [ $STARTUP_TIME -gt 200 ]; then
    echo "• ${RED}Startup time exceeds 200ms target${NC}"
    echo "  - Consider lazy initialization"
    echo "  - Reduce thread count at startup"
fi

if [ $MEMORY_MB -gt 50 ]; then
    echo "• ${RED}Memory usage exceeds 50MB target${NC}"
    echo "  - Reduce thread stack size"
    echo "  - Implement buffer pooling"
fi

if [ $AVG_RESPONSE -gt 5 ]; then
    echo "• ${RED}Response time exceeds 5ms target${NC}"
    echo "  - Optimize JSON parsing"
    echo "  - Add fast-path for common requests"
fi

echo -e "\n${GREEN}Profiling complete!${NC}"