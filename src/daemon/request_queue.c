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

#include "request_queue.h"
#include "../log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

// ============================================================================
// CONSTANTS AND LIMITS
// ============================================================================

#define REQUEST_QUEUE_MIN_SIZE 1            /**< Minimum queue size */
#define REQUEST_QUEUE_MAX_SIZE 65536        /**< Maximum queue size */
#define REQUEST_QUEUE_DEFAULT_SIZE 1024     /**< Default queue size */
#define REQUEST_QUEUE_DEFAULT_TIMEOUT 30000 /**< Default timeout (30 seconds) */
#define REQUEST_QUEUE_ERROR_MSG_SIZE 256    /**< Error message buffer size */

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

/**
 * Request queue structure.
 */
struct request_queue {
    // Configuration
    request_queue_config_t config;       /**< Queue configuration */
    
    // Queue management
    queued_request_t *head;              /**< Queue head pointer */
    queued_request_t *tail;              /**< Queue tail pointer */
    queued_request_t *overflow_head;     /**< Overflow queue head */
    queued_request_t *overflow_tail;     /**< Overflow queue tail */
    
    int size;                            /**< Current queue size */
    int overflow_size;                   /**< Current overflow queue size */
    uint32_t next_request_id;            /**< Next request ID to assign */
    
    // Synchronization
    pthread_mutex_t queue_mutex;         /**< Queue access mutex */
    pthread_mutex_t stats_mutex;         /**< Statistics mutex */
    
    // Statistics
    request_queue_stats_t stats;         /**< Queue statistics */
    int64_t start_time_us;               /**< Queue creation time */
    
    // State
    bool initialized;                    /**< Initialization flag */
    
    // Error handling
    char last_error[REQUEST_QUEUE_ERROR_MSG_SIZE]; /**< Last error message */
};

// ============================================================================
// UTILITY MACROS
// ============================================================================

#define SET_ERROR(queue, fmt, ...) do { \
    snprintf((queue)->last_error, sizeof((queue)->last_error), fmt, ##__VA_ARGS__); \
    LOG_E("Request Queue: " fmt, ##__VA_ARGS__); \
} while(0)

#define LOCK_MUTEX(mutex) do { \
    if (pthread_mutex_lock(mutex) != 0) { \
        LOG_E("Mutex lock failed: %s", strerror(errno)); \
        return REQUEST_QUEUE_ERROR_MUTEX_FAILED; \
    } \
} while(0)

#define UNLOCK_MUTEX(mutex) do { \
    if (pthread_mutex_unlock(mutex) != 0) { \
        LOG_E("Mutex unlock failed: %s", strerror(errno)); \
    } \
} while(0)

#define SAFE_FREE(ptr) do { \
    if (ptr) { \
        free(ptr); \
        ptr = NULL; \
    } \
} while(0)

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static int64_t get_current_time_us(void);
static queued_request_t *create_queued_request(socket_client_t *client,
                                              json_rpc_request_t *rpc_request,
                                              request_priority_t priority,
                                              int timeout_ms,
                                              uint32_t request_id);
static void destroy_queued_request_internal(queued_request_t *request);
static request_queue_error_t insert_request_by_priority(request_queue_t *queue,
                                                       queued_request_t *request);
static request_queue_error_t insert_request_fifo(request_queue_t *queue,
                                                queued_request_t *request);
static queued_request_t *remove_request_from_queue(request_queue_t *queue,
                                                  queued_request_t *request);
