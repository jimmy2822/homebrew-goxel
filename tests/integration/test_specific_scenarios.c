/* Goxel 3D voxels editor - Specific Scenario Tests
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

/*
 * This file contains targeted test scenarios that can be run individually
 * from the main test_e2e_workflow.c file using the -t option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <assert.h>

#define TEST_SOCKET "/tmp/goxel_test.sock"
#define TEST_PID_FILE "/tmp/goxel_test.pid"
#define MAX_BUFFER 4096

// ============================================================================
// TEST SCENARIO: Daemon Startup
// ============================================================================
int test_daemon_startup(void)
{
    printf("Testing daemon startup...\n");
    
    // Fork and start daemon
    pid_t pid = fork();
    if (pid == 0) {
        // Child - start daemon
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Parent - wait for daemon to start
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) {
            printf("Daemon started successfully (PID: %d)\n", pid);
            
            // Clean up
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            unlink(TEST_SOCKET);
            unlink(TEST_PID_FILE);
            return 0;
        }
        usleep(100000); // 100ms
    }
    
    printf("ERROR: Daemon failed to start\n");
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return 1;
}

// ============================================================================
// TEST SCENARIO: Single Client Connection
// ============================================================================
int test_single_connect(void)
{
    printf("Testing single client connection...\n");
    
    // Start daemon first
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Wait for daemon
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) break;
        usleep(100000);
    }
    
    // Connect to daemon
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("ERROR: Failed to create socket\n");
        kill(daemon_pid, SIGTERM);
        return 1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("ERROR: Failed to connect: %s\n", strerror(errno));
        close(sock);
        kill(daemon_pid, SIGTERM);
        return 1;
    }
    
    printf("Connected successfully\n");
    
    // Send a test request
    const char *request = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_version\",\"id\":1}\n";
    send(sock, request, strlen(request), 0);
    
    // Read response
    char buffer[MAX_BUFFER];
    ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Response: %s", buffer);
    }
    
    // Cleanup
    close(sock);
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(TEST_SOCKET);
    unlink(TEST_PID_FILE);
    
    return 0;
}

// ============================================================================
// TEST SCENARIO: Multiple Client Connections
// ============================================================================
int test_multi_connect(void)
{
    printf("Testing multiple client connections...\n");
    
    // Start daemon
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Wait for daemon
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) break;
        usleep(100000);
    }
    
    // Connect multiple clients
    int sockets[5];
    int connected = 0;
    
    for (int i = 0; i < 5; i++) {
        sockets[i] = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockets[i] < 0) continue;
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET, sizeof(addr.sun_path) - 1);
        
        if (connect(sockets[i], (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            connected++;
            printf("Client %d connected\n", i + 1);
        } else {
            close(sockets[i]);
            sockets[i] = -1;
        }
    }
    
    printf("Connected %d/5 clients\n", connected);
    
    // Send requests from all clients
    for (int i = 0; i < 5; i++) {
        if (sockets[i] >= 0) {
            char request[256];
            snprintf(request, sizeof(request),
                "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.echo\",\"params\":[\"Client %d\"],\"id\":%d}\n",
                i + 1, i + 1);
            send(sockets[i], request, strlen(request), 0);
        }
    }
    
    // Read responses
    for (int i = 0; i < 5; i++) {
        if (sockets[i] >= 0) {
            char buffer[MAX_BUFFER];
            ssize_t n = recv(sockets[i], buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Client %d response: %s", i + 1, buffer);
            }
            close(sockets[i]);
        }
    }
    
    // Cleanup
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(TEST_SOCKET);
    unlink(TEST_PID_FILE);
    
    return (connected >= 4) ? 0 : 1; // Pass if at least 4 clients connected
}

// ============================================================================
// TEST SCENARIO: Graceful Shutdown
// ============================================================================
int test_shutdown(void)
{
    printf("Testing graceful shutdown...\n");
    
    // Start daemon
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Wait for daemon
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) break;
        usleep(100000);
    }
    
    // Send SIGTERM
    printf("Sending SIGTERM to daemon (PID: %d)\n", daemon_pid);
    kill(daemon_pid, SIGTERM);
    
    // Wait for clean shutdown
    int status;
    pid_t result = waitpid(daemon_pid, &status, 0);
    
    if (result == daemon_pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("Daemon shut down cleanly\n");
        
        // Verify files are cleaned up
        if (access(TEST_SOCKET, F_OK) != 0 && access(TEST_PID_FILE, F_OK) != 0) {
            printf("Clean shutdown confirmed - files removed\n");
            return 0;
        }
    }
    
    printf("ERROR: Daemon did not shut down cleanly\n");
    unlink(TEST_SOCKET);
    unlink(TEST_PID_FILE);
    return 1;
}

// ============================================================================
// TEST SCENARIO: Method Tests
// ============================================================================
int test_method(const char *method_name, const char *method_call, const char *params)
{
    printf("Testing method: %s\n", method_name);
    
    // Start daemon
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Wait for daemon
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) break;
        usleep(100000);
    }
    
    // Connect
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("ERROR: Failed to connect\n");
        kill(daemon_pid, SIGTERM);
        return 1;
    }
    
    // Send request
    char request[1024];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}\n",
        method_call, params ? params : "[]");
    
    send(sock, request, strlen(request), 0);
    
    // Get response
    char buffer[MAX_BUFFER];
    ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Response: %s", buffer);
        
        // Check for error
        if (strstr(buffer, "\"error\"") != NULL && strstr(method_name, "error") == NULL) {
            printf("ERROR: Method returned error\n");
            close(sock);
            kill(daemon_pid, SIGTERM);
            return 1;
        }
    }
    
    // Cleanup
    close(sock);
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(TEST_SOCKET);
    unlink(TEST_PID_FILE);
    
    return 0;
}

// ============================================================================
// TEST SCENARIO: Stress Tests
// ============================================================================
int test_stress_clients(int num_clients)
{
    printf("Testing %d concurrent clients...\n", num_clients);
    
    // Start daemon
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", TEST_SOCKET,
            "--pid-file", TEST_PID_FILE,
            NULL
        };
        execv("../../goxel-headless", args);
        exit(1);
    }
    
    // Wait for daemon
    for (int i = 0; i < 50; i++) {
        if (access(TEST_SOCKET, F_OK) == 0) break;
        usleep(100000);
    }
    
    // Fork multiple client processes
    int successful_clients = 0;
    for (int i = 0; i < num_clients; i++) {
        pid_t client_pid = fork();
        if (client_pid == 0) {
            // Client process
            int sock = socket(AF_UNIX, SOCK_STREAM, 0);
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, TEST_SOCKET, sizeof(addr.sun_path) - 1);
            
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                // Send multiple requests
                for (int j = 0; j < 10; j++) {
                    char request[256];
                    snprintf(request, sizeof(request),
                        "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.ping\",\"id\":%d}\n",
                        j + 1);
                    send(sock, request, strlen(request), 0);
                    
                    char buffer[MAX_BUFFER];
                    recv(sock, buffer, sizeof(buffer) - 1, 0);
                }
                close(sock);
                exit(0); // Success
            }
            exit(1); // Failed
        }
    }
    
    // Wait for all clients
    for (int i = 0; i < num_clients; i++) {
        int status;
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            successful_clients++;
        }
    }
    
    printf("Successful clients: %d/%d\n", successful_clients, num_clients);
    
    // Cleanup
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(TEST_SOCKET);
    unlink(TEST_PID_FILE);
    
    // Pass if at least 80% of clients succeeded
    return (successful_clients >= num_clients * 0.8) ? 0 : 1;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================
int main(int argc, char *argv[])
{
    if (argc < 3 || strcmp(argv[1], "-t") != 0) {
        printf("Usage: %s -t <test_name>\n", argv[0]);
        printf("Available tests:\n");
        printf("  startup\n");
        printf("  connect\n");
        printf("  multi_connect\n");
        printf("  shutdown\n");
        printf("  method_echo\n");
        printf("  method_version\n");
        printf("  method_status\n");
        printf("  method_create_project\n");
        printf("  method_add_voxel\n");
        printf("  stress_10_clients\n");
        printf("  stress_50_clients\n");
        return 1;
    }
    
    const char *test_name = argv[2];
    
    // Run specific test
    if (strcmp(test_name, "startup") == 0) {
        return test_daemon_startup();
    } else if (strcmp(test_name, "connect") == 0) {
        return test_single_connect();
    } else if (strcmp(test_name, "multi_connect") == 0) {
        return test_multi_connect();
    } else if (strcmp(test_name, "shutdown") == 0) {
        return test_shutdown();
    } else if (strcmp(test_name, "method_echo") == 0) {
        return test_method("echo", "goxel.echo", "[\"Hello World\"]");
    } else if (strcmp(test_name, "method_version") == 0) {
        return test_method("version", "goxel.get_version", NULL);
    } else if (strcmp(test_name, "method_status") == 0) {
        return test_method("status", "goxel.get_status", NULL);
    } else if (strcmp(test_name, "method_create_project") == 0) {
        return test_method("create_project", "goxel.create_project", "[\"Test\",16,16,16]");
    } else if (strcmp(test_name, "method_add_voxel") == 0) {
        return test_method("add_voxel", "goxel.add_voxel", "[0,-16,0,255,0,0,255,0]");
    } else if (strcmp(test_name, "stress_10_clients") == 0) {
        return test_stress_clients(10);
    } else if (strcmp(test_name, "stress_50_clients") == 0) {
        return test_stress_clients(50);
    } else {
        printf("Unknown test: %s\n", test_name);
        return 1;
    }
}