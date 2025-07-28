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

#include "daemon_lifecycle.h"
#include "worker_pool.h"
#include "request_queue.h"
#include "json_rpc.h"
#include "socket_server.h"
#include "json_socket_handler.h"
#include "../core/goxel_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

// Forward declarations
static int64_t get_current_time_us(void);

// Logging macros
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) fprintf(stderr, "[WARNING] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

// ============================================================================
// DAEMON MAIN CONFIGURATION
// ============================================================================

#define PROGRAM_NAME "goxel-daemon"
#define VERSION "14.0.0-daemon"
#define DEFAULT_SOCKET_PATH "/tmp/goxel-daemon.sock"
#define DEFAULT_PID_PATH "/tmp/goxel-daemon.pid"
#define DEFAULT_LOG_PATH "/tmp/goxel-daemon.log"

// ============================================================================
// COMMAND LINE OPTIONS
// ============================================================================

static const char *short_options = "hvVDfp:s:c:l:w:u:g:j:q:m:";

static const struct option long_options[] = {
    {"help",          no_argument,       NULL, 'h'},
    {"version",       no_argument,       NULL, 'v'},
    {"verbose",       no_argument,       NULL, 'V'},
    {"daemonize",     no_argument,       NULL, 'D'},
    {"foreground",    no_argument,       NULL, 'f'},
    {"pid-file",      required_argument, NULL, 'p'},
    {"socket",        required_argument, NULL, 's'},
    {"config",        required_argument, NULL, 'c'},
    {"log-file",      required_argument, NULL, 'l'},
    {"working-dir",   required_argument, NULL, 'w'},
    {"user",          required_argument, NULL, 'u'},
    {"group",         required_argument, NULL, 'g'},
    {"workers",       required_argument, NULL, 'j'},
    {"queue-size",    required_argument, NULL, 'q'},
    {"max-connections", required_argument, NULL, 'm'},
    {"priority-queue", no_argument,      NULL, 1005},
    {"test-signals",  no_argument,       NULL, 1000},
    {"test-lifecycle", no_argument,      NULL, 1001},
    {"test-concurrent", no_argument,     NULL, 1006},
    {"status",        no_argument,       NULL, 1002},
    {"stop",          no_argument,       NULL, 1003},
    {"reload",        no_argument,       NULL, 1004},
    {NULL, 0, NULL, 0}
};

// ============================================================================
// PROGRAM CONFIGURATION
// ============================================================================

typedef struct {
    bool help;
    bool version;
    bool verbose;
    bool daemonize;
    bool foreground;
    bool test_signals;
    bool test_lifecycle;
    bool status;
    bool stop;
    bool reload;
    const char *pid_file;
    const char *socket_path;
    const char *config_file;
    const char *log_file;
    const char *working_dir;
    const char *user;
    const char *group;
    // Concurrent processing options
    int worker_threads;
    int queue_size;
    bool enable_priority_queue;
    int max_connections;
} program_config_t;

/**
 * Concurrent daemon context structure.
 */
typedef struct {
    // Core components
    socket_server_t *socket_server;
    worker_pool_t *worker_pool;
    request_queue_t *request_queue;
    
    // Goxel core instances (one per worker for thread safety)
    void **goxel_contexts;
    int num_contexts;
    
    // Configuration
    program_config_t *config;
    
    // State management
    bool running;
    pthread_mutex_t state_mutex;
    
    // Statistics
    struct {
        uint64_t requests_processed;
        uint64_t requests_failed;
        uint64_t concurrent_connections;
        int64_t start_time_us;
    } stats;
} concurrent_daemon_t;

// ============================================================================
// HELP AND VERSION FUNCTIONS
// ============================================================================

static void print_version(void)
{
    printf("%s version %s\n", PROGRAM_NAME, VERSION);
    printf("Goxel v14.0 Daemon Architecture - Process Lifecycle Management\n");
    printf("Copyright (c) 2025 Guillaume Chereau\n");
    printf("Licensed under GNU General Public License v3.0\n");
}

static void print_help(void)
{
    print_version();
    printf("\n");
    printf("Usage: %s [OPTIONS]\n", PROGRAM_NAME);
    printf("\n");
    printf("Goxel daemon for headless 3D voxel editing operations.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message and exit\n");
    printf("  -v, --version           Show version information and exit\n");
    printf("  -V, --verbose           Enable verbose output\n");
    printf("  -D, --daemonize         Run as daemon (background process)\n");
    printf("  -f, --foreground        Run in foreground (default)\n");
    printf("  -p, --pid-file PATH     PID file path (default: %s)\n", DEFAULT_PID_PATH);
    printf("  -s, --socket PATH       Unix socket path (default: %s)\n", DEFAULT_SOCKET_PATH);
    printf("  -c, --config FILE       Configuration file path\n");
    printf("  -l, --log-file PATH     Log file path (default: %s)\n", DEFAULT_LOG_PATH);
    printf("  -w, --working-dir DIR   Working directory (default: /)\n");
    printf("  -u, --user USER         Run as specified user\n");
    printf("  -g, --group GROUP       Run as specified group\n");
    printf("\n");
    printf("Concurrent Processing Options:\n");
    printf("  -j, --workers NUM       Number of worker threads (default: 4)\n");
    printf("  -q, --queue-size NUM    Request queue size (default: 1024)\n");
    printf("  -m, --max-connections NUM Maximum concurrent connections (default: 256)\n");
    printf("      --priority-queue    Enable priority-based request processing\n");
    printf("\n");
    printf("Control Commands:\n");
    printf("      --status            Show daemon status\n");
    printf("      --stop              Stop running daemon\n");
    printf("      --reload            Reload daemon configuration\n");
    printf("\n");
    printf("Testing Commands:\n");
    printf("      --test-signals      Test signal handling functionality\n");
    printf("      --test-lifecycle    Test daemon lifecycle management\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --daemonize                    # Start daemon in background\n", PROGRAM_NAME);
    printf("  %s --foreground --verbose         # Start in foreground with verbose output\n", PROGRAM_NAME);
    printf("  %s --status                       # Check daemon status\n", PROGRAM_NAME);
    printf("  %s --stop                         # Stop running daemon\n", PROGRAM_NAME);
    printf("  %s --test-lifecycle               # Test daemon functionality\n", PROGRAM_NAME);
    printf("\n");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

__attribute__((unused))
static uid_t get_user_id(const char *username)
{
    if (!username) return 0;
    
    // For simplicity, just try to parse as numeric UID
    char *endptr;
    long uid = strtol(username, &endptr, 10);
    if (*endptr == '\0' && uid > 0) {
        return (uid_t)uid;
    }
    
    // In a full implementation, would use getpwnam()
    return 0;
}

__attribute__((unused))
static gid_t get_group_id(const char *groupname)
{
    if (!groupname) return 0;
    
    // For simplicity, just try to parse as numeric GID
    char *endptr;
    long gid = strtol(groupname, &endptr, 10);
    if (*endptr == '\0' && gid > 0) {
        return (gid_t)gid;
    }
    
    // In a full implementation, would use getgrnam()
    return 0;
}

// ============================================================================
// DAEMON CONTROL FUNCTIONS
// ============================================================================

static int daemon_status_command(const char *pid_file)
{
    printf("Checking daemon status...\n");
    
    pid_t pid;
    daemon_error_t result = daemon_read_pid_file(pid_file, &pid);
    
    if (result != DAEMON_SUCCESS) {
        printf("Daemon is not running (no PID file found)\n");
        return 1;
    }
    
    if (daemon_is_process_running(pid)) {
        printf("Daemon is running (PID: %d)\n", pid);
        return 0;
    } else {
        printf("Daemon is not running (stale PID file: %d)\n", pid);
        daemon_remove_pid_file(pid_file);
        return 1;
    }
}

static int daemon_stop_command(const char *pid_file)
{
    printf("Stopping daemon...\n");
    
    pid_t pid;
    daemon_error_t result = daemon_read_pid_file(pid_file, &pid);
    
    if (result != DAEMON_SUCCESS) {
        printf("Daemon is not running (no PID file found)\n");
        return 1;
    }
    
    if (!daemon_is_process_running(pid)) {
        printf("Daemon is not running (stale PID file: %d)\n", pid);
        daemon_remove_pid_file(pid_file);
        return 1;
    }
    
    // Send SIGTERM for graceful shutdown
    printf("Sending SIGTERM to daemon (PID: %d)...\n", pid);
    result = daemon_send_shutdown_signal(pid);
    
    if (result != DAEMON_SUCCESS) {
        printf("Failed to send shutdown signal: %s\n", daemon_error_string(result));
        return 1;
    }
    
    // Wait for daemon to stop
    printf("Waiting for daemon to stop...\n");
    int timeout = 30; // 30 seconds
    while (timeout > 0 && daemon_is_process_running(pid)) {
        sleep(1);
        timeout--;
    }
    
    if (daemon_is_process_running(pid)) {
        printf("Daemon did not stop gracefully, sending SIGKILL...\n");
        daemon_send_kill_signal(pid);
        sleep(2);
        
        if (daemon_is_process_running(pid)) {
            printf("Failed to stop daemon\n");
            return 1;
        }
    }
    
    printf("Daemon stopped successfully\n");
    daemon_remove_pid_file(pid_file);
    return 0;
}

static int daemon_reload_command(const char *pid_file)
{
    printf("Reloading daemon configuration...\n");
    
    pid_t pid;
    daemon_error_t result = daemon_read_pid_file(pid_file, &pid);
    
    if (result != DAEMON_SUCCESS) {
        printf("Daemon is not running (no PID file found)\n");
        return 1;
    }
    
    if (!daemon_is_process_running(pid)) {
        printf("Daemon is not running (stale PID file: %d)\n", pid);
        daemon_remove_pid_file(pid_file);
        return 1;
    }
    
    // Send SIGHUP for configuration reload
    printf("Sending SIGHUP to daemon (PID: %d)...\n", pid);
    result = daemon_send_reload_signal(pid);
    
    if (result != DAEMON_SUCCESS) {
        printf("Failed to send reload signal: %s\n", daemon_error_string(result));
        return 1;
    }
    
    printf("Configuration reload signal sent successfully\n");
    return 0;
}

// ============================================================================
// TESTING FUNCTIONS
// ============================================================================

static int test_signal_handling(daemon_context_t *ctx)
{
    printf("Testing signal handling...\n");
    
    daemon_error_t result;
    
    // Test SIGHUP handling
    printf("  Testing SIGHUP (reload signal)...\n");
    result = daemon_test_signal_handling(ctx, SIGHUP);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        return 1;
    }
    printf("  OK: SIGHUP handled correctly\n");
    
    // Test SIGTERM handling
    printf("  Testing SIGTERM (shutdown signal)...\n");
    result = daemon_test_signal_handling(ctx, SIGTERM);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        return 1;
    }
    printf("  OK: SIGTERM handled correctly\n");
    
    // Test SIGINT handling
    printf("  Testing SIGINT (interrupt signal)...\n");
    result = daemon_test_signal_handling(ctx, SIGINT);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        return 1;
    }
    printf("  OK: SIGINT handled correctly\n");
    
    printf("Signal handling tests completed successfully\n");
    return 0;
}