static void update_stats_on_enqueue(request_queue_t *queue);
static void update_stats_on_dequeue(request_queue_t *queue, int64_t wait_time_us);
static void update_stats_on_completion(request_queue_t *queue, 
                                      int64_t processing_time_us, bool success);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static int64_t get_current_time_us(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

static void update_stats_on_enqueue(request_queue_t *queue)
{
    if (!queue->config.enable_statistics) return;
    
    pthread_mutex_lock(&queue->stats_mutex);
    queue->stats.total_enqueued++;
    queue->stats.current_size = queue->size;
    if (queue->size > queue->stats.peak_size) {
        queue->stats.peak_size = queue->size;
    }
    queue->stats.overflow_size = queue->overflow_size;
    pthread_mutex_unlock(&queue->stats_mutex);
}

static void update_stats_on_dequeue(request_queue_t *queue, int64_t wait_time_us)
{
    if (!queue->config.enable_statistics) return;
    
    pthread_mutex_lock(&queue->stats_mutex);
    queue->stats.total_dequeued++;
    queue->stats.current_size = queue->size;
    queue->stats.total_wait_time_us += wait_time_us;
    if (queue->stats.total_dequeued > 0) {
        queue->stats.average_wait_time_us = 
            queue->stats.total_wait_time_us / queue->stats.total_dequeued;
    }
    pthread_mutex_unlock(&queue->stats_mutex);
}

static void update_stats_on_completion(request_queue_t *queue, 
                                      int64_t processing_time_us, bool success)
{
    if (!queue->config.enable_statistics) return;
    
    pthread_mutex_lock(&queue->stats_mutex);
    if (success) {
        queue->stats.total_completed++;
    } else {
        queue->stats.total_failed++;
    }
    
    queue->stats.total_processing_time_us += processing_time_us;
    uint64_t total_processed = queue->stats.total_completed + queue->stats.total_failed;
    if (total_processed > 0) {
        queue->stats.average_processing_time_us = 
            queue->stats.total_processing_time_us / total_processed;
    }
    pthread_mutex_unlock(&queue->stats_mutex);
}

// ============================================================================
// REQUEST MANAGEMENT FUNCTIONS
// ============================================================================

static queued_request_t *create_queued_request(socket_client_t *client,
                                              json_rpc_request_t *rpc_request,
                                              request_priority_t priority,
                                              int timeout_ms,
                                              uint32_t request_id)
{
    queued_request_t *request = calloc(1, sizeof(queued_request_t));
    if (!request) return NULL;
    
    request->request_id = request_id;
    request->client = client;
    request->rpc_request = rpc_request;
    request->priority = priority;
    request->status = REQUEST_STATUS_QUEUED;
    request->submit_time_us = get_current_time_us();
    request->start_time_us = 0;
    request->complete_time_us = 0;
    request->timeout_ms = timeout_ms;
    request->worker_id = -1;
    request->context = NULL;
    request->next = NULL;
    request->prev = NULL;
    
    return request;
}

static void destroy_queued_request_internal(queued_request_t *request)
{
    if (!request) return;
    
    // Note: We don't free client or rpc_request here as they may be owned elsewhere
    // The caller is responsible for managing those resources
    
    free(request);
}

static request_queue_error_t insert_request_by_priority(request_queue_t *queue,
                                                       queued_request_t *request)
{
    if (queue->head == NULL) {
        // Empty queue
        queue->head = request;
        queue->tail = request;
        request->next = NULL;
        request->prev = NULL;
    } else {
        // Find insertion point (higher priority = higher enum value)
        queued_request_t *current = queue->head;
        while (current && current->priority >= request->priority) {
            current = current->next;
        }
        
        if (current == NULL) {
            // Insert at tail
            request->prev = queue->tail;
            request->next = NULL;
            queue->tail->next = request;
            queue->tail = request;
        } else if (current->prev == NULL) {
            // Insert at head
            request->next = queue->head;
            request->prev = NULL;
            queue->head->prev = request;
            queue->head = request;
        } else {
            // Insert in middle
            request->next = current;
            request->prev = current->prev;
            current->prev->next = request;
            current->prev = request;
        }
    }
    
    queue->size++;
    return REQUEST_QUEUE_SUCCESS;
}

static request_queue_error_t insert_request_fifo(request_queue_t *queue,
                                                queued_request_t *request)
{
    if (queue->tail == NULL) {
        // Empty queue
        queue->head = request;
        queue->tail = request;
        request->next = NULL;
        request->prev = NULL;
    } else {
        // Insert at tail
        request->prev = queue->tail;
        request->next = NULL;
        queue->tail->next = request;
        queue->tail = request;
    }
    
    queue->size++;
    return REQUEST_QUEUE_SUCCESS;
}

static queued_request_t *remove_request_from_queue(request_queue_t *queue,
                                                  queued_request_t *request)
{
    if (!queue || !request) return NULL;
    
    // Update linkage
    if (request->prev) {
        request->prev->next = request->next;
    } else {
        queue->head = request->next;
    }
    
    if (request->next) {
        request->next->prev = request->prev;
    } else {
        queue->tail = request->prev;
    }
    
    request->next = NULL;
    request->prev = NULL;
    queue->size--;
    
    return request;
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

request_queue_config_t request_queue_default_config(void)
{
    request_queue_config_t config = {0};
    
    config.max_size = REQUEST_QUEUE_DEFAULT_SIZE;
    config.default_timeout_ms = REQUEST_QUEUE_DEFAULT_TIMEOUT;
    config.enable_priority_queue = true;
    config.enable_overflow_handling = true;
    config.overflow_max_size = REQUEST_QUEUE_DEFAULT_SIZE / 4;
    config.enable_statistics = true;
    
    return config;
}

request_queue_error_t request_queue_validate_config(const request_queue_config_t *config)
{
    if (!config) return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    
    if (config->max_size < REQUEST_QUEUE_MIN_SIZE || 
        config->max_size > REQUEST_QUEUE_MAX_SIZE) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    if (config->default_timeout_ms < 0) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    if (config->enable_overflow_handling && config->overflow_max_size < 0) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    return REQUEST_QUEUE_SUCCESS;
}

// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

request_queue_t *request_queue_create(const request_queue_config_t *config)
{
    if (!config) return NULL;
    
    request_queue_error_t error = request_queue_validate_config(config);
    if (error != REQUEST_QUEUE_SUCCESS) {
        LOG_E("Invalid request queue configuration: %s", request_queue_error_string(error));
        return NULL;
    }
    
    request_queue_t *queue = calloc(1, sizeof(request_queue_t));
    if (!queue) return NULL;
    
    // Copy configuration
    queue->config = *config;
    
    // Initialize state
    queue->head = NULL;
    queue->tail = NULL;
    queue->overflow_head = NULL;
    queue->overflow_tail = NULL;
    queue->size = 0;
    queue->overflow_size = 0;
    queue->next_request_id = 1;
    queue->start_time_us = get_current_time_us();
    queue->initialized = false;
    
    // Initialize mutexes
    if (pthread_mutex_init(&queue->queue_mutex, NULL) != 0 ||
        pthread_mutex_init(&queue->stats_mutex, NULL) != 0) {
        request_queue_destroy(queue);
        return NULL;
    }
    
    // Initialize statistics
    memset(&queue->stats, 0, sizeof(queue->stats));
    
    queue->initialized = true;
    return queue;
}

void request_queue_destroy(request_queue_t *queue)
{
    if (!queue) return;
    
    // Clear all requests
    request_queue_clear(queue);
    
    // Destroy mutexes
    if (queue->initialized) {
        pthread_mutex_destroy(&queue->queue_mutex);
        pthread_mutex_destroy(&queue->stats_mutex);
    }
    
    free(queue);
}

request_queue_error_t request_queue_clear(request_queue_t *queue)
{
    if (!queue || !queue->initialized) {
        return REQUEST_QUEUE_ERROR_NOT_INITIALIZED;
    }
    
    LOCK_MUTEX(&queue->queue_mutex);
    
    // Clear main queue
    queued_request_t *request = queue->head;
    while (request) {
        queued_request_t *next = request->next;
        destroy_queued_request_internal(request);
        request = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    
    // Clear overflow queue
    request = queue->overflow_head;
    while (request) {
        queued_request_t *next = request->next;
        destroy_queued_request_internal(request);
        request = next;
    }
    queue->overflow_head = NULL;
    queue->overflow_tail = NULL;
    queue->overflow_size = 0;
    
    UNLOCK_MUTEX(&queue->queue_mutex);
    
    return REQUEST_QUEUE_SUCCESS;
}

// ============================================================================
// REQUEST MANAGEMENT FUNCTIONS
// ============================================================================

request_queue_error_t request_queue_enqueue(request_queue_t *queue,
                                           socket_client_t *client,
                                           json_rpc_request_t *rpc_request,
                                           request_priority_t priority,
                                           int timeout_ms,
                                           uint32_t *request_id)
{
    if (!queue || !queue->initialized || !client || !rpc_request) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    if (timeout_ms <= 0) {
        timeout_ms = queue->config.default_timeout_ms;
    }
    
    LOCK_MUTEX(&queue->queue_mutex);
    
    // Check if main queue is full
    if (queue->size >= queue->config.max_size) {
        if (queue->config.enable_overflow_handling && 
            queue->overflow_size < queue->config.overflow_max_size) {
            // Use overflow queue
            // Implementation for overflow queue would go here
            UNLOCK_MUTEX(&queue->queue_mutex);
            
            if (queue->config.enable_statistics) {
                pthread_mutex_lock(&queue->stats_mutex);
                queue->stats.total_dropped++;
                pthread_mutex_unlock(&queue->stats_mutex);
            }
            
            return REQUEST_QUEUE_ERROR_QUEUE_FULL;
        } else {
            UNLOCK_MUTEX(&queue->queue_mutex);
            
            if (queue->config.enable_statistics) {
                pthread_mutex_lock(&queue->stats_mutex);
                queue->stats.total_dropped++;
                pthread_mutex_unlock(&queue->stats_mutex);
            }
            
            return REQUEST_QUEUE_ERROR_QUEUE_FULL;
        }
    }
    
    // Create request
    uint32_t id = queue->next_request_id++;
    queued_request_t *request = create_queued_request(client, rpc_request, 
                                                     priority, timeout_ms, id);
    if (!request) {
        UNLOCK_MUTEX(&queue->queue_mutex);
        return REQUEST_QUEUE_ERROR_OUT_OF_MEMORY;
    }
    
    // Insert request into queue
    request_queue_error_t result;
    if (queue->config.enable_priority_queue) {
        result = insert_request_by_priority(queue, request);
    } else {
        result = insert_request_fifo(queue, request);
    }
    
    if (result == REQUEST_QUEUE_SUCCESS) {
        update_stats_on_enqueue(queue);
        if (request_id) {
            *request_id = id;
        }
    } else {
        destroy_queued_request_internal(request);
    }
    
    UNLOCK_MUTEX(&queue->queue_mutex);
    return result;
}

queued_request_t *request_queue_dequeue(request_queue_t *queue, int worker_id)
{
    if (!queue || !queue->initialized) return NULL;
    
    pthread_mutex_lock(&queue->queue_mutex);
    
    queued_request_t *request = queue->head;
    if (request) {
        // Remove from queue
        remove_request_from_queue(queue, request);
        
        // Update request status
        request->status = REQUEST_STATUS_PROCESSING;
        request->start_time_us = get_current_time_us();
        request->worker_id = worker_id;
        
        // Update statistics
        int64_t wait_time_us = request->start_time_us - request->submit_time_us;
        update_stats_on_dequeue(queue, wait_time_us);
    }
    
    pthread_mutex_unlock(&queue->queue_mutex);
    return request;
}

request_queue_error_t request_queue_complete_request(request_queue_t *queue,
                                                    queued_request_t *request,
                                                    bool success)
{
    if (!queue || !queue->initialized || !request) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    request->complete_time_us = get_current_time_us();
    request->status = success ? REQUEST_STATUS_COMPLETED : REQUEST_STATUS_FAILED;
    
    int64_t processing_time_us = request->complete_time_us - request->start_time_us;
    update_stats_on_completion(queue, processing_time_us, success);
    
    return REQUEST_QUEUE_SUCCESS;
}

request_queue_error_t request_queue_cancel_request(request_queue_t *queue,
                                                  uint32_t request_id)
{
    if (!queue || !queue->initialized) {
        return REQUEST_QUEUE_ERROR_NOT_INITIALIZED;
    }
    
    LOCK_MUTEX(&queue->queue_mutex);
    
    // Find request in main queue
    queued_request_t *request = queue->head;
    while (request) {
        if (request->request_id == request_id) {
            remove_request_from_queue(queue, request);
            destroy_queued_request_internal(request);
            UNLOCK_MUTEX(&queue->queue_mutex);
            return REQUEST_QUEUE_SUCCESS;
        }
        request = request->next;
    }
    
    UNLOCK_MUTEX(&queue->queue_mutex);
    return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
}

queued_request_t *request_queue_find_request(const request_queue_t *queue,
                                            uint32_t request_id)
{
    if (!queue || !queue->initialized) return NULL;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->queue_mutex);
    
    queued_request_t *request = queue->head;
    while (request) {
        if (request->request_id == request_id) {
            pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
            return request;
        }
        request = request->next;
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
    return NULL;
}

// ============================================================================
// QUEUE STATUS FUNCTIONS
// ============================================================================

int request_queue_get_size(const request_queue_t *queue)
{
    if (!queue || !queue->initialized) return -1;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->queue_mutex);
    int size = queue->size;
    pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
    
    return size;
}

bool request_queue_is_empty(const request_queue_t *queue)
{
    return request_queue_get_size(queue) == 0;
}

bool request_queue_is_full(const request_queue_t *queue)
{
    if (!queue || !queue->initialized) return true;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->queue_mutex);
    bool full = queue->size >= queue->config.max_size;
    pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
    
    return full;
}

int request_queue_get_capacity(const request_queue_t *queue)
{
    return queue ? queue->config.max_size : -1;
}

int request_queue_get_overflow_size(const request_queue_t *queue)
{
    if (!queue || !queue->initialized) return -1;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->queue_mutex);
    int size = queue->overflow_size;
    pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
    
    return size;
}

