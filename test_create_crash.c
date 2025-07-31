#include <stdio.h>
#include <stdlib.h>
#include "../src/core/goxel_core.h"
#include "../src/goxel.h"
#include "../src/log.h"

// External global goxel instance
extern goxel_t goxel;

int main(void)
{
    printf("Testing multiple create_project calls...\n");
    
    // Create context
    goxel_core_context_t *ctx = goxel_core_create_context();
    if (!ctx) {
        printf("Failed to create context\n");
        return 1;
    }
    
    // Initialize context
    int ret = goxel_core_init(ctx);
    if (ret != 0) {
        printf("Failed to init context: %d\n", ret);
        return 1;
    }
    
    printf("\n=== First create_project call ===\n");
    printf("Before: ctx->image=%p, goxel.image=%p\n", ctx->image, goxel.image);
    
    ret = goxel_core_create_project(ctx, "Project 1", 64, 64, 64);
    if (ret != 0) {
        printf("First create failed: %d\n", ret);
        return 1;
    }
    
    printf("After: ctx->image=%p, goxel.image=%p\n", ctx->image, goxel.image);
    printf("First create succeeded\n");
    
    printf("\n=== Second create_project call ===\n");
    printf("Before: ctx->image=%p, goxel.image=%p\n", ctx->image, goxel.image);
    
    ret = goxel_core_create_project(ctx, "Project 2", 64, 64, 64);
    if (ret != 0) {
        printf("Second create failed: %d\n", ret);
        return 1;
    }
    
    printf("After: ctx->image=%p, goxel.image=%p\n", ctx->image, goxel.image);
    printf("Second create succeeded\n");
    
    // Cleanup
    goxel_core_shutdown(ctx);
    goxel_core_destroy_context(ctx);
    
    printf("\nTest completed successfully!\n");
    return 0;
}