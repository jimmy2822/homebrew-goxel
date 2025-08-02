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

#include "socket_server.h"
#include "json_socket_handler.h"
#include "../log.h"

// External declaration for JSON client handler
extern void json_socket_client_handler(socket_server_t *server,
                                     socket_client_t *client,
                                     bool connected,
                                     void *user_data);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <netinet/in.h>

// ============================================================================
// CONSTANTS AND LIMITS
// ============================================================================

#define SOCKET_MAX_PATH_LEN 108         /**< Maximum Unix socket path length */
#define SOCKET_BUFFER_INITIAL_SIZE 4096 /**< Initial buffer size per client */
#define SOCKET_MAX_MESSAGE_SIZE (1024 * 1024) /**< Maximum message size (1MB) */
#define SOCKET_DEFAULT_TIMEOUT_MS 30000 /**< Default timeout (30 seconds) */
#define SOCKET_DEFAULT_BACKLOG 128      /**< Default listen backlog */
#define SOCKET_DEFAULT_MAX_CONNECTIONS 256 /**< Default max connections */
#define SOCKET_MESSAGE_HEADER_SIZE 16   /**< Message header size in bytes */
#define SOCKET_STATS_RESET_VALUE 0      /**< Value for resetting statistics */

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

/**
 * Thread pool worker thread structure.
 */
typedef struct socket_worker {
    pthread_t thread;                   /**< Worker thread */
    socket_server_t *server;            /**< Reference to server */
    int worker_id;                      /**< Worker thread ID */
    bool running;                       /**< Worker running flag */
    struct socket_worker *next;         /**< Next worker in list */
} socket_worker_t;

/**
 * Work queue item for thread pool.
 */
typedef struct socket_work_item {
    socket_client_t *client;            /**< Client for this work item */
    socket_message_t *message;          /**< Message to process */
    struct socket_work_item *next;      /**< Next item in queue */
} socket_work_item_t;

/**
 * Internal socket server structure.
 */
struct socket_server {
    // Configuration
    socket_server_config_t config;      /**< Server configuration */
    char *socket_path;                  /**< Allocated socket path */
    
    // Server state
    int server_fd;                      /**< Server socket file descriptor */
    bool running;                       /**< Server running flag */
    bool initialized;                   /**< Server initialization flag */
    
    // Threading
    pthread_t accept_thread;            /**< Accept connections thread */
    pthread_mutex_t server_mutex;       /**< Server state mutex */
    pthread_mutex_t clients_mutex;      /**< Client list mutex */
    pthread_mutex_t stats_mutex;        /**< Statistics mutex */
    pthread_cond_t work_cond;           /**< Work queue condition */
    pthread_mutex_t work_mutex;         /**< Work queue mutex */
    
    // Client management
    socket_client_t **clients;          /**< Array of client pointers */
    int client_count;                   /**< Current number of clients */
    uint32_t next_client_id;            /**< Next client ID to assign */
    
    // Thread pool (if enabled)
    socket_worker_t *workers;           /**< Worker thread list */
    socket_work_item_t *work_queue;     /**< Work queue head */
    socket_work_item_t *work_queue_tail; /**< Work queue tail */
    int work_queue_size;                /**< Current work queue size */
    
    // Error handling
    char last_error[256];               /**< Last error message */
    
    // Statistics
    socket_server_stats_t stats;        /**< Server statistics */
};

// ============================================================================
// UTILITY MACROS
// ============================================================================

