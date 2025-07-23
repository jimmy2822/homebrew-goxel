/* Goxel Headless API - Implementation
 *
 * Copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
 * 
 * This file implements the public C API for the Goxel Headless library.
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

#include "../../include/goxel_headless.h"
#include "../core/goxel_core.h"
#include "../goxel.h"
#include "render_headless.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

// For PNG encoding to memory buffer
#include "stb_image_write.h"

// ============================================================================
// INTERNAL CONTEXT STRUCTURE
// ============================================================================

/**
 * Internal context structure that wraps the core context.
 */
struct goxel_context {
    goxel_core_context_t *core;      /**< Core Goxel context */
    char last_error[512];            /**< Last error message buffer */
    bool initialized;                /**< Whether context is initialized */
    pthread_mutex_t mutex;           /**< Thread safety mutex */
    size_t memory_used;              /**< Current memory usage tracking */
    size_t memory_allocated;         /**< Total memory allocated tracking */
};

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Error message strings for each error code.
 */
static const char *get_error_message(goxel_error_t error)
{
    switch (error) {
        case GOXEL_SUCCESS: return "Success";
        case GOXEL_ERROR_INVALID_CONTEXT: return "Invalid or NULL context provided";
        case GOXEL_ERROR_INVALID_PARAMETER: return "Invalid parameter value";
        case GOXEL_ERROR_FILE_NOT_FOUND: return "File does not exist";
        case GOXEL_ERROR_FILE_ACCESS: return "Cannot read/write file";
        case GOXEL_ERROR_UNSUPPORTED_FORMAT: return "File format not supported";
        case GOXEL_ERROR_OUT_OF_MEMORY: return "Memory allocation failed";
        case GOXEL_ERROR_INVALID_OPERATION: return "Operation not valid in current state";
        case GOXEL_ERROR_LAYER_NOT_FOUND: return "Specified layer does not exist";
        case GOXEL_ERROR_RENDER_FAILED: return "Rendering operation failed";
        case GOXEL_ERROR_SCRIPT_FAILED: return "Script execution failed";
        case GOXEL_ERROR_INIT_FAILED: return "Context initialization failed";
        case GOXEL_ERROR_UNKNOWN: return "Unknown or unspecified error";
        default: return "Unknown error";
    }
}

/**
 * Sets the last error message for a context.
 */
static void set_last_error(goxel_context_t *ctx, const char *format, ...)
{
    if (ctx) {
        va_list args;
        va_start(args, format);
        vsnprintf(ctx->last_error, sizeof(ctx->last_error), format, args);
        va_end(args);
    }
}

/**
 * Validates that a context pointer is valid and initialized.
 */
static bool validate_context(goxel_context_t *ctx)
{
    return ctx != NULL && ctx->initialized && ctx->core != NULL;
}

// ============================================================================
// CONTEXT MANAGEMENT IMPLEMENTATION
// ============================================================================

goxel_context_t *goxel_create_context(void)
{
    goxel_context_t *ctx = calloc(1, sizeof(goxel_context_t));
    if (!ctx) {
        return NULL;
    }
    
    // Initialize mutex for thread safety
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return NULL;
    }
    
    // Create core context
    ctx->core = goxel_core_create_context();
    if (!ctx->core) {
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return NULL;
    }
    
    ctx->initialized = false;
    ctx->memory_used = sizeof(goxel_context_t);
    ctx->memory_allocated = sizeof(goxel_context_t);
    
    return ctx;
}

