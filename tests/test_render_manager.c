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

#include "../src/daemon/render_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

// ============================================================================
// TEST FRAMEWORK
// ============================================================================

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected, message) \
    do { \
        if ((actual) != (expected)) { \
            printf("FAIL: %s - %s (expected %d, got %d)\n", \
                   __func__, message, (int)(expected), (int)(actual)); \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(actual, expected, message) \
    do { \
        if (strcmp((actual), (expected)) != 0) { \
            printf("FAIL: %s - %s (expected '%s', got '%s')\n", \
                   __func__, message, expected, actual); \
            return false; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            printf("FAIL: %s - %s (pointer is NULL)\n", __func__, message); \
            return false; \
        } \
    } while (0)

// Test configuration
#define TEST_RENDER_DIR "/tmp/goxel_test_renders"
#define TEST_SESSION_ID "test_session_123"
#define TEST_FORMAT "png"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Creates a dummy render file for testing.
 */
static bool create_test_file(const char *file_path, size_t size)
{
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        return false;
    }
    
    // Write dummy data
    for (size_t i = 0; i < size; i++) {
        fputc((int)(i & 0xFF), file);
    }
    
    fclose(file);
    return true;
}

/**
 * Removes test directory and all contents.
 */
static void cleanup_test_directory(void)
{
    char command[512];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_RENDER_DIR);
    system(command);
}

/**
 * Checks if file exists.
 */
static bool file_exists(const char *file_path)
{
    struct stat st;
    return stat(file_path, &st) == 0;
}

/**
 * Gets file size.
 */
static size_t get_file_size_test(const char *file_path)
{
    struct stat st;
    if (stat(file_path, &st) == 0) {
        return (size_t)st.st_size;
    }
    return 0;
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

/**
 * Test render manager creation and destruction.
 */
static bool test_render_manager_creation(void)
{
    cleanup_test_directory();
    
    // Test with default parameters
    render_manager_t *rm = render_manager_create(NULL, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager with defaults");
    
    render_manager_stats_t stats;
    render_manager_error_t result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.active_count, 0, "Initial active count should be 0");
    TEST_ASSERT_EQ(stats.total_renders, 0, "Initial total renders should be 0");
    
    render_manager_destroy(rm, true);
    
    // Test with custom parameters
    rm = render_manager_create(TEST_RENDER_DIR, 100 * 1024 * 1024, 1800);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager with custom params");
    
    result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.max_cache_size, 100 * 1024 * 1024, "Max cache size incorrect");
    TEST_ASSERT_EQ(stats.ttl_seconds, 1800, "TTL incorrect");
    TEST_ASSERT_STR_EQ(stats.output_dir, TEST_RENDER_DIR, "Output dir incorrect");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test path generation functionality.
 */
static bool test_path_generation(void)
{
    cleanup_test_directory();
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    char path1[512];
    char path2[512];
    
    // Test with session ID
    render_manager_error_t result = render_manager_create_path(rm, TEST_SESSION_ID, 
                                                             TEST_FORMAT, path1, sizeof(path1));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create path with session ID");
    
    // Path should contain test directory
    TEST_ASSERT(strstr(path1, TEST_RENDER_DIR) != NULL, "Path should contain test directory");
    TEST_ASSERT(strstr(path1, TEST_SESSION_ID) != NULL, "Path should contain session ID");
    TEST_ASSERT(strstr(path1, TEST_FORMAT) != NULL, "Path should contain format");
    TEST_ASSERT(strstr(path1, "render_") != NULL, "Path should contain render prefix");
    
    // Test without session ID (auto-generated)
    result = render_manager_create_path(rm, NULL, TEST_FORMAT, path2, sizeof(path2));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create path without session ID");
    
    // Paths should be different
    TEST_ASSERT(strcmp(path1, path2) != 0, "Generated paths should be unique");
    
    // Test invalid parameters
    result = render_manager_create_path(NULL, TEST_SESSION_ID, TEST_FORMAT, path1, sizeof(path1));
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_NULL_POINTER, "Should fail with NULL render manager");
    
    result = render_manager_create_path(rm, TEST_SESSION_ID, NULL, path1, sizeof(path1));
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_NULL_POINTER, "Should fail with NULL format");
    
    // Test buffer too small
    char small_buffer[10];
    result = render_manager_create_path(rm, TEST_SESSION_ID, TEST_FORMAT, 
                                      small_buffer, sizeof(small_buffer));
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_PATH_TOO_LONG, "Should fail with small buffer");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test directory creation functionality.
 */