#define SET_ERROR(server, fmt, ...) do { \
    snprintf((server)->last_error, sizeof((server)->last_error), fmt, ##__VA_ARGS__); \
    LOG_E("Socket Server: " fmt, ##__VA_ARGS__); \
} while(0)

#define LOCK_MUTEX(mutex) do { \
    if (pthread_mutex_lock(mutex) != 0) { \
        LOG_E("Mutex lock failed: %s", strerror(errno)); \
        return SOCKET_ERROR_MUTEX_FAILED; \
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

typedef struct {
    socket_server_t *server;
    socket_client_t *client;
} client_thread_data_t;

static void *accept_thread_func(void *arg);
static void *worker_thread_func(void *arg);
static void *client_connection_thread(void *arg);
static socket_error_t handle_client_connection(socket_server_t *server, 
                                              socket_client_t *client);
static socket_error_t write_message_to_client(socket_client_t *client,
                                             const socket_message_t *message);
static socket_client_t *create_client(socket_server_t *server, int client_fd);
static void destroy_client(socket_client_t *client);
static socket_error_t add_client(socket_server_t *server, socket_client_t *client);
static socket_error_t remove_client(socket_server_t *server, socket_client_t *client);
static socket_error_t setup_socket_options(int fd);
static int64_t get_current_time_us(void);
static socket_error_t handle_binary_client(socket_server_t *server,
                                          socket_client_t *client);
static socket_error_t handle_json_client(socket_server_t *server,
                                        socket_client_t *client);
static socket_error_t read_binary_message_from_client(socket_client_t *client,
                                                     socket_message_t **message);

// ============================================================================
// ERROR HANDLING IMPLEMENTATION
// ============================================================================

const char *socket_error_string(socket_error_t error)
{
    switch (error) {
        case SOCKET_SUCCESS: return "Success";
        case SOCKET_ERROR_INVALID_CONTEXT: return "Invalid context";
        case SOCKET_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case SOCKET_ERROR_SOCKET_CREATE_FAILED: return "Socket creation failed";
        case SOCKET_ERROR_BIND_FAILED: return "Socket bind failed";
        case SOCKET_ERROR_LISTEN_FAILED: return "Socket listen failed";
        case SOCKET_ERROR_ACCEPT_FAILED: return "Socket accept failed";
        case SOCKET_ERROR_WRITE_FAILED: return "Socket write failed";
        case SOCKET_ERROR_READ_FAILED: return "Socket read failed";
        case SOCKET_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case SOCKET_ERROR_THREAD_CREATE_FAILED: return "Thread creation failed";
        case SOCKET_ERROR_MUTEX_FAILED: return "Mutex operation failed";
        case SOCKET_ERROR_SHUTDOWN_FAILED: return "Server shutdown failed";
        case SOCKET_ERROR_ALREADY_RUNNING: return "Server already running";
        case SOCKET_ERROR_NOT_RUNNING: return "Server not running";
        case SOCKET_ERROR_PERMISSION_DENIED: return "Permission denied";
        case SOCKET_ERROR_PATH_TOO_LONG: return "Socket path too long";
        case SOCKET_ERROR_CONNECTION_LOST: return "Connection lost";
        case SOCKET_ERROR_TIMEOUT: return "Operation timed out";
        case SOCKET_ERROR_UNKNOWN:
        default: return "Unknown error";
    }
}

const char *socket_server_get_last_error(const socket_server_t *server)
{
    if (!server) return "Invalid server context";
    return server->last_error[0] ? server->last_error : NULL;
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

socket_server_config_t socket_server_default_config(void)
{
    socket_server_config_t config = {0};
    
    config.socket_path = "/tmp/goxel-daemon.sock";
    config.max_connections = SOCKET_DEFAULT_MAX_CONNECTIONS;
    config.listen_backlog = SOCKET_DEFAULT_BACKLOG;
    config.receive_timeout_ms = SOCKET_DEFAULT_TIMEOUT_MS;
    config.send_timeout_ms = SOCKET_DEFAULT_TIMEOUT_MS;
    config.max_message_size = SOCKET_MAX_MESSAGE_SIZE;
    config.buffer_initial_size = SOCKET_BUFFER_INITIAL_SIZE;
    config.auto_reconnect = false;
    config.thread_per_client = false;
    config.thread_pool_size = 4;
    config.msg_handler = NULL;
    config.client_handler = NULL;
    config.user_data = NULL;
    
    return config;
}

socket_error_t socket_server_validate_config(const socket_server_config_t *config)
{
    if (!config) return SOCKET_ERROR_INVALID_PARAMETER;
    
    if (!config->socket_path || strlen(config->socket_path) == 0) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    if (strlen(config->socket_path) >= SOCKET_MAX_PATH_LEN) {
        return SOCKET_ERROR_PATH_TOO_LONG;
    }
    
    if (config->max_connections <= 0 || config->max_connections > 65536) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    if (config->listen_backlog <= 0) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    if (config->max_message_size <= 0 || config->max_message_size > (100 * 1024 * 1024)) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    if (config->buffer_initial_size <= 0) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    if (!config->thread_per_client && config->thread_pool_size <= 0) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    return SOCKET_SUCCESS;
}

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

static socket_error_t setup_socket_options(int fd)
{
    int opt = 1;
    
    // Set socket to non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return SOCKET_ERROR_SOCKET_CREATE_FAILED;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return SOCKET_ERROR_SOCKET_CREATE_FAILED;
    }
    
    // Set socket options
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return SOCKET_ERROR_SOCKET_CREATE_FAILED;
    }
    
    return SOCKET_SUCCESS;
}

bool socket_server_path_available(const char *path)
{
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        // Path exists, check if it's a socket
        if (S_ISSOCK(st.st_mode)) {
            // Try to connect to see if it's active
            int test_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (test_fd == -1) return false;
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
            
            int result = connect(test_fd, (struct sockaddr*)&addr, sizeof(addr));
            close(test_fd);
            
            return result != 0; // Available if connection failed
        }
        return false; // Path exists but not a socket
    }
    
    return errno == ENOENT; // Available if path doesn't exist
}

socket_error_t socket_server_cleanup_path(const char *path)
{
    if (!path) return SOCKET_ERROR_INVALID_PARAMETER;
    
    if (unlink(path) == 0 || errno == ENOENT) {
        return SOCKET_SUCCESS;
    }
    
    if (errno == EACCES) {
        return SOCKET_ERROR_PERMISSION_DENIED;
    }
    
    return SOCKET_ERROR_UNKNOWN;
}

// ============================================================================
// MESSAGE FUNCTIONS
// ============================================================================

socket_message_t *socket_message_create(uint32_t id, uint32_t type, 
                                       const char *data, uint32_t length)
{
    socket_message_t *message = calloc(1, sizeof(socket_message_t));
    if (!message) return NULL;
    
    message->id = id;
    message->type = type;
    message->length = length;
    message->timestamp = get_current_time_us();
    
    if (length > 0 && data) {
        message->data = malloc(length + 1); // +1 for null terminator
        if (!message->data) {
            free(message);
            return NULL;
        }
        memcpy(message->data, data, length);
        message->data[length] = '\0'; // Null terminate for safety
    } else {
        message->data = NULL;
    }
    
    return message;
}

