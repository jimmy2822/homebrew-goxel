/* Goxel 3D voxels editor - Core API
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

#ifndef GOXEL_CORE_H
#define GOXEL_CORE_H

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#ifndef NOMINMAX
#   define NOMINMAX
#endif

// Forward declarations
typedef struct goxel_core goxel_core_t;

// Core data structures
#include "volume.h"
#include "volume_utils.h"
#include "image.h"
#include "layer.h"
#include "material.h"
#include "shape.h"
#include "palette.h"
#include "file_format.h"

// Forward declarations for utility functions
typedef struct vec4 vec4_t;
typedef struct mat4 mat4_t;

// Standard includes
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOXEL_VERSION_STR "0.17.32"

// Core context structure for headless operation
typedef struct goxel_core goxel_core_context_t;

struct goxel_core {
    // Active image
    image_t *image;
    
    // Tool parameters
    int tool_radius;
    float snap_offset;
    uint8_t snap_mask;
    
    // Drawing parameters
    uint8_t painter_color[4];
    int painter_mode;
    int painter_shape;
    
    // Current palette
    palette_t *palette;
    
    // File history
    char recent_files[8][1024];
    
    // Read-only mode flag
    bool read_only;
    
};

// Core context management
goxel_core_context_t *goxel_core_create_context(void);
void goxel_core_destroy_context(goxel_core_context_t *ctx);

// Core initialization and management
int goxel_core_init(goxel_core_context_t *ctx);
void goxel_core_shutdown(goxel_core_context_t *ctx);
void goxel_core_reset(goxel_core_context_t *ctx);

// Project management
int goxel_core_create_project(goxel_core_context_t *ctx, const char *name, int width, int height, int depth);
int goxel_core_load_project(goxel_core_context_t *ctx, const char *path);
int goxel_core_save_project(goxel_core_context_t *ctx, const char *path);
int goxel_core_save_project_format(goxel_core_context_t *ctx, const char *path, const char *format);
int goxel_core_create_backup(goxel_core_context_t *ctx, const char *path);
void goxel_core_set_read_only(goxel_core_context_t *ctx, bool read_only);
bool goxel_core_is_read_only(goxel_core_context_t *ctx);
int goxel_core_get_project_bounds(goxel_core_context_t *ctx, int *width, int *height, int *depth);

// Volume operations
int goxel_core_add_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4], int layer_id);
int goxel_core_remove_voxel(goxel_core_context_t *ctx, int x, int y, int z, int layer_id);
int goxel_core_remove_voxels_in_box(goxel_core_context_t *ctx, int x1, int y1, int z1, int x2, int y2, int z2, int layer_id);
int goxel_core_paint_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4], int layer_id);
int goxel_core_get_voxel(goxel_core_context_t *ctx, int x, int y, int z, uint8_t rgba[4]);

// Batch voxel operations
typedef struct voxel_op {
    int x, y, z;
    uint8_t rgba[4];
    int layer_id;
} voxel_op_t;

int goxel_core_add_voxels_batch(goxel_core_context_t *ctx, const voxel_op_t *ops, int count);
int goxel_core_remove_voxels_batch(goxel_core_context_t *ctx, const voxel_op_t *ops, int count);

// Layer operations
int goxel_core_create_layer(goxel_core_context_t *ctx, const char *name, uint8_t rgba[4], int visible);
int goxel_core_delete_layer(goxel_core_context_t *ctx, int layer_id, const char *name);
int goxel_core_set_active_layer(goxel_core_context_t *ctx, int layer_id);
int goxel_core_get_layer_count(goxel_core_context_t *ctx);
int goxel_core_merge_layers(goxel_core_context_t *ctx, int source_id, int target_id, const char *source_name, const char *target_name);
int goxel_core_set_layer_visibility(goxel_core_context_t *ctx, int layer_id, const char *name, int visible);
int goxel_core_rename_layer(goxel_core_context_t *ctx, int layer_id, const char *old_name, const char *new_name);
void goxel_core_debug_layers(goxel_core_context_t *ctx);

// Rendering operations  
int goxel_core_render_to_file(goxel_core_context_t *ctx, const char *output_file, int width, int height, const char *format, int quality, const char *camera_preset, const uint8_t *background_color);
int goxel_core_render_to_buffer(goxel_core_context_t *ctx, int width, int height, const char *camera_preset, void **buffer, size_t *buffer_size, const char *format);

// Export operations
int goxel_core_export_project(goxel_core_context_t *ctx, const char *output_file, const char *format);
int goxel_core_list_export_formats(char *buffer, size_t buffer_size);

// Scripting operations
int goxel_core_execute_script_file(goxel_core_context_t *ctx, const char *script_file);
int goxel_core_execute_script(goxel_core_context_t *ctx, const char *script_code);

// Include project management functions after core type is defined
#include "project_mgmt.h"

#endif // GOXEL_CORE_H