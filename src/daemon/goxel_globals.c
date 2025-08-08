/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * Minimal global state for daemon mode
 * This provides the necessary global goxel instance and initialization
 * without GUI dependencies.
 */

#include "goxel.h"
#include "../log.h"
#include "daemon_render/render_daemon.h"

// The global goxel instance for daemon mode
goxel_t goxel = {};

// Minimal initialization for daemon mode
void goxel_init(void)
{
    LOG_I("Initializing goxel for daemon mode");
    
    // Initialize core components
    shapes_init();
    // Note: script_init() is not available in daemon mode
    
    // Initialize daemon rendering with default size
    if (daemon_render_init(512, 512) != 0) {
        LOG_W("Failed to initialize daemon rendering, rendering will be limited");
    }
    
    // Initialize a minimal default palette for daemon mode
    // This avoids file I/O which can cause hangs in daemon mode
    palette_t *default_palette = calloc(1, sizeof(palette_t));
    strcpy(default_palette->name, "Default");
    default_palette->columns = 8;
    
    // Add some basic colors
    const uint8_t basic_colors[][4] = {
        {0, 0, 0, 255},       // Black
        {255, 255, 255, 255}, // White
        {255, 0, 0, 255},     // Red
        {0, 255, 0, 255},     // Green
        {0, 0, 255, 255},     // Blue
        {255, 255, 0, 255},   // Yellow
        {255, 0, 255, 255},   // Magenta
        {0, 255, 255, 255},   // Cyan
        {128, 128, 128, 255}, // Gray
        {255, 128, 0, 255},   // Orange
        {128, 0, 255, 255},   // Purple
        {0, 128, 255, 255},   // Light Blue
        {255, 128, 128, 255}, // Light Red
        {128, 255, 128, 255}, // Light Green
        {128, 128, 255, 255}, // Light Blue
        {64, 64, 64, 255},    // Dark Gray
    };
    
    default_palette->size = sizeof(basic_colors) / sizeof(basic_colors[0]);
    default_palette->allocated = default_palette->size;
    default_palette->entries = calloc(default_palette->size, sizeof(palette_entry_t));
    
    for (int i = 0; i < default_palette->size; i++) {
        memcpy(default_palette->entries[i].color, basic_colors[i], 4);
        snprintf(default_palette->entries[i].name, sizeof(default_palette->entries[i].name), 
                 "Color %d", i);
    }
    
    goxel.palettes = default_palette;
    goxel.palette = default_palette;
    
    // Call reset to initialize default state
    goxel_reset();
}

// Reset goxel state
void goxel_reset(void)
{
    LOG_D("Resetting goxel state");
    
    // Clean up existing image
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Create new empty image
    goxel.image = image_new();
    
    // Reset plane to horizontal at origin
    plane_from_vectors(goxel.plane,
            VEC(0, 0, 0), VEC(1, 0, 0), VEC(0, 1, 0));
    
    // Set default colors
    vec4_set(goxel.back_color, 70, 70, 70, 255);
    vec4_set(goxel.grid_color, 255, 255, 255, 127);
    vec4_set(goxel.image_box_color, 204, 204, 255, 255);
    
    // Set default tool radius
    goxel.tool_radius = 0.5;
    
    // Set default painter settings
    goxel.painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_OVER,
        .smoothness = 0,
        .color = {255, 255, 255, 255},
    };
    
    // Set symmetry origin to the center of the image
    if (goxel.image) {
        mat4_mul_vec3(goxel.image->box, VEC(0, 0, 0),
                      goxel.painter.symmetry_origin);
    }
    
    // Set default renderer settings
    goxel.rend.light = (typeof(goxel.rend.light)) {
        .pitch = 35 * DD2R,
        .yaw = 25 * DD2R,
        .intensity = 1.0,
    };
    goxel.rend.settings = (render_settings_t) {
        .occlusion_strength = 0.5,
        .effects = 0,
    };
    
    // Set default snap settings
    goxel.snap_mask = SNAP_VOLUME;
    goxel.snap_offset = 0.5;
    
    // Clear layers volume cache
    if (goxel.layers_volume_) {
        volume_delete(goxel.layers_volume_);
        goxel.layers_volume_ = NULL;
    }
    if (goxel.render_volume_) {
        volume_delete(goxel.render_volume_);
        goxel.render_volume_ = NULL;
    }
    
    LOG_D("Goxel state reset complete");
}

// Cleanup function for daemon shutdown
void goxel_release(void)
{
    LOG_I("Releasing goxel resources");
    
    // Shutdown daemon rendering
    daemon_render_shutdown();
    
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    if (goxel.layers_volume_) {
        volume_delete(goxel.layers_volume_);
        goxel.layers_volume_ = NULL;
    }
    
    if (goxel.render_volume_) {
        volume_delete(goxel.render_volume_);
        goxel.render_volume_ = NULL;
    }
    
    // Clean up the default palette created for daemon mode
    if (goxel.palettes) {
        palette_t *pal = goxel.palettes;
        if (pal->entries) {
            free(pal->entries);
        }
        free(pal);
        goxel.palettes = NULL;
        goxel.palette = NULL;
    }
}