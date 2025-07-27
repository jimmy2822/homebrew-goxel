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

#ifndef DAEMON_CONFIG_SCHEMA_H
#define DAEMON_CONFIG_SCHEMA_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DEFAULT CONFIGURATION VALUES
// ============================================================================

// Process management defaults
#define DEFAULT_PID_FILE            "/var/run/goxel/goxel.pid"
#define DEFAULT_WORKING_DIRECTORY   "/var/lib/goxel"
#define DEFAULT_USER                "goxel"
#define DEFAULT_GROUP               "goxel"

// Socket defaults
#define DEFAULT_UNIX_SOCKET_PATH    "/tmp/goxel.sock"
#define DEFAULT_UNIX_SOCKET_PERMS   0666
#define DEFAULT_TCP_BIND_ADDRESS    "127.0.0.1"
#define DEFAULT_TCP_PORT            7890
#define DEFAULT_SOCKET_BACKLOG      128

// Worker configuration defaults
#define DEFAULT_WORKER_COUNT        4
#define DEFAULT_QUEUE_SIZE          1000
#define DEFAULT_STACK_SIZE          (2 * 1024 * 1024)  // 2MB per thread

// Performance defaults
#define DEFAULT_MAX_CONNECTIONS     10
#define DEFAULT_REQUEST_TIMEOUT_MS  30000   // 30 seconds
#define DEFAULT_SHUTDOWN_TIMEOUT_MS 10000   // 10 seconds
#define DEFAULT_BUFFER_SIZE         65536   // 64KB
#define DEFAULT_MAX_MESSAGE_SIZE    (10 * 1024 * 1024)  // 10MB

// Health monitoring defaults
#define DEFAULT_HEALTH_CHECK_INTERVAL_MS    5000    // 5 seconds
#define DEFAULT_RESTART_ON_FAILURE          true
#define DEFAULT_MAX_RESTART_ATTEMPTS        3
#define DEFAULT_RESTART_DELAY_MS            5000    // 5 seconds
#define DEFAULT_HEARTBEAT_TIMEOUT_MS        30000   // 30 seconds

// Logging defaults
#define DEFAULT_LOG_LEVEL           "info"
#define DEFAULT_LOG_FILE            "/var/log/goxel/daemon.log"
#define DEFAULT_LOG_MAX_SIZE_MB     100
#define DEFAULT_LOG_MAX_FILES       10
#define DEFAULT_LOG_USE_SYSLOG      true
#define DEFAULT_LOG_USE_COLORS      false

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

/**
 * Process management configuration.
 */
typedef struct {
    char *pid_file;                 /**< PID file path */
    char *working_directory;        /**< Working directory for daemon */
    char *user;                     /**< User to run as */
    char *group;                    /**< Group to run as */
    uid_t uid;                      /**< Resolved user ID */
    gid_t gid;                      /**< Resolved group ID */
    bool daemonize;                 /**< Fork to background */
    bool create_pid_file;           /**< Create PID file */
    int nice_level;                 /**< Process nice level (-20 to 19) */
} daemon_process_config_t;

/**
 * Unix domain socket configuration.
 */
typedef struct {
    bool enabled;                   /**< Enable Unix socket */
    char *path;                     /**< Socket file path */
    mode_t permissions;             /**< Socket file permissions */
    bool unlink_existing;           /**< Remove existing socket file */
    int backlog;                    /**< Listen backlog */
} daemon_unix_socket_config_t;

/**
 * TCP socket configuration.
 */
typedef struct {
    bool enabled;                   /**< Enable TCP socket */
    char *bind_address;             /**< Bind address (IP or hostname) */
    uint16_t port;                  /**< TCP port number */
    int backlog;                    /**< Listen backlog */
    bool nodelay;                   /**< TCP_NODELAY option */
    bool keepalive;                 /**< SO_KEEPALIVE option */
    int keepalive_idle;             /**< TCP_KEEPIDLE seconds */
    int keepalive_interval;         /**< TCP_KEEPINTVL seconds */
    int keepalive_count;            /**< TCP_KEEPCNT probes */
} daemon_tcp_socket_config_t;

