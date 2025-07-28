#!/bin/bash

# Quick Socket Communication Test
# Run this to verify if the socket issue has been resolved

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

SOCKET="/tmp/goxel_quick_test.sock"
PID_FILE="/tmp/goxel_quick_test.pid"
DAEMON="../../goxel-headless"

echo -e "${YELLOW}Quick Socket Communication Test${NC}"
echo "================================"

# Cleanup function
cleanup() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        kill -TERM "$PID" 2>/dev/null || true
    fi
    rm -f "$SOCKET" "$PID_FILE"
}

# Ensure cleanup on exit
trap cleanup EXIT

# Initial cleanup
cleanup

# Check if daemon exists
if [ ! -f "$DAEMON" ]; then
    echo -e "${RED}ERROR: Daemon not found at $DAEMON${NC}"
    echo "Please build the daemon first: scons headless=1 daemon=1"
    exit 1
fi

echo -e "\n1. Starting daemon..."
$DAEMON --daemon --socket "$SOCKET" --pid-file "$PID_FILE" &
DAEMON_PID=$!

# Wait for socket
echo -n "2. Waiting for socket"
for i in {1..50}; do
    if [ -f "$SOCKET" ]; then
        echo -e " ${GREEN}✓${NC}"
        break
    fi
    echo -n "."
    sleep 0.1
done

if [ ! -f "$SOCKET" ]; then
    echo -e " ${RED}✗${NC}"
    echo -e "${RED}ERROR: Socket not created after 5 seconds${NC}"
    exit 1
fi

echo -e "\n3. Testing socket connection..."

# Test with netcat if available
if command -v nc &> /dev/null; then
    echo -e "   Using netcat to test connection..."
    if echo '{"jsonrpc":"2.0","method":"goxel.ping","id":1}' | nc -U "$SOCKET" -w 2; then
        echo -e "   ${GREEN}✓ Connection successful${NC}"
    else
        echo -e "   ${RED}✗ Connection failed${NC}"
    fi
fi

# Test with simple C client
echo -e "\n4. Compiling test client..."
cat > /tmp/quick_client.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        return 1;
    }
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);
    
    printf("Connecting to %s...\n", argv[1]);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    printf("Connected! Sending request...\n");
    const char *request = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_version\",\"id\":1}\n";
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }
    
    printf("Waiting for response...\n");
    char buffer[4096];
    ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Response: %s", buffer);
    } else if (n == 0) {
        printf("Connection closed by server\n");
    } else {
        perror("recv");
    }
    
    close(sock);
    return (n > 0) ? 0 : 1;
}
EOF

gcc -o /tmp/quick_client /tmp/quick_client.c

echo -e "\n5. Testing with C client..."
if /tmp/quick_client "$SOCKET"; then
    echo -e "${GREEN}✓ Socket communication working!${NC}"
    RESULT=0
else
    echo -e "${RED}✗ Socket communication failed${NC}"
    RESULT=1
fi

# Check daemon logs
echo -e "\n6. Daemon process info:"
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null; then
        echo -e "   ${GREEN}✓ Daemon running (PID: $PID)${NC}"
    else
        echo -e "   ${RED}✗ Daemon not running${NC}"
    fi
fi

# Summary
echo -e "\n${YELLOW}Summary:${NC}"
if [ $RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ Socket communication is working!${NC}"
    echo -e "The daemon can receive and respond to JSON-RPC requests."
else
    echo -e "${RED}❌ Socket communication is NOT working${NC}"
    echo -e "The daemon either can't accept connections or can't process requests."
fi

echo -e "\nCleaning up..."
exit $RESULT