static int test_lifecycle_management(void)
{
    printf("Testing daemon lifecycle management...\n");
    
    // Create test configuration
    daemon_config_t config = daemon_default_config();
    config.pid_file_path = "/tmp/test-goxel-daemon.pid";
    config.socket_path = "/tmp/test-goxel-daemon.sock";
    config.daemonize = false; // Don't fork for testing
    
    printf("  Creating daemon context...\n");
    daemon_context_t *ctx = daemon_context_create(&config);
    if (!ctx) {
        printf("  FAILED: Could not create daemon context\n");
        return 1;
    }
    printf("  OK: Daemon context created\n");
    
    printf("  Initializing daemon...\n");
    daemon_error_t result = daemon_initialize(ctx, NULL);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        daemon_context_destroy(ctx);
        return 1;
    }
    printf("  OK: Daemon initialized\n");
    
    printf("  Starting daemon...\n");
    result = daemon_start(ctx);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        daemon_context_destroy(ctx);
        return 1;
    }
    printf("  OK: Daemon started\n");
    
    // Test state management
    printf("  Testing state management...\n");
    if (!daemon_is_running(ctx)) {
        printf("  FAILED: Daemon should be running\n");
        daemon_shutdown(ctx);
        daemon_context_destroy(ctx);
        return 1;
    }
    printf("  OK: Daemon state is correct\n");
    
    // Test statistics
    printf("  Testing statistics...\n");
    daemon_stats_t stats;
    result = daemon_get_stats(ctx, &stats);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: Could not get daemon statistics\n");
        daemon_shutdown(ctx);
        daemon_context_destroy(ctx);
        return 1;
    }
    printf("  OK: Statistics retrieved (PID: %d, State: %d)\n", stats.pid, stats.state);
    
    // Test signal handling
    if (test_signal_handling(ctx) != 0) {
        daemon_shutdown(ctx);
        daemon_context_destroy(ctx);
        return 1;
    }
    
    printf("  Shutting down daemon...\n");
    result = daemon_shutdown(ctx);
    if (result != DAEMON_SUCCESS) {
        printf("  FAILED: %s\n", daemon_error_string(result));
        daemon_context_destroy(ctx);
        return 1;
    }
    printf("  OK: Daemon shut down\n");
    
    printf("  Destroying daemon context...\n");
    daemon_context_destroy(ctx);
    printf("  OK: Daemon context destroyed\n");
    
    // Clean up test files
    daemon_remove_pid_file(config.pid_file_path);
    unlink(config.socket_path);
    
    printf("Lifecycle management tests completed successfully\n");
    return 0;
}

