/* Goxel 3D voxels editor - Headless Stubs for GUI Builds
 *
 * This file provides stub implementations of headless rendering functions
 * for GUI builds where headless functionality is not available.
 */

#include <stddef.h>
#include <stdbool.h>
#include "render_headless.h"
#include "../log.h"

// Include core types for function signatures
#include "../core/image.h"
#include "../camera.h"
#include "../volume.h"
#include "../layer.h"
#include "../material.h"

#ifndef GOXEL_HEADLESS

// Define OSMesaContext for GUI builds where OSMesa is not available
typedef void* OSMesaContext;

// Stub implementations for GUI builds
int headless_render_init(int width, int height)
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

void headless_render_shutdown(void)
{
    // No-op for GUI builds
}

int headless_render_resize(int width, int height)
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

int headless_render_scene(void)
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

int headless_render_to_file(const char *filename, const char *format)
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

void *headless_render_get_buffer(int *width, int *height, int *bpp)
{
    LOG_W("Headless rendering not available in GUI mode");
    if (width) *width = 0;
    if (height) *height = 0;
    if (bpp) *bpp = 0;
    return NULL;
}

bool headless_render_is_initialized(void)
{
    return false;
}

OSMesaContext headless_render_create_context(void)
{
    LOG_W("OSMesa context not available in GUI mode");
    return NULL;
}

int headless_render_scene_with_camera(const image_t *image, const camera_t *camera,
                                      const uint8_t background_color[4])
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

int headless_render_layers(const layer_t *layers, const camera_t *camera,
                          const uint8_t background_color[4])
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

int headless_render_volume_direct(const volume_t *volume, const camera_t *camera,
                                 const material_t *material, const uint8_t background_color[4])
{
    LOG_W("Headless rendering not available in GUI mode");
    return -1;
}

#endif // !GOXEL_HEADLESS