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
#include "mcp_handler.h"
#include "test_methods.h"
#include "project_mutex.h"
#include "../core/goxel_core.h"
#include "../goxel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

// Global goxel instance required by core functions
extern goxel_t goxel;

// Forward declarations
static int64_t get_current_time_us(void);
static const char *get_persistent_socket_path(const char *requested_path);

// Logging macros
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) fprintf(stderr, "[WARNING] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

// ============================================================================
// DAEMON MAIN CONFIGURATION
// ============================================================================

#define PROGRAM_NAME "goxel-daemon"
#define VERSION "0.17.2"
#define DEFAULT_SOCKET_PATH "/tmp/goxel-daemon.sock"
#define DEFAULT_PID_PATH "/tmp/goxel-daemon.pid"
#define DEFAULT_LOG_PATH "/tmp/goxel-daemon.log"

// ============================================================================
// COMMAND LINE OPTIONS
// ============================================================================

static const char *short_options = "hvVDfp:s:c:l:w:u:g:j:q:m:P:";

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
    {"protocol",       required_argument, NULL, 'P'},
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
// PROTOCOL DEFINITIONS
// ============================================================================

// Protocol modes - these are defined in socket_server.h
// Additional protocol modes beyond binary/JSON-RPC
#define PROTOCOL_AUTO 0        /**< Auto-detect protocol */
#define PROTOCOL_MCP 2         /**< MCP only */

// Magic bytes for protocol detection (4 bytes)
#define JSONRPC_MAGIC 0x7B227661  // {"version or {"method or similar JSON-RPC start
#define MCP_MAGIC     0x7B22746F  // {"tool or similar MCP start
#define MAGIC_DETECT_SIZE 4

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
    // Protocol configuration
    protocol_mode_t protocol_mode;
    const char *protocol_string;
} program_config_t;

/**
 * Protocol statistics for dual-mode operation
 */
typedef struct {
    uint64_t jsonrpc_requests;
    uint64_t mcp_requests;
    uint64_t protocol_switches;
    uint64_t protocol_detection_time_us;
    uint64_t auto_detections;
    uint64_t detection_errors;
} protocol_stats_t;

// Global worker pools for async execution
worker_pool_t *g_script_worker_pool = NULL;
worker_pool_t *g_worker_pool = NULL;  // Main worker pool for bulk operations

/**
 * Concurrent daemon context structure with dual-mode support.
 */
typedef struct {
    // Core components
    socket_server_t *socket_server;
    worker_pool_t *worker_pool;
    worker_pool_t *script_worker_pool;  // Separate pool for script execution
    request_queue_t *request_queue;
    
    // Goxel core instances (one per worker for thread safety)
    void **goxel_contexts;
    int num_contexts;
    
    // Configuration
    program_config_t *config;
    
    // State management
    bool running;
    pthread_mutex_t state_mutex;
    
    // Protocol handling
    bool mcp_initialized;
    pthread_mutex_t protocol_mutex;
    
    // Statistics
    struct {
        uint64_t requests_processed;
        uint64_t requests_failed;
        uint64_t concurrent_connections;
        int64_t start_time_us;
        protocol_stats_t protocol_stats;
    } stats;
    
    // Cleanup thread
    pthread_t cleanup_thread;
    bool cleanup_thread_running;
} concurrent_daemon_t;

// Global daemon pointer for signal handler
static concurrent_daemon_t *g_daemon = NULL;
static char g_socket_path[256] = {0};

// Cleanup function for atexit
static void cleanup_socket_on_exit(void) {
    if (strlen(g_socket_path) > 0) {
        LOG_INFO("Cleaning up socket on exit: %s", g_socket_path);
        fprintf(stderr, "ATEXIT: Cleaning up socket: %s\n", g_socket_path);
        if (unlink(g_socket_path) == 0) {
            LOG_INFO("Socket file removed on exit");
            fprintf(stderr, "ATEXIT: Socket removed successfully\n");
        } else if (errno != ENOENT) {
            LOG_W("Failed to remove socket file on exit: %s", strerror(errno));
            fprintf(stderr, "ATEXIT: Failed to remove socket: %s\n", strerror(errno));
        }
    }
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (g_daemon) {
            LOG_INFO("Received signal %d, shutting down", sig);
            g_daemon->running = false;
            // Let the main loop handle cleanup properly
            // The atexit handler will clean up the socket
        }
    }
}

