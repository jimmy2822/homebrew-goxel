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
            
            // Initialize OpenGL buffers and resources after OSMesa context is ready
            render_init();
            LOG_I("OpenGL rendering resources initialized");
            
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
    LOG_D("Set viewport: %dx%d", g_daemon_ctx.width, g_daemon_ctx.height);

    // Don't clear here - let render_submit handle the background
    // The clearing will be done by render_background() with the proper color
    
    // Check for OpenGL errors
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        LOG_E("OpenGL error in daemon_render_scene: 0x%04X", gl_error);
        return -1;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LOG_D("OpenGL state configured successfully");
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

#ifdef HAVE_OSMESA
    // CRITICAL DEBUG: Force OSMesa to flush and read the buffer
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        LOG_I("=== FRAMEBUFFER BUFFER DEBUG ===");
        
        // Ensure all OpenGL commands are finished
        glFinish();
        
        // Get the actual OSMesa color buffer
        GLint width, height, format;
        void *osmesa_buffer;
        if (OSMesaGetColorBuffer(g_daemon_ctx.osmesa_context, &width, &height, &format, &osmesa_buffer)) {
            LOG_I("OSMesa buffer: %dx%d, format=%d, buffer=%p", width, height, format, osmesa_buffer);
            LOG_I("Our buffer: %dx%d, buffer=%p", g_daemon_ctx.width, g_daemon_ctx.height, g_daemon_ctx.buffer);
            
            // Decode OSMesa format for color channel order debugging
            const char *format_name = "UNKNOWN";
            switch (format) {
                case 0x1907: format_name = "RGB (OSMESA_RGB)"; break;     // GL_RGB
                case 0x1908: format_name = "RGBA (OSMESA_RGBA)"; break;   // GL_RGBA  
                case 0x80E1: format_name = "BGRA (OSMESA_BGRA)"; break;   // GL_BGRA
                case 0x1909: format_name = "LUMINANCE"; break;            // GL_LUMINANCE
                default: format_name = "OTHER"; break;
            }
            LOG_I("OSMesa COLOR FORMAT: %s (0x%04X) - This determines byte order!", format_name, format);
            
            // Check if buffers match
            if (osmesa_buffer != g_daemon_ctx.buffer) {
                LOG_W("OSMesa buffer (%p) differs from our buffer (%p)! Using OSMesa buffer.", 
                      osmesa_buffer, g_daemon_ctx.buffer);
                // Update our buffer pointer
                g_daemon_ctx.buffer = osmesa_buffer;
            }
        } else {
            LOG_E("Failed to get OSMesa color buffer!");
        }
        
        // ENHANCED buffer analysis
        uint8_t *buf = (uint8_t*)g_daemon_ctx.buffer;
        int total_pixels = g_daemon_ctx.width * g_daemon_ctx.height;
        int non_zero_pixels = 0;
        int red_pixels = 0;
        int unique_colors = 0;
        
        // Track unique colors (up to 100)
        typedef struct { uint8_t r, g, b, a; int count; } color_count_t;
        color_count_t colors[100] = {0};
        
        for (int i = 0; i < total_pixels; i++) {
            uint8_t r = buf[i * 4 + 0];
            uint8_t g = buf[i * 4 + 1]; 
            uint8_t b = buf[i * 4 + 2];
            uint8_t a = buf[i * 4 + 3];
            
            if (r != 0 || g != 0 || b != 0 || a != 0) {
                non_zero_pixels++;
                
                // Count red pixels (our test voxels)
                if (r > 200 && g < 50 && b < 50) {
                    red_pixels++;
                }
                
                // Track unique colors
                bool found = false;
                for (int j = 0; j < unique_colors && j < 100; j++) {
                    if (colors[j].r == r && colors[j].g == g && colors[j].b == b && colors[j].a == a) {
                        colors[j].count++;
                        found = true;
                        break;
                    }
                }
                if (!found && unique_colors < 100) {
                    colors[unique_colors] = (color_count_t){r, g, b, a, 1};
                    unique_colors++;
                }
            }
        }
        
        LOG_I("BUFFER ANALYSIS:");
        LOG_I("  Total pixels: %d", total_pixels);
        LOG_I("  Non-zero pixels: %d (%.1f%%)", non_zero_pixels, 100.0 * non_zero_pixels / total_pixels);
        LOG_I("  Red pixels (test voxels): %d", red_pixels);
        LOG_I("  Unique colors: %d", unique_colors);
        
        // Log the most common colors
        for (int i = 0; i < unique_colors && i < 10; i++) {
            LOG_I("    Color %d: RGBA=(%d,%d,%d,%d) count=%d", i, 
                  colors[i].r, colors[i].g, colors[i].b, colors[i].a, colors[i].count);
        }
        
        // Sample corners and center
        int center_x = g_daemon_ctx.width / 2;
        int center_y = g_daemon_ctx.height / 2;
        int center_idx = (center_y * g_daemon_ctx.width + center_x) * 4;
        LOG_I("Center pixel (%d,%d): RGBA=(%d,%d,%d,%d)", center_x, center_y,
              buf[center_idx], buf[center_idx+1], buf[center_idx+2], buf[center_idx+3]);
              
        int corner_idx = 0;
        LOG_I("Top-left corner: RGBA=(%d,%d,%d,%d)", 
              buf[corner_idx], buf[corner_idx+1], buf[corner_idx+2], buf[corner_idx+3]);
              
        // COLOR CHANNEL ORDER DIAGNOSTIC
        // Look for red pixels (255, 0, 0) and see which byte order they appear as
        bool found_red_test = false;
        // Removed unused red_positions array to fix compilation warning
        
        for (int i = 0; i < total_pixels && !found_red_test; i++) {
            uint8_t b0 = buf[i * 4 + 0];
            uint8_t b1 = buf[i * 4 + 1]; 
            uint8_t b2 = buf[i * 4 + 2];
            uint8_t b3 = buf[i * 4 + 3];
            
            // Look for a pixel with one dominant red channel and low other channels
            if ((b0 > 200 && b1 < 50 && b2 < 50) || 
                (b1 > 200 && b0 < 50 && b2 < 50) ||
                (b2 > 200 && b0 < 50 && b1 < 50)) {
                found_red_test = true;
                // Direct values (b0, b1, b2, b3) are used in logging below
                
                LOG_I("FOUND RED-ISH PIXEL [%d,%d,%d,%d] at pixel index %d", b0, b1, b2, b3, i);
                
                if (b0 > 200 && b1 < 50 && b2 < 50) {
                    LOG_I("  → RED in byte 0: FORMAT IS RGBA (CORRECT)");
                } else if (b1 > 200 && b0 < 50 && b2 < 50) {
                    LOG_I("  → RED in byte 1: UNUSUAL FORMAT");
                } else if (b2 > 200 && b0 < 50 && b1 < 50) {
                    LOG_I("  → RED in byte 2: FORMAT IS BGRA (BYTE ORDER ISSUE!)");
                }
                break;
            }
        }
        
        if (!found_red_test) {
            LOG_I("No red test pixel found - this could indicate rendering issues");
        }
        
        LOG_I("=== FRAMEBUFFER BUFFER DEBUG END ===");
    }
