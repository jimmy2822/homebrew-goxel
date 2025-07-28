/*
 * Goxel v14.6 Socket Connection Integration Test
 * 
 * Tests Unix domain socket connections, multiple clients, and connection cleanup
 * against the real daemon implementation.
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "../framework/test_framework.h"
#include <sys/un.h>
#include <errno.h>

#define DAEMON_BINARY "../../../goxel"
#define DAEMON_SOCKET "/tmp/goxel.sock"
#define DAEMON_PID_FILE "/tmp/goxel-daemon.pid"
#define CONNECTION_TIMEOUT_MS 1000
#define MAX_CLIENTS 32

static pid_t daemon_pid = -1;

// Helper function to check if socket exists
static bool socket_exists(void) {
    return access(DAEMON_SOCKET, F_OK) == 0;
}

// Setup function - start daemon
static void setup_daemon(void) {
    // Clean up any previous state
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    daemon_pid = fork();
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for daemon to start
    test_wait_for_condition(socket_exists, 2000);
}

// Teardown function - stop daemon
static void teardown_daemon(void) {
    if (daemon_pid > 0) {
        kill(daemon_pid, SIGTERM);
        waitpid(daemon_pid, NULL, 0);
        daemon_pid = -1;
    }
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
}

// Helper to connect to daemon socket
static int connect_to_daemon(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

// Test: Basic Unix socket connection
TEST_CASE(unix_socket_connection) {
    // Connect to daemon
    int client_fd = connect_to_daemon();
    TEST_ASSERT(client_fd >= 0);
    test_log_info("Connected to daemon socket");
    
    // Test socket is valid
    int flags = fcntl(client_fd, F_GETFL);
    TEST_ASSERT(flags >= 0);
    
    // Send a simple message
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\",\"params\":{\"message\":\"test\"}}\n";
    ssize_t sent = send(client_fd, msg, strlen(msg), 0);
    TEST_ASSERT(sent == strlen(msg));
    test_log_info("Sent message: %zu bytes", sent);
    
    // Receive response (with timeout)
    fd_set read_fds;
    struct timeval timeout = {1, 0}; // 1 second
    FD_ZERO(&read_fds);
    FD_SET(client_fd, &read_fds);
    
    int ready = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
    TEST_ASSERT(ready > 0);
    
    char buffer[1024];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    TEST_ASSERT(received > 0);
    buffer[received] = '\0';
    test_log_info("Received response: %s", buffer);
    
    // Verify response contains expected fields
    TEST_ASSERT(strstr(buffer, "\"jsonrpc\"") != NULL);
    TEST_ASSERT(strstr(buffer, "\"id\":1") != NULL);
    
    // Close connection
    close(client_fd);
    
    return TEST_PASS;
}

// Test: Multiple simultaneous connections
TEST_CASE(multiple_connections) {
    int clients[MAX_CLIENTS];
    int connected_count = 0;
    
    // Connect multiple clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = connect_to_daemon();
        if (clients[i] >= 0) {
            connected_count++;
        } else {
            test_log_warning("Failed to connect client %d: %s", i, strerror(errno));
            break;
        }
    }
    
    test_log_info("Connected %d clients", connected_count);
    TEST_ASSERT(connected_count >= 16); // Should support at least 16 clients
    
    // Send a message from each client
    for (int i = 0; i < connected_count; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"echo\",\"params\":{\"client\":%d}}\n",
                 i + 1, i);
        
        ssize_t sent = send(clients[i], msg, strlen(msg), 0);
        TEST_ASSERT(sent == strlen(msg));
    }
    
    // Receive responses
    for (int i = 0; i < connected_count; i++) {
        fd_set read_fds;
        struct timeval timeout = {1, 0};
        FD_ZERO(&read_fds);
        FD_SET(clients[i], &read_fds);
        
        int ready = select(clients[i] + 1, &read_fds, NULL, NULL, &timeout);
        TEST_ASSERT(ready > 0);
        
        char buffer[1024];
        ssize_t received = recv(clients[i], buffer, sizeof(buffer) - 1, 0);
        TEST_ASSERT(received > 0);
        buffer[received] = '\0';
        
        // Verify response has correct ID
        char expected_id[32];
        snprintf(expected_id, sizeof(expected_id), "\"id\":%d", i + 1);
        TEST_ASSERT(strstr(buffer, expected_id) != NULL);
    }
    
    // Close all connections
    for (int i = 0; i < connected_count; i++) {
        close(clients[i]);
    }
    
    test_log_info("All clients processed successfully");
    
    return TEST_PASS;
}

// Test: Connection cleanup on client disconnect
TEST_CASE(connection_cleanup) {
    // Connect and abruptly disconnect
    for (int i = 0; i < 10; i++) {
        int fd = connect_to_daemon();
        TEST_ASSERT(fd >= 0);
        
        // Send partial message then disconnect
        const char *partial = "{\"jsonrpc\":\"2.0\",";
        send(fd, partial, strlen(partial), 0);
        close(fd);
    }
    
    test_log_info("Performed 10 abrupt disconnections");
    
    // Daemon should still be responsive
    int test_fd = connect_to_daemon();
    TEST_ASSERT(test_fd >= 0);
    
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":999,\"method\":\"echo\",\"params\":{}}\n";
    ssize_t sent = send(test_fd, msg, strlen(msg), 0);
    TEST_ASSERT(sent == strlen(msg));
    
    char buffer[1024];
    ssize_t received = recv(test_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    
    // Wait a bit for response if needed
    if (received < 0 && errno == EAGAIN) {
        usleep(100000); // 100ms
        received = recv(test_fd, buffer, sizeof(buffer) - 1, 0);
    }
    
    TEST_ASSERT(received > 0);
    close(test_fd);
    
    test_log_info("Daemon still responsive after cleanup test");
    
    return TEST_PASS;
}

// Test: Connection refused when daemon not running
TEST_CASE(connection_refused) {
    // Stop the daemon
    teardown_daemon();
    
    // Try to connect
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_ASSERT(fd >= 0);
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
    
    int result = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    TEST_ASSERT(result < 0);
    TEST_ASSERT(errno == ECONNREFUSED || errno == ENOENT);
    
    close(fd);
    test_log_info("Connection correctly refused when daemon not running");
    
    // Restart daemon for other tests
    setup_daemon();
    
    return TEST_PASS;
}

// Test: Large message handling
TEST_CASE(large_message_handling) {
    int fd = connect_to_daemon();
    TEST_ASSERT(fd >= 0);
    
    // Create a large JSON-RPC request
    char *large_params = malloc(64 * 1024); // 64KB
    memset(large_params, 'A', 64 * 1024 - 1);
    large_params[64 * 1024 - 1] = '\0';
    
    char *msg = malloc(128 * 1024);
    snprintf(msg, 128 * 1024,
             "{\"jsonrpc\":\"2.0\",\"id\":1000,\"method\":\"echo\",\"params\":{\"data\":\"%s\"}}\n",
             large_params);
    
    size_t msg_len = strlen(msg);
    test_log_info("Sending large message: %zu bytes", msg_len);
    
    // Send in chunks to avoid blocking
    size_t total_sent = 0;
    while (total_sent < msg_len) {
        ssize_t sent = send(fd, msg + total_sent, msg_len - total_sent, 0);
        if (sent < 0) {
            TEST_ASSERT(errno == EAGAIN || errno == EWOULDBLOCK);
            usleep(10000); // 10ms
            continue;
        }
        total_sent += sent;
    }
    
    TEST_ASSERT(total_sent == msg_len);
    test_log_info("Large message sent successfully");
    
    // Receive response (may be large too)
    char response[4096];
    ssize_t received = 0;
    int attempts = 0;
    
    while (attempts < 100) { // 1 second timeout
        ssize_t chunk = recv(fd, response + received, sizeof(response) - received - 1, MSG_DONTWAIT);
        if (chunk > 0) {
            received += chunk;
            if (strchr(response, '\n') != NULL) {
                break; // Complete message received
            }
        } else if (chunk < 0 && errno != EAGAIN) {
            break;
        }
        usleep(10000); // 10ms
        attempts++;
    }
    
    TEST_ASSERT(received > 0);
    response[received] = '\0';
    
    // Verify response structure
    TEST_ASSERT(strstr(response, "\"id\":1000") != NULL);
    
    free(large_params);
    free(msg);
    close(fd);
    
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(socket_connection) {
    // Set up suite-level daemon management
    test_suite_t *suite = find_or_create_suite("socket_connection");
    suite->suite_setup = setup_daemon;
    suite->suite_teardown = teardown_daemon;
    
    REGISTER_TEST(socket_connection, unix_socket_connection, NULL, NULL);
    REGISTER_TEST(socket_connection, multiple_connections, NULL, NULL);
    REGISTER_TEST(socket_connection, connection_cleanup, NULL, NULL);
    REGISTER_TEST(socket_connection, connection_refused, NULL, NULL);
    REGISTER_TEST(socket_connection, large_message_handling, NULL, NULL);
}