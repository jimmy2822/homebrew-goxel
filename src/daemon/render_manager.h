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

#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include "../../ext_src/uthash/uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FORWARD DECLARATIONS AND TYPES
// ============================================================================

/**
 * Render information structure for tracking individual renders.
 */
typedef struct render_info {
    char *file_path;                    /**< Full file path to rendered file */
    char *session_id;                   /**< Session ID for tracking */
    char *format;                       /**< File format (png, jpg, etc) */
    size_t file_size;                   /**< File size in bytes */
    time_t created_at;                  /**< Creation timestamp */
    time_t expires_at;                  /**< Expiration timestamp */
    char *checksum;                     /**< File checksum for integrity */
    int width;                          /**< Image width */
    int height;                         /**< Image height */
    
    // Hash table linkage (using uthash)
    char path_key[256];                 /**< Hash table key (file path) */
    UT_hash_handle hh;                  /**< uthash handle */
} render_info_t;

// Forward declaration for cleanup thread
typedef struct render_cleanup_thread render_cleanup_thread_t;

/**
 * Main render manager structure.
 * Manages temporary render files with automatic cleanup and resource limits.
 */
typedef struct render_manager {
    char *output_dir;                   /**< Directory for temporary renders */
    size_t max_cache_size;              /**< Maximum cache size in bytes */
    int ttl_seconds;                    /**< Time to live for files in seconds */
    render_info_t *active_renders;      /**< Hash table of active renders */
    
    // Thread safety
    pthread_mutex_t mutex;              /**< Mutex for thread-safe operations */
    
    // Statistics
    uint64_t total_renders;             /**< Total renders created */
    uint64_t total_cleanups;            /**< Total cleanup operations */
    size_t current_cache_size;          /**< Current cache size in bytes */
    int active_count;                   /**< Current number of active renders */
    uint64_t files_cleaned;             /**< Total files cleaned up */
    size_t bytes_freed;                 /**< Total bytes freed by cleanup */
    uint64_t security_violations;       /**< Security violation attempts */
    
    // Auto cleanup
    render_cleanup_thread_t *cleanup_thread; /**< Background cleanup thread */
} render_manager_t;

/**
 * Render manager error codes.
 */
typedef enum {
    RENDER_MGR_SUCCESS = 0,             /**< Operation successful */
    RENDER_MGR_ERROR_NULL_POINTER,      /**< NULL pointer provided */
    RENDER_MGR_ERROR_INVALID_PARAMETER, /**< Invalid parameter value */
    RENDER_MGR_ERROR_OUT_OF_MEMORY,     /**< Memory allocation failed */
    RENDER_MGR_ERROR_FILE_EXISTS,       /**< File already exists */
    RENDER_MGR_ERROR_FILE_NOT_FOUND,    /**< File not found */
    RENDER_MGR_ERROR_PERMISSION_DENIED, /**< Permission denied */
    RENDER_MGR_ERROR_DISK_FULL,         /**< Disk full */
    RENDER_MGR_ERROR_IO_ERROR,          /**< I/O error */
    RENDER_MGR_ERROR_MUTEX_ERROR,       /**< Mutex operation failed */
    RENDER_MGR_ERROR_PATH_TOO_LONG,     /**< Path too long */
    RENDER_MGR_ERROR_CACHE_FULL,        /**< Cache size limit exceeded */
    RENDER_MGR_ERROR_UNKNOWN = -1       /**< Unknown error */
} render_manager_error_t;

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

/**
 * Default configuration values.
 */
#define RENDER_MANAGER_DEFAULT_TTL_SECONDS      3600    /**< 1 hour TTL */
#define RENDER_MANAGER_DEFAULT_MAX_CACHE_SIZE   (1024 * 1024 * 1024)  /**< 1GB */
#define RENDER_MANAGER_DEFAULT_CLEANUP_INTERVAL 300     /**< 5 minutes */
#define RENDER_MANAGER_MAX_PATH_LENGTH          4096    /**< Maximum path length */
#define RENDER_MANAGER_MAX_SESSION_ID_LENGTH    64      /**< Maximum session ID length */
#define RENDER_MANAGER_MAX_FORMAT_LENGTH        16      /**< Maximum format string length */
#define RENDER_MANAGER_MAX_FILE_SIZE           (100 * 1024 * 1024)  /**< 100MB per file */
#define RENDER_MANAGER_MAX_CONCURRENT_RENDERS   100     /**< Max concurrent renders */

/**
 * Environment variable names for configuration.
 */
#define RENDER_MANAGER_ENV_DIR                  "GOXEL_RENDER_DIR"
#define RENDER_MANAGER_ENV_TTL                  "GOXEL_RENDER_TTL"
#define RENDER_MANAGER_ENV_MAX_SIZE             "GOXEL_RENDER_MAX_SIZE"
#define RENDER_MANAGER_ENV_CLEANUP_INTERVAL     "GOXEL_RENDER_CLEANUP_INTERVAL"

/**
 * Platform-specific default directories.
 */