// ============================================================================
// TIMEOUT MANAGEMENT FUNCTIONS
// ============================================================================

int request_queue_handle_timeouts(request_queue_t *queue)
{
    if (!queue || !queue->initialized) return -1;
    
    int64_t current_time_us = get_current_time_us();
    int timeout_count = 0;
    
    LOCK_MUTEX(&queue->queue_mutex);
    
    queued_request_t *request = queue->head;
    while (request) {
        queued_request_t *next = request->next;
        
        int64_t age_us = current_time_us - request->submit_time_us;
        if (age_us > (int64_t)request->timeout_ms * 1000) {
            // Request has timed out
            remove_request_from_queue(queue, request);
            request->status = REQUEST_STATUS_TIMEOUT;
            destroy_queued_request_internal(request);
            timeout_count++;
            
            if (queue->config.enable_statistics) {
                pthread_mutex_lock(&queue->stats_mutex);
                queue->stats.total_timeout++;
                pthread_mutex_unlock(&queue->stats_mutex);
            }
        }
        
        request = next;
    }
    
    UNLOCK_MUTEX(&queue->queue_mutex);
    return timeout_count;
}

request_queue_error_t request_queue_set_timeout(request_queue_t *queue,
                                               uint32_t request_id,
                                               int timeout_ms)
{
    if (!queue || !queue->initialized || timeout_ms < 0) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    queued_request_t *request = request_queue_find_request(queue, request_id);
    if (!request) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    request->timeout_ms = timeout_ms;
    return REQUEST_QUEUE_SUCCESS;
}

