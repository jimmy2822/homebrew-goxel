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

#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include "json_rpc.h"
#include "socket_server.h"
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declarations
typedef struct request_queue request_queue_t;
typedef struct queued_request queued_request_t;

// ============================================================================
// TYPES AND ENUMS
// ============================================================================

/**
 * Request queue error codes.
 */
typedef enum {
    REQUEST_QUEUE_SUCCESS = 0,
    REQUEST_QUEUE_ERROR_INVALID_PARAMETER,
    REQUEST_QUEUE_ERROR_OUT_OF_MEMORY,
    REQUEST_QUEUE_ERROR_QUEUE_FULL,
    REQUEST_QUEUE_ERROR_QUEUE_EMPTY,
    REQUEST_QUEUE_ERROR_MUTEX_FAILED,
    REQUEST_QUEUE_ERROR_TIMEOUT,
    REQUEST_QUEUE_ERROR_NOT_INITIALIZED,
    REQUEST_QUEUE_ERROR_UNKNOWN
} request_queue_error_t;

/**
 * Request priority levels (higher number = higher priority).
 */
typedef enum {
    REQUEST_PRIORITY_LOW = 0,
    REQUEST_PRIORITY_NORMAL = 1,
    REQUEST_PRIORITY_HIGH = 2,
    REQUEST_PRIORITY_CRITICAL = 3
} request_priority_t;

/**
 * Request processing status.
 */
typedef enum {
    REQUEST_STATUS_QUEUED = 0,
    REQUEST_STATUS_PROCESSING,
    REQUEST_STATUS_COMPLETED,
    REQUEST_STATUS_FAILED,
    REQUEST_STATUS_TIMEOUT
} request_status_t;

/**
 * Queued request structure.
 */
struct queued_request {
    // Request identification
    uint32_t request_id;                 /**< Unique request ID */
    
    // Client and message data
    socket_client_t *client;             /**< Client that sent the request */
    json_rpc_request_t *rpc_request;     /**< JSON-RPC request data */
    
    // Queue management
    request_priority_t priority;         /**< Request priority */
    request_status_t status;             /**< Current processing status */
    
    // Timing information
    int64_t submit_time_us;              /**< Request submission timestamp */
    int64_t start_time_us;               /**< Processing start timestamp */
    int64_t complete_time_us;            /**< Processing completion timestamp */
    int timeout_ms;                      /**< Request timeout in milliseconds */
    
    // Processing context
    int worker_id;                       /**< ID of worker processing request */
    void *context;                       /**< Additional request context */
    
    // Queue linkage
    struct queued_request *next;         /**< Next request in queue */
    struct queued_request *prev;         /**< Previous request in queue */
};

/**
 * Request queue configuration.
 */
typedef struct {
    int max_size;                        /**< Maximum queue size */
    int default_timeout_ms;              /**< Default request timeout */
    bool enable_priority_queue;          /**< Enable priority-based ordering */
    bool enable_overflow_handling;       /**< Enable overflow queue */
    int overflow_max_size;               /**< Maximum overflow queue size */
    bool enable_statistics;              /**< Enable queue statistics */
} request_queue_config_t;

/**
 * Request queue statistics.
 */
typedef struct {
    uint64_t total_enqueued;             /**< Total requests enqueued */
    uint64_t total_dequeued;             /**< Total requests dequeued */
    uint64_t total_completed;            /**< Total requests completed */
    uint64_t total_failed;               /**< Total requests failed */
    uint64_t total_timeout;              /**< Total requests timed out */
    uint64_t total_dropped;              /**< Total requests dropped */
    
    int current_size;                    /**< Current queue size */
    int peak_size;                       /**< Peak queue size */
    int overflow_size;                   /**< Current overflow queue size */
    
    uint64_t total_wait_time_us;         /**< Total wait time in queue */
    uint64_t total_processing_time_us;   /**< Total processing time */
    uint64_t average_wait_time_us;       /**< Average wait time */
    uint64_t average_processing_time_us; /**< Average processing time */
    
    int64_t uptime_us;                   /**< Queue uptime */
} request_queue_stats_t;

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

/**
 * Get default request queue configuration.
 * 
 * @return Default configuration structure
 */
request_queue_config_t request_queue_default_config(void);

/**
 * Validate request queue configuration.
 * 
 * @param config Configuration to validate
 * @return REQUEST_QUEUE_SUCCESS if valid, error code otherwise
 */
request_queue_error_t request_queue_validate_config(const request_queue_config_t *config);

// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

/**
 * Create a new request queue.
 * 
 * @param config Queue configuration
 * @return Request queue instance or NULL on error
 */
request_queue_t *request_queue_create(const request_queue_config_t *config);

/**
 * Destroy the request queue.
 * Frees all resources and pending requests.
 * 
 * @param queue Request queue instance
 */
void request_queue_destroy(request_queue_t *queue);