socket_message_t *socket_message_create_json(uint32_t id, uint32_t type,
                                            const char *json_data)
{
    if (!json_data) return NULL;
    return socket_message_create(id, type, json_data, strlen(json_data));
}

void socket_message_destroy(socket_message_t *message)
{
    if (!message) return;
    
    SAFE_FREE(message->data);
    free(message);
}

// ============================================================================
// CLIENT MANAGEMENT
// ============================================================================

static socket_client_t *create_client(socket_server_t *server, int client_fd)
{
    if (!server || client_fd < 0) return NULL;
    
    socket_client_t *client = calloc(1, sizeof(socket_client_t));
    if (!client) return NULL;
    
    client->fd = client_fd;
    client->id = server->next_client_id++;
    client->connect_time = get_current_time_us();
    client->buffer_capacity = server->config.buffer_initial_size;
    client->buffer = malloc(client->buffer_capacity);
    client->authenticated = false;
    client->user_data = NULL;
    client->protocol = PROTOCOL_BINARY;  // Default to binary, will be detected later
    
    // Initialize protocol-specific data
    memset(&client->handler_data, 0, sizeof(client->handler_data));
    
    if (!client->buffer) {
        free(client);
        return NULL;
    }
    
    // Get client credentials if available (Unix domain sockets)
#ifdef __linux__
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
        client->pid = cred.pid;
        client->uid = cred.uid;
        client->gid = cred.gid;
    }
#else
    // On non-Linux systems, set default values
    client->pid = 0;
    client->uid = 0;
    client->gid = 0;
#endif
    
    return client;
}

static void destroy_client(socket_client_t *client)
{
    if (!client) return;
    
    if (client->fd >= 0) {
        close(client->fd);
    }
    
    SAFE_FREE(client->buffer);
    free(client);
}

