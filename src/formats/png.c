/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"
#include "file_format.h"
#include "core/goxel_core.h"

// XXX: this function has to be rewritten.
static int png_export(const image_t *img, const char *path, int w, int h)
{
    // In daemon mode, graphics_initialized is false and goxel_render_to_buf doesn't work
    // We need to use the OSMesa rendering pipeline instead
    extern goxel_t goxel;
    LOG_I("PNG export: graphics_initialized = %d", goxel.graphics_initialized);
    
    // Force daemon mode detection - check for OSMesa context existence instead
    // In daemon mode, we should use the proper render pipeline
    extern goxel_core_context_t *g_goxel_context;
    if (g_goxel_context != NULL) {
        // We're in daemon mode - use proper rendering pipeline
        LOG_I("Daemon mode detected for PNG export - using OSMesa pipeline (forced)");
        
        // Use image dimensions if custom size is set, otherwise use provided w,h
        int render_width = w;
        int render_height = h;
        
        if (img->export_custom_size) {
            render_width = img->export_width;
            render_height = img->export_height;
            LOG_I("Using custom export dimensions: %dx%d", render_width, render_height);
        } else {
            LOG_I("Using provided dimensions: %dx%d", render_width, render_height);
        }
        
        int result = goxel_core_render_to_file(g_goxel_context, path, render_width, render_height, "png", 90, NULL, NULL);
        if (result != 0) {
            LOG_E("Failed to render PNG in daemon mode: error code %d", result);
            return -1;
        }
        
        LOG_I("PNG export completed successfully using daemon render pipeline");
        return 0;
    }
    
    if (!goxel.graphics_initialized) {
        // Daemon mode detected - use the proper render pipeline
        LOG_I("Daemon mode detected for PNG export - using OSMesa pipeline");
        
        // We need access to the goxel core context for rendering
        // Get it through external linkage since we're in daemon mode
        extern goxel_core_context_t *g_goxel_context;
        if (!g_goxel_context) {
            LOG_E("Goxel context not available for daemon PNG export");
            return -1;
        }
        
        // Use the same render pipeline as render_scene
        // Use image dimensions if custom size is set, otherwise use provided w,h
        int render_width = w;
        int render_height = h;
        
        if (img->export_custom_size) {
            render_width = img->export_width;
            render_height = img->export_height;
            LOG_I("Using custom export dimensions: %dx%d", render_width, render_height);
        } else {
            LOG_I("Using provided dimensions: %dx%d", render_width, render_height);
        }
        
        int result = goxel_core_render_to_file(g_goxel_context, path, render_width, render_height, "png", 90, NULL, NULL);
        if (result != 0) {
            LOG_E("Failed to render PNG in daemon mode: error code %d", result);
            return -1;
        }
        
        LOG_I("PNG export completed successfully using daemon render pipeline");
        return 0;
    }

    // GUI mode - use original rendering method
    goxel_create_graphics();

    uint8_t *buf;
    int bpp = img->export_transparent_background ? 4 : 3;
    if (!path) return -1;
    LOG_I("Exporting to file %s", path);
    buf = calloc(w * h, bpp);
    goxel_render_to_buf(buf, w, h, bpp);
    img_write(buf, w, h, bpp, path);
    free(buf);
    return 0;
}

static void export_gui(file_format_t *format)
{
    int maxsize, i;

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    gui_checkbox(_("Size"), &goxel.image->export_custom_size, NULL);
    if (!goxel.image->export_custom_size) {
        goxel.image->export_width = goxel.gui.viewport[2];
        goxel.image->export_height = goxel.gui.viewport[3];
    }

    gui_enabled_begin(goxel.image->export_custom_size);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 1, maxsize))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 1, maxsize))
        goxel.image->export_height = clamp(i, 1, maxsize);
    gui_enabled_end();
    gui_group_end();

    gui_checkbox(_("Transparent Background"),
                 &goxel.image->export_transparent_background,
                 NULL);
}

static int export_as_png(const file_format_t *format, const image_t *img,
                         const char *path)
{
    png_export(img, path, img->export_width, img->export_height);
    return 0;
}

FILE_FORMAT_REGISTER(png,
    .name = "png",
    .exts = {"*.png"},
    .exts_desc = "png",
    .export_gui = export_gui,
    .export_func = export_as_png,
    .priority = 90,
)
