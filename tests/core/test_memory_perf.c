/* Goxel Memory and Performance Tests
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
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "../../src/core/goxel_core.h"

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

// Performance measurement helpers
static double get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static size_t get_memory_usage(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

// Test memory leak detection
TEST(memory_leak_basic)
{
    size_t mem_before = get_memory_usage();
    
    // Create and destroy contexts multiple times
    for (int i = 0; i < 100; i++) {
        goxel_core_t ctx;
        int ret = goxel_core_init(&ctx);
        ASSERT_EQ(ret, 0);
        
        ret = goxel_core_create_project(&ctx, "leak_test");
        ASSERT_EQ(ret, 0);
        
        // Add some voxels
        uint8_t color[4] = {255, 255, 255, 255};
        for (int j = 0; j < 10; j++) {
            goxel_core_add_voxel(&ctx, j, 0, 0, color);
        }
        
        goxel_core_shutdown(&ctx);
    }
    
    size_t mem_after = get_memory_usage();
    size_t mem_growth = mem_after - mem_before;
    
    // Allow some growth but not excessive (< 10MB)
    printf(" (Memory growth: %zu KB)", mem_growth / 1024);
    ASSERT(mem_growth < 10 * 1024 * 1024);
    
    return 0;
}

// Test single voxel operation performance
TEST(perf_single_voxel_ops)
{
    goxel_core_t ctx;
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "perf_test");
    ASSERT_EQ(ret, 0);
    
    uint8_t color[4] = {255, 0, 0, 255};
    
    // Test add voxel performance
    double start = get_time_ms();
    for (int i = 0; i < 1000; i++) {
        goxel_core_add_voxel(&ctx, i % 10, i / 10 % 10, i / 100, color);
    }
    double elapsed = get_time_ms() - start;
    
    printf(" (1000 adds: %.2f ms, %.2f ms/op)", elapsed, elapsed / 1000.0);
    ASSERT(elapsed < 100); // Should complete in < 100ms
    
    // Test get voxel performance
    uint8_t retrieved[4];
    start = get_time_ms();
    for (int i = 0; i < 1000; i++) {
        goxel_core_get_voxel(&ctx, i % 10, i / 10 % 10, i / 100, retrieved);
    }
    elapsed = get_time_ms() - start;
    
    printf(" (1000 gets: %.2f ms)", elapsed);
    ASSERT(elapsed < 50); // Gets should be faster
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test batch voxel operations
TEST(perf_batch_voxel_ops)
{
    goxel_core_t ctx;
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "batch_perf_test");
    ASSERT_EQ(ret, 0);
    
    // Prepare batch data
    voxel_batch_t *batch = malloc(sizeof(voxel_batch_t) * 10000);
    ASSERT(batch != NULL);
    
    for (int i = 0; i < 10000; i++) {
        batch[i].x = i % 100;
        batch[i].y = (i / 100) % 100;
        batch[i].z = i / 10000;
        batch[i].color[0] = (i * 7) % 256;
        batch[i].color[1] = (i * 13) % 256;
        batch[i].color[2] = (i * 17) % 256;
        batch[i].color[3] = 255;
    }
    
    // Test batch add performance
    double start = get_time_ms();
    ret = goxel_core_add_voxel_batch(&ctx, batch, 10000);
    double elapsed = get_time_ms() - start;
    
    ASSERT_EQ(ret, 0);
    printf(" (10k batch add: %.2f ms, %.3f ms/voxel)", elapsed, elapsed / 10000.0);
    ASSERT(elapsed < 1000); // Should complete in < 1s
    
    free(batch);
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test large scene memory usage
TEST(memory_large_scene)
{
    goxel_core_t ctx;
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "large_scene");
    ASSERT_EQ(ret, 0);
    
    size_t mem_before = get_memory_usage();
    
    // Create a 100x100x100 cube (1M voxels)
    uint8_t color[4] = {128, 128, 128, 255};
    int count = 0;
    
    for (int x = 0; x < 100; x++) {
        for (int y = 0; y < 100; y++) {
            for (int z = 0; z < 100; z++) {
                goxel_core_add_voxel(&ctx, x, y, z, color);
                count++;
            }
        }
    }
    
    size_t mem_after = get_memory_usage();
    size_t mem_used = mem_after - mem_before;
    double bytes_per_voxel = (double)mem_used / count;
    
    printf(" (1M voxels: %zu KB total, %.2f bytes/voxel)", 
           mem_used / 1024, bytes_per_voxel);
    
    // Should use less than 1KB per 1000 voxels (target from spec)
    ASSERT(bytes_per_voxel < 1.0);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test layer performance
TEST(perf_layer_operations)
{
    goxel_core_t ctx;
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "layer_perf");
    ASSERT_EQ(ret, 0);
    
    // Test layer creation performance
    double start = get_time_ms();
    int layer_ids[100];
    
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "layer_%d", i);
        layer_ids[i] = goxel_core_create_layer(&ctx, name);
        ASSERT(layer_ids[i] >= 0);
    }
    
    double elapsed = get_time_ms() - start;
    printf(" (100 layers created: %.2f ms)", elapsed);
    ASSERT(elapsed < 100);
    
    // Test layer switching performance
    start = get_time_ms();
    for (int i = 0; i < 1000; i++) {
        ret = goxel_core_set_active_layer(&ctx, layer_ids[i % 100]);
        ASSERT_EQ(ret, 0);
    }
    elapsed = get_time_ms() - start;
    
    printf(" (1000 switches: %.2f ms)", elapsed);
    ASSERT(elapsed < 50);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test save/load performance
TEST(perf_save_load)
{
    goxel_core_t ctx;
    const char *test_file = "/tmp/perf_test.gox";
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "save_load_perf");
    ASSERT_EQ(ret, 0);
    
    // Add 10k voxels
    uint8_t color[4] = {255, 128, 64, 255};
    for (int i = 0; i < 10000; i++) {
        goxel_core_add_voxel(&ctx, i % 50, (i / 50) % 50, i / 2500, color);
    }
    
    // Test save performance
    double start = get_time_ms();
    ret = goxel_core_save_project(&ctx, test_file);
    double save_time = get_time_ms() - start;
    
    ASSERT_EQ(ret, 0);
    printf(" (Save 10k voxels: %.2f ms)", save_time);
    ASSERT(save_time < 1000); // Should save in < 1s
    
    // Test load performance
    goxel_core_t ctx2;
    ret = goxel_core_init(&ctx2);
    ASSERT_EQ(ret, 0);
    
    start = get_time_ms();
    ret = goxel_core_load_project(&ctx2, test_file);
    double load_time = get_time_ms() - start;
    
    ASSERT_EQ(ret, 0);
    printf(" (Load: %.2f ms)", load_time);
    ASSERT(load_time < 1000); // Should load in < 1s
    
    goxel_core_shutdown(&ctx);
    goxel_core_shutdown(&ctx2);
    remove(test_file);
    
    return 0;
}

// Test undo/redo performance
TEST(perf_undo_redo)
{
    goxel_core_t ctx;
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "undo_perf");
    ASSERT_EQ(ret, 0);
    
    // Create 100 undo states
    uint8_t color[4] = {255, 255, 255, 255};
    
    double start = get_time_ms();
    for (int i = 0; i < 100; i++) {
        goxel_core_add_voxel(&ctx, i, 0, 0, color);
        goxel_core_push_undo(&ctx);
    }
    double create_time = get_time_ms() - start;
    
    printf(" (100 undo states: %.2f ms)", create_time);
    
    // Test undo performance
    start = get_time_ms();
    for (int i = 0; i < 100; i++) {
        ret = goxel_core_undo(&ctx);
        ASSERT_EQ(ret, 0);
    }
    double undo_time = get_time_ms() - start;
    
    printf(" (100 undos: %.2f ms)", undo_time);
    ASSERT(undo_time < 500);
    
    // Test redo performance
    start = get_time_ms();
    for (int i = 0; i < 100; i++) {
        ret = goxel_core_redo(&ctx);
        ASSERT_EQ(ret, 0);
    }
    double redo_time = get_time_ms() - start;
    
    printf(" (100 redos: %.2f ms)", redo_time);
    ASSERT(redo_time < 500);
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test concurrent operations (thread safety)
TEST(thread_safety_basic)
{
    // This test would require pthread implementation
    // For now, just test that multiple contexts can coexist
    
    goxel_core_t ctx1, ctx2, ctx3;
    
    int ret = goxel_core_init(&ctx1);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_init(&ctx2);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_init(&ctx3);
    ASSERT_EQ(ret, 0);
    
    // Create projects in each
    ret = goxel_core_create_project(&ctx1, "project1");
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx2, "project2");
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx3, "project3");
    ASSERT_EQ(ret, 0);
    
    // Add voxels to each
    uint8_t colors[3][4] = {
        {255, 0, 0, 255},
        {0, 255, 0, 255},
        {0, 0, 255, 255}
    };
    
    goxel_core_add_voxel(&ctx1, 0, 0, 0, colors[0]);
    goxel_core_add_voxel(&ctx2, 0, 0, 0, colors[1]);
    goxel_core_add_voxel(&ctx3, 0, 0, 0, colors[2]);
    
    // Verify each context maintains its own state
    uint8_t color[4];
    goxel_core_get_voxel(&ctx1, 0, 0, 0, color);
    ASSERT_EQ(color[0], 255); // Red
    
    goxel_core_get_voxel(&ctx2, 0, 0, 0, color);
    ASSERT_EQ(color[1], 255); // Green
    
    goxel_core_get_voxel(&ctx3, 0, 0, 0, color);
    ASSERT_EQ(color[2], 255); // Blue
    
    goxel_core_shutdown(&ctx1);
    goxel_core_shutdown(&ctx2);
    goxel_core_shutdown(&ctx3);
    
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel Memory and Performance Tests\n");
    printf("==========================================\n");
    
    RUN_TEST(memory_leak_basic);
    RUN_TEST(perf_single_voxel_ops);
    RUN_TEST(perf_batch_voxel_ops);
    RUN_TEST(memory_large_scene);
    RUN_TEST(perf_layer_operations);
    RUN_TEST(perf_save_load);
    RUN_TEST(perf_undo_redo);
    RUN_TEST(thread_safety_basic);
    
    printf("\n==========================================\n");
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