static socket_error_t add_client(socket_server_t *server, socket_client_t *client)
{
    if (!server || !client) return SOCKET_ERROR_INVALID_PARAMETER;
    
    LOCK_MUTEX(&server->clients_mutex);
    
    if (server->client_count >= server->config.max_connections) {
        UNLOCK_MUTEX(&server->clients_mutex);
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    // Find empty slot
    for (int i = 0; i < server->config.max_connections; i++) {
        if (!server->clients[i]) {
            server->clients[i] = client;
            server->client_count++;
            
            // Update statistics
            pthread_mutex_lock(&server->stats_mutex);
            server->stats.total_connections++;
            server->stats.current_connections = server->client_count;
            pthread_mutex_unlock(&server->stats_mutex);
            
            UNLOCK_MUTEX(&server->clients_mutex);
            
            // Call client handler if set
            if (server->config.client_handler) {
                server->config.client_handler(server, client, true, 
                                            server->config.user_data);
            }
            
            return SOCKET_SUCCESS;
        }
    }
    
    UNLOCK_MUTEX(&server->clients_mutex);
    return SOCKET_ERROR_OUT_OF_MEMORY;
}

static socket_error_t remove_client(socket_server_t *server, socket_client_t *client)
{
    if (!server || !client) return SOCKET_ERROR_INVALID_PARAMETER;
    
    LOCK_MUTEX(&server->clients_mutex);
    
    // Find and remove client
    for (int i = 0; i < server->config.max_connections; i++) {
        if (server->clients[i] == client) {
            server->clients[i] = NULL;
            server->client_count--;
            
            // Update statistics
            pthread_mutex_lock(&server->stats_mutex);
            server->stats.current_connections = server->client_count;
            pthread_mutex_unlock(&server->stats_mutex);
            
            UNLOCK_MUTEX(&server->clients_mutex);
            
            // Call client handler if set
            if (server->config.client_handler) {
                server->config.client_handler(server, client, false,
                                            server->config.user_data);
            }
            
            return SOCKET_SUCCESS;
        }
    }
    
    UNLOCK_MUTEX(&server->clients_mutex);
    return SOCKET_ERROR_INVALID_PARAMETER;
}

void socket_client_set_user_data(socket_client_t *client, void *user_data)
{
    if (client) {
        client->user_data = user_data;
    }
}

void *socket_client_get_user_data(const socket_client_t *client)
{
    return client ? client->user_data : NULL;
}

// ============================================================================
// SERVER LIFECYCLE IMPLEMENTATION
// ============================================================================

socket_server_t *socket_server_create(const socket_server_config_t *config)
{
    if (!config) return NULL;
    
    socket_error_t error = socket_server_validate_config(config);
    if (error != SOCKET_SUCCESS) {
        LOG_E("Invalid socket server configuration: %s", socket_error_string(error));
        return NULL;
    }
    
    socket_server_t *server = calloc(1, sizeof(socket_server_t));
    if (!server) return NULL;
    
    // Copy configuration
    server->config = *config;
    server->socket_path = strdup(config->socket_path);
    if (!server->socket_path) {
        free(server);
        return NULL;
    }
    
    // Initialize server state
    server->server_fd = -1;
    server->running = false;
    server->initialized = false;
    server->client_count = 0;
    server->next_client_id = 1;
    server->work_queue = NULL;
    server->work_queue_tail = NULL;
    server->work_queue_size = 0;
    server->workers = NULL;
    
    // Allocate client array
    server->clients = calloc(config->max_connections, sizeof(socket_client_t*));
    if (!server->clients) {
        SAFE_FREE(server->socket_path);
        free(server);
        return NULL;
    }
    
    // Initialize mutexes and condition variables
    if (pthread_mutex_init(&server->server_mutex, NULL) != 0 ||
        pthread_mutex_init(&server->clients_mutex, NULL) != 0 ||
        pthread_mutex_init(&server->stats_mutex, NULL) != 0 ||
        pthread_mutex_init(&server->work_mutex, NULL) != 0 ||
        pthread_cond_init(&server->work_cond, NULL) != 0) {
        
        socket_server_destroy(server);
        return NULL;
    }
    
    // Initialize statistics
    memset(&server->stats, 0, sizeof(server->stats));
    server->stats.start_time = get_current_time_us();
    
    server->initialized = true;
    return server;
}

socket_error_t socket_server_start(socket_server_t *server)
{
    if (!server || !server->initialized) {
        return SOCKET_ERROR_INVALID_CONTEXT;
    }
    
    LOCK_MUTEX(&server->server_mutex);
    
    if (server->running) {
        UNLOCK_MUTEX(&server->server_mutex);
        return SOCKET_ERROR_ALREADY_RUNNING;
    }
    
    // Create Unix domain socket
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        SET_ERROR(server, "Failed to create socket: %s", strerror(errno));
        UNLOCK_MUTEX(&server->server_mutex);
        return SOCKET_ERROR_SOCKET_CREATE_FAILED;
    }
    
    // Set socket options
    socket_error_t result = setup_socket_options(server->server_fd);
    if (result != SOCKET_SUCCESS) {
        SET_ERROR(server, "Failed to set socket options");
        close(server->server_fd);
        server->server_fd = -1;
        UNLOCK_MUTEX(&server->server_mutex);
        return result;
    }
    
    // Clean up existing socket file if needed
    socket_server_cleanup_path(server->socket_path);
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server->socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        SET_ERROR(server, "Failed to bind socket to %s: %s", 
                 server->socket_path, strerror(errno));
        close(server->server_fd);
        server->server_fd = -1;
        UNLOCK_MUTEX(&server->server_mutex);
        
        if (errno == EACCES) {
            return SOCKET_ERROR_PERMISSION_DENIED;
        }
        return SOCKET_ERROR_BIND_FAILED;
    }
    
    // Set socket permissions
    if (chmod(server->socket_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1) {
        LOG_W("Failed to set socket permissions: %s", strerror(errno));
    }
    
    // Start listening
    if (listen(server->server_fd, server->config.listen_backlog) == -1) {
        SET_ERROR(server, "Failed to listen on socket: %s", strerror(errno));
        close(server->server_fd);
        server->server_fd = -1;
        unlink(server->socket_path);
        UNLOCK_MUTEX(&server->server_mutex);
        return SOCKET_ERROR_LISTEN_FAILED;
    }
    
    // Start worker threads if using thread pool
    if (!server->config.thread_per_client) {
        for (int i = 0; i < server->config.thread_pool_size; i++) {
            socket_worker_t *worker = calloc(1, sizeof(socket_worker_t));
            if (!worker) {
                SET_ERROR(server, "Failed to allocate worker thread");
                UNLOCK_MUTEX(&server->server_mutex);
                socket_server_stop(server);
                return SOCKET_ERROR_OUT_OF_MEMORY;
            }
            
            worker->server = server;
            worker->worker_id = i;
            worker->running = true;
            worker->next = server->workers;
            server->workers = worker;
            
            // Configure thread attributes for reduced memory usage
            pthread_attr_t thread_attr;
            if (pthread_attr_init(&thread_attr) != 0) {
                SET_ERROR(server, "Failed to initialize thread attributes for worker %d", i);
                worker->running = false;
                UNLOCK_MUTEX(&server->server_mutex);
                socket_server_stop(server);
                return SOCKET_ERROR_THREAD_CREATE_FAILED;
            }
            
            // Set smaller stack size (256KB instead of default 8MB)
            size_t stack_size = 256 * 1024; // 256KB
            if (pthread_attr_setstacksize(&thread_attr, stack_size) != 0) {
                LOG_W("Failed to set stack size for socket worker thread %d, using default", i);
            }
            
            if (pthread_create(&worker->thread, &thread_attr, worker_thread_func, worker) != 0) {
                SET_ERROR(server, "Failed to create worker thread %d", i);
                worker->running = false;
                pthread_attr_destroy(&thread_attr);
                UNLOCK_MUTEX(&server->server_mutex);
                socket_server_stop(server);
                return SOCKET_ERROR_THREAD_CREATE_FAILED;
            }
            
            pthread_attr_destroy(&thread_attr);
        }
    }
    
    // Start accept thread with optimized stack size
    server->running = true;
    
    pthread_attr_t accept_attr;
    if (pthread_attr_init(&accept_attr) == 0) {
        // Accept thread needs minimal stack (128KB)
        size_t accept_stack_size = 128 * 1024; // 128KB
        if (pthread_attr_setstacksize(&accept_attr, accept_stack_size) != 0) {
            LOG_W("Failed to set stack size for accept thread, using default");
        }
    }
    
    if (pthread_create(&server->accept_thread, &accept_attr, accept_thread_func, server) != 0) {
        SET_ERROR(server, "Failed to create accept thread");
        server->running = false;
        pthread_attr_destroy(&accept_attr);
        UNLOCK_MUTEX(&server->server_mutex);
        socket_server_stop(server);
        return SOCKET_ERROR_THREAD_CREATE_FAILED;
    }
    
    pthread_attr_destroy(&accept_attr);
    
    UNLOCK_MUTEX(&server->server_mutex);
    
    LOG_I("Socket server started on %s", server->socket_path);
    return SOCKET_SUCCESS;
}

