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

#include "worker_pool.h"
#include "../log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

// ============================================================================
// CONSTANTS AND LIMITS
// ============================================================================

#define WORKER_POOL_MIN_WORKERS 1              /**< Minimum number of workers */
#define WORKER_POOL_MAX_WORKERS 64             /**< Maximum number of workers */
#define WORKER_POOL_MIN_QUEUE_SIZE 1           /**< Minimum queue capacity */
#define WORKER_POOL_MAX_QUEUE_SIZE 65536       /**< Maximum queue capacity */
#define WORKER_POOL_DEFAULT_WORKERS 8          /**< Default number of workers */
#define WORKER_POOL_DEFAULT_QUEUE_SIZE 1024    /**< Default queue capacity */
#define WORKER_POOL_DEFAULT_SHUTDOWN_TIMEOUT 5000 /**< Default shutdown timeout (ms) */
#define WORKER_POOL_ERROR_MSG_SIZE 256         /**< Error message buffer size */

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

/**
 * Worker request structure.
 */
struct worker_request {
    void *data;                          /**< User request data */
    worker_priority_t priority;          /**< Request priority */
    int64_t submit_time_us;              /**< Request submission time */
    int64_t start_time_us;               /**< Processing start time */
    struct worker_request *next;         /**< Next request in queue */
};

/**
 * Worker thread structure.
 */
typedef struct worker_thread {
    pthread_t thread;                    /**< Thread handle */
    int worker_id;                       /**< Worker ID */
    bool running;                        /**< Worker running flag */
    bool active;                         /**< Worker active flag */
    worker_pool_t *pool;                 /**< Parent pool reference */
    int64_t last_activity_us;            /**< Last activity timestamp */
    uint64_t requests_processed;         /**< Requests processed by this worker */
    struct worker_thread *next;          /**< Next worker in list */
} worker_thread_t;

/**
 * Worker pool structure.
 */
struct worker_pool {
    // Configuration
    worker_pool_config_t config;         /**< Pool configuration */
    
    // Thread management
    worker_thread_t *workers;            /**< Worker thread list */
    bool running;                        /**< Pool running flag */
    bool initialized;                    /**< Pool initialization flag */
    
    // Request queue
    worker_request_t *queue_head;        /**< Queue head pointer */
    worker_request_t *queue_tail;        /**< Queue tail pointer */
    int queue_size;                      /**< Current queue size */
    
    // Synchronization
    pthread_mutex_t queue_mutex;         /**< Queue access mutex */
    pthread_cond_t queue_cond;           /**< Queue condition variable */
    pthread_mutex_t stats_mutex;         /**< Statistics mutex */
    pthread_mutex_t pool_mutex;          /**< Pool state mutex */
    
    // Statistics
    worker_stats_t stats;                /**< Pool statistics */
    int64_t start_time_us;               /**< Pool start time */
    
    // Error handling
    char last_error[WORKER_POOL_ERROR_MSG_SIZE]; /**< Last error message */
};

// ============================================================================
// UTILITY MACROS
// ============================================================================