/**
 * Socket configuration container.
 */
typedef struct {
    daemon_unix_socket_config_t unix;   /**< Unix socket config */
    daemon_tcp_socket_config_t tcp;     /**< TCP socket config */
    int receive_timeout_ms;             /**< SO_RCVTIMEO in milliseconds */
    int send_timeout_ms;                /**< SO_SNDTIMEO in milliseconds */
    size_t receive_buffer_size;         /**< SO_RCVBUF size */
    size_t send_buffer_size;            /**< SO_SNDBUF size */
} daemon_socket_config_t;

/**
 * Worker thread configuration.
 */
typedef struct {
    int count;                      /**< Number of worker threads */
    size_t queue_size;              /**< Request queue size */
    size_t stack_size;              /**< Thread stack size */
    int priority;                   /**< Thread priority */
    bool pin_to_cpu;                /**< Pin threads to CPUs */
    cpu_set_t *cpu_affinity;        /**< CPU affinity mask */
} daemon_worker_config_t;

/**
 * Performance tuning configuration.
 */
typedef struct {
    int max_connections;            /**< Maximum concurrent connections */
    int request_timeout_ms;         /**< Request processing timeout */
    int shutdown_timeout_ms;        /**< Graceful shutdown timeout */
    size_t buffer_size;             /**< I/O buffer size */
    size_t max_message_size;        /**< Maximum message size */
    bool use_splice;                /**< Use splice() for zero-copy */
    bool tcp_cork;                  /**< Use TCP_CORK for batching */
    int io_threads;                 /**< Dedicated I/O threads */
} daemon_performance_config_t;

/**
 * Health monitoring configuration.
 */
typedef struct {
    int check_interval_ms;          /**< Health check interval */
    bool restart_on_failure;        /**< Auto-restart on failure */
    int max_restart_attempts;       /**< Maximum restart attempts */
    int restart_delay_ms;           /**< Delay between restarts */
    int heartbeat_timeout_ms;       /**< Heartbeat timeout */
    char *health_check_script;      /**< External health check script */
    bool enable_watchdog;           /**< Enable system watchdog */
    int watchdog_interval_ms;       /**< Watchdog ping interval */
} daemon_health_config_t;

/**
 * Logging configuration.
 */
typedef struct {
    char *level;                    /**< Log level string */
    int numeric_level;              /**< Resolved numeric level */
    char *file;                     /**< Log file path */
    int max_size_mb;                /**< Maximum log file size */
    int max_files;                  /**< Maximum rotated files */
    bool use_syslog;                /**< Send to syslog */
    bool use_colors;                /**< Use ANSI colors */
    bool log_to_stderr;             /**< Also log to stderr */
    char *syslog_facility;          /**< Syslog facility */
    char *syslog_ident;             /**< Syslog identifier */
} daemon_logging_config_t;

/**
 * Security configuration.
 */
typedef struct {
    bool enable_chroot;             /**< Chroot to working directory */
    char *chroot_directory;         /**< Chroot directory path */
    bool drop_capabilities;         /**< Drop Linux capabilities */
    char **allowed_capabilities;    /**< List of capabilities to keep */
    mode_t umask;                   /**< File creation mask */
    bool enable_seccomp;            /**< Enable seccomp filtering */
    char *seccomp_profile;          /**< Seccomp profile path */
} daemon_security_config_t;

/**
 * Resource limits configuration.
 */
typedef struct {
    int64_t max_memory_mb;          /**< Maximum memory usage (MB) */
    int max_file_descriptors;       /**< Maximum open files */
    int64_t max_core_size_mb;       /**< Maximum core dump size */
    int max_processes;              /**< Maximum processes */
    int64_t max_cpu_time_sec;       /**< Maximum CPU time */
    int scheduling_priority;        /**< Scheduling priority */
} daemon_limits_config_t;

/**
 * Complete daemon configuration.
 */
