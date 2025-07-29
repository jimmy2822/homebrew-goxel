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

#ifndef DAEMON_LIFECYCLE_H
#define DAEMON_LIFECYCLE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ERROR CODES AND RETURN TYPES  
// ============================================================================

/**
 * Daemon lifecycle error codes.
 */
typedef enum {
    DAEMON_SUCCESS = 0,                     /**< Operation completed successfully */
    DAEMON_ERROR_INVALID_CONTEXT,           /**< Invalid or NULL context */
    DAEMON_ERROR_INVALID_PARAMETER,         /**< Invalid parameter value */
    DAEMON_ERROR_ALREADY_RUNNING,           /**< Daemon already running */
    DAEMON_ERROR_NOT_RUNNING,               /**< Daemon not running */
    DAEMON_ERROR_FORK_FAILED,               /**< Failed to fork daemon process */
    DAEMON_ERROR_SETSID_FAILED,             /**< Failed to create new session */
    DAEMON_ERROR_CHDIR_FAILED,              /**< Failed to change directory */
    DAEMON_ERROR_SIGNAL_SETUP_FAILED,       /**< Failed to setup signal handlers */
    DAEMON_ERROR_PID_FILE_CREATE_FAILED,    /**< Failed to create PID file */
    DAEMON_ERROR_PID_FILE_WRITE_FAILED,     /**< Failed to write PID file */
    DAEMON_ERROR_PID_FILE_REMOVE_FAILED,    /**< Failed to remove PID file */
    DAEMON_ERROR_PID_FILE_INVALID,          /**< Invalid PID file format */
    DAEMON_ERROR_MUTEX_FAILED,              /**< Mutex operation failed */
    DAEMON_ERROR_OUT_OF_MEMORY,             /**< Memory allocation failed */
    DAEMON_ERROR_CONFIG_INVALID,            /**< Configuration file invalid */
    DAEMON_ERROR_CONFIG_NOT_FOUND,          /**< Configuration file not found */
    DAEMON_ERROR_PERMISSION_DENIED,         /**< Permission denied */
    DAEMON_ERROR_SERVER_INIT_FAILED,        /**< Server initialization failed */
    DAEMON_ERROR_GOXEL_INIT_FAILED,         /**< Goxel instance initialization failed */
    DAEMON_ERROR_TIMEOUT,                   /**< Operation timed out */
    DAEMON_ERROR_UNKNOWN = -1               /**< Unknown error */
} daemon_error_t;

/**
 * Daemon state enumeration.
 */
typedef enum {
    DAEMON_STATE_STOPPED,                   /**< Daemon is stopped */
    DAEMON_STATE_STARTING,                  /**< Daemon is starting up */
    DAEMON_STATE_RUNNING,                   /**< Daemon is running normally */
    DAEMON_STATE_STOPPING,                  /**< Daemon is shutting down */
    DAEMON_STATE_ERROR                      /**< Daemon is in error state */
} daemon_state_t;

// ============================================================================
// FORWARD DECLARATIONS AND MOCK INTERFACES
// ============================================================================

/**
 * Mock server interface for independent testing.
 * This represents the socket server without requiring actual implementation.
 */
typedef struct mock_server {
    int mock_socket_fd;                     /**< Mock socket file descriptor */
    bool is_running;                        /**< Mock running state */
    char *socket_path;                      /**< Mock socket path */
    void *user_data;                        /**< Mock user data */
} mock_server_t;

/**
 * Mock Goxel instance interface for independent testing.
 * This represents the Goxel core without requiring actual implementation.
 */
typedef struct mock_goxel_instance {
    bool is_initialized;                    /**< Mock initialization state */
    char *config_file;                      /**< Mock config file path */
    void *render_context;                   /**< Mock render context */
    void *user_data;                        /**< Mock user data */
} mock_goxel_instance_t;

/**
 * Daemon configuration structure.
 */