socket_error_t socket_server_stop(socket_server_t *server)
{
    if (!server || !server->initialized) {
        return SOCKET_ERROR_INVALID_CONTEXT;
    }
    
    LOCK_MUTEX(&server->server_mutex);
    
    if (!server->running) {
        UNLOCK_MUTEX(&server->server_mutex);
        return SOCKET_ERROR_NOT_RUNNING;
    }
    
    server->running = false;
    
    // Close server socket to break accept loop
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
    
    UNLOCK_MUTEX(&server->server_mutex);
    
    // Wait for accept thread to finish
    if (pthread_join(server->accept_thread, NULL) != 0) {
        LOG_W("Failed to join accept thread");
    }
    
    // Stop worker threads
    if (!server->config.thread_per_client) {
        socket_worker_t *worker = server->workers;
        while (worker) {
            worker->running = false;
            worker = worker->next;
        }
        
        // Wake up all workers
        pthread_cond_broadcast(&server->work_cond);
        
        // Wait for workers to finish
        worker = server->workers;
        while (worker) {
            if (pthread_join(worker->thread, NULL) != 0) {
                LOG_W("Failed to join worker thread %d", worker->worker_id);
            }
            socket_worker_t *next = worker->next;
            free(worker);
            worker = next;
        }
        server->workers = NULL;
    }
    
    // Disconnect all clients
    LOCK_MUTEX(&server->clients_mutex);
    for (int i = 0; i < server->config.max_connections; i++) {
        if (server->clients[i]) {
            destroy_client(server->clients[i]);
            server->clients[i] = NULL;
        }
    }
    server->client_count = 0;
    UNLOCK_MUTEX(&server->clients_mutex);
    
    // Clean up work queue
    LOCK_MUTEX(&server->work_mutex);
    socket_work_item_t *item = server->work_queue;
    while (item) {
        socket_work_item_t *next = item->next;
        socket_message_destroy(item->message);
        free(item);
        item = next;
    }
    server->work_queue = NULL;
    server->work_queue_tail = NULL;
    server->work_queue_size = 0;
    UNLOCK_MUTEX(&server->work_mutex);
    
    // Clean up socket file
    if (server->socket_path) {
        unlink(server->socket_path);
    }
    
    LOG_I("Socket server stopped");
    return SOCKET_SUCCESS;
}

void socket_server_destroy(socket_server_t *server)
{
    if (!server) return;
    
    if (server->running) {
        socket_server_stop(server);
    }
    
    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&server->server_mutex);
    pthread_mutex_destroy(&server->clients_mutex);
    pthread_mutex_destroy(&server->stats_mutex);
    pthread_mutex_destroy(&server->work_mutex);
    pthread_cond_destroy(&server->work_cond);
    
    // Free resources
    SAFE_FREE(server->socket_path);
    SAFE_FREE(server->clients);
    free(server);
}

bool socket_server_is_running(const socket_server_t *server)
{
    return server && server->running;
}

const char *socket_server_get_path(const socket_server_t *server)
{
    return server ? server->socket_path : NULL;
}

// ============================================================================
// THREAD FUNCTION IMPLEMENTATIONS
// ============================================================================

static void *accept_thread_func(void *arg)
{
    socket_server_t *server = (socket_server_t*)arg;
    struct sockaddr_un client_addr;
    socklen_t client_addr_len;
    
    LOG_I("Accept thread started");
    
    while (server->running) {
        client_addr_len = sizeof(client_addr);
        
        // Use poll to check for incoming connections with timeout
        struct pollfd pfd;
        pfd.fd = server->server_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        
        int poll_result = poll(&pfd, 1, 1000); // 1 second timeout
        
        if (poll_result < 0) {
            if (errno == EINTR) continue; // Interrupted, retry
            if (server->running) {
                SET_ERROR(server, "Poll failed: %s", strerror(errno));
            }
            break;
        }
        
        if (poll_result == 0) continue; // Timeout, check running flag
        
        // Accept new connection
        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, 
                              &client_addr_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (errno == EINTR) continue;
            
            if (server->running) {
                SET_ERROR(server, "Accept failed: %s", strerror(errno));
                pthread_mutex_lock(&server->stats_mutex);
                server->stats.connection_errors++;
                pthread_mutex_unlock(&server->stats_mutex);
            }
            continue;
        }
        
        // Create client structure
        socket_client_t *client = create_client(server, client_fd);
        if (!client) {
            SET_ERROR(server, "Failed to create client structure");
            close(client_fd);
            continue;
        }
        
        // Add client to server
        socket_error_t result = add_client(server, client);
        if (result != SOCKET_SUCCESS) {
            LOG_W("Failed to add client: %s", socket_error_string(result));
            destroy_client(client);
            continue;
        }
        
        LOG_I("Client connected: ID=%u, PID=%d, UID=%d", 
              client->id, client->pid, client->uid);
        
        // Handle client connection based on configuration
        if (server->config.thread_per_client) {
            // Create thread per client
            client_thread_data_t *thread_data = malloc(sizeof(client_thread_data_t));
            if (!thread_data) {
                SET_ERROR(server, "Failed to allocate thread data");
                remove_client(server, client);
                destroy_client(client);
                continue;
            }
            
            thread_data->server = server;
            thread_data->client = client;
            
            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, 
                             client_connection_thread, 
                             thread_data) != 0) {
                SET_ERROR(server, "Failed to create client thread");
                free(thread_data);
                remove_client(server, client);
                destroy_client(client);
                continue;
            }
            pthread_detach(client_thread);
        } else {
            // Use thread pool - need to detect protocol and start appropriate handler
            // Do protocol detection in a separate thread to avoid blocking accept
            client_thread_data_t *thread_data = malloc(sizeof(client_thread_data_t));
            if (!thread_data) {
                SET_ERROR(server, "Failed to allocate thread data");
                remove_client(server, client);
                destroy_client(client);
                continue;
            }
            
            thread_data->server = server;
            thread_data->client = client;
            
            pthread_t protocol_thread;
            if (pthread_create(&protocol_thread, NULL, 
                             client_connection_thread, 
                             thread_data) != 0) {
                SET_ERROR(server, "Failed to create protocol detection thread");
                free(thread_data);
                remove_client(server, client);
                destroy_client(client);
                continue;
            }
            pthread_detach(protocol_thread);
        }
    }
    
    LOG_I("Accept thread stopped");
    return NULL;
}