#ifdef __APPLE__
#define RENDER_MANAGER_DEFAULT_DIR "/var/tmp/goxel_renders"
#elif defined(__linux__)
#define RENDER_MANAGER_DEFAULT_DIR "/tmp/goxel_renders"
#elif defined(_WIN32)
#define RENDER_MANAGER_DEFAULT_DIR "%TEMP%\\goxel_renders"
#else
#define RENDER_MANAGER_DEFAULT_DIR "/tmp/goxel_renders"
#endif

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

/**
 * Configuration structure for render manager creation.
 */
typedef struct {
    const char *output_dir;                 /**< Directory for temporary files (NULL for default) */
    size_t max_cache_size;                  /**< Maximum cache size in bytes (0 for default) */
    int ttl_seconds;                        /**< Time to live for files in seconds (0 for default) */
    int cleanup_interval_seconds;           /**< Cleanup interval in seconds (0 for default) */
    bool enable_auto_cleanup;               /**< Whether to start cleanup thread automatically */
    bool use_env_config;                    /**< Whether to read from environment variables */
} render_manager_config_t;

/**
 * Creates a new render manager instance with configuration.
 * 
 * @param config Configuration structure (NULL for all defaults)
 * @return New render manager instance or NULL on failure
 */
render_manager_t *render_manager_create_with_config(const render_manager_config_t *config);

/**
 * Creates a new render manager instance (legacy interface).
 * 
 * @param output_dir Directory for temporary files (NULL for default)
 * @param max_cache_size Maximum cache size in bytes (0 for default)
 * @param ttl_seconds Time to live for files in seconds (0 for default)
 * @return New render manager instance or NULL on failure
 */
render_manager_t *render_manager_create(const char *output_dir, 
                                       size_t max_cache_size, 
                                       int ttl_seconds);

/**
 * Destroys a render manager instance and cleans up all resources.
 * Optionally removes all managed files.
 * 
 * @param rm Render manager instance (may be NULL)
 * @param cleanup_files Whether to remove all managed files
 */
void render_manager_destroy(render_manager_t *rm, bool cleanup_files);

/**
 * Generates a unique file path for a new render.
 * Path format: render_[timestamp]_[session_id]_[hash].[format]
 * 
 * @param rm Render manager instance
 * @param session_id Session identifier (NULL for auto-generated)
 * @param format File format extension (e.g., "png", "jpg")
 * @param path_out Buffer to store generated path
 * @param path_size Size of path buffer
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_create_path(render_manager_t *rm,
                                                 const char *session_id,
                                                 const char *format,
                                                 char *path_out,
                                                 size_t path_size);

/**
 * Registers a new render in the manager's tracking system.
 * This should be called after successfully creating a render file.
 * 
 * @param rm Render manager instance
 * @param file_path Path to the created file
 * @param session_id Session identifier
 * @param format File format
 * @param width Image width
 * @param height Image height
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_register(render_manager_t *rm,
                                              const char *file_path,
                                              const char *session_id,
                                              const char *format,
                                              int width,
                                              int height);

/**
 * Removes expired render files from the filesystem and tracking system.
 * This function is thread-safe and can be called from background threads.
 * 
 * @param rm Render manager instance
 * @param removed_count Pointer to store count of removed files (may be NULL)
 * @param freed_bytes Pointer to store bytes freed (may be NULL)
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_cleanup_expired(render_manager_t *rm,
                                                     int *removed_count,
                                                     size_t *freed_bytes);

/**
 * Forces cleanup to meet cache size limits.
 * Removes oldest files first until under the limit.
 * 
 * @param rm Render manager instance
 * @param removed_count Pointer to store count of removed files (may be NULL)
 * @param freed_bytes Pointer to store bytes freed (may be NULL)
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_enforce_cache_limit(render_manager_t *rm,
                                                        int *removed_count,
                                                        size_t *freed_bytes);

// ============================================================================
// QUERY AND MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Gets information about a specific render by file path.
 * 
 * @param rm Render manager instance
 * @param file_path Path to the render file
 * @param info_out Pointer to store render information (may be NULL)
 * @return RENDER_MGR_SUCCESS if found, error code otherwise
 */
render_manager_error_t render_manager_get_render_info(render_manager_t *rm,
                                                     const char *file_path,
                                                     render_info_t **info_out);

/**
 * Removes a specific render from tracking and filesystem.
 * 
 * @param rm Render manager instance
 * @param file_path Path to the render file to remove
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_remove_render(render_manager_t *rm,
                                                   const char *file_path);

/**
 * Lists all active renders.
 * Caller must free the returned array with free().
 * 
 * @param rm Render manager instance
 * @param renders_out Pointer to store array of render info pointers
 * @param count_out Pointer to store array size
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_list_renders(render_manager_t *rm,
                                                  render_info_t ***renders_out,
                                                  int *count_out);

/**
 * Gets render manager statistics.
 */