// ============================================================================
// COMMAND LINE PARSING
// ============================================================================

static int parse_command_line(int argc, char *argv[], program_config_t *config)
{
    // Initialize with defaults
    memset(config, 0, sizeof(program_config_t));
    config->pid_file = DEFAULT_PID_PATH;
    config->socket_path = DEFAULT_SOCKET_PATH;
    config->log_file = DEFAULT_LOG_PATH;
    config->working_dir = "/";
    // Concurrent processing defaults
    config->worker_threads = 4;
    config->queue_size = 1024;
    config->enable_priority_queue = false;
    config->max_connections = 256;
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                config->help = true;
                break;
            case 'v':
                config->version = true;
                break;
            case 'V':
                config->verbose = true;
                break;
            case 'D':
                config->daemonize = true;
                config->foreground = false;
                break;
            case 'f':
                config->foreground = true;
                config->daemonize = false;
                break;
            case 'p':
                config->pid_file = optarg;
                break;
            case 's':
                config->socket_path = optarg;
                break;
            case 'c':
                config->config_file = optarg;
                break;
            case 'l':
                config->log_file = optarg;
                break;
            case 'w':
                config->working_dir = optarg;
                break;
            case 'u':
                config->user = optarg;
                break;
            case 'g':
                config->group = optarg;
                break;
            case 'j':
                config->worker_threads = atoi(optarg);
                if (config->worker_threads <= 0 || config->worker_threads > 64) {
                    fprintf(stderr, "Invalid number of worker threads: %s (must be 1-64)\n", optarg);
                    return -1;
                }
                break;
            case 'q':
                config->queue_size = atoi(optarg);
                if (config->queue_size <= 0 || config->queue_size > 65536) {
                    fprintf(stderr, "Invalid queue size: %s (must be 1-65536)\n", optarg);
                    return -1;
                }
                break;
            case 'm':
                config->max_connections = atoi(optarg);
                if (config->max_connections <= 0 || config->max_connections > 65536) {
                    fprintf(stderr, "Invalid max connections: %s (must be 1-65536)\n", optarg);
                    return -1;
                }
                break;
            case 1000:
                config->test_signals = true;
                break;
            case 1001:
                config->test_lifecycle = true;
                break;
            case 1002:
                config->status = true;
                break;
            case 1003:
                config->stop = true;
                break;
            case 1004:
                config->reload = true;
                break;
            case 1005:
                config->enable_priority_queue = true;
                break;
            case 1006:
                // Test concurrent processing (to be implemented)
                printf("Concurrent processing test not yet implemented\n");
                return -1;
            case '?':
                return -1;
            default:
                return -1;
        }
    }
    
    return 0;
}

