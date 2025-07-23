/* Goxel File Format Tests
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
#include "../../src/core/file_formats.h"

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

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("\nAssertion failed: %s != %s (\"%s\" != \"%s\")\n", #a, #b, (a), (b)); \
        return -1; \
    } \
} while(0)

// Helper functions
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void cleanup_test_file(const char *path)
{
    if (file_exists(path)) {
        remove(path);
    }
}

// Test format detection
TEST(format_detection)
{
    file_format_t format;
    
    // Test .gox format
    format = detect_format_from_path("test.gox");
    ASSERT_EQ(format, FORMAT_GOX);
    
    // Test .vox format (MagicaVoxel)
    format = detect_format_from_path("model.vox");
    ASSERT_EQ(format, FORMAT_VOX);
    
    // Test .ply format
    format = detect_format_from_path("mesh.ply");
    ASSERT_EQ(format, FORMAT_PLY);
    
    // Test .obj format
    format = detect_format_from_path("model.obj");
    ASSERT_EQ(format, FORMAT_OBJ);
    
    // Test .qb format (Qubicle)
    format = detect_format_from_path("voxels.qb");
    ASSERT_EQ(format, FORMAT_QB);
    
    // Test unknown format
    format = detect_format_from_path("unknown.xyz");
    ASSERT_EQ(format, FORMAT_UNKNOWN);
    
    return 0;
}

// Test GOX format save/load
TEST(gox_format)
{
    goxel_core_t ctx;
    const char *test_file = "/tmp/test_gox_format.gox";
    
    cleanup_test_file(test_file);
    
    // Initialize and create project
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_gox");
    ASSERT_EQ(ret, 0);
    
    // Add some voxels
    uint8_t red[4] = {255, 0, 0, 255};
    uint8_t green[4] = {0, 255, 0, 255};
    uint8_t blue[4] = {0, 0, 255, 255};
    
    goxel_core_add_voxel(&ctx, 0, 0, 0, red);
    goxel_core_add_voxel(&ctx, 1, 0, 0, green);
    goxel_core_add_voxel(&ctx, 0, 1, 0, blue);
    
    // Save project
    ret = goxel_core_save_project(&ctx, test_file);
    ASSERT_EQ(ret, 0);
    ASSERT(file_exists(test_file));
    
    // Create new context and load
    goxel_core_t ctx2;
    ret = goxel_core_init(&ctx2);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_load_project(&ctx2, test_file);
    ASSERT_EQ(ret, 0);
    
    // Verify voxels were loaded
    uint8_t color[4];
    ret = goxel_core_get_voxel(&ctx2, 0, 0, 0, color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(color[0], 255); // Red
    
    ret = goxel_core_get_voxel(&ctx2, 1, 0, 0, color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(color[1], 255); // Green
    
    // Cleanup
    goxel_core_shutdown(&ctx);
    goxel_core_shutdown(&ctx2);
    cleanup_test_file(test_file);
    
    return 0;
}

// Test export to OBJ format
TEST(obj_export)
{
    goxel_core_t ctx;
    const char *test_file = "/tmp/test_export.obj";
    
    cleanup_test_file(test_file);
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_obj_export");
    ASSERT_EQ(ret, 0);
    
    // Create a simple cube
    uint8_t white[4] = {255, 255, 255, 255};
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            for (int z = 0; z < 3; z++) {
                goxel_core_add_voxel(&ctx, x, y, z, white);
            }
        }
    }
    
    // Export to OBJ
    export_options_t options = {0};
    options.format = FORMAT_OBJ;
    options.include_colors = 1;
    
    ret = goxel_core_export(&ctx, test_file, &options);
    ASSERT_EQ(ret, 0);
    ASSERT(file_exists(test_file));
    
    // Check that MTL file was also created
    char mtl_file[256];
    snprintf(mtl_file, sizeof(mtl_file), "/tmp/test_export.mtl");
    ASSERT(file_exists(mtl_file));
    
    // Cleanup
    goxel_core_shutdown(&ctx);
    cleanup_test_file(test_file);
    cleanup_test_file(mtl_file);
    
    return 0;
}

// Test export to PLY format
TEST(ply_export)
{
    goxel_core_t ctx;
    const char *test_file = "/tmp/test_export.ply";
    
    cleanup_test_file(test_file);
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_ply_export");
    ASSERT_EQ(ret, 0);
    
    // Add some colored voxels
    uint8_t colors[3][4] = {
        {255, 0, 0, 255},    // Red
        {0, 255, 0, 255},    // Green
        {0, 0, 255, 255}     // Blue
    };
    
    for (int i = 0; i < 3; i++) {
        goxel_core_add_voxel(&ctx, i, 0, 0, colors[i]);
    }
    
    // Export to PLY
    export_options_t options = {0};
    options.format = FORMAT_PLY;
    options.binary = 0; // ASCII PLY for easier verification
    
    ret = goxel_core_export(&ctx, test_file, &options);
    ASSERT_EQ(ret, 0);
    ASSERT(file_exists(test_file));
    
    // Read first few lines to verify PLY header
    FILE *f = fopen(test_file, "r");
    ASSERT(f != NULL);
    
    char line[256];
    fgets(line, sizeof(line), f);
    ASSERT(strstr(line, "ply") != NULL);
    
    fclose(f);
    
    // Cleanup
    goxel_core_shutdown(&ctx);
    cleanup_test_file(test_file);
    
    return 0;
}

// Test batch export
TEST(batch_export)
{
    goxel_core_t ctx;
    const char *base_name = "/tmp/test_batch";
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_batch");
    ASSERT_EQ(ret, 0);
    
    // Add voxels
    uint8_t white[4] = {255, 255, 255, 255};
    goxel_core_add_voxel(&ctx, 0, 0, 0, white);
    
    // Test exporting to multiple formats
    file_format_t formats[] = {FORMAT_OBJ, FORMAT_PLY, FORMAT_STL};
    const char *extensions[] = {".obj", ".ply", ".stl"};
    
    for (int i = 0; i < 3; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s%s", base_name, extensions[i]);
        cleanup_test_file(filename);
        
        export_options_t options = {0};
        options.format = formats[i];
        
        ret = goxel_core_export(&ctx, filename, &options);
        ASSERT_EQ(ret, 0);
        ASSERT(file_exists(filename));
        
        cleanup_test_file(filename);
    }
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test format validation
TEST(format_validation)
{
    // Test valid formats
    ASSERT(is_format_supported(FORMAT_GOX, FORMAT_CAP_READ | FORMAT_CAP_WRITE));
    ASSERT(is_format_supported(FORMAT_VOX, FORMAT_CAP_READ | FORMAT_CAP_WRITE));
    ASSERT(is_format_supported(FORMAT_OBJ, FORMAT_CAP_WRITE));
    ASSERT(is_format_supported(FORMAT_PLY, FORMAT_CAP_WRITE));
    
    // Test format capabilities
    format_caps_t caps = get_format_capabilities(FORMAT_GOX);
    ASSERT(caps & FORMAT_CAP_READ);
    ASSERT(caps & FORMAT_CAP_WRITE);
    ASSERT(caps & FORMAT_CAP_LAYERS);
    
    caps = get_format_capabilities(FORMAT_OBJ);
    ASSERT(caps & FORMAT_CAP_WRITE);
    ASSERT(!(caps & FORMAT_CAP_READ)); // OBJ is write-only
    
    return 0;
}

// Test import error handling
TEST(import_errors)
{
    goxel_core_t ctx;
    int ret;
    
    ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    // Try to load non-existent file
    ret = goxel_core_load_project(&ctx, "/tmp/non_existent_file.gox");
    ASSERT(ret != 0);
    
    // Try to load file with wrong extension
    const char *bad_file = "/tmp/test_bad.txt";
    FILE *f = fopen(bad_file, "w");
    if (f) {
        fprintf(f, "This is not a voxel file\n");
        fclose(f);
        
        ret = goxel_core_load_project(&ctx, bad_file);
        ASSERT(ret != 0);
        
        cleanup_test_file(bad_file);
    }
    
    goxel_core_shutdown(&ctx);
    return 0;
}

// Test metadata handling
TEST(metadata)
{
    goxel_core_t ctx;
    const char *test_file = "/tmp/test_metadata.gox";
    
    cleanup_test_file(test_file);
    
    int ret = goxel_core_init(&ctx);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_create_project(&ctx, "test_metadata");
    ASSERT_EQ(ret, 0);
    
    // Set metadata
    project_metadata_t meta = {0};
    strncpy(meta.author, "Test Author", sizeof(meta.author));
    strncpy(meta.description, "Test Description", sizeof(meta.description));
    meta.version = 1;
    
    ret = goxel_core_set_metadata(&ctx, &meta);
    ASSERT_EQ(ret, 0);
    
    // Save with metadata
    ret = goxel_core_save_project(&ctx, test_file);
    ASSERT_EQ(ret, 0);
    
    // Load and verify metadata
    goxel_core_t ctx2;
    ret = goxel_core_init(&ctx2);
    ASSERT_EQ(ret, 0);
    
    ret = goxel_core_load_project(&ctx2, test_file);
    ASSERT_EQ(ret, 0);
    
    project_metadata_t loaded_meta;
    ret = goxel_core_get_metadata(&ctx2, &loaded_meta);
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(loaded_meta.author, "Test Author");
    ASSERT_STR_EQ(loaded_meta.description, "Test Description");
    ASSERT_EQ(loaded_meta.version, 1);
    
    goxel_core_shutdown(&ctx);
    goxel_core_shutdown(&ctx2);
    cleanup_test_file(test_file);
    
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel File Format Tests\n");
    printf("================================\n");
    
    RUN_TEST(format_detection);
    RUN_TEST(gox_format);
    RUN_TEST(obj_export);
    RUN_TEST(ply_export);
    RUN_TEST(batch_export);
    RUN_TEST(format_validation);
    RUN_TEST(import_errors);
    RUN_TEST(metadata);
    
    printf("\n================================\n");
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