typedef struct {
    uint64_t total_renders;             /**< Total renders created */
    uint64_t total_cleanups;            /**< Total cleanup operations */
    size_t current_cache_size;          /**< Current cache size in bytes */
    int active_count;                   /**< Current number of active renders */
    size_t max_cache_size;              /**< Maximum cache size limit */
    int ttl_seconds;                    /**< Current TTL setting */
    char *output_dir;                   /**< Current output directory */
    uint64_t files_cleaned;             /**< Total files cleaned up */
    size_t bytes_freed;                 /**< Total bytes freed by cleanup */
    uint64_t security_violations;       /**< Security violation attempts */
    bool cleanup_thread_active;        /**< Whether cleanup thread is running */
} render_manager_stats_t;

/**
 * Gets current render manager statistics.
 * 
 * @param rm Render manager instance
 * @param stats_out Statistics structure to fill
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_get_stats(render_manager_t *rm,
                                               render_manager_stats_t *stats_out);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Creates the output directory if it doesn't exist.
 * Sets appropriate permissions (0755).
 * 
 * @param dir_path Directory path to create
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_create_directory(const char *dir_path);

/**
 * Generates a secure random token for file names.
 * 
 * @param token_out Buffer to store token
 * @param token_size Size of token buffer
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_generate_token(char *token_out, 
                                                    size_t token_size);

/**
 * Calculates SHA256 checksum of a file.
 * 
 * @param file_path Path to file
 * @param checksum_out Buffer to store hex checksum string
 * @param checksum_size Size of checksum buffer (should be at least 65 bytes)
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_calculate_checksum(const char *file_path,
                                                        char *checksum_out,
                                                        size_t checksum_size);

/**
 * Validates a file path to prevent directory traversal attacks.
 * 
 * @param file_path File path to validate
 * @param base_dir Base directory that path must be within
 * @return true if path is safe, false otherwise
 */
bool render_manager_validate_path(const char *file_path, const char *base_dir);

/**
 * Sets secure file permissions (0600) on a file.
 * 
 * @param file_path Path to file
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_set_secure_permissions(const char *file_path);

/**
 * Validates file ownership before deletion.
 * 
 * @param file_path Path to file
 * @return true if file can be safely deleted, false otherwise
 */
bool render_manager_validate_ownership(const char *file_path);

/**
 * Gets human-readable error message for error code.
 * 
 * @param error Error code
 * @return Error message string (do not free)
 */
const char *render_manager_error_string(render_manager_error_t error);

// ============================================================================
// BACKGROUND CLEANUP SUPPORT
// ============================================================================

/**
 * Structure for background cleanup thread configuration.
 */
struct render_cleanup_thread {
    render_manager_t *rm;               /**< Render manager instance */
    int cleanup_interval_seconds;       /**< Cleanup interval in seconds */
    bool stop_requested;                /**< Stop flag for thread */
    pthread_t thread_id;                /**< Thread ID */
    pthread_mutex_t stop_mutex;         /**< Mutex for stop flag */
    bool is_running;                    /**< Whether thread is currently running */
    uint64_t cleanup_cycles;            /**< Number of cleanup cycles completed */
    time_t last_cleanup_time;           /**< Timestamp of last cleanup */
};

/**
 * Gets the default configuration with environment variable overrides.
 * 
 * @param config_out Configuration structure to fill
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_get_default_config(render_manager_config_t *config_out);

/**
 * Starts background cleanup thread.
 * 
 * @param rm Render manager instance
 * @param cleanup_interval_seconds Cleanup interval (0 for default)
 * @return Cleanup thread context or NULL on failure
 */
render_cleanup_thread_t *render_manager_start_cleanup_thread(render_manager_t *rm,
                                                           int cleanup_interval_seconds);

/**
 * Stops background cleanup thread.
 * 
 * @param cleanup_thread Cleanup thread context (may be NULL)
 */
void render_manager_stop_cleanup_thread(render_cleanup_thread_t *cleanup_thread);

/**
 * Enables or disables automatic cleanup for a render manager.
 * If enabled, starts the cleanup thread. If disabled, stops it.
 * 
 * @param rm Render manager instance
 * @param enabled Whether to enable automatic cleanup
 * @param cleanup_interval_seconds Cleanup interval (0 for default, ignored if disabled)
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_set_auto_cleanup(render_manager_t *rm,
                                                      bool enabled,
                                                      int cleanup_interval_seconds);

/**
 * Configuration parsing utilities
 */

/**
 * Parses size string with unit suffixes (B, KB, MB, GB).
 * Examples: "1GB", "512MB", "1024"
 * 
 * @param size_str Size string to parse
 * @param size_out Parsed size in bytes
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_parse_size(const char *size_str, size_t *size_out);

/**
 * Parses time string with unit suffixes (s, m, h, d).
 * Examples: "1h", "30m", "3600"
 * 
 * @param time_str Time string to parse
 * @param seconds_out Parsed time in seconds
 * @return RENDER_MGR_SUCCESS on success, error code on failure
 */
render_manager_error_t render_manager_parse_time(const char *time_str, int *seconds_out);

#ifdef __cplusplus
}
#endif

#endif // RENDER_MANAGER_H