// ============================================================================
// PROJECT CLEANUP THREAD
// ============================================================================

/**
 * Cleanup thread function - automatically cleans up idle projects
 */
static void *project_cleanup_thread(void *arg)
{
    concurrent_daemon_t *daemon = (concurrent_daemon_t *)arg;
    
    LOG_I("Project cleanup thread started");
    
    while (daemon->cleanup_thread_running) {
        sleep(10); // Check every 10 seconds
        
        if (project_is_idle(300)) { // 5 minute timeout
            LOG_I("Auto-cleaning idle project");
            
            if (project_lock_acquire("auto_cleanup") == 0) {
                // Reset all Goxel contexts
                for (int i = 0; i < daemon->num_contexts; i++) {
                    if (daemon->goxel_contexts[i]) {
                        goxel_core_reset(daemon->goxel_contexts[i]);
                    }
                }
                
                // Clear project state
                g_project_state.has_active_project = false;
                memset(g_project_state.project_id, 0, sizeof(g_project_state.project_id));
                
                project_lock_release();
                LOG_I("Idle project cleaned up successfully");
            } else {
                LOG_W("Could not acquire lock for auto-cleanup");
            }
        }
    }
    
    LOG_I("Project cleanup thread stopped");
    return NULL;
}

// ============================================================================
// HELP AND VERSION FUNCTIONS
// ============================================================================

