/* Simple standalone test for render manager
 * This test can be compiled and run independently
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

// Mock the logging functions for standalone testing
void dolog(int level, const char *fmt, ...) {
    // Do nothing for standalone tests
    (void)level; (void)fmt;
}

// Define logging macros that do nothing
#define LOG_I(msg, ...) do { printf("[INFO] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_W(msg, ...) do { printf("[WARN] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_E(msg, ...) do { printf("[ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_D(msg, ...) do { printf("[DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)

// Now include the render manager
#include "../src/daemon/render_manager.h"

// Test configuration
#define TEST_RENDER_DIR "/tmp/goxel_test_renders_simple"

/**
 * Simple test to verify basic render manager functionality
 */
int main(void) {
    printf("Running simple render manager test...\n");
    
    // Clean up any existing test directory
    char cleanup_cmd[256];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", TEST_RENDER_DIR);
    system(cleanup_cmd);
    
    // Test 1: Create render manager
    printf("Test 1: Creating render manager...\n");
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 100*1024*1024, 3600);
    if (!rm) {
        printf("FAIL: Failed to create render manager\n");
        return 1;
    }
    printf("PASS: Render manager created successfully\n");
    
    // Test 2: Check if directory was created
    printf("Test 2: Checking directory creation...\n");
    struct stat st;
    if (stat(TEST_RENDER_DIR, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("FAIL: Test directory was not created\n");
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Directory created successfully\n");
    
    // Test 3: Generate render path
    printf("Test 3: Generating render path...\n");
    char render_path[512];
    render_manager_error_t result = render_manager_create_path(rm, "test_session", 
                                                             "png", render_path, sizeof(render_path));
    if (result != RENDER_MGR_SUCCESS) {
        printf("FAIL: Failed to create render path (error: %s)\n", 
               render_manager_error_string(result));
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Render path generated: %s\n", render_path);
    
    // Test 4: Verify path format
    printf("Test 4: Verifying path format...\n");
    if (strstr(render_path, TEST_RENDER_DIR) == NULL ||
        strstr(render_path, "test_session") == NULL ||
        strstr(render_path, ".png") == NULL ||
        strstr(render_path, "render_") == NULL) {
        printf("FAIL: Path format is incorrect\n");
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Path format is correct\n");
    
    // Test 5: Get statistics
    printf("Test 5: Getting statistics...\n");
    render_manager_stats_t stats;
    result = render_manager_get_stats(rm, &stats);
    if (result != RENDER_MGR_SUCCESS) {
        printf("FAIL: Failed to get statistics\n");
        render_manager_destroy(rm, true);
        return 1;
    }
    if (stats.active_count != 0 || stats.total_renders != 0) {
        printf("FAIL: Statistics are incorrect (active=%d, total=%llu)\n", 
               stats.active_count, (unsigned long long)stats.total_renders);
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Statistics are correct\n");
    
    // Test 6: Test utility functions
    printf("Test 6: Testing utility functions...\n");
    char token[16];
    result = render_manager_generate_token(token, sizeof(token));
    if (result != RENDER_MGR_SUCCESS || strlen(token) != 8) {
        printf("FAIL: Token generation failed\n");
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Token generated: %s\n", token);
    
    // Test path validation
    if (!render_manager_validate_path("/tmp/goxel_renders/test.png", "/tmp/goxel_renders") ||
        render_manager_validate_path("/etc/passwd", "/tmp/goxel_renders") ||
        render_manager_validate_path("/tmp/goxel_renders/../passwd", "/tmp/goxel_renders")) {
        printf("FAIL: Path validation failed\n");
        render_manager_destroy(rm, true);
        return 1;
    }
    printf("PASS: Path validation works correctly\n");
    
    // Test 7: Cleanup
    printf("Test 7: Cleanup...\n");
    render_manager_destroy(rm, true);
    printf("PASS: Cleanup successful\n");
    
    // Cleanup test directory
    system(cleanup_cmd);
    
    printf("\n========================================\n");
    printf("✅ All simple render manager tests passed!\n");
    printf("========================================\n");
    
    return 0;
}