// ============================================================================
// STATISTICS AND MONITORING FUNCTIONS
// ============================================================================

request_queue_error_t request_queue_get_stats(const request_queue_t *queue,
                                             request_queue_stats_t *stats)
{
    if (!queue || !queue->initialized || !stats) {
        return REQUEST_QUEUE_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->stats_mutex);
    *stats = queue->stats;
    
    // Calculate uptime
    if (queue->start_time_us > 0) {
        stats->uptime_us = get_current_time_us() - queue->start_time_us;
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&queue->stats_mutex);
    
    return REQUEST_QUEUE_SUCCESS;
}

request_queue_error_t request_queue_reset_stats(request_queue_t *queue)
{
    if (!queue || !queue->initialized) {
        return REQUEST_QUEUE_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&queue->stats_mutex);
    memset(&queue->stats, 0, sizeof(queue->stats));
    queue->start_time_us = get_current_time_us();
    pthread_mutex_unlock(&queue->stats_mutex);
    
    return REQUEST_QUEUE_SUCCESS;
}

int request_queue_get_requests_by_status(const request_queue_t *queue,
                                        request_status_t status,
                                        queued_request_t **requests,
                                        int max_requests)
{
    if (!queue || !queue->initialized || !requests || max_requests <= 0) {
        return -1;
    }
    
    int count = 0;
    
    pthread_mutex_lock((pthread_mutex_t*)&queue->queue_mutex);
    
    queued_request_t *request = queue->head;
    while (request && count < max_requests) {
        if (request->status == status) {
            requests[count++] = request;
        }
        request = request->next;
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&queue->queue_mutex);
    
    return count;
}