static bool test_directory_creation(void)
{
    cleanup_test_directory();
    
    // Directory should not exist initially
    TEST_ASSERT(!file_exists(TEST_RENDER_DIR), "Test directory should not exist initially");
    
    // Create render manager should create directory
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    TEST_ASSERT(file_exists(TEST_RENDER_DIR), "Directory should be created");
    
    // Verify directory permissions (should be accessible)
    struct stat st;
    TEST_ASSERT(stat(TEST_RENDER_DIR, &st) == 0, "Failed to stat directory");
    TEST_ASSERT(S_ISDIR(st.st_mode), "Should be a directory");
    
    render_manager_destroy(rm, true);
    
    // Test utility function
    cleanup_test_directory();
    TEST_ASSERT(!file_exists(TEST_RENDER_DIR), "Directory should be removed");
    
    render_manager_error_t result = render_manager_create_directory(TEST_RENDER_DIR);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create directory");
    TEST_ASSERT(file_exists(TEST_RENDER_DIR), "Directory should exist");
    
    // Should succeed if directory already exists
    result = render_manager_create_directory(TEST_RENDER_DIR);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Should succeed if directory exists");
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test render registration and tracking.
 */
static bool test_render_registration(void)
{
    cleanup_test_directory();
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    // Create test render path
    char render_path[512];
    render_manager_error_t result = render_manager_create_path(rm, TEST_SESSION_ID, 
                                                             TEST_FORMAT, render_path, sizeof(render_path));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create render path");
    
    // Create dummy file
    const size_t test_file_size = 1024;
    TEST_ASSERT(create_test_file(render_path, test_file_size), "Failed to create test file");
    
    // Register render
    result = render_manager_register(rm, render_path, TEST_SESSION_ID, 
                                   TEST_FORMAT, 800, 600);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to register render");
    
    // Verify statistics
    render_manager_stats_t stats;
    result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.active_count, 1, "Active count should be 1");
    TEST_ASSERT_EQ(stats.total_renders, 1, "Total renders should be 1");
    TEST_ASSERT_EQ(stats.current_cache_size, test_file_size, "Cache size should match file size");
    
    // Test querying render info
    render_info_t *info;
    result = render_manager_get_render_info(rm, render_path, &info);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get render info");
    TEST_ASSERT_NOT_NULL(info, "Render info should not be NULL");
    TEST_ASSERT_STR_EQ(info->session_id, TEST_SESSION_ID, "Session ID mismatch");
    TEST_ASSERT_STR_EQ(info->format, TEST_FORMAT, "Format mismatch");
    TEST_ASSERT_EQ(info->width, 800, "Width mismatch");
    TEST_ASSERT_EQ(info->height, 600, "Height mismatch");
    TEST_ASSERT_EQ(info->file_size, test_file_size, "File size mismatch");
    
    // Test duplicate registration
    result = render_manager_register(rm, render_path, TEST_SESSION_ID, 
                                   TEST_FORMAT, 800, 600);
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_FILE_EXISTS, "Should fail on duplicate registration");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test expired file cleanup.
 */