typedef struct {
    const char *pid_file_path;              /**< PID file path */
    const char *socket_path;                /**< Socket path for server */
    const char *config_file_path;           /**< Configuration file path */
    const char *log_file_path;              /**< Log file path */
    const char *working_directory;          /**< Working directory */
    int max_connections;                    /**< Maximum client connections */
    int startup_timeout_ms;                 /**< Startup timeout in milliseconds */
    int shutdown_timeout_ms;                /**< Shutdown timeout in milliseconds */
    bool daemonize;                         /**< Whether to fork to background */
    bool create_pid_file;                   /**< Whether to create PID file */
    uid_t run_as_uid;                       /**< User ID to run as (0 = no change) */
    gid_t run_as_gid;                       /**< Group ID to run as (0 = no change) */
} daemon_config_t;

/**
 * Main daemon context structure.
 */
typedef struct daemon_context {
    // Core state
    daemon_state_t state;                   /**< Current daemon state */
    pid_t daemon_pid;                       /**< Daemon process PID */
    pthread_mutex_t state_mutex;            /**< State protection mutex */
    bool shutdown_requested;                /**< Shutdown request flag */
    
    // Configuration
    daemon_config_t config;                 /**< Daemon configuration */
    
    // Mock components (for independent testing)
    mock_server_t *server;                  /**< Mock server instance */
    mock_goxel_instance_t *goxel_instance;  /**< Mock Goxel instance */
    
    // Error handling
    daemon_error_t last_error;              /**< Last error code */
    char *last_error_message;               /**< Last error message */
    
    // Timestamps
    int64_t start_time;                     /**< Daemon start timestamp */
    int64_t last_activity;                  /**< Last activity timestamp */
    
    // Statistics
    uint64_t total_requests;                /**< Total requests processed */
    uint64_t total_errors;                  /**< Total errors encountered */
} daemon_context_t;

// ============================================================================
// DAEMON LIFECYCLE MANAGEMENT
// ============================================================================

/**
 * Creates a new daemon context with specified configuration.
 * 
 * @param config Daemon configuration (will be copied)
 * @return New daemon context, or NULL on failure
 */
daemon_context_t *daemon_context_create(const daemon_config_t *config);

/**
 * Destroys a daemon context and frees all resources.
 * Daemon must be stopped before calling this function.
 * 
 * @param ctx Daemon context (may be NULL)
 */
void daemon_context_destroy(daemon_context_t *ctx);

/**
 * Initializes the daemon with the given configuration.
 * This sets up signal handlers, creates directories, validates config.
 * 
 * @param ctx Daemon context
 * @param config_path Optional configuration file path
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_initialize(daemon_context_t *ctx, const char *config_path);

/**
 * Starts the daemon process.
 * This function daemonizes the process (if configured) and starts services.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_start(daemon_context_t *ctx);

/**
 * Initiates graceful daemon shutdown.
 * This function signals the daemon to shut down and waits for completion.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_shutdown(daemon_context_t *ctx);

/**
 * Forces immediate daemon shutdown.
 * This function forcibly terminates the daemon without graceful cleanup.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_force_shutdown(daemon_context_t *ctx);

/**
 * Main daemon run loop.
 * This function runs the daemon until shutdown is requested.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_run(daemon_context_t *ctx);

// ============================================================================
// DAEMON PROCESS MANAGEMENT
// ============================================================================

/**
 * Forks the process into a background daemon.
 * This function implements the standard Unix daemon fork pattern.
 * 
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_daemonize(void);

/**
 * Creates and locks a PID file for the daemon.
 * 
 * @param pid_file_path Path to PID file
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_create_pid_file(const char *pid_file_path);

/**
 * Removes the daemon PID file.
 * 
 * @param pid_file_path Path to PID file
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_remove_pid_file(const char *pid_file_path);

/**
 * Reads PID from PID file.
 * 
 * @param pid_file_path Path to PID file
 * @param pid Pointer to store PID value
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_read_pid_file(const char *pid_file_path, pid_t *pid);

/**
 * Checks if a daemon with the given PID is running.
 * 
 * @param pid Process ID to check
 * @return true if daemon is running, false otherwise
 */
bool daemon_is_process_running(pid_t pid);

/**
 * Changes the daemon's user and group IDs.
 * 
 * @param uid User ID (0 = no change)
 * @param gid Group ID (0 = no change)
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_drop_privileges(uid_t uid, gid_t gid);

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

/**
 * Gets the current daemon state.
 * 
 * @param ctx Daemon context
 * @return Current daemon state
 */