static void *worker_thread_func(void *arg)
{
    socket_worker_t *worker = (socket_worker_t*)arg;
    socket_server_t *server = worker->server;
    
    LOG_I("Worker thread %d started", worker->worker_id);
    
    while (worker->running) {
        socket_work_item_t *work_item = NULL;
        
        // Get work from queue
        pthread_mutex_lock(&server->work_mutex);
        while (server->work_queue == NULL && worker->running) {
            pthread_cond_wait(&server->work_cond, &server->work_mutex);
        }
        
        if (!worker->running) {
            pthread_mutex_unlock(&server->work_mutex);
            break;
        }
        
        // Dequeue work item
        work_item = server->work_queue;
        if (work_item) {
            server->work_queue = work_item->next;
            if (server->work_queue == NULL) {
                server->work_queue_tail = NULL;
            }
            server->work_queue_size--;
        }
        
        pthread_mutex_unlock(&server->work_mutex);
        
        if (work_item) {
            // Process the message
            if (server->config.msg_handler) {
                socket_message_t *response = server->config.msg_handler(
                    server, work_item->client, work_item->message, 
                    server->config.user_data);
                
                if (response) {
                    socket_error_t send_result = socket_server_send_message(
                        server, work_item->client, response);
                    if (send_result != SOCKET_SUCCESS) {
                        LOG_W("Failed to send response to client %u: %s",
                             work_item->client->id, socket_error_string(send_result));
                    }
                    socket_message_destroy(response);
                }
            }
            
            // Clean up work item
            socket_message_destroy(work_item->message);
            free(work_item);
        }
    }
    
    LOG_I("Worker thread %d stopped", worker->worker_id);
    return NULL;
}

static void *client_connection_thread(void *arg)
{
    client_thread_data_t *data = (client_thread_data_t *)arg;
    socket_server_t *server = data->server;
    socket_client_t *client = data->client;
    
    free(data);  // Free the thread data
    
    handle_client_connection(server, client);
    return NULL;
}

static socket_error_t handle_client_connection(socket_server_t *server, 
                                              socket_client_t *client)
{
    LOG_I("Handling client connection: ID=%u", client->id);
    
    // Peek at first few bytes to detect protocol
    char magic[4];
    ssize_t peeked = recv(client->fd, magic, 4, MSG_PEEK);
    
    if (peeked >= 2 && magic[0] == '{' && magic[1] == '"') {
        client->protocol = PROTOCOL_JSON_RPC;
        LOG_I("Client %u detected as JSON-RPC protocol", client->id);
        // For JSON clients, the monitor thread handles everything including cleanup
        return handle_json_client(server, client);
    } else {
        client->protocol = PROTOCOL_BINARY;
        LOG_I("Client %u detected as binary protocol", client->id);
        return handle_binary_client(server, client);
    }
}

static socket_error_t handle_binary_client(socket_server_t *server,
                                          socket_client_t *client)
{
    LOG_I("Handling binary client: ID=%u", client->id);
    
    while (server->running) {
        socket_message_t *message = NULL;
        socket_error_t result = read_binary_message_from_client(client, &message);
        
        if (result == SOCKET_ERROR_CONNECTION_LOST) {
            LOG_I("Client %u disconnected", client->id);
            break;
        }
        
        if (result != SOCKET_SUCCESS) {
            LOG_W("Failed to read message from client %u: %s",
                 client->id, socket_error_string(result));
            
            pthread_mutex_lock(&server->stats_mutex);
            server->stats.message_errors++;
            pthread_mutex_unlock(&server->stats_mutex);
            
            break;
        }
        
        if (!message) continue; // No complete message yet
        
        // Update statistics
        pthread_mutex_lock(&server->stats_mutex);
        server->stats.messages_received++;
        server->stats.bytes_received += message->length;
        pthread_mutex_unlock(&server->stats_mutex);
        
        // Process message
        if (server->config.msg_handler) {
            socket_message_t *response = server->config.msg_handler(
                server, client, message, server->config.user_data);
            
            if (response) {
                socket_error_t send_result = socket_server_send_message(
                    server, client, response);
                if (send_result != SOCKET_SUCCESS) {
                    LOG_W("Failed to send response to client %u: %s",
                         client->id, socket_error_string(send_result));
                }
                socket_message_destroy(response);
            }
        }
        
        socket_message_destroy(message);
    }
    
    // Clean up client
    remove_client(server, client);
    destroy_client(client);
    
    return SOCKET_SUCCESS;
}