goxel_error_t goxel_init_context(goxel_context_t *ctx)
{
    if (!ctx) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (ctx->initialized) {
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_SUCCESS;
    }
    
    // Initialize core context
    int result = goxel_core_init(ctx->core);
    if (result != 0) {
        set_last_error(ctx, "Failed to initialize core context: %d", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INIT_FAILED;
    }
    
    ctx->initialized = true;
    pthread_mutex_unlock(&ctx->mutex);
    
    return GOXEL_SUCCESS;
}

void goxel_destroy_context(goxel_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    
    if (ctx->initialized) {
        goxel_core_shutdown(ctx->core);
    }
    
    if (ctx->core) {
        goxel_core_destroy_context(ctx->core);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
}

// ============================================================================
// PROJECT MANAGEMENT IMPLEMENTATION
// ============================================================================

goxel_error_t goxel_create_project(goxel_context_t *ctx, const char *name, 
                                   int width, int height, int depth)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (width <= 0 || height <= 0 || depth <= 0) {
        set_last_error(ctx, "Invalid project dimensions: %dx%dx%d", 
                      width, height, depth);
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_create_project(ctx->core, name, width, height, depth);
    if (result != 0) {
        set_last_error(ctx, "Failed to create project: %d", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_load_project(goxel_context_t *ctx, const char *path)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!path) {
        set_last_error(ctx, "Project path cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_load_project(ctx->core, path);
    if (result != 0) {
        set_last_error(ctx, "Failed to load project from '%s': %d", path, result);
        pthread_mutex_unlock(&ctx->mutex);
        
        // Try to determine more specific error type
        if (access(path, F_OK) != 0) {
            return GOXEL_ERROR_FILE_NOT_FOUND;
        } else if (access(path, R_OK) != 0) {
            return GOXEL_ERROR_FILE_ACCESS;
        } else {
            return GOXEL_ERROR_UNSUPPORTED_FORMAT;
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_save_project(goxel_context_t *ctx, const char *path)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!path) {
        set_last_error(ctx, "Save path cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_save_project(ctx->core, path);
    if (result != 0) {
        set_last_error(ctx, "Failed to save project to '%s': %d", path, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_FILE_ACCESS;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_save_project_format(goxel_context_t *ctx, const char *path, 
                                        const char *format)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!path || !format) {
        set_last_error(ctx, "Save path and format cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_save_project_format(ctx->core, path, format);
    if (result != 0) {
        set_last_error(ctx, "Failed to save project to '%s' in format '%s': %d", 
                      path, format, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_UNSUPPORTED_FORMAT;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_close_project(goxel_context_t *ctx)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    goxel_core_reset(ctx->core);
    pthread_mutex_unlock(&ctx->mutex);
    
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_get_project_bounds(goxel_context_t *ctx, int *width, 
                                       int *height, int *depth)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!width || !height || !depth) {
        set_last_error(ctx, "Output parameters cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_get_project_bounds(ctx->core, width, height, depth);
    if (result != 0) {
        set_last_error(ctx, "Failed to get project bounds: %d", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

// ============================================================================
// VOXEL OPERATIONS IMPLEMENTATION
// ============================================================================

goxel_error_t goxel_add_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              const goxel_color_t *color)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!color) {
        set_last_error(ctx, "Color cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    uint8_t rgba[4] = {color->r, color->g, color->b, color->a};
    int result = goxel_core_add_voxel(ctx->core, x, y, z, rgba, -1);
    if (result != 0) {
        set_last_error(ctx, "Failed to add voxel at (%d,%d,%d): %d", x, y, z, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_remove_voxel(goxel_context_t *ctx, int x, int y, int z)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_remove_voxel(ctx->core, x, y, z, -1);
    if (result != 0) {
        set_last_error(ctx, "Failed to remove voxel at (%d,%d,%d): %d", x, y, z, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_get_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              goxel_color_t *color)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!color) {
        set_last_error(ctx, "Color output parameter cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    uint8_t rgba[4];
    int result = goxel_core_get_voxel(ctx->core, x, y, z, rgba);
    if (result != 0) {
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_PARAMETER; // No voxel at position
    }
    
    color->r = rgba[0];
    color->g = rgba[1];
    color->b = rgba[2];
    color->a = rgba[3];
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_add_voxel_batch(goxel_context_t *ctx, 
                                    const goxel_pos_t *positions,
                                    const goxel_color_t *colors,
                                    size_t count)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!positions || !colors || count == 0) {
        set_last_error(ctx, "Invalid batch parameters");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    for (size_t i = 0; i < count; i++) {
        uint8_t rgba[4] = {colors[i].r, colors[i].g, colors[i].b, colors[i].a};
        int result = goxel_core_add_voxel(ctx->core, positions[i].x, positions[i].y, 
                                         positions[i].z, rgba, -1);
        if (result != 0) {
            set_last_error(ctx, "Failed to add voxel %zu at (%d,%d,%d): %d", 
                          i, positions[i].x, positions[i].y, positions[i].z, result);
            pthread_mutex_unlock(&ctx->mutex);
            return GOXEL_ERROR_INVALID_OPERATION;
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_remove_voxels_in_box(goxel_context_t *ctx, 
                                         const goxel_box_t *box)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!box) {
        set_last_error(ctx, "Box parameter cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_remove_voxels_in_box(ctx->core, 
                                                box->min.x, box->min.y, box->min.z,
                                                box->max.x, box->max.y, box->max.z, -1);
    if (result != 0) {
        set_last_error(ctx, "Failed to remove voxels in box: %d", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_paint_voxel(goxel_context_t *ctx, int x, int y, int z, 
                                const goxel_color_t *color)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!color) {
        set_last_error(ctx, "Color cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    uint8_t rgba[4] = {color->r, color->g, color->b, color->a};
    int result = goxel_core_paint_voxel(ctx->core, x, y, z, rgba, -1);
    if (result != 0) {
        set_last_error(ctx, "Failed to paint voxel at (%d,%d,%d): %d", x, y, z, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

// ============================================================================
// LAYER MANAGEMENT IMPLEMENTATION
// ============================================================================

goxel_error_t goxel_create_layer(goxel_context_t *ctx, const char *name,
                                 const goxel_color_t *color, bool visible,
                                 goxel_layer_id_t *layer_id)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!layer_id) {
        set_last_error(ctx, "Layer ID output parameter cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    uint8_t rgba[4] = {255, 255, 255, 255}; // Default white
    if (color) {
        rgba[0] = color->r;
        rgba[1] = color->g;
        rgba[2] = color->b;
        rgba[3] = color->a;
    }
    
    int result = goxel_core_create_layer(ctx->core, name, rgba, visible ? 1 : 0);
    if (result < 0) {
        set_last_error(ctx, "Failed to create layer '%s': %d", name ? name : "(unnamed)", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_INVALID_OPERATION;
    }
    
    *layer_id = result;
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_delete_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_delete_layer(ctx->core, layer_id, NULL);
    if (result != 0) {
        set_last_error(ctx, "Failed to delete layer %d: %d", layer_id, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_LAYER_NOT_FOUND;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_set_active_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_set_active_layer(ctx->core, layer_id);
    if (result != 0) {
        set_last_error(ctx, "Failed to set active layer %d: %d", layer_id, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_LAYER_NOT_FOUND;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_get_active_layer(goxel_context_t *ctx, goxel_layer_id_t *layer_id)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!layer_id) {
        set_last_error(ctx, "Layer ID output parameter cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    // Get current active layer from context
    if (ctx->core->image && ctx->core->image->active_layer) {
        // This would need to be implemented in core to return layer ID
        *layer_id = 0; // Placeholder - would need core API extension
    } else {
        *layer_id = -1;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_set_layer_visibility(goxel_context_t *ctx, 
                                         goxel_layer_id_t layer_id, bool visible)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int result = goxel_core_set_layer_visibility(ctx->core, layer_id, NULL, visible ? 1 : 0);
    if (result != 0) {
        set_last_error(ctx, "Failed to set layer %d visibility to %s: %d", 
                      layer_id, visible ? "visible" : "hidden", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_LAYER_NOT_FOUND;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_get_layer_count(goxel_context_t *ctx, int *count)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!count) {
        set_last_error(ctx, "Count output parameter cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    *count = goxel_core_get_layer_count(ctx->core);
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

// ============================================================================
// RENDERING IMPLEMENTATION
// ============================================================================

/**
 * Converts public camera preset to internal string representation.
 */
static const char *camera_preset_to_string(goxel_camera_preset_t preset)
{
    switch (preset) {
        case GOXEL_CAMERA_FRONT: return "front";
        case GOXEL_CAMERA_BACK: return "back";
        case GOXEL_CAMERA_LEFT: return "left";
        case GOXEL_CAMERA_RIGHT: return "right";
        case GOXEL_CAMERA_TOP: return "top";
        case GOXEL_CAMERA_BOTTOM: return "bottom";
        case GOXEL_CAMERA_ISOMETRIC: return "isometric";
        default: return "front";
    }
}

/**
 * Converts public render format to internal string representation.
 */
static const char *render_format_to_string(goxel_render_format_t format)
{
    switch (format) {
        case GOXEL_FORMAT_PNG: return "png";
        case GOXEL_FORMAT_JPEG: return "jpeg";
        case GOXEL_FORMAT_BMP: return "bmp";
        default: return "png";
    }
}

goxel_error_t goxel_render_to_file(goxel_context_t *ctx, const char *output_path,
                                   const goxel_render_options_t *options)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!output_path || !options) {
        set_last_error(ctx, "Output path and options cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    if (options->width <= 0 || options->height <= 0) {
        set_last_error(ctx, "Invalid render dimensions: %dx%d", 
                      options->width, options->height);
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    const char *camera_str = camera_preset_to_string(options->camera);
    const char *format_str = render_format_to_string(options->format);
    
    int result = goxel_core_render_to_file(ctx->core, output_path, 
                                          options->width, options->height,
                                          format_str, options->quality, camera_str);
    if (result != 0) {
        set_last_error(ctx, "Failed to render to file '%s': %d", output_path, result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_RENDER_FAILED;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

goxel_error_t goxel_render_to_buffer(goxel_context_t *ctx, uint8_t **buffer,
                                     size_t *buffer_size,
                                     const goxel_render_options_t *options)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!buffer || !buffer_size || !options) {
        set_last_error(ctx, "Buffer, buffer_size, and options cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    if (options->width <= 0 || options->height <= 0) {
        set_last_error(ctx, "Invalid render dimensions: %dx%d", 
                      options->width, options->height);
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    // Direct buffer rendering - no temp files needed
    pthread_mutex_lock(&ctx->mutex);
    
    // Initialize headless rendering context for requested size
    if (headless_render_init(options->width, options->height) != 0) {
        set_last_error(ctx, "Failed to initialize headless rendering");
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_RENDER_FAILED;
    }
    
    // Convert camera preset to string
    const char *camera_str = camera_preset_to_string(options->camera);
    const char *format_str = render_format_to_string(options->format);
    
    // Use the new core render-to-buffer function (eliminates temp file workaround)
    int result = goxel_core_render_to_buffer(ctx->core, options->width, options->height,
                                           camera_str, (void**)buffer, buffer_size, format_str);
    if (result != 0) {
        set_last_error(ctx, "Failed to render to buffer: %d", result);
        pthread_mutex_unlock(&ctx->mutex);
        return GOXEL_ERROR_RENDER_FAILED;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return GOXEL_SUCCESS;
}

// ============================================================================
// ERROR HANDLING IMPLEMENTATION
// ============================================================================

const char *goxel_get_error_string(goxel_error_t error)
{
    return get_error_message(error);
}

const char *goxel_get_last_error(goxel_context_t *ctx)
{
    if (!ctx) {
        return NULL;
    }
    
    if (ctx->last_error[0] == '\0') {
        return NULL;
    }
    
    return ctx->last_error;
}

// ============================================================================
// MEMORY MANAGEMENT IMPLEMENTATION
// ============================================================================

goxel_error_t goxel_get_memory_usage(goxel_context_t *ctx, size_t *bytes_used,
                                     size_t *bytes_allocated)
{
    if (!validate_context(ctx)) {
        return GOXEL_ERROR_INVALID_CONTEXT;
    }
    
    if (!bytes_used || !bytes_allocated) {
        set_last_error(ctx, "Output parameters cannot be NULL");
        return GOXEL_ERROR_INVALID_PARAMETER;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    *bytes_used = ctx->memory_used;
    *bytes_allocated = ctx->memory_allocated;
    pthread_mutex_unlock(&ctx->mutex);
    
    return GOXEL_SUCCESS;
}

// ============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// ============================================================================

const char *goxel_get_version(int *major, int *minor, int *patch)
{
    if (major) *major = GOXEL_VERSION_MAJOR;
    if (minor) *minor = GOXEL_VERSION_MINOR;
    if (patch) *patch = GOXEL_VERSION_PATCH;
    return GOXEL_VERSION_STRING;
}

bool goxel_has_feature(const char *feature)
{
    if (!feature) {
        return false;
    }
    
    if (strcmp(feature, "osmesa") == 0) {
        return true;  // OSMesa is always supported in headless mode
    } else if (strcmp(feature, "scripting") == 0) {
        return true;  // QuickJS scripting support
    } else if (strcmp(feature, "threading") == 0) {
        return true;  // Thread safety support
    }
    
    return false;
}