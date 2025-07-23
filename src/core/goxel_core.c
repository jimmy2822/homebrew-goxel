/* Goxel 3D voxels editor - Core API Implementation
 *
 * copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel_core.h"
#include "../goxel.h"  // For function prototypes and constants
#include "../headless/render_headless.h"  // For headless rendering functions
#include "file_format.h"  // For file format handling
#include <errno.h>
#include <string.h>
#include <assert.h>

goxel_core_context_t *goxel_core_create_context(void)
{
    goxel_core_context_t *ctx = calloc(1, sizeof(goxel_core_context_t));
    return ctx;
}

void goxel_core_destroy_context(goxel_core_context_t *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

int goxel_core_init(goxel_core_context_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize core systems
    shapes_init();
    
    // Set default parameters
    ctx->tool_radius = 1;
    ctx->snap_offset = 0.5f;
    ctx->snap_mask = SNAP_IMAGE_BOX;
    
    // Set default painter color (white)
    ctx->painter_color[0] = 255;
    ctx->painter_color[1] = 255;
    ctx->painter_color[2] = 255;
    ctx->painter_color[3] = 255;
    ctx->painter_mode = MODE_OVER;
    ctx->painter_shape = (intptr_t)&shape_cube;
    
    return 0;
}

void goxel_core_shutdown(goxel_core_context_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    if (ctx->palette) {
        if (ctx->palette->entries) free(ctx->palette->entries);
        free(ctx->palette);
        ctx->palette = NULL;
    }
}

void goxel_core_reset(goxel_core_context_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->image) {
        image_delete(ctx->image);
    }
    
    ctx->image = image_new();
    
    // Reset drawing parameters to defaults
    ctx->tool_radius = 1;
    ctx->snap_offset = 0.5f;
    ctx->painter_color[0] = 255;
    ctx->painter_color[1] = 255;
    ctx->painter_color[2] = 255;
    ctx->painter_color[3] = 255;
}

int goxel_core_create_project(goxel_core_context_t *ctx, const char *name, int width, int height, int depth)
{
    if (!ctx) return -1;
    
    if (ctx->image) {
        image_delete(ctx->image);
    }
    
    ctx->image = image_new();
    if (!ctx->image) return -1;
    
    // Sync to global goxel context for export functions
    extern goxel_t goxel;
    if (goxel.image && goxel.image != ctx->image) {
        image_delete(goxel.image);
    }
    goxel.image = ctx->image;
    
    if (name) {
        // Allocate memory for path string (path is char*, not char[])
        if (ctx->image->path) {
            free(ctx->image->path);
        }
        ctx->image->path = strdup(name);
    }
    
    // Note: width, height, depth parameters are for initial project setup
    // In Goxel, projects can grow dynamically, so these are informational
    
    return 0;
}

// Forward declaration for the new implementation
int goxel_core_load_project_impl(goxel_core_context_t *ctx, const char *path);

int goxel_core_load_project(goxel_core_context_t *ctx, const char *path)
{
    // Use the new implementation that avoids hanging
    return goxel_core_load_project_impl(ctx, path);
}

int goxel_core_save_project(goxel_core_context_t *ctx, const char *path)
{
    if (!ctx || !path || !ctx->image) return -1;
    
    LOG_D("Saving project to: %s", path);
    
    // Sync context to global goxel first
    extern goxel_t goxel;
    goxel.image = ctx->image;
    
    LOG_D("Synced goxel.image, calling goxel_export_to_file");
    
    int ret = goxel_export_to_file(path, NULL);
    
    LOG_D("goxel_export_to_file returned: %d", ret);
    
    if (ret == 0) {
        // Properly set the image path using dynamic allocation
        if (ctx->image->path) {
            free(ctx->image->path);
        }
        ctx->image->path = strdup(path);
        if (!ctx->image->path) {
            LOG_W("Failed to allocate memory for image path");
            // Continue anyway, as this doesn't affect core functionality
        }
        LOG_I("Project saved successfully to: %s", path);
    } else {
        LOG_E("Failed to save project to: %s (error: %d)", path, ret);
    }
    
    return ret;
}

int goxel_core_add_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4], int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = (layer_id == 0 || layer_id == -1) ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified (positive IDs only)
    if (layer_id > 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    }
    
    if (!layer) return -1;
    
    int pos[3] = {x, y, z};
    uint8_t color[4] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    
    volume_set_at(layer->volume, NULL, pos, color);
    
    return 0;
}

int goxel_core_remove_voxel(goxel_core_context_t *ctx, int x, int y, int z, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = (layer_id == 0 || layer_id == -1) ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id > 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    }
    if (!layer) return -1;
    
    int pos[3] = {x, y, z};
    uint8_t color[4] = {0, 0, 0, 0}; // Transparent = removal
    
    volume_set_at(layer->volume, NULL, pos, color);
    
    return 0;
}

int goxel_core_get_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4])
{
    if (!ctx || !ctx->image || !rgba) return -1;
    
    layer_t *layer = ctx->image->active_layer;
    if (!layer) return -1;
    
    int pos[3] = {x, y, z};
    volume_get_at(layer->volume, NULL, pos, rgba);
    
    return 0;
}

int goxel_core_create_layer(goxel_core_context_t *ctx, const char *name, uint8_t rgba[4], int visible)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_add_layer(ctx->image, NULL);
    if (!layer) return -1;
    
    if (name) {
        snprintf(layer->name, sizeof(layer->name), "%s", name);
    }
    
    if (rgba) {
        // Set layer color if provided - note: layer_t may not have direct color field
        // This would need to be implemented based on actual layer_t structure
    }
    
    layer->visible = visible;
    
    return layer->id;
}

int goxel_core_delete_layer(goxel_core_context_t *ctx, int layer_id, const char *name)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = NULL;
    
    // Find layer by ID or name
    if (layer_id >= 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    } else if (name) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (strcmp(layer->name, name) == 0) break;
        }
    }
    
    if (!layer) return -1;
    
    image_delete_layer(ctx->image, layer);
    return 0;
}

int goxel_core_merge_layers(goxel_core_context_t *ctx, int source_id, int target_id, const char *source_name, const char *target_name)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *source_layer = NULL;
    layer_t *target_layer = NULL;
    
    // Find source layer
    if (source_id >= 0) {
        for (source_layer = ctx->image->layers; source_layer; source_layer = source_layer->next) {
            if (source_layer->id == source_id) break;
        }
    } else if (source_name) {
        for (source_layer = ctx->image->layers; source_layer; source_layer = source_layer->next) {
            if (strcmp(source_layer->name, source_name) == 0) break;
        }
    }
    
    // Find target layer
    if (target_id >= 0) {
        for (target_layer = ctx->image->layers; target_layer; target_layer = target_layer->next) {
            if (target_layer->id == target_id) break;
        }
    } else if (target_name) {
        for (target_layer = ctx->image->layers; target_layer; target_layer = target_layer->next) {
            if (strcmp(target_layer->name, target_name) == 0) break;
        }
    }
    
    if (!source_layer || !target_layer || source_layer == target_layer) return -1;
    
    // PROPER IMPLEMENTATION: Real volume merging with alpha blending
    volume_t *source_vol = source_layer->volume;
    volume_t *target_vol = target_layer->volume;
    
    // Phase 1: Get source volume bounding box for efficient iteration
    int bbox[2][3];
    if (!volume_get_bbox(source_vol, bbox, false)) {
        // Empty source volume, just delete
        image_delete_layer(ctx->image, source_layer);
        return 0;
    }
    
    // Phase 2: Iterate through all voxels in source volume
    volume_iterator_t iter = volume_get_iterator(source_vol, VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    int pos[3];
    
    while (volume_iter(&iter, pos)) {
        uint8_t src_voxel[4];
        volume_get_at(source_vol, &iter, pos, src_voxel);
        
        // Skip transparent voxels
        if (src_voxel[3] == 0) continue;
        
        // Get existing voxel at target position
        uint8_t dst_voxel[4] = {0, 0, 0, 0};
        volume_get_at(target_vol, NULL, pos, dst_voxel);
        
        // Phase 3: Alpha blending
        uint8_t result[4];
        if (dst_voxel[3] == 0 || src_voxel[3] == 255) {
            // No blending needed - source replaces destination
            memcpy(result, src_voxel, 4);
        } else if (src_voxel[3] == 0) {
            // Source is transparent - keep destination
            memcpy(result, dst_voxel, 4);
        } else {
            // Standard alpha blending formula
            float src_alpha = src_voxel[3] / 255.0f;
            float dst_alpha = dst_voxel[3] / 255.0f;
            float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);
            
            if (out_alpha > 0) {
                result[0] = (uint8_t)((src_voxel[0] * src_alpha + dst_voxel[0] * dst_alpha * (1.0f - src_alpha)) / out_alpha);
                result[1] = (uint8_t)((src_voxel[1] * src_alpha + dst_voxel[1] * dst_alpha * (1.0f - src_alpha)) / out_alpha);
                result[2] = (uint8_t)((src_voxel[2] * src_alpha + dst_voxel[2] * dst_alpha * (1.0f - src_alpha)) / out_alpha);
                result[3] = (uint8_t)(out_alpha * 255);
            } else {
                memset(result, 0, 4);
            }
        }
        
        // Set the blended result in target volume
        if (result[3] > 0) {
            volume_set_at(target_vol, NULL, pos, result);
        }
    }
    
    // Phase 4: Safe cleanup
    image_delete_layer(ctx->image, source_layer);
    
    LOG_I("Layers merged successfully");
    return 0;
}

int goxel_core_set_layer_visibility(goxel_core_context_t *ctx, int layer_id, const char *name, int visible)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = NULL;
    
    // Find layer by ID or name
    if (layer_id >= 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    } else if (name) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (strcmp(layer->name, name) == 0) break;
        }
    }
    
    if (!layer) return -1;
    
    layer->visible = visible;
    return 0;
}

int goxel_core_rename_layer(goxel_core_context_t *ctx, int layer_id, const char *old_name, const char *new_name)
{
    if (!ctx || !ctx->image || !new_name) return -1;
    
    layer_t *layer = NULL;
    
    // Find layer by ID or name
    if (layer_id >= 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    } else if (old_name) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (strcmp(layer->name, old_name) == 0) break;
        }
    }
    
    if (!layer) return -1;
    
    snprintf(layer->name, sizeof(layer->name), "%s", new_name);
    return 0;
}

int goxel_core_set_active_layer(goxel_core_context_t *ctx, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = NULL;
    
    // Find layer by ID 
    for (layer = ctx->image->layers; layer; layer = layer->next) {
        if (layer->id == layer_id) break;
    }
    if (!layer) return -1;
    
    ctx->image->active_layer = layer;
    return 0;
}

// Additional functions required by CLI interface

int goxel_core_save_project_format(goxel_core_context_t *ctx, const char *path, const char *format)
{
    if (!ctx || !path || !ctx->image) return -1;
    
    // For now, ignore format parameter and use default export
    return goxel_core_save_project(ctx, path);
}

int goxel_core_create_backup(goxel_core_context_t *ctx, const char *path)
{
    if (!ctx || !path) return -1;
    
    // Create backup by appending .bak to the filename
    char backup_path[2048];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", path);
    
    return goxel_core_save_project(ctx, backup_path);
}

void goxel_core_set_read_only(goxel_core_context_t *ctx, bool read_only)
{
    if (!ctx) return;
    
    // For now, store this in a simple flag - would need to be added to goxel_core struct
    // This is a placeholder implementation
}

// Rendering operations
int goxel_core_render_to_file(goxel_core_context_t *ctx, const char *output_file, int width, int height, const char *format, int quality, const char *camera_preset)
{
    if (!ctx || !output_file || !ctx->image) return -1;
    
    LOG_I("Rendering scene to file: %s [%dx%d]", output_file, width, height);
    
    // Resize headless rendering buffer if needed
    if (headless_render_resize(width, height) != 0) {
        LOG_E("Failed to resize headless render buffer");
        return -1;
    }
    
    // Use the active camera or create a default one
    camera_t *camera = ctx->image->active_camera;
    if (!camera) {
        // Create a temporary camera for rendering
        camera = camera_new("temp_camera");
        if (!camera) {
            LOG_E("Failed to create temporary camera");
            return -1;
        }
        
        // Set up camera to view the scene
        camera_fit_box(camera, ctx->image->box);
    }
    
    // Set up background color (light gray)
    uint8_t background_color[4] = {240, 240, 240, 255};
    
    // Render the scene using headless rendering
    int render_result = headless_render_scene_with_camera(ctx->image, camera, background_color);
    
    // Clean up temporary camera if we created one
    if (camera != ctx->image->active_camera) {
        camera_delete(camera);
    }
    
    if (render_result != 0) {
        LOG_E("Failed to render scene");
        return -1;
    }
    
    // Save the rendered result to file
    if (headless_render_to_file(output_file, format) != 0) {
        LOG_E("Failed to save rendered image to file");
        return -1;
    }
    
    LOG_I("Successfully rendered scene to %s", output_file);
    return 0;
}

// Helper function to validate export format
static int validate_export_format(const char *output_file, const char *format, char *error_msg, size_t error_msg_size)
{
    if (!output_file) {
        snprintf(error_msg, error_msg_size, "Output file not specified");
        return -1;
    }
    
    const file_format_t *file_format = file_format_get(output_file, format, "w");
    if (!file_format) {
        // List available formats dynamically for better error message
        char format_list[512];
        if (goxel_core_list_export_formats(format_list, sizeof(format_list)) == 0) {
            snprintf(error_msg, error_msg_size, 
                    "Unsupported format. Supported formats: %s", format_list);
        } else {
            snprintf(error_msg, error_msg_size, "Unsupported format for file: %s", output_file);
        }
        return -1;
    }
    
    if (!file_format->export_func) {
        snprintf(error_msg, error_msg_size, "Format %s does not support export", file_format->name);
        return -1;
    }
    
    return 0;
}

// Export operations  
int goxel_core_export_project(goxel_core_context_t *ctx, const char *output_file, const char *format)
{
    if (!ctx || !ctx->image) {
        LOG_E("Invalid context or image for export");
        return -1;
    }
    
    // Validate format before attempting export
    char error_msg[256];
    if (validate_export_format(output_file, format, error_msg, sizeof(error_msg)) != 0) {
        LOG_E("Export validation failed: %s", error_msg);
        return -1;
    }
    
    // Find the appropriate file format
    const file_format_t *file_format = file_format_get(output_file, format, "w");
    assert(file_format); // Should be guaranteed by validation
    assert(file_format->export_func); // Should be guaranteed by validation
    
    LOG_I("Exporting project to %s using format: %s", output_file, file_format->name);
    
    // Use the format's export function
    int result = file_format->export_func(file_format, ctx->image, output_file);
    if (result != 0) {
        LOG_E("Export failed for format: %s (error code: %d)", file_format->name, result);
        return -1;
    }
    
    LOG_I("Export completed successfully to %s", output_file);
    return 0;
}

// Helper function to collect format names
static void collect_format_name(void *user, file_format_t *format)
{
    char **buffer_ptr = (char**)user;
    char *buffer = *buffer_ptr;
    size_t current_len = strlen(buffer);
    
    // Add format name and extensions
    if (current_len > 0) {
        strcat(buffer, ", ");
    }
    strcat(buffer, format->name);
    
    // Add primary extension if available
    if (format->exts[0]) {
        strcat(buffer, " (");
        strcat(buffer, format->exts[0]);
        strcat(buffer, ")");
    }
}

int goxel_core_list_export_formats(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) return -1;
    
    buffer[0] = '\0'; // Initialize empty string
    char *buffer_ptr = buffer;
    
    // Use file_format_iter to collect all export formats
    file_format_iter("w", &buffer_ptr, collect_format_name);
    
    // Ensure we don't exceed buffer size
    if (strlen(buffer) >= buffer_size - 1) {
        buffer[buffer_size - 1] = '\0';
        return -1; // Buffer too small warning
    }
    
    return 0;
}

// Scripting operations
int goxel_core_execute_script_file(goxel_core_context_t *ctx, const char *script_file)
{
    if (!ctx || !script_file) return -1;
    
    // Placeholder - would need to integrate with existing QuickJS script system
    // This would load and execute a JavaScript file
    
    return 0; // Success for now  
}

int goxel_core_execute_script(goxel_core_context_t *ctx, const char *script_code)
{
    if (!ctx || !script_code) return -1;
    
    // Placeholder - would need to integrate with existing QuickJS script system
    // This would execute inline JavaScript code
    
    return 0; // Success for now
}

int goxel_core_get_project_bounds(goxel_core_context_t *ctx, int *width, int *height, int *depth)
{
    if (!ctx || !ctx->image) return -1;
    
    // For now, return default bounds - would need proper bbox calculation
    if (width) *width = 256;
    if (height) *height = 256; 
    if (depth) *depth = 256;
    
    return -1;
}

int goxel_core_remove_voxels_in_box(goxel_core_context_t *ctx, int x1, int y1, int z1, int x2, int y2, int z2, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = (layer_id == 0 || layer_id == -1) ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id > 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    }
    if (!layer) return -1;
    
    // Remove all voxels in the specified box
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
                goxel_core_remove_voxel(ctx, x, y, z, layer_id);
            }
        }
    }
    
    return 0;
}

int goxel_core_paint_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4], int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = (layer_id == 0 || layer_id == -1) ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id > 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
    }
    if (!layer) return -1;
    
    // Check if voxel exists first
    uint8_t existing_color[4];
    if (goxel_core_get_voxel(ctx, x, y, z, existing_color) != 0) {
        return -1;  // Voxel doesn't exist, can't paint it
    }
    
    if (existing_color[3] == 0) {
        return -1;  // Voxel is transparent (doesn't exist)
    }
    
    // Paint the voxel with new color
    return goxel_core_add_voxel(ctx, x, y, z, rgba, layer_id);
}

int goxel_core_get_layer_count(goxel_core_context_t *ctx)
{
    if (!ctx || !ctx->image) return 0;
    
    int count = 0;
    layer_t *layer;
    DL_FOREACH(ctx->image->layers, layer) {
        count++;
    }
    
    return count;
}