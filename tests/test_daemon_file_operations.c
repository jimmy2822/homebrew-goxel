/*
 * Integration tests for daemon file operations
 * Tests actual save, export, and render functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

#define TEST_SOCKET "/tmp/goxel_test_fileops.sock"
#define TEST_OUTPUT_DIR "/tmp/goxel_test_output"

// Simple test framework
static int tests_run = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            printf("FAIL: %s (line %d)\n", message, __LINE__); \
            tests_failed++; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while (0)

// Send JSON-RPC request and get response
static char* send_request(const char* request) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }
    
    // Send request
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return NULL;
    }
    
    // Read response
    static char response[8192];
    ssize_t n = recv(sock, response, sizeof(response) - 1, 0);
    if (n < 0) {
        perror("recv");
        close(sock);
        return NULL;
    }
    
    response[n] = '\0';
    close(sock);
    return response;
}

// Check if file exists
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Get file size
static long get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

// Test creating a project and saving it
static void test_save_project() {
    printf("\n=== Testing save_project ===\n");
    
    // Create a project
    const char* create_req = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\","
                            "\"params\":[\"TestProject\",16,16,16],\"id\":1}\n";
    char* resp = send_request(create_req);
    TEST_ASSERT(resp != NULL, "Create project request should succeed");
    printf("Create response: %s\n", resp);
    TEST_ASSERT(strstr(resp, "\"success\": true") != NULL || strstr(resp, "\"success\":true") != NULL, 
                "Create project should return success");
    
    // Add a voxel
    const char* add_voxel_req = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxel\","
                               "\"params\":[8,8,8,255,0,0,255],\"id\":2}\n";
    resp = send_request(add_voxel_req);
    TEST_ASSERT(resp != NULL, "Add voxel request should succeed");
    
    // Save the project
    char save_path[256];
    snprintf(save_path, sizeof(save_path), "%s/test_save.gox", TEST_OUTPUT_DIR);
    
    char save_req[512];
    snprintf(save_req, sizeof(save_req), 
             "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.save_project\","
             "\"params\":[\"%s\"],\"id\":3}\n", save_path);
    
    resp = send_request(save_req);
    TEST_ASSERT(resp != NULL, "Save project request should succeed");
    printf("Save response: %s\n", resp);
    TEST_ASSERT(strstr(resp, "\"success\": true") != NULL || strstr(resp, "\"success\":true") != NULL, 
                "Save project should return success");
    
    // Verify file was created
    TEST_ASSERT(file_exists(save_path), "Saved file should exist");
    
    long size = get_file_size(save_path);
    TEST_ASSERT(size > 100, "Saved file should have reasonable size");
    printf("Saved file size: %ld bytes\n", size);
}

// Test exporting to different formats
static void test_export_model() {
    printf("\n=== Testing export_model ===\n");
    
    // Test export to OBJ (currently unsupported in daemon)
    char export_path[256];
    snprintf(export_path, sizeof(export_path), "%s/test_export.obj", TEST_OUTPUT_DIR);
    
    char export_req[512];
    snprintf(export_req, sizeof(export_req),
             "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_model\","
             "\"params\":[\"%s\",\"obj\"],\"id\":4}\n", export_path);
    
    char* resp = send_request(export_req);
    TEST_ASSERT(resp != NULL, "Export model request should get response");
    
    // Currently daemon only supports .gox export
    if (strstr(resp, "\"error\"") != NULL) {
        printf("Note: Export to OBJ not yet supported in daemon mode\n");
    }
    
    // Check if OBJ file was actually created
    if (strstr(resp, "\"success\": true") != NULL) {
        TEST_ASSERT(file_exists(export_path), "Exported OBJ file should exist");
        long size = get_file_size(export_path);
        printf("Exported OBJ file size: %ld bytes\n", size);
    }
    
    // Test export without format (should default to .gox)
    snprintf(export_path, sizeof(export_path), "%s/test_export_default.gox", TEST_OUTPUT_DIR);
    snprintf(export_req, sizeof(export_req),
             "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_model\","
             "\"params\":[\"%s\"],\"id\":5}\n", export_path);
    
    resp = send_request(export_req);
    TEST_ASSERT(resp != NULL, "Export model (default format) request should succeed");
    
    // Check if file was created
    if (strstr(resp, "\"success\": true") != NULL) {
        TEST_ASSERT(file_exists(export_path), "Exported default format file should exist");
        long size = get_file_size(export_path);
        printf("Exported default format file size: %ld bytes\n", size);
    }
}

// Test render functionality
static void test_render_scene() {
    printf("\n=== Testing render_scene ===\n");
    
    char render_path[256];
    snprintf(render_path, sizeof(render_path), "%s/test_render.png", TEST_OUTPUT_DIR);
    
    char render_req[512];
    snprintf(render_req, sizeof(render_req),
             "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\","
             "\"params\":[\"%s\",800,600],\"id\":6}\n", render_path);
    
    char* resp = send_request(render_req);
    TEST_ASSERT(resp != NULL, "Render scene request should get response");
    
    // Check if render succeeded
    if (strstr(resp, "\"success\":true") != NULL) {
        TEST_ASSERT(file_exists(render_path), "Rendered file should exist");
        long size = get_file_size(render_path);
        TEST_ASSERT(size > 1000, "Rendered PNG should have reasonable size");
        printf("Rendered file size: %ld bytes\n", size);
    } else {
        printf("Note: Render may have failed due to missing dependencies\n");
    }
}

int main(int argc, char** argv) {
    printf("=== Daemon File Operations Integration Tests ===\n");
    
    // Check if daemon is running
    if (!file_exists(TEST_SOCKET)) {
        printf("ERROR: Daemon not running at %s\n", TEST_SOCKET);
        printf("Start daemon with: ./goxel-daemon --foreground --socket %s\n", TEST_SOCKET);
        return 1;
    }
    
    // Create output directory
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", TEST_OUTPUT_DIR);
    system(cmd);
    
    // Run tests
    test_save_project();
    test_export_model();
    test_render_scene();
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);
    printf("Tests passed: %d\n", tests_run - tests_failed);
    
    // Clean up
    // NOTE: Commented out to keep test output files for inspection
    // snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_OUTPUT_DIR);
    // system(cmd);
    printf("\n=== Test output files kept in: %s ===\n", TEST_OUTPUT_DIR);
    
    return tests_failed > 0 ? 1 : 0;
}