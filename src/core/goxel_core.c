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
    
    if (name) {
        snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", name);
    }
    
    // Note: width, height, depth parameters are for initial project setup
    // In Goxel, projects can grow dynamically, so these are informational
    
    return 0;
}

int goxel_core_load_project(goxel_core_context_t *ctx, const char *path)
{
    if (!ctx || !path) return -1;
    
    image_t *img = image_new();
    if (!img) return -1;
    
    int ret = goxel_import_file(path, NULL);
    if (ret != 0) {
        image_delete(img);
        return ret;
    }
    
    if (ctx->image) {
        image_delete(ctx->image);
    }
    
    ctx->image = img;
    snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", path);
    
    // Add to recent files
    for (int i = 7; i > 0; i--) {
        strcpy(ctx->recent_files[i], ctx->recent_files[i-1]);
    }
    snprintf(ctx->recent_files[0], sizeof(ctx->recent_files[0]), "%s", path);
    
    return 0;
}

int goxel_core_save_project(goxel_core_context_t *ctx, const char *path)
{
    if (!ctx || !path || !ctx->image) return -1;
    
    int ret = goxel_export_to_file(path, NULL);
    if (ret == 0) {
        snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", path);
    }
    
    return ret;
}

int goxel_core_add_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4], int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = layer_id == 0 ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id != 0) {
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
    
    layer_t *layer = layer_id == 0 ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id != 0) {
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

int goxel_core_create_layer(goxel_core_context_t *ctx, const char *name)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_add_layer(ctx->image, NULL);
    if (!layer) return -1;
    
    if (name) {
        snprintf(layer->name, sizeof(layer->name), "%s", name);
    }
    
    return layer->id;
}

int goxel_core_delete_layer(goxel_core_context_t *ctx, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = NULL;
    
    // Find layer by ID 
    for (layer = ctx->image->layers; layer; layer = layer->next) {
        if (layer->id == layer_id) break;
    }
    if (!layer) return -1;
    
    image_delete_layer(ctx->image, layer);
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
    
    layer_t *layer = layer_id == 0 ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id != 0) {
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
    
    layer_t *layer = layer_id == 0 ? ctx->image->active_layer : NULL;
    
    // Find layer by ID if specified
    if (layer_id != 0) {
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