// ============================================================================
// CONCURRENT PROCESSING IMPLEMENTATION
// ============================================================================

/**
 * Request processing data structure for worker threads.
 */
typedef struct {
    socket_client_t *client;
    json_rpc_request_t *rpc_request;
    socket_server_t *socket_server;
    void *goxel_context;
    uint32_t request_id;
} request_process_data_t;

/**
 * Worker thread processing function.
 * Processes JSON-RPC requests using Goxel core functionality.
 */
static int process_rpc_request(void *request_data, int worker_id, void *context)
{
    concurrent_daemon_t *daemon = (concurrent_daemon_t*)context;
    request_process_data_t *data = (request_process_data_t*)request_data;
    
    if (!data || !data->rpc_request || !data->client) {
        return -1;
    }
    
    // Use worker-specific Goxel context for thread safety
    void *goxel_ctx = (worker_id < daemon->num_contexts) ? 
                      daemon->goxel_contexts[worker_id] : NULL;
    (void)goxel_ctx; // TODO: Use this when implementing method execution
    
    // Process the JSON-RPC request
    json_rpc_response_t *response = NULL;
    
    // Call the actual JSON-RPC method handler
    response = json_rpc_handle_method(data->rpc_request);
    
    json_rpc_result_t result = response ? JSON_RPC_SUCCESS : JSON_RPC_ERROR_UNKNOWN;
    
    // Send response back to client
    if (response) {
        // Serialize response to JSON
        char *response_json = NULL;
        size_t response_len = 0; (void)response_len; // Silence unused warning
        json_rpc_result_t serialize_result = json_rpc_serialize_response(response, 
                                                                        &response_json);
        if (serialize_result == JSON_RPC_SUCCESS) {
            response_len = strlen(response_json);
        }
        
        if (serialize_result == JSON_RPC_SUCCESS && response_json) {
            // Create socket message
            socket_message_t *message = socket_message_create_json(data->request_id, 
                                                                  0, response_json);
            if (message) {
                socket_server_send_message(data->socket_server, data->client, message);
                socket_message_destroy(message);
            }
            free(response_json);
        }
        
        json_rpc_free_response(response);
    }
    
    // Update statistics
    pthread_mutex_lock(&daemon->state_mutex);
    if (result == JSON_RPC_SUCCESS) {
        daemon->stats.requests_processed++;
    } else {
        daemon->stats.requests_failed++;
    }
    pthread_mutex_unlock(&daemon->state_mutex);
    
    return (result == JSON_RPC_SUCCESS) ? 0 : -1;
}

