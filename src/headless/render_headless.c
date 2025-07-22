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

#include "render_headless.h"

#ifdef OSMESA_RENDERING
#include <GL/osmesa.h>
#endif

#ifdef EGL_RENDERING
#include <EGL/egl.h>
#endif

// Minimal logging macros for standalone use
#ifndef LOG_I
#define LOG_I(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_W
#define LOG_W(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_E
#define LOG_E(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// OpenGL headers
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

typedef struct {
#ifdef OSMESA_RENDERING
    OSMesaContext osmesa_context;
#endif
#ifdef EGL_RENDERING
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
#endif
    void *buffer;
    int width;
    int height;
    int bpp;           // bytes per pixel (typically 4 for RGBA)
    bool initialized;
    int backend;       // 0 = OSMesa, 1 = EGL, 2 = fallback
} headless_context_t;

static headless_context_t g_headless_ctx = {0};

int headless_render_init(int width, int height)
{
#ifdef OSMESA_RENDERING
    if (g_headless_ctx.initialized) {
        LOG_W("Headless render already initialized");
        return 0;
    }

    // Create OSMesa context for RGBA rendering
    g_headless_ctx.context = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!g_headless_ctx.context) {
        LOG_E("Failed to create OSMesa context");
        return -1;
    }

    g_headless_ctx.width = width;
    g_headless_ctx.height = height;
    g_headless_ctx.bpp = 4; // RGBA

    // Allocate framebuffer
    size_t buffer_size = width * height * g_headless_ctx.bpp;
    g_headless_ctx.buffer = malloc(buffer_size);
    if (!g_headless_ctx.buffer) {
        LOG_E("Failed to allocate framebuffer (%zu bytes)", buffer_size);
        OSMesaDestroyContext(g_headless_ctx.context);
        return -1;
    }

    // Make the context current
    if (!OSMesaMakeCurrent(g_headless_ctx.context, 
                          g_headless_ctx.buffer,
                          GL_UNSIGNED_BYTE,
                          width, height)) {
        LOG_E("Failed to make OSMesa context current");
        free(g_headless_ctx.buffer);
        OSMesaDestroyContext(g_headless_ctx.context);
        return -1;
    }

    g_headless_ctx.initialized = true;

    LOG_I("Headless rendering initialized: %dx%d", width, height);
    LOG_I("OSMesa version: %s", (char*)glGetString(GL_VERSION));
    LOG_I("OSMesa renderer: %s", (char*)glGetString(GL_RENDERER));

    return 0;
#else
    // For now, create a dummy buffer for testing
    g_headless_ctx.width = width;
    g_headless_ctx.height = height;
    g_headless_ctx.bpp = 4;
    g_headless_ctx.context = NULL;

    size_t buffer_size = width * height * g_headless_ctx.bpp;
    g_headless_ctx.buffer = malloc(buffer_size);
    if (!g_headless_ctx.buffer) {
        LOG_E("Failed to allocate framebuffer (%zu bytes)", buffer_size);
        return -1;
    }

    g_headless_ctx.initialized = true;
    LOG_I("Headless rendering initialized (fallback mode): %dx%d", width, height);
    return 0;
#endif
}

void headless_render_shutdown(void)
{
    if (!g_headless_ctx.initialized) {
        return;
    }

#ifdef OSMESA_RENDERING
    if (g_headless_ctx.context) {
        OSMesaDestroyContext(g_headless_ctx.context);
        g_headless_ctx.context = NULL;
    }
#endif

    if (g_headless_ctx.buffer) {
        free(g_headless_ctx.buffer);
        g_headless_ctx.buffer = NULL;
    }

    g_headless_ctx.initialized = false;
    LOG_I("Headless rendering shutdown");
}

int headless_render_resize(int width, int height)
{
#ifdef OSMESA_RENDERING
    if (!g_headless_ctx.initialized) {
        LOG_E("Headless render not initialized");
        return -1;
    }

    if (g_headless_ctx.width == width && g_headless_ctx.height == height) {
        return 0; // No resize needed
    }

    // Reallocate buffer
    size_t buffer_size = width * height * g_headless_ctx.bpp;
    void *new_buffer = realloc(g_headless_ctx.buffer, buffer_size);
    if (!new_buffer) {
        LOG_E("Failed to reallocate framebuffer (%zu bytes)", buffer_size);
        return -1;
    }

    g_headless_ctx.buffer = new_buffer;
    g_headless_ctx.width = width;
    g_headless_ctx.height = height;

    // Make context current with new buffer size
    if (!OSMesaMakeCurrent(g_headless_ctx.context,
                          g_headless_ctx.buffer,
                          GL_UNSIGNED_BYTE,
                          width, height)) {
        LOG_E("Failed to resize OSMesa context");
        return -1;
    }

    LOG_I("Headless rendering resized to: %dx%d", width, height);
    return 0;
#else
    return -1;
#endif
}

int headless_render_scene(void)
{
    if (!g_headless_ctx.initialized) {
        LOG_E("Headless render not initialized");
        return -1;
    }

    // Set viewport
    glViewport(0, 0, g_headless_ctx.width, g_headless_ctx.height);

    // Clear framebuffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return 0;
}

int headless_render_to_file(const char *filename, const char *format)
{
#ifdef OSMESA_RENDERING
    if (!g_headless_ctx.initialized || !g_headless_ctx.buffer) {
        LOG_E("Headless render not initialized or no buffer available");
        return -1;
    }

    if (!filename) {
        LOG_E("No filename provided");
        return -1;
    }

    // For now, assume PNG format
    // The actual image saving will be implemented using the existing img utilities
    LOG_I("Saving rendered image to: %s (format: %s)", 
          filename, format ? format : "png");

    // TODO: Implement actual image saving using img_save or similar
    // This would need to flip the Y coordinates since OpenGL and image formats
    // typically have different Y-axis orientations
    
    return 0;
#else
    return -1;
#endif
}

void *headless_render_get_buffer(int *width, int *height, int *bpp)
{
    if (!g_headless_ctx.initialized) {
        return NULL;
    }

    if (width) *width = g_headless_ctx.width;
    if (height) *height = g_headless_ctx.height;
    if (bpp) *bpp = g_headless_ctx.bpp;

    return g_headless_ctx.buffer;
}

bool headless_render_is_initialized(void)
{
    return g_headless_ctx.initialized;
}

#ifdef OSMESA_RENDERING
OSMesaContext headless_render_create_context(void)
{
    return OSMesaCreateContext(OSMESA_RGBA, NULL);
}
#endif