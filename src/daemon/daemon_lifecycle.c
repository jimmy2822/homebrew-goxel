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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

// ============================================================================
// GLOBAL STATE FOR SIGNAL HANDLING
// ============================================================================

// Forward declaration for signal handling functions
extern daemon_error_t daemon_setup_signals_impl(daemon_context_t *ctx);
extern daemon_error_t daemon_cleanup_signals_impl(void);

// ============================================================================
// ERROR HANDLING
// ============================================================================

const char *daemon_error_string(daemon_error_t error)
{
    switch (error) {
        case DAEMON_SUCCESS:
            return "Success";
        case DAEMON_ERROR_INVALID_CONTEXT:
            return "Invalid or NULL context";
        case DAEMON_ERROR_INVALID_PARAMETER:
            return "Invalid parameter value";
        case DAEMON_ERROR_ALREADY_RUNNING:
            return "Daemon already running";
        case DAEMON_ERROR_NOT_RUNNING:
            return "Daemon not running";
        case DAEMON_ERROR_FORK_FAILED:
            return "Failed to fork daemon process";
        case DAEMON_ERROR_SETSID_FAILED:
            return "Failed to create new session";
        case DAEMON_ERROR_CHDIR_FAILED:
            return "Failed to change directory";
        case DAEMON_ERROR_SIGNAL_SETUP_FAILED:
            return "Failed to setup signal handlers";
        case DAEMON_ERROR_PID_FILE_CREATE_FAILED:
            return "Failed to create PID file";
        case DAEMON_ERROR_PID_FILE_WRITE_FAILED:
            return "Failed to write PID file";
        case DAEMON_ERROR_PID_FILE_REMOVE_FAILED:
            return "Failed to remove PID file";
        case DAEMON_ERROR_PID_FILE_INVALID:
            return "Invalid PID file format";
        case DAEMON_ERROR_MUTEX_FAILED:
            return "Mutex operation failed";
        case DAEMON_ERROR_OUT_OF_MEMORY:
            return "Memory allocation failed";
        case DAEMON_ERROR_CONFIG_INVALID:
            return "Configuration file invalid";
        case DAEMON_ERROR_CONFIG_NOT_FOUND:
            return "Configuration file not found";
        case DAEMON_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case DAEMON_ERROR_SERVER_INIT_FAILED:
            return "Server initialization failed";
        case DAEMON_ERROR_GOXEL_INIT_FAILED:
            return "Goxel instance initialization failed";
        case DAEMON_ERROR_TIMEOUT:
            return "Operation timed out";
        default:
            return "Unknown error";
    }
}

daemon_error_t daemon_get_last_error(const daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    return ctx->last_error;
}

const char *daemon_get_last_error_message(const daemon_context_t *ctx)
{
    if (!ctx) return NULL;
    return ctx->last_error_message;
}

