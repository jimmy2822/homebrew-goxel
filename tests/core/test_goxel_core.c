/* Goxel Core API Tests
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

// Include the core API
#include "../../src/core/goxel_core.h"

// Simple test framework
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

// Test core initialization and shutdown
TEST(core_init_shutdown)
{
    goxel_core_t ctx;
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    ASSERT(ctx.tool_radius == 1);
    ASSERT(ctx.painter_color[0] == 255);
    ASSERT(ctx.painter_color[1] == 255);
    ASSERT(ctx.painter_color[2] == 255);
    ASSERT(ctx.painter_color[3] == 255);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test project creation
TEST(project_creation)
{
    goxel_core_t ctx;
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_project");
    ASSERT_EQ(ret, 0);
    ASSERT(ctx.image != NULL);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test layer operations
TEST(layer_operations)
{
    goxel_core_t ctx;
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_project");
    ASSERT_EQ(ret, 0);
    
    // Create a new layer
    int layer_id = goxel_core_create_layer(&ctx, "test_layer");
    ASSERT(layer_id >= 0);
    
    // Set active layer
    ret = goxel_core_set_active_layer(&ctx, layer_id);
    ASSERT_EQ(ret, 0);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test voxel operations
TEST(voxel_operations)
{
    goxel_core_t ctx;
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_project");
    ASSERT_EQ(ret, 0);
    
    // Add a voxel
    uint8_t red_color[4] = {255, 0, 0, 255};
    ret = goxel_core_add_voxel(&ctx, 0, 0, 0, red_color);
    ASSERT_EQ(ret, 0);
    
    // Check the voxel
    uint8_t retrieved_color[4];
    ret = goxel_core_get_voxel(&ctx, 0, 0, 0, retrieved_color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(retrieved_color[0], 255); // Red
    ASSERT_EQ(retrieved_color[1], 0);   // Green
    ASSERT_EQ(retrieved_color[2], 0);   // Blue
    ASSERT_EQ(retrieved_color[3], 255); // Alpha
    
    // Remove the voxel
    ret = goxel_core_remove_voxel(&ctx, 0, 0, 0);
    ASSERT_EQ(ret, 0);
    
    // Check it's removed (should be transparent)
    ret = goxel_core_get_voxel(&ctx, 0, 0, 0, retrieved_color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(retrieved_color[3], 0); // Alpha should be 0
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test project management
TEST(project_management)
{
    goxel_core_t ctx;
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    // Test creating projects
    ret = goxel_core_create_project(&ctx, "test_project_1");
    ASSERT_EQ(ret, 0);
    
    // Test resetting to new project
    goxel_core_reset(&ctx);
    ASSERT(ctx.image != NULL);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test volume operations directly
TEST(volume_direct_operations)
{
    volume_t *vol = volume_new();
    ASSERT(vol != NULL);
    
    // Test basic volume operations
    float pos[3] = {1.0f, 2.0f, 3.0f};
    uint8_t color[4] = {128, 64, 32, 255};
    
    volume_set_at(vol, NULL, pos, color);
    
    uint8_t retrieved[4];
    volume_get_at(vol, NULL, pos, retrieved);
    
    ASSERT_EQ(retrieved[0], 128);
    ASSERT_EQ(retrieved[1], 64);
    ASSERT_EQ(retrieved[2], 32);
    ASSERT_EQ(retrieved[3], 255);
    
    volume_delete(vol);
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel Core API Tests\n");
    printf("============================\n");
    
    RUN_TEST(core_init_shutdown);
    RUN_TEST(project_creation);
    RUN_TEST(layer_operations);
    RUN_TEST(voxel_operations);
    RUN_TEST(project_management);
    RUN_TEST(volume_direct_operations);
    
    printf("\n============================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed!\n");
        return 1;
    }
}