// ============================================================================
// ERROR HANDLING FUNCTIONS
// ============================================================================

const char *request_queue_error_string(request_queue_error_t error)
{
    switch (error) {
        case REQUEST_QUEUE_SUCCESS: return "Success";
        case REQUEST_QUEUE_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case REQUEST_QUEUE_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case REQUEST_QUEUE_ERROR_QUEUE_FULL: return "Queue is full";
        case REQUEST_QUEUE_ERROR_QUEUE_EMPTY: return "Queue is empty";
        case REQUEST_QUEUE_ERROR_MUTEX_FAILED: return "Mutex operation failed";
        case REQUEST_QUEUE_ERROR_TIMEOUT: return "Operation timed out";
        case REQUEST_QUEUE_ERROR_NOT_INITIALIZED: return "Queue not initialized";
        case REQUEST_QUEUE_ERROR_UNKNOWN:
        default: return "Unknown error";
    }
}

const char *request_queue_get_last_error(const request_queue_t *queue)
{
    if (!queue) return "Invalid request queue context";
    return queue->last_error[0] ? queue->last_error : NULL;
}

// ============================================================================
// REQUEST HELPER FUNCTIONS
// ============================================================================

queued_request_t *request_queue_clone_request(const queued_request_t *request)
{
    if (!request) return NULL;
    
    queued_request_t *clone = calloc(1, sizeof(queued_request_t));
    if (!clone) return NULL;
    
    *clone = *request;
    clone->next = NULL;
    clone->prev = NULL;
    
    return clone;
}

void request_queue_destroy_request(queued_request_t *request)
{
    destroy_queued_request_internal(request);
}

int64_t request_queue_get_request_age_us(const queued_request_t *request)
{
    if (!request) return -1;
    return get_current_time_us() - request->submit_time_us;
}

bool request_queue_is_request_timed_out(const queued_request_t *request)
{
    if (!request) return false;
    int64_t age_us = request_queue_get_request_age_us(request);
    return age_us > (int64_t)request->timeout_ms * 1000;
}