void daemon_set_error(daemon_context_t *ctx, daemon_error_t error, const char *message)
{
    if (!ctx) return;
    
    ctx->last_error = error;
    
    if (ctx->last_error_message) {
        free(ctx->last_error_message);
        ctx->last_error_message = NULL;
    }
    
    if (message) {
        ctx->last_error_message = strdup(message);
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int64_t daemon_get_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void daemon_sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

daemon_error_t daemon_redirect_stdio(void)
{
    int fd;
    
    // Redirect stdin
    fd = open("/dev/null", O_RDONLY);
    if (fd == -1) return DAEMON_ERROR_PERMISSION_DENIED;
    if (dup2(fd, STDIN_FILENO) == -1) {
        close(fd);
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    close(fd);
    
    // Redirect stdout
    fd = open("/dev/null", O_WRONLY);
    if (fd == -1) return DAEMON_ERROR_PERMISSION_DENIED;
    if (dup2(fd, STDOUT_FILENO) == -1) {
        close(fd);
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    close(fd);
    
    // Redirect stderr
    fd = open("/dev/null", O_WRONLY);
    if (fd == -1) return DAEMON_ERROR_PERMISSION_DENIED;
    if (dup2(fd, STDERR_FILENO) == -1) {
        close(fd);
        return DAEMON_ERROR_PERMISSION_DENIED;
    }
    close(fd);
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

daemon_config_t daemon_default_config(void)
{
    daemon_config_t config = {0};
    
    config.pid_file_path = "/tmp/goxel-daemon.pid";
    config.socket_path = "/tmp/goxel-daemon.sock";
    config.config_file_path = NULL;
    config.log_file_path = "/tmp/goxel-daemon.log";
    config.working_directory = "/";
    config.max_connections = 10;
    config.startup_timeout_ms = 30000;  // 30 seconds
    config.shutdown_timeout_ms = 10000; // 10 seconds
    config.daemonize = true;
    config.create_pid_file = true;
    config.run_as_uid = 0; // no change
    config.run_as_gid = 0; // no change
    
    return config;
}

daemon_error_t daemon_load_config(const char *config_path, daemon_config_t *config)
{
    if (!config_path || !config) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    // For now, just use default config
    // In a full implementation, this would parse a config file
    *config = daemon_default_config();
    
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_validate_config(const daemon_config_t *config)
{
    if (!config) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    if (!config->pid_file_path || strlen(config->pid_file_path) == 0) {
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    if (!config->socket_path || strlen(config->socket_path) == 0) {
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    if (config->max_connections <= 0) {
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    if (config->startup_timeout_ms <= 0 || config->shutdown_timeout_ms <= 0) {
        return DAEMON_ERROR_CONFIG_INVALID;
    }
    
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_create_directories(const daemon_config_t *config)
{
    if (!config) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    // Create directory for PID file
    if (config->pid_file_path) {
        char *dir = strdup(config->pid_file_path);
        if (!dir) return DAEMON_ERROR_OUT_OF_MEMORY;
        
        char *last_slash = strrchr(dir, '/');
        if (last_slash && last_slash != dir) {
            *last_slash = '\0';
            if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
                free(dir);
                return DAEMON_ERROR_PERMISSION_DENIED;
            }
        }
        free(dir);
    }
    
    // Create directory for socket
    if (config->socket_path) {
        char *dir = strdup(config->socket_path);
        if (!dir) return DAEMON_ERROR_OUT_OF_MEMORY;
        
        char *last_slash = strrchr(dir, '/');
        if (last_slash && last_slash != dir) {
            *last_slash = '\0';
            if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
                free(dir);
                return DAEMON_ERROR_PERMISSION_DENIED;
            }
        }
        free(dir);
    }
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// PID FILE MANAGEMENT
// ============================================================================

daemon_error_t daemon_create_pid_file(const char *pid_file_path)
{
    if (!pid_file_path) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    int fd = open(pid_file_path, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            return DAEMON_ERROR_ALREADY_RUNNING;
        }
        return DAEMON_ERROR_PID_FILE_CREATE_FAILED;
    }
    
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    
    if (write(fd, pid_str, strlen(pid_str)) == -1) {
        close(fd);
        unlink(pid_file_path);
        return DAEMON_ERROR_PID_FILE_WRITE_FAILED;
    }
    
    close(fd);
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_remove_pid_file(const char *pid_file_path)
{
    if (!pid_file_path) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    if (unlink(pid_file_path) == -1) {
        if (errno != ENOENT) {
            return DAEMON_ERROR_PID_FILE_REMOVE_FAILED;
        }
    }
    
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_read_pid_file(const char *pid_file_path, pid_t *pid)
{
    if (!pid_file_path || !pid) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    FILE *file = fopen(pid_file_path, "r");
    if (!file) {
        return DAEMON_ERROR_CONFIG_NOT_FOUND;
    }
    
    char pid_str[32];
    if (!fgets(pid_str, sizeof(pid_str), file)) {
        fclose(file);
        return DAEMON_ERROR_PID_FILE_INVALID;
    }
    
    fclose(file);
    
    char *endptr;
    long pid_val = strtol(pid_str, &endptr, 10);
    if (*endptr != '\n' && *endptr != '\0') {
        return DAEMON_ERROR_PID_FILE_INVALID;
    }
    
    *pid = (pid_t)pid_val;
    return DAEMON_SUCCESS;
}

bool daemon_is_process_running(pid_t pid)
{
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

daemon_error_t daemon_drop_privileges(uid_t uid, gid_t gid)
{
    if (gid != 0) {
        if (setgid(gid) == -1) {
            return DAEMON_ERROR_PERMISSION_DENIED;
        }
    }
    
    if (uid != 0) {
        if (setuid(uid) == -1) {
            return DAEMON_ERROR_PERMISSION_DENIED;
        }
    }
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// DAEMON PROCESS MANAGEMENT
// ============================================================================

daemon_error_t daemon_daemonize(void)
{
    pid_t pid, sid;
    
    // First fork
    pid = fork();
    if (pid < 0) {
        return DAEMON_ERROR_FORK_FAILED;
    }
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Create new session
    sid = setsid();
    if (sid < 0) {
        return DAEMON_ERROR_SETSID_FAILED;
    }
    
    // Second fork (to prevent daemon from acquiring a controlling terminal)
    pid = fork();
    if (pid < 0) {
        return DAEMON_ERROR_FORK_FAILED;
    }
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Change working directory
    if (chdir("/") < 0) {
        return DAEMON_ERROR_CHDIR_FAILED;
    }
    
    // Set file mode mask
    umask(0);
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

daemon_state_t daemon_get_state(const daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_STATE_ERROR;
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->state_mutex);
    daemon_state_t state = ctx->state;
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->state_mutex);
    
    return state;
}

daemon_error_t daemon_set_state(daemon_context_t *ctx, daemon_state_t state)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    if (pthread_mutex_lock(&ctx->state_mutex) != 0) {
        return DAEMON_ERROR_MUTEX_FAILED;
    }
    
    ctx->state = state;
    
    if (pthread_mutex_unlock(&ctx->state_mutex) != 0) {
        return DAEMON_ERROR_MUTEX_FAILED;
    }
    
    return DAEMON_SUCCESS;
}

bool daemon_is_running(const daemon_context_t *ctx)
{
    if (!ctx) return false;
    return daemon_get_state(ctx) == DAEMON_STATE_RUNNING;
}

bool daemon_shutdown_requested(const daemon_context_t *ctx)
{
    if (!ctx) return false;
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->state_mutex);
    bool requested = ctx->shutdown_requested;
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->state_mutex);
    
    return requested;
}

void daemon_request_shutdown(daemon_context_t *ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->state_mutex);
    ctx->shutdown_requested = true;
    pthread_mutex_unlock(&ctx->state_mutex);
}

// ============================================================================
// MOCK INTERFACES
// ============================================================================

mock_server_t *mock_server_create(const char *socket_path)
{
    if (!socket_path) return NULL;
    
    mock_server_t *server = calloc(1, sizeof(mock_server_t));
    if (!server) return NULL;
    
    server->socket_path = strdup(socket_path);
    if (!server->socket_path) {
        free(server);
        return NULL;
    }
    
    server->mock_socket_fd = -1;
    server->is_running = false;
    
    return server;
}

void mock_server_destroy(mock_server_t *server)
{
    if (!server) return;
    
    if (server->socket_path) {
        free(server->socket_path);
    }
    
    free(server);
}

daemon_error_t mock_server_start(mock_server_t *server)
{
    if (!server) return DAEMON_ERROR_INVALID_PARAMETER;
    
    if (server->is_running) {
        return DAEMON_ERROR_ALREADY_RUNNING;
    }
    
    // Mock implementation - just set running flag
    server->mock_socket_fd = 42; // Mock file descriptor
    server->is_running = true;
    
    return DAEMON_SUCCESS;
}

daemon_error_t mock_server_stop(mock_server_t *server)
{
    if (!server) return DAEMON_ERROR_INVALID_PARAMETER;
    
    if (!server->is_running) {
        return DAEMON_ERROR_NOT_RUNNING;
    }
    
    // Mock implementation - just clear running flag
    server->mock_socket_fd = -1;
    server->is_running = false;
    
    return DAEMON_SUCCESS;
}

mock_goxel_instance_t *mock_goxel_create(const char *config_file)
{
    mock_goxel_instance_t *instance = calloc(1, sizeof(mock_goxel_instance_t));
    if (!instance) return NULL;
    
    if (config_file) {
        instance->config_file = strdup(config_file);
        if (!instance->config_file) {
            free(instance);
            return NULL;
        }
    }
    
    instance->is_initialized = false;
    
    return instance;
}

void mock_goxel_destroy(mock_goxel_instance_t *instance)
{
    if (!instance) return;
    
    if (instance->config_file) {
        free(instance->config_file);
    }
    
    free(instance);
}

daemon_error_t mock_goxel_initialize(mock_goxel_instance_t *instance)
{
    if (!instance) return DAEMON_ERROR_INVALID_PARAMETER;
    
    if (instance->is_initialized) {
        return DAEMON_ERROR_ALREADY_RUNNING;
    }
    
    // Mock implementation - just set initialized flag
    instance->is_initialized = true;
    
    return DAEMON_SUCCESS;
}

daemon_error_t mock_goxel_shutdown(mock_goxel_instance_t *instance)
{
    if (!instance) return DAEMON_ERROR_INVALID_PARAMETER;
    
    if (!instance->is_initialized) {
        return DAEMON_ERROR_NOT_RUNNING;
    }
    
    // Mock implementation - just clear initialized flag
    instance->is_initialized = false;
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// DAEMON CONTEXT MANAGEMENT
// ============================================================================

daemon_context_t *daemon_context_create(const daemon_config_t *config)
{
    if (!config) return NULL;
    
    daemon_context_t *ctx = calloc(1, sizeof(daemon_context_t));
    if (!ctx) return NULL;
    
    // Copy configuration
    ctx->config = *config;
    
    // Duplicate string fields
    if (config->pid_file_path) {
        ctx->config.pid_file_path = strdup(config->pid_file_path);
        if (!ctx->config.pid_file_path) goto error;
    }
    
    if (config->socket_path) {
        ctx->config.socket_path = strdup(config->socket_path);
        if (!ctx->config.socket_path) goto error;
    }
    
    if (config->config_file_path) {
        ctx->config.config_file_path = strdup(config->config_file_path);
        if (!ctx->config.config_file_path) goto error;
    }
    
    if (config->log_file_path) {
        ctx->config.log_file_path = strdup(config->log_file_path);
        if (!ctx->config.log_file_path) goto error;
    }
    
    if (config->working_directory) {
        ctx->config.working_directory = strdup(config->working_directory);
        if (!ctx->config.working_directory) goto error;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->state_mutex, NULL) != 0) {
        goto error;
    }
    
    // Initialize state
    ctx->state = DAEMON_STATE_STOPPED;
    ctx->daemon_pid = 0;
    ctx->shutdown_requested = false;
    ctx->last_error = DAEMON_SUCCESS;
    ctx->last_error_message = NULL;
    ctx->start_time = 0;
    ctx->last_activity = 0;
    ctx->total_requests = 0;
    ctx->total_errors = 0;
    
    // Create mock components
    ctx->server = mock_server_create(ctx->config.socket_path);
    if (!ctx->server) goto error;
    
    ctx->goxel_instance = mock_goxel_create(ctx->config.config_file_path);
    if (!ctx->goxel_instance) goto error;
    
    return ctx;
    
error:
    daemon_context_destroy(ctx);
    return NULL;
}

void daemon_context_destroy(daemon_context_t *ctx)
{
    if (!ctx) return;
    
    // Clean up mock components
    mock_server_destroy(ctx->server);
    mock_goxel_destroy(ctx->goxel_instance);
    
    // Clean up string fields
    if (ctx->config.pid_file_path) free((char*)ctx->config.pid_file_path);
    if (ctx->config.socket_path) free((char*)ctx->config.socket_path);
    if (ctx->config.config_file_path) free((char*)ctx->config.config_file_path);
    if (ctx->config.log_file_path) free((char*)ctx->config.log_file_path);
    if (ctx->config.working_directory) free((char*)ctx->config.working_directory);
    
    if (ctx->last_error_message) {
        free(ctx->last_error_message);
    }
    
    // Clean up mutex
    pthread_mutex_destroy(&ctx->state_mutex);
    
    free(ctx);
}

// ============================================================================
// DAEMON LIFECYCLE IMPLEMENTATION
// ============================================================================

daemon_error_t daemon_initialize(daemon_context_t *ctx, const char *config_path)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    daemon_error_t result;
    
    daemon_set_state(ctx, DAEMON_STATE_STARTING);
    
    // Load configuration if path provided
    if (config_path) {
        daemon_config_t loaded_config;
        result = daemon_load_config(config_path, &loaded_config);
        if (result != DAEMON_SUCCESS) {
            daemon_set_error(ctx, result, "Failed to load configuration");
            daemon_set_state(ctx, DAEMON_STATE_ERROR);
            return result;
        }
        // Update context config
        // For simplicity, we'll keep the existing config for now
    }
    
    // Validate configuration
    result = daemon_validate_config(&ctx->config);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Configuration validation failed");
        daemon_set_state(ctx, DAEMON_STATE_ERROR);
        return result;
    }
    
    // Create necessary directories
    result = daemon_create_directories(&ctx->config);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to create directories");
        daemon_set_state(ctx, DAEMON_STATE_ERROR);
        return result;
    }
    
    // Check if another daemon is already running
    if (ctx->config.create_pid_file) {
        pid_t existing_pid;
        if (daemon_read_pid_file(ctx->config.pid_file_path, &existing_pid) == DAEMON_SUCCESS) {
            if (daemon_is_process_running(existing_pid)) {
                daemon_set_error(ctx, DAEMON_ERROR_ALREADY_RUNNING, "Daemon already running");
                daemon_set_state(ctx, DAEMON_STATE_ERROR);
                return DAEMON_ERROR_ALREADY_RUNNING;
            } else {
                // Remove stale PID file
                daemon_remove_pid_file(ctx->config.pid_file_path);
            }
        }
    }
    
    // Set up signal handlers
    result = daemon_setup_signals(ctx);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to setup signal handlers");
        daemon_set_state(ctx, DAEMON_STATE_ERROR);
        return result;
    }
    
    // Initialize mock Goxel instance
    result = mock_goxel_initialize(ctx->goxel_instance);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to initialize Goxel instance");
        daemon_set_state(ctx, DAEMON_STATE_ERROR);
        return result;
    }
    
    ctx->start_time = daemon_get_timestamp();
    ctx->last_activity = ctx->start_time;
    
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_start(daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    daemon_error_t result;
    
    // Daemonize if configured
    if (ctx->config.daemonize) {
        result = daemon_daemonize();
        if (result != DAEMON_SUCCESS) {
            daemon_set_error(ctx, result, "Failed to daemonize process");
            return result;
        }
        
        // Redirect stdio after daemonizing
        result = daemon_redirect_stdio();
        if (result != DAEMON_SUCCESS) {
            daemon_set_error(ctx, result, "Failed to redirect stdio");
            return result;
        }
    }
    
    // Store daemon PID
    ctx->daemon_pid = getpid();
    
    // Create PID file
    if (ctx->config.create_pid_file) {
        result = daemon_create_pid_file(ctx->config.pid_file_path);
        if (result != DAEMON_SUCCESS) {
            daemon_set_error(ctx, result, "Failed to create PID file");
            return result;
        }
    }
    
    // Drop privileges if configured
    result = daemon_drop_privileges(ctx->config.run_as_uid, ctx->config.run_as_gid);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to drop privileges");
        goto cleanup_pid;
    }
    
    // Start mock server
    result = mock_server_start(ctx->server);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to start server");
        goto cleanup_pid;
    }
    
    daemon_set_state(ctx, DAEMON_STATE_RUNNING);
    return DAEMON_SUCCESS;
    
cleanup_pid:
    if (ctx->config.create_pid_file) {
        daemon_remove_pid_file(ctx->config.pid_file_path);
    }
    return result;
}

daemon_error_t daemon_run(daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    if (daemon_get_state(ctx) != DAEMON_STATE_RUNNING) {
        return DAEMON_ERROR_NOT_RUNNING;
    }
    
    // Main daemon loop
    while (!daemon_shutdown_requested(ctx)) {
        // Process any pending signals first
        daemon_error_t signal_result = daemon_process_signals(ctx);
        if (signal_result != DAEMON_SUCCESS) {
            // Log error but continue running unless it's a shutdown
            if (daemon_shutdown_requested(ctx)) {
                break;
            }
        }
        
        // Update activity timestamp
        daemon_update_activity(ctx);
        
        // Mock processing - in real implementation, this would:
        // - Accept client connections
        // - Process requests
        // - Handle server events
        // - Monitor system health
        
        // Sleep for a short time to avoid busy waiting
        daemon_sleep_ms(100);
        
        // Simulate some request processing
        daemon_increment_requests(ctx);
    }
    
    daemon_set_state(ctx, DAEMON_STATE_STOPPING);
    return DAEMON_SUCCESS;
}

daemon_error_t daemon_shutdown(daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    daemon_error_t result = DAEMON_SUCCESS;
    
    // Request shutdown
    daemon_request_shutdown(ctx);
    
    // Wait for daemon to stop (with timeout)
    int timeout_ms = ctx->config.shutdown_timeout_ms;
    int elapsed_ms = 0;
    const int poll_interval_ms = 100;
    
    while (daemon_is_running(ctx) && elapsed_ms < timeout_ms) {
        daemon_sleep_ms(poll_interval_ms);
        elapsed_ms += poll_interval_ms;
    }
    
    if (daemon_is_running(ctx)) {
        // Force shutdown if timeout exceeded
        result = daemon_force_shutdown(ctx);
    }
    
    // Stop mock server
    if (ctx->server) {
        mock_server_stop(ctx->server);
    }
    
    // Shutdown mock Goxel instance
    if (ctx->goxel_instance) {
        mock_goxel_shutdown(ctx->goxel_instance);
    }
    
    // Remove PID file
    if (ctx->config.create_pid_file) {
        daemon_remove_pid_file(ctx->config.pid_file_path);
    }
    
    // Clean up signal handlers
    daemon_cleanup_signals();
    
    daemon_set_state(ctx, DAEMON_STATE_STOPPED);
    
    return result;
}

daemon_error_t daemon_force_shutdown(daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_CONTEXT;
    
    // Force immediate shutdown
    daemon_request_shutdown(ctx);
    daemon_set_state(ctx, DAEMON_STATE_STOPPED);
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// STATISTICS AND MONITORING
// ============================================================================

daemon_error_t daemon_get_stats(const daemon_context_t *ctx, daemon_stats_t *stats)
{
    if (!ctx || !stats) return DAEMON_ERROR_INVALID_PARAMETER;
    
    memset(stats, 0, sizeof(daemon_stats_t));
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->state_mutex);
    
    stats->state = ctx->state;
    stats->pid = ctx->daemon_pid;
    stats->start_time = ctx->start_time;
    stats->last_activity = ctx->last_activity;
    stats->total_requests = ctx->total_requests;
    stats->total_errors = ctx->total_errors;
    
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->state_mutex);
    
    // Calculate uptime
    int64_t current_time = daemon_get_timestamp();
    if (stats->start_time > 0) {
        stats->uptime = (current_time - stats->start_time) / 1000000; // Convert to seconds
    }
    
    // Mock values for other stats
    stats->memory_usage = 1024 * 1024; // 1MB mock usage
    stats->active_connections = ctx->server && ctx->server->is_running ? 1 : 0;
    
    return DAEMON_SUCCESS;
}

void daemon_update_activity(daemon_context_t *ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->state_mutex);
    ctx->last_activity = daemon_get_timestamp();
    pthread_mutex_unlock(&ctx->state_mutex);
}

void daemon_increment_requests(daemon_context_t *ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->state_mutex);
    ctx->total_requests++;
    pthread_mutex_unlock(&ctx->state_mutex);
}

void daemon_increment_errors(daemon_context_t *ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->state_mutex);
    ctx->total_errors++;
    pthread_mutex_unlock(&ctx->state_mutex);
}

// ============================================================================
// SIGNAL HANDLING SETUP (stub - will be implemented in signal_handling.c)
// ============================================================================

daemon_error_t daemon_setup_signals(daemon_context_t *ctx)
{
    if (!ctx) return DAEMON_ERROR_INVALID_PARAMETER;
    
    // Call the actual implementation in signal_handling.c
    return daemon_setup_signals_impl(ctx);
}

daemon_error_t daemon_cleanup_signals(void)
{
    // Call the actual implementation in signal_handling.c
    return daemon_cleanup_signals_impl();
}