static void print_version(void)
{
    printf("%s version %s\n", PROGRAM_NAME, VERSION);
    printf("Goxel v%s Daemon Architecture - Process Lifecycle Management\n", VERSION);
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
    printf("  -P, --protocol PROTO    Protocol mode: auto|jsonrpc|mcp (default: auto)\n");
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
    printf("Protocol Support:\n");
    printf("  auto      - Auto-detect JSON-RPC or MCP (4-byte magic detection)\n");
    printf("  jsonrpc   - JSON-RPC protocol only (Goxel v13 compatible)\n");
    printf("  mcp       - Model Context Protocol only (LLM integration)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --daemonize                    # Start daemon in background\n", PROGRAM_NAME);
    printf("  %s --foreground --verbose         # Start in foreground with verbose output\n", PROGRAM_NAME);
    printf("  %s --protocol=mcp                 # Start with MCP protocol only\n", PROGRAM_NAME);
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
    config->worker_threads = 8;
    config->queue_size = 1024;
    config->enable_priority_queue = false;
    config->max_connections = 256;
    // Protocol defaults
    config->protocol_mode = PROTOCOL_AUTO;
    config->protocol_string = "auto";
    
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
            case 'P':
                config->protocol_string = optarg;
                if (strcmp(optarg, "auto") == 0) {
                    config->protocol_mode = PROTOCOL_AUTO;
                } else if (strcmp(optarg, "jsonrpc") == 0) {
                    config->protocol_mode = PROTOCOL_JSON_RPC;
                } else if (strcmp(optarg, "mcp") == 0) {
                    config->protocol_mode = PROTOCOL_MCP;
                } else {
                    fprintf(stderr, "Invalid protocol: %s (must be auto, jsonrpc, or mcp)\n", optarg);
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
// FORWARD DECLARATIONS FOR ASYNC PROCESSING
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
 * Cleanup function for request processing data.
 */
static void cleanup_request_data(void *request_data);

// ============================================================================
// PROTOCOL DETECTION AND HANDLING
// ============================================================================

/**
 * Detect protocol from message magic bytes (4 bytes)
 * Returns detected protocol or PROTOCOL_AUTO if unclear
 */
static int detect_protocol_from_magic(const char *data, size_t length)
{
    if (length < MAGIC_DETECT_SIZE) {
        return PROTOCOL_AUTO; // Need more data
    }
    
    // Check for JSON-RPC patterns
    if (data[0] == '{' && data[1] == '"') {
        // Look for common JSON-RPC starts: {"method, {"id, {"jsonrpc
        if (strncmp(data, "{\"method", 8) == 0 ||
            strncmp(data, "{\"id", 4) == 0 ||
            strncmp(data, "{\"jsonrpc", 9) == 0) {
            return PROTOCOL_JSON_RPC;
        }
        
        // Look for MCP patterns: {"tool
        if (strncmp(data, "{\"tool", 6) == 0) {
            return PROTOCOL_MCP;
        }
    }
    
    // Default to JSON-RPC for any JSON-like structure
    if (data[0] == '{') {
        return PROTOCOL_JSON_RPC;
    }
    
    return PROTOCOL_AUTO;
}

/**
 * Handle MCP protocol message
 */
static socket_message_t *handle_mcp_message(concurrent_daemon_t *daemon,
                                           socket_client_t *client,
                                           const socket_message_t *message)
{
    if (!daemon->mcp_initialized) {
        // Initialize MCP handler if not done
        mcp_error_code_t result = mcp_handler_init();
        if (result != MCP_SUCCESS) {
            LOG_ERROR("Failed to initialize MCP handler: %s", mcp_error_string(result));
            return NULL;
        }
        daemon->mcp_initialized = true;
    }
    
    // Parse MCP request
    char *json_str = malloc(message->length + 1);
    if (!json_str) {
        return NULL;
    }
    memcpy(json_str, message->data, message->length);
    json_str[message->length] = '\0';
    
    mcp_tool_request_t *mcp_request = NULL;
    mcp_error_code_t parse_result = mcp_parse_request(json_str, &mcp_request);
    free(json_str);
    
    if (parse_result != MCP_SUCCESS) {
        LOG_ERROR("Failed to parse MCP request: %s", mcp_error_string(parse_result));
        return NULL;
    }
    
    // Handle MCP request
    mcp_tool_response_t *mcp_response = NULL;
    uint64_t start_time = get_current_time_us();
    
    mcp_error_code_t handle_result = mcp_handle_tool_request(mcp_request, &mcp_response);
    
    uint64_t end_time = get_current_time_us();
    
    // Update statistics
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->stats.protocol_stats.mcp_requests++;
    daemon->stats.protocol_stats.protocol_detection_time_us += (end_time - start_time);
    if (handle_result == MCP_SUCCESS) {
        daemon->stats.requests_processed++;
    } else {
        daemon->stats.requests_failed++;
    }
    pthread_mutex_unlock(&daemon->state_mutex);
    
    socket_message_t *response_msg = NULL;
    
    if (handle_result == MCP_SUCCESS && mcp_response) {
        // Serialize MCP response
        char *response_json = NULL;
        mcp_error_code_t serialize_result = mcp_serialize_response(mcp_response, &response_json);
        
        if (serialize_result == MCP_SUCCESS && response_json) {
            response_msg = socket_message_create_json(message->id, 0, response_json);
            free(response_json);
        }
    }
    
    // Cleanup
    mcp_free_request(mcp_request);
    mcp_free_response(mcp_response);
    
    return response_msg;
}

/**
 * Handle JSON-RPC protocol message (existing logic)
 */
static socket_message_t *handle_jsonrpc_message(concurrent_daemon_t *daemon,
                                               socket_client_t *client,
                                               const socket_message_t *message)
{
    LOG_I("handle_jsonrpc_message called with daemon=%p, client=%p, message=%p", 
          daemon, client, message);
    
    // Existing JSON-RPC handling logic from the original handle_socket_message
    if (!daemon || !daemon->running || !message || !message->data) {
        LOG_W("Invalid parameters to handle_jsonrpc_message");
        return NULL;
    }
    
    // Null-terminate the message for parsing
    char *json_str = malloc(message->length + 1);
    if (!json_str) {
        LOG_ERROR("Failed to allocate memory for JSON string");
        return NULL;
    }
    memcpy(json_str, message->data, message->length);
    json_str[message->length] = '\0';
    
    // Check if this is a batch request
    const char *trimmed = json_str;
    while (*trimmed && (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r')) {
        trimmed++;
    }
    
    bool is_batch = (*trimmed == '[');
    
    if (is_batch) {
        // Handle batch request synchronously
        char *response_str = NULL;
        json_rpc_result_t batch_result = json_rpc_handle_batch(json_str, &response_str);
        free(json_str);
        
        // Update statistics
        pthread_mutex_lock(&daemon->state_mutex);
        daemon->stats.protocol_stats.jsonrpc_requests++;
        if (batch_result == JSON_RPC_SUCCESS) {
            daemon->stats.requests_processed++;
        } else {
            daemon->stats.requests_failed++;
        }
        pthread_mutex_unlock(&daemon->state_mutex);
        
        if (batch_result == JSON_RPC_SUCCESS && response_str) {
            socket_message_t *response_msg = socket_message_create_json(message->id, 0, response_str);
            free(response_str);
            return response_msg;
        }
        return NULL;
    }
    
    // Single JSON-RPC request - parse first to check if it's a test method
    json_rpc_request_t *rpc_request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(json_str, &rpc_request);
    free(json_str);
    
    if (parse_result != JSON_RPC_SUCCESS || !rpc_request) {
        // Send error response for invalid requests
        json_rpc_response_t *error_response = json_rpc_create_response_error(
            JSON_RPC_PARSE_ERROR,
            "Invalid JSON-RPC request",
            NULL,
            NULL
        );
        
        if (!error_response) {
            return NULL;
        }
        
        char *error_json = NULL;
        if (json_rpc_serialize_response(error_response, &error_json) == JSON_RPC_SUCCESS) {
            socket_message_t *error_msg = socket_message_create_json(message->id, 0, error_json);
            free(error_json);
            json_rpc_free_response(error_response);
            return error_msg;
        }
        json_rpc_free_response(error_response);
        return NULL;
    }
    
    // Check if this is a test method that should be handled synchronously
    if (rpc_request->method) {
        json_rpc_response_t *test_response = handle_test_method(rpc_request->method, rpc_request);
        if (test_response) {
            // This is a test method - handle synchronously
            char *response_json = NULL;
            if (json_rpc_serialize_response(test_response, &response_json) == JSON_RPC_SUCCESS && response_json) {
                socket_message_t *response_msg = socket_message_create_json(message->id, 0, response_json);
                free(response_json);
                json_rpc_free_response(test_response);
                json_rpc_free_request(rpc_request);
                
                // Update statistics
                pthread_mutex_lock(&daemon->state_mutex);
                daemon->stats.protocol_stats.jsonrpc_requests++;
                daemon->stats.requests_processed++;
                pthread_mutex_unlock(&daemon->state_mutex);
                
                return response_msg;
            }
            json_rpc_free_response(test_response);
        }
    }
    
    // Update statistics
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->stats.protocol_stats.jsonrpc_requests++;
    pthread_mutex_unlock(&daemon->state_mutex);
    
    // Note: process_data is only needed for async handling which we're not using yet
    
    // Skip worker pool submission for now - process synchronously
    // TODO: Re-enable async processing with proper response handling
    /*
    worker_pool_error_t submit_result = worker_pool_submit_request(daemon->worker_pool,
                                                                  process_data,
                                                                  WORKER_PRIORITY_NORMAL);
    
    if (submit_result != WORKER_POOL_SUCCESS) {
        cleanup_request_data(process_data);
        
        // Send error response for queue full
        json_rpc_response_t *error_response = json_rpc_create_response_error(
            JSON_RPC_INTERNAL_ERROR,
            "Server overloaded - request queue full",
            NULL,
            &rpc_request->id
        );
        
        if (!error_response) {
            return NULL;
        }
        
        char *error_json = NULL;
        if (json_rpc_serialize_response(error_response, &error_json) == JSON_RPC_SUCCESS) {
            socket_message_t *error_msg = socket_message_create_json(message->id, 0, error_json);
            free(error_json);
            json_rpc_free_response(error_response);
            return error_msg;
        }
        json_rpc_free_response(error_response);
    }
    */
    
    // Update statistics
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->stats.protocol_stats.jsonrpc_requests++;
    pthread_mutex_unlock(&daemon->state_mutex);
    
    // For now, process synchronously to ensure response is sent
    // TODO: Implement proper async response handling
    json_rpc_response_t *response = json_rpc_handle_method(rpc_request);
    socket_message_t *response_msg = NULL;
    
    if (response) {
        char *response_json = NULL;
        if (json_rpc_serialize_response(response, &response_json) == JSON_RPC_SUCCESS) {
            LOG_I("Generated JSON response: %s", response_json);
            response_msg = socket_message_create_json(message->id, 0, response_json);
            if (response_msg) {
                LOG_I("Created response message: id=%u, length=%u, data=%p", 
                      response_msg->id, response_msg->length, response_msg->data);
                if (response_msg->data) {
                    LOG_I("Response data: %.*s", (int)response_msg->length, response_msg->data);
                }
            } else {
                LOG_E("Failed to create response message");
            }
            free(response_json);
        } else {
            LOG_E("Failed to serialize response");
        }
        json_rpc_free_response(response);
    } else {
        LOG_W("No response generated from json_rpc_handle_method");
    }
    
    // Clean up the request since we processed synchronously
    LOG_I("About to free request");
    json_rpc_free_request(rpc_request);
    LOG_I("Request freed successfully");
    
    // Update statistics
    LOG_I("About to update statistics");
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->stats.requests_processed++;
    pthread_mutex_unlock(&daemon->state_mutex);
    LOG_I("Statistics updated");
    
    if (response_msg) {
        LOG_I("Returning response_msg: %p (data=%p, length=%u)", 
              response_msg, response_msg->data, response_msg->length);
        
        // Validate response_msg before returning
        if (response_msg->length > 0 && response_msg->data == NULL) {
            LOG_E("Invalid response_msg: length=%u but data=NULL", response_msg->length);
            socket_message_destroy(response_msg);
            return NULL;
        }
    } else {
        LOG_I("Returning NULL response_msg");
    }
    fflush(stdout);
    fflush(stderr);
    return response_msg;
}

// ============================================================================
// CONCURRENT PROCESSING IMPLEMENTATION
// =============================================================================

/**
 * Worker thread processing function.
 * Processes JSON-RPC requests using Goxel core functionality.
 */
static int process_rpc_request(void *request_data, int worker_id, void *context);

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
    LOG_I("cleanup_request_data: data=%p", data);
    if (data) {
        LOG_I("cleanup_request_data: rpc_request=%p", data->rpc_request);
        if (data->rpc_request) {
            // Cast away const for cleanup - we own this memory
            LOG_I("cleanup_request_data: freeing rpc_request");
            json_rpc_free_request((json_rpc_request_t*)data->rpc_request);
            data->rpc_request = NULL;
            LOG_I("cleanup_request_data: rpc_request freed");
        }
        LOG_I("cleanup_request_data: freeing data structure");
        free(data);
        LOG_I("cleanup_request_data: data structure freed");
    }
}

/**
 * Unified socket server message handler with dual-mode protocol support.
 * Detects protocol automatically and routes to appropriate handler.
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
    
    uint64_t detection_start = get_current_time_us();
    int detected_protocol = PROTOCOL_JSON_RPC; // Default fallback
    
    // Protocol detection based on configuration
    if (daemon->config->protocol_mode == PROTOCOL_AUTO) {
        detected_protocol = detect_protocol_from_magic((char*)message->data, message->length);
        if (detected_protocol == PROTOCOL_AUTO) {
            // Fallback to JSON-RPC if detection is unclear
            detected_protocol = PROTOCOL_JSON_RPC;
        }
        pthread_mutex_lock(&daemon->state_mutex);
        daemon->stats.protocol_stats.auto_detections++;
        daemon->stats.protocol_stats.protocol_detection_time_us += (get_current_time_us() - detection_start);
        pthread_mutex_unlock(&daemon->state_mutex);
    } else if (daemon->config->protocol_mode == PROTOCOL_JSON_RPC) {
        detected_protocol = PROTOCOL_JSON_RPC;
    } else if (daemon->config->protocol_mode == PROTOCOL_MCP) {
        detected_protocol = PROTOCOL_MCP;
    }
    
    // Route to appropriate protocol handler
    socket_message_t *response = NULL;
    if (detected_protocol == PROTOCOL_MCP) {
        response = handle_mcp_message(daemon, client, message);
    } else {
        // Default to JSON-RPC
        response = handle_jsonrpc_message(daemon, client, message);
    }
    
    return response;
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
    daemon->mcp_initialized = false;
    
    // Initialize state and protocol mutexes
    if (pthread_mutex_init(&daemon->state_mutex, NULL) != 0 ||
        pthread_mutex_init(&daemon->protocol_mutex, NULL) != 0) {
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
    
    // Initialize global goxel instance for daemon mode
    goxel_init();
    
    // Initialize project mutex system
    if (project_mutex_init() != 0) {
        LOG_ERROR("Failed to initialize project mutex system");
        pthread_mutex_destroy(&daemon->state_mutex);
        pthread_mutex_destroy(&daemon->protocol_mutex);
        free(daemon->goxel_contexts);
        free(daemon);
        return NULL;
    }
    
    // Initialize Goxel contexts - one per worker thread
    json_rpc_result_t init_result = json_rpc_init_goxel_context();
    if (init_result != JSON_RPC_SUCCESS) {
        LOG_ERROR("Failed to initialize Goxel context: %s", json_rpc_result_string(init_result));
        pthread_mutex_destroy(&daemon->state_mutex);
        pthread_mutex_destroy(&daemon->protocol_mutex);
        free(daemon->goxel_contexts);
        free(daemon);
        return NULL;
    }
    
    // Initialize MCP handler if MCP-only mode
    if (config->protocol_mode == PROTOCOL_MCP) {
        mcp_error_code_t mcp_result = mcp_handler_init();
        if (mcp_result != MCP_SUCCESS) {
            LOG_ERROR("Failed to initialize MCP handler: %s", mcp_error_string(mcp_result));
            json_rpc_cleanup_goxel_context();
            pthread_mutex_destroy(&daemon->state_mutex);
            pthread_mutex_destroy(&daemon->protocol_mutex);
            free(daemon->goxel_contexts);
            free(daemon);
            return NULL;
        }
        daemon->mcp_initialized = true;
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
    const char *socket_path = get_persistent_socket_path(config->socket_path);
    server_config.socket_path = socket_path;
    
    // Store socket path globally for cleanup
    strncpy(g_socket_path, socket_path, sizeof(g_socket_path) - 1);
    g_socket_path[sizeof(g_socket_path) - 1] = '\0';
    
    server_config.max_connections = config->max_connections;
    server_config.thread_per_client = false;
    server_config.thread_pool_size = config->worker_threads;
    server_config.msg_handler = handle_socket_message;
    server_config.client_handler = NULL;  // Don't set client handler - will call manually after protocol detection
    server_config.user_data = daemon;
    
    daemon->socket_server = socket_server_create(&server_config);
    if (!daemon->socket_server) {
        for (int i = 0; i < daemon->num_contexts; i++) {
            // goxel_core_destroy_context(daemon->goxel_contexts[i]);
        }
        free(daemon->goxel_contexts);
        pthread_mutex_destroy(&daemon->state_mutex);
        pthread_mutex_destroy(&daemon->protocol_mutex);
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
        pthread_mutex_destroy(&daemon->protocol_mutex);
        free(daemon);
        return NULL;
    }
    
    
    // Create script worker pool with separate configuration
#if 1
    // Forward declare the process function from json_rpc.c
    extern int process_script_execution(void *request_data, int worker_id, void *context);
    extern void cleanup_script_execution(void *request_data);
    
    worker_pool_config_t script_pool_config = worker_pool_default_config();
    script_pool_config.worker_count = 4;  // Fewer workers for script execution
    script_pool_config.queue_capacity = 100;  // Smaller queue for scripts
    script_pool_config.enable_priority_queue = true;
    script_pool_config.process_func = process_script_execution;
    script_pool_config.cleanup_func = NULL; // We'll clean up manually after reading results
    script_pool_config.context = daemon;
    
    daemon->script_worker_pool = worker_pool_create(&script_pool_config);
    if (!daemon->script_worker_pool) {
        worker_pool_destroy(daemon->worker_pool);
        socket_server_destroy(daemon->socket_server);
        for (int i = 0; i < daemon->num_contexts; i++) {
            // goxel_core_destroy_context(daemon->goxel_contexts[i]);
        }
        free(daemon->goxel_contexts);
        pthread_mutex_destroy(&daemon->state_mutex);
        pthread_mutex_destroy(&daemon->protocol_mutex);
        free(daemon);
        return NULL;
    }
#endif
    
    // Set global pointers for access from json_rpc.c
    g_script_worker_pool = daemon->script_worker_pool;
    g_worker_pool = daemon->worker_pool;
    
    // Create request queue
    request_queue_config_t queue_config = request_queue_default_config();
    queue_config.max_size = config->queue_size;
    queue_config.enable_priority_queue = config->enable_priority_queue;
    
    daemon->request_queue = request_queue_create(&queue_config);
    if (!daemon->request_queue) {
        worker_pool_destroy(daemon->worker_pool);
        worker_pool_destroy(daemon->script_worker_pool);
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
    
    LOG_INFO("Destroying daemon...");
    daemon->running = false;
    
    // Stop cleanup thread
    if (daemon->cleanup_thread_running) {
        daemon->cleanup_thread_running = false;
        pthread_join(daemon->cleanup_thread, NULL);
    }
    
    // Cleanup project mutex system
    project_mutex_cleanup();
    
    // Stop components
    if (daemon->worker_pool) {
        worker_pool_stop(daemon->worker_pool);
        worker_pool_destroy(daemon->worker_pool);
    }
    
    if (daemon->script_worker_pool) {
        worker_pool_stop(daemon->script_worker_pool);
        worker_pool_destroy(daemon->script_worker_pool);
        g_script_worker_pool = NULL;  // Clear global pointer
    }
    
    if (daemon->worker_pool) {
        g_worker_pool = NULL;  // Clear global pointer
    }
    
    if (daemon->socket_server) {
        socket_server_stop(daemon->socket_server);
        socket_server_destroy(daemon->socket_server);
    }
    
    if (daemon->request_queue) {
        request_queue_destroy(daemon->request_queue);
    }
    
    // Cleanup protocol handlers
    if (daemon->mcp_initialized) {
        mcp_handler_cleanup();
    }
    
    // Cleanup Goxel contexts
    if (daemon->goxel_contexts) {
        // Cleanup the shared Goxel context
        json_rpc_cleanup_goxel_context();
        free(daemon->goxel_contexts);
    }
    
    pthread_mutex_destroy(&daemon->state_mutex);
    pthread_mutex_destroy(&daemon->protocol_mutex);
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
        printf("  Socket: %s\n", get_persistent_socket_path(prog_config->socket_path));
        printf("  Log file: %s\n", prog_config->log_file);
        printf("  Working directory: %s\n", prog_config->working_dir);
        printf("  Daemonize: %s\n", prog_config->daemonize ? "yes" : "no");
        printf("  Protocol mode: %s\n", prog_config->protocol_string);
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
    
    // Start script worker pool
    pool_result = worker_pool_start(daemon->script_worker_pool);
    if (pool_result != WORKER_POOL_SUCCESS) {
        fprintf(stderr, "Failed to start script worker pool: %s\n", 
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
    
    // Start cleanup thread
    daemon->cleanup_thread_running = true;
    int ret = pthread_create(&daemon->cleanup_thread, NULL, project_cleanup_thread, daemon);
    if (ret != 0) {
        fprintf(stderr, "Failed to create cleanup thread: %s\n", strerror(ret));
        destroy_concurrent_daemon(daemon);
        return 1;
    }
    
    // Set global daemon pointer for signal handler
    g_daemon = daemon;
    
    // Install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // Register atexit handler for socket cleanup
    atexit(cleanup_socket_on_exit);
    
    // Set daemon as running
    pthread_mutex_lock(&daemon->state_mutex);
    daemon->running = true;
    daemon->stats.start_time_us = get_current_time_us();
    pthread_mutex_unlock(&daemon->state_mutex);
    
    if (prog_config->verbose && !prog_config->daemonize) {
        printf("Concurrent daemon started successfully (PID: %d)\n", getpid());
        printf("  Socket server listening on: %s\n", get_persistent_socket_path(prog_config->socket_path));
        printf("  Protocol mode: %s\n", prog_config->protocol_string);
        printf("  Worker pool with %d threads ready\n", prog_config->worker_threads);
        if (daemon->mcp_initialized) {
            printf("  MCP handler initialized and ready\n");
        }
        printf("Press Ctrl+C to stop the daemon\n");
    }
    
    // Main daemon loop - wait for shutdown signal
    while (daemon->running) {
        sleep(1);
        
        // Handle timeouts and cleanup
        request_queue_handle_timeouts(daemon->request_queue);
        
        // Check if we should continue running
        if (!socket_server_is_running(daemon->socket_server) ||
            !worker_pool_is_running(daemon->worker_pool) ||
            !worker_pool_is_running(daemon->script_worker_pool)) {
            break;
        }
    }
    
    LOG_INFO("Exited main loop, daemon->running = %d", daemon->running);
    
    if (prog_config->verbose && !prog_config->daemonize) {
        printf("Daemon shutting down...\n");
        
        // Print final statistics
        worker_stats_t worker_stats;
        if (worker_pool_get_stats(daemon->worker_pool, &worker_stats) == WORKER_POOL_SUCCESS) {
            printf("Final statistics:\n");
            printf("  Requests processed: %llu\n", (unsigned long long)worker_stats.requests_processed);
            printf("  Requests failed: %llu\n", (unsigned long long)worker_stats.requests_failed);
            printf("  Average processing time: %llu μs\n", (unsigned long long)worker_stats.average_processing_time_us);
            
            // Protocol-specific statistics
            printf("  JSON-RPC requests: %llu\n", (unsigned long long)daemon->stats.protocol_stats.jsonrpc_requests);
            printf("  MCP requests: %llu\n", (unsigned long long)daemon->stats.protocol_stats.mcp_requests);
            if (daemon->config->protocol_mode == PROTOCOL_AUTO) {
                printf("  Auto-detections: %llu\n", (unsigned long long)daemon->stats.protocol_stats.auto_detections);
                printf("  Avg detection time: %llu μs\n", 
                       daemon->stats.protocol_stats.auto_detections > 0 ?
                       (unsigned long long)(daemon->stats.protocol_stats.protocol_detection_time_us / daemon->stats.protocol_stats.auto_detections) : 0);
            }
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
    g_daemon = NULL;
    
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

/**
 * Get persistent socket path with fallback logic.
 * Ensures consistent socket path across daemon restarts.
 */
static const char *get_persistent_socket_path(const char *requested_path)
{
    static char persistent_path[256];
    
    if (requested_path && strlen(requested_path) > 0) {
        strncpy(persistent_path, requested_path, sizeof(persistent_path) - 1);
        persistent_path[sizeof(persistent_path) - 1] = '\0';
    } else {
        // Use Homebrew path if available
        if (access("/opt/homebrew/var/run/goxel", F_OK) == 0) {
            strcpy(persistent_path, "/opt/homebrew/var/run/goxel/goxel.sock");
        } else {
            strcpy(persistent_path, DEFAULT_SOCKET_PATH);
        }
    }
    
    return persistent_path;
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