daemon_state_t daemon_get_state(const daemon_context_t *ctx);

/**
 * Sets the daemon state (thread-safe).
 * 
 * @param ctx Daemon context
 * @param state New state
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_set_state(daemon_context_t *ctx, daemon_state_t state);

/**
 * Checks if the daemon is in a running state.
 * 
 * @param ctx Daemon context
 * @return true if running, false otherwise
 */
bool daemon_is_running(const daemon_context_t *ctx);

/**
 * Checks if shutdown has been requested.
 * 
 * @param ctx Daemon context
 * @return true if shutdown requested, false otherwise
 */
bool daemon_shutdown_requested(const daemon_context_t *ctx);

/**
 * Requests daemon shutdown (thread-safe).
 * 
 * @param ctx Daemon context
 */
void daemon_request_shutdown(daemon_context_t *ctx);

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

/**
 * Gets the default daemon configuration.
 * 
 * @return Default configuration structure
 */
daemon_config_t daemon_default_config(void);

/**
 * Loads daemon configuration from file.
 * 
 * @param config_path Configuration file path
 * @param config Configuration structure to populate
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_load_config(const char *config_path, daemon_config_t *config);

/**
 * Validates a daemon configuration.
 * 
 * @param config Configuration to validate
 * @return DAEMON_SUCCESS if valid, error code otherwise
 */
daemon_error_t daemon_validate_config(const daemon_config_t *config);

/**
 * Creates necessary directories for daemon operation.
 * 
 * @param config Daemon configuration
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_create_directories(const daemon_config_t *config);

// ============================================================================
// MOCK INTERFACES (for independent testing)
// ============================================================================

/**
 * Creates a mock server instance.
 * 
 * @param socket_path Socket path for mock server
 * @return New mock server instance, or NULL on failure
 */
mock_server_t *mock_server_create(const char *socket_path);

/**
 * Destroys a mock server instance.
 * 
 * @param server Mock server instance (may be NULL)
 */
void mock_server_destroy(mock_server_t *server);

/**
 * Starts the mock server.
 * 
 * @param server Mock server instance
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t mock_server_start(mock_server_t *server);

/**
 * Stops the mock server.
 * 
 * @param server Mock server instance
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t mock_server_stop(mock_server_t *server);

/**
 * Creates a mock Goxel instance.
 * 
 * @param config_file Configuration file path
 * @return New mock Goxel instance, or NULL on failure
 */
mock_goxel_instance_t *mock_goxel_create(const char *config_file);

/**
 * Destroys a mock Goxel instance.
 * 
 * @param instance Mock Goxel instance (may be NULL)
 */
void mock_goxel_destroy(mock_goxel_instance_t *instance);

/**
 * Initializes the mock Goxel instance.
 * 
 * @param instance Mock Goxel instance
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t mock_goxel_initialize(mock_goxel_instance_t *instance);

/**
 * Shuts down the mock Goxel instance.
 * 
 * @param instance Mock Goxel instance
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t mock_goxel_shutdown(mock_goxel_instance_t *instance);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Gets a human-readable error message for an error code.
 * 
 * @param error Error code
 * @return Pointer to error message string (do not free)
 */
const char *daemon_error_string(daemon_error_t error);

/**
 * Gets the last error from the daemon context.
 * 
 * @param ctx Daemon context
 * @return Last error code
 */
daemon_error_t daemon_get_last_error(const daemon_context_t *ctx);

/**
 * Gets the last error message from the daemon context.
 * 
 * @param ctx Daemon context
 * @return Pointer to error message string (do not free), or NULL if no error
 */
const char *daemon_get_last_error_message(const daemon_context_t *ctx);

/**
 * Sets the last error in the daemon context.
 * 
 * @param ctx Daemon context
 * @param error Error code
 * @param message Error message (will be copied)
 */
void daemon_set_error(daemon_context_t *ctx, daemon_error_t error, const char *message);

// ============================================================================
// STATISTICS AND MONITORING
// ============================================================================

/**
 * Daemon statistics structure.
 */
