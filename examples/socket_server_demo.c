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

/**
 * Socket Server Demonstration Program
 * 
 * This demonstrates basic usage of the Unix socket server infrastructure
 * for Goxel v14.0 daemon architecture. It shows:
 * - Server setup and configuration
 * - Client connection handling
 * - Basic message processing
 * - Graceful shutdown
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

// Include socket server
#include "../src/daemon/socket_server.h"

// ============================================================================
// DEMONSTRATION CONFIGURATION
// ============================================================================

#define DEMO_SOCKET_PATH "/tmp/goxel_demo_daemon.sock"
#define DEMO_RUNTIME_SECONDS 30

// Global server handle for signal handler
static socket_server_t *g_server = NULL;
static volatile int g_shutdown_requested = 0;

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

/**
 * Demo message handler that processes different message types.
 */
static socket_message_t *demo_message_handler(socket_server_t *server,
                                             socket_client_t *client,
                                             const socket_message_t *message,
                                             void *user_data)
{
    (void)server;
    (void)user_data;
    
    printf("📨 Received message from client %p: ID=%u, Type=%u, Length=%u\n",
           (void*)client, message->id, message->type, message->length);
    
    if (message->data && message->length > 0) {
        printf("   Data: %.*s\n", (int)message->length, message->data);
    }
    
    // Handle different message types
    switch (message->type) {
        case 1: { // Echo message
            printf("   → Echoing message back\n");
            return socket_message_create(message->id, message->type,
                                        message->data, message->length);
        }
        
        case 2: { // Greeting message
            printf("   → Sending greeting response\n");
            const char *greeting = "Hello from Goxel daemon!";
            return socket_message_create(message->id, message->type,
                                        greeting, strlen(greeting));
        }
        
        case 3: { // Status request
            printf("   → Sending status response\n");
            socket_server_stats_t stats;
            socket_server_get_stats(server, &stats);
            
            char status_msg[256];
            snprintf(status_msg, sizeof(status_msg),
                    "Status: %d clients, %llu total connections, %llu messages processed",
                    stats.current_connections,
                    (unsigned long long)stats.total_connections,
                    (unsigned long long)stats.messages_received);
            
            return socket_message_create(message->id, message->type,
                                        status_msg, strlen(status_msg));
        }
        
        default:
            printf("   → Unknown message type, no response\n");
            return NULL;
    }
}

/**
 * Demo client event handler that logs connection events.
 */
static void demo_client_event_handler(socket_server_t *server,
                                     socket_client_t *client,
                                     bool connected,
                                     void *user_data)
{
    (void)user_data;
    
    if (connected) {
        int client_count = socket_server_get_client_count(server);
        printf("🔗 Client connected: %p (total clients: %d)\n", 
               (void*)client, client_count);
    } else {
        int client_count = socket_server_get_client_count(server);
        printf("🔌 Client disconnected: %p (remaining clients: %d)\n", 
               (void*)client, client_count);
    }
}

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

static void signal_handler(int signal)
{
    printf("\n📢 Received signal %d, initiating graceful shutdown...\n", signal);
    g_shutdown_requested = 1;
    
    if (g_server) {
        socket_server_stop(g_server);
    }
}

static void setup_signal_handlers(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
}

// ============================================================================
// STATISTICS REPORTING
// ============================================================================

static void print_server_statistics(socket_server_t *server)
{
    socket_server_stats_t stats;
    socket_error_t result = socket_server_get_stats(server, &stats);
    
    if (result != SOCKET_SUCCESS) {
        printf("❌ Failed to get server statistics\n");
        return;
    }
    
    printf("\n📊 Server Statistics:\n");
    printf("   Current connections: %d\n", stats.current_connections);
    printf("   Total connections: %llu\n", (unsigned long long)stats.total_connections);
    printf("   Messages received: %llu\n", (unsigned long long)stats.messages_received);
    printf("   Messages sent: %llu\n", (unsigned long long)stats.messages_sent);
    printf("   Bytes received: %llu\n", (unsigned long long)stats.bytes_received);
    printf("   Bytes sent: %llu\n", (unsigned long long)stats.bytes_sent);
    printf("   Connection errors: %llu\n", (unsigned long long)stats.connection_errors);
    printf("   Message errors: %llu\n", (unsigned long long)stats.message_errors);
    printf("   Uptime: %lld seconds\n", 
           (long long)((stats.start_time > 0) ? 
                      (time(NULL) * 1000000LL - stats.start_time) / 1000000LL : 0));
}

// ============================================================================
// CLIENT SIMULATION THREAD
// ============================================================================

/**
 * Thread function that creates test clients and sends messages.
 */
