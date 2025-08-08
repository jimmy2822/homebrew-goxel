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

#include "json_socket_handler.h"
#include "socket_server.h"
#include "json_rpc.h"
#include "../log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>

// ============================================================================
// JSON SOCKET HANDLER IMPLEMENTATION
// ============================================================================

/**
 * Client monitoring thread data
 */
typedef struct {
    socket_server_t *server;
    socket_client_t *client;
    socket_message_handler_t msg_handler;
    void *user_data;
    bool running;
} client_monitor_data_t;

// Global storage for message handler and user data
// This is set when the server is configured
static socket_message_handler_t g_msg_handler = NULL;
static void *g_user_data = NULL;

void json_socket_set_handler(socket_message_handler_t handler, void *user_data)
{
    g_msg_handler = handler;
    g_user_data = user_data;
}

/**
 * Read raw JSON from socket.
 * Supports both newline-delimited and complete JSON object detection.
 * Uses buffered reading to avoid busy loops and high CPU usage.
 */
static int read_json_line(socket_client_t *client, char *buffer, size_t max_size)
{
    int fd = client->fd;
    char *read_buffer = client->handler_data.json.read_buffer;
    size_t *read_buffer_pos = &client->handler_data.json.read_buffer_pos;
    size_t *read_buffer_len = &client->handler_data.json.read_buffer_len;
    
    size_t pos = 0;
    int brace_count = 0;
    int bracket_count = 0;
    bool in_string = false;
    bool escape_next = false;
    bool found_start = false;
    
    while (pos < max_size - 1) {
        // Fill read buffer if empty
        if (*read_buffer_pos >= *read_buffer_len) {
            ssize_t n = recv(fd, read_buffer, sizeof(client->handler_data.json.read_buffer), MSG_DONTWAIT);
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available
                    if (pos == 0) {
                        // No data read yet, return "no data"
                        return -1;
                    }
                    // If we have a complete JSON object, return it
                    if (found_start && brace_count == 0 && bracket_count == 0) {
                        buffer[pos] = '\0';
                        return (int)pos;
                    }
                    // Partial message, need more data
                    return -1;
                }
                if (errno == EINTR) {
                    *read_buffer_pos = 0;
                    *read_buffer_len = 0;
                    continue;
                }
                // Actual error
                return -2;
            }
            
            if (n == 0) {
                // Connection closed
                if (pos == 0) {
                    // No data read yet - check if truly closed
                    struct pollfd pfd;
                    pfd.fd = fd;
                    pfd.events = POLLIN | POLLHUP | POLLERR;
                    pfd.revents = 0;
                    
                    int poll_result = poll(&pfd, 1, 0);
                    if (poll_result > 0 && (pfd.revents & (POLLHUP | POLLERR))) {
                        return -3;
                    }
                    
                    // Check socket error state
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0) {
                        return -3;
                    }
                    
                    return -1;
                }
                // If we already read some data and now get 0, likely closed
                return -3;
            }
            
            *read_buffer_pos = 0;
            *read_buffer_len = n;
        }
        
        // Process buffered data
        while (*read_buffer_pos < *read_buffer_len && pos < max_size - 1) {
            char c = read_buffer[(*read_buffer_pos)++];
            
            // Skip leading whitespace
            if (!found_start && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
                continue;
            }
            
            buffer[pos++] = c;
            
            // Track JSON structure
            if (!in_string && !escape_next) {
                if (c == '{') {
                    found_start = true;
                    brace_count++;
                } else if (c == '}') {
                    brace_count--;
                    if (brace_count == 0 && bracket_count == 0 && found_start) {
                        buffer[pos] = '\0';
                        return (int)pos;
                    }
                } else if (c == '[') {
                    found_start = true;
                    bracket_count++;
                } else if (c == ']') {
                    bracket_count--;
                    if (brace_count == 0 && bracket_count == 0 && found_start) {
                        buffer[pos] = '\0';
                        return (int)pos;
                    }
                } else if (c == '"') {
                    in_string = true;
                } else if (c == '\n' && !found_start) {
                    // Empty line before JSON
                    pos = 0;
                    continue;
                } else if (c == '\n' && found_start && brace_count == 0 && bracket_count == 0) {
                    // Newline after complete JSON
                    pos--; // Remove the newline
                    buffer[pos] = '\0';
                    return (int)pos;
                }
            } else if (in_string && !escape_next) {
                if (c == '\\') {
                    escape_next = true;
                } else if (c == '"') {
                    in_string = false;
                }
            } else {
                escape_next = false;
            }
        }
    }
    
    buffer[pos] = '\0';
    
    // Check if we have a complete JSON object
    if (found_start && brace_count == 0 && bracket_count == 0) {
        return (int)pos;
    }
    
    // Buffer full or incomplete JSON
    return -4;
}

