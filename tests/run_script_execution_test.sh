#!/bin/bash

# Script to run daemon script execution integration test

TEST_SOCKET="/tmp/goxel_test_script.sock"
DAEMON_PID_FILE="/tmp/goxel_test_script_daemon.pid"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo "=== Running Daemon Script Execution Test ==="

# Check if daemon is already running
if [ -f "$DAEMON_PID_FILE" ]; then
    PID=$(cat "$DAEMON_PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo -e "${YELLOW}Daemon already running with PID $PID${NC}"
    else
        rm -f "$DAEMON_PID_FILE"
    fi
fi

# Start daemon if not running
if [ ! -f "$DAEMON_PID_FILE" ]; then
    echo "Starting daemon..."
    ../goxel-daemon --foreground --socket "$TEST_SOCKET" &
    DAEMON_PID=$!
    echo $DAEMON_PID > "$DAEMON_PID_FILE"
    
    # Wait for daemon to start
    sleep 2
    
    # Check if daemon started successfully
    if ! ps -p "$DAEMON_PID" > /dev/null 2>&1; then
        echo -e "${RED}Failed to start daemon${NC}"
        rm -f "$DAEMON_PID_FILE"
        exit 1
    fi
    echo -e "${GREEN}Daemon started with PID $DAEMON_PID${NC}"
fi

# Export socket path for test script
export GOXEL_SOCKET_PATH="$TEST_SOCKET"

# Run the test
echo "Running script execution tests..."
python3 test_daemon_script_execution.py
TEST_RESULT=$?

# Stop daemon if we started it
if [ -f "$DAEMON_PID_FILE" ]; then
    PID=$(cat "$DAEMON_PID_FILE")
    echo "Stopping daemon (PID $PID)..."
    kill "$PID" 2>/dev/null
    
    # Wait for daemon to stop
    for i in {1..10}; do
        if ! ps -p "$PID" > /dev/null 2>&1; then
            break
        fi
        sleep 0.5
    done
    
    # Force kill if still running
    if ps -p "$PID" > /dev/null 2>&1; then
        kill -9 "$PID" 2>/dev/null
    fi
    
    rm -f "$DAEMON_PID_FILE"
    rm -f "$TEST_SOCKET"
fi

# Report result
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}All script execution tests passed!${NC}"
else
    echo -e "${RED}Some script execution tests failed!${NC}"
fi

exit $TEST_RESULT