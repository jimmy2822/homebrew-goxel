/*
 * Goxel 3D voxels editor
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
 * Headless implementation of goxel global state and functions
 * This provides the minimal set of functions needed by the core system
 * without the full GUI implementation.
 */

#include "goxel.h"
#include "render_daemon.h"

// Forward declarations for missing functions
const tool_t *tool_get(int id)
{
    // In headless mode, return NULL tool
    (void)id;
    return NULL;
}

// Get the active camera from the current image
static camera_t *get_camera(void)
{
    if (!goxel.image) return NULL;
    
    if (!goxel.image->cameras)
        image_add_camera(goxel.image, NULL);
    if (goxel.image->active_camera)
        return goxel.image->active_camera;
    return goxel.image->cameras;
}

// Daemon render function using the proper rendering system
void goxel_render_to_buf(uint8_t *buf, int w, int h, int bpp)
{
    if (!buf || !goxel.image) return;
    
    // Initialize daemon rendering if needed
    static bool daemon_render_initialized = false;
    if (!daemon_render_initialized) {
        if (daemon_render_init(w, h) != 0) {
            LOG_E("Failed to initialize daemon rendering");
            // Fall back to gray background
            for (int i = 0; i < w * h * bpp; i += bpp) {
                buf[i + 0] = 128;  // R
                buf[i + 1] = 128;  // G  
                buf[i + 2] = 128;  // B
                if (bpp == 4) {
                    buf[i + 3] = 255;  // A
                }
            }
            return;
        }
        daemon_render_initialized = true;
    }
    
    // Resize render buffer if needed
    if (daemon_render_resize(w, h) != 0) {
        LOG_E("Failed to resize daemon render buffer");
        return;
    }
    
    // Get the current camera
    camera_t *camera = get_camera();
    camera->aspect = (float)w / h;
    camera_update(camera);
    
    // Set up background color (light gray)
    uint8_t background_color[4] = {240, 240, 240, 255};
    
    // Render the scene
    if (daemon_render_scene_with_camera(goxel.image, camera, background_color) != 0) {
        LOG_E("Failed to render scene");
        return;
    }
    
    // Get the rendered framebuffer data
    int fb_width, fb_height, fb_bpp;
    void *fb_buffer = daemon_render_get_buffer(&fb_width, &fb_height, &fb_bpp);
    if (!fb_buffer) {
        LOG_E("Failed to get framebuffer data");
        return;
    }
    
    // Copy the rendered data to the output buffer
    // Handle different bpp cases
    if (fb_bpp == bpp && fb_width == w && fb_height == h) {
        // Direct copy if formats match
        memcpy(buf, fb_buffer, w * h * bpp);
    } else {
        // Need to convert format
        LOG_W("Buffer format conversion needed: fb(%dx%d,%d) -> out(%dx%d,%d)", 
              fb_width, fb_height, fb_bpp, w, h, bpp);
        
        // Simple copy with format conversion (assumes RGBA source)
        for (int y = 0; y < h && y < fb_height; y++) {
            for (int x = 0; x < w && x < fb_width; x++) {
                int src_idx = (y * fb_width + x) * 4;  // Assume RGBA source
                int dst_idx = (y * w + x) * bpp;
                uint8_t *src = (uint8_t*)fb_buffer + src_idx;
                
                buf[dst_idx + 0] = src[0];  // R
                buf[dst_idx + 1] = src[1];  // G
                buf[dst_idx + 2] = src[2];  // B
                if (bpp == 4) {
                    buf[dst_idx + 3] = src[3];  // A
                }
            }
        }
    }
}

// Minimal GUI functions for headless mode (no-ops)
bool gui_checkbox(const char *label, bool *value, const char *hint)
{
    (void)label; (void)value; (void)hint;
    return false;
}

void gui_dummy(int w, int h)
{
    (void)w; (void)h;
}

void gui_enabled_begin(bool enabled)
{
    (void)enabled;
}

void gui_enabled_end(void)
{
}

void gui_group_begin(const char *label)
{
    (void)label;
}

void gui_group_end(void)
{
}

bool gui_input_float(const char *label, float *v, float step, float min, float max, const char *format)
{
    (void)label; (void)v; (void)step; (void)min; (void)max; (void)format;
    return false;
}

bool gui_input_int(const char *label, int *v, int minv, int maxv)
{
    (void)label; (void)v; (void)minv; (void)maxv;
    return false;
}

// Texture function for headless mode
texture_t *texture_new_image(const char *path, int flags)
{
    // In headless mode, don't load textures
    (void)path;
    (void)flags;
    return NULL;
}