/**
 * Client monitoring thread function.
 * Reads JSON messages from the client and processes them.
 */
static void *json_client_monitor_thread(void *arg)
{
    client_monitor_data_t *data = (client_monitor_data_t*)arg;
    char buffer[65536]; // 64KB buffer for JSON messages
    
    LOG_I("JSON client monitor started for client %u", data->client->id);
    
    // Set socket to non-blocking for better control
    int flags = fcntl(data->client->fd, F_GETFL, 0);
    fcntl(data->client->fd, F_SETFL, flags | O_NONBLOCK);
    
    while (data->running && data->client->handler_data.json.monitor_running) {
        // Use poll to check for data with timeout
        struct pollfd pfd;
        pfd.fd = data->client->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        
        int poll_result = poll(&pfd, 1, 100); // 100ms timeout
        
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            LOG_E("Poll error: %s", strerror(errno));
            break;
        }
        
        if (poll_result == 0) continue; // Timeout
        
        // Log poll events
        if (pfd.revents & POLLHUP) {
            LOG_I("Client %u: POLLHUP detected", data->client->id);
            break;
        }
        if (pfd.revents & POLLERR) {
            LOG_E("Client %u: POLLERR detected", data->client->id);
            break;
        }
        if (pfd.revents & POLLNVAL) {
            LOG_E("Client %u: POLLNVAL detected", data->client->id);
            break;
        }
        
        // Read JSON message
        int len = read_json_line(data->client, buffer, sizeof(buffer));
        
        if (len == -3) {
            // Connection closed
            LOG_I("Client %u disconnected (connection closed)", data->client->id);
            break;
        }
        
        if (len == -2) {
            // Error
            LOG_E("Read error from client %u: %s", 
                  data->client->id, strerror(errno));
            break;
        }
        
        if (len == -1) {
            // No data available - this is normal for non-blocking sockets
            continue;
        }
        
        if (len == -4) {
            // Buffer full or incomplete JSON
            LOG_E("JSON message from client %u too large or malformed (max: %zu bytes)", 
                  data->client->id, sizeof(buffer));
            
            // Send error response directly (no binary wrapper)
            const char *error_response = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error: JSON message too large\"},\"id\":null}\n";
            send(data->client->fd, error_response, strlen(error_response), MSG_NOSIGNAL);
            
            // Clear the socket buffer to recover
            char discard[1024];
            while (recv(data->client->fd, discard, sizeof(discard), MSG_DONTWAIT) > 0) {
                // Discard data
            }
            continue;
        }
        
        if (len <= 0) continue; // No data or would block
        
        // Skip empty lines
        if (len == 0 || (len == 1 && buffer[0] == '\r')) continue;
        
        LOG_D("Received JSON from client %u: %s", data->client->id, buffer);
        
        // Create a socket message with the JSON data
        socket_message_t *msg = socket_message_create(
            data->client->id,  // Use client ID as message ID
            0,                 // Type 0 for JSON
            buffer,
            (uint32_t)len
        );
        
        if (!msg) {
            LOG_E("Failed to create socket message");
            continue;
        }
        
        // Process the message
        if (data->msg_handler) {
            LOG_I("Calling message handler for client %u", data->client->id);
            socket_message_t *response = data->msg_handler(
                data->server,
                data->client,
                msg,
                data->user_data
            );
            
            LOG_I("Message handler returned: %p", response);
            if (response) {
                LOG_I("Response message: id=%u, type=%u, length=%u, data=%p", 
                      response->id, response->type, response->length, response->data);
                
                // Validate response structure
                if (response->length > 0 && response->data == NULL) {
                    LOG_E("Invalid response: length=%u but data=NULL", response->length);
                    socket_message_destroy(response);
                    response = NULL;
                }
            }
            fflush(stdout);
            fflush(stderr);
            if (response) {
                LOG_I("Response details: data=%p, length=%u", response->data, response->length);
                if (response->data) {
                    // Send response back as JSON with newline
                    // Note: response->data should already contain the JSON string
                    LOG_I("Sending response to client %u: %.*s", data->client->id, 
                          (int)response->length, response->data);
                    
                    // Debug: Log fd and response details
                    LOG_I("Client fd=%d, response length=%u", data->client->fd, response->length);
                          
                    ssize_t sent = send(data->client->fd, response->data, response->length, MSG_NOSIGNAL);
                    if (sent < 0) {
                        if (errno == EPIPE || errno == ECONNRESET) {
                            LOG_I("Client %u disconnected during send", data->client->id);
                            socket_message_destroy(response);
                            socket_message_destroy(msg);
                            break;
                        }
                        LOG_E("Failed to send response: %s", strerror(errno));
                    } else {
                        LOG_I("Successfully sent %zd bytes to client %u", sent, data->client->id);
                    }
                    
                    // Send newline delimiter
                    ssize_t nl_sent = send(data->client->fd, "\n", 1, MSG_NOSIGNAL);
                    LOG_I("Sent newline: %zd bytes", nl_sent);
                    
                    socket_message_destroy(response);
                } else {
                    LOG_W("Response has NULL data for client %u", data->client->id);
                }
            } else {
                LOG_W("No response generated for client %u message", data->client->id);
            }
        } else {
            LOG_W("No message handler set for JSON socket");
        }
        
        socket_message_destroy(msg);
        
        // Log that we're continuing to wait for next message
        LOG_I("Ready for next message from client %u", data->client->id);
    }
    
    LOG_I("JSON client monitor stopped for client %u (loop exited)", data->client->id);
    LOG_I("Loop exit reason: data->running=%d, monitor_running=%d", 
          data->running, data->client->handler_data.json.monitor_running);
    
    // Mark monitor as not running
    data->client->handler_data.json.monitor_running = false;
    
    // DO NOT automatically disconnect the client!
    // The monitor thread should only handle disconnection if there was an actual error
    // detected in the loop (POLLHUP, POLLERR, connection closed, etc.)
    
    // The monitoring thread exiting does not mean the connection is broken.
    // Let the socket server handle connection cleanup when appropriate.
    LOG_I("Monitor thread for client %u exited - connection remains active", data->client->id);
    
    // Clean up monitor data
    free(data);
    return NULL;
}

/**
 * Socket client connected handler for JSON mode.
 * Starts a monitoring thread for each client.
 */
void json_socket_client_handler(socket_server_t *server,
                               socket_client_t *client,
                               bool connected,
                               void *user_data)
{
    if (connected) {
        LOG_I("Starting JSON monitor for client %u", client->id);
        
        // Store the handler info globally on first call
        // This is a bit hacky but avoids modifying the socket_server API
        if (!g_msg_handler && user_data) {
            // user_data should be the concurrent_daemon_t pointer
            // We'll get the handler through the server's message callback
            g_user_data = user_data;
        }
        
        // Get the message handler from global
        socket_message_handler_t msg_handler = g_msg_handler;
        
        // Create monitor data
        client_monitor_data_t *data = calloc(1, sizeof(client_monitor_data_t));
        if (!data) {
            LOG_E("Failed to allocate monitor data");
            return;
        }
        
        data->server = server;
        data->client = client;
        data->msg_handler = msg_handler;
        data->user_data = user_data;
        data->running = true;
        
        // Start monitor thread
        pthread_t thread;
        if (pthread_create(&thread, NULL, json_client_monitor_thread, data) != 0) {
            LOG_E("Failed to create monitor thread");
            free(data);
            return;
        }
        
        // Store thread handle in client's handler data
        client->handler_data.json.monitor_thread = thread;
        client->handler_data.json.monitor_running = true;
        
        // Don't detach - we'll join in handle_json_client
    } else {
        LOG_I("Client %u disconnected", client->id);
        
        // Mark monitor as not running
        client->handler_data.json.monitor_running = false;
        
        // Thread will exit on its own when it detects disconnection
    }
}

/**
 * Create a JSON-RPC response message.
 */
socket_message_t *json_rpc_create_socket_response(json_rpc_response_t *response,
                                                 uint32_t request_id)
{
    if (!response) return NULL;
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    
    if (result != JSON_RPC_SUCCESS || !json_str) {
        return NULL;
    }
    
    socket_message_t *msg = socket_message_create_json(request_id, 0, json_str);
    free(json_str);
    
    return msg;
}