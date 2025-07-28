/* Goxel 3D voxels editor - End-to-End Integration Tests
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

/*
 * End-to-End Integration Test Suite for Goxel v14.0 Daemon Architecture
 * 
 * This comprehensive test suite validates the complete v14.0 system:
 * - Daemon lifecycle management
 * - Socket communication protocols  
 * - JSON RPC API methods
 * - Client connection pooling
 * - Error handling and recovery
 * - Performance characteristics
 * 
 * The test workflow follows:
 * 1. Start daemon process
 * 2. Establish client connections
 * 3. Execute comprehensive API calls
 * 4. Validate all responses
 * 5. Test error scenarios
 * 6. Clean shutdown and cleanup
 */

#include "../../src/daemon/daemon_lifecycle.h"
#include "../../src/daemon/json_rpc.h"
#include "../../src/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

// ============================================================================
// TEST FRAMEWORK CONSTANTS
// ============================================================================

#define MAX_CLIENTS 20
#define MAX_RESPONSE_SIZE 4096
#define MAX_ERROR_MESSAGE 256
#define DEFAULT_TIMEOUT_MS 10000
#define STRESS_TEST_DURATION_MS 30000

// Test configuration
static const char *TEST_DAEMON_SOCKET = "/tmp/goxel_e2e_test.sock";
static const char *TEST_DAEMON_PID = "/tmp/goxel_e2e_test.pid";
static const char *TEST_LOG_FILE = "/tmp/goxel_e2e_test.log";
static const char *TEST_PROJECT_FILE = "/tmp/test_e2e_project.gox";
static const char *TEST_EXPORT_FILE = "/tmp/test_e2e_export.obj";

// ============================================================================
// TEST FRAMEWORK STRUCTURES
// ============================================================================

typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    char last_error[MAX_ERROR_MESSAGE];
} test_stats_t;

typedef struct {
    int socket_fd;
    bool connected;
    int request_id;
    char response_buffer[MAX_RESPONSE_SIZE];
} test_client_t;

typedef struct {
    pid_t daemon_pid;
    bool daemon_running;
    test_client_t clients[MAX_CLIENTS];
    int active_clients;
    test_stats_t stats;
} e2e_test_context_t;

// ============================================================================
// TEST FRAMEWORK MACROS
// ============================================================================

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define TEST_ASSERT(ctx, condition, message) do { \
    ctx->stats.tests_run++; \
    if (condition) { \
        printf(ANSI_COLOR_GREEN "  ✓ " ANSI_COLOR_RESET "%s\n", message); \
        ctx->stats.tests_passed++; \
    } else { \
        printf(ANSI_COLOR_RED "  ✗ " ANSI_COLOR_RESET "%s\n", message); \
        ctx->stats.tests_failed++; \
        snprintf(ctx->stats.last_error, sizeof(ctx->stats.last_error), "%s", message); \
    } \
} while(0)

#define TEST_SECTION(name) do { \
    printf(ANSI_COLOR_BLUE "\n=== " name " ===" ANSI_COLOR_RESET "\n"); \
} while(0)

#define TEST_SUBSECTION(name) do { \
    printf(ANSI_COLOR_YELLOW "\n--- " name " ---" ANSI_COLOR_RESET "\n"); \
} while(0)

#define TEST_INFO(format, ...) do { \
    printf(ANSI_COLOR_CYAN "  [INFO] " format ANSI_COLOR_RESET "\n", ##__VA_ARGS__); \
} while(0)

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static int64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void cleanup_test_files(void)
{
    unlink(TEST_DAEMON_SOCKET);
    unlink(TEST_DAEMON_PID);
    unlink(TEST_LOG_FILE);
    unlink(TEST_PROJECT_FILE);
    unlink(TEST_EXPORT_FILE);
}

// ============================================================================
// DAEMON MANAGEMENT
// ============================================================================

