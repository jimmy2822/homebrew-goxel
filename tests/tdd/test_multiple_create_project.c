/**
 * TDD test for multiple create_project calls
 * This ensures the daemon doesn't crash when creating multiple projects
 */

#include "tdd_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/wait.h>

static int connect_to_daemon(const char *socket_path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

static int send_request(int fd, const char *request)
{
    return send(fd, request, strlen(request), 0) == (ssize_t)strlen(request);
}

static int receive_response(int fd, char *buffer, size_t max_size)
{
    size_t pos = 0;
    int brace_count = 0;
    
    while (pos < max_size - 1) {
        ssize_t n = recv(fd, &buffer[pos], 1, 0);
        if (n <= 0) break;
        
        if (buffer[pos] == '{') brace_count++;
        else if (buffer[pos] == '}') {
            brace_count--;
            if (brace_count == 0) {
                pos++;
                buffer[pos] = '\0';
                return pos;
            }
        }
        pos++;
    }
    
    buffer[pos] = '\0';
    return pos;
}

int test_single_create_project()
{
    const char *socket_path = "/tmp/goxel_tdd_test.sock";
    
    // Start daemon in background
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        // Child process - run daemon
        execl("./goxel-daemon", "goxel-daemon", "--foreground", 
              "--socket", socket_path, (char*)NULL);
        exit(1); // If exec fails
    }
    
    // Parent process - run test
    sleep(1); // Give daemon time to start
    
    int fd = connect_to_daemon(socket_path);
    TEST_ASSERT(fd >= 0, "Failed to connect to daemon");
    
    // Send create_project request
    const char *request = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\","
                         "\"params\":[\"Test1\",16,16,16],\"id\":1}\n";
    TEST_ASSERT(send_request(fd, request), "Failed to send request");
    
    // Receive response
    char response[4096];
    int len = receive_response(fd, response, sizeof(response));
    TEST_ASSERT(len > 0, "Failed to receive response");
    TEST_ASSERT(strstr(response, "\"result\"") != NULL, "Response should contain result");
    TEST_ASSERT(strstr(response, "Test1") != NULL, "Response should contain project name");
    
    close(fd);
    
    // Clean up daemon
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(socket_path);
    
    return 1;
}

int test_multiple_create_projects()
{
    const char *socket_path = "/tmp/goxel_tdd_test2.sock";
    
    // Start daemon in background
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        // Child process - run daemon
        execl("./goxel-daemon", "goxel-daemon", "--foreground", 
              "--socket", socket_path, (char*)NULL);
        exit(1); // If exec fails
    }
    
    // Parent process - run test
    sleep(1); // Give daemon time to start
    
    int fd = connect_to_daemon(socket_path);
    TEST_ASSERT(fd >= 0, "Failed to connect to daemon");
    
    // Send multiple create_project requests
    for (int i = 1; i <= 5; i++) {
        char request[256];
        snprintf(request, sizeof(request),
                "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\","
                "\"params\":[\"Test%d\",16,16,16],\"id\":%d}\n", i, i);
        
        TEST_ASSERT(send_request(fd, request), "Failed to send request");
        
        // Receive response
        char response[4096];
        int len = receive_response(fd, response, sizeof(response));
        TEST_ASSERT(len > 0, "Failed to receive response");
        TEST_ASSERT(strstr(response, "\"result\"") != NULL, "Response should contain result");
        
        char expected_name[32];
        snprintf(expected_name, sizeof(expected_name), "Test%d", i);
        TEST_ASSERT(strstr(response, expected_name) != NULL, "Response should contain project name");
    }
    
    close(fd);
    
    // Clean up daemon
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(socket_path);
    
    return 1;
}

int test_create_project_with_other_operations()
{
    const char *socket_path = "/tmp/goxel_tdd_test3.sock";
    
    // Start daemon in background
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        // Child process - run daemon
        execl("./goxel-daemon", "goxel-daemon", "--foreground", 
              "--socket", socket_path, (char*)NULL);
        exit(1); // If exec fails
    }
    
    // Parent process - run test
    sleep(1); // Give daemon time to start
    
    int fd = connect_to_daemon(socket_path);
    TEST_ASSERT(fd >= 0, "Failed to connect to daemon");
    
    // Create first project
    const char *create1 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\","
                         "\"params\":[\"Project1\",32,32,32],\"id\":1}\n";
    TEST_ASSERT(send_request(fd, create1), "Failed to send create request");
    
    char response[4096];
    TEST_ASSERT(receive_response(fd, response, sizeof(response)) > 0, "Failed to receive response");
    
    // List layers
    const char *list_layers = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.list_layers\","
                             "\"params\":[],\"id\":2}\n";
    TEST_ASSERT(send_request(fd, list_layers), "Failed to send list_layers request");
    TEST_ASSERT(receive_response(fd, response, sizeof(response)) > 0, "Failed to receive response");
    
    // Create second project
    const char *create2 = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\","
                         "\"params\":[\"Project2\",64,64,64],\"id\":3}\n";
    TEST_ASSERT(send_request(fd, create2), "Failed to send create request");
    TEST_ASSERT(receive_response(fd, response, sizeof(response)) > 0, "Failed to receive response");
    TEST_ASSERT(strstr(response, "Project2") != NULL, "Response should contain new project name");
    
    close(fd);
    
    // Clean up daemon
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    unlink(socket_path);
    
    return 1;
}

int main()
{
    // Ignore SIGPIPE to handle closed connections gracefully
    signal(SIGPIPE, SIG_IGN);
    
    TEST_SUITE_BEGIN();
    
    RUN_TEST(test_single_create_project);
    RUN_TEST(test_multiple_create_projects);
    RUN_TEST(test_create_project_with_other_operations);
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}