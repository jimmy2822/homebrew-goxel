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
#include <errno.h>
#include <string.h>
#include <assert.h>

int goxel_core_init(goxel_core_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize core systems
    shapes_init();
    
    // Set default parameters
    ctx->tool_radius = 1;
    ctx->snap_offset = 0.5f;
    ctx->snap_mask = SNAP_MASK_IMAGE_BOX;
    
    // Set default painter color (white)
    ctx->painter_color[0] = 255;
    ctx->painter_color[1] = 255;
    ctx->painter_color[2] = 255;
    ctx->painter_color[3] = 255;
    ctx->painter_mode = MODE_OVER;
    ctx->painter_shape = SHAPE_CUBE;
    
    return 0;
}

void goxel_core_shutdown(goxel_core_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    if (ctx->palette) {
        palette_delete(ctx->palette);
        ctx->palette = NULL;
    }
}

void goxel_core_reset(goxel_core_t *ctx)
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

int goxel_core_create_project(goxel_core_t *ctx, const char *name)
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
    
    return 0;
}

int goxel_core_load_project(goxel_core_t *ctx, const char *path)
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

int goxel_core_save_project(goxel_core_t *ctx, const char *path)
{
    if (!ctx || !path || !ctx->image) return -1;
    
    int ret = goxel_export_to_file(path, NULL);
    if (ret == 0) {
        snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", path);
    }
    
    return ret;
}

int goxel_core_add_voxel(goxel_core_t *ctx, int x, int y, int z, uint8_t rgba[4])
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_get_active_layer(ctx->image);
    if (!layer) return -1;
    
    float pos[3] = {(float)x, (float)y, (float)z};
    uint8_t color[4] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    
    volume_set_at(layer->volume, NULL, pos, color);
    
    return 0;
}

int goxel_core_remove_voxel(goxel_core_t *ctx, int x, int y, int z)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_get_active_layer(ctx->image);
    if (!layer) return -1;
    
    float pos[3] = {(float)x, (float)y, (float)z};
    uint8_t color[4] = {0, 0, 0, 0}; // Transparent = removal
    
    volume_set_at(layer->volume, NULL, pos, color);
    
    return 0;
}

int goxel_core_get_voxel(goxel_core_t *ctx, int x, int y, int z, uint8_t rgba[4])
{
    if (!ctx || !ctx->image || !rgba) return -1;
    
    layer_t *layer = image_get_active_layer(ctx->image);
    if (!layer) return -1;
    
    float pos[3] = {(float)x, (float)y, (float)z};
    volume_get_at(layer->volume, NULL, pos, rgba);
    
    return 0;
}

int goxel_core_create_layer(goxel_core_t *ctx, const char *name)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_add_layer(ctx->image, NULL);
    if (!layer) return -1;
    
    if (name) {
        snprintf(layer->name, sizeof(layer->name), "%s", name);
    }
    
    return layer->id;
}

int goxel_core_delete_layer(goxel_core_t *ctx, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_get_layer_by_id(ctx->image, layer_id);
    if (!layer) return -1;
    
    image_delete_layer(ctx->image, layer);
    return 0;
}

int goxel_core_set_active_layer(goxel_core_t *ctx, int layer_id)
{
    if (!ctx || !ctx->image) return -1;
    
    layer_t *layer = image_get_layer_by_id(ctx->image, layer_id);
    if (!layer) return -1;
    
    ctx->image->active_layer = layer;
    return 0;
}