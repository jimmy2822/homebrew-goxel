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

#include "goxel.h"
#include "render_daemon.h"
#include "core/utils/img.h"

#ifdef HAVE_OSMESA
#include <GL/osmesa.h>
#endif

#ifdef DAEMON_SOFTWARE_FALLBACK
// Software fallback when OSMesa is not available
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
typedef void* OSMesaContext;
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
#include <stdbool.h>

// OpenGL headers
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

typedef struct {
#ifdef HAVE_OSMESA
    OSMesaContext osmesa_context;
#endif
    void *buffer;
    int width;
    int height;
    int bpp;           // bytes per pixel (typically 4 for RGBA)
    bool initialized;
    int backend;       // 0 = OSMesa, 2 = software fallback
} daemon_context_t;

static daemon_context_t g_daemon_ctx = {0};

int daemon_render_init(int width, int height)
{
    if (g_daemon_ctx.initialized) {
        LOG_W("Daemon render already initialized");
        return 0;
    }

    g_daemon_ctx.width = width;
    g_daemon_ctx.height = height;
    g_daemon_ctx.bpp = 4; // RGBA

    // Allocate framebuffer
    size_t buffer_size = width * height * g_daemon_ctx.bpp;
    g_daemon_ctx.buffer = malloc(buffer_size);
    if (!g_daemon_ctx.buffer) {
        LOG_E("Failed to allocate framebuffer (%zu bytes)", buffer_size);
        return -1;
    }

#ifdef HAVE_OSMESA
    // Try OSMesa first
    g_daemon_ctx.osmesa_context = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (g_daemon_ctx.osmesa_context) {
        // Make the context current
        if (OSMesaMakeCurrent(g_daemon_ctx.osmesa_context, 
                             g_daemon_ctx.buffer,
                             GL_UNSIGNED_BYTE,
                             width, height)) {
            g_daemon_ctx.backend = 0; // OSMesa
            g_daemon_ctx.initialized = true;
            
            LOG_I("Daemon rendering initialized with OSMesa: %dx%d", width, height);
            LOG_I("OSMesa version: %s", (char*)glGetString(GL_VERSION));
            LOG_I("OSMesa renderer: %s", (char*)glGetString(GL_RENDERER));
            return 0;
        } else {
            LOG_W("Failed to make OSMesa context current, falling back to software mode");
            OSMesaDestroyContext(g_daemon_ctx.osmesa_context);
            g_daemon_ctx.osmesa_context = NULL;
        }
    } else {
        LOG_W("Failed to create OSMesa context, falling back to software mode");
    }
#endif


    // Software fallback mode
    g_daemon_ctx.backend = 2; // Software fallback
    g_daemon_ctx.initialized = true;
    
    // Initialize buffer with test pattern for software fallback
    uint8_t *buf = (uint8_t*)g_daemon_ctx.buffer;
    for (int i = 0; i < width * height; i++) {
        buf[i * 4 + 0] = 128; // R
        buf[i * 4 + 1] = 128; // G  
        buf[i * 4 + 2] = 128; // B
        buf[i * 4 + 3] = 255; // A
    }
    
    LOG_I("Daemon rendering initialized (software fallback): %dx%d", width, height);
    LOG_W("OSMesa not available - rendering will use software fallback");
    
    return 0;
}

void daemon_render_shutdown(void)
{
    if (!g_daemon_ctx.initialized) {
        return;
    }

#ifdef HAVE_OSMESA
    if (g_daemon_ctx.osmesa_context) {
        OSMesaDestroyContext(g_daemon_ctx.osmesa_context);
        g_daemon_ctx.osmesa_context = NULL;
    }
#endif


    if (g_daemon_ctx.buffer) {
        free(g_daemon_ctx.buffer);
        g_daemon_ctx.buffer = NULL;
    }

    g_daemon_ctx.initialized = false;
    LOG_I("Daemon rendering shutdown");
}

int daemon_render_resize(int width, int height)
{
    if (!g_daemon_ctx.initialized) {
        LOG_E("Daemon render not initialized");
        return -1;
    }

    if (g_daemon_ctx.width == width && g_daemon_ctx.height == height) {
        return 0; // No resize needed
    }

    // Reallocate buffer
    size_t buffer_size = width * height * g_daemon_ctx.bpp;
    void *new_buffer = realloc(g_daemon_ctx.buffer, buffer_size);
    if (!new_buffer) {
        LOG_E("Failed to reallocate framebuffer (%zu bytes)", buffer_size);
        return -1;
    }

    g_daemon_ctx.buffer = new_buffer;
    g_daemon_ctx.width = width;
    g_daemon_ctx.height = height;

#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        // Make context current with new buffer size
        if (!OSMesaMakeCurrent(g_daemon_ctx.osmesa_context,
                              g_daemon_ctx.buffer,
                              GL_UNSIGNED_BYTE,
                              width, height)) {
            LOG_E("Failed to resize OSMesa context");
            return -1;
        }
    }
