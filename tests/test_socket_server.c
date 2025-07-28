/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Comprehensive Unit Tests for Socket Server Infrastructure
 * 
 * This test suite validates the Unix socket server implementation
 * for Goxel v14.0 daemon architecture, covering:
 * - Server lifecycle management
 * - Client connection handling
 * - Message passing
 * - Error conditions
 * - Performance characteristics
 * - Resource management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <signal.h>

// Include socket server under test
#include "../src/daemon/socket_server.h"

// ============================================================================
// TEST FRAMEWORK MACROS
// ============================================================================

#define TEST_SOCKET_PATH "/tmp/goxel_test_daemon.sock"
#define TEST_TIMEOUT_MS 5000
#define MAX_TEST_CLIENTS 10

// Color output for test results
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_START(name) do { \
    printf("Testing %s... ", name); \
    fflush(stdout); \
    test_count++; \
} while(0)

#define TEST_PASS() do { \
    printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); \
    test_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf(COLOR_RED "FAIL" COLOR_RESET " - %s\n", msg); \
    test_failed++; \
} while(0)

#define TEST_ASSERT(condition, msg) do { \
    if (!(condition)) { \
        TEST_FAIL(msg); \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf(COLOR_RED "FAIL" COLOR_RESET " - %s (expected %d, got %d)\n", msg, (int)(b), (int)(a)); \
        test_failed++; \
        return 0; \
    } \
} while(0)

// ============================================================================
// TEST UTILITIES
// ============================================================================

static void cleanup_socket_file(void)
{
    unlink(TEST_SOCKET_PATH);
}

static int64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

static void msleep(int ms)
{
    usleep(ms * 1000);
}

// Simple echo message handler for tests
static socket_message_t *echo_message_handler(socket_server_t *server,
                                             socket_client_t *client,
                                             const socket_message_t *message,
                                             void *user_data)
{
    (void)server;
    (void)client;
    (void)user_data;
    
    // Echo the message back
    return socket_message_create(message->id, message->type, 
                                message->data, message->length);
}

// Client connection event handler for tests
static volatile int client_connected = 0;
static volatile int client_disconnected = 0;

static void client_event_handler(socket_server_t *server,
                                socket_client_t *client,
                                bool connected,
                                void *user_data)
{
    (void)server;
    (void)client;
    (void)user_data;
    
    if (connected) {
        client_connected++;
    } else {
        client_disconnected++;
    }
}

// Test client connection function
static int create_test_client(const char *socket_path)
{
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(client_fd);
        return -1;
    }
    
    return client_fd;
}

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