#define SET_ERROR(pool, fmt, ...) do { \
    snprintf((pool)->last_error, sizeof((pool)->last_error), fmt, ##__VA_ARGS__); \
    LOG_E("Worker Pool: " fmt, ##__VA_ARGS__); \
} while(0)

#define LOCK_MUTEX(mutex) do { \
    if (pthread_mutex_lock(mutex) != 0) { \
        LOG_E("Mutex lock failed: %s", strerror(errno)); \
        return WORKER_POOL_ERROR_MUTEX_FAILED; \
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

static void *worker_thread_func(void *arg);
static worker_request_t *create_request(void *data, worker_priority_t priority);
static void destroy_request(worker_request_t *request, worker_pool_t *pool);
static worker_request_t *dequeue_request(worker_pool_t *pool) __attribute__((unused));
static worker_pool_error_t enqueue_request(worker_pool_t *pool, worker_request_t *request);
static int64_t get_current_time_us(void);
static void update_stats_on_completion(worker_pool_t *pool, int64_t processing_time_us, bool success);

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

static void update_stats_on_completion(worker_pool_t *pool, int64_t processing_time_us, bool success)
{
    if (!pool->config.enable_statistics) return;
    
    pthread_mutex_lock(&pool->stats_mutex);
    
    if (success) {
        pool->stats.requests_processed++;
        pool->stats.total_processing_time_us += processing_time_us;
        
        if (pool->stats.requests_processed == 1) {
            pool->stats.min_processing_time_us = processing_time_us;
            pool->stats.max_processing_time_us = processing_time_us;
        } else {
            if (processing_time_us < pool->stats.min_processing_time_us) {
                pool->stats.min_processing_time_us = processing_time_us;
            }
            if (processing_time_us > pool->stats.max_processing_time_us) {
                pool->stats.max_processing_time_us = processing_time_us;
            }
        }
        
        pool->stats.average_processing_time_us = 
            pool->stats.total_processing_time_us / pool->stats.requests_processed;
    } else {
        pool->stats.requests_failed++;
    }
    
    pthread_mutex_unlock(&pool->stats_mutex);
}

// ============================================================================
// REQUEST MANAGEMENT
// ============================================================================

static worker_request_t *create_request(void *data, worker_priority_t priority)
{
    worker_request_t *request = calloc(1, sizeof(worker_request_t));
    if (!request) return NULL;
    
    request->data = data;
    request->priority = priority;
    request->submit_time_us = get_current_time_us();
    request->start_time_us = 0;
    request->next = NULL;
    
    return request;
}

static void destroy_request(worker_request_t *request, worker_pool_t *pool)
{
    if (!request) return;
    
    if (pool->config.cleanup_func && request->data) {
        pool->config.cleanup_func(request->data);
    }
    
    free(request);
}

static worker_pool_error_t enqueue_request(worker_pool_t *pool, worker_request_t *request)
{
    if (!pool || !request) return WORKER_POOL_ERROR_INVALID_PARAMETER;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    if (pool->queue_size >= pool->config.queue_capacity) {
        pthread_mutex_unlock(&pool->queue_mutex);
        
        if (pool->config.enable_statistics) {
            pthread_mutex_lock(&pool->stats_mutex);
            pool->stats.requests_dropped++;
            pthread_mutex_unlock(&pool->stats_mutex);
        }
        
        return WORKER_POOL_ERROR_QUEUE_FULL;
    }
    
    // Insert based on priority if enabled
    if (pool->config.enable_priority_queue) {
        worker_request_t *current = pool->queue_head;
        worker_request_t *prev = NULL;
        
        // Find insertion point (higher priority = lower enum value)
        while (current && current->priority <= request->priority) {
            prev = current;
            current = current->next;
        }
        
        // Insert request
        request->next = current;
        if (prev) {
            prev->next = request;
        } else {
            pool->queue_head = request;
        }
        
        // Update tail if we inserted at the end
        if (!current) {
            pool->queue_tail = request;
        }
    } else {
        // Simple FIFO queue
        if (pool->queue_tail) {
            pool->queue_tail->next = request;
        } else {
            pool->queue_head = request;
        }
        pool->queue_tail = request;
    }
    
    pool->queue_size++;
    
    if (pool->config.enable_statistics) {
        pthread_mutex_lock(&pool->stats_mutex);
        pool->stats.requests_queued = pool->queue_size;
        pthread_mutex_unlock(&pool->stats_mutex);
    }
    
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return WORKER_POOL_SUCCESS;
}

static worker_request_t *dequeue_request(worker_pool_t *pool)
{
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    worker_request_t *request = pool->queue_head;
    if (request) {
        pool->queue_head = request->next;
        if (!pool->queue_head) {
            pool->queue_tail = NULL;
        }
        pool->queue_size--;
        request->next = NULL;
        
        if (pool->config.enable_statistics) {
            pthread_mutex_lock(&pool->stats_mutex);
            pool->stats.requests_queued = pool->queue_size;
            pthread_mutex_unlock(&pool->stats_mutex);
        }
    }
    
    pthread_mutex_unlock(&pool->queue_mutex);
    return request;
}

// ============================================================================
// WORKER THREAD IMPLEMENTATION
// ============================================================================

static void *worker_thread_func(void *arg)
{
    worker_thread_t *worker = (worker_thread_t*)arg;
    worker_pool_t *pool = worker->pool;
    
    LOG_I("Worker thread %d started", worker->worker_id);
    
    // Ignore SIGPIPE to prevent worker threads from crashing
    signal(SIGPIPE, SIG_IGN);
    
    while (worker->running) {
        worker_request_t *request = NULL;
        
        // Wait for work
        pthread_mutex_lock(&pool->queue_mutex);
        while (pool->queue_size == 0 && worker->running) {
            worker->active = false;
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }
        
        if (!worker->running) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // Get request from queue
        request = pool->queue_head;
        if (request) {
            pool->queue_head = request->next;
            if (!pool->queue_head) {
                pool->queue_tail = NULL;
            }
            pool->queue_size--;
            request->next = NULL;
            
            if (pool->config.enable_statistics) {
                pthread_mutex_lock(&pool->stats_mutex);
                pool->stats.requests_queued = pool->queue_size;
                pthread_mutex_unlock(&pool->stats_mutex);
            }
        }
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        if (request) {
            worker->active = true;
            worker->last_activity_us = get_current_time_us();
            request->start_time_us = worker->last_activity_us;
            
            // Process the request
            int result = 0;
            if (pool->config.process_func) {
                result = pool->config.process_func(request->data, worker->worker_id, pool->config.context);
            }
            
            int64_t end_time_us = get_current_time_us();
            int64_t processing_time_us = end_time_us - request->start_time_us;
            
            // Update statistics
            update_stats_on_completion(pool, processing_time_us, result == 0);
            
            // Update worker statistics
            worker->requests_processed++;
            
            // Clean up request
            destroy_request(request, pool);
            
            worker->active = false;
        }
    }
    
    LOG_I("Worker thread %d stopped (processed %lu requests)", 
          worker->worker_id, worker->requests_processed);
    return NULL;
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

worker_pool_config_t worker_pool_default_config(void)
{
    worker_pool_config_t config = {0};
    
    config.worker_count = WORKER_POOL_DEFAULT_WORKERS;
    config.queue_capacity = WORKER_POOL_DEFAULT_QUEUE_SIZE;
    config.shutdown_timeout_ms = WORKER_POOL_DEFAULT_SHUTDOWN_TIMEOUT;
    config.enable_priority_queue = false;
    config.enable_statistics = true;
    config.process_func = NULL;
    config.cleanup_func = NULL;
    config.context = NULL;
    
    return config;
}

worker_pool_error_t worker_pool_validate_config(const worker_pool_config_t *config)
{
    if (!config) return WORKER_POOL_ERROR_INVALID_PARAMETER;
    
    if (config->worker_count < WORKER_POOL_MIN_WORKERS || 
        config->worker_count > WORKER_POOL_MAX_WORKERS) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    if (config->queue_capacity < WORKER_POOL_MIN_QUEUE_SIZE || 
        config->queue_capacity > WORKER_POOL_MAX_QUEUE_SIZE) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    if (config->shutdown_timeout_ms < 0) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    if (!config->process_func) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    return WORKER_POOL_SUCCESS;
}

// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

worker_pool_t *worker_pool_create(const worker_pool_config_t *config)
{
    if (!config) return NULL;
    
    worker_pool_error_t error = worker_pool_validate_config(config);
    if (error != WORKER_POOL_SUCCESS) {
        LOG_E("Invalid worker pool configuration: %s", worker_pool_error_string(error));
        return NULL;
    }
    
    worker_pool_t *pool = calloc(1, sizeof(worker_pool_t));
    if (!pool) return NULL;
    
    // Copy configuration
    pool->config = *config;
    
    // Initialize state
    pool->running = false;
    pool->initialized = false;
    pool->workers = NULL;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_size = 0;
    pool->start_time_us = 0;
    
    // Initialize mutexes and condition variables
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&pool->queue_cond, NULL) != 0 ||
        pthread_mutex_init(&pool->stats_mutex, NULL) != 0 ||
        pthread_mutex_init(&pool->pool_mutex, NULL) != 0) {
        
        worker_pool_destroy(pool);
        return NULL;
    }
    
    // Initialize statistics
    memset(&pool->stats, 0, sizeof(pool->stats));
    
    pool->initialized = true;
    return pool;
}

worker_pool_error_t worker_pool_start(worker_pool_t *pool)
{
    if (!pool || !pool->initialized) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    LOCK_MUTEX(&pool->pool_mutex);
    
    if (pool->running) {
        UNLOCK_MUTEX(&pool->pool_mutex);
        return WORKER_POOL_ERROR_ALREADY_STARTED;
    }
    
    pool->start_time_us = get_current_time_us();
    
    // Create worker threads
    for (int i = 0; i < pool->config.worker_count; i++) {
        worker_thread_t *worker = calloc(1, sizeof(worker_thread_t));
        if (!worker) {
            SET_ERROR(pool, "Failed to allocate worker thread %d", i);
            UNLOCK_MUTEX(&pool->pool_mutex);
            worker_pool_stop(pool);
            return WORKER_POOL_ERROR_OUT_OF_MEMORY;
        }
        
        worker->worker_id = i;
        worker->running = true;
        worker->active = false;
        worker->pool = pool;
        worker->last_activity_us = pool->start_time_us;
        worker->requests_processed = 0;
        worker->next = pool->workers;
        pool->workers = worker;
        
        if (pthread_create(&worker->thread, NULL, worker_thread_func, worker) != 0) {
            SET_ERROR(pool, "Failed to create worker thread %d", i);
            worker->running = false;
            UNLOCK_MUTEX(&pool->pool_mutex);
            worker_pool_stop(pool);
            return WORKER_POOL_ERROR_THREAD_CREATE_FAILED;
        }
    }
    
    pool->running = true;
    UNLOCK_MUTEX(&pool->pool_mutex);
    
    LOG_I("Worker pool started with %d threads", pool->config.worker_count);
    return WORKER_POOL_SUCCESS;
}

worker_pool_error_t worker_pool_stop(worker_pool_t *pool)
{
    if (!pool || !pool->initialized) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    LOCK_MUTEX(&pool->pool_mutex);
    
    if (!pool->running) {
        UNLOCK_MUTEX(&pool->pool_mutex);
        return WORKER_POOL_ERROR_NOT_STARTED;
    }
    
    pool->running = false;
    
    // Signal all workers to stop
    worker_thread_t *worker = pool->workers;
    while (worker) {
        worker->running = false;
        worker = worker->next;
    }
    
    // Wake up all waiting workers
    pthread_cond_broadcast(&pool->queue_cond);
    
    UNLOCK_MUTEX(&pool->pool_mutex);
    
    // Wait for workers to finish
    worker = pool->workers;
    while (worker) {
        if (pthread_join(worker->thread, NULL) != 0) {
            LOG_W("Failed to join worker thread %d", worker->worker_id);
        }
        
        worker_thread_t *next = worker->next;
        free(worker);
        worker = next;
    }
    pool->workers = NULL;
    
    // Clean up remaining requests
    pthread_mutex_lock(&pool->queue_mutex);
    worker_request_t *request = pool->queue_head;
    while (request) {
        worker_request_t *next = request->next;
        destroy_request(request, pool);
        request = next;
    }
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_size = 0;
    pthread_mutex_unlock(&pool->queue_mutex);
    
    LOG_I("Worker pool stopped");
    return WORKER_POOL_SUCCESS;
}

void worker_pool_destroy(worker_pool_t *pool)
{
    if (!pool) return;
    
    if (pool->running) {
        worker_pool_stop(pool);
    }
    
    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    pthread_mutex_destroy(&pool->stats_mutex);
    pthread_mutex_destroy(&pool->pool_mutex);
    
    free(pool);
}

bool worker_pool_is_running(const worker_pool_t *pool)
{
    return pool && pool->running;
}

// ============================================================================
// REQUEST SUBMISSION FUNCTIONS
// ============================================================================

worker_pool_error_t worker_pool_submit_request(worker_pool_t *pool, 
                                              void *request_data,
                                              worker_priority_t priority)
{
    if (!pool || !pool->running) {
        return WORKER_POOL_ERROR_INVALID_PARAMETER;
    }
    
    worker_request_t *request = create_request(request_data, priority);
    if (!request) {
        return WORKER_POOL_ERROR_OUT_OF_MEMORY;
    }
    
    worker_pool_error_t result = enqueue_request(pool, request);
    if (result != WORKER_POOL_SUCCESS) {
        destroy_request(request, pool);
    }
    
    return result;
}

worker_pool_error_t worker_pool_submit_request_timeout(worker_pool_t *pool,
                                                      void *request_data,
                                                      worker_priority_t priority,
                                                      int timeout_ms)
{
    // For now, implement without timeout (can be enhanced later)
    return worker_pool_submit_request(pool, request_data, priority);
}

int worker_pool_get_queue_size(const worker_pool_t *pool)
{
    if (!pool) return -1;
    
    pthread_mutex_lock((pthread_mutex_t*)&pool->queue_mutex);
    int size = pool->queue_size;
    pthread_mutex_unlock((pthread_mutex_t*)&pool->queue_mutex);
    
    return size;
}

bool worker_pool_is_queue_full(const worker_pool_t *pool)
{
    if (!pool) return true;
    
    pthread_mutex_lock((pthread_mutex_t*)&pool->queue_mutex);
    bool full = pool->queue_size >= pool->config.queue_capacity;
    pthread_mutex_unlock((pthread_mutex_t*)&pool->queue_mutex);
    
    return full;
}

// ============================================================================
// STATISTICS AND MONITORING FUNCTIONS
// ============================================================================

worker_pool_error_t worker_pool_get_stats(const worker_pool_t *pool,
                                         worker_stats_t *stats)
{
    if (!pool || !stats) return WORKER_POOL_ERROR_INVALID_PARAMETER;
    
    pthread_mutex_lock((pthread_mutex_t*)&pool->stats_mutex);
    *stats = pool->stats;
    
    // Calculate uptime
    if (pool->start_time_us > 0) {
        stats->uptime_us = get_current_time_us() - pool->start_time_us;
    }
    
    // Count active/idle workers
    stats->active_workers = 0;
    stats->idle_workers = 0;
    
    worker_thread_t *worker = pool->workers;
    while (worker) {
        if (worker->active) {
            stats->active_workers++;
        } else {
            stats->idle_workers++;
        }
        worker = worker->next;
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&pool->stats_mutex);
    
    return WORKER_POOL_SUCCESS;
}

worker_pool_error_t worker_pool_reset_stats(worker_pool_t *pool)
{
    if (!pool) return WORKER_POOL_ERROR_INVALID_PARAMETER;
    
    pthread_mutex_lock(&pool->stats_mutex);
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->start_time_us = get_current_time_us();
    pthread_mutex_unlock(&pool->stats_mutex);
    
    return WORKER_POOL_SUCCESS;
}

int worker_pool_get_capacity(const worker_pool_t *pool)
{
    return pool ? pool->config.queue_capacity : -1;
}

int worker_pool_get_worker_count(const worker_pool_t *pool)
{
    return pool ? pool->config.worker_count : -1;
}

// ============================================================================
// ERROR HANDLING FUNCTIONS
// ============================================================================

const char *worker_pool_error_string(worker_pool_error_t error)
{
    switch (error) {
        case WORKER_POOL_SUCCESS: return "Success";
        case WORKER_POOL_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case WORKER_POOL_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case WORKER_POOL_ERROR_THREAD_CREATE_FAILED: return "Thread creation failed";
        case WORKER_POOL_ERROR_MUTEX_FAILED: return "Mutex operation failed";
        case WORKER_POOL_ERROR_ALREADY_STARTED: return "Worker pool already started";
        case WORKER_POOL_ERROR_NOT_STARTED: return "Worker pool not started";
        case WORKER_POOL_ERROR_QUEUE_FULL: return "Request queue is full";
        case WORKER_POOL_ERROR_SHUTDOWN_TIMEOUT: return "Shutdown timeout";
        case WORKER_POOL_ERROR_UNKNOWN:
        default: return "Unknown error";
    }
}

const char *worker_pool_get_last_error(const worker_pool_t *pool)
{
    if (!pool) return "Invalid worker pool context";
    return pool->last_error[0] ? pool->last_error : NULL;
}