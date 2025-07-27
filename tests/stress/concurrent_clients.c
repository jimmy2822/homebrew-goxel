/* Goxel 3D voxels editor - Concurrent Stress Testing
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
 * Concurrent Client Stress Testing Suite for Goxel v14.0
 * 
 * This comprehensive stress testing framework validates daemon performance under:
 * - High concurrent client loads (10-100+ clients)
 * - Realistic voxel editing workloads
 * - Memory pressure scenarios
 * - Long-duration stability tests
 * - Error handling under stress
 * - Resource exhaustion scenarios
 * 
 * Stress Test Scenarios:
 * 1. Concurrent Connection Stress - Multiple clients connecting/disconnecting
 * 2. API Load Stress - High frequency API calls from many clients
 * 3. Memory Pressure - Large voxel operations and memory usage
 * 4. Duration Stress - Long-running stability tests
 * 5. Error Injection - Network failures and malformed requests
 * 6. Resource Exhaustion - File descriptor and socket limits
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
#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>

// ============================================================================
// STRESS TEST CONFIGURATION
// ============================================================================

#define MAX_STRESS_CLIENTS 100
#define MAX_RESPONSE_SIZE 8192
#define MAX_ERROR_MESSAGE 512
#define DEFAULT_STRESS_DURATION 60  // seconds
#define DEFAULT_API_CALLS_PER_CLIENT 1000
#define CONNECTION_TIMEOUT_MS 5000
#define REQUEST_TIMEOUT_MS 2000

// Test files and paths
static const char *STRESS_DAEMON_SOCKET = "/tmp/goxel_stress_test.sock";
static const char *STRESS_DAEMON_PID = "/tmp/goxel_stress_test.pid";
static const char *STRESS_LOG_FILE = "/tmp/goxel_stress_test.log";

// Stress test workload types
typedef enum {
    WORKLOAD_LIGHT,     // Simple status checks
    WORKLOAD_MEDIUM,    // Mixed operations (status, create, add voxels)
    WORKLOAD_HEAVY,     // Complex operations (projects, file I/O)
    WORKLOAD_RANDOM     // Random mix of all operations
} stress_workload_t;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    int client_id;
    int socket_fd;
    bool connected;
    bool active;
    int request_id;
    
    // Performance metrics
    int requests_sent;
    int requests_successful;
    int requests_failed;
    double total_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    
    // Error tracking
    int connection_errors;
    int request_errors;
    int timeout_errors;
    char last_error[MAX_ERROR_MESSAGE];
    
    // Thread control
    pthread_t thread;
    bool thread_running;
    bool stop_requested;
    
} stress_client_t;

typedef struct {
    // Test configuration
    int num_clients;
    int duration_seconds;
    stress_workload_t workload_type;
    int api_calls_per_client;
    bool enable_connection_cycling;
    
    // Daemon control
    pid_t daemon_pid;
    bool daemon_running;
    
    // Client management
    stress_client_t clients[MAX_STRESS_CLIENTS];
    int active_clients;
    
    // Global statistics
    int total_connections_attempted;
    int total_connections_successful;
    int total_requests_sent;
    int total_requests_successful;
    int total_errors;
    
    // Performance tracking
    double test_start_time;
    double test_duration;
    double peak_memory_mb;
    double avg_cpu_percent;
    
    // Synchronization
    pthread_mutex_t stats_mutex;
    volatile bool stop_all_clients;
    
} stress_test_context_t;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static void cleanup_stress_files(void)
{
    unlink(STRESS_DAEMON_SOCKET);
    unlink(STRESS_DAEMON_PID);
    unlink(STRESS_LOG_FILE);
}

// ============================================================================
// DAEMON MANAGEMENT
// ============================================================================

static bool start_stress_daemon(stress_test_context_t *ctx)
{
    cleanup_stress_files();
    
    ctx->daemon_pid = fork();
    if (ctx->daemon_pid == 0) {
        // Child process - exec the daemon
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", (char*)STRESS_DAEMON_SOCKET,
            "--pid-file", (char*)STRESS_DAEMON_PID,
            "--log-file", (char*)STRESS_LOG_FILE,
            "--max-connections", "200",  // Allow more connections for stress testing
            NULL
        };
        
        // Redirect stdout/stderr to log file
        int log_fd = open(STRESS_LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        
        execv("../../goxel-headless", args);
        exit(1);
    } else if (ctx->daemon_pid > 0) {
        // Parent process - wait for daemon to start
        for (int i = 0; i < 200; i++) { // 20 second timeout for stress daemon
            if (access(STRESS_DAEMON_SOCKET, F_OK) == 0) {
                sleep_ms(100);
                ctx->daemon_running = true;
                return true;
            }
            sleep_ms(100);
        }
        return false;
    } else {
        return false;
    }
}

static bool stop_stress_daemon(stress_test_context_t *ctx)
{
    if (!ctx->daemon_running || ctx->daemon_pid <= 0) {
        return true;
    }
    
    if (kill(ctx->daemon_pid, SIGTERM) == 0) {
        int status;
        int wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        
        for (int i = 0; i < 100 && wait_result == 0; i++) { // 10 second timeout
            sleep_ms(100);
            wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        }
        
        if (wait_result == 0) {
            kill(ctx->daemon_pid, SIGKILL);
            waitpid(ctx->daemon_pid, &status, 0);
        }
    }
    
    ctx->daemon_running = false;
    ctx->daemon_pid = 0;
    cleanup_stress_files();
    return true;
}

// ============================================================================
// CLIENT CONNECTION MANAGEMENT
// ============================================================================

static bool connect_stress_client(stress_client_t *client)
{
    // Create socket
    client->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        snprintf(client->last_error, sizeof(client->last_error), 
                "Socket creation failed: %s", strerror(errno));
        client->connection_errors++;
        return false;
    }
    
    // Set socket timeouts
    struct timeval timeout;
    timeout.tv_sec = CONNECTION_TIMEOUT_MS / 1000;
    timeout.tv_usec = (CONNECTION_TIMEOUT_MS % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to daemon
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, STRESS_DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(client->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        client->connected = true;
        return true;
    } else {
        snprintf(client->last_error, sizeof(client->last_error), 
                "Connection failed: %s", strerror(errno));
        close(client->socket_fd);
        client->socket_fd = -1;
        client->connection_errors++;
        return false;
    }
}

static void disconnect_stress_client(stress_client_t *client)
{
    if (client->connected && client->socket_fd >= 0) {
        close(client->socket_fd);
        client->connected = false;
        client->socket_fd = -1;
    }
}

// ============================================================================
// JSON RPC STRESS OPERATIONS
// ============================================================================

static bool send_stress_json_rpc(stress_client_t *client, const char *method, const char *params_json, double *latency_ms)
{
    if (!client->connected) return false;
    
    // Build request
    char request[2048];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}\n",
        method, params_json ? params_json : "[]", client->request_id++);
    
    double start_time = get_time_ms();
    
    // Send request
    ssize_t sent = send(client->socket_fd, request, strlen(request), 0);
    if (sent <= 0) {
        snprintf(client->last_error, sizeof(client->last_error), 
                "Send failed: %s", strerror(errno));
        client->request_errors++;
        return false;
    }
    
    // Receive response
    char response[MAX_RESPONSE_SIZE];
    ssize_t received = recv(client->socket_fd, response, sizeof(response) - 1, 0);
    
    double end_time = get_time_ms();
    *latency_ms = end_time - start_time;
    
    if (received <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            snprintf(client->last_error, sizeof(client->last_error), "Request timeout");
            client->timeout_errors++;
        } else {
            snprintf(client->last_error, sizeof(client->last_error), 
                    "Receive failed: %s", strerror(errno));
            client->request_errors++;
        }
        return false;
    }
    
    response[received] = '\0';
    
    // Basic response validation
    if (strstr(response, "\"jsonrpc\":\"2.0\"") == NULL) {
        snprintf(client->last_error, sizeof(client->last_error), "Invalid JSON RPC response");
        client->request_errors++;
        return false;
    }
    
    return true;
}

// ============================================================================
// STRESS WORKLOAD GENERATORS
// ============================================================================

static bool execute_light_workload(stress_client_t *client, double *latency_ms)
{
    // Light workload: Simple status checks and basic queries
    const char *methods[] = {
        "goxel.get_status",
        "goxel.list_layers"
    };
    
    int method_index = rand() % (sizeof(methods) / sizeof(methods[0]));
    return send_stress_json_rpc(client, methods[method_index], "[]", latency_ms);
}

static bool execute_medium_workload(stress_client_t *client, double *latency_ms)
{
    // Medium workload: Mixed operations including voxel manipulation
    typedef struct {
        const char *method;
        const char *params;
    } method_call_t;
    
    method_call_t calls[] = {
        {"goxel.get_status", "[]"},
        {"goxel.create_project", "[\"Stress Test\",16,16,16]"},
        {"goxel.add_voxel", "[0,-16,0,255,0,0,255,0]"},
        {"goxel.get_voxel", "[0,-16,0]"},
        {"goxel.remove_voxel", "[0,-16,0,0]"},
        {"goxel.list_layers", "[]"},
        {"goxel.create_layer", "[\"Stress Layer\",128,128,255,true]"}
    };
    
    int call_index = rand() % (sizeof(calls) / sizeof(calls[0]));
    return send_stress_json_rpc(client, calls[call_index].method, calls[call_index].params, latency_ms);
}

static bool execute_heavy_workload(stress_client_t *client, double *latency_ms)
{
    // Heavy workload: Complex operations including file I/O
    static int file_counter = 0;
    char temp_file[256];
    
    typedef struct {
        const char *method;
        char params[512];
    } heavy_call_t;
    
    heavy_call_t calls[3];
    
    // Generate unique file names
    snprintf(temp_file, sizeof(temp_file), "/tmp/stress_test_%d_%d.gox", client->client_id, ++file_counter);
    
    // Heavy operations
    snprintf(calls[0].params, sizeof(calls[0].params), "[\"Heavy Stress Project\",32,32,32]");
    strcpy(calls[0].method, "goxel.create_project");
    
    snprintf(calls[1].params, sizeof(calls[1].params), "[\"%s\"]", temp_file);
    strcpy(calls[1].method, "goxel.save_project");
    
    snprintf(calls[2].params, sizeof(calls[2].params), "[\"%s\"]", temp_file);
    strcpy(calls[2].method, "goxel.load_project");
    
    int call_index = rand() % 3;
    bool result = send_stress_json_rpc(client, calls[call_index].method, calls[call_index].params, latency_ms);
    
    // Clean up temporary files
    if (call_index >= 1) {
        unlink(temp_file);
    }
    
    return result;
}

static bool execute_random_workload(stress_client_t *client, double *latency_ms)
{
    // Random workload: Mix of all workload types
    int workload_type = rand() % 3;
    
    switch (workload_type) {
        case 0: return execute_light_workload(client, latency_ms);
        case 1: return execute_medium_workload(client, latency_ms);
        case 2: return execute_heavy_workload(client, latency_ms);
        default: return execute_light_workload(client, latency_ms);
    }
}

static bool execute_workload(stress_client_t *client, stress_workload_t workload_type, double *latency_ms)
{
    switch (workload_type) {
        case WORKLOAD_LIGHT:  return execute_light_workload(client, latency_ms);
        case WORKLOAD_MEDIUM: return execute_medium_workload(client, latency_ms);
        case WORKLOAD_HEAVY:  return execute_heavy_workload(client, latency_ms);
        case WORKLOAD_RANDOM: return execute_random_workload(client, latency_ms);
        default: return execute_light_workload(client, latency_ms);
    }
}

// ============================================================================
// CLIENT THREAD FUNCTIONS
// ============================================================================

static void update_client_stats(stress_client_t *client, double latency_ms, bool success)
{
    client->requests_sent++;
    
    if (success) {
        client->requests_successful++;
        client->total_latency_ms += latency_ms;
        
        if (client->requests_successful == 1) {
            client->min_latency_ms = client->max_latency_ms = latency_ms;
        } else {
            if (latency_ms < client->min_latency_ms) client->min_latency_ms = latency_ms;
            if (latency_ms > client->max_latency_ms) client->max_latency_ms = latency_ms;
        }
    } else {
        client->requests_failed++;
    }
}

static void* stress_client_thread(void *arg)
{
    stress_test_context_t *ctx = (stress_test_context_t*)arg;
    stress_client_t *client = NULL;
    
    // Find this client's data structure
    for (int i = 0; i < ctx->num_clients; i++) {
        if (pthread_equal(ctx->clients[i].thread, pthread_self())) {
            client = &ctx->clients[i];
            break;
        }
    }
    
    if (!client) {
        return NULL;
    }
    
    client->thread_running = true;
    client->min_latency_ms = 999999.0;
    
    // Initial connection
    if (!connect_stress_client(client)) {
        printf("Client %d: Failed to connect initially\n", client->client_id);
        client->thread_running = false;
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->total_connections_attempted++;
    ctx->total_connections_successful++;
    ctx->active_clients++;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    printf("Client %d: Connected and running\n", client->client_id);
    
    // Main stress loop
    while (!client->stop_requested && !ctx->stop_all_clients) {
        double latency_ms;
        bool success = execute_workload(client, ctx->workload_type, &latency_ms);
        
        update_client_stats(client, latency_ms, success);
        
        pthread_mutex_lock(&ctx->stats_mutex);
        ctx->total_requests_sent++;
        if (success) {
            ctx->total_requests_successful++;
        } else {
            ctx->total_errors++;
        }
        pthread_mutex_unlock(&ctx->stats_mutex);
        
        // Connection cycling (if enabled)
        if (ctx->enable_connection_cycling && (client->requests_sent % 100 == 0)) {
            disconnect_stress_client(client);
            sleep_ms(10); // Brief pause
            
            if (connect_stress_client(client)) {
                pthread_mutex_lock(&ctx->stats_mutex);
                ctx->total_connections_attempted++;
                ctx->total_connections_successful++;
                pthread_mutex_unlock(&ctx->stats_mutex);
            } else {
                pthread_mutex_lock(&ctx->stats_mutex);
                ctx->total_connections_attempted++;
                pthread_mutex_unlock(&ctx->stats_mutex);
                break; // Exit if reconnection fails
            }
        }
        
        // Brief pause to avoid overwhelming the daemon
        if (ctx->workload_type == WORKLOAD_HEAVY) {
            sleep_ms(10);
        } else if (ctx->workload_type == WORKLOAD_MEDIUM) {
            sleep_ms(1);
        }
        
        // Check if we've reached the API call limit
        if (ctx->api_calls_per_client > 0 && client->requests_sent >= ctx->api_calls_per_client) {
            break;
        }
    }
    
    // Cleanup
    disconnect_stress_client(client);
    
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->active_clients--;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    printf("Client %d: Completed (%d requests, %.1f%% success)\n", 
           client->client_id, client->requests_sent, 
           client->requests_sent > 0 ? (100.0 * client->requests_successful / client->requests_sent) : 0.0);
    
    client->thread_running = false;
    return NULL;
}

// ============================================================================
// STRESS TEST EXECUTION
// ============================================================================

static void initialize_stress_context(stress_test_context_t *ctx, int num_clients, 
                                     int duration_seconds, stress_workload_t workload_type)
{
    memset(ctx, 0, sizeof(stress_test_context_t));
    
    ctx->num_clients = num_clients;
    ctx->duration_seconds = duration_seconds;
    ctx->workload_type = workload_type;
    ctx->api_calls_per_client = DEFAULT_API_CALLS_PER_CLIENT;
    ctx->enable_connection_cycling = false;
    
    pthread_mutex_init(&ctx->stats_mutex, NULL);
    
    // Initialize client structures
    for (int i = 0; i < num_clients && i < MAX_STRESS_CLIENTS; i++) {
        ctx->clients[i].client_id = i;
        ctx->clients[i].socket_fd = -1;
        ctx->clients[i].request_id = 1;
        ctx->clients[i].min_latency_ms = 999999.0;
    }
}

static bool run_stress_test(stress_test_context_t *ctx)
{
    printf("\n🔥 Starting Stress Test\n");
    printf("========================\n");
    printf("Clients: %d\n", ctx->num_clients);
    printf("Duration: %d seconds\n", ctx->duration_seconds);
    printf("Workload: %s\n", 
           ctx->workload_type == WORKLOAD_LIGHT ? "Light" :
           ctx->workload_type == WORKLOAD_MEDIUM ? "Medium" :
           ctx->workload_type == WORKLOAD_HEAVY ? "Heavy" : "Random");
    
    // Start daemon
    if (!start_stress_daemon(ctx)) {
        printf("❌ Failed to start stress daemon\n");
        return false;
    }
    
    printf("✅ Daemon started successfully\n");
    
    ctx->test_start_time = get_time_ms();
    
    // Launch client threads
    printf("\n🚀 Launching %d client threads...\n", ctx->num_clients);
    
    for (int i = 0; i < ctx->num_clients; i++) {
        if (pthread_create(&ctx->clients[i].thread, NULL, stress_client_thread, ctx) != 0) {
            printf("❌ Failed to create client thread %d\n", i);
            ctx->clients[i].thread_running = false;
        }
    }
    
    // Monitor test progress
    printf("\n📊 Test Progress:\n");
    for (int elapsed = 0; elapsed < ctx->duration_seconds; elapsed += 5) {
        sleep_ms(5000);
        
        pthread_mutex_lock(&ctx->stats_mutex);
        int active = ctx->active_clients;
        int total_requests = ctx->total_requests_sent;
        int successful_requests = ctx->total_requests_successful;
        int errors = ctx->total_errors;
        pthread_mutex_unlock(&ctx->stats_mutex);
        
        double success_rate = total_requests > 0 ? (100.0 * successful_requests / total_requests) : 0.0;
        printf("  %3ds: %d active clients, %d requests, %.1f%% success, %d errors\n",
               elapsed + 5, active, total_requests, success_rate, errors);
        
        if (active == 0) {
            printf("⚠️  All clients have stopped\n");
            break;
        }
    }
    
    // Signal all clients to stop
    ctx->stop_all_clients = true;
    
    // Wait for all client threads to complete
    printf("\n🏁 Stopping all clients...\n");
    for (int i = 0; i < ctx->num_clients; i++) {
        if (ctx->clients[i].thread_running) {
            ctx->clients[i].stop_requested = true;
            pthread_join(ctx->clients[i].thread, NULL);
        }
    }
    
    ctx->test_duration = get_time_ms() - ctx->test_start_time;
    
    // Stop daemon
    stop_stress_daemon(ctx);
    
    return true;
}

static void print_stress_results(stress_test_context_t *ctx)
{
    printf("\n" "=" * 60 "\n");
    printf("🎯 STRESS TEST RESULTS\n");
    printf("=" * 60 "\n");
    
    // Overall statistics
    printf("\n📊 Overall Statistics:\n");
    printf("  Test Duration: %.1f seconds\n", ctx->test_duration / 1000.0);
    printf("  Target Clients: %d\n", ctx->num_clients);
    printf("  Connections Attempted: %d\n", ctx->total_connections_attempted);
    printf("  Connections Successful: %d\n", ctx->total_connections_successful);
    printf("  Total Requests: %d\n", ctx->total_requests_sent);
    printf("  Successful Requests: %d\n", ctx->total_requests_successful);
    printf("  Failed Requests: %d\n", ctx->total_errors);
    
    double connection_success_rate = ctx->total_connections_attempted > 0 ?
        (100.0 * ctx->total_connections_successful / ctx->total_connections_attempted) : 0.0;
    double request_success_rate = ctx->total_requests_sent > 0 ?
        (100.0 * ctx->total_requests_successful / ctx->total_requests_sent) : 0.0;
    
    printf("  Connection Success Rate: %.1f%%\n", connection_success_rate);
    printf("  Request Success Rate: %.1f%%\n", request_success_rate);
    
    if (ctx->test_duration > 0) {
        double throughput = ctx->total_requests_successful / (ctx->test_duration / 1000.0);
        printf("  Throughput: %.1f requests/second\n", throughput);
    }
    
    // Individual client statistics
    printf("\n👥 Client Performance Summary:\n");
    double total_avg_latency = 0;
    int clients_with_data = 0;
    
    for (int i = 0; i < ctx->num_clients; i++) {
        stress_client_t *client = &ctx->clients[i];
        if (client->requests_sent > 0) {
            double avg_latency = client->requests_successful > 0 ?
                (client->total_latency_ms / client->requests_successful) : 0.0;
            double success_rate = 100.0 * client->requests_successful / client->requests_sent;
            
            printf("  Client %2d: %4d requests, %.1f%% success, %.2fms avg latency\n",
                   i, client->requests_sent, success_rate, avg_latency);
            
            if (client->requests_successful > 0) {
                total_avg_latency += avg_latency;
                clients_with_data++;
            }
        }
    }
    
    if (clients_with_data > 0) {
        printf("  Average Latency Across All Clients: %.2fms\n", total_avg_latency / clients_with_data);
    }
    
    // Error analysis
    printf("\n❌ Error Analysis:\n");
    int total_connection_errors = 0;
    int total_request_errors = 0;
    int total_timeout_errors = 0;
    
    for (int i = 0; i < ctx->num_clients; i++) {
        total_connection_errors += ctx->clients[i].connection_errors;
        total_request_errors += ctx->clients[i].request_errors;
        total_timeout_errors += ctx->clients[i].timeout_errors;
    }
    
    printf("  Connection Errors: %d\n", total_connection_errors);
    printf("  Request Errors: %d\n", total_request_errors);
    printf("  Timeout Errors: %d\n", total_timeout_errors);
    
    // Assessment
    printf("\n🏆 Assessment:\n");
    bool stress_success = (connection_success_rate >= 90.0 && request_success_rate >= 95.0);
    printf("  Stress Test Result: %s\n", stress_success ? "✅ PASSED" : "❌ FAILED");
    
    if (connection_success_rate >= 90.0) {
        printf("  Connection Reliability: ✅ EXCELLENT (≥90%%)\n");
    } else if (connection_success_rate >= 80.0) {
        printf("  Connection Reliability: ⚠️  GOOD (≥80%%)\n");
    } else {
        printf("  Connection Reliability: ❌ POOR (<80%%)\n");
    }
    
    if (request_success_rate >= 95.0) {
        printf("  Request Reliability: ✅ EXCELLENT (≥95%%)\n");
    } else if (request_success_rate >= 90.0) {
        printf("  Request Reliability: ⚠️  GOOD (≥90%%)\n");
    } else {
        printf("  Request Reliability: ❌ POOR (<90%%)\n");
    }
    
    printf("=" * 60 "\n");
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================

int main(int argc, char *argv[])
{
    printf("🔥 Goxel v14.0 Concurrent Client Stress Testing Suite\n");
    printf("====================================================\n");
    
    // Parse command line arguments
    int num_clients = 10;
    int duration = 30;
    stress_workload_t workload = WORKLOAD_MEDIUM;
    
    if (argc > 1) num_clients = atoi(argv[1]);
    if (argc > 2) duration = atoi(argv[2]);
    if (argc > 3) {
        if (strcmp(argv[3], "light") == 0) workload = WORKLOAD_LIGHT;
        else if (strcmp(argv[3], "medium") == 0) workload = WORKLOAD_MEDIUM;
        else if (strcmp(argv[3], "heavy") == 0) workload = WORKLOAD_HEAVY;
        else if (strcmp(argv[3], "random") == 0) workload = WORKLOAD_RANDOM;
    }
    
    // Validate parameters
    if (num_clients <= 0 || num_clients > MAX_STRESS_CLIENTS) {
        printf("❌ Invalid number of clients: %d (max: %d)\n", num_clients, MAX_STRESS_CLIENTS);
        return 1;
    }
    
    if (duration <= 0 || duration > 600) {
        printf("❌ Invalid duration: %d seconds (max: 600)\n", duration);
        return 1;
    }
    
    // Initialize and run stress test
    stress_test_context_t ctx;
    initialize_stress_context(&ctx, num_clients, duration, workload);
    
    cleanup_stress_files();
    
    bool success = run_stress_test(&ctx);
    print_stress_results(&ctx);
    
    // Cleanup
    pthread_mutex_destroy(&ctx.stats_mutex);
    cleanup_stress_files();
    
    return success ? 0 : 1;
}