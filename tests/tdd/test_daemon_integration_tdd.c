#include "tdd_framework.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/time.h>

#define TEST_SOCKET_PATH "/tmp/goxel_integration_test.sock"
#define DAEMON_PATH "../../goxel-daemon"
#define BUFFER_SIZE 4096

// Helper to remove socket file if it exists
void cleanup_socket() {
    unlink(TEST_SOCKET_PATH);
}

// Helper to check if socket file exists
int socket_exists() {
    return access(TEST_SOCKET_PATH, F_OK) == 0;
}

// Helper to start daemon process
pid_t start_daemon() {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - run daemon
        execl(DAEMON_PATH, "goxel-daemon", "--foreground", "--socket", TEST_SOCKET_PATH, NULL);
        // If execl fails
        exit(1);
    }
    return pid;
}

// Helper to stop daemon process
void stop_daemon(pid_t pid) {
    if (pid > 0) {
        kill(pid, SIGTERM);
        
        // Wait for daemon to exit with timeout
        int wait_time = 0;
        int status;
        while (wait_time < 2000000) { // 2 second timeout
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                // Process exited
                return;
            } else if (result == -1) {
                // Error
                return;
            }
            // Still running, wait a bit more
            usleep(100000); // 100ms
            wait_time += 100000;
        }
        
        // If still running after timeout, force kill
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
}

// Helper to connect to daemon socket
int connect_to_daemon() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

// Helper to send JSON-RPC request
int send_request(int sock, const char* request) {
    size_t len = strlen(request);
    return send(sock, request, len, 0) == len;
}

// Helper to receive response with timeout
int receive_response(int sock, char* buffer, size_t buffer_size, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int bytes = recv(sock, buffer, buffer_size - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    }
    return bytes;
}

// Test 1: Daemon starts and creates socket
int test_daemon_creates_socket() {
    cleanup_socket();
    TEST_ASSERT(!socket_exists(), "Socket should not exist before daemon starts");
    
    pid_t daemon_pid = start_daemon();
    TEST_ASSERT(daemon_pid > 0, "Daemon should start successfully");
    
    // Give daemon time to create socket
    usleep(100000); // 100ms
    
    TEST_ASSERT(socket_exists(), "Socket should exist after daemon starts");
    
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 2: Client can connect to daemon socket
int test_client_connects_to_daemon() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000); // Let daemon start
    
    int sock = connect_to_daemon();
    TEST_ASSERT(sock >= 0, "Client should connect to daemon successfully");
    
    if (sock >= 0) close(sock);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 3: First request succeeds
int test_first_request_succeeds() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    int sock = connect_to_daemon();
    TEST_ASSERT(sock >= 0, "Should connect to daemon");
    
    const char* request = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test\",16,16,16],\"id\":1}\n";
    TEST_ASSERT(send_request(sock, request), "Should send request successfully");
    
    char response[BUFFER_SIZE];
    int bytes = receive_response(sock, response, BUFFER_SIZE, 1000);
    TEST_ASSERT(bytes > 0, "Should receive response");
    TEST_ASSERT(strstr(response, "\"result\"") != NULL, "Response should contain result");
    
    close(sock);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 4: Second request on same connection hangs (known limitation)
int test_second_request_hangs() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    int sock = connect_to_daemon();
    TEST_ASSERT(sock >= 0, "Should connect to daemon");
    
    // First request
    const char* request1 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test1\",16,16,16],\"id\":1}\n";
    send_request(sock, request1);
    
    char response[BUFFER_SIZE];
    receive_response(sock, response, BUFFER_SIZE, 1000);
    
    // Second request - should timeout due to daemon hang
    const char* request2 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test2\",16,16,16],\"id\":2}\n";
    TEST_ASSERT(send_request(sock, request2), "Should send second request");
    
    int bytes = receive_response(sock, response, BUFFER_SIZE, 500); // Short timeout
    TEST_ASSERT(bytes <= 0, "Second request should timeout (daemon hangs)");
    
    close(sock);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 5: Reconnecting allows another request
int test_reconnect_allows_new_request() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    // First connection
    int sock1 = connect_to_daemon();
    const char* request1 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test1\",16,16,16],\"id\":1}\n";
    send_request(sock1, request1);
    char response[BUFFER_SIZE];
    receive_response(sock1, response, BUFFER_SIZE, 1000);
    close(sock1);
    
    // Second connection - should work
    int sock2 = connect_to_daemon();
    TEST_ASSERT(sock2 >= 0, "Should connect again after closing first connection");
    
    const char* request2 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test2\",16,16,16],\"id\":2}\n";
    TEST_ASSERT(send_request(sock2, request2), "Should send request on new connection");
    
    int bytes = receive_response(sock2, response, BUFFER_SIZE, 1000);
    TEST_ASSERT(bytes > 0, "Should receive response on new connection");
    
    close(sock2);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 6: Multiple clients can connect
int test_multiple_clients_connect() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    int sock1 = connect_to_daemon();
    TEST_ASSERT(sock1 >= 0, "First client should connect");
    
    int sock2 = connect_to_daemon();
    TEST_ASSERT(sock2 >= 0, "Second client should connect");
    
    int sock3 = connect_to_daemon();
    TEST_ASSERT(sock3 >= 0, "Third client should connect");
    
    if (sock1 >= 0) close(sock1);
    if (sock2 >= 0) close(sock2);
    if (sock3 >= 0) close(sock3);
    
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 7: Daemon cleans up socket on shutdown
int test_daemon_cleans_up_socket() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    TEST_ASSERT(daemon_pid > 0, "Daemon should start");
    
    // Give daemon more time to create socket
    usleep(500000); // 500ms
    
    TEST_ASSERT(socket_exists(), "Socket should exist while daemon runs");
    
    stop_daemon(daemon_pid);
    
    // Wait a bit more for socket cleanup
    usleep(500000); // 500ms
    
    TEST_ASSERT(!socket_exists(), "Socket should be removed after daemon stops");
    
    return 1;
}