#endif

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
    
    // Check if we need BGRA->RGBA conversion
    bool need_bgra_conversion = false;
#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        GLint mesa_w, mesa_h, mesa_format;
        void *mesa_buf;
        if (OSMesaGetColorBuffer(g_daemon_ctx.osmesa_context, &mesa_w, &mesa_h, &mesa_format, &mesa_buf)) {
            if (mesa_format == 0x80E1) { // GL_BGRA
                need_bgra_conversion = true;
                LOG_I("OSMesa using BGRA format - will convert to RGBA for PNG");
            } else if (mesa_format == 0x1908) { // GL_RGBA
                LOG_I("OSMesa using RGBA format - no conversion needed");
            }
        }
    }
#endif
    
    {
        uint8_t *src = (uint8_t*)g_daemon_ctx.buffer;
        for (int y = 0; y < h; y++) {
            if (need_bgra_conversion && bpp == 4) {
                // Convert BGRA to RGBA while flipping Y
                uint8_t *src_row = src + (h - 1 - y) * w * bpp;
                uint8_t *dst_row = flipped_buffer + y * w * bpp;
                for (int x = 0; x < w; x++) {
                    dst_row[x * 4 + 0] = src_row[x * 4 + 2]; // R = B
                    dst_row[x * 4 + 1] = src_row[x * 4 + 1]; // G = G  
                    dst_row[x * 4 + 2] = src_row[x * 4 + 0]; // B = R
                    dst_row[x * 4 + 3] = src_row[x * 4 + 3]; // A = A
                }
                LOG_D("Row %d: BGRA->RGBA conversion applied", y);
            } else {
                // Normal Y-flip copy
                memcpy(flipped_buffer + y * w * bpp, 
                       src + (h - 1 - y) * w * bpp, 
                       w * bpp);
            }
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
        
        for (layer = image->layers; layer; layer = layer->next) {
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

    // Create renderer with proper initialization
    renderer_t rend = {0};
    
    // Initialize renderer settings from goxel defaults
    rend.settings = goxel.rend.settings;
    rend.fbo = 0; // Use default framebuffer
    rend.scale = 1.0;
    rend.items = NULL;
    
    // Initialize view matrix to identity
    mat4_set_identity(rend.view_mat);
    mat4_set_identity(rend.proj_mat);

    // DEBUG: Print image bounding box information
    LOG_I("=== RENDERING DEBUG INFO ===");
    LOG_I("Image box: [%.1f,%.1f,%.1f] to [%.1f,%.1f,%.1f]", 
          image->box[0][0], image->box[0][1], image->box[0][2],
          image->box[1][0], image->box[1][1], image->box[1][2]);
    
    // Count total voxels for debugging
    int total_voxels = 0;
    const layer_t *debug_layer;
    for (debug_layer = image->layers; debug_layer; debug_layer = debug_layer->next) {
        if (debug_layer->visible && debug_layer->volume) {
            int bbox[2][3];
            volume_get_bbox(debug_layer->volume, bbox, true);
            LOG_I("Layer: visible=%d, bbox=[%d,%d,%d] to [%d,%d,%d]",
                  debug_layer->visible, bbox[0][0], bbox[0][1], bbox[0][2],
                  bbox[1][0], bbox[1][1], bbox[1][2]);
            
            // Count voxels in this layer
            for (int z = bbox[0][2]; z < bbox[1][2]; z++) {
                for (int y = bbox[0][1]; y < bbox[1][1]; y++) {
                    for (int x = bbox[0][0]; x < bbox[1][0]; x++) {
                        int pos[3] = {x, y, z};
                        uint8_t voxel[4];
                        volume_get_at(debug_layer->volume, NULL, pos, voxel);
                        if (voxel[3] > 0) total_voxels++;
                    }
                }
            }
        }
    }
    LOG_I("Total voxels found: %d", total_voxels);

    // Set camera matrices
    camera_t *cam = (camera_t*)camera; // Remove const for camera_update
    cam->aspect = (float)g_daemon_ctx.width / g_daemon_ctx.height;
    
    // DEBUG: Print camera matrix BEFORE update
    LOG_I("Camera matrix BEFORE update: [%.2f,%.2f,%.2f,%.2f]", 
          cam->mat[3][0], cam->mat[3][1], cam->mat[3][2], cam->mat[3][3]);
    LOG_I("Camera rotation components: m[0][0]=%.2f, m[1][1]=%.2f, m[2][2]=%.2f", 
          cam->mat[0][0], cam->mat[1][1], cam->mat[2][2]);
    
    camera_update(cam);
    
    // DEBUG: Print camera information AFTER update
    LOG_I("Camera aspect: %.2f (%.0fx%.0f)", cam->aspect, 
          (float)g_daemon_ctx.width, (float)g_daemon_ctx.height);
    LOG_I("Camera matrix AFTER update: [%.2f, %.2f, %.2f]", 
          cam->mat[3][0], cam->mat[3][1], cam->mat[3][2]);
    LOG_I("Camera distance: %.2f, ortho: %d, fovy: %.2f", cam->dist, cam->ortho, cam->fovy);
    LOG_I("View matrix: [%.2f,%.2f,%.2f,%.2f]", 
          cam->view_mat[3][0], cam->view_mat[3][1], cam->view_mat[3][2], cam->view_mat[3][3]);
    
    mat4_copy(cam->view_mat, rend.view_mat);
    mat4_copy(cam->proj_mat, rend.proj_mat);

    // Render all visible layers
    const layer_t *layer;
    int layer_count = 0;
    for (layer = image->layers; layer; layer = layer->next) {
        layer_count++;
        LOG_I("Processing layer %d: visible=%d, volume=%p", 
              layer_count, layer->visible, layer->volume);
        if (layer->visible && layer->volume) {
            // Get bounding box for this layer
            int layer_bbox[2][3];
            volume_get_bbox(layer->volume, layer_bbox, true);
            LOG_I("Layer %d bbox: [%d,%d,%d] to [%d,%d,%d]", layer_count,
                  layer_bbox[0][0], layer_bbox[0][1], layer_bbox[0][2],
                  layer_bbox[1][0], layer_bbox[1][1], layer_bbox[1][2]);
            
            LOG_I("Calling render_volume() for layer %d", layer_count);
            
            // Check if render items exist before render_volume
            bool had_items_before = (rend.items != NULL);
            LOG_I("Render items before render_volume: %s", had_items_before ? "present" : "empty");
            
            render_volume(&rend, layer->volume, layer->material, 0);
            
            bool has_items_after = (rend.items != NULL);
            LOG_I("Render items after render_volume: %s", has_items_after ? "present" : "empty");
            
            // Check if any items were actually added
            if (has_items_after) {
                if (!had_items_before) {
                    LOG_I("✓ render_volume successfully added render items (first items)");
                } else {
                    LOG_I("✓ render_volume called (items may have been added to existing list)");
                }
            } else {
                LOG_E("✗ render_volume failed to add any render items!");
            }
            
            LOG_I("render_volume() completed for layer %d", layer_count);
        }
    }

    // Submit the rendering
    uint8_t clear_color[4] = {128, 128, 128, 255}; // Default gray background
    if (background_color) {
        memcpy(clear_color, background_color, 4);
        LOG_I("Using custom background color: [%d,%d,%d,%d]",
              background_color[0], background_color[1], background_color[2], background_color[3]);
    } else {
        LOG_I("Using default background color: [%d,%d,%d,%d]",
              clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    }
    
    LOG_I("Calling render_submit() with viewport [%.0f,%.0f,%.0f,%.0f]",
          viewport[0], viewport[1], viewport[2], viewport[3]);
    
    // DEBUG: Check if items exist for submit
    bool has_final_items = (rend.items != NULL);
    LOG_I("Final render items for submit: %s", has_final_items ? "present" : "empty");
    
    if (!has_final_items) {
        LOG_E("CRITICAL: No render items to submit! This will result in blank render.");
        LOG_I("This explains why output_path mode produces empty images!");
    } else {
        LOG_I("✓ Render items exist - should produce visible output");
    }
    
    // Check OpenGL state before submit
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        LOG_E("OpenGL error before render_submit: 0x%04X", gl_error);
    }
    
    LOG_I("About to call render_submit with items=%s and clear color [%d,%d,%d,%d]",
          has_final_items ? "present" : "empty", clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    
    // CRITICAL FIX: Ensure OSMesa context is current before OpenGL operations
    // This prevents "Assertion failed: (vertex_shader), function gl_shader_create" errors
#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        if (!OSMesaMakeCurrent(g_daemon_ctx.osmesa_context,
                              g_daemon_ctx.buffer,
                              GL_UNSIGNED_BYTE,
                              g_daemon_ctx.width, g_daemon_ctx.height)) {
            LOG_E("CRITICAL: Failed to make OSMesa context current before rendering!");
            LOG_E("This will cause shader creation to fail with assertion errors.");
            return -1;
        }
        LOG_D("OSMesa context made current before render_submit()");
    }
#endif
    
    render_submit(&rend, viewport, clear_color);
    
    // Check OpenGL state after submit
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        LOG_E("OpenGL error after render_submit: 0x%04X", gl_error);
    }
    
    // CRITICAL FIX: Force OpenGL to complete all rendering operations
    // This ensures the OSMesa buffer is fully updated before we read it
    glFinish();
    LOG_I("glFinish() called to ensure rendering is complete");
    
    // For OSMesa, we need to ensure the buffer pointer is correct
