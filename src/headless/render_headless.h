/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef RENDER_HEADLESS_H
#define RENDER_HEADLESS_H

#include <stdbool.h>

#ifdef OSMESA_RENDERING
#ifdef __APPLE__
// OSMesa may not be available on macOS through standard Mesa
typedef void* OSMesaContext;
#else
#include <GL/osmesa.h>
#endif
#endif

// EGL support for alternative headless rendering
#ifdef EGL_RENDERING
#include <EGL/egl.h>
#endif

/*
 * Headless rendering system for Goxel using OSMesa.
 * Provides offscreen OpenGL rendering capabilities without requiring
 * a display system like X11 or Wayland.
 */

/*
 * Function: headless_render_init
 * Initialize the headless rendering system.
 *
 * Parameters:
 *   width  - Framebuffer width in pixels
 *   height - Framebuffer height in pixels
 *
 * Returns:
 *   0 on success, -1 on error
 */
int headless_render_init(int width, int height);

/*
 * Function: headless_render_shutdown
 * Cleanup headless rendering resources.
 */
void headless_render_shutdown(void);

/*
 * Function: headless_render_resize
 * Resize the headless framebuffer.
 *
 * Parameters:
 *   width  - New framebuffer width in pixels
 *   height - New framebuffer height in pixels
 *
 * Returns:
 *   0 on success, -1 on error
 */
int headless_render_resize(int width, int height);

/*
 * Function: headless_render_scene
 * Set up OpenGL state for scene rendering.
 * This should be called before any render_xxx functions.
 *
 * Returns:
 *   0 on success, -1 on error
 */
int headless_render_scene(void);

/*
 * Function: headless_render_to_file
 * Save the current framebuffer to a file.
 *
 * Parameters:
 *   filename - Output file path
 *   format   - Image format (e.g., "png", "jpg") - can be NULL for auto-detect
 *
 * Returns:
 *   0 on success, -1 on error
 */
int headless_render_to_file(const char *filename, const char *format);

/*
 * Function: headless_render_get_buffer
 * Get direct access to the framebuffer data.
 *
 * Parameters:
 *   width  - Pointer to store framebuffer width (can be NULL)
 *   height - Pointer to store framebuffer height (can be NULL)
 *   bpp    - Pointer to store bytes per pixel (can be NULL)
 *
 * Returns:
 *   Pointer to RGBA framebuffer data, or NULL if not initialized
 */
void *headless_render_get_buffer(int *width, int *height, int *bpp);

/*
 * Function: headless_render_is_initialized
 * Check if headless rendering is initialized.
 *
 * Returns:
 *   true if initialized, false otherwise
 */
bool headless_render_is_initialized(void);

/*
 * Function: headless_render_create_context
 * Create a new OSMesa context (for advanced usage).
 * Only available when compiled with OSMESA_RENDERING.
 *
 * Returns:
 *   OSMesa context handle, or NULL on error
 */
#ifdef OSMESA_RENDERING
OSMesaContext headless_render_create_context(void);
#endif

#endif // RENDER_HEADLESS_H