#endif


    if (g_daemon_ctx.backend == 2) {
        // Software fallback - just clear the buffer
        memset(g_daemon_ctx.buffer, 0, buffer_size);
    }

    LOG_I("Daemon rendering resized to: %dx%d", width, height);
    return 0;
}

int daemon_render_scene(void)
{
    if (!g_daemon_ctx.initialized) {
        LOG_E("Daemon render not initialized");
        return -1;
    }

    // Set viewport
    glViewport(0, 0, g_daemon_ctx.width, g_daemon_ctx.height);

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

int daemon_render_to_file(const char *filename, const char *format)
{
    if (!g_daemon_ctx.initialized || !g_daemon_ctx.buffer) {
        LOG_E("Daemon render not initialized or no buffer available");
        return -1;
    }

    if (!filename) {
        LOG_E("No filename provided");
        return -1;
    }

    // Use Goxel's image utilities to save the framebuffer
    uint8_t *flipped_buffer = NULL;
    int w = g_daemon_ctx.width;
    int h = g_daemon_ctx.height;
    int bpp = g_daemon_ctx.bpp;
    
    // OpenGL renders with Y-axis flipped compared to image formats
    // Create a flipped copy of the buffer
    flipped_buffer = malloc(w * h * bpp);
    if (!flipped_buffer) {
        LOG_E("Failed to allocate buffer for image saving");
        return -1;
    }
    
    {
        uint8_t *src = (uint8_t*)g_daemon_ctx.buffer;
        for (int y = 0; y < h; y++) {
            memcpy(flipped_buffer + y * w * bpp, 
                   src + (h - 1 - y) * w * bpp, 
                   w * bpp);
        }
    }
    
    // Use Goxel's img_write function
    img_write(flipped_buffer, w, h, bpp, filename);
    int result = 0;
    
    free(flipped_buffer);
    
    if (result) {
        LOG_E("Failed to save image to: %s", filename);
        return -1;
    }
    
    LOG_I("Successfully saved rendered image to: %s", filename);
    return 0;
}

void *daemon_render_get_buffer(int *width, int *height, int *bpp)
{
    if (!g_daemon_ctx.initialized) {
        return NULL;
    }

    if (width) *width = g_daemon_ctx.width;
    if (height) *height = g_daemon_ctx.height;
    if (bpp) *bpp = g_daemon_ctx.bpp;

    return g_daemon_ctx.buffer;
}

bool daemon_render_is_initialized(void)
{
    return g_daemon_ctx.initialized;
}

#ifdef HAVE_OSMESA
OSMesaContext daemon_render_create_context(void)
{
    return OSMesaCreateContext(OSMESA_RGBA, NULL);
}
#endif

/*
 * High-level rendering functions that integrate with Goxel's rendering system
 */

int daemon_render_scene_with_camera(const image_t *image, const camera_t *camera,
                                    const uint8_t background_color[4])
{
    if (!g_daemon_ctx.initialized) {
        LOG_E("Daemon render not initialized");
        return -1;
    }

    if (!image || !camera) {
        LOG_E("Invalid image or camera");
        return -1;
    }
    
    // For software fallback mode, render voxels as simple 2D projection
    if (g_daemon_ctx.backend == 2) {
        uint8_t *buf = (uint8_t*)g_daemon_ctx.buffer;
        int w = g_daemon_ctx.width;
        int h = g_daemon_ctx.height;
        
        // Clear background to dark gray for better contrast
        uint8_t bg_color[4] = {64, 64, 64, 255};
        if (background_color) {
            memcpy(bg_color, background_color, 4);
        }
        
        for (int i = 0; i < w * h; i++) {
            buf[i * 4 + 0] = bg_color[0];
            buf[i * 4 + 1] = bg_color[1];
            buf[i * 4 + 2] = bg_color[2];
            buf[i * 4 + 3] = bg_color[3];
        }
        
        // Simple voxel rendering: iterate through all visible layers and voxels
        const layer_t *layer;
        
        for (layer = goxel_get_render_layers(true); layer; layer = layer->next) {
            if (!layer->visible || !layer->volume) continue;
            
            
            // Simple voxel iteration - project each voxel to screen
            int bbox[2][3];
            volume_get_bbox(layer->volume, bbox, true);
            
            for (int z = bbox[0][2]; z < bbox[1][2]; z++) {
                for (int y = bbox[0][1]; y < bbox[1][1]; y++) {
                    for (int x = bbox[0][0]; x < bbox[1][0]; x++) {
                        int pos[3] = {x, y, z};
                        uint8_t voxel[4];
                        volume_get_at(layer->volume, NULL, pos, voxel);
                        if (voxel[3] > 0) {
                            
                            // Simple orthogonal projection: X->screen_x, Z->screen_y (top-down view)
                            // Center the projection and add some offset
                            int center_x = (bbox[0][0] + bbox[1][0]) / 2;
                            int center_z = (bbox[0][2] + bbox[1][2]) / 2;
                            int screen_x = w/2 + (x - center_x) * 12;  // Increased scale
                            int screen_y = h/2 + (z - center_z) * 12;  // Increased scale
                            
                            // Draw a 8x8 pixel square for each voxel (larger for visibility)
                            for (int dy = 0; dy < 8; dy++) {
                                for (int dx = 0; dx < 8; dx++) {
                                    int px = screen_x + dx;
                                    int py = screen_y + dy;
                                    
                                    if (px >= 0 && px < w && py >= 0 && py < h) {
                                        int idx = (py * w + px) * 4;
                                        buf[idx + 0] = voxel[0]; // R
                                        buf[idx + 1] = voxel[1]; // G
                                        buf[idx + 2] = voxel[2]; // B
                                        buf[idx + 3] = 255;     // A
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        return 0;
    }

    // Prepare daemon rendering setup
    if (daemon_render_scene() != 0) {
        LOG_E("daemon_render_scene failed");
        return -1;
    }

    // Set up viewport
    float viewport[4] = {0, 0, g_daemon_ctx.width, g_daemon_ctx.height};

    // Create renderer
    renderer_t rend = goxel.rend;
    rend.view_mat[0][0] = 1; // Initialize view matrix
    rend.fbo = 0; // Use default framebuffer
    rend.scale = 1.0;
    rend.items = NULL;

    // Set camera matrices
    camera_t *cam = (camera_t*)camera; // Remove const for camera_update
    cam->aspect = (float)g_daemon_ctx.width / g_daemon_ctx.height;
    camera_update(cam);
    
    mat4_copy(cam->view_mat, rend.view_mat);
    mat4_copy(cam->proj_mat, rend.proj_mat);

    // Render all visible layers
    const layer_t *layer;
    for (layer = goxel_get_render_layers(true); layer; layer = layer->next) {
        if (layer->visible && layer->volume) {
            render_volume(&rend, layer->volume, layer->material, 0);
        }
    }

    // Submit the rendering
    uint8_t clear_color[4] = {128, 128, 128, 255}; // Default gray background
    if (background_color) {
        memcpy(clear_color, background_color, 4);
    }
    
    render_submit(&rend, viewport, clear_color);

    return 0;
}

int daemon_render_layers(const layer_t *layers, const camera_t *camera,
                          const uint8_t background_color[4])
{
    if (!g_daemon_ctx.initialized) {
        LOG_E("Daemon render not initialized");
        return -1;
    }

    if (!layers || !camera) {
        LOG_E("Invalid layers or camera");
        return -1;
    }

    // Prepare daemon rendering setup
    if (daemon_render_scene() != 0) {
        return -1;
    }

    // Set up viewport
    float viewport[4] = {0, 0, g_daemon_ctx.width, g_daemon_ctx.height};

    // Create renderer
    renderer_t rend = goxel.rend;
    rend.fbo = 0;
    rend.scale = 1.0;
    rend.items = NULL;

    // Set camera matrices
    camera_t *cam = (camera_t*)camera;
    cam->aspect = (float)g_daemon_ctx.width / g_daemon_ctx.height;
    camera_update(cam);
    
    mat4_copy(cam->view_mat, rend.view_mat);
    mat4_copy(cam->proj_mat, rend.proj_mat);

    // Render specified layers
    const layer_t *layer;
    for (layer = layers; layer; layer = layer->next) {
        if (layer->visible && layer->volume) {
            render_volume(&rend, layer->volume, layer->material, 0);
        }
    }

    // Submit the rendering
    uint8_t clear_color[4] = {128, 128, 128, 255};
    if (background_color) {
        memcpy(clear_color, background_color, 4);
    }
    
    render_submit(&rend, viewport, clear_color);

    return 0;
}

int daemon_render_volume_direct(const volume_t *volume, const camera_t *camera,
                                 const material_t *material, const uint8_t background_color[4])
{
    if (!g_daemon_ctx.initialized) {
        LOG_E("Daemon render not initialized");
        return -1;
    }

    if (!volume || !camera) {
        LOG_E("Invalid volume or camera");
        return -1;
    }

    // Prepare daemon rendering setup
    if (daemon_render_scene() != 0) {
        return -1;
    }

    // Set up viewport
    float viewport[4] = {0, 0, g_daemon_ctx.width, g_daemon_ctx.height};

    // Create renderer
    renderer_t rend = goxel.rend;
    rend.fbo = 0;
    rend.scale = 1.0;
    rend.items = NULL;

    // Set camera matrices
    camera_t *cam = (camera_t*)camera;
    cam->aspect = (float)g_daemon_ctx.width / g_daemon_ctx.height;
    camera_update(cam);
    
    mat4_copy(cam->view_mat, rend.view_mat);
    mat4_copy(cam->proj_mat, rend.proj_mat);

    // Render the volume
    render_volume(&rend, volume, material, 0);

    // Submit the rendering
    uint8_t clear_color[4] = {128, 128, 128, 255};
    if (background_color) {
        memcpy(clear_color, background_color, 4);
    }
    
    render_submit(&rend, viewport, clear_color);

    return 0;
}