#!/bin/bash

# Simple script to run individual integration tests with timeout

TEST_NAME=$1
if [ -z "$TEST_NAME" ]; then
    echo "Usage: $0 <test_name>"
    echo "Available tests:"
    echo "  test_daemon_creates_socket"
    echo "  test_client_connects_to_daemon"
    echo "  test_daemon_cleans_up_socket"
    echo "  test_first_request_succeeds"
    echo "  test_second_request_hangs"
    echo "  test_reconnect_allows_new_request"
    echo "  test_multiple_clients_connect"
    echo "  test_concurrent_client_requests"
    echo "  test_malformed_json_handling"
    echo "  test_large_payload_handling"
    exit 1
fi

# Kill any existing daemons
pkill -f goxel-daemon 2>/dev/null

# Remove any existing socket
rm -f /tmp/goxel_integration_test.sock

# Compile test
echo "Compiling test..."
make test_daemon_integration_tdd || exit 1

# Create a simple C program that runs just one test
cat > run_one_test.c << EOF
#include <stdio.h>
#include <string.h>

// External test functions
int test_daemon_creates_socket();
int test_client_connects_to_daemon();
int test_daemon_cleans_up_socket();
int test_first_request_succeeds();
int test_second_request_hangs();
int test_reconnect_allows_new_request();
int test_multiple_clients_connect();
int test_concurrent_client_requests();
int test_malformed_json_handling();
int test_large_payload_handling();

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <test_name>\n", argv[0]);
        return 1;
    }
    
    printf("Running test: %s\n", argv[1]);
    
    int result = 0;
    if (strcmp(argv[1], "test_daemon_creates_socket") == 0) {
        result = test_daemon_creates_socket();
    } else if (strcmp(argv[1], "test_client_connects_to_daemon") == 0) {
        result = test_client_connects_to_daemon();
    } else if (strcmp(argv[1], "test_daemon_cleans_up_socket") == 0) {
        result = test_daemon_cleans_up_socket();
    } else if (strcmp(argv[1], "test_first_request_succeeds") == 0) {
        result = test_first_request_succeeds();
    } else if (strcmp(argv[1], "test_second_request_hangs") == 0) {
        result = test_second_request_hangs();
    } else if (strcmp(argv[1], "test_reconnect_allows_new_request") == 0) {
        result = test_reconnect_allows_new_request();
    } else if (strcmp(argv[1], "test_multiple_clients_connect") == 0) {
        result = test_multiple_clients_connect();
    } else if (strcmp(argv[1], "test_concurrent_client_requests") == 0) {
        result = test_concurrent_client_requests();
    } else if (strcmp(argv[1], "test_malformed_json_handling") == 0) {
        result = test_malformed_json_handling();
    } else if (strcmp(argv[1], "test_large_payload_handling") == 0) {
        result = test_large_payload_handling();
    } else {
        printf("Unknown test: %s\n", argv[1]);
        return 1;
    }
    
    printf("Test %s: %s\n", argv[1], result ? "PASSED" : "FAILED");
    return result ? 0 : 1;
}
EOF

# Extract test functions from the compiled binary
# On macOS, we need to recompile with the test file
gcc -o run_one_test run_one_test.c test_daemon_integration_tdd.c tdd_framework.h -lm

# Use gtimeout on macOS if available, otherwise use no timeout
TIMEOUT_CMD=""
if command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout 10s"
elif command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout 10s"
fi

# Run the test
echo "Running $TEST_NAME..."
if [ -n "$TIMEOUT_CMD" ]; then
    $TIMEOUT_CMD ./run_one_test "$TEST_NAME"
else
    ./run_one_test "$TEST_NAME"
fi
RESULT=$?

# Cleanup
pkill -f goxel-daemon 2>/dev/null
rm -f /tmp/goxel_integration_test.sock
rm -f run_one_test run_one_test.c

if [ $RESULT -eq 124 ]; then
    echo "Test $TEST_NAME: TIMEOUT"
    exit 1
elif [ $RESULT -eq 0 ]; then
    echo "Test $TEST_NAME: SUCCESS"
    exit 0
else
    echo "Test $TEST_NAME: FAILED"
    exit 1
fi