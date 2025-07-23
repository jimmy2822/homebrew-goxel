/* Goxel Headless Rendering Tests
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

#include "../../src/core/goxel_core.h"
#include "../../src/headless/render_headless.h"
#include "../../src/headless/camera_headless.h"

// Test framework from test_goxel_core.c
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

// Helper to check if file exists
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// Test headless rendering initialization
TEST(render_init)
{
    int ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    headless_render_cleanup();
    return 0;
}

// Test context creation
TEST(render_context_creation)
{
    int ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    void *context = headless_render_create_context(1920, 1080);
    ASSERT(context != NULL);
    
    headless_render_destroy_context(context);
    headless_render_cleanup();
    return 0;
}

// Test camera presets
TEST(camera_presets)
{
    camera_t cam;
    float mat[4][4];
    
    // Test front view
    camera_set_preset(&cam, CAMERA_PRESET_FRONT);
    camera_get_view_matrix(&cam, mat);
    ASSERT(mat[3][2] != 0); // Should have Z translation
    
    // Test isometric view
    camera_set_preset(&cam, CAMERA_PRESET_ISOMETRIC);
    camera_get_view_matrix(&cam, mat);
    // Just verify it doesn't crash
    
    return 0;
}

// Test render to file
TEST(render_to_file)
{
    goxel_core_t ctx;
    int ret;
    
    ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_render");
    ASSERT_EQ(ret, 0);
    
    // Add some voxels to render
    uint8_t red[4] = {255, 0, 0, 255};
    uint8_t green[4] = {0, 255, 0, 255};
    uint8_t blue[4] = {0, 0, 255, 255};
    
    goxel_core_add_voxel(&ctx, 0, 0, 0, red);
    goxel_core_add_voxel(&ctx, 1, 0, 0, green);
    goxel_core_add_voxel(&ctx, 0, 1, 0, blue);
    
    ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    // Try to render (may fail if OSMesa not available)
    ret = headless_render_to_file(&ctx, "/tmp/test_render.png", 640, 480, CAMERA_PRESET_FRONT);
    if (ret == 0) {
        ASSERT(file_exists("/tmp/test_render.png"));
        remove("/tmp/test_render.png");
    }
    
    headless_render_cleanup();
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test render quality settings
TEST(render_quality)
{
    render_settings_t settings;
    
    // Test default settings
    render_settings_init(&settings);
    ASSERT_EQ(settings.quality, RENDER_QUALITY_NORMAL);
    ASSERT_EQ(settings.samples, 1);
    
    // Test high quality
    settings.quality = RENDER_QUALITY_HIGH;
    render_settings_apply(&settings);
    ASSERT(settings.samples > 1);
    
    return 0;
}

// Test render to buffer
TEST(render_to_buffer)
{
    goxel_core_t ctx;
    int ret;
    
    ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_buffer_render");
    ASSERT_EQ(ret, 0);
    
    // Add a voxel
    uint8_t white[4] = {255, 255, 255, 255};
    goxel_core_add_voxel(&ctx, 0, 0, 0, white);
    
    ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    // Allocate buffer for 100x100 RGBA image
    uint8_t *buffer = malloc(100 * 100 * 4);
    ASSERT(buffer != NULL);
    
    ret = headless_render_to_buffer(&ctx, buffer, 100, 100, CAMERA_PRESET_FRONT);
    if (ret == 0) {
        // Check that buffer has some non-zero data
        int has_data = 0;
        for (int i = 0; i < 100 * 100 * 4; i++) {
            if (buffer[i] != 0) {
                has_data = 1;
                break;
            }
        }
        ASSERT(has_data);
    }
    
    free(buffer);
    headless_render_cleanup();
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test multiple render contexts
TEST(multiple_contexts)
{
    int ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    void *ctx1 = headless_render_create_context(640, 480);
    void *ctx2 = headless_render_create_context(1920, 1080);
    
    ASSERT(ctx1 != NULL);
    ASSERT(ctx2 != NULL);
    ASSERT(ctx1 != ctx2);
    
    headless_render_destroy_context(ctx1);
    headless_render_destroy_context(ctx2);
    headless_render_cleanup();
    return 0;
}

// Test render progress callback
TEST(render_progress)
{
    static int progress_calls = 0;
    
    void progress_callback(float progress) {
        progress_calls++;
        ASSERT(progress >= 0.0f && progress <= 1.0f);
    }
    
    goxel_core_t ctx;
    int ret;
    
    ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_progress");
    ASSERT_EQ(ret, 0);
    
    render_settings_t settings;
    render_settings_init(&settings);
    settings.progress_callback = progress_callback;
    
    // Reset counter
    progress_calls = 0;
    
    ret = headless_render_init();
    ASSERT_EQ(ret, 0);
    
    ret = headless_render_with_settings(&ctx, "/tmp/test_progress.png", 
                                        640, 480, CAMERA_PRESET_FRONT, &settings);
    
    if (ret == 0) {
        ASSERT(progress_calls > 0);
        remove("/tmp/test_progress.png");
    }
    
    headless_render_cleanup();
    goxel_core_shutdown(&ctx);
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel Headless Rendering Tests\n");
    printf("======================================\n");
    
    RUN_TEST(render_init);
    RUN_TEST(render_context_creation);
    RUN_TEST(camera_presets);
    RUN_TEST(render_to_file);
    RUN_TEST(render_quality);
    RUN_TEST(render_to_buffer);
    RUN_TEST(multiple_contexts);
    RUN_TEST(render_progress);
    
    printf("\n======================================\n");
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