typedef struct {
    // Core sections
    daemon_process_config_t process;        /**< Process management */
    daemon_socket_config_t sockets;         /**< Socket configuration */
    daemon_worker_config_t workers;         /**< Worker threads */
    daemon_performance_config_t performance; /**< Performance tuning */
    daemon_health_config_t health;          /**< Health monitoring */
    daemon_logging_config_t logging;        /**< Logging settings */
    daemon_security_config_t security;      /**< Security settings */
    daemon_limits_config_t limits;          /**< Resource limits */
    
    // Configuration metadata
    char *config_file_path;                 /**< Source config file */
    time_t config_load_time;                /**< When config was loaded */
    uint32_t config_version;                /**< Config format version */
    
    // Runtime flags
    bool debug_mode;                        /**< Enable debug features */
    bool test_mode;                         /**< Test mode (no fork) */
    bool validate_only;                     /**< Only validate config */
} daemon_full_config_t;

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

/**
 * Creates a default daemon configuration.
 * 
 * @return New configuration with default values
 */
daemon_full_config_t* daemon_config_create_default(void);

/**
 * Loads daemon configuration from file.
 * 
 * @param path Configuration file path
 * @return Loaded configuration, or NULL on error
 */
daemon_full_config_t* daemon_config_load_file(const char *path);

/**
 * Loads daemon configuration from JSON string.
 * 
 * @param json JSON configuration string
 * @return Loaded configuration, or NULL on error
 */
daemon_full_config_t* daemon_config_load_json(const char *json);

/**
 * Validates a daemon configuration.
 * 
 * @param config Configuration to validate
 * @param errors Buffer for error messages (optional)
 * @param errors_size Size of error buffer
 * @return true if valid, false otherwise
 */
bool daemon_config_validate(const daemon_full_config_t *config,
                           char *errors, size_t errors_size);

/**
 * Merges two configurations (overlay over base).
 * 
 * @param base Base configuration
 * @param overlay Configuration to overlay
 * @return Merged configuration (new allocation)
 */
daemon_full_config_t* daemon_config_merge(const daemon_full_config_t *base,
                                         const daemon_full_config_t *overlay);

/**
 * Saves configuration to file.
 * 
 * @param config Configuration to save
 * @param path Output file path
 * @return true on success, false on error
 */
bool daemon_config_save_file(const daemon_full_config_t *config,
                            const char *path);

/**
 * Converts configuration to JSON string.
 * 
 * @param config Configuration to convert
 * @return JSON string (caller must free), or NULL on error
 */
char* daemon_config_to_json(const daemon_full_config_t *config);

/**
 * Frees a daemon configuration.
 * 
 * @param config Configuration to free (may be NULL)
 */
void daemon_config_free(daemon_full_config_t *config);

/**
 * Resolves user/group names to IDs.
 * 
 * @param config Configuration to resolve
 * @return true on success, false on error
 */
bool daemon_config_resolve_ids(daemon_full_config_t *config);

/**
 * Expands environment variables in configuration.
 * 
 * @param config Configuration to expand
 * @return true on success, false on error
 */
bool daemon_config_expand_vars(daemon_full_config_t *config);

/**
 * Gets a configuration value by path.
 * 
 * @param config Configuration to query
 * @param path Dot-separated path (e.g., "sockets.unix.path")
 * @return Value string, or NULL if not found
 */
const char* daemon_config_get(const daemon_full_config_t *config,
                             const char *path);

/**
 * Sets a configuration value by path.
 * 
 * @param config Configuration to modify
 * @param path Dot-separated path
 * @param value New value
 * @return true on success, false on error
 */
bool daemon_config_set(daemon_full_config_t *config,
                      const char *path, const char *value);

/**
 * Reloads configuration from file (for SIGHUP).
 * 
 * @param config Current configuration
 * @param path Configuration file path
 * @return New configuration, or NULL on error
 */
daemon_full_config_t* daemon_config_reload(const daemon_full_config_t *config,
                                          const char *path);

/**
 * Gets configuration differences.
 * 
 * @param old_config Previous configuration
 * @param new_config New configuration
 * @return List of changed paths (caller must free)
 */
char** daemon_config_diff(const daemon_full_config_t *old_config,
                         const daemon_full_config_t *new_config);

#ifdef __cplusplus
}
#endif

#endif // DAEMON_CONFIG_SCHEMA_H