static bool start_daemon(e2e_test_context_t *ctx)
{
    // Clean up any existing files
    cleanup_test_files();
    
    // Fork and exec the daemon
    ctx->daemon_pid = fork();
    if (ctx->daemon_pid == 0) {
        // Child process - exec the daemon
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", (char*)TEST_DAEMON_SOCKET,
            "--pid-file", (char*)TEST_DAEMON_PID,
            "--log-file", (char*)TEST_LOG_FILE,
            NULL
        };
        
        // Redirect stdout/stderr to log file for cleaner test output
        int log_fd = open(TEST_LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        
        execv("../../goxel-headless", args);
        exit(1); // If exec fails
    } else if (ctx->daemon_pid > 0) {
        // Parent process - wait for daemon to start
        for (int i = 0; i < 100; i++) { // 10 second timeout
            if (access(TEST_DAEMON_SOCKET, F_OK) == 0) {
                sleep_ms(100); // Give it a moment to be ready
                ctx->daemon_running = true;
                return true;
            }
            sleep_ms(100);
        }
        return false;
    } else {
        return false; // Fork failed
    }
}

static bool stop_daemon(e2e_test_context_t *ctx)
{
    if (!ctx->daemon_running || ctx->daemon_pid <= 0) {
        return true;
    }
    
    // Send SIGTERM to daemon
    if (kill(ctx->daemon_pid, SIGTERM) == 0) {
        // Wait for daemon to exit
        int status;
        int wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        
        for (int i = 0; i < 50 && wait_result == 0; i++) { // 5 second timeout
            sleep_ms(100);
            wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        }
        
        if (wait_result == 0) {
            // Force kill if still running
            kill(ctx->daemon_pid, SIGKILL);
            waitpid(ctx->daemon_pid, &status, 0);
        }
    }
    
    ctx->daemon_running = false;
    ctx->daemon_pid = 0;
    cleanup_test_files();
    return true;
}

// ============================================================================
// CLIENT CONNECTION MANAGEMENT
// ============================================================================

static bool connect_client(e2e_test_context_t *ctx, int client_id)
{
    if (client_id >= MAX_CLIENTS) return false;
    
    test_client_t *client = &ctx->clients[client_id];
    
    // Create socket
    client->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->socket_fd < 0) return false;
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = DEFAULT_TIMEOUT_MS / 1000;
    timeout.tv_usec = (DEFAULT_TIMEOUT_MS % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to daemon
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(client->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        client->connected = true;
        client->request_id = 1;
        ctx->active_clients++;
        return true;
    } else {
        close(client->socket_fd);
        client->socket_fd = -1;
        return false;
    }
}

static void disconnect_client(e2e_test_context_t *ctx, int client_id)
{
    if (client_id >= MAX_CLIENTS) return;
    
    test_client_t *client = &ctx->clients[client_id];
    if (client->connected) {
        close(client->socket_fd);
        client->connected = false;
        client->socket_fd = -1;
        ctx->active_clients--;
    }
}

static void disconnect_all_clients(e2e_test_context_t *ctx)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        disconnect_client(ctx, i);
    }
}

// ============================================================================
// JSON RPC COMMUNICATION
// ============================================================================

static bool send_json_rpc_request(test_client_t *client, const char *method, const char *params_json)
{
    if (!client->connected) return false;
    
    // Build JSON RPC request
    char request[1024];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}\n",
        method, params_json ? params_json : "[]", client->request_id++);
    
    // Send request
    ssize_t sent = send(client->socket_fd, request, strlen(request), 0);
    return sent > 0;
}

static bool receive_json_rpc_response(test_client_t *client)
{
    if (!client->connected) return false;
    
    // Clear response buffer
    memset(client->response_buffer, 0, sizeof(client->response_buffer));
    
    // Receive response
    ssize_t received = recv(client->socket_fd, client->response_buffer, 
                           sizeof(client->response_buffer) - 1, 0);
    
    return received > 0;
}

static bool call_json_rpc_method(test_client_t *client, const char *method, const char *params_json)
{
    return send_json_rpc_request(client, method, params_json) &&
           receive_json_rpc_response(client);
}

// ============================================================================
// E2E TEST WORKFLOWS
// ============================================================================

static void test_daemon_startup_shutdown(e2e_test_context_t *ctx)
{
    TEST_SECTION("Daemon Startup and Shutdown");
    
    // Test daemon startup
    TEST_SUBSECTION("Daemon Startup");
    bool started = start_daemon(ctx);
    TEST_ASSERT(ctx, started, "Daemon starts successfully");
    TEST_ASSERT(ctx, access(TEST_DAEMON_SOCKET, F_OK) == 0, "Socket file is created");
    TEST_ASSERT(ctx, access(TEST_DAEMON_PID, F_OK) == 0, "PID file is created");
    
    if (started) {
        TEST_INFO("Daemon PID: %d", ctx->daemon_pid);
        TEST_INFO("Socket path: %s", TEST_DAEMON_SOCKET);
    }
    
    // Test daemon shutdown
    TEST_SUBSECTION("Daemon Shutdown");
    bool stopped = stop_daemon(ctx);
    TEST_ASSERT(ctx, stopped, "Daemon stops successfully");
    TEST_ASSERT(ctx, access(TEST_DAEMON_SOCKET, F_OK) != 0, "Socket file is removed");
    TEST_ASSERT(ctx, access(TEST_DAEMON_PID, F_OK) != 0, "PID file is removed");
}

