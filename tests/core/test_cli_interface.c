/* Goxel CLI Interface Tests
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

#include "../../src/headless/cli_interface.h"

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

// Test command line argument parsing
TEST(parse_args_create)
{
    const char *argv[] = {"goxel-cli", "create", "myproject", "--size", "32,32,32"};
    int argc = 5;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(args.command, "create");
    ASSERT_STR_EQ(args.project_name, "myproject");
    ASSERT_EQ(args.size[0], 32);
    ASSERT_EQ(args.size[1], 32);
    ASSERT_EQ(args.size[2], 32);
    
    return 0;
}

// Test voxel-add command parsing
TEST(parse_args_voxel_add)
{
    const char *argv[] = {"goxel-cli", "voxel-add", "--pos", "10,20,30", 
                          "--color", "255,0,0,255", "--layer", "2"};
    int argc = 8;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(args.command, "voxel-add");
    ASSERT_EQ(args.position[0], 10);
    ASSERT_EQ(args.position[1], 20);
    ASSERT_EQ(args.position[2], 30);
    ASSERT_EQ(args.color[0], 255);
    ASSERT_EQ(args.color[1], 0);
    ASSERT_EQ(args.color[2], 0);
    ASSERT_EQ(args.color[3], 255);
    ASSERT_EQ(args.layer_id, 2);
    
    return 0;
}

// Test render command parsing
TEST(parse_args_render)
{
    const char *argv[] = {"goxel-cli", "render", "--output", "output.png",
                          "--resolution", "1920x1080", "--camera", "isometric"};
    int argc = 7;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(args.command, "render");
    ASSERT_STR_EQ(args.output_path, "output.png");
    ASSERT_EQ(args.resolution[0], 1920);
    ASSERT_EQ(args.resolution[1], 1080);
    ASSERT_STR_EQ(args.camera_preset, "isometric");
    
    return 0;
}

// Test batch add parsing
TEST(parse_args_batch_add)
{
    const char *argv[] = {"goxel-cli", "voxel-batch-add", "--file", "voxels.csv"};
    int argc = 4;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(args.command, "voxel-batch-add");
    ASSERT_STR_EQ(args.batch_file, "voxels.csv");
    
    return 0;
}

// Test invalid arguments
TEST(parse_args_invalid)
{
    const char *argv[] = {"goxel-cli", "invalid-command"};
    int argc = 2;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT(ret != 0);
    
    return 0;
}

// Test help command
TEST(parse_args_help)
{
    const char *argv[] = {"goxel-cli", "--help"};
    int argc = 2;
    
    cli_args_t args;
    int ret = cli_parse_args(argc, (char**)argv, &args);
    
    ASSERT_EQ(ret, 1); // Help returns 1 to indicate exit
    
    return 0;
}

// Test color parsing edge cases
TEST(color_parsing)
{
    cli_args_t args;
    
    // Test RGB (3 values)
    int ret = cli_parse_color("128,64,32", args.color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(args.color[0], 128);
    ASSERT_EQ(args.color[1], 64);
    ASSERT_EQ(args.color[2], 32);
    ASSERT_EQ(args.color[3], 255); // Default alpha
    
    // Test RGBA (4 values)
    ret = cli_parse_color("255,0,255,128", args.color);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(args.color[0], 255);
    ASSERT_EQ(args.color[1], 0);
    ASSERT_EQ(args.color[2], 255);
    ASSERT_EQ(args.color[3], 128);
    
    // Test invalid format
    ret = cli_parse_color("not-a-color", args.color);
    ASSERT(ret != 0);
    
    return 0;
}

// Test position parsing
TEST(position_parsing)
{
    int pos[3];
    
    // Valid position
    int ret = cli_parse_position("10,20,30", pos);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(pos[0], 10);
    ASSERT_EQ(pos[1], 20);
    ASSERT_EQ(pos[2], 30);
    
    // Negative positions
    ret = cli_parse_position("-5,10,-15", pos);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(pos[0], -5);
    ASSERT_EQ(pos[1], 10);
    ASSERT_EQ(pos[2], -15);
    
    // Invalid format
    ret = cli_parse_position("1,2", pos);
    ASSERT(ret != 0);
    
    return 0;
}

// Test resolution parsing
TEST(resolution_parsing)
{
    int res[2];
    
    // Valid resolution
    int ret = cli_parse_resolution("1920x1080", res);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(res[0], 1920);
    ASSERT_EQ(res[1], 1080);
    
    // Small resolution
    ret = cli_parse_resolution("640x480", res);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(res[0], 640);
    ASSERT_EQ(res[1], 480);
    
    // Invalid format
    ret = cli_parse_resolution("invalid", res);
    ASSERT(ret != 0);
    
    return 0;
}

// Test command registry
TEST(command_registry)
{
    cli_command_registry_t *registry = cli_create_registry();
    ASSERT(registry != NULL);
    
    // Check that basic commands are registered
    cli_command_t *cmd = cli_find_command(registry, "create");
    ASSERT(cmd != NULL);
    ASSERT_STR_EQ(cmd->name, "create");
    
    cmd = cli_find_command(registry, "voxel-add");
    ASSERT(cmd != NULL);
    
    cmd = cli_find_command(registry, "render");
    ASSERT(cmd != NULL);
    
    // Non-existent command
    cmd = cli_find_command(registry, "non-existent");
    ASSERT(cmd == NULL);
    
    cli_destroy_registry(registry);
    return 0;
}

int main(int argc, char **argv)
{
    printf("Running Goxel CLI Interface Tests\n");
    printf("==================================\n");
    
    RUN_TEST(parse_args_create);
    RUN_TEST(parse_args_voxel_add);
    RUN_TEST(parse_args_render);
    RUN_TEST(parse_args_batch_add);
    RUN_TEST(parse_args_invalid);
    RUN_TEST(parse_args_help);
    RUN_TEST(color_parsing);
    RUN_TEST(position_parsing);
    RUN_TEST(resolution_parsing);
    RUN_TEST(command_registry);
    
    printf("\n==================================\n");
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