static socket_error_t handle_json_client(socket_server_t *server,
                                        socket_client_t *client)
{
    LOG_I("Handling JSON client: ID=%u", client->id);
    
    // For JSON clients, we need to start the JSON monitor thread
    // Call the JSON socket client handler directly
    json_socket_client_handler(server, client, true, server->config.user_data);
    
    // For JSON clients, the monitor thread handles all I/O
    // We don't need to wait here - the monitor thread will handle cleanup
    // when the client disconnects
    
    LOG_I("JSON client %u monitor thread started, returning from handler", client->id);
    return SOCKET_SUCCESS;
}

// ============================================================================
// MESSAGE I/O IMPLEMENTATION
// ============================================================================

static socket_error_t read_binary_message_from_client(socket_client_t *client,
                                                     socket_message_t **message)
{
    if (!client || !message) return SOCKET_ERROR_INVALID_PARAMETER;
    
    *message = NULL;
    
    // Read data into client buffer
    while (true) {
        ssize_t bytes_read = recv(client->fd, 
                                 client->buffer + client->buffer_size,
                                 client->buffer_capacity - client->buffer_size - 1,
                                 MSG_DONTWAIT);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more data available
            }
            if (errno == EINTR) continue;
            if (errno == ECONNRESET || errno == EPIPE) {
                return SOCKET_ERROR_CONNECTION_LOST;
            }
            return SOCKET_ERROR_READ_FAILED;
        }
        
        if (bytes_read == 0) {
            return SOCKET_ERROR_CONNECTION_LOST;
        }
        
        client->buffer_size += bytes_read;
        client->buffer[client->buffer_size] = '\0';
        
        // Expand buffer if needed
        if (client->buffer_size >= client->buffer_capacity - 1) {
            size_t new_capacity = client->buffer_capacity * 2;
            char *new_buffer = realloc(client->buffer, new_capacity);
            if (!new_buffer) {
                return SOCKET_ERROR_OUT_OF_MEMORY;
            }
            client->buffer = new_buffer;
            client->buffer_capacity = new_capacity;
        }
    }
    
    // Try to parse a complete message from buffer
    if (client->buffer_size < SOCKET_MESSAGE_HEADER_SIZE) {
        return SOCKET_SUCCESS; // Need more data
    }
    
    // Parse message header (assuming network byte order)
    uint32_t msg_id, msg_type, msg_length, timestamp_high;
    int64_t timestamp;
    
    memcpy(&msg_id, client->buffer, sizeof(uint32_t));
    memcpy(&msg_type, client->buffer + 4, sizeof(uint32_t));
    memcpy(&msg_length, client->buffer + 8, sizeof(uint32_t));
    memcpy(&timestamp_high, client->buffer + 12, sizeof(uint32_t));
    
    // Convert from network byte order if needed
    msg_id = ntohl(msg_id);
    msg_type = ntohl(msg_type);
    msg_length = ntohl(msg_length);
    timestamp_high = ntohl(timestamp_high);
    
    // Reconstruct 64-bit timestamp (simplified - just use high part for now)
    timestamp = (int64_t)timestamp_high << 32;
    
    // Check if we have the complete message
    size_t total_message_size = SOCKET_MESSAGE_HEADER_SIZE + msg_length;
    if (client->buffer_size < total_message_size) {
        return SOCKET_SUCCESS; // Need more data
    }
    
    // Create message structure
    const char *msg_data = msg_length > 0 ? 
                          client->buffer + SOCKET_MESSAGE_HEADER_SIZE : NULL;
    *message = socket_message_create(msg_id, msg_type, msg_data, msg_length);
    if (!*message) {
        return SOCKET_ERROR_OUT_OF_MEMORY;
    }
    
    (*message)->timestamp = timestamp;
    
    // Remove processed message from buffer
    size_t remaining = client->buffer_size - total_message_size;
    if (remaining > 0) {
        memmove(client->buffer, client->buffer + total_message_size, remaining);
    }
    client->buffer_size = remaining;
    client->buffer[client->buffer_size] = '\0';
    
    return SOCKET_SUCCESS;
}