/**
 * Clear all requests from the queue.
 * 
 * @param queue Request queue instance
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_clear(request_queue_t *queue);

// ============================================================================
// REQUEST MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Enqueue a new request.
 * 
 * @param queue Request queue instance
 * @param client Client that sent the request
 * @param rpc_request JSON-RPC request data
 * @param priority Request priority
 * @param timeout_ms Request timeout in milliseconds (0 = use default)
 * @param request_id Output parameter for assigned request ID
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_enqueue(request_queue_t *queue,
                                           socket_client_t *client,
                                           json_rpc_request_t *rpc_request,
                                           request_priority_t priority,
                                           int timeout_ms,
                                           uint32_t *request_id);

/**
 * Dequeue the next request for processing.
 * 
 * @param queue Request queue instance
 * @param worker_id ID of the worker processing the request
 * @return Queued request or NULL if queue is empty
 */
queued_request_t *request_queue_dequeue(request_queue_t *queue, int worker_id);

/**
 * Complete a request and update statistics.
 * 
 * @param queue Request queue instance
 * @param request Completed request
 * @param success Whether the request completed successfully
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_complete_request(request_queue_t *queue,
                                                    queued_request_t *request,
                                                    bool success);

/**
 * Cancel a pending request.
 * 
 * @param queue Request queue instance
 * @param request_id Request ID to cancel
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_cancel_request(request_queue_t *queue,
                                                  uint32_t request_id);

/**
 * Find a request by ID.
 * 
 * @param queue Request queue instance
 * @param request_id Request ID to find
 * @return Queued request or NULL if not found
 */
queued_request_t *request_queue_find_request(const request_queue_t *queue,
                                            uint32_t request_id);

// ============================================================================
// QUEUE STATUS FUNCTIONS
// ============================================================================

/**
 * Get current queue size.
 * 
 * @param queue Request queue instance
 * @return Number of requests in queue, -1 on error
 */
int request_queue_get_size(const request_queue_t *queue);

/**
 * Check if queue is empty.
 * 
 * @param queue Request queue instance
 * @return true if empty, false otherwise
 */
bool request_queue_is_empty(const request_queue_t *queue);

/**
 * Check if queue is full.
 * 
 * @param queue Request queue instance
 * @return true if full, false otherwise
 */
bool request_queue_is_full(const request_queue_t *queue);

/**
 * Get queue capacity.
 * 
 * @param queue Request queue instance
 * @return Queue capacity, -1 on error
 */
int request_queue_get_capacity(const request_queue_t *queue);

/**
 * Get overflow queue size.
 * 
 * @param queue Request queue instance
 * @return Overflow queue size, -1 on error
 */
int request_queue_get_overflow_size(const request_queue_t *queue);

// ============================================================================
// TIMEOUT MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Check for and handle timed-out requests.
 * 
 * @param queue Request queue instance
 * @return Number of requests timed out, -1 on error
 */
int request_queue_handle_timeouts(request_queue_t *queue);

/**
 * Set timeout for a specific request.
 * 
 * @param queue Request queue instance
 * @param request_id Request ID
 * @param timeout_ms New timeout in milliseconds
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_set_timeout(request_queue_t *queue,
                                               uint32_t request_id,
                                               int timeout_ms);

// ============================================================================
// STATISTICS AND MONITORING FUNCTIONS
// ============================================================================

/**
 * Get request queue statistics.
 * 
 * @param queue Request queue instance
 * @param stats Output statistics structure
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_get_stats(const request_queue_t *queue,
                                             request_queue_stats_t *stats);

/**
 * Reset request queue statistics.
 * 
 * @param queue Request queue instance
 * @return REQUEST_QUEUE_SUCCESS on success, error code on failure
 */
request_queue_error_t request_queue_reset_stats(request_queue_t *queue);

/**
 * Get requests by status.
 * 
 * @param queue Request queue instance
 * @param status Request status to filter by
 * @param requests Output array of request pointers
 * @param max_requests Maximum number of requests to return
 * @return Number of requests returned, -1 on error
 */
int request_queue_get_requests_by_status(const request_queue_t *queue,
                                        request_status_t status,
                                        queued_request_t **requests,
                                        int max_requests);

// ============================================================================
// ERROR HANDLING FUNCTIONS
// ============================================================================

/**
 * Convert error code to human-readable string.
 * 
 * @param error Error code
 * @return Error description string
 */
const char *request_queue_error_string(request_queue_error_t error);

/**
 * Get last error message from request queue.
 * 
 * @param queue Request queue instance
 * @return Last error message or NULL if no error
 */
const char *request_queue_get_last_error(const request_queue_t *queue);

// ============================================================================
// REQUEST HELPER FUNCTIONS
// ============================================================================

/**
 * Create a copy of a queued request.
 * 
 * @param request Request to copy
 * @return Copy of the request or NULL on error
 */
queued_request_t *request_queue_clone_request(const queued_request_t *request);

/**
 * Destroy a queued request and free its resources.
 * 
 * @param request Request to destroy
 */
void request_queue_destroy_request(queued_request_t *request);

/**
 * Get request age in microseconds.
 * 
 * @param request Queued request
 * @return Age in microseconds since submission
 */
int64_t request_queue_get_request_age_us(const queued_request_t *request);

/**
 * Check if request has timed out.
 * 
 * @param request Queued request
 * @return true if timed out, false otherwise
 */
bool request_queue_is_request_timed_out(const queued_request_t *request);

#endif // REQUEST_QUEUE_H