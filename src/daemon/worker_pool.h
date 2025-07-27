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

#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declarations
typedef struct worker_pool worker_pool_t;
typedef struct worker_request worker_request_t;
typedef struct worker_stats worker_stats_t;

// ============================================================================
// TYPES AND ENUMS
// ============================================================================

/**
 * Worker pool error codes.
 */
typedef enum {
    WORKER_POOL_SUCCESS = 0,
    WORKER_POOL_ERROR_INVALID_PARAMETER,
    WORKER_POOL_ERROR_OUT_OF_MEMORY,
    WORKER_POOL_ERROR_THREAD_CREATE_FAILED,
    WORKER_POOL_ERROR_MUTEX_FAILED,
    WORKER_POOL_ERROR_ALREADY_STARTED,
    WORKER_POOL_ERROR_NOT_STARTED,
    WORKER_POOL_ERROR_QUEUE_FULL,
    WORKER_POOL_ERROR_SHUTDOWN_TIMEOUT,
    WORKER_POOL_ERROR_UNKNOWN
} worker_pool_error_t;

/**
 * Worker request priority levels.
 */
typedef enum {
    WORKER_PRIORITY_LOW = 0,
    WORKER_PRIORITY_NORMAL = 1,
    WORKER_PRIORITY_HIGH = 2,
    WORKER_PRIORITY_CRITICAL = 3
} worker_priority_t;

/**
 * Worker request processing function.
 * 
 * @param request_data User data passed with the request
 * @param worker_id ID of the worker thread processing the request
 * @param context User-defined context passed during pool creation
 * @return Processing result (0 = success, non-zero = error)
 */
typedef int (*worker_process_func_t)(void *request_data, int worker_id, void *context);

/**
 * Worker request cleanup function.
 * 
 * @param request_data User data to cleanup
 */
typedef void (*worker_cleanup_func_t)(void *request_data);

/**
 * Worker pool configuration.
 */
typedef struct {
    int worker_count;                    /**< Number of worker threads (1-64) */
    int queue_capacity;                  /**< Maximum queued requests (1-65536) */
    int shutdown_timeout_ms;             /**< Shutdown timeout in milliseconds */
    bool enable_priority_queue;          /**< Enable priority-based processing */
    bool enable_statistics;              /**< Enable performance statistics */
    worker_process_func_t process_func;  /**< Request processing function */
    worker_cleanup_func_t cleanup_func;  /**< Request cleanup function */
    void *context;                       /**< User-defined context */
} worker_pool_config_t;

/**
 * Worker pool statistics.
 */
struct worker_stats {
    uint64_t requests_processed;         /**< Total requests processed */
    uint64_t requests_failed;            /**< Total requests that failed */
    uint64_t requests_queued;            /**< Current requests in queue */
    uint64_t requests_dropped;           /**< Requests dropped due to full queue */
    uint64_t total_processing_time_us;   /**< Total processing time in microseconds */
    uint64_t average_processing_time_us; /**< Average processing time in microseconds */
    uint64_t max_processing_time_us;     /**< Maximum processing time in microseconds */
    uint64_t min_processing_time_us;     /**< Minimum processing time in microseconds */
    int active_workers;                  /**< Currently active worker threads */
    int idle_workers;                    /**< Currently idle worker threads */
    int64_t uptime_us;                   /**< Pool uptime in microseconds */
};

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

/**
 * Get default worker pool configuration.
 * 
 * @return Default configuration structure
 */
worker_pool_config_t worker_pool_default_config(void);

/**
 * Validate worker pool configuration.
 * 
 * @param config Configuration to validate
 * @return WORKER_POOL_SUCCESS if valid, error code otherwise
 */
worker_pool_error_t worker_pool_validate_config(const worker_pool_config_t *config);

// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

/**
 * Create a new worker pool.
 * 
 * @param config Pool configuration
 * @return Worker pool instance or NULL on error
 */
worker_pool_t *worker_pool_create(const worker_pool_config_t *config);

/**
 * Start the worker pool.
 * Creates and starts all worker threads.
 * 
 * @param pool Worker pool instance
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_start(worker_pool_t *pool);

/**
 * Stop the worker pool.
 * Gracefully shuts down all worker threads.
 * 
 * @param pool Worker pool instance
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_stop(worker_pool_t *pool);

/**
 * Destroy the worker pool.
 * Frees all resources associated with the pool.
 * 
 * @param pool Worker pool instance
 */
void worker_pool_destroy(worker_pool_t *pool);

/**
 * Check if worker pool is running.
 * 
 * @param pool Worker pool instance
 * @return true if running, false otherwise
 */
bool worker_pool_is_running(const worker_pool_t *pool);

// ============================================================================
// REQUEST SUBMISSION FUNCTIONS
// ============================================================================

/**
 * Submit a request to the worker pool.
 * 
 * @param pool Worker pool instance
 * @param request_data User data for the request
 * @param priority Request priority level
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_submit_request(worker_pool_t *pool, 
                                              void *request_data,
                                              worker_priority_t priority);

/**
 * Submit a request with timeout.
 * 
 * @param pool Worker pool instance
 * @param request_data User data for the request
 * @param priority Request priority level
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_submit_request_timeout(worker_pool_t *pool,
                                                      void *request_data,
                                                      worker_priority_t priority,
                                                      int timeout_ms);

/**
 * Get current queue size.
 * 
 * @param pool Worker pool instance
 * @return Number of requests currently in queue, -1 on error
 */
int worker_pool_get_queue_size(const worker_pool_t *pool);

/**
 * Check if queue is full.
 * 
 * @param pool Worker pool instance
 * @return true if queue is full, false otherwise
 */
bool worker_pool_is_queue_full(const worker_pool_t *pool);

// ============================================================================
// STATISTICS AND MONITORING FUNCTIONS
// ============================================================================

/**
 * Get worker pool statistics.
 * 
 * @param pool Worker pool instance
 * @param stats Output statistics structure
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_get_stats(const worker_pool_t *pool,
                                         worker_stats_t *stats);

/**
 * Reset worker pool statistics.
 * 
 * @param pool Worker pool instance
 * @return WORKER_POOL_SUCCESS on success, error code on failure
 */
worker_pool_error_t worker_pool_reset_stats(worker_pool_t *pool);

/**
 * Get worker pool capacity.
 * 
 * @param pool Worker pool instance
 * @return Queue capacity, -1 on error
 */
int worker_pool_get_capacity(const worker_pool_t *pool);

/**
 * Get number of worker threads.
 * 
 * @param pool Worker pool instance
 * @return Number of worker threads, -1 on error
 */
int worker_pool_get_worker_count(const worker_pool_t *pool);

// ============================================================================
// ERROR HANDLING FUNCTIONS
// ============================================================================

/**
 * Convert error code to human-readable string.
 * 
 * @param error Error code
 * @return Error description string
 */
const char *worker_pool_error_string(worker_pool_error_t error);

/**
 * Get last error message from worker pool.
 * 
 * @param pool Worker pool instance
 * @return Last error message or NULL if no error
 */
const char *worker_pool_get_last_error(const worker_pool_t *pool);

#endif // WORKER_POOL_H