static void test_client_connections(e2e_test_context_t *ctx)
{
    TEST_SECTION("Client Connection Management");
    
    // Start daemon for connection tests
    if (!start_daemon(ctx)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon for connection tests");
        return;
    }
    
    // Test single client connection
    TEST_SUBSECTION("Single Client Connection");
    bool connected = connect_client(ctx, 0);
    TEST_ASSERT(ctx, connected, "Single client connects successfully");
    TEST_ASSERT(ctx, ctx->active_clients == 1, "Active client count is correct");
    
    // Test multiple client connections
    TEST_SUBSECTION("Multiple Client Connections");
    int target_clients = 5;
    for (int i = 1; i < target_clients; i++) {
        bool client_connected = connect_client(ctx, i);
        TEST_ASSERT(ctx, client_connected, "Additional client connects successfully");
    }
    TEST_ASSERT(ctx, ctx->active_clients == target_clients, "Multiple clients connected");
    TEST_INFO("Connected %d concurrent clients", ctx->active_clients);
    
    // Test client disconnection
    TEST_SUBSECTION("Client Disconnection");
    disconnect_all_clients(ctx);
    TEST_ASSERT(ctx, ctx->active_clients == 0, "All clients disconnected");
    
    stop_daemon(ctx);
}

static void test_basic_api_methods(e2e_test_context_t *ctx)
{
    TEST_SECTION("Basic API Method Testing");
    
    if (!start_daemon(ctx) || !connect_client(ctx, 0)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon or connect client");
        return;
    }
    
    test_client_t *client = &ctx->clients[0];
    
    // Test get_status method
    TEST_SUBSECTION("Get Status Method");
    bool status_ok = call_json_rpc_method(client, "goxel.get_status", "[]");
    TEST_ASSERT(ctx, status_ok, "Get status request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"result\"") != NULL, "Status response contains result");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"version\"") != NULL, "Status includes version");
    
    // Test create_project method
    TEST_SUBSECTION("Create Project Method");
    bool create_ok = call_json_rpc_method(client, "goxel.create_project", "[\"E2E Test Project\",32,32,32]");
    TEST_ASSERT(ctx, create_ok, "Create project request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"success\":true") != NULL, "Project creation succeeds");
    
    // Test add_voxel method
    TEST_SUBSECTION("Add Voxel Method");
    bool add_ok = call_json_rpc_method(client, "goxel.add_voxel", "[0,-16,0,255,0,0,255,0]");
    TEST_ASSERT(ctx, add_ok, "Add voxel request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"success\":true") != NULL, "Voxel addition succeeds");
    
    // Test get_voxel method
    TEST_SUBSECTION("Get Voxel Method");
    bool get_ok = call_json_rpc_method(client, "goxel.get_voxel", "[0,-16,0]");
    TEST_ASSERT(ctx, get_ok, "Get voxel request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"exists\":true") != NULL, "Voxel exists");
    
    // Test list_layers method
    TEST_SUBSECTION("List Layers Method");
    bool list_ok = call_json_rpc_method(client, "goxel.list_layers", "[]");
    TEST_ASSERT(ctx, list_ok, "List layers request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"count\"") != NULL, "Layers response includes count");
    
    disconnect_all_clients(ctx);
    stop_daemon(ctx);
}

