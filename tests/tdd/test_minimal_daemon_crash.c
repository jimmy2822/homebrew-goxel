#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define SOCKET_PATH "/tmp/minimal_daemon_test.sock"

void cleanup() {
    unlink(SOCKET_PATH);
}

int main() {
    cleanup();
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child - run daemon
        execl("../../goxel-daemon", "goxel-daemon", "--foreground", "--socket", SOCKET_PATH, NULL);
        exit(1);
    }
    
    // Parent - wait for daemon to start
    sleep(1);
    
    // Connect and send request
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Failed to connect\n");
        kill(pid, SIGTERM);
        cleanup();
        return 1;
    }
    
    // Send a simple request
    const char* req = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test\",16,16,16],\"id\":1}\n";
    send(sock, req, strlen(req), 0);
    
    // Receive response
    char buffer[1024];
    int n = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Response: %s\n", buffer);
    }
    
    // Close connection
    close(sock);
    printf("Connection closed\n");
    
    // Wait a moment
    sleep(1);
    
    // Check if daemon is still running
    if (kill(pid, 0) == 0) {
        printf("Daemon still running\n");
        
        // Try second connection
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Second connection failed - daemon may have crashed\n");
        } else {
            printf("Second connection succeeded\n");
            close(sock);
        }
    } else {
        printf("Daemon crashed after first connection\n");
    }
    
    // Cleanup
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    cleanup();
    
    return 0;
}