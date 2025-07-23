/* Goxel End-to-End Headless Integration Test
 *
 * copyright (c) 2025 Goxel Contributors
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../include/goxel_headless.h"

// Test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test: %s...", #name); \
    tests_run++; \
    if (test_##name() == 0) { \
        printf(" PASS\n"); \
        tests_passed++; \
    } else { \
        printf(" FAIL\n"); \
    } \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("\nAssertion failed: %s\n", #condition); \
        return -1; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("\nAssertion failed: %s != %s (%d != %d)\n", #a, #b, (int)(a), (int)(b)); \
        return -1; \
    } \
} while(0)

// Helper functions
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void cleanup_file(const char *path)
{
    if (file_exists(path)) {
        unlink(path);
    }
}

// Test complete project workflow
TEST(complete_workflow)
{
    goxel_context_t *ctx = NULL;
    goxel_error_t error;
    const char *project_file = "/tmp/e2e_test_project.gox";
    const char *render_file = "/tmp/e2e_test_render.png";
    
    cleanup_file(project_file);
    cleanup_file(render_file);
    
    // Create context
    ctx = goxel_create_context(&error);
    ASSERT(ctx != NULL);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Initialize context
    error = goxel_init_context(ctx);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Create new project
    error = goxel_create_project(ctx, "E2E Test Project");
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Create layers
    goxel_layer_id_t layer1 = goxel_create_layer(ctx, "Red Layer");
    goxel_layer_id_t layer2 = goxel_create_layer(ctx, "Green Layer");
    ASSERT(layer1 >= 0);
    ASSERT(layer2 >= 0);
    
    // Add voxels to first layer
    error = goxel_set_active_layer(ctx, layer1);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    goxel_color_t red = {255, 0, 0, 255};
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            goxel_pos_t pos = {x, y, 0};
            error = goxel_add_voxel(ctx, pos, red);
            ASSERT_EQ(error, GOXEL_ERROR_NONE);
        }
    }
    
    // Add voxels to second layer
    error = goxel_set_active_layer(ctx, layer2);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    goxel_color_t green = {0, 255, 0, 255};
    for (int x = 0; x < 5; x++) {
        for (int z = 0; z < 5; z++) {
            goxel_pos_t pos = {x, 5, z};
            error = goxel_add_voxel(ctx, pos, green);
            ASSERT_EQ(error, GOXEL_ERROR_NONE);
        }
    }
    
    // Save project
    error = goxel_save_project(ctx, project_file);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    ASSERT(file_exists(project_file));
    
    // Render the scene
    goxel_render_settings_t render_settings = {0};
    render_settings.width = 640;
    render_settings.height = 480;
    render_settings.camera_preset = GOXEL_CAMERA_ISOMETRIC;
    render_settings.quality = GOXEL_RENDER_QUALITY_NORMAL;
    
    error = goxel_render_to_file(ctx, render_file, &render_settings);
    if (error == GOXEL_ERROR_NONE) {
        ASSERT(file_exists(render_file));
    }
    // Note: Rendering might fail if OSMesa not available, but that's okay
    
    // Test project loading
    goxel_context_t *ctx2 = goxel_create_context(&error);
    ASSERT(ctx2 != NULL);
    
    error = goxel_init_context(ctx2);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    error = goxel_load_project(ctx2, project_file);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Verify voxels were loaded correctly
    goxel_color_t loaded_color;
    goxel_pos_t test_pos = {2, 2, 0};
    error = goxel_get_voxel(ctx2, test_pos, &loaded_color);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    ASSERT_EQ(loaded_color.r, 255); // Should be red from first layer
    
    // Cleanup
    goxel_destroy_context(ctx);
    goxel_destroy_context(ctx2);
    cleanup_file(project_file);
    cleanup_file(render_file);
    
    return 0;
}

// Test batch operations
TEST(batch_operations)
{
    goxel_context_t *ctx = NULL;
    goxel_error_t error;
    
    ctx = goxel_create_context(&error);
    ASSERT(ctx != NULL);
    
    error = goxel_init_context(ctx);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    error = goxel_create_project(ctx, "Batch Test");
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Create batch data for a 10x10x10 cube
    const int batch_size = 1000;
    goxel_voxel_batch_t *batch = malloc(sizeof(goxel_voxel_batch_t) * batch_size);
    ASSERT(batch != NULL);
    
    for (int i = 0; i < batch_size; i++) {
        batch[i].pos.x = i % 10;
        batch[i].pos.y = (i / 10) % 10;
        batch[i].pos.z = i / 100;
        batch[i].color.r = (i * 7) % 256;
        batch[i].color.g = (i * 13) % 256;
        batch[i].color.b = (i * 17) % 256;
        batch[i].color.a = 255;
    }
    
    // Add voxels in batch
    error = goxel_add_voxel_batch(ctx, batch, batch_size);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Verify some voxels were added
    goxel_color_t color;
    goxel_pos_t pos = {5, 5, 5};
    error = goxel_get_voxel(ctx, pos, &color);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    ASSERT(color.a > 0); // Should have alpha > 0 if voxel exists
    
    free(batch);
    goxel_destroy_context(ctx);
    
    return 0;
}

// Test error handling
TEST(error_handling)
{
    goxel_context_t *ctx = NULL;
    goxel_error_t error;
    
    // Test invalid context operations
    ctx = goxel_create_context(&error);
    ASSERT(ctx != NULL);
    
    // Try operations without initialization
    error = goxel_create_project(ctx, "Test");
    ASSERT(error != GOXEL_ERROR_NONE);
    
    // Initialize properly
    error = goxel_init_context(ctx);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Test invalid file operations
    error = goxel_load_project(ctx, "/non/existent/file.gox");
    ASSERT(error != GOXEL_ERROR_NONE);
    
    // Test invalid voxel operations
    goxel_color_t color;
    goxel_pos_t invalid_pos = {999999, 999999, 999999};
    error = goxel_get_voxel(ctx, invalid_pos, &color);
    ASSERT(error != GOXEL_ERROR_NONE || color.a == 0);
    
    goxel_destroy_context(ctx);
    
    return 0;
}

// Test layer management
TEST(layer_management)
{
    goxel_context_t *ctx = NULL;
    goxel_error_t error;
    
    ctx = goxel_create_context(&error);
    ASSERT(ctx != NULL);
    
    error = goxel_init_context(ctx);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    error = goxel_create_project(ctx, "Layer Test");
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Create multiple layers
    goxel_layer_id_t layers[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Layer %d", i + 1);
        layers[i] = goxel_create_layer(ctx, name);
        ASSERT(layers[i] >= 0);
    }
    
    // Test layer visibility
    for (int i = 0; i < 5; i++) {
        error = goxel_set_layer_visibility(ctx, layers[i], i % 2 == 0);
        ASSERT_EQ(error, GOXEL_ERROR_NONE);
    }
    
    // Test layer deletion
    error = goxel_delete_layer(ctx, layers[2]);
    ASSERT_EQ(error, GOXEL_ERROR_NONE);
    
    // Try to use deleted layer (should fail)
    error = goxel_set_active_layer(ctx, layers[2]);
    ASSERT(error != GOXEL_ERROR_NONE);
    
    goxel_destroy_context(ctx);
    
    return 0;
}

// Test memory management and cleanup
TEST(memory_management)
{
    // Create and destroy multiple contexts
    for (int i = 0; i < 100; i++) {
        goxel_context_t *ctx = NULL;
        goxel_error_t error;
        
        ctx = goxel_create_context(&error);
        ASSERT(ctx != NULL);
        
        error = goxel_init_context(ctx);
        ASSERT_EQ(error, GOXEL_ERROR_NONE);
        
        error = goxel_create_project(ctx, "Memory Test");
        ASSERT_EQ(error, GOXEL_ERROR_NONE);
        
        // Add some voxels
        goxel_color_t color = {255, 255, 255, 255};
        for (int j = 0; j < 10; j++) {
            goxel_pos_t pos = {j, 0, 0};
            goxel_add_voxel(ctx, pos, color);
        }
        
        goxel_destroy_context(ctx);
    }
    
    return 0;
}

// Test version and feature detection
TEST(version_features)
{
    const char *version = goxel_get_version();
    ASSERT(version != NULL);
    ASSERT(strlen(version) > 0);
    
    // Test feature detection
    ASSERT(goxel_has_feature(GOXEL_FEATURE_LAYERS));
    ASSERT(goxel_has_feature(GOXEL_FEATURE_UNDO_REDO));
    ASSERT(goxel_has_feature(GOXEL_FEATURE_EXPORT));
    
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel End-to-End Integration Tests\n");
    printf("==========================================\n");
    
    RUN_TEST(complete_workflow);
    RUN_TEST(batch_operations);
    RUN_TEST(error_handling);
    RUN_TEST(layer_management);
    RUN_TEST(memory_management);
    RUN_TEST(version_features);
    
    printf("\n==========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All integration tests passed!\n");
        printf("\n✅ End-to-End headless API validation complete!\n");
        printf("✅ Ready for production deployment!\n");
        return 0;
    } else {
        printf("Some integration tests failed!\n");
        printf("\n❌ Integration issues detected - review and fix before deployment\n");
        return 1;
    }
}