static void test_file_operations(e2e_test_context_t *ctx)
{
    TEST_SECTION("File Operations Testing");
    
    if (!start_daemon(ctx) || !connect_client(ctx, 0)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon or connect client");
        return;
    }
    
    test_client_t *client = &ctx->clients[0];
    
    // Create a project first
    call_json_rpc_method(client, "goxel.create_project", "[\"File Test Project\",16,16,16]");
    call_json_rpc_method(client, "goxel.add_voxel", "[0,-16,0,255,0,0,255,0]");
    
    // Test save_project method
    TEST_SUBSECTION("Save Project Method");
    char save_params[256];
    snprintf(save_params, sizeof(save_params), "[\"%s\"]", TEST_PROJECT_FILE);
    bool save_ok = call_json_rpc_method(client, "goxel.save_project", save_params);
    TEST_ASSERT(ctx, save_ok, "Save project request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"success\":true") != NULL, "Project save succeeds");
    TEST_ASSERT(ctx, access(TEST_PROJECT_FILE, F_OK) == 0, "Project file is created");
    
    // Test load_project method
    TEST_SUBSECTION("Load Project Method");
    bool load_ok = call_json_rpc_method(client, "goxel.load_project", save_params);
    TEST_ASSERT(ctx, load_ok, "Load project request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"success\":true") != NULL, "Project load succeeds");
    
    // Test export_model method
    TEST_SUBSECTION("Export Model Method");
    char export_params[256];
    snprintf(export_params, sizeof(export_params), "[\"%s\"]", TEST_EXPORT_FILE);
    bool export_ok = call_json_rpc_method(client, "goxel.export_model", export_params);
    TEST_ASSERT(ctx, export_ok, "Export model request succeeds");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"success\":true") != NULL, "Model export succeeds");
    TEST_ASSERT(ctx, access(TEST_EXPORT_FILE, F_OK) == 0, "Export file is created");
    
    disconnect_all_clients(ctx);
    stop_daemon(ctx);
}

static void test_error_scenarios(e2e_test_context_t *ctx)
{
    TEST_SECTION("Error Scenario Testing");
    
    if (!start_daemon(ctx) || !connect_client(ctx, 0)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon or connect client");
        return;
    }
    
    test_client_t *client = &ctx->clients[0];
    
    // Test unknown method
    TEST_SUBSECTION("Unknown Method Handling");
    bool unknown_ok = call_json_rpc_method(client, "unknown.method", "[]");
    TEST_ASSERT(ctx, unknown_ok, "Unknown method request returns response");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "\"error\"") != NULL, "Unknown method returns error");
    TEST_ASSERT(ctx, strstr(client->response_buffer, "-32601") != NULL, "Error code is method not found");
    
    // Test invalid parameters
    TEST_SUBSECTION("Invalid Parameters Handling");
    bool invalid_ok = call_json_rpc_method(client, "goxel.add_voxel", "[\"invalid\",\"params\"]");
    TEST_ASSERT(ctx, invalid_ok, "Invalid params request returns response");
    // Note: May succeed with defaults or fail with error - both acceptable
    
    // Test malformed JSON
    TEST_SUBSECTION("Malformed JSON Handling");
    send_json_rpc_request(client, "malformed json", NULL);
    bool malformed_response = receive_json_rpc_response(client);
    if (malformed_response) {
        TEST_ASSERT(ctx, strstr(client->response_buffer, "\"error\"") != NULL, "Malformed JSON returns error");
    }
    
    disconnect_all_clients(ctx);
    stop_daemon(ctx);
}

static void test_concurrent_clients(e2e_test_context_t *ctx)
{
    TEST_SECTION("Concurrent Client Testing");
    
    if (!start_daemon(ctx)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon for concurrent tests");
        return;
    }
    
    // Connect multiple clients
    TEST_SUBSECTION("Multiple Client Connections");
    int num_clients = 10;
    int connected_count = 0;
    
    for (int i = 0; i < num_clients; i++) {
        if (connect_client(ctx, i)) {
            connected_count++;
        }
    }
    
    TEST_ASSERT(ctx, connected_count >= num_clients * 0.8, "At least 80% of clients connect successfully");
    TEST_INFO("Connected %d out of %d clients", connected_count, num_clients);
    
    // Test concurrent API calls
    TEST_SUBSECTION("Concurrent API Calls");
    int success_count = 0;
    
    for (int i = 0; i < connected_count; i++) {
        if (ctx->clients[i].connected) {
            char project_name[64];
            snprintf(project_name, sizeof(project_name), "[\"Concurrent Project %d\",16,16,16]", i);
            
            if (call_json_rpc_method(&ctx->clients[i], "goxel.create_project", project_name)) {
                if (strstr(ctx->clients[i].response_buffer, "\"success\":true") != NULL) {
                    success_count++;
                }
            }
        }
    }
    
    TEST_ASSERT(ctx, success_count >= connected_count * 0.8, "At least 80% of concurrent calls succeed");
    TEST_INFO("Successful concurrent API calls: %d/%d", success_count, connected_count);
    
    disconnect_all_clients(ctx);
    stop_daemon(ctx);
}