/**
 * Cleanup function for request processing data.
 */
static void cleanup_request_data(void *request_data)
{
    request_process_data_t *data = (request_process_data_t*)request_data;
    if (data) {
        if (data->rpc_request) {
            json_rpc_free_request(data->rpc_request);
        }
        free(data);
    }
}

/**
 * Socket server message handler.
 * Receives messages from clients and queues them for processing.
 */
static socket_message_t *handle_socket_message(socket_server_t *server,
                                              socket_client_t *client,
                                              const socket_message_t *message,
                                              void *user_data)
{
    concurrent_daemon_t *daemon = (concurrent_daemon_t*)user_data;
    
    if (!daemon || !daemon->running || !message || !message->data) {
        return NULL;
    }
    
    // Parse JSON-RPC request
    json_rpc_request_t *rpc_request = NULL;
    // Null-terminate the message for parsing
    char *json_str = malloc(message->length + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for JSON string");
        return NULL;
    }
    memcpy(json_str, message->data, message->length);
    json_str[message->length] = '\0';
    
    json_rpc_result_t parse_result = json_rpc_parse_request(json_str, 
                                                           &rpc_request);
    free(json_str);
    
    if (parse_result != JSON_RPC_SUCCESS || !rpc_request) {
        // Send error response for invalid requests
        json_rpc_response_t error_response = {0};
        json_rpc_create_id_null(&error_response.id);
        error_response.result = NULL;
        error_response.error.code = JSON_RPC_PARSE_ERROR;
        error_response.error.message = "Invalid JSON-RPC request";
        error_response.error.data = NULL;
        
        char *error_json = NULL;
        size_t error_len = 0; (void)error_len; // Silence unused warning
        if (json_rpc_serialize_response(&error_response, &error_json) == JSON_RPC_SUCCESS) {
            error_len = strlen(error_json);
            socket_message_t *error_msg = socket_message_create_json(message->id, 0, error_json);
            free(error_json);
            return error_msg;
        }
        return NULL;
    }
    
    // Create request processing data
    request_process_data_t *process_data = calloc(1, sizeof(request_process_data_t));
    if (!process_data) {
        json_rpc_free_request(rpc_request);
        return NULL;
    }
    
    process_data->client = client;
    process_data->rpc_request = rpc_request;
    process_data->socket_server = server;
    process_data->request_id = message->id;
    
    // Submit to worker pool
    worker_pool_error_t submit_result = worker_pool_submit_request(daemon->worker_pool,
                                                                  process_data,
                                                                  WORKER_PRIORITY_NORMAL);
    
    if (submit_result != WORKER_POOL_SUCCESS) {
        cleanup_request_data(process_data);
        
        // Send error response for queue full
        json_rpc_response_t error_response = {0};
        json_rpc_clone_id(&rpc_request->id, &error_response.id);
        error_response.result = NULL;
        error_response.error.code = JSON_RPC_INTERNAL_ERROR;
        error_response.error.message = "Server overloaded - request queue full";
        error_response.error.data = NULL;
        
        char *error_json = NULL;
        size_t error_len = 0; (void)error_len; // Silence unused warning
        if (json_rpc_serialize_response(&error_response, &error_json) == JSON_RPC_SUCCESS) {
            error_len = strlen(error_json);
            socket_message_t *error_msg = socket_message_create_json(message->id, 0, error_json);
            free(error_json);
            json_rpc_free_id(&error_response.id);
            return error_msg;
        }
        json_rpc_free_id(&error_response.id);
    }
    
    // Return NULL since we process asynchronously
    return NULL;
}