static void *client_simulation_thread(void *arg)
{
    (void)arg;
    
    printf("🤖 Starting client simulation thread...\n");
    
    // Wait for server to be ready
    sleep(2);
    
    // Create and connect test clients
    for (int i = 0; i < 3 && !g_shutdown_requested; i++) {
        printf("🤖 Creating test client %d...\n", i + 1);
        
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_fd < 0) {
            printf("❌ Failed to create client socket\n");
            continue;
        }
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, DEMO_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        
        if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("❌ Failed to connect client %d\n", i + 1);
            close(client_fd);
            continue;
        }
        
        printf("✅ Client %d connected successfully\n", i + 1);
        
        // Send a few test messages
        const char *test_messages[] = {
            "Hello from test client!",
            "This is a test message",
            "Final message from client"
        };
        
        for (int j = 0; j < 3 && !g_shutdown_requested; j++) {
            const char *msg = test_messages[j % 3];
            ssize_t sent = send(client_fd, msg, strlen(msg), 0);
            if (sent > 0) {
                printf("🤖 Client %d sent: %s\n", i + 1, msg);
            }
            sleep(1);
        }
        
        close(client_fd);
        printf("🤖 Client %d disconnected\n", i + 1);
        
        sleep(2);
    }
    
    printf("🤖 Client simulation thread finished\n");
    return NULL;
}

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    printf("🚀 Goxel v14.0 Socket Server Infrastructure Demo\n");
    printf("================================================\n\n");
    
    // Setup signal handling
    setup_signal_handlers();
    
    // Clean up any existing socket file
    unlink(DEMO_SOCKET_PATH);
    
    // Create server configuration
    socket_server_config_t config = socket_server_default_config();
    config.socket_path = DEMO_SOCKET_PATH;
    config.max_connections = 10;
    config.msg_handler = demo_message_handler;
    config.client_handler = demo_client_event_handler;
    config.user_data = NULL;
    
    printf("⚙️  Server Configuration:\n");
    printf("   Socket path: %s\n", config.socket_path);
    printf("   Max connections: %d\n", config.max_connections);
    printf("   Message handler: %s\n", config.msg_handler ? "enabled" : "disabled");
    printf("   Client handler: %s\n", config.client_handler ? "enabled" : "disabled");
    printf("\n");
    
    // Create server
    printf("🔧 Creating server...\n");
    g_server = socket_server_create(&config);
    if (!g_server) {
        printf("❌ Failed to create server\n");
        return 1;
    }
    printf("✅ Server created successfully\n");
    
    // Start server
    printf("🚀 Starting server...\n");
    socket_error_t result = socket_server_start(g_server);
    if (result != SOCKET_SUCCESS) {
        printf("❌ Failed to start server: %s\n", socket_error_string(result));
        socket_server_destroy(g_server);
        return 1;
    }
    printf("✅ Server started successfully on %s\n", DEMO_SOCKET_PATH);
    
    // Start client simulation thread
    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, client_simulation_thread, NULL) != 0) {
        printf("⚠️  Failed to create client simulation thread\n");
    } else {
        printf("🤖 Client simulation thread started\n");
    }
    
    printf("\n📡 Server is running... (Press Ctrl+C to stop)\n");
    printf("💡 You can also connect manually with: socat - UNIX-CONNECT:%s\n\n", DEMO_SOCKET_PATH);
    
    // Main server loop
    int stats_counter = 0;
    while (!g_shutdown_requested && socket_server_is_running(g_server)) {
        sleep(1);
        
        // Print statistics every 10 seconds
        if (++stats_counter >= 10) {
            print_server_statistics(g_server);
            stats_counter = 0;
        }
    }
    
    // Wait for client simulation thread to finish
    pthread_join(client_thread, NULL);
    
    // Stop server if not already stopped
    if (socket_server_is_running(g_server)) {
        printf("🛑 Stopping server...\n");
        result = socket_server_stop(g_server);
        if (result != SOCKET_SUCCESS) {
            printf("⚠️  Error stopping server: %s\n", socket_error_string(result));
        } else {
            printf("✅ Server stopped successfully\n");
        }
    }
    
    // Print final statistics
    print_server_statistics(g_server);
    
    // Destroy server
    printf("🧹 Cleaning up server resources...\n");
    socket_server_destroy(g_server);
    g_server = NULL;
    
    // Clean up socket file
    unlink(DEMO_SOCKET_PATH);
    
    printf("\n🎉 Demo completed successfully!\n");
    printf("💡 This demonstrates the basic Unix socket server infrastructure\n");
    printf("   that will be used for the Goxel v14.0 daemon architecture.\n");
    
    return 0;
}