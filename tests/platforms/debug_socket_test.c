/**
 * Debug test to diagnose socket creation issue on macOS ARM64.
 * This test creates a minimal socket server to verify basic functionality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/goxel_debug_test.sock"

static int create_unix_socket(const char *path) {
    printf("[DEBUG] Creating Unix domain socket...\n");
    
    // Create socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("[ERROR] socket() failed: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }
    printf("[DEBUG] Socket created, fd=%d\n", fd);
    
    // Remove existing socket file
    if (unlink(path) == -1 && errno != ENOENT) {
        printf("[WARN] unlink(%s) failed: %s\n", path, strerror(errno));
    }
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    printf("[DEBUG] Binding to path: %s\n", path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("[ERROR] bind() failed: %s (errno=%d)\n", strerror(errno), errno);
        close(fd);
        return -1;
    }
    printf("[DEBUG] Socket bound successfully\n");
    
    // Check if socket file exists
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("[DEBUG] Socket file created: %s (mode=%o)\n", path, st.st_mode);
    } else {
        printf("[ERROR] Socket file not found after bind: %s\n", strerror(errno));
    }
    
    // Set permissions
    if (chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1) {
        printf("[WARN] chmod() failed: %s\n", strerror(errno));
    }
    
    // Start listening
    printf("[DEBUG] Starting to listen...\n");
    if (listen(fd, 5) == -1) {
        printf("[ERROR] listen() failed: %s (errno=%d)\n", strerror(errno), errno);
        close(fd);
        unlink(path);
        return -1;
    }
    printf("[DEBUG] Listening on socket\n");
    
    return fd;
}

static void *accept_thread(void *arg) {
    int server_fd = *(int*)arg;
    printf("[THREAD] Accept thread started, fd=%d\n", server_fd);
    
    while (1) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        printf("[THREAD] Waiting for connection...\n");
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            printf("[THREAD] accept() failed: %s\n", strerror(errno));
            break;
        }
        
        printf("[THREAD] Client connected, fd=%d\n", client_fd);
        
        // Simple echo
        char buffer[256];
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[THREAD] Received: %s\n", buffer);
            write(client_fd, "OK\n", 3);
        }
        
        close(client_fd);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    printf("=== Goxel Socket Debug Test ===\n");
    printf("Platform: macOS ARM64\n");
    printf("Socket path: %s\n", SOCKET_PATH);
    printf("\n");
    
    // Check directory permissions
    struct stat st;
    if (stat("/tmp", &st) == 0) {
        printf("[INFO] /tmp permissions: %o\n", st.st_mode);
    }
    
    // Create socket
    int server_fd = create_unix_socket(SOCKET_PATH);
    if (server_fd == -1) {
        printf("\n[FAIL] Socket creation failed\n");
        return 1;
    }
    
    // Verify socket file exists
    if (access(SOCKET_PATH, F_OK) == 0) {
        printf("\n[SUCCESS] Socket file exists: %s\n", SOCKET_PATH);
    } else {
        printf("\n[FAIL] Socket file not found after creation\n");
        close(server_fd);
        return 1;
    }
    
    // Test with thread
    pthread_t thread;
    if (pthread_create(&thread, NULL, accept_thread, &server_fd) != 0) {
        printf("[ERROR] Failed to create thread\n");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }
    
    printf("\n[INFO] Socket server running. Press Ctrl+C to exit.\n");
    printf("[INFO] Test with: echo 'test' | nc -U %s\n", SOCKET_PATH);
    
    // Wait for signal
    pause();
    
    // Cleanup
    close(server_fd);
    unlink(SOCKET_PATH);
    
    return 0;
}