/**
 * Create and initialize concurrent daemon.
 */
static concurrent_daemon_t *create_concurrent_daemon(const program_config_t *config)
{
    concurrent_daemon_t *daemon = calloc(1, sizeof(concurrent_daemon_t));
    if (!daemon) return NULL;
    
    daemon->config = (program_config_t*)config;
    daemon->running = false;
    daemon->num_contexts = config->worker_threads;
    
    // Initialize state mutex
    if (pthread_mutex_init(&daemon->state_mutex, NULL) != 0) {
        free(daemon);
        return NULL;
    }
    
    // Create Goxel contexts for each worker
    daemon->goxel_contexts = calloc(daemon->num_contexts, sizeof(void*));
    if (!daemon->goxel_contexts) {
        pthread_mutex_destroy(&daemon->state_mutex);
        free(daemon);
        return NULL;
    }
    
    // Initialize Goxel contexts - one per worker thread
    // For now, we'll initialize the first context only and share it
    // TODO: Create separate contexts per worker for true thread isolation
    json_rpc_result_t init_result = json_rpc_init_goxel_context();
    if (init_result != JSON_RPC_SUCCESS) {
        LOG_ERROR("Failed to initialize Goxel context: %s", json_rpc_result_string(init_result));
        pthread_mutex_destroy(&daemon->state_mutex);
        free(daemon->goxel_contexts);
        free(daemon);
        return NULL;
    }
    
    for (int i = 0; i < daemon->num_contexts; i++) {
        // For now, all workers share the same context
        // TODO: Implement per-worker contexts with proper synchronization
        daemon->goxel_contexts[i] = (void*)(intptr_t)(1); // Placeholder - actual context managed by json_rpc
    }
    
    // Set up JSON socket handler
    json_socket_set_handler(handle_socket_message, daemon);
    
    // Create socket server
    socket_server_config_t server_config = socket_server_default_config();
    server_config.socket_path = config->socket_path;
    server_config.max_connections = config->max_connections;
    server_config.thread_per_client = false;
    server_config.thread_pool_size = config->worker_threads;
    server_config.msg_handler = handle_socket_message;
    server_config.client_handler = json_socket_client_handler;  // Use JSON client handler
    server_config.user_data = daemon;
    
    daemon->socket_server = socket_server_create(&server_config);
    if (!daemon->socket_server) {
        for (int i = 0; i < daemon->num_contexts; i++) {
            // goxel_core_destroy_context(daemon->goxel_contexts[i]);
        }
        free(daemon->goxel_contexts);
        pthread_mutex_destroy(&daemon->state_mutex);
        free(daemon);
        return NULL;
    }
    
    // Create worker pool
    worker_pool_config_t pool_config = worker_pool_default_config();
    pool_config.worker_count = config->worker_threads;
    pool_config.queue_capacity = config->queue_size;
    pool_config.enable_priority_queue = config->enable_priority_queue;
    pool_config.process_func = process_rpc_request;
    pool_config.cleanup_func = cleanup_request_data;
    pool_config.context = daemon;
    
    daemon->worker_pool = worker_pool_create(&pool_config);
    if (!daemon->worker_pool) {
        socket_server_destroy(daemon->socket_server);
        for (int i = 0; i < daemon->num_contexts; i++) {
            // goxel_core_destroy_context(daemon->goxel_contexts[i]);
        }
        free(daemon->goxel_contexts);
        pthread_mutex_destroy(&daemon->state_mutex);
        free(daemon);
        return NULL;
    }
    
    // Create request queue
    request_queue_config_t queue_config = request_queue_default_config();
    queue_config.max_size = config->queue_size;
    queue_config.enable_priority_queue = config->enable_priority_queue;
    
    daemon->request_queue = request_queue_create(&queue_config);
    if (!daemon->request_queue) {
        worker_pool_destroy(daemon->worker_pool);
        socket_server_destroy(daemon->socket_server);
        for (int i = 0; i < daemon->num_contexts; i++) {
            // goxel_core_destroy_context(daemon->goxel_contexts[i]);
        }
        free(daemon->goxel_contexts);
        pthread_mutex_destroy(&daemon->state_mutex);
        free(daemon);
        return NULL;
    }
    
    return daemon;
}

