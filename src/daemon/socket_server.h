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

#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ERROR CODES AND RETURN TYPES  
// ============================================================================

/**
 * Socket server error codes.
 */
typedef enum {
    SOCKET_SUCCESS = 0,                  /**< Operation completed successfully */
    SOCKET_ERROR_INVALID_CONTEXT,        /**< Invalid or NULL context */
    SOCKET_ERROR_INVALID_PARAMETER,      /**< Invalid parameter value */
    SOCKET_ERROR_SOCKET_CREATE_FAILED,   /**< Failed to create socket */
    SOCKET_ERROR_BIND_FAILED,           /**< Failed to bind socket */
    SOCKET_ERROR_LISTEN_FAILED,         /**< Failed to listen on socket */
    SOCKET_ERROR_ACCEPT_FAILED,         /**< Failed to accept connection */
    SOCKET_ERROR_WRITE_FAILED,          /**< Failed to write to socket */
    SOCKET_ERROR_READ_FAILED,           /**< Failed to read from socket */
    SOCKET_ERROR_OUT_OF_MEMORY,         /**< Memory allocation failed */
    SOCKET_ERROR_THREAD_CREATE_FAILED,  /**< Failed to create thread */
    SOCKET_ERROR_MUTEX_FAILED,          /**< Mutex operation failed */
    SOCKET_ERROR_SHUTDOWN_FAILED,       /**< Server shutdown failed */
    SOCKET_ERROR_ALREADY_RUNNING,       /**< Server already running */
    SOCKET_ERROR_NOT_RUNNING,           /**< Server not running */
    SOCKET_ERROR_PERMISSION_DENIED,     /**< Permission denied for socket path */
    SOCKET_ERROR_PATH_TOO_LONG,         /**< Socket path too long */
    SOCKET_ERROR_CONNECTION_LOST,       /**< Connection lost */
    SOCKET_ERROR_TIMEOUT,               /**< Operation timed out */
    SOCKET_ERROR_UNKNOWN = -1           /**< Unknown error */
} socket_error_t;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

typedef struct socket_server socket_server_t;
typedef struct socket_client socket_client_t;
typedef struct socket_message socket_message_t;

/**
 * Callback function type for handling client messages.
 * 
 * @param server The socket server instance
 * @param client The client that sent the message
 * @param message The received message
 * @param user_data User-provided data passed to the callback
 * @return Response message (caller takes ownership), or NULL for no response
 */
typedef socket_message_t *(*socket_message_handler_t)(
    socket_server_t *server,
    socket_client_t *client, 
    const socket_message_t *message,
    void *user_data
);

/**
 * Callback function type for client connection events.
 * 
 * @param server The socket server instance
 * @param client The client that connected/disconnected
 * @param connected true for connection, false for disconnection
 * @param user_data User-provided data passed to the callback
 */
typedef void (*socket_client_handler_t)(
    socket_server_t *server,
    socket_client_t *client,
    bool connected,
    void *user_data
);

// ============================================================================
// MESSAGE STRUCTURE
// ============================================================================

/**
 * Socket message structure for client-server communication.
 */
struct socket_message {
    uint32_t id;                    /**< Message ID for request/response matching */
    uint32_t type;                  /**< Message type identifier */
    uint32_t length;                /**< Length of data payload */
    char *data;                     /**< Message payload data */
    int64_t timestamp;              /**< Message timestamp (microseconds) */
};

// ============================================================================
// CLIENT INFORMATION
// ============================================================================

/**
 * Protocol mode for client connections.
 */
typedef enum {
    PROTOCOL_BINARY,                /**< Binary protocol with 16-byte header */
    PROTOCOL_JSON_RPC              /**< JSON-RPC protocol */
} protocol_mode_t;

/**
 * Client connection information.
 */
struct socket_client {
    int fd;                         /**< Client socket file descriptor */
    uint32_t id;                    /**< Unique client identifier */
    pid_t pid;                      /**< Client process ID */
    uid_t uid;                      /**< Client user ID */
    gid_t gid;                      /**< Client group ID */
    int64_t connect_time;           /**< Connection timestamp */
    char *buffer;                   /**< Receive buffer */
    size_t buffer_size;             /**< Current buffer size */
    size_t buffer_capacity;         /**< Buffer capacity */
    bool authenticated;             /**< Client authentication status */
    void *user_data;                /**< User-defined client data */
    protocol_mode_t protocol;       /**< Protocol mode for this client */
    union {
        struct {
            char *buffer;
            size_t buffer_size;
            size_t buffer_capacity;
        } binary;
        struct {
            pthread_t monitor_thread;
            bool monitor_running;
        } json;
    } handler_data;                 /**< Protocol-specific handler data */
};

// ============================================================================
// SERVER CONFIGURATION
// ============================================================================

/**
 * Socket server configuration structure.
 */
typedef struct {
    const char *socket_path;            /**< Unix socket file path */
    int max_connections;                /**< Maximum concurrent connections */
    int listen_backlog;                 /**< Listen backlog size */
    int receive_timeout_ms;             /**< Receive timeout in milliseconds */
    int send_timeout_ms;                /**< Send timeout in milliseconds */
    size_t max_message_size;            /**< Maximum message size in bytes */
    size_t buffer_initial_size;         /**< Initial buffer size per client */
    bool auto_reconnect;                /**< Auto-reconnect on connection loss */
    bool thread_per_client;             /**< Use thread per client (vs thread pool) */
    int thread_pool_size;               /**< Thread pool size (if not per-client) */
    socket_message_handler_t msg_handler;  /**< Message handler callback */
    socket_client_handler_t client_handler; /**< Client event handler callback */
    void *user_data;                    /**< User data passed to callbacks */
} socket_server_config_t;