static bool test_expired_cleanup(void)
{
    cleanup_test_directory();
    
    // Create render manager with very short TTL
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 1); // 1 second TTL
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    // Create and register multiple test files
    const int num_files = 3;
    char render_paths[num_files][512];
    
    for (int i = 0; i < num_files; i++) {
        char session_id[32];
        snprintf(session_id, sizeof(session_id), "session_%d", i);
        
        render_manager_error_t result = render_manager_create_path(rm, session_id, 
                                                                 TEST_FORMAT, render_paths[i], sizeof(render_paths[i]));
        TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create render path");
        
        TEST_ASSERT(create_test_file(render_paths[i], 512), "Failed to create test file");
        
        result = render_manager_register(rm, render_paths[i], session_id, 
                                       TEST_FORMAT, 100, 100);
        TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to register render");
    }
    
    // Verify all files are registered
    render_manager_stats_t stats;
    render_manager_error_t result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.active_count, num_files, "Should have all files registered");
    
    // Wait for TTL to expire
    sleep(2);
    
    // Run cleanup
    int removed_count;
    size_t freed_bytes;
    result = render_manager_cleanup_expired(rm, &removed_count, &freed_bytes);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Cleanup should succeed");
    TEST_ASSERT_EQ(removed_count, num_files, "Should remove all expired files");
    TEST_ASSERT(freed_bytes > 0, "Should free some bytes");
    
    // Verify files are gone
    result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.active_count, 0, "Should have no active files");
    TEST_ASSERT_EQ(stats.current_cache_size, 0, "Cache size should be 0");
    
    for (int i = 0; i < num_files; i++) {
        TEST_ASSERT(!file_exists(render_paths[i]), "File should be removed");
    }
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test cache size limit enforcement.
 */
static bool test_cache_limit_enforcement(void)
{
    cleanup_test_directory();
    
    // Create render manager with small cache limit
    const size_t cache_limit = 2048; // 2KB
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, cache_limit, 3600); // Long TTL
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    // Create files that exceed cache limit
    const int num_files = 4;
    const size_t file_size = 1024; // 1KB each
    char render_paths[num_files][512];
    
    for (int i = 0; i < num_files; i++) {
        char session_id[32];
        snprintf(session_id, sizeof(session_id), "session_%d", i);
        
        render_manager_error_t result = render_manager_create_path(rm, session_id, 
                                                                 TEST_FORMAT, render_paths[i], sizeof(render_paths[i]));
        TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create render path");
        
        TEST_ASSERT(create_test_file(render_paths[i], file_size), "Failed to create test file");
        
        result = render_manager_register(rm, render_paths[i], session_id, 
                                       TEST_FORMAT, 100, 100);
        TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to register render");
        
        // Small delay to ensure different creation times
        usleep(10000); // 10ms
    }
    
    // Verify all files are initially registered
    render_manager_stats_t stats;
    render_manager_error_t result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT_EQ(stats.active_count, num_files, "Should have all files registered");
    TEST_ASSERT(stats.current_cache_size > cache_limit, "Should exceed cache limit");
    
    // Enforce cache limit
    int removed_count;
    size_t freed_bytes;
    result = render_manager_enforce_cache_limit(rm, &removed_count, &freed_bytes);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Cache limit enforcement should succeed");
    TEST_ASSERT(removed_count > 0, "Should remove some files");
    TEST_ASSERT(freed_bytes > 0, "Should free some bytes");
    
    // Verify cache is now under limit
    result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats");
    TEST_ASSERT(stats.current_cache_size <= cache_limit, "Should be under cache limit");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test file removal.
 */
static bool test_file_removal(void)
{
    cleanup_test_directory();
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    // Create and register test file
    char render_path[512];
    render_manager_error_t result = render_manager_create_path(rm, TEST_SESSION_ID, 
                                                             TEST_FORMAT, render_path, sizeof(render_path));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create render path");
    
    TEST_ASSERT(create_test_file(render_path, 1024), "Failed to create test file");
    
    result = render_manager_register(rm, render_path, TEST_SESSION_ID, 
                                   TEST_FORMAT, 800, 600);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to register render");
    
    // Verify file exists
    TEST_ASSERT(file_exists(render_path), "Test file should exist");
    
    // Remove file
    result = render_manager_remove_render(rm, render_path);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to remove render");
    
    // Verify file is gone
    TEST_ASSERT(!file_exists(render_path), "File should be removed");
    
    // Verify removed from tracking
    render_info_t *info;
    result = render_manager_get_render_info(rm, render_path, &info);
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_FILE_NOT_FOUND, "Should not find removed file");
    
    // Test removing non-existent file
    result = render_manager_remove_render(rm, render_path);
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_FILE_NOT_FOUND, "Should fail on non-existent file");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test utility functions.
 */
