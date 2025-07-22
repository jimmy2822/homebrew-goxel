/* Simple Goxel Headless C API Example
 * 
 * This example demonstrates basic usage of the Goxel Headless C API:
 * - Creating and initializing a context
 * - Creating a new voxel project
 * - Adding some voxels to create a simple shape
 * - Saving the project to a file
 * - Cleaning up resources
 */

#include "../../include/goxel_headless.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("Goxel Headless C API Example\n");
    printf("============================\n\n");
    
    // Display version information
    int major, minor, patch;
    const char *version = goxel_get_version(&major, &minor, &patch);
    printf("Goxel Version: %s (%d.%d.%d)\n", version, major, minor, patch);
    
    // Check feature support
    printf("Features: ");
    if (goxel_has_feature("osmesa")) printf("OSMesa ");
    if (goxel_has_feature("scripting")) printf("Scripting ");
    if (goxel_has_feature("threading")) printf("Threading ");
    printf("\n\n");
    
    // Step 1: Create and initialize context
    printf("1. Creating context...\n");
    goxel_context_t *ctx = goxel_create_context();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create context\n");
        return 1;
    }
    
    goxel_error_t result = goxel_init_context(ctx);
    if (result != GOXEL_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize context: %s\n", 
                goxel_get_error_string(result));
        goxel_destroy_context(ctx);
        return 1;
    }
    printf("   Context created and initialized successfully.\n");
    
    // Step 2: Create a new project
    printf("2. Creating new project...\n");
    result = goxel_create_project(ctx, "Example Project", 32, 32, 32);
    if (result != GOXEL_SUCCESS) {
        fprintf(stderr, "Error: Failed to create project: %s\n", 
                goxel_get_error_string(result));
        const char *last_error = goxel_get_last_error(ctx);
        if (last_error) {
            fprintf(stderr, "   Details: %s\n", last_error);
        }
        goxel_destroy_context(ctx);
        return 1;
    }
    printf("   Project created with dimensions 32x32x32.\n");
    
    // Step 3: Add some voxels to create a simple cube
    printf("3. Adding voxels to create a 3x3x3 cube...\n");
    goxel_color_t red = {255, 0, 0, 255};    // Red
    goxel_color_t green = {0, 255, 0, 255};  // Green
    goxel_color_t blue = {0, 0, 255, 255};   // Blue
    
    int voxels_added = 0;
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            for (int z = 0; z < 3; z++) {
                goxel_color_t *color;
                // Use different colors for different layers
                if (z == 0) color = &red;
                else if (z == 1) color = &green;
                else color = &blue;
                
                result = goxel_add_voxel(ctx, x + 15, y + 15, z + 15, color);
                if (result == GOXEL_SUCCESS) {
                    voxels_added++;
                } else {
                    fprintf(stderr, "Warning: Failed to add voxel at (%d,%d,%d): %s\n", 
                            x + 15, y + 15, z + 15, goxel_get_error_string(result));
                }
            }
        }
    }
    printf("   Successfully added %d voxels.\n", voxels_added);
    
    // Step 4: Get project bounds
    printf("4. Checking project bounds...\n");
    int width, height, depth;
    result = goxel_get_project_bounds(ctx, &width, &height, &depth);
    if (result == GOXEL_SUCCESS) {
        printf("   Project bounds: %dx%dx%d\n", width, height, depth);
    } else {
        printf("   Warning: Could not get project bounds: %s\n", 
               goxel_get_error_string(result));
    }
    
    // Step 5: Save the project
    printf("5. Saving project to file...\n");
    result = goxel_save_project(ctx, "example_output.gox");
    if (result != GOXEL_SUCCESS) {
        fprintf(stderr, "Error: Failed to save project: %s\n", 
                goxel_get_error_string(result));
        const char *last_error = goxel_get_last_error(ctx);
        if (last_error) {
            fprintf(stderr, "   Details: %s\n", last_error);
        }
    } else {
        printf("   Project saved to 'example_output.gox'.\n");
    }
    
    // Step 6: Memory usage information
    printf("6. Memory usage information...\n");
    size_t bytes_used, bytes_allocated;
    result = goxel_get_memory_usage(ctx, &bytes_used, &bytes_allocated);
    if (result == GOXEL_SUCCESS) {
        printf("   Memory used: %zu bytes (%.2f KB)\n", bytes_used, bytes_used / 1024.0);
        printf("   Memory allocated: %zu bytes (%.2f KB)\n", bytes_allocated, bytes_allocated / 1024.0);
    }
    
    // Step 7: Clean up
    printf("7. Cleaning up...\n");
    goxel_destroy_context(ctx);
    printf("   Context destroyed.\n");
    
    printf("\nExample completed successfully!\n");
    printf("Check 'example_output.gox' for the created voxel project.\n");
    
    return 0;
}