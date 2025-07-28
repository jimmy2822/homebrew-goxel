/*
 * Goxel v14.6 JSON-RPC Echo Integration Test
 * 
 * Tests JSON-RPC protocol implementation including echo method,
 * error handling, and batch requests against the real daemon.
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "../framework/test_framework.h"
#include <sys/un.h>

#define DAEMON_BINARY "../../../goxel"
#define DAEMON_SOCKET "/tmp/goxel.sock"
#define DAEMON_PID_FILE "/tmp/goxel-daemon.pid"

static pid_t daemon_pid = -1;
static int client_fd = -1;

// Helper to check if socket exists
static bool daemon_socket_exists(void) {
    return access(DAEMON_SOCKET, F_OK) == 0;
}

// Setup - start daemon and connect
static void setup_json_rpc_test(void) {
    // Clean up
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    // Start daemon
    daemon_pid = fork();
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for daemon
    test_wait_for_condition(daemon_socket_exists, 2000);
    
    // Connect client
    client_fd = test_connect_unix_socket(DAEMON_SOCKET);
}

// Teardown - disconnect and stop daemon
static void teardown_json_rpc_test(void) {
    if (client_fd >= 0) {
        close(client_fd);
        client_fd = -1;
    }
    
    if (daemon_pid > 0) {
        kill(daemon_pid, SIGTERM);
        waitpid(daemon_pid, NULL, 0);
        daemon_pid = -1;
    }
}

// Helper to send and receive JSON-RPC
static char* send_json_rpc(const char *request) {
    // Send request
    size_t len = strlen(request);
    ssize_t sent = send(client_fd, request, len, 0);
    if (sent != len) return NULL;
    
    // Receive response
    static char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    ssize_t received = 0;
    int attempts = 0;
    
    while (attempts < 100) { // 1 second timeout
        ssize_t chunk = recv(client_fd, buffer + received, 
                           sizeof(buffer) - received - 1, MSG_DONTWAIT);
        if (chunk > 0) {
            received += chunk;
            if (strchr(buffer, '\n') != NULL) {
                break; // Complete message
            }
        } else if (chunk < 0 && errno != EAGAIN) {
            return NULL;
        }
        usleep(10000); // 10ms
        attempts++;
    }
    
    return received > 0 ? buffer : NULL;
}

// Test: Basic echo method
TEST_CASE(json_rpc_echo) {
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\","
                         "\"params\":{\"message\":\"Hello, Goxel!\"}}\n";
    
    char *response = send_json_rpc(request);
    TEST_ASSERT_NOT_NULL(response);
    test_log_info("Response: %s", response);
    
    // Verify response structure
    TEST_ASSERT(strstr(response, "\"jsonrpc\":\"2.0\"") != NULL);
    TEST_ASSERT(strstr(response, "\"id\":1") != NULL);
    TEST_ASSERT(strstr(response, "\"result\"") != NULL);
    TEST_ASSERT(strstr(response, "Hello, Goxel!") != NULL);
    
    return TEST_PASS;
}

// Test: JSON-RPC error handling - invalid JSON
TEST_CASE(json_rpc_parse_error) {
    const char *request = "{this is not valid json}\n";
    
    char *response = send_json_rpc(request);
    TEST_ASSERT_NOT_NULL(response);
    test_log_info("Error response: %s", response);
    
    // Should return parse error (-32700)
    TEST_ASSERT(strstr(response, "\"error\"") != NULL);
    TEST_ASSERT(strstr(response, "\"-32700\"") != NULL || 
                strstr(response, "-32700") != NULL);
    TEST_ASSERT(strstr(response, "Parse error") != NULL);
    
    return TEST_PASS;
}

// Test: JSON-RPC error handling - invalid request
TEST_CASE(json_rpc_invalid_request) {
    // Missing jsonrpc version
    const char *request = "{\"id\":2,\"method\":\"echo\",\"params\":{}}\n";
    
    char *response = send_json_rpc(request);
    TEST_ASSERT_NOT_NULL(response);
    
    // Should return invalid request error (-32600)
    TEST_ASSERT(strstr(response, "\"error\"") != NULL);
    TEST_ASSERT(strstr(response, "\"-32600\"") != NULL || 
                strstr(response, "-32600") != NULL);
    
    return TEST_PASS;
}

// Test: JSON-RPC error handling - method not found
TEST_CASE(json_rpc_method_not_found) {
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":3,"
                         "\"method\":\"nonexistent_method\",\"params\":{}}\n";
    
    char *response = send_json_rpc(request);
    TEST_ASSERT_NOT_NULL(response);
    
    // Should return method not found error (-32601)
    TEST_ASSERT(strstr(response, "\"error\"") != NULL);
    TEST_ASSERT(strstr(response, "\"-32601\"") != NULL || 
                strstr(response, "-32601") != NULL);
    
    return TEST_PASS;
}

// Test: JSON-RPC notification (no id)
TEST_CASE(json_rpc_notification) {
    const char *request = "{\"jsonrpc\":\"2.0\",\"method\":\"echo\","
                         "\"params\":{\"message\":\"notification\"}}\n";
    
    // Send notification
    ssize_t sent = send(client_fd, request, strlen(request), 0);
    TEST_ASSERT(sent == strlen(request));
    
    // Should not receive a response for notifications
    char buffer[256];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    
    // Give it a moment
    if (received < 0 && errno == EAGAIN) {
        usleep(100000); // 100ms
        received = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    }
    
    // Should still be no response
    TEST_ASSERT(received < 0);
    TEST_ASSERT(errno == EAGAIN || errno == EWOULDBLOCK);
    
    test_log_info("Notification correctly produced no response");
    
    return TEST_PASS;
}

// Test: JSON-RPC batch request
TEST_CASE(json_rpc_batch) {
    const char *request = "["
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"echo\",\"params\":{\"n\":1}},"
        "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"echo\",\"params\":{\"n\":2}},"
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"echo\",\"params\":{\"n\":3}}"
        "]\n";
    
    char *response = send_json_rpc(request);
    TEST_ASSERT_NOT_NULL(response);
    test_log_info("Batch response: %s", response);
    
    // Should return array of responses
    TEST_ASSERT(response[0] == '[');
    TEST_ASSERT(strstr(response, "\"id\":10") != NULL);
    TEST_ASSERT(strstr(response, "\"id\":11") != NULL);
    TEST_ASSERT(strstr(response, "\"id\":12") != NULL);
    
    // Count responses (should be 3)
    int response_count = 0;
    char *ptr = response;
    while ((ptr = strstr(ptr, "\"result\"")) != NULL) {
        response_count++;
        ptr++;
    }
    TEST_ASSERT_EQ(3, response_count);
    
    return TEST_PASS;
}

// Test: Different parameter types
TEST_CASE(json_rpc_param_types) {
    // Test with string parameter
    const char *string_req = "{\"jsonrpc\":\"2.0\",\"id\":20,\"method\":\"echo\","
                            "\"params\":{\"type\":\"string\",\"value\":\"test\"}}\n";
    char *response = send_json_rpc(string_req);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT(strstr(response, "\"type\":\"string\"") != NULL);
    
    // Test with number parameter
    const char *number_req = "{\"jsonrpc\":\"2.0\",\"id\":21,\"method\":\"echo\","
                            "\"params\":{\"type\":\"number\",\"value\":42}}\n";
    response = send_json_rpc(number_req);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT(strstr(response, "\"type\":\"number\"") != NULL);
    TEST_ASSERT(strstr(response, "42") != NULL);
    
    // Test with boolean parameter
    const char *bool_req = "{\"jsonrpc\":\"2.0\",\"id\":22,\"method\":\"echo\","
                          "\"params\":{\"type\":\"boolean\",\"value\":true}}\n";
    response = send_json_rpc(bool_req);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT(strstr(response, "\"type\":\"boolean\"") != NULL);
    TEST_ASSERT(strstr(response, "true") != NULL);
    
    // Test with array parameter
    const char *array_req = "{\"jsonrpc\":\"2.0\",\"id\":23,\"method\":\"echo\","
                           "\"params\":{\"type\":\"array\",\"value\":[1,2,3]}}\n";
    response = send_json_rpc(array_req);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT(strstr(response, "\"type\":\"array\"") != NULL);
    TEST_ASSERT(strstr(response, "[1,2,3]") != NULL);
    
    return TEST_PASS;
}

// Test: Concurrent JSON-RPC requests
TEST_CASE(json_rpc_concurrent) {
    // Send multiple requests without waiting for responses
    for (int i = 0; i < 10; i++) {
        char request[256];
        snprintf(request, sizeof(request),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"echo\","
                "\"params\":{\"index\":%d}}\n", 100 + i, i);
        
        ssize_t sent = send(client_fd, request, strlen(request), 0);
        TEST_ASSERT(sent == strlen(request));
    }
    
    test_log_info("Sent 10 concurrent requests");
    
    // Receive all responses
    int received_count = 0;
    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    size_t total_received = 0;
    
    // Collect responses
    int attempts = 0;
    while (attempts < 200 && received_count < 10) { // 2 second timeout
        ssize_t chunk = recv(client_fd, buffer + total_received,
                           sizeof(buffer) - total_received - 1, MSG_DONTWAIT);
        if (chunk > 0) {
            total_received += chunk;
            
            // Count complete responses (by counting closing braces at root level)
            char *ptr = buffer;
            int brace_depth = 0;
            while (*ptr) {
                if (*ptr == '{') brace_depth++;
                else if (*ptr == '}') {
                    brace_depth--;
                    if (brace_depth == 0) received_count++;
                }
                ptr++;
            }
        }
        usleep(10000); // 10ms
        attempts++;
    }
    
    TEST_ASSERT_EQ(10, received_count);
    test_log_info("Received all 10 responses");
    
    // Verify all IDs are present
    for (int i = 0; i < 10; i++) {
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "\"id\":%d", 100 + i);
        TEST_ASSERT(strstr(buffer, id_str) != NULL);
    }
    
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(json_rpc_echo) {
    REGISTER_TEST(json_rpc_echo, json_rpc_echo, 
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_parse_error,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_invalid_request,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_method_not_found,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_notification,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_batch,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_param_types,
                  setup_json_rpc_test, teardown_json_rpc_test);
    REGISTER_TEST(json_rpc_echo, json_rpc_concurrent,
                  setup_json_rpc_test, teardown_json_rpc_test);
}