typedef struct {
    daemon_state_t state;                   /**< Current daemon state */
    pid_t pid;                              /**< Daemon process ID */
    int64_t start_time;                     /**< Daemon start timestamp */
    int64_t uptime;                         /**< Daemon uptime in seconds */
    int64_t last_activity;                  /**< Last activity timestamp */
    uint64_t total_requests;                /**< Total requests processed */
    uint64_t total_errors;                  /**< Total errors encountered */
    size_t memory_usage;                    /**< Current memory usage */
    int active_connections;                 /**< Current active connections */
} daemon_stats_t;

/**
 * Gets daemon statistics.
 * 
 * @param ctx Daemon context
 * @param stats Structure to fill with statistics
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_get_stats(const daemon_context_t *ctx, daemon_stats_t *stats);

/**
 * Updates daemon activity timestamp.
 * 
 * @param ctx Daemon context
 */
void daemon_update_activity(daemon_context_t *ctx);

/**
 * Increments daemon request counter.
 * 
 * @param ctx Daemon context
 */
void daemon_increment_requests(daemon_context_t *ctx);

/**
 * Increments daemon error counter.
 * 
 * @param ctx Daemon context
 */
void daemon_increment_errors(daemon_context_t *ctx);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Gets current timestamp in microseconds.
 * 
 * @return Current timestamp in microseconds
 */
int64_t daemon_get_timestamp(void);

/**
 * Sleeps for the specified number of milliseconds.
 * 
 * @param ms Milliseconds to sleep
 */
void daemon_sleep_ms(int ms);

/**
 * Redirects standard file descriptors to /dev/null.
 * 
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_redirect_stdio(void);

/**
 * Sets up daemon signal handlers.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_setup_signals(daemon_context_t *ctx);

/**
 * Cleans up daemon signal handlers.
 * 
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_cleanup_signals(void);

/**
 * Processes any pending signals in the main thread.
 * This should be called periodically by the daemon main loop.
 * 
 * @param ctx Daemon context
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_process_signals(daemon_context_t *ctx);

/**
 * Checks if any signals are pending without processing them.
 * 
 * @return true if signals are pending, false otherwise
 */
bool daemon_has_pending_signals(void);

/**
 * Resets all signal flags (mainly for testing).
 */
void daemon_reset_signal_flags(void);

// ============================================================================
// SIGNAL UTILITIES
// ============================================================================

/**
 * Sends a signal to a daemon process by PID.
 * 
 * @param pid Process ID to send signal to
 * @param signal Signal number to send
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_send_signal(pid_t pid, int signal);

/**
 * Sends SIGTERM to a daemon process.
 * 
 * @param pid Process ID to send signal to
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_send_shutdown_signal(pid_t pid);

/**
 * Sends SIGHUP to a daemon process.
 * 
 * @param pid Process ID to send signal to
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_send_reload_signal(pid_t pid);

/**
 * Sends SIGKILL to a daemon process (force termination).
 * 
 * @param pid Process ID to send signal to
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_send_kill_signal(pid_t pid);

/**
 * Blocks signals during critical sections.
 * 
 * @param old_mask Pointer to store old signal mask
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_block_signals(sigset_t *old_mask);

/**
 * Unblocks signals after critical sections.
 * 
 * @param old_mask Pointer to old signal mask to restore
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_unblock_signals(const sigset_t *old_mask);

/**
 * Waits for a specific signal with timeout.
 * 
 * @param signal Signal number to wait for
 * @param timeout_ms Timeout in milliseconds
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_wait_for_signal(int signal, int timeout_ms);

/**
 * Checks if signal handlers are properly installed.
 * 
 * @return true if signal handlers are installed, false otherwise
 */
bool daemon_signals_installed(void);

/**
 * Tests signal handling by sending a signal to self.
 * 
 * @param ctx Daemon context
 * @param signal Signal number to test
 * @return DAEMON_SUCCESS on success, error code on failure
 */
daemon_error_t daemon_test_signal_handling(daemon_context_t *ctx, int signal);

/**
 * Gets a human-readable name for a signal.
 * 
 * @param signal Signal number
 * @return Pointer to signal name string (do not free)
 */
const char *daemon_signal_name(int signal);

#ifdef __cplusplus
}
#endif

#endif // DAEMON_LIFECYCLE_H