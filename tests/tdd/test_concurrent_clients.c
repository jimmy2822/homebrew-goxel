#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/concurrent_test.sock"
#define NUM_CLIENTS 3

typedef struct {
    int client_id;
    int success;
    char response[1024];
} client_result_t;

void* client_thread(void* arg) {
    client_result_t* result = (client_result_t*)arg;
    
    // Connect to daemon
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result->success = 0;
        return NULL;
    }
    
    // Send request
    char request[256];
    snprintf(request, sizeof(request), 
             "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test%d\",16,16,16],\"id\":%d}\n",
             result->client_id, result->client_id);
    
    if (send(sock, request, strlen(request), 0) < 0) {
        result->success = 0;
        close(sock);
        return NULL;
    }
    
    // Receive response
    int n = recv(sock, result->response, sizeof(result->response)-1, 0);
    if (n > 0) {
        result->response[n] = '\0';
        result->success = 1;
        printf("Client %d response: %s\n", result->client_id, result->response);
    } else {
        result->success = 0;
    }
    
    close(sock);
    return NULL;
}

int main() {
    // Cleanup
    unlink(SOCKET_PATH);
    
    // Start daemon
    pid_t pid = fork();
    if (pid == 0) {
        execl("../../goxel-daemon", "goxel-daemon", "--foreground", "--socket", SOCKET_PATH, NULL);
        exit(1);
    }
    
    // Wait for daemon to start
    sleep(1);
    
    // Create concurrent clients
    pthread_t threads[NUM_CLIENTS];
    client_result_t results[NUM_CLIENTS];
    
    printf("Starting %d concurrent clients...\n", NUM_CLIENTS);
    
    for (int i = 0; i < NUM_CLIENTS; i++) {
        results[i].client_id = i + 1;
        results[i].success = 0;
        pthread_create(&threads[i], NULL, client_thread, &results[i]);
    }
    
    // Wait for all clients
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Check results
    int successes = 0;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (results[i].success) {
            successes++;
            if (strstr(results[i].response, "\"success\":true")) {
                printf("Client %d: SUCCESS\n", results[i].client_id);
            } else if (strstr(results[i].response, "in progress")) {
                printf("Client %d: BLOCKED by project lock\n", results[i].client_id);
            }
        } else {
            printf("Client %d: FAILED\n", results[i].client_id);
        }
    }
    
    printf("\nConcurrent test result: %d/%d clients succeeded\n", successes, NUM_CLIENTS);
    
    // Note: With project lock, only one should create successfully
    // Others should get "project operation in progress" errors
    
    // Cleanup
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    unlink(SOCKET_PATH);
    
    return 0;
}