// Test 8: Malformed JSON handling
int test_malformed_json_handling() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    int sock = connect_to_daemon();
    TEST_ASSERT(sock >= 0, "Should connect to daemon");
    
    const char* malformed = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":[,\"id\":1}\n";
    TEST_ASSERT(send_request(sock, malformed), "Should send malformed request");
    
    char response[BUFFER_SIZE];
    int bytes = receive_response(sock, response, BUFFER_SIZE, 1000);
    TEST_ASSERT(bytes > 0, "Should receive error response");
    TEST_ASSERT(strstr(response, "\"error\"") != NULL, "Response should contain error");
    
    close(sock);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 9: Large payload handling
int test_large_payload_handling() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    int sock = connect_to_daemon();
    TEST_ASSERT(sock >= 0, "Should connect to daemon");
    
    // Create large voxel array
    char large_request[8192];
    snprintf(large_request, sizeof(large_request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[");
    
    // Add 100 voxels
    for (int i = 0; i < 100; i++) {
        char voxel[64];
        snprintf(voxel, sizeof(voxel), 
            "%s{\"position\":[%d,%d,%d],\"color\":[255,0,0,255]}", 
            i > 0 ? "," : "", i, i, i);
        strcat(large_request, voxel);
    }
    strcat(large_request, "]},\"id\":1}\n");
    
    TEST_ASSERT(send_request(sock, large_request), "Should send large request");
    
    char response[BUFFER_SIZE];
    int bytes = receive_response(sock, response, BUFFER_SIZE, 2000);
    TEST_ASSERT(bytes > 0, "Should handle large payload");
    
    close(sock);
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

// Test 10: Concurrent client requests
int test_concurrent_client_requests() {
    cleanup_socket();
    
    pid_t daemon_pid = start_daemon();
    usleep(100000);
    
    // Connect multiple clients
    int socks[3];
    for (int i = 0; i < 3; i++) {
        socks[i] = connect_to_daemon();
        TEST_ASSERT(socks[i] >= 0, "Client should connect");
    }
    
    // Send requests from each client
    const char* requests[3] = {
        "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test1\",16,16,16],\"id\":1}\n",
        "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test2\",16,16,16],\"id\":2}\n",
        "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test3\",16,16,16],\"id\":3}\n"
    };
    
    // Each client can make one request
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(send_request(socks[i], requests[i]), "Should send request");
        
        char response[BUFFER_SIZE];
        int bytes = receive_response(socks[i], response, BUFFER_SIZE, 1000);
        TEST_ASSERT(bytes > 0, "Should receive response");
        
        close(socks[i]);
    }
    
    stop_daemon(daemon_pid);
    cleanup_socket();
    return 1;
}

int main(int argc, char *argv[]) {
    TEST_SUITE_BEGIN();
    
    // If a specific test is requested, run only that test
    if (argc > 1) {
        if (strcmp(argv[1], "test_daemon_creates_socket") == 0) {
            RUN_TEST(test_daemon_creates_socket);
        } else if (strcmp(argv[1], "test_client_connects_to_daemon") == 0) {
            RUN_TEST(test_client_connects_to_daemon);
        } else if (strcmp(argv[1], "test_daemon_cleans_up_socket") == 0) {
            RUN_TEST(test_daemon_cleans_up_socket);
        } else if (strcmp(argv[1], "test_first_request_succeeds") == 0) {
            RUN_TEST(test_first_request_succeeds);
        } else if (strcmp(argv[1], "test_second_request_hangs") == 0) {
            RUN_TEST(test_second_request_hangs);
        } else if (strcmp(argv[1], "test_reconnect_allows_new_request") == 0) {
            RUN_TEST(test_reconnect_allows_new_request);
        } else if (strcmp(argv[1], "test_multiple_clients_connect") == 0) {
            RUN_TEST(test_multiple_clients_connect);
        } else if (strcmp(argv[1], "test_concurrent_client_requests") == 0) {
            RUN_TEST(test_concurrent_client_requests);
        } else if (strcmp(argv[1], "test_malformed_json_handling") == 0) {
            RUN_TEST(test_malformed_json_handling);
        } else if (strcmp(argv[1], "test_large_payload_handling") == 0) {
            RUN_TEST(test_large_payload_handling);
        } else {
            printf("Unknown test: %s\n", argv[1]);
            return 1;
        }
    } else {
        // Run all tests
        // Socket lifecycle tests
        RUN_TEST(test_daemon_creates_socket);
        RUN_TEST(test_client_connects_to_daemon);
        RUN_TEST(test_daemon_cleans_up_socket);
        
        // Single request per session tests
        RUN_TEST(test_first_request_succeeds);
        RUN_TEST(test_second_request_hangs);
        RUN_TEST(test_reconnect_allows_new_request);
        
        // Multi-client tests
        RUN_TEST(test_multiple_clients_connect);
        RUN_TEST(test_concurrent_client_requests);
        
        // Protocol tests
        RUN_TEST(test_malformed_json_handling);
        RUN_TEST(test_large_payload_handling);
    }
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}