/**
 * Destroy concurrent daemon and cleanup resources.
 */
static void destroy_concurrent_daemon(concurrent_daemon_t *daemon)
{
    if (!daemon) return;
    
    daemon->running = false;
    
    // Stop components
    if (daemon->worker_pool) {
        worker_pool_stop(daemon->worker_pool);
        worker_pool_destroy(daemon->worker_pool);
    }
    
    if (daemon->socket_server) {
        socket_server_stop(daemon->socket_server);
        socket_server_destroy(daemon->socket_server);
    }
    
    if (daemon->request_queue) {
        request_queue_destroy(daemon->request_queue);
    }
    
    // Cleanup Goxel contexts
    if (daemon->goxel_contexts) {
        // Cleanup the shared Goxel context
        json_rpc_cleanup_goxel_context();
        free(daemon->goxel_contexts);
    }
    
    pthread_mutex_destroy(&daemon->state_mutex);
    free(daemon);
}

// ============================================================================
// MAIN DAEMON FUNCTION
// ============================================================================

static int run_daemon(const program_config_t *prog_config)
{
    if (prog_config->verbose) {
        printf("Starting Goxel daemon with concurrent processing:\n");
        printf("  PID file: %s\n", prog_config->pid_file);
        printf("  Socket: %s\n", prog_config->socket_path);
        printf("  Log file: %s\n", prog_config->log_file);
        printf("  Working directory: %s\n", prog_config->working_dir);
        printf("  Daemonize: %s\n", prog_config->daemonize ? "yes" : "no");
        printf("  Worker threads: %d\n", prog_config->worker_threads);
        printf("  Queue size: %d\n", prog_config->queue_size);
        printf("  Max connections: %d\n", prog_config->max_connections);
        printf("  Priority queue: %s\n", prog_config->enable_priority_queue ? "yes" : "no");
    }
    
    // Create concurrent daemon
    concurrent_daemon_t *daemon = create_concurrent_daemon(prog_config);
    if (!daemon) {
        fprintf(stderr, "Failed to create concurrent daemon\n");
        return 1;
    }
    
    // Start worker pool
    worker_pool_error_t pool_result = worker_pool_start(daemon->worker_pool);
    if (pool_result != WORKER_POOL_SUCCESS) {
        fprintf(stderr, "Failed to start worker pool: %s\n", 
                worker_pool_error_string(pool_result));
        destroy_concurrent_daemon(daemon);
        return 1;
    }
    
    // Start socket server
    socket_error_t server_result = socket_server_start(daemon->socket_server);
    if (server_result != SOCKET_SUCCESS) {
        fprintf(stderr, "Failed to start socket server: %s\n", 
                socket_error_string(server_result));
        destroy_concurrent_daemon(daemon);
        return 1;
    }
    
    // Set daemon as running
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->running = true;
    daemon->stats.start_time_us = get_current_time_us();
    pthread_mutex_unlock(&daemon->state_mutex);
    
    if (prog_config->verbose && !prog_config->daemonize) {
        printf("Concurrent daemon started successfully (PID: %d)\n", getpid());
        printf("  Socket server listening on: %s\n", prog_config->socket_path);
        printf("  Worker pool with %d threads ready\n", prog_config->worker_threads);
        printf("Press Ctrl+C to stop the daemon\n");
    }
    
    // Main daemon loop - wait for shutdown signal
    while (daemon->running) {
        sleep(1);
        
        // Handle timeouts and cleanup
        request_queue_handle_timeouts(daemon->request_queue);
        
        // Check if we should continue running
        if (!socket_server_is_running(daemon->socket_server) ||
            !worker_pool_is_running(daemon->worker_pool)) {
            break;
        }
    }
    
    if (prog_config->verbose && !prog_config->daemonize) {
        printf("Daemon shutting down...\n");
        
        // Print final statistics
        worker_stats_t worker_stats;
        if (worker_pool_get_stats(daemon->worker_pool, &worker_stats) == WORKER_POOL_SUCCESS) {
            printf("Final statistics:\n");
            printf("  Requests processed: %llu\n", (unsigned long long)worker_stats.requests_processed);
            printf("  Requests failed: %llu\n", (unsigned long long)worker_stats.requests_failed);
            printf("  Average processing time: %llu μs\n", (unsigned long long)worker_stats.average_processing_time_us);
        }
        
        socket_server_stats_t server_stats;
        if (socket_server_get_stats(daemon->socket_server, &server_stats) == SOCKET_SUCCESS) {
            printf("  Total connections: %llu\n", (unsigned long long)server_stats.total_connections);
            printf("  Messages received: %llu\n", (unsigned long long)server_stats.messages_received);
            printf("  Messages sent: %llu\n", (unsigned long long)server_stats.messages_sent);
        }
    }
    
    // Cleanup daemon
    destroy_concurrent_daemon(daemon);
    
    if (prog_config->verbose && !prog_config->daemonize) {
        printf("Daemon stopped\n");
    }
    
    return 0;
}

