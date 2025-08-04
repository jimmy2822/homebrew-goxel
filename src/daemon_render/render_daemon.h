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

#ifndef RENDER_DAEMON_H
#define RENDER_DAEMON_H

#include <stdbool.h>

#ifdef HAVE_OSMESA
#include <GL/osmesa.h>
#elif defined(OSMESA_RENDERING)
// OSMesa rendering requested but headers not available - use stub
typedef void* OSMesaContext;
#endif

// EGL support for alternative daemon rendering
#ifdef EGL_RENDERING
#include <EGL/egl.h>
#endif

/*
 * Daemon rendering system for Goxel using OSMesa.
 * Provides offscreen OpenGL rendering capabilities without requiring
 * a display system like X11 or Wayland.
 */

/*
 * Function: daemon_render_init
 * Initialize the daemon rendering system.
 *
 * Parameters:
 *   width  - Framebuffer width in pixels
 *   height - Framebuffer height in pixels
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_init(int width, int height);

/*
 * Function: daemon_render_shutdown
 * Cleanup daemon rendering resources.
 */
void daemon_render_shutdown(void);

/*
 * Function: daemon_render_resize
 * Resize the daemon framebuffer.
 *
 * Parameters:
 *   width  - New framebuffer width in pixels
 *   height - New framebuffer height in pixels
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_resize(int width, int height);

/*
 * Function: daemon_render_scene
 * Set up OpenGL state for scene rendering.
 * This should be called before any render_xxx functions.
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_scene(void);

/*
 * Function: daemon_render_to_file
 * Save the current framebuffer to a file.
 *
 * Parameters:
 *   filename - Output file path
 *   format   - Image format (e.g., "png", "jpg") - can be NULL for auto-detect
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_to_file(const char *filename, const char *format);

/*
 * Function: daemon_render_get_buffer
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
void *daemon_render_get_buffer(int *width, int *height, int *bpp);

/*
 * Function: daemon_render_is_initialized
 * Check if daemon rendering is initialized.
 *
 * Returns:
 *   true if initialized, false otherwise
 */
bool daemon_render_is_initialized(void);

/*
 * Function: daemon_render_create_context
 * Create a new OSMesa context (for advanced usage).
 * Only available when compiled with OSMESA_RENDERING.
 *
 * Returns:
 *   OSMesa context handle, or NULL on error
 */
#ifdef OSMESA_RENDERING
OSMesaContext daemon_render_create_context(void);
#endif

/*
 * High-level rendering functions that integrate with Goxel's rendering system
 */

// Forward declarations for Goxel types - use existing includes
#include "../core/image.h"
#include "../camera.h"
#include "../core/layer.h" 
#include "../volume.h"
#include "../core/material.h"

/*
 * Function: daemon_render_scene_with_camera
 * Render a complete Goxel scene with camera setup.
 *
 * Parameters:
 *   image - Goxel image containing layers to render
 *   camera - Camera for viewing transformation
 *   background_color - Background color RGBA (can be NULL for default gray)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_scene_with_camera(const image_t *image, const camera_t *camera,
                                    const uint8_t background_color[4]);

/*
 * Function: daemon_render_layers
 * Render a specific set of layers.
 *
 * Parameters:
 *   layers - Linked list of layers to render
 *   camera - Camera for viewing transformation
 *   background_color - Background color RGBA (can be NULL for default gray)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_layers(const layer_t *layers, const camera_t *camera,
                          const uint8_t background_color[4]);

/*
 * Function: daemon_render_volume_direct
 * Render a single volume directly.
 *
 * Parameters:
 *   volume - Volume to render
 *   camera - Camera for viewing transformation
 *   material - Material properties (can be NULL for default)
 *   background_color - Background color RGBA (can be NULL for default gray)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int daemon_render_volume_direct(const volume_t *volume, const camera_t *camera,
                                 const material_t *material, const uint8_t background_color[4]);

#endif // RENDER_DAEMON_H