#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        GLint width, height, format;
        void *osmesa_buffer;
        if (OSMesaGetColorBuffer(g_daemon_ctx.osmesa_context, &width, &height, &format, &osmesa_buffer)) {
            if (osmesa_buffer != g_daemon_ctx.buffer) {
                LOG_W("Updating buffer pointer from OSMesa: %p -> %p", g_daemon_ctx.buffer, osmesa_buffer);
                g_daemon_ctx.buffer = osmesa_buffer;
            }
        }
    }
#endif
    
    LOG_I("render_submit() completed");
    LOG_I("=== RENDERING DEBUG COMPLETE ===");

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
    
    // CRITICAL FIX: Ensure OSMesa context is current before OpenGL operations
#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        if (!OSMesaMakeCurrent(g_daemon_ctx.osmesa_context,
                              g_daemon_ctx.buffer,
                              GL_UNSIGNED_BYTE,
                              g_daemon_ctx.width, g_daemon_ctx.height)) {
            LOG_E("CRITICAL: Failed to make OSMesa context current before rendering!");
            return -1;
        }
        LOG_D("OSMesa context made current before render_submit() in daemon_render_layers");
    }
#endif
    
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
    
    // CRITICAL FIX: Ensure OSMesa context is current before OpenGL operations
#ifdef HAVE_OSMESA
    if (g_daemon_ctx.backend == 0 && g_daemon_ctx.osmesa_context) {
        if (!OSMesaMakeCurrent(g_daemon_ctx.osmesa_context,
                              g_daemon_ctx.buffer,
                              GL_UNSIGNED_BYTE,
                              g_daemon_ctx.width, g_daemon_ctx.height)) {
            LOG_E("CRITICAL: Failed to make OSMesa context current before rendering!");
            return -1;
        }
        LOG_D("OSMesa context made current before render_submit() in daemon_render_volume_direct");
    }
#endif
    
    render_submit(&rend, viewport, clear_color);

    return 0;
}