// PNG functionality is provided by STB image library in core/utils/img.c
// No stubs needed - STB implementation works without libpng dependency

// Global goxel instance is defined in goxel.c
extern goxel_t goxel;

// This function is now defined in goxel.c
// We should not redefine it here to avoid duplicate symbols
#if 0
void goxel_init(void)
{
    memset(&goxel, 0, sizeof(goxel));
    
    // Initialize core components
    goxel.palette = NULL; // Palette will be loaded on demand
    goxel.image = NULL;    // Don't create a default image - let contexts manage their own
    goxel.tool = NULL;     // No tools in headless mode
    goxel.snap_mask = SNAP_VOLUME;
    
    // Initialize default material
    goxel.painter = (painter_t){
        .mode = MODE_OVER,  // MODE_ADD is actually MODE_OVER (0)
        .shape = &shape_cube,
        .color = {1.0, 1.0, 1.0, 1.0},
    };
    
    // Initialize basic renderer
    memset(&goxel.rend, 0, sizeof(goxel.rend));
}
#endif

// This function is now defined in goxel_globals.c
#if 0
void goxel_release(void)
{
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    // Note: palette is not managed by us in headless mode
}
#endif

// This function is now defined in goxel.c
#if 0
void goxel_reset(void)
{
    if (goxel.image) {
        image_delete(goxel.image);
    }
    goxel.image = image_new();
}
#endif

// Minimal hint system for headless mode
void goxel_add_hint(int flags, const char *title, const char *msg)
{
    // In headless mode, hints are ignored
    (void)flags;
    (void)title;
    (void)msg;
}

// Minimal recent files system for headless mode
void goxel_add_recent_file(const char *path)
{
    // In headless mode, recent files are ignored
    (void)path;
}

// Headless graphics creation (no-op since we don't need GUI graphics)
void goxel_create_graphics(void)
{
    // Nothing to do in headless mode
}

// Get layers volume for export/render operations
const volume_t *goxel_get_layers_volume(const image_t *img)
{
    const image_t *image = img ? img : goxel.image;
    if (!image || !image->active_layer) {
        return NULL;
    }
    return image->active_layer->volume;
}

// Get render layers (simplified for headless)
const layer_t *goxel_get_render_layers(bool with_tool_preview)
{
    if (!goxel.image) {
        return NULL;
    }
    (void)with_tool_preview; // Ignored in headless mode
    return goxel.image->layers;
}

// File operations
int goxel_import_file(const char *path, const char *format)
{
    if (!goxel.image) {
        goxel.image = image_new();
    }
    
    return load_from_file(path, format);
}

int goxel_export_to_file(const char *path, const char *format)
{
    if (!goxel.image) {
        return -1;
    }
    
    // For .gox format or when format is not specified, use save_to_file
    if (!format || strcmp(format, "gox") == 0) {
        save_to_file(goxel.image, path);
        
        // Update export path for consistency
        free(goxel.image->export_path);
        goxel.image->export_path = strdup(path);
        goxel.image->export_fmt = "gox";
        
        return 0;
    }
    
    // For other formats, we need the file format system
    // Since we're in daemon mode, let's check if the formats are available
    LOG_E("Export format '%s' not supported in daemon mode yet", format);
    return -1;
}

// Headless gesture system (no-op)
bool goxel_gesture3d(const gesture3d_t *gesture)
{
    // In headless mode, gestures are ignored
    (void)gesture;
    return false;
}

// Update function for headless mode
void goxel_update(void)
{
    // Minimal update for headless mode
    // No GUI updates needed
}

// Main iteration function for headless mode  
int goxel_iter(const inputs_t *inputs)
{
    // In headless mode, we ignore inputs
    (void)inputs;
    goxel_update();
    return 0;
}

// Set/get current layer
void goxel_set_layer(layer_t *layer)
{
    if (goxel.image) {
        goxel.image->active_layer = layer;
    }
}

layer_t *goxel_get_layer(void)
{
    if (!goxel.image) {
        return NULL;
    }
    return goxel.image->active_layer;
}

// Create new layer
layer_t *goxel_add_layer(const char *name)
{
    if (!goxel.image) {
        goxel.image = image_new();
    }
    
    layer_t *layer = layer_new(name);
    DL_APPEND(goxel.image->layers, layer);
    goxel.image->active_layer = layer;
    
    return layer;
}





// Project operations
void goxel_new_project(void)
{
    goxel_reset();
}