static int64_t get_current_time_us(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char *argv[])
{
    program_config_t config;
    
    // Parse command line arguments
    if (parse_command_line(argc, argv, &config) != 0) {
        fprintf(stderr, "Invalid command line arguments. Use --help for usage information.\n");
        return 1;
    }
    
    // Handle help and version requests
    if (config.help) {
        print_help();
        return 0;
    }
    
    if (config.version) {
        print_version();
        return 0;
    }
    
    // Handle control commands
    if (config.status) {
        return daemon_status_command(config.pid_file);
    }
    
    if (config.stop) {
        return daemon_stop_command(config.pid_file);
    }
    
    if (config.reload) {
        return daemon_reload_command(config.pid_file);
    }
    
    // Handle testing commands
    if (config.test_lifecycle) {
        return test_lifecycle_management();
    }
    
    if (config.test_signals) {
        // For signal testing, we need a running daemon context
        daemon_config_t daemon_config = daemon_default_config();
        daemon_config.daemonize = false;
        daemon_context_t *ctx = daemon_context_create(&daemon_config);
        if (!ctx) {
            fprintf(stderr, "Failed to create daemon context for testing\n");
            return 1;
        }
        
        daemon_error_t result = daemon_initialize(ctx, NULL);
        if (result != DAEMON_SUCCESS) {
            fprintf(stderr, "Failed to initialize daemon for testing: %s\n", daemon_error_string(result));
            daemon_context_destroy(ctx);
            return 1;
        }
        
        int test_result = test_signal_handling(ctx);
        daemon_context_destroy(ctx);
        return test_result;
    }
    
    // Run the daemon
    return run_daemon(&config);
}