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

// Utility functions
#include "utils/box.h"
#include "utils/cache.h"
#include "utils/color.h"
#include "utils/geometry.h"
#include "utils/img.h"
#include "utils/path.h"
#include "utils/plane.h"
#include "utils/vec.h"
#include "utils/json.h"

// Standard includes
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOXEL_VERSION_STR "0.15.2"

// Core context structure for headless operation
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
    
};

// Core initialization and management
int goxel_core_init(goxel_core_t *ctx);
void goxel_core_shutdown(goxel_core_t *ctx);
void goxel_core_reset(goxel_core_t *ctx);

// Project management
int goxel_core_create_project(goxel_core_t *ctx, const char *name);
int goxel_core_load_project(goxel_core_t *ctx, const char *path);
int goxel_core_save_project(goxel_core_t *ctx, const char *path);

// Volume operations
int goxel_core_add_voxel(goxel_core_t *ctx, int x, int y, int z, uint8_t rgba[4]);
int goxel_core_remove_voxel(goxel_core_t *ctx, int x, int y, int z);
int goxel_core_get_voxel(goxel_core_t *ctx, int x, int y, int z, uint8_t rgba[4]);

// Layer operations
int goxel_core_create_layer(goxel_core_t *ctx, const char *name);
int goxel_core_delete_layer(goxel_core_t *ctx, int layer_id);
int goxel_core_set_active_layer(goxel_core_t *ctx, int layer_id);

// Include project management functions after core type is defined
#include "project_mgmt.h"

#endif // GOXEL_CORE_H