/**
 * Isolated test for socket_server.c functionality on macOS ARM64.
 * This test creates a minimal socket server without the full daemon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdbool.h>

// Minimal socket server structure for testing
typedef struct {
    int server_fd;
    char *socket_path;
    bool running;
    pthread_t accept_thread;
    pthread_mutex_t mutex;
} test_socket_server_t;

// Socket statistics
static struct {
    int connections_accepted;
    int messages_received;
} stats = {0, 0};

// Accept thread function
static void *accept_thread_func(void *arg) {
    test_socket_server_t *server = (test_socket_server_t *)arg;
    
    printf("[THREAD] Accept thread started, server_fd=%d\n", server->server_fd);
    
    while (server->running) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept connection
        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            if (server->running) {
                printf("[THREAD] accept() error: %s\n", strerror(errno));
            }
            break;
        }
        
        printf("[THREAD] Client connected, fd=%d\n", client_fd);
        stats.connections_accepted++;
        
        // Simple echo handler
        char buffer[256];
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[THREAD] Received: %s", buffer);
            stats.messages_received++;
            
            // Echo back
            const char *response = "OK: Message received\n";
            write(client_fd, response, strlen(response));
        }
        
        close(client_fd);
    }
    
    printf("[THREAD] Accept thread exiting\n");
    return NULL;
}

// Start socket server
static int start_socket_server(test_socket_server_t *server, const char *socket_path) {
    printf("\n=== Starting Socket Server ===\n");
    
    // Initialize mutex
    pthread_mutex_init(&server->mutex, NULL);
    
    // Save socket path
    server->socket_path = strdup(socket_path);
    
    // Remove existing socket
    unlink(socket_path);
    
    // Create socket
    printf("1. Creating Unix domain socket...\n");
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        printf("   FAILED: %s\n", strerror(errno));
        return -1;
    }
    printf("   SUCCESS: fd=%d\n", server->server_fd);
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    printf("2. Binding to %s...\n", socket_path);
    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("   FAILED: %s\n", strerror(errno));
        close(server->server_fd);
        return -1;
    }
    printf("   SUCCESS\n");
    
    // Verify socket file exists
    struct stat st;
    if (stat(socket_path, &st) == 0) {
        printf("3. Socket file created: %s\n", socket_path);
        printf("   - Type: %s\n", S_ISSOCK(st.st_mode) ? "Socket" : "Other");
        printf("   - Permissions: %o\n", st.st_mode & 0777);
    } else {
        printf("3. WARNING: Socket file not visible in filesystem\n");
    }
    
    // Set permissions
    chmod(socket_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    
    // Listen
    printf("4. Starting to listen...\n");
    if (listen(server->server_fd, 5) == -1) {
        printf("   FAILED: %s\n", strerror(errno));
        close(server->server_fd);
        unlink(socket_path);
        return -1;
    }
    printf("   SUCCESS\n");
    
    // Start accept thread
    server->running = true;
    printf("5. Creating accept thread...\n");
    if (pthread_create(&server->accept_thread, NULL, accept_thread_func, server) != 0) {
        printf("   FAILED: %s\n", strerror(errno));
        server->running = false;
        close(server->server_fd);
        unlink(socket_path);
        return -1;
    }
    printf("   SUCCESS\n");
    
    printf("\n=== Socket Server Running ===\n");
    printf("Socket path: %s\n", socket_path);
    printf("Server fd: %d\n", server->server_fd);
    printf("PID: %d\n", getpid());
    
    return 0;
}

// Stop socket server
static void stop_socket_server(test_socket_server_t *server) {
    printf("\n=== Stopping Socket Server ===\n");
    
    server->running = false;
    
    // Close server socket to wake accept thread
    if (server->server_fd >= 0) {
        close(server->server_fd);
    }
    
    // Wait for accept thread
    pthread_join(server->accept_thread, NULL);
    
    // Cleanup
    if (server->socket_path) {
        unlink(server->socket_path);
        free(server->socket_path);
    }
    
    pthread_mutex_destroy(&server->mutex);
    
    printf("Socket server stopped\n");
}

// Test client connection
static int test_client_connection(const char *socket_path) {
    printf("\n=== Testing Client Connection ===\n");
    
    // Create client socket
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        printf("Failed to create client socket: %s\n", strerror(errno));
        return -1;
    }
    
    // Connect to server
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    printf("Connecting to %s...\n", socket_path);
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Failed to connect: %s\n", strerror(errno));
        close(client_fd);
        return -1;
    }
    printf("Connected successfully\n");
    
    // Send test message
    const char *message = "Hello from test client\n";
    printf("Sending: %s", message);
    write(client_fd, message, strlen(message));
    
    // Read response
    char buffer[256];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Received: %s", buffer);
    }
    
    close(client_fd);
    printf("Client test completed\n");
    
    return 0;
}

// Signal handler
static volatile sig_atomic_t keep_running = 1;
static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/tmp/goxel_socket_test.sock";
    
    if (argc > 1) {
        socket_path = argv[1];
    }
    
    printf("=== Goxel Socket Server Isolated Test ===\n");
    printf("Platform: macOS ARM64\n");
    printf("Process: %s (PID %d)\n", argv[0], getpid());
    printf("\n");
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create and start server
    test_socket_server_t server = {0};
    if (start_socket_server(&server, socket_path) != 0) {
        printf("\nFAILED: Could not start socket server\n");
        return 1;
    }
    
    printf("\nTest the server with:\n");
    printf("  echo 'test message' | nc -U %s\n", socket_path);
    printf("  ./test_socket_server_isolated %s client\n", socket_path);
    printf("\nPress Ctrl+C to stop\n\n");
    
    // If "client" argument provided, run client test
    if (argc > 2 && strcmp(argv[2], "client") == 0) {
        sleep(1); // Give server time to start
        test_client_connection(socket_path);
    } else {
        // Wait for shutdown
        while (keep_running) {
            sleep(1);
            
            // Print stats periodically
            static int counter = 0;
            if (++counter % 10 == 0) {
                pthread_mutex_lock(&server.mutex);
                printf("[STATS] Connections: %d, Messages: %d\n", 
                       stats.connections_accepted, stats.messages_received);
                pthread_mutex_unlock(&server.mutex);
            }
        }
    }
    
    // Stop server
    stop_socket_server(&server);
    
    // Final stats
    printf("\n=== Final Statistics ===\n");
    printf("Connections accepted: %d\n", stats.connections_accepted);
    printf("Messages received: %d\n", stats.messages_received);
    
    printf("\n=== TEST COMPLETED ===\n");
    
    return 0;
}