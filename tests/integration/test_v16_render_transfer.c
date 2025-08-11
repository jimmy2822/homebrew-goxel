/* Goxel v0.16 Render Transfer Architecture - Integration Test Suite
 * 
 * Comprehensive tests for file-path based render transfer
 * Tests all aspects: functionality, performance, security, cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include "../../src/daemon/json_rpc.h"
#include "../../src/daemon/render_manager.h"
#include "../tdd/tdd_framework.h"

#define SOCKET_PATH "/tmp/goxel_test.sock"
#define TEST_RENDER_DIR "/tmp/test_renders"

// Test context
typedef struct {
    int sock;
    render_manager_t *rm;
    int test_count;
    int pass_count;
    int fail_count;
} test_context_t;

// Helper: Send JSON-RPC request
static char* send_request(int sock, const char* method, const char* params) {
    static char response[8192];
    char request[1024];
    
    snprintf(request, sizeof(request),
             "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}\n",
             method, params ? params : "[]");
    
    send(sock, request, strlen(request), 0);
    
    int n = recv(sock, response, sizeof(response) - 1, 0);
    if (n > 0) {
        response[n] = '\0';
        return response;
    }
    return NULL;
}

// Test 1: Basic file-path render
static void test_basic_render(test_context_t *ctx) {
    TEST_START("Basic file-path render");
    
    // Create project
    char* resp = send_request(ctx->sock, "goxel.create_project", 
                              "[\"TestProject\", 32, 32, 32]");
    ASSERT_NOT_NULL(resp);
    
    // Add some voxels
    for (int i = 0; i < 5; i++) {
        send_request(ctx->sock, "goxel.add_voxel",
                    "[16, 16, 16, 255, 0, 0, 255]");
    }
    
    // Render with file-path mode
    resp = send_request(ctx->sock, "goxel.render_scene",
                       "{\"width\":200,\"height\":200,\"options\":{\"return_mode\":\"file_path\"}}");
    ASSERT_NOT_NULL(resp);
    ASSERT_TRUE(strstr(resp, "\"success\":true") != NULL);
    ASSERT_TRUE(strstr(resp, "\"file\":{") != NULL);
    ASSERT_TRUE(strstr(resp, TEST_RENDER_DIR) != NULL);
    
    // Extract path and verify file exists
    char* path_start = strstr(resp, "\"path\":\"");
    if (path_start) {
        path_start += 8;
        char* path_end = strchr(path_start, '"');
        if (path_end) {
            char path[256];
            strncpy(path, path_start, path_end - path_start);
            path[path_end - path_start] = '\0';
            
            struct stat st;
            ASSERT_TRUE(stat(path, &st) == 0);
            ASSERT_TRUE(st.st_size > 0);
        }
    }
    
    TEST_END();
}

// Test 2: Backward compatibility
static void test_backward_compatibility(test_context_t *ctx) {
    TEST_START("Backward compatibility");
    
    // Test legacy array format
    char* resp = send_request(ctx->sock, "goxel.render_scene",
                              "[\"/tmp/test_legacy.png\", 100, 100]");
    ASSERT_NOT_NULL(resp);
    ASSERT_TRUE(strstr(resp, "\"success\":true") != NULL);
    
    // Verify file was created
    struct stat st;
    ASSERT_TRUE(stat("/tmp/test_legacy.png", &st) == 0);
    
    // Clean up
    unlink("/tmp/test_legacy.png");
    
    TEST_END();
}

// Test 3: Environment variable configuration
static void test_env_configuration(test_context_t *ctx) {
    TEST_START("Environment variable configuration");
    
    // Set environment variables
    setenv("GOXEL_RENDER_DIR", "/tmp/custom_renders", 1);
    setenv("GOXEL_RENDER_TTL", "120", 1);
    setenv("GOXEL_RENDER_MAX_SIZE", "52428800", 1);
    
    // Create new render manager with env vars
    render_manager_t *rm = render_manager_create(NULL, 0, 0);
    ASSERT_NOT_NULL(rm);
    ASSERT_STREQ(rm->output_dir, "/tmp/custom_renders");
    ASSERT_EQ(rm->ttl_seconds, 120);
    ASSERT_EQ(rm->max_cache_size, 52428800);
    
    render_manager_destroy(rm, true);
    
    // Restore defaults
    unsetenv("GOXEL_RENDER_DIR");
    unsetenv("GOXEL_RENDER_TTL");
    unsetenv("GOXEL_RENDER_MAX_SIZE");
    
    TEST_END();
}

// Test 4: TTL and cleanup
static void test_ttl_cleanup(test_context_t *ctx) {
    TEST_START("TTL and cleanup");
    
    // Create render manager with short TTL
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 2); // 2 second TTL
    ASSERT_NOT_NULL(rm);
    
    // Create a render
    char path[512];
    int result = render_manager_create_path(rm, "png", path, sizeof(path));
    ASSERT_EQ(result, RENDER_MGR_SUCCESS);
    
    // Create dummy file
    FILE *f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "test");
    fclose(f);
    
    // Register render
    result = render_manager_register(rm, path, "test_session", "png", 4);
    ASSERT_EQ(result, RENDER_MGR_SUCCESS);
    
    // File should exist
    struct stat st;
    ASSERT_TRUE(stat(path, &st) == 0);
    
    // Wait for TTL to expire
    sleep(3);
    
    // Run cleanup
    int removed_count = 0;
    size_t freed_bytes = 0;
    result = render_manager_cleanup_expired(rm, &removed_count, &freed_bytes);
    ASSERT_EQ(result, RENDER_MGR_SUCCESS);
    ASSERT_EQ(removed_count, 1);
    ASSERT_EQ(freed_bytes, 4);
    
    // File should be removed
    ASSERT_TRUE(stat(path, &st) != 0);
    
    render_manager_destroy(rm, true);
    
    TEST_END();
}

// Test 5: Cache size limits
static void test_cache_limits(test_context_t *ctx) {
    TEST_START("Cache size limits");
    
    // Create render manager with small cache limit
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 1024, 3600); // 1KB limit
    ASSERT_NOT_NULL(rm);
    
    // Add renders until we exceed limit
    for (int i = 0; i < 5; i++) {
        char path[512];
        render_manager_create_path(rm, "png", path, sizeof(path));
        
        // Create 300-byte file
        FILE *f = fopen(path, "w");
        if (f) {
            char data[300];
            memset(data, 'x', sizeof(data));
            fwrite(data, 1, sizeof(data), f);
            fclose(f);
            
            render_manager_register(rm, path, "test", "png", 300);
        }
    }
    
    // Enforce cache limit
    int removed_count = 0;
    size_t freed_bytes = 0;
    render_manager_enforce_cache_limit(rm, &removed_count, &freed_bytes);
    
    // Should have removed files to stay under 1KB
    ASSERT_TRUE(removed_count > 0);
    ASSERT_TRUE(rm->current_cache_size <= 1024);
    
    render_manager_destroy(rm, true);
    
    TEST_END();
}

// Test 6: Concurrent access
static void test_concurrent_access(test_context_t *ctx) {
    TEST_START("Concurrent access");
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 3600);
    ASSERT_NOT_NULL(rm);
    
    // Simulate concurrent renders
    #define NUM_CONCURRENT 10
    char paths[NUM_CONCURRENT][512];
    
    for (int i = 0; i < NUM_CONCURRENT; i++) {
        int result = render_manager_create_path(rm, "png", paths[i], sizeof(paths[i]));
        ASSERT_EQ(result, RENDER_MGR_SUCCESS);
        
        // Verify unique paths
        for (int j = 0; j < i; j++) {
            ASSERT_TRUE(strcmp(paths[i], paths[j]) != 0);
        }
    }
    
    // All paths should be unique
    ASSERT_EQ(rm->active_count, 0); // Not registered yet
    
    // Register all
    for (int i = 0; i < NUM_CONCURRENT; i++) {
        render_manager_register(rm, paths[i], "test", "png", 100);
    }
    
    ASSERT_EQ(rm->active_count, NUM_CONCURRENT);
    
    render_manager_destroy(rm, true);
    
    TEST_END();
}

// Test 7: Security validations
static void test_security(test_context_t *ctx) {
    TEST_START("Security validations");
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 3600);
    ASSERT_NOT_NULL(rm);
    
    // Test path traversal prevention
    char* bad_paths[] = {
        "../etc/passwd",
        "/etc/passwd",
        "../../root/.ssh/id_rsa",
        "/tmp/../etc/shadow"
    };
    
    for (int i = 0; i < 4; i++) {
        // Attempt to register with bad path
        int result = render_manager_register(rm, bad_paths[i], "test", "png", 100);
        ASSERT_TRUE(result != RENDER_MGR_SUCCESS);
    }
    
    // Security violations should be tracked
    ASSERT_TRUE(rm->security_violations > 0);
    
    render_manager_destroy(rm, true);
    
    TEST_END();
}

// Test 8: Performance benchmarks
static void test_performance(test_context_t *ctx) {
    TEST_START("Performance benchmarks");
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 3600);
    ASSERT_NOT_NULL(rm);
    
    // Benchmark path generation
    clock_t start = clock();
    char path[512];
    for (int i = 0; i < 1000; i++) {
        render_manager_create_path(rm, "png", path, sizeof(path));
    }
    double path_gen_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("  Path generation: %.2f ms/1000 ops\n", path_gen_time * 1000);
    ASSERT_TRUE(path_gen_time < 0.1); // Should be < 100ms for 1000 ops
    
    // Benchmark registration
    start = clock();
    for (int i = 0; i < 100; i++) {
        render_manager_create_path(rm, "png", path, sizeof(path));
        render_manager_register(rm, path, "perf_test", "png", 1024);
    }
    double reg_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("  Registration: %.2f ms/100 ops\n", reg_time * 1000);
    ASSERT_TRUE(reg_time < 0.05); // Should be < 50ms for 100 ops
    
    // Benchmark cleanup
    start = clock();
    int removed_count;
    size_t freed_bytes;
    render_manager_cleanup_expired(rm, &removed_count, &freed_bytes);
    double cleanup_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("  Cleanup scan: %.2f ms\n", cleanup_time * 1000);
    ASSERT_TRUE(cleanup_time < 0.01); // Should be < 10ms
    
    render_manager_destroy(rm, true);
    
    TEST_END();
}

// Test 9: List renders functionality
static void test_list_renders(test_context_t *ctx) {
    TEST_START("List renders functionality");
    
    // Create some renders
    for (int i = 0; i < 3; i++) {
        char params[256];
        snprintf(params, sizeof(params),
                "{\"width\":%d,\"height\":%d,\"options\":{\"return_mode\":\"file_path\"}}",
                100 + i*50, 100 + i*50);
        send_request(ctx->sock, "goxel.render_scene", params);
    }
    
    // List renders
    char* resp = send_request(ctx->sock, "goxel.list_renders", "{}");
    ASSERT_NOT_NULL(resp);
    ASSERT_TRUE(strstr(resp, "\"success\":true") != NULL);
    ASSERT_TRUE(strstr(resp, "\"renders\":[") != NULL);
    
    TEST_END();
}

// Test 10: Get render info
static void test_get_render_info(test_context_t *ctx) {
    TEST_START("Get render info");
    
    // Create a render
    char* resp = send_request(ctx->sock, "goxel.render_scene",
                             "{\"width\":150,\"height\":150,\"options\":{\"return_mode\":\"file_path\"}}");
    ASSERT_NOT_NULL(resp);
    
    // Extract path
    char* path_start = strstr(resp, "\"path\":\"");
    if (path_start) {
        path_start += 8;
        char* path_end = strchr(path_start, '"');
        if (path_end) {
            char path[256];
            strncpy(path, path_start, path_end - path_start);
            path[path_end - path_start] = '\0';
            
            // Get render info
            char params[512];
            snprintf(params, sizeof(params), "{\"path\":\"%s\"}", path);
            resp = send_request(ctx->sock, "goxel.get_render_info", params);
            ASSERT_NOT_NULL(resp);
            ASSERT_TRUE(strstr(resp, "\"success\":true") != NULL);
            ASSERT_TRUE(strstr(resp, "\"file\":{") != NULL);
        }
    }
    
    TEST_END();
}

// Main test runner
int main(int argc, char** argv) {
    printf("=================================================\n");
    printf("Goxel v0.16 Render Transfer - Integration Tests\n");
    printf("=================================================\n\n");
    
    test_context_t ctx = {0};
    
    // Create test directory
    mkdir(TEST_RENDER_DIR, 0755);
    
    // Connect to daemon
    ctx.sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx.sock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(ctx.sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "Failed to connect to daemon at %s\n", SOCKET_PATH);
        fprintf(stderr, "Start daemon with: ./goxel-daemon --socket %s\n", SOCKET_PATH);
        close(ctx.sock);
        return 1;
    }
    
    printf("Connected to daemon at %s\n\n", SOCKET_PATH);
    
    // Run tests
    test_basic_render(&ctx);
    test_backward_compatibility(&ctx);
    test_env_configuration(&ctx);
    test_ttl_cleanup(&ctx);
    test_cache_limits(&ctx);
    test_concurrent_access(&ctx);
    test_security(&ctx);
    test_performance(&ctx);
    test_list_renders(&ctx);
    test_get_render_info(&ctx);
    
    // Print summary
    printf("\n=================================================\n");
    printf("Test Summary\n");
    printf("=================================================\n");
    printf("Total Tests: %d\n", g_test_stats.total);
    printf("Passed: %d\n", g_test_stats.passed);
    printf("Failed: %d\n", g_test_stats.failed);
    printf("Assertions: %d\n", g_test_stats.assertions);
    
    if (g_test_stats.failed == 0) {
        printf("\n✅ ALL TESTS PASSED!\n");
    } else {
        printf("\n❌ %d TEST(S) FAILED\n", g_test_stats.failed);
    }
    
    // Cleanup
    close(ctx.sock);
    
    // Remove test directory
    system("rm -rf " TEST_RENDER_DIR);
    
    return g_test_stats.failed > 0 ? 1 : 0;
}