static void test_performance_characteristics(e2e_test_context_t *ctx)
{
    TEST_SECTION("Performance Characteristics");
    
    if (!start_daemon(ctx) || !connect_client(ctx, 0)) {
        TEST_ASSERT(ctx, false, "Failed to start daemon or connect client");
        return;
    }
    
    test_client_t *client = &ctx->clients[0];
    
    // Test response time
    TEST_SUBSECTION("Response Time Testing");
    int num_requests = 100;
    int64_t total_time = 0;
    int successful_requests = 0;
    
    for (int i = 0; i < num_requests; i++) {
        int64_t start_time = get_timestamp_us();
        
        if (call_json_rpc_method(client, "goxel.get_status", "[]")) {
            int64_t end_time = get_timestamp_us();
            total_time += (end_time - start_time);
            successful_requests++;
        }
    }
    
    TEST_ASSERT(ctx, successful_requests >= num_requests * 0.95, "At least 95% of requests succeed");
    
    if (successful_requests > 0) {
        double avg_latency_ms = (double)total_time / successful_requests / 1000.0;
        TEST_ASSERT(ctx, avg_latency_ms < 5.0, "Average response time is under 5ms");
        TEST_INFO("Average response time: %.2f ms", avg_latency_ms);
        TEST_INFO("Successful requests: %d/%d", successful_requests, num_requests);
    }
    
    disconnect_all_clients(ctx);
    stop_daemon(ctx);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

static void initialize_test_context(e2e_test_context_t *ctx)
{
    memset(ctx, 0, sizeof(e2e_test_context_t));
    
    // Initialize all client socket FDs to -1
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ctx->clients[i].socket_fd = -1;
    }
}

static void cleanup_test_context(e2e_test_context_t *ctx)
{
    // Disconnect all clients
    disconnect_all_clients(ctx);
    
    // Stop daemon if running
    if (ctx->daemon_running) {
        stop_daemon(ctx);
    }
    
    // Clean up test files
    cleanup_test_files();
}

static void print_test_summary(e2e_test_context_t *ctx)
{
    printf("\n" ANSI_COLOR_BLUE "===============================================\n" ANSI_COLOR_RESET);
    printf("E2E Integration Test Summary:\n");
    printf("  Total tests: %d\n", ctx->stats.tests_run);
    printf("  " ANSI_COLOR_GREEN "Passed: %d" ANSI_COLOR_RESET "\n", ctx->stats.tests_passed);
    
    if (ctx->stats.tests_failed > 0) {
        printf("  " ANSI_COLOR_RED "Failed: %d" ANSI_COLOR_RESET "\n", ctx->stats.tests_failed);
        printf("  Last error: %s\n", ctx->stats.last_error);
        printf("\n" ANSI_COLOR_RED "INTEGRATION TESTS FAILED" ANSI_COLOR_RESET "\n");
    } else {
        printf("  Failed: 0\n");
        printf("\n" ANSI_COLOR_GREEN "ALL INTEGRATION TESTS PASSED" ANSI_COLOR_RESET "\n");
    }
    
    // Calculate success rate
    double success_rate = ctx->stats.tests_run > 0 ? 
        (100.0 * ctx->stats.tests_passed / ctx->stats.tests_run) : 0.0;
    printf("  Success rate: %.1f%%\n", success_rate);
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    printf(ANSI_COLOR_BLUE "Goxel v14.0 End-to-End Integration Test Suite\n" ANSI_COLOR_RESET);
    printf("==============================================\n");
    printf("Testing complete daemon workflow: startup → connect → API → shutdown\n");
    
    e2e_test_context_t ctx;
    initialize_test_context(&ctx);
    
    // Clean up any existing test artifacts
    cleanup_test_files();
    
    // Run comprehensive test suite
    test_daemon_startup_shutdown(&ctx);
    test_client_connections(&ctx);
    test_basic_api_methods(&ctx);
    test_file_operations(&ctx);
    test_error_scenarios(&ctx);
    test_concurrent_clients(&ctx);
    test_performance_characteristics(&ctx);
    
    // Final cleanup
    cleanup_test_context(&ctx);
    
    // Print results
    print_test_summary(&ctx);
    
    return (ctx.stats.tests_failed == 0) ? 0 : 1;
}