static socket_error_t write_message_to_client(socket_client_t *client,
                                             const socket_message_t *message)
{
    if (!client || !message) return SOCKET_ERROR_INVALID_PARAMETER;
    
    // Prepare message header - fix for 64-bit timestamp in 16-byte header
    char header[SOCKET_MESSAGE_HEADER_SIZE];
    uint32_t msg_id = htonl(message->id);
    uint32_t msg_type = htonl(message->type);
    uint32_t msg_length = htonl(message->length);
    uint32_t timestamp_high = htonl((uint32_t)(message->timestamp >> 32));
    
    memcpy(header, &msg_id, sizeof(uint32_t));
    memcpy(header + 4, &msg_type, sizeof(uint32_t));
    memcpy(header + 8, &msg_length, sizeof(uint32_t));
    memcpy(header + 12, &timestamp_high, sizeof(uint32_t));
    
    // Send header
    ssize_t bytes_sent = send(client->fd, header, SOCKET_MESSAGE_HEADER_SIZE, MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            return SOCKET_ERROR_CONNECTION_LOST;
        }
        return SOCKET_ERROR_WRITE_FAILED;
    }
    
    if (bytes_sent != SOCKET_MESSAGE_HEADER_SIZE) {
        return SOCKET_ERROR_WRITE_FAILED;
    }
    
    // Send payload if present
    if (message->length > 0 && message->data) {
        bytes_sent = send(client->fd, message->data, message->length, MSG_NOSIGNAL);
        if (bytes_sent < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return SOCKET_ERROR_CONNECTION_LOST;
            }
            return SOCKET_ERROR_WRITE_FAILED;
        }
        
        if ((uint32_t)bytes_sent != message->length) {
            return SOCKET_ERROR_WRITE_FAILED;
        }
    }
    
    return SOCKET_SUCCESS;
}

socket_error_t socket_server_send_message(socket_server_t *server,
                                         socket_client_t *client,
                                         const socket_message_t *message)
{
    if (!server || !client || !message) {
        return SOCKET_ERROR_INVALID_PARAMETER;
    }
    
    socket_error_t result = write_message_to_client(client, message);
    
    if (result == SOCKET_SUCCESS) {
        // Update statistics
        pthread_mutex_lock(&server->stats_mutex);
        server->stats.messages_sent++;
        server->stats.bytes_sent += message->length + SOCKET_MESSAGE_HEADER_SIZE;
        pthread_mutex_unlock(&server->stats_mutex);
    }
    
    return result;
}

socket_error_t socket_server_broadcast_message(socket_server_t *server,
                                              const socket_message_t *message)
{
    if (!server || !message) return SOCKET_ERROR_INVALID_PARAMETER;
    
    socket_error_t last_error = SOCKET_SUCCESS;
    int successful_sends = 0;
    
    LOCK_MUTEX(&server->clients_mutex);
    
    for (int i = 0; i < server->config.max_connections; i++) {
        if (server->clients[i]) {
            socket_error_t result = write_message_to_client(server->clients[i], message);
            if (result == SOCKET_SUCCESS) {
                successful_sends++;
            } else {
                last_error = result;
                LOG_W("Failed to send broadcast to client %u: %s",
                     server->clients[i]->id, socket_error_string(result));
            }
        }
    }
    
    UNLOCK_MUTEX(&server->clients_mutex);
    
    // Update statistics
    if (successful_sends > 0) {
        pthread_mutex_lock(&server->stats_mutex);
        server->stats.messages_sent += successful_sends;
        server->stats.bytes_sent += successful_sends * 
                                   (message->length + SOCKET_MESSAGE_HEADER_SIZE);
        pthread_mutex_unlock(&server->stats_mutex);
    }
    
    return last_error;
}

// ============================================================================
// CLIENT AND STATISTICS FUNCTIONS
// ============================================================================

int socket_server_get_client_count(const socket_server_t *server)
{
    if (!server) return -1;
    return server->client_count;
}

int socket_server_get_clients(const socket_server_t *server,
                             socket_client_t **clients, int max_clients)
{
    if (!server || !clients || max_clients <= 0) return -1;
    
    LOCK_MUTEX((pthread_mutex_t*)&server->clients_mutex);
    
    int count = 0;
    for (int i = 0; i < server->config.max_connections && count < max_clients; i++) {
        if (server->clients[i]) {
            clients[count++] = server->clients[i];
        }
    }
    
    UNLOCK_MUTEX((pthread_mutex_t*)&server->clients_mutex);
    return count;
}

socket_error_t socket_server_disconnect_client(socket_server_t *server,
                                              socket_client_t *client)
{
    if (!server || !client) return SOCKET_ERROR_INVALID_PARAMETER;
    
    LOG_I("Disconnecting client %u", client->id);
    
    // Close client socket
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
    
    // Remove from server
    return remove_client(server, client);
}

socket_error_t socket_server_get_stats(const socket_server_t *server,
                                      socket_server_stats_t *stats)
{
    if (!server || !stats) return SOCKET_ERROR_INVALID_PARAMETER;
    
    pthread_mutex_lock((pthread_mutex_t*)&server->stats_mutex);
    *stats = server->stats;
    pthread_mutex_unlock((pthread_mutex_t*)&server->stats_mutex);
    
    return SOCKET_SUCCESS;
}

socket_error_t socket_server_reset_stats(socket_server_t *server)
{
    if (!server) return SOCKET_ERROR_INVALID_PARAMETER;
    
    pthread_mutex_lock(&server->stats_mutex);
    
    // Reset counters but preserve start time and current connections
    int current_connections = server->stats.current_connections;
    int64_t start_time = server->stats.start_time;
    
    memset(&server->stats, SOCKET_STATS_RESET_VALUE, sizeof(server->stats));
    server->stats.current_connections = current_connections;
    server->stats.start_time = start_time;
    
    pthread_mutex_unlock(&server->stats_mutex);
    
    return SOCKET_SUCCESS;
}