static int test_server_creation(void)
{
    TEST_START("server creation and destruction");
    
    cleanup_socket_file();
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    TEST_ASSERT(!socket_server_is_running(server), "Server should not be running initially");
    
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

static int test_invalid_config(void)
{
    TEST_START("invalid configuration handling");
    
    // Test NULL config
    socket_server_t *server = socket_server_create(NULL);
    TEST_ASSERT(server == NULL, "Should reject NULL config");
    
    // Test invalid socket path
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = NULL;
    server = socket_server_create(&config);
    TEST_ASSERT(server == NULL, "Should reject NULL socket path");
    
    // Test path too long
    config.socket_path = "/this/is/a/very/long/path/that/should/exceed/the/maximum/unix/socket/path/length/limit/and/cause/validation/to/fail";
    server = socket_server_create(&config);
    TEST_ASSERT(server == NULL, "Should reject overly long socket path");
    
    TEST_PASS();
    return 1;
}

static int test_server_lifecycle(void)
{
    TEST_START("server lifecycle (start/stop)");
    
    cleanup_socket_file();
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    // Test starting server
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    TEST_ASSERT(socket_server_is_running(server), "Server should be running");
    
    // Wait for server to be ready
    msleep(100);
    
    // Test stopping server
    result = socket_server_stop(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to stop server");
    TEST_ASSERT(!socket_server_is_running(server), "Server should be stopped");
    
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

static int test_socket_path_validation(void)
{
    TEST_START("socket path availability checking");
    
    cleanup_socket_file();
    
    // Test available path
    TEST_ASSERT(socket_server_path_available(TEST_SOCKET_PATH), "Path should be available");
    
    // Create server to make path unavailable
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Path should now be in use
    TEST_ASSERT(!socket_server_path_available(TEST_SOCKET_PATH), "Path should be in use");
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    // Clean up
    result = socket_server_cleanup_path(TEST_SOCKET_PATH);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to cleanup socket path");
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// CLIENT CONNECTION TESTS
// ============================================================================

static int test_single_client_connection(void)
{
    TEST_START("single client connection");
    
    cleanup_socket_file();
    client_connected = 0;
    client_disconnected = 0;
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    config.client_handler = client_event_handler;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Connect client
    int client_fd = create_test_client(TEST_SOCKET_PATH);
    TEST_ASSERT(client_fd >= 0, "Failed to connect client");
    
    msleep(100);
    
    // Check client count
    int client_count = socket_server_get_client_count(server);
    TEST_ASSERT(client_count == 1, "Expected 1 client");
    TEST_ASSERT(client_connected == 1, "Client connection event not triggered");
    
    // Disconnect client
    close(client_fd);
    
    msleep(100);
    
    TEST_ASSERT(client_disconnected == 1, "Client disconnection event not triggered");
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

static int test_multiple_client_connections(void)
{
    TEST_START("multiple client connections");
    
    cleanup_socket_file();
    client_connected = 0;
    client_disconnected = 0;
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    config.max_connections = MAX_TEST_CLIENTS;
    config.client_handler = client_event_handler;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Connect multiple clients
    int client_fds[MAX_TEST_CLIENTS];
    for (int i = 0; i < MAX_TEST_CLIENTS; i++) {
        client_fds[i] = create_test_client(TEST_SOCKET_PATH);
        TEST_ASSERT(client_fds[i] >= 0, "Failed to connect client");
        msleep(10);
    }
    
    msleep(100);
    
    // Check client count
    int client_count = socket_server_get_client_count(server);
    TEST_ASSERT(client_count == MAX_TEST_CLIENTS, "Unexpected client count");
    TEST_ASSERT(client_connected == MAX_TEST_CLIENTS, "Not all client connection events triggered");
    
    // Disconnect all clients
    for (int i = 0; i < MAX_TEST_CLIENTS; i++) {
        close(client_fds[i]);
        msleep(10);
    }
    
    msleep(100);
    
    TEST_ASSERT(client_disconnected == MAX_TEST_CLIENTS, "Not all client disconnection events triggered");
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// MESSAGE PASSING TESTS
// ============================================================================

static int test_message_creation_and_destruction(void)
{
    TEST_START("message creation and destruction");
    
    const char *test_data = "Hello, World!";
    uint32_t test_id = 12345;
    uint32_t test_type = 67890;
    
    // Test creating message with data
    socket_message_t *msg = socket_message_create(test_id, test_type, test_data, strlen(test_data));
    TEST_ASSERT(msg != NULL, "Failed to create message");
    TEST_ASSERT(msg->id == test_id, "Message ID mismatch");
    TEST_ASSERT(msg->type == test_type, "Message type mismatch");
    TEST_ASSERT(msg->length == strlen(test_data), "Message length mismatch");
    TEST_ASSERT(memcmp(msg->data, test_data, strlen(test_data)) == 0, "Message data mismatch");
    TEST_ASSERT(msg->timestamp > 0, "Message timestamp not set");
    
    socket_message_destroy(msg);
    
    // Test creating message without data
    msg = socket_message_create(test_id, test_type, NULL, 0);
    TEST_ASSERT(msg != NULL, "Failed to create empty message");
    TEST_ASSERT(msg->data == NULL, "Empty message should have NULL data");
    TEST_ASSERT(msg->length == 0, "Empty message should have zero length");
    
    socket_message_destroy(msg);
    
    // Test creating JSON message
    const char *json_data = "{\"method\": \"test\", \"params\": [1, 2, 3]}";
    msg = socket_message_create_json(test_id, test_type, json_data);
    TEST_ASSERT(msg != NULL, "Failed to create JSON message");
    TEST_ASSERT(msg->length == strlen(json_data), "JSON message length mismatch");
    
    socket_message_destroy(msg);
    
    TEST_PASS();
    return 1;
}

static int test_basic_message_passing(void)
{
    TEST_START("basic message passing");
    
    cleanup_socket_file();
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    config.msg_handler = echo_message_handler;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Connect client
    int client_fd = create_test_client(TEST_SOCKET_PATH);
    TEST_ASSERT(client_fd >= 0, "Failed to connect client");
    
    msleep(100);
    
    // Send a simple message through raw socket (basic test)
    const char *test_msg = "test message";
    ssize_t sent = send(client_fd, test_msg, strlen(test_msg), 0);
    TEST_ASSERT(sent == (ssize_t)strlen(test_msg), "Failed to send message");
    
    msleep(100);
    
    close(client_fd);
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

static int test_error_handling(void)
{
    TEST_START("error handling and validation");
    
    // Test error message retrieval
    const char *error_msg = socket_error_string(SOCKET_ERROR_INVALID_PARAMETER);
    TEST_ASSERT(error_msg != NULL, "Error message should not be NULL");
    TEST_ASSERT(strlen(error_msg) > 0, "Error message should not be empty");
    
    // Test unknown error
    error_msg = socket_error_string((socket_error_t)-999);
    TEST_ASSERT(error_msg != NULL, "Unknown error message should not be NULL");
    
    // Test server error retrieval
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    const char *last_error = socket_server_get_last_error(server);
    // Should be NULL initially (no error) - just test it doesn't crash
    (void)last_error; // Suppress unused variable warning
    
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

static int test_resource_limits(void)
{
    TEST_START("resource limits and bounds checking");
    
    cleanup_socket_file();
    
    // Test with very low connection limit
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    config.max_connections = 1;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Connect first client (should succeed)
    int client1_fd = create_test_client(TEST_SOCKET_PATH);
    TEST_ASSERT(client1_fd >= 0, "Failed to connect first client");
    
    msleep(100);
    
    // Verify one client is connected
    int client_count = socket_server_get_client_count(server);
    TEST_ASSERT(client_count == 1, "Expected 1 client connected");
    
    close(client1_fd);
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

static int test_connection_performance(void)
{
    TEST_START("connection performance");
    
    cleanup_socket_file();
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    config.max_connections = 100;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Measure time to connect/disconnect multiple clients
    int64_t start_time = get_time_us();
    
    const int num_clients = 50;
    for (int i = 0; i < num_clients; i++) {
        int client_fd = create_test_client(TEST_SOCKET_PATH);
        if (client_fd >= 0) {
            close(client_fd);
        }
        
        // Small delay to avoid overwhelming the server
        if (i % 10 == 0) {
            msleep(1);
        }
    }
    
    int64_t end_time = get_time_us();
    int64_t duration_us = end_time - start_time;
    double duration_ms = duration_us / 1000.0;
    
    printf("(%d connections in %.2fms, avg %.2fms per connection) ", 
           num_clients, duration_ms, duration_ms / num_clients);
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    // Performance should be reasonable (less than 10ms per connection on average)
    TEST_ASSERT(duration_ms / num_clients < 10.0, "Connection performance too slow");
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

static int test_server_statistics(void)
{
    TEST_START("server statistics tracking");
    
    cleanup_socket_file();
    
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = TEST_SOCKET_PATH;
    
    socket_server_t *server = socket_server_create(&config);
    TEST_ASSERT(server != NULL, "Failed to create server");
    
    socket_error_t result = socket_server_start(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to start server");
    
    msleep(100);
    
    // Get initial statistics
    socket_server_stats_t stats;
    result = socket_server_get_stats(server, &stats);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to get statistics");
    TEST_ASSERT(stats.current_connections == 0, "Initial connection count should be 0");
    TEST_ASSERT(stats.start_time > 0, "Start time should be set");
    
    // Connect a client
    int client_fd = create_test_client(TEST_SOCKET_PATH);
    TEST_ASSERT(client_fd >= 0, "Failed to connect client");
    
    msleep(100);
    
    // Check updated statistics
    result = socket_server_get_stats(server, &stats);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to get updated statistics");
    TEST_ASSERT(stats.current_connections == 1, "Connection count should be 1");
    TEST_ASSERT(stats.total_connections >= 1, "Total connections should be at least 1");
    
    close(client_fd);
    
    msleep(100);
    
    // Test statistics reset
    result = socket_server_reset_stats(server);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to reset statistics");
    
    result = socket_server_get_stats(server, &stats);
    TEST_ASSERT(result == SOCKET_SUCCESS, "Failed to get reset statistics");
    TEST_ASSERT(stats.total_connections == 0, "Total connections should be reset");
    
    socket_server_stop(server);
    socket_server_destroy(server);
    
    TEST_PASS();
    return 1;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

static void print_test_summary(void)
{
    printf("\n" COLOR_YELLOW "=== Test Summary ===" COLOR_RESET "\n");
    printf("Total tests: %d\n", test_count);
    printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", test_passed);
    if (test_failed > 0) {
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", test_failed);
    } else {
        printf("Failed: 0\n");
    }
    
    if (test_failed == 0) {
        printf(COLOR_GREEN "\nAll tests passed! ✓" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "\nSome tests failed! ✗" COLOR_RESET "\n");
    }
    
    double pass_rate = test_count > 0 ? (double)test_passed / test_count * 100.0 : 0.0;
    printf("Pass rate: %.1f%%\n", pass_rate);
}

int main(void)
{
    printf(COLOR_YELLOW "=== Goxel v14.0 Socket Server Infrastructure Tests ===" COLOR_RESET "\n\n");
    
    // Ignore SIGPIPE to prevent test crashes
    signal(SIGPIPE, SIG_IGN);
    
    // Run all tests
    test_server_creation();
    test_invalid_config();
    test_server_lifecycle();
    test_socket_path_validation();
    test_single_client_connection();
    test_multiple_client_connections();
    test_message_creation_and_destruction();
    test_basic_message_passing();
    test_error_handling();
    test_resource_limits();
    test_connection_performance();
    test_server_statistics();
    
    // Clean up
    cleanup_socket_file();
    
    print_test_summary();
    
    return test_failed > 0 ? 1 : 0;
}