// ============================================================================
// SERVER LIFECYCLE MANAGEMENT
// ============================================================================

/**
 * Creates a new Unix socket server with specified configuration.
 * 
 * @param config Server configuration (must remain valid during server lifetime)
 * @return New server instance, or NULL on failure
 */
socket_server_t *socket_server_create(const socket_server_config_t *config);

/**
 * Initializes and starts the socket server.
 * This function creates the socket, binds it, and starts listening.
 * 
 * @param server Server instance
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_start(socket_server_t *server);

/**
 * Stops the socket server and closes all connections.
 * This is a graceful shutdown that waits for current operations to complete.
 * 
 * @param server Server instance
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_stop(socket_server_t *server);

/**
 * Destroys the socket server and frees all resources.
 * Server must be stopped before calling this function.
 * 
 * @param server Server instance (may be NULL)
 */
void socket_server_destroy(socket_server_t *server);

/**
 * Checks if the server is currently running.
 * 
 * @param server Server instance
 * @return true if running, false otherwise
 */
bool socket_server_is_running(const socket_server_t *server);

// ============================================================================
// MESSAGE HANDLING
// ============================================================================

/**
 * Creates a new socket message.
 * 
 * @param id Message ID
 * @param type Message type
 * @param data Message data (will be copied)
 * @param length Data length
 * @return New message instance, or NULL on failure
 */
socket_message_t *socket_message_create(uint32_t id, uint32_t type, 
                                       const char *data, uint32_t length);

/**
 * Creates a new socket message from a JSON string.
 * 
 * @param id Message ID
 * @param type Message type
 * @param json_data JSON string (will be copied)
 * @return New message instance, or NULL on failure
 */
socket_message_t *socket_message_create_json(uint32_t id, uint32_t type,
                                            const char *json_data);

/**
 * Destroys a socket message and frees its resources.
 * 
 * @param message Message instance (may be NULL)
 */
void socket_message_destroy(socket_message_t *message);

/**
 * Sends a message to a specific client.
 * 
 * @param server Server instance
 * @param client Target client
 * @param message Message to send
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_send_message(socket_server_t *server,
                                         socket_client_t *client,
                                         const socket_message_t *message);

/**
 * Broadcasts a message to all connected clients.
 * 
 * @param server Server instance
 * @param message Message to broadcast
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_broadcast_message(socket_server_t *server,
                                              const socket_message_t *message);

// ============================================================================
// CLIENT MANAGEMENT
// ============================================================================

/**
 * Gets the number of currently connected clients.
 * 
 * @param server Server instance
 * @return Number of connected clients, or -1 on error
 */
int socket_server_get_client_count(const socket_server_t *server);

/**
 * Gets a list of all connected clients.
 * 
 * @param server Server instance
 * @param clients Array to store client pointers (allocated by caller)
 * @param max_clients Maximum number of clients to return
 * @return Number of clients returned, or -1 on error
 */
int socket_server_get_clients(const socket_server_t *server,
                             socket_client_t **clients, int max_clients);

/**
 * Disconnects a specific client.
 * 
 * @param server Server instance
 * @param client Client to disconnect
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_disconnect_client(socket_server_t *server,
                                              socket_client_t *client);

/**
 * Sets user data for a client connection.
 * 
 * @param client Client instance
 * @param user_data User data pointer
 */
void socket_client_set_user_data(socket_client_t *client, void *user_data);

/**
 * Gets user data from a client connection.
 * 
 * @param client Client instance
 * @return User data pointer, or NULL if not set
 */
void *socket_client_get_user_data(const socket_client_t *client);

// ============================================================================
// STATISTICS AND MONITORING
// ============================================================================

/**
 * Server statistics structure.
 */
typedef struct {
    uint64_t total_connections;         /**< Total connections since start */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t bytes_received;            /**< Total bytes received */
    uint64_t bytes_sent;                /**< Total bytes sent */
    uint64_t connection_errors;         /**< Total connection errors */
    uint64_t message_errors;            /**< Total message processing errors */
    int64_t start_time;                 /**< Server start timestamp */
    int current_connections;            /**< Current active connections */
} socket_server_stats_t;

/**
 * Gets server statistics.
 * 
 * @param server Server instance
 * @param stats Structure to fill with statistics
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_get_stats(const socket_server_t *server,
                                      socket_server_stats_t *stats);

/**
 * Resets server statistics counters.
 * 
 * @param server Server instance
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_reset_stats(socket_server_t *server);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Gets a human-readable error message for an error code.
 * 
 * @param error Error code
 * @return Pointer to error message string (do not free)
 */
const char *socket_error_string(socket_error_t error);

/**
 * Gets the last error message from the server.
 * 
 * @param server Server instance
 * @return Pointer to error message string (do not free), or NULL if no error
 */
const char *socket_server_get_last_error(const socket_server_t *server);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Gets the default socket server configuration.
 * 
 * @return Default configuration structure
 */
socket_server_config_t socket_server_default_config(void);

/**
 * Validates a socket server configuration.
 * 
 * @param config Configuration to validate
 * @return SOCKET_SUCCESS if valid, error code otherwise
 */
socket_error_t socket_server_validate_config(const socket_server_config_t *config);

/**
 * Gets the server's socket path.
 * 
 * @param server Server instance
 * @return Socket path string, or NULL on error
 */
const char *socket_server_get_path(const socket_server_t *server);

/**
 * Checks if a socket path is available for binding.
 * 
 * @param path Socket path to check
 * @return true if available, false otherwise
 */
bool socket_server_path_available(const char *path);

/**
 * Removes an existing socket file (for cleanup).
 * 
 * @param path Socket path to remove
 * @return SOCKET_SUCCESS on success, error code on failure
 */
socket_error_t socket_server_cleanup_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif // SOCKET_SERVER_H