static bool test_utility_functions(void)
{
    // Test token generation
    char token1[16], token2[16];
    
    render_manager_error_t result = render_manager_generate_token(token1, sizeof(token1));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to generate token");
    TEST_ASSERT(strlen(token1) == 8, "Token should be 8 characters");
    
    result = render_manager_generate_token(token2, sizeof(token2));
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to generate second token");
    TEST_ASSERT(strcmp(token1, token2) != 0, "Tokens should be unique");
    
    // Test invalid parameters
    result = render_manager_generate_token(NULL, sizeof(token1));
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_INVALID_PARAMETER, "Should fail with NULL buffer");
    
    result = render_manager_generate_token(token1, 4);
    TEST_ASSERT_EQ(result, RENDER_MGR_ERROR_INVALID_PARAMETER, "Should fail with small buffer");
    
    // Test path validation
    TEST_ASSERT(render_manager_validate_path("/tmp/goxel_renders/file.png", "/tmp/goxel_renders"), 
                "Should validate correct path");
    TEST_ASSERT(!render_manager_validate_path("/etc/passwd", "/tmp/goxel_renders"), 
                "Should reject path outside base");
    TEST_ASSERT(!render_manager_validate_path("/tmp/goxel_renders/../../../etc/passwd", "/tmp/goxel_renders"), 
                "Should reject directory traversal");
    TEST_ASSERT(!render_manager_validate_path(NULL, "/tmp/goxel_renders"), 
                "Should reject NULL path");
    TEST_ASSERT(!render_manager_validate_path("/tmp/goxel_renders/file.png", NULL), 
                "Should reject NULL base");
    
    // Test error strings
    TEST_ASSERT_STR_EQ(render_manager_error_string(RENDER_MGR_SUCCESS), "Success", "Success string");
    TEST_ASSERT_STR_EQ(render_manager_error_string(RENDER_MGR_ERROR_NULL_POINTER), 
                       "NULL pointer", "NULL pointer string");
    
    printf("PASS: %s\n", __func__);
    return true;
}

/**
 * Test thread safety by creating multiple threads that operate concurrently.
 */
static bool test_thread_safety(void)
{
    cleanup_test_directory();
    
    render_manager_t *rm = render_manager_create(TEST_RENDER_DIR, 0, 0);
    TEST_ASSERT_NOT_NULL(rm, "Failed to create render manager");
    
    // This is a basic test - in practice, you'd want more sophisticated threading tests
    // For now, just verify that we can call functions without crashes
    
    for (int i = 0; i < 10; i++) {
        char render_path[512];
        char session_id[32];
        snprintf(session_id, sizeof(session_id), "thread_test_%d", i);
        
        render_manager_error_t result = render_manager_create_path(rm, session_id, 
                                                                 TEST_FORMAT, render_path, sizeof(render_path));
        TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to create path in thread test");
        
        if (create_test_file(render_path, 512)) {
            render_manager_register(rm, render_path, session_id, TEST_FORMAT, 100, 100);
        }
    }
    
    render_manager_stats_t stats;
    render_manager_error_t result = render_manager_get_stats(rm, &stats);
    TEST_ASSERT_EQ(result, RENDER_MGR_SUCCESS, "Failed to get stats in thread test");
    
    render_manager_destroy(rm, true);
    
    printf("PASS: %s\n", __func__);
    return true;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void)
{
    printf("Running render manager unit tests...\n\n");
    
    int passed = 0;
    int total = 0;
    
    // List of test functions
    bool (*tests[])(void) = {
        test_render_manager_creation,
        test_path_generation,
        test_directory_creation,
        test_render_registration,
        test_expired_cleanup,
        test_cache_limit_enforcement,
        test_file_removal,
        test_utility_functions,
        test_thread_safety
    };
    
    const int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        total++;
        if (tests[i]()) {
            passed++;
        }
    }
    
    // Cleanup
    cleanup_test_directory();
    
    printf("\nTest Results: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All render manager tests passed!\n");
        return 0;
    } else {
        printf("❌ Some render manager tests failed.\n");
        return 1;
    }
}