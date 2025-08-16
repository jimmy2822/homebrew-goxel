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
#include "../daemon_render/render_daemon.h"  // For daemon rendering functions
#include "../daemon_render/camera_daemon.h"  // For camera preset functions
#include "file_format.h"  // For file format handling
#include "../script.h"  // For script execution functions
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>  // For close() and unlink()
#include <stdlib.h>  // For mkstemp()
#include <math.h>    // For M_PI
#include <stdbool.h> // For bool type

// Helper function to check read-only mode
static int check_read_only(goxel_core_context_t *ctx, const char *operation)
{
    if (ctx && ctx->read_only) {
        LOG_E("Operation '%s' denied - context is in read-only mode", operation);
        return -1;
    }
    return 0;
}

goxel_core_context_t *goxel_core_create_context(void)
{
    goxel_core_context_t *ctx = calloc(1, sizeof(goxel_core_context_t));
    if (ctx) {
        ctx->read_only = false;  // Default to writable mode
    }
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
    if (check_read_only(ctx, "create project") != 0) return -1;
    
    // Clean up existing image if present
    if (ctx->image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    // Also clean up global goxel.image if it exists and create a new one
    // This prevents double-free issues when context and global share references
    extern goxel_t goxel;
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Create new image for context
    ctx->image = image_new();
    if (!ctx->image) return -1;
    
    // Create new image for global goxel to maintain consistency
    goxel.image = image_new();
    if (!goxel.image) {
        image_delete(ctx->image);
        ctx->image = NULL;
        return -1;
    }
    
    // Set project name if provided
    if (name) {
        // Set name on context image
        if (ctx->image->path) {
            free(ctx->image->path);
        }
        ctx->image->path = strdup(name);
        
        // Also set on global image for consistency
        if (goxel.image->path) {
            free(goxel.image->path);
        }
        goxel.image->path = strdup(name);
    }
    
    // Note: width, height, depth parameters are for initial project setup
    // In Goxel, projects can grow dynamically, so these are informational
    
    // IMPORTANT: Context and global images are kept separate to avoid conflicts
    // Operations should work on the appropriate image based on context
    
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
    if (check_read_only(ctx, "save project") != 0) return -1;
    
    LOG_D("Saving project to: %s", path);
    
    // Temporarily sync context to global goxel for export operation
    extern goxel_t goxel;
    image_t *original_image = goxel.image;
    goxel.image = ctx->image;
    
    LOG_D("Temporarily synced goxel.image, calling goxel_export_to_file");
    
    int ret = goxel_export_to_file(path, NULL);
    
    LOG_D("goxel_export_to_file returned: %d", ret);
    
    // Restore original global state
    goxel.image = original_image;
    
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
    
    layer_t *layer = NULL;
    
    // Find layer by ID if specified (positive IDs only)
    if (layer_id > 0) {
        for (layer = ctx->image->layers; layer; layer = layer->next) {
            if (layer->id == layer_id) break;
        }
        // If specified layer not found, log warning and use active layer
        if (!layer) {
            LOG_W("Layer with ID %d not found, using active layer", layer_id);
            layer = ctx->image->active_layer;
        }
    } else {
        // layer_id <= 0 means use active layer
        layer = ctx->image->active_layer;
    }
    
    // If still no layer (no active layer), create or use first layer
    if (!layer) {
        if (ctx->image->layers) {
            layer = ctx->image->layers;
            LOG_W("No active layer, using first layer (ID: %d)", layer->id);
        } else {
            LOG_E("No layers available in the image");
            return -1;
        }
    }
    
    // Ensure layer has a material
    if (!layer->material && ctx->image->materials) {
        layer->material = ctx->image->active_material ? ctx->image->active_material : ctx->image->materials;
        LOG_I("DEBUG: Assigned material '%s' to layer '%s'", layer->material->name, layer->name);
    }
    
    int pos[3] = {x, y, z};
    uint8_t color[4] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    
    LOG_I("DEBUG: Adding voxel at (%d,%d,%d) with color (%d,%d,%d,%d) to layer %d", 
          x, y, z, rgba[0], rgba[1], rgba[2], rgba[3], layer->id);
    
    // Check what's currently at this position
    uint8_t existing[4];
    volume_get_at(layer->volume, NULL, pos, existing);
    LOG_I("DEBUG: Existing voxel at position: (%d,%d,%d,%d)", 
          existing[0], existing[1], existing[2], existing[3]);
    
    volume_set_at(layer->volume, NULL, pos, color);
    
    // Verify it was set
    uint8_t verify[4];
    volume_get_at(layer->volume, NULL, pos, verify);
    LOG_I("DEBUG: After setting, voxel at position: (%d,%d,%d,%d)", 
          verify[0], verify[1], verify[2], verify[3]);
    
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
    if (check_read_only(ctx, "delete layer") != 0) return -1;
    
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
    if (check_read_only(ctx, "merge layers") != 0) return -1;
    
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
    
    ctx->read_only = read_only;
    LOG_I("Read-only mode %s", read_only ? "enabled" : "disabled");
}

bool goxel_core_is_read_only(goxel_core_context_t *ctx)
{
    return ctx ? ctx->read_only : false;
}

// Rendering operations
int goxel_core_render_to_file(goxel_core_context_t *ctx, const char *output_file, int width, int height, const char *format, int quality, const char *camera_preset, const uint8_t *background_color)
{
    if (!ctx || !output_file || !ctx->image) return -1;
    
    LOG_I("Rendering scene to file: %s [%dx%d]", output_file, width, height);
    
    // Resize headless rendering buffer if needed
    if (daemon_render_resize(width, height) != 0) {
        LOG_E("Failed to resize headless render buffer");
        return -1;
    }
    
    // CRITICAL FIX: Always update image bounding box from layer voxels before rendering
    // This ensures the camera is positioned correctly regardless of existing camera state
    int aabb[2][3] = {{999999, 999999, 999999}, {-999999, -999999, -999999}};
    bool has_voxels = false;
    
    const layer_t *layer;
    for (layer = ctx->image->layers; layer; layer = layer->next) {
        if (layer->visible && layer->volume) {
            int layer_bbox[2][3];
            volume_get_bbox(layer->volume, layer_bbox, true);
            if (layer_bbox[0][0] < layer_bbox[1][0]) { // Valid bounding box
                has_voxels = true;
                for (int i = 0; i < 3; i++) {
                    if (layer_bbox[0][i] < aabb[0][i]) aabb[0][i] = layer_bbox[0][i];
                    if (layer_bbox[1][i] > aabb[1][i]) aabb[1][i] = layer_bbox[1][i];
                }
            }
        }
    }
    
    if (has_voxels) {
        // Convert AABB to 4x4 box matrix
        float center[3] = {
            (aabb[0][0] + aabb[1][0]) / 2.0f,
            (aabb[0][1] + aabb[1][1]) / 2.0f,
            (aabb[0][2] + aabb[1][2]) / 2.0f
        };
        float size[3] = {
            aabb[1][0] - aabb[0][0],
            aabb[1][1] - aabb[0][1], 
            aabb[1][2] - aabb[0][2]
        };
        
        mat4_set_identity(ctx->image->box);
        ctx->image->box[0][0] = size[0];
        ctx->image->box[1][1] = size[1];
        ctx->image->box[2][2] = size[2];
        ctx->image->box[3][0] = center[0];
        ctx->image->box[3][1] = center[1];
        ctx->image->box[3][2] = center[2];
        
        LOG_I("FIXED: Updated image box: center=[%.1f,%.1f,%.1f] size=[%.1f,%.1f,%.1f]",
              center[0], center[1], center[2], size[0], size[1], size[2]);
    }
    
    // Always create a new camera when a preset is specified, otherwise use existing
    camera_t *camera = NULL;
    bool temp_camera = false;
    
    if (camera_preset && camera_preset[0]) {
        LOG_I("Creating new camera for preset: %s", camera_preset);
        camera = camera_new("temp_preset_cam");
        temp_camera = true;
    } else if (ctx->image->active_camera) {
        LOG_I("Using existing active camera");
        camera = ctx->image->active_camera;
    } else {
        LOG_I("Creating temporary camera for rendering");
        camera = camera_new("temp_render_cam");
        temp_camera = true;
    }
    
    if (!camera) {
        LOG_E("Failed to create camera");
        return -1;
    }
    
    // First fit camera to get the proper distance
    camera_fit_box(camera, ctx->image->box);
    
    // Apply camera preset if specified (will override the rotation but keep distance)
    if (camera_preset && camera_preset[0]) {
        float saved_dist = camera->dist;  // Save distance from fit_box
        LOG_I("Applying camera preset: %s with distance %.2f", camera_preset, saved_dist);
        
        // Reset camera matrix to identity
        mat4_set_identity(camera->mat);
        
        // Set the distance
        camera->dist = saved_dist;
        mat4_itranslate(camera->mat, 0, 0, saved_dist);
        
        // Apply the rotation for the preset
        const char *angles[] = {"front", "back", "left", "right", "top", "bottom", "isometric"};
        float rotations[][2] = {
            {0, 0},           // front - no rotation
            {M_PI, 0},        // back - 180° around Z
            {M_PI/2, 0},      // left - 90° around Z
            {-M_PI/2, 0},     // right - -90° around Z
            {0, -M_PI/2},     // top - -90° around X (look down)
            {0, M_PI/2},      // bottom - 90° around X (look up)
            {M_PI/4, -M_PI/6} // isometric - classic 3/4 view
        };
        
        bool found = false;
        for (int i = 0; i < 7; i++) {
            if (strcmp(camera_preset, angles[i]) == 0) {
                LOG_I("Applying rotation for %s: rz=%.2f, rx=%.2f", camera_preset, rotations[i][0], rotations[i][1]);
                
                // Debug: Print camera matrix before rotation
                LOG_I("Camera matrix before rotation: [%.2f,%.2f,%.2f,%.2f]", 
                      camera->mat[3][0], camera->mat[3][1], camera->mat[3][2], camera->mat[3][3]);
                
                camera_turntable(camera, rotations[i][0], rotations[i][1]);
                
                // Debug: Print camera matrix after rotation
                LOG_I("Camera matrix after rotation: [%.2f,%.2f,%.2f,%.2f]", 
                      camera->mat[3][0], camera->mat[3][1], camera->mat[3][2], camera->mat[3][3]);
                
                found = true;
                break;
            }
        }
        
        if (!found) {
            LOG_W("Unknown camera preset '%s', using default view", camera_preset);
        }
    }
    
    // Set up background color - use provided color or default light gray
    uint8_t default_bg_color[4] = {240, 240, 240, 255};
    const uint8_t *bg_color = background_color ? background_color : default_bg_color;
    
    if (background_color) {
        LOG_I("Using custom background color: [%d,%d,%d,%d]", 
              background_color[0], background_color[1], background_color[2], background_color[3]);
    } else {
        LOG_I("Using default background color: [%d,%d,%d,%d]",
              default_bg_color[0], default_bg_color[1], default_bg_color[2], default_bg_color[3]);
    }
    
    // Render the scene using headless rendering
    LOG_I("About to call daemon_render_scene_with_camera...");
    int render_result = daemon_render_scene_with_camera(ctx->image, camera, bg_color);
    LOG_I("daemon_render_scene_with_camera returned: %d", render_result);
    
    // Clean up temporary camera if we created one
    if (temp_camera) {
        camera_delete(camera);
    }
    
    if (render_result != 0) {
        LOG_E("Failed to render scene");
        return -1;
    }
    
    // Save the rendered result to file
    LOG_I("About to call daemon_render_to_file...");
    if (daemon_render_to_file(output_file, format) != 0) {
        LOG_E("Failed to save rendered image to file");
        return -1;
    }
    LOG_I("daemon_render_to_file completed successfully");
    
    LOG_I("Successfully rendered scene to %s", output_file);
    return 0;
}

int goxel_core_render_to_buffer(goxel_core_context_t *ctx, int width, int height, const char *camera_preset, void **buffer, size_t *buffer_size, const char *format)
{
    if (!ctx || !ctx->image || !buffer || !buffer_size) return -1;
    
    LOG_I("Rendering scene to buffer: %dx%d format=%s", width, height, format ? format : "png");
    
    // Resize headless rendering buffer if needed
    if (daemon_render_resize(width, height) != 0) {
        LOG_E("Failed to resize headless render buffer");
        return -1;
    }
    
    // Use the active camera or create a default one
    camera_t *camera = ctx->image->active_camera;
    bool temp_camera = false;
    if (!camera) {
        // Create a temporary camera for rendering
        camera = camera_new("temp_camera");
        if (!camera) {
            LOG_E("Failed to create temporary camera");
            return -1;
        }
        temp_camera = true;
        
        // Set up camera to view the scene
        camera_fit_box(camera, ctx->image->box);
    }
    
    // Set up background color (light gray)
    uint8_t background_color[4] = {240, 240, 240, 255};
    
    // Render the scene using headless rendering
    int render_result = daemon_render_scene_with_camera(ctx->image, camera, background_color);
    
    // Clean up temporary camera if we created one
    if (temp_camera) {
        camera_delete(camera);
    }
    
    if (render_result != 0) {
        LOG_E("Failed to render scene to buffer");
        return -1;
    }
    
    // Get the rendered framebuffer data
    int fb_width, fb_height, bpp;
    void *fb_buffer = daemon_render_get_buffer(&fb_width, &fb_height, &bpp);
    if (!fb_buffer) {
        LOG_E("Failed to get framebuffer data");
        return -1;
    }
    
    // For now, we'll still encode to PNG format since that's what the API expects
    // In the future, we could return raw RGBA data if format is "raw"
    
    // Create temporary file for PNG encoding (we'll eliminate this in a future improvement)
    char temp_path[] = "/tmp/goxel_buffer_encode_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        LOG_E("Failed to create temp file for buffer encoding");
        return -1;
    }
    close(fd);
    
    // Save framebuffer to temporary PNG file
    if (daemon_render_to_file(temp_path, format) != 0) {
        LOG_E("Failed to encode framebuffer to format");
        unlink(temp_path);
        return -1;
    }
    
    // Read the encoded file into memory buffer
    FILE *file = fopen(temp_path, "rb");
    if (!file) {
        LOG_E("Failed to open encoded temp file");
        unlink(temp_path);
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    *buffer = malloc(size);
    if (!*buffer) {
        LOG_E("Failed to allocate buffer for encoded data");
        fclose(file);
        unlink(temp_path);
        return -1;
    }
    
    size_t bytes_read = fread(*buffer, 1, size, file);
    fclose(file);
    unlink(temp_path);
    
    if (bytes_read != (size_t)size) {
        LOG_E("Failed to read complete encoded data");
        free(*buffer);
        *buffer = NULL;
        return -1;
    }
    
    *buffer_size = size;
    LOG_I("Successfully rendered scene to buffer (%zu bytes)", size);
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
    if (check_read_only(ctx, "execute script") != 0) return -1;
    
    LOG_I("Executing script file: %s", script_file);
    
    // Make sure the global goxel.image is set to our context's image
    // This allows scripts to operate on the current project
    image_t *original_image = goxel.image;
    goxel.image = ctx->image;
    
    // Execute the script using the existing QuickJS system
    int result = script_run_from_file(script_file, 0, NULL);
    
    // If script created a new image, update our context
    if (goxel.image != ctx->image) {
        // Script created a new image, adopt it
        if (ctx->image) {
            image_delete(ctx->image);
        }
        ctx->image = goxel.image;
    }
    
    // Restore original global state 
    goxel.image = original_image;
    
    if (result != 0) {
        LOG_E("Script execution failed with code: %d", result);
        return result;
    }
    
    LOG_I("Script executed successfully: %s", script_file);
    return 0;
}

int goxel_core_execute_script(goxel_core_context_t *ctx, const char *script_code)
{
    if (!ctx || !script_code) return -1;
    if (check_read_only(ctx, "execute script") != 0) return -1;
    
    LOG_I("Executing inline script code");
    
    // Make sure the global goxel.image is set to our context's image
    // This allows scripts to operate on the current project
    image_t *original_image = goxel.image;
    goxel.image = ctx->image;
    
    // Execute the script using the new inline script function
    int result = script_run_from_string(script_code, "<inline-script>");
    
    // If script created a new image, update our context
    if (goxel.image != ctx->image) {
        // Script created a new image, adopt it
        if (ctx->image) {
            image_delete(ctx->image);
        }
        ctx->image = goxel.image;
    }
    
    // Restore original global state 
    goxel.image = original_image;
    
    if (result != 0) {
        LOG_E("Inline script execution failed with code: %d", result);
        return result;
    }
    
    LOG_I("Inline script executed successfully");
    return 0;
}

int goxel_core_get_project_bounds(goxel_core_context_t *ctx, int *width, int *height, int *depth)
{
    if (!ctx || !ctx->image) return -1;
    
    // Check if the image box is null (empty project)
    if (box_is_null(ctx->image->box)) {
        if (width) *width = 0;
        if (height) *height = 0;
        if (depth) *depth = 0;
        return 0;
    }
    
    // Convert the image box to axis-aligned bounding box coordinates
    int aabb[2][3];
    bbox_to_aabb(ctx->image->box, aabb);
    
    // Calculate dimensions from min/max coordinates
    // aabb[0] = min coordinates, aabb[1] = max coordinates
    if (width) *width = aabb[1][0] - aabb[0][0];
    if (height) *height = aabb[1][1] - aabb[0][1]; 
    if (depth) *depth = aabb[1][2] - aabb[0][2];
    
    LOG_D("Project bounds: %dx%dx%d (from box min:[%d,%d,%d] max:[%d,%d,%d])",
          width ? *width : 0, height ? *height : 0, depth ? *depth : 0,
          aabb[0][0], aabb[0][1], aabb[0][2], aabb[1][0], aabb[1][1], aabb[1][2]);
    
    return 0;
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

void goxel_core_debug_layers(goxel_core_context_t *ctx)
{
    if (!ctx || !ctx->image) {
        LOG_E("No context or image available");
        return;
    }
    
    LOG_I("=== Layer Debug Info ===");
    LOG_I("Active layer: %p", ctx->image->active_layer);
    
    int count = 0;
    layer_t *layer;
    DL_FOREACH(ctx->image->layers, layer) {
        count++;
        LOG_I("Layer %d: ID=%d, Name='%s', Visible=%d, Addr=%p", 
              count, layer->id, layer->name, layer->visible, layer);
        if (layer == ctx->image->active_layer) {
            LOG_I("  ^ This is the active layer");
        }
        
        // Count voxels in the layer
        int voxel_count = 0;
        volume_iterator_t iter = volume_get_iterator(layer->volume, VOLUME_ITER_VOXELS);
        int pos[3];
        uint8_t color[4];
        while (volume_iter(&iter, pos)) {
            volume_get_at(layer->volume, &iter, pos, color);
            if (color[3] > 0) voxel_count++;
        }
        LOG_I("  Voxel count: %d", voxel_count);
    }
    LOG_I("Total layers: %d", count);
    LOG_I("=======================");
}

int goxel_core_add_voxels_batch(goxel_core_context_t *ctx, const voxel_op_t *ops, int count)
{
    if (!ctx || !ctx->image || !ops || count <= 0) return -1;
    if (check_read_only(ctx, "add voxels batch") != 0) return -1;
    
    int failed = 0;
    layer_t *current_layer = NULL;
    int current_layer_id = -999; // Invalid ID to force first lookup
    
    LOG_I("Starting batch add of %d voxels", count);
    
    for (int i = 0; i < count; i++) {
        // Only look up layer if ID changed from previous operation
        if (ops[i].layer_id != current_layer_id) {
            current_layer_id = ops[i].layer_id;
            current_layer = NULL;
            
            // Find layer by ID if specified (positive IDs only)
            if (current_layer_id > 0) {
                for (current_layer = ctx->image->layers; current_layer; current_layer = current_layer->next) {
                    if (current_layer->id == current_layer_id) break;
                }
                if (!current_layer) {
                    LOG_W("Batch op %d: Layer with ID %d not found, using active layer", i, current_layer_id);
                    current_layer = ctx->image->active_layer;
                }
            } else {
                // layer_id <= 0 means use active layer
                current_layer = ctx->image->active_layer;
            }
            
            // If still no layer, use first layer
            if (!current_layer) {
                if (ctx->image->layers) {
                    current_layer = ctx->image->layers;
                    LOG_W("Batch op %d: No active layer, using first layer (ID: %d)", i, current_layer->id);
                } else {
                    LOG_E("Batch op %d: No layers available in the image", i);
                    failed++;
                    continue;
                }
            }
        }
        
        // Add voxel to current layer
        int pos[3] = {ops[i].x, ops[i].y, ops[i].z};
        uint8_t color[4];
        memcpy(color, ops[i].rgba, 4);
        
        volume_set_at(current_layer->volume, NULL, pos, color);
    }
    
    LOG_I("Batch add completed: %d succeeded, %d failed", count - failed, failed);
    return failed > 0 ? -1 : 0;
}

int goxel_core_remove_voxels_batch(goxel_core_context_t *ctx, const voxel_op_t *ops, int count)
{
    if (!ctx || !ctx->image || !ops || count <= 0) return -1;
    if (check_read_only(ctx, "remove voxels batch") != 0) return -1;
    
    int failed = 0;
    layer_t *current_layer = NULL;
    int current_layer_id = -999; // Invalid ID to force first lookup
    
    LOG_I("Starting batch remove of %d voxels", count);
    
    for (int i = 0; i < count; i++) {
        // Only look up layer if ID changed from previous operation
        if (ops[i].layer_id != current_layer_id) {
            current_layer_id = ops[i].layer_id;
            current_layer = NULL;
            
            // Find layer by ID if specified (positive IDs only)
            if (current_layer_id > 0) {
                for (current_layer = ctx->image->layers; current_layer; current_layer = current_layer->next) {
                    if (current_layer->id == current_layer_id) break;
                }
                if (!current_layer) {
                    LOG_W("Batch op %d: Layer with ID %d not found, using active layer", i, current_layer_id);
                    current_layer = ctx->image->active_layer;
                }
            } else {
                // layer_id <= 0 means use active layer
                current_layer = ctx->image->active_layer;
            }
            
            // If still no layer, use first layer
            if (!current_layer) {
                if (ctx->image->layers) {
                    current_layer = ctx->image->layers;
                    LOG_W("Batch op %d: No active layer, using first layer (ID: %d)", i, current_layer->id);
                } else {
                    LOG_E("Batch op %d: No layers available in the image", i);
                    failed++;
                    continue;
                }
            }
        }
        
        // Remove voxel from current layer
        int pos[3] = {ops[i].x, ops[i].y, ops[i].z};
        uint8_t transparent[4] = {0, 0, 0, 0};
        
        volume_set_at(current_layer->volume, NULL, pos, transparent);
    }
    
    LOG_I("Batch remove completed: %d succeeded, %d failed", count - failed, failed);
    return failed > 0 ? -1 : 0;
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