/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#include "compatibility_proxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

/**
 * Client request context for async processing
 */
typedef struct {
    compat_client_context_t *client;
    char *request_data;
    size_t request_length;
    compat_proxy_server_t *server;
} client_request_context_t;

// ============================================================================
// INTERNAL FUNCTION DECLARATIONS
// ============================================================================

static json_rpc_result_t create_server_socket(const char *socket_path, int *server_fd);
static json_rpc_result_t connect_to_daemon(
    const char *daemon_socket, 
    compat_client_context_t *client);
static void *client_handler_thread(void *arg);
static json_rpc_result_t process_client_request(
    compat_proxy_server_t *server,
    compat_client_context_t *client,
    const char *request_data,
    size_t request_length);
static json_rpc_result_t send_deprecation_warning_internal(
    compat_client_context_t *client,
    const char *method);

// ============================================================================
// SERVER INITIALIZATION AND CLEANUP
// ============================================================================

json_rpc_result_t compat_proxy_init(
    const compat_proxy_config_t *config,
    compat_proxy_server_t **server)
{
    if (!config || !server) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Validate configuration
    json_rpc_result_t result = compat_validate_config(config);
    if (result != JSON_RPC_SUCCESS) {
        return result;
    }
    
    // Allocate server context
    compat_proxy_server_t *srv = calloc(1, sizeof(compat_proxy_server_t));
    if (!srv) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy configuration
    srv->config = *config;
    
    // Initialize statistics
    memset(&srv->stats, 0, sizeof(srv->stats));
    
    // Initialize client management
    srv->max_clients = config->max_concurrent_clients;
    srv->clients = calloc(srv->max_clients, sizeof(compat_client_context_t));
    if (!srv->clients) {
        free(srv);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize mutexes
    if (pthread_mutex_init(&srv->stats_mutex, NULL) != 0 ||
        pthread_mutex_init(&srv->clients_mutex, NULL) != 0) {
        free(srv->clients);
        free(srv);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Set translation mappings (using static tables for now)
    srv->method_mappings = NULL; // Will be set dynamically per client
    srv->mapping_count = 0;
    
    // Initialize socket file descriptors
    srv->legacy_mcp_server_fd = -1;
    srv->legacy_daemon_server_fd = -1;
    
    *server = srv;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_proxy_start(compat_proxy_server_t *server)
{
    if (!server) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    json_rpc_result_t result;
    
    // Create legacy MCP server socket
    result = create_server_socket(
        server->config.legacy_mcp_socket,
        &server->legacy_mcp_server_fd);
    if (result != JSON_RPC_SUCCESS) {
        fprintf(stderr, "[Compatibility] Failed to create MCP server socket: %s\n",
                server->config.legacy_mcp_socket);
        return result;
    }
    
    // Create legacy daemon server socket
    result = create_server_socket(
        server->config.legacy_daemon_socket,
        &server->legacy_daemon_server_fd);
    if (result != JSON_RPC_SUCCESS) {
        fprintf(stderr, "[Compatibility] Failed to create daemon server socket: %s\n",
                server->config.legacy_daemon_socket);
        close(server->legacy_mcp_server_fd);
        return result;
    }
    
    server->running = true;
    
    printf("[Compatibility] Proxy server started:\n");
    printf("  Legacy MCP socket: %s\n", server->config.legacy_mcp_socket);
    printf("  Legacy daemon socket: %s\n", server->config.legacy_daemon_socket);
    printf("  Target daemon: %s\n", server->config.new_daemon_socket);
    printf("  Max clients: %u\n", server->config.max_concurrent_clients);
    
    // Main server loop
    fd_set read_fds, master_fds;
    int max_fd;
    
    FD_ZERO(&master_fds);
    FD_SET(server->legacy_mcp_server_fd, &master_fds);
    FD_SET(server->legacy_daemon_server_fd, &master_fds);
    
    max_fd = (server->legacy_mcp_server_fd > server->legacy_daemon_server_fd) ?
             server->legacy_mcp_server_fd : server->legacy_daemon_server_fd;
    
    while (server->running) {
        read_fds = master_fds;
        
        // Add active client sockets to read set
        pthread_mutex_lock(&server->clients_mutex);
        for (uint32_t i = 0; i < server->max_clients; i++) {
            if (server->clients[i].client_fd > 0) {
                FD_SET(server->clients[i].client_fd, &read_fds);
                if (server->clients[i].client_fd > max_fd) {
                    max_fd = server->clients[i].client_fd;
                }
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        struct timeval timeout = {1, 0}; // 1 second timeout
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }
        
        if (activity == 0) {
            continue; // Timeout, check for shutdown
        }
        
        // Check for new connections on MCP server socket
        if (FD_ISSET(server->legacy_mcp_server_fd, &read_fds)) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server->legacy_mcp_server_fd,
                                 (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                // Find available client slot
                pthread_mutex_lock(&server->clients_mutex);
                for (uint32_t i = 0; i < server->max_clients; i++) {
                    if (server->clients[i].client_fd == 0) {
                        server->clients[i].client_fd = client_fd;
                        server->clients[i].detected_protocol = COMPAT_PROTOCOL_LEGACY_MCP;
                        server->clients[i].is_legacy_client = true;
                        strcpy(server->clients[i].user_agent, "Legacy-MCP-Server/1.0");
                        snprintf(server->clients[i].client_id, 
                                sizeof(server->clients[i].client_id),
                                "mcp_client_%d_%ld", client_fd, time(NULL));
                        server->clients[i].first_request = time(NULL);
                        server->active_clients++;
                        break;
                    }
                }
                pthread_mutex_unlock(&server->clients_mutex);
                printf("[Compatibility] New MCP client connected: fd=%d\n", client_fd);
            }
        }
        
        // Check for new connections on daemon server socket
        if (FD_ISSET(server->legacy_daemon_server_fd, &read_fds)) {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server->legacy_daemon_server_fd,
                                 (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                // Find available client slot
                pthread_mutex_lock(&server->clients_mutex);
                for (uint32_t i = 0; i < server->max_clients; i++) {
                    if (server->clients[i].client_fd == 0) {
                        server->clients[i].client_fd = client_fd;
                        server->clients[i].detected_protocol = COMPAT_PROTOCOL_LEGACY_TYPESCRIPT;
                        server->clients[i].is_legacy_client = true;
                        strcpy(server->clients[i].user_agent, "TypeScript-Client/14.0-legacy");
                        snprintf(server->clients[i].client_id,
                                sizeof(server->clients[i].client_id), 
                                "ts_client_%d_%ld", client_fd, time(NULL));
                        server->clients[i].first_request = time(NULL);
                        server->active_clients++;
                        break;
                    }
                }
                pthread_mutex_unlock(&server->clients_mutex);
                printf("[Compatibility] New TypeScript client connected: fd=%d\n", client_fd);
            }
        }
        
        // Check for data from existing clients
        pthread_mutex_lock(&server->clients_mutex);
        for (uint32_t i = 0; i < server->max_clients; i++) {
            if (server->clients[i].client_fd > 0 && 
                FD_ISSET(server->clients[i].client_fd, &read_fds)) {
                
                char buffer[4096];
                ssize_t bytes_read = recv(server->clients[i].client_fd, buffer, 
                                        sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    // Client disconnected
                    printf("[Compatibility] Client %s disconnected\n", 
                           server->clients[i].client_id);
                    close(server->clients[i].client_fd);
                    if (server->clients[i].daemon_fd > 0) {
                        close(server->clients[i].daemon_fd);
                    }
                    memset(&server->clients[i], 0, sizeof(compat_client_context_t));
                    server->active_clients--;
                } else {
                    // Process client request
                    buffer[bytes_read] = '\0';
                    process_client_request(server, &server->clients[i], 
                                         buffer, bytes_read);
                }
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
    }
    
    return JSON_RPC_SUCCESS;
}

void compat_proxy_stop(compat_proxy_server_t *server)
{
    if (!server) return;
    
    server->running = false;
    
    // Close server sockets
    if (server->legacy_mcp_server_fd >= 0) {
        close(server->legacy_mcp_server_fd);
        unlink(server->config.legacy_mcp_socket);
    }
    
    if (server->legacy_daemon_server_fd >= 0) {
        close(server->legacy_daemon_server_fd);
        unlink(server->config.legacy_daemon_socket);
    }
    
    // Close all client connections
    pthread_mutex_lock(&server->clients_mutex);
    for (uint32_t i = 0; i < server->max_clients; i++) {
        if (server->clients[i].client_fd > 0) {
            close(server->clients[i].client_fd);
        }
        if (server->clients[i].daemon_fd > 0) {
            close(server->clients[i].daemon_fd);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    printf("[Compatibility] Proxy server stopped\n");
}

void compat_proxy_cleanup(compat_proxy_server_t *server)
{
    if (!server) return;
    
    compat_proxy_stop(server);
    
    // Cleanup resources
    if (server->clients) {
        free(server->clients);
    }
    
    pthread_mutex_destroy(&server->stats_mutex);
    pthread_mutex_destroy(&server->clients_mutex);
    
    free(server);
}

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

static json_rpc_result_t create_server_socket(const char *socket_path, int *server_fd)
{
    if (!socket_path || !server_fd) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Remove existing socket file
    unlink(socket_path);
    
    // Create socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sock);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Listen for connections
    if (listen(sock, 10) < 0) {
        close(sock);
        unlink(socket_path);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    *server_fd = sock;
    return JSON_RPC_SUCCESS;
}

static json_rpc_result_t connect_to_daemon(
    const char *daemon_socket,
    compat_client_context_t *client)
{
    if (!daemon_socket || !client) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (client->daemon_connected && client->daemon_fd > 0) {
        return JSON_RPC_SUCCESS; // Already connected
    }
    
    // Create socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Connect to daemon
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, daemon_socket, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    client->daemon_fd = sock;
    client->daemon_connected = true;
    
    return JSON_RPC_SUCCESS;
}

static json_rpc_result_t process_client_request(
    compat_proxy_server_t *server,
    compat_client_context_t *client,
    const char *request_data,
    size_t request_length)
{
    if (!server || !client || !request_data || request_length == 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    uint64_t start_time = get_time_microseconds();
    
    // Parse request JSON
    json_value *request_json = json_parse(request_data, request_length);
    if (!request_json) {
        return JSON_RPC_ERROR_JSON_PARSE;
    }
    
    // Update statistics
    pthread_mutex_lock(&server->stats_mutex);
    server->stats.total_requests++;
    if (client->is_legacy_client) {
        switch (client->detected_protocol) {
            case COMPAT_PROTOCOL_LEGACY_MCP:
                server->stats.legacy_mcp_requests++;
                break;
            case COMPAT_PROTOCOL_LEGACY_TYPESCRIPT:
                server->stats.legacy_typescript_requests++;
                break;
            case COMPAT_PROTOCOL_LEGACY_JSONRPC:
                server->stats.legacy_jsonrpc_requests++;
                break;
            default:
                break;
        }
    } else {
        server->stats.native_requests++;
    }
    pthread_mutex_unlock(&server->stats_mutex);
    
    // Translate request if needed
    json_value *translated_request = NULL;
    json_rpc_result_t result = compat_translate_request(
        request_json, client->detected_protocol, &translated_request, client);
    
    if (result != JSON_RPC_SUCCESS) {
        json_value_free(request_json);
        pthread_mutex_lock(&server->stats_mutex);
        server->stats.translation_errors++;
        pthread_mutex_unlock(&server->stats_mutex);
        return result;
    }
    
    // Connect to daemon if not already connected
    if (!client->daemon_connected) {
        result = connect_to_daemon(server->config.new_daemon_socket, client);
        if (result != JSON_RPC_SUCCESS) {
            json_value_free(request_json);
            json_value_free(translated_request);
            return result;
        }
    }
    
    // Send request to daemon
    char *request_str = malloc(json_measure(translated_request));
    if (!request_str) {
        json_value_free(request_json);
        json_value_free(translated_request);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    json_serialize(request_str, translated_request);
    size_t request_str_len = strlen(request_str);
    
    ssize_t bytes_sent = send(client->daemon_fd, request_str, request_str_len, 0);
    send(client->daemon_fd, "\n", 1, 0); // Newline delimiter
    
    free(request_str);
    json_value_free(translated_request);
    
    if (bytes_sent < 0) {
        json_value_free(request_json);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    // Receive response from daemon
    char response_buffer[8192];
    ssize_t bytes_received = recv(client->daemon_fd, response_buffer, 
                                sizeof(response_buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        json_value_free(request_json);
        return JSON_RPC_ERROR_INTERNAL;
    }
    
    response_buffer[bytes_received] = '\0';
    
    // Parse daemon response
    json_value *daemon_response = json_parse(response_buffer, bytes_received);
    if (!daemon_response) {
        json_value_free(request_json);
        return JSON_RPC_ERROR_JSON_PARSE;
    }
    
    // Translate response back to client format
    json_value *client_response = NULL;
    result = compat_translate_response(
        daemon_response, client->detected_protocol, &client_response, client);
    
    json_value_free(daemon_response);
    
    if (result != JSON_RPC_SUCCESS) {
        json_value_free(request_json);
        return result;
    }
    
    // Send response to client
    char *response_str = malloc(json_measure(client_response));
    if (!response_str) {
        json_value_free(request_json);
        json_value_free(client_response);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    json_serialize(response_str, client_response);
    send(client->client_fd, response_str, strlen(response_str), 0);
    send(client->client_fd, "\n", 1, 0);
    
    free(response_str);
    json_value_free(client_response);
    json_value_free(request_json);
    
    // Update timing statistics
    uint64_t end_time = get_time_microseconds();
    uint64_t duration = end_time - start_time;
    
    pthread_mutex_lock(&server->stats_mutex);
    server->stats.translation_successes++;
    server->stats.total_translation_time_us += duration;
    server->stats.avg_translation_time_us = 
        (double)server->stats.total_translation_time_us / 
        server->stats.translation_successes;
    pthread_mutex_unlock(&server->stats_mutex);
    
    // Send deprecation warning if needed
    if (client->is_legacy_client && server->config.enable_deprecation_warnings) {
        client->requests_translated++;
        if (client->requests_translated % server->config.warning_frequency == 0) {
            send_deprecation_warning_internal(client, "legacy_method");
        }
    }
    
    client->last_request = time(NULL);
    
    return JSON_RPC_SUCCESS;
}

static json_rpc_result_t send_deprecation_warning_internal(
    compat_client_context_t *client,
    const char *method)
{
    if (!client || !method) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Create deprecation warning response
    json_value *warning = json_object_new(0);
    json_object_push(warning, "type", json_string_new("deprecation_warning"));
    json_object_push(warning, "message", 
                    json_string_new("This client is using deprecated API. Please migrate to v14.0 unified daemon."));
    json_object_push(warning, "method", json_string_new(method));
    json_object_push(warning, "migration_guide", 
                    json_string_new("https://goxel.xyz/docs/v14/migration"));
    
    char *warning_str = malloc(json_measure(warning));
    if (!warning_str) {
        json_value_free(warning);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    json_serialize(warning_str, warning);
    
    // Send as separate message (non-blocking)
    send(client->client_fd, warning_str, strlen(warning_str), MSG_DONTWAIT);
    send(client->client_fd, "\n", 1, MSG_DONTWAIT);
    
    free(warning_str);
    json_value_free(warning);
    
    client->warnings_sent++;
    
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// TELEMETRY AND MONITORING IMPLEMENTATION
// ============================================================================

void compat_record_request(
    compat_proxy_server_t *server,
    compat_protocol_type_t protocol_type,
    const char *method,
    compat_client_context_t *client_context)
{
    if (!server || !client_context) return;
    
    pthread_mutex_lock(&server->stats_mutex);
    
    // Update first/last legacy request timestamps
    if (compat_is_legacy_protocol(protocol_type)) {
        time_t now = time(NULL);
        if (server->stats.first_legacy_request == 0) {
            server->stats.first_legacy_request = now;
        }
        server->stats.last_legacy_request = now;
    }
    
    pthread_mutex_unlock(&server->stats_mutex);
}

json_rpc_result_t compat_send_deprecation_warning(
    compat_client_context_t *client_context,
    const char *method)
{
    return send_deprecation_warning_internal(client_context, method);
}

void compat_get_migration_stats(
    const compat_proxy_server_t *server,
    compat_migration_stats_t *stats)
{
    if (!server || !stats) return;
    
    pthread_mutex_lock((pthread_mutex_t*)&server->stats_mutex);
    *stats = server->stats;
    pthread_mutex_unlock((pthread_mutex_t*)&server->stats_mutex);
}