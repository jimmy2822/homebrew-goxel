
#include "../../include/goxel_daemon.h"
#include <stdlib.h>
#include <string.h>

struct goxel_context { int placeholder; };

goxel_context_t *goxel_create_context(void) {
    return calloc(1, sizeof(struct goxel_context));
}

goxel_error_t goxel_init_context(goxel_context_t *ctx) {
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

void goxel_destroy_context(goxel_context_t *ctx) {
    if (ctx) free(ctx);
}

goxel_error_t goxel_create_project(goxel_context_t *ctx, const char *name, 
                                   int width, int height, int depth) {
    (void)name; (void)width; (void)height; (void)depth;
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

goxel_error_t goxel_add_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              const goxel_color_t *color) {
    (void)x; (void)y; (void)z; (void)color;
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

const char *goxel_get_error_string(goxel_error_t error) {
    switch(error) {
        case GOXEL_SUCCESS: return "Success";
        case GOXEL_ERROR_INVALID_CONTEXT: return "Invalid context";
        default: return "Unknown error";
    }
}

const char *goxel_get_version(int *major, int *minor, int *patch) {
    if (major) *major = 0;
    if (minor) *minor = 17;
    if (patch) *patch = 2;
    return "0.17.2";
}

bool goxel_has_feature(const char *feature) {
    (void)feature;
    return false;
}

// Stub implementations for all other API functions
goxel_error_t goxel_load_project(goxel_context_t *ctx, const char *path) { (void)ctx; (void)path; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_save_project(goxel_context_t *ctx, const char *path) { (void)ctx; (void)path; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_save_project_format(goxel_context_t *ctx, const char *path, const char *format) { (void)ctx; (void)path; (void)format; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_close_project(goxel_context_t *ctx) { (void)ctx; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_project_bounds(goxel_context_t *ctx, int *width, int *height, int *depth) { (void)ctx; (void)width; (void)height; (void)depth; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_remove_voxel(goxel_context_t *ctx, int x, int y, int z) { (void)ctx; (void)x; (void)y; (void)z; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_voxel(goxel_context_t *ctx, int x, int y, int z, goxel_color_t *color) { (void)ctx; (void)x; (void)y; (void)z; (void)color; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_add_voxel_batch(goxel_context_t *ctx, const goxel_pos_t *positions, const goxel_color_t *colors, size_t count) { (void)ctx; (void)positions; (void)colors; (void)count; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_remove_voxels_in_box(goxel_context_t *ctx, const goxel_box_t *box) { (void)ctx; (void)box; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_paint_voxel(goxel_context_t *ctx, int x, int y, int z, const goxel_color_t *color) { (void)ctx; (void)x; (void)y; (void)z; (void)color; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_create_layer(goxel_context_t *ctx, const char *name, const goxel_color_t *color, bool visible, goxel_layer_id_t *layer_id) { (void)ctx; (void)name; (void)color; (void)visible; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_delete_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_set_active_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_active_layer(goxel_context_t *ctx, goxel_layer_id_t *layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_set_layer_visibility(goxel_context_t *ctx, goxel_layer_id_t layer_id, bool visible) { (void)ctx; (void)layer_id; (void)visible; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_layer_count(goxel_context_t *ctx, int *count) { (void)ctx; (void)count; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_render_to_file(goxel_context_t *ctx, const char *output_path, const goxel_render_options_t *options) { (void)ctx; (void)output_path; (void)options; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_render_to_buffer(goxel_context_t *ctx, uint8_t **buffer, size_t *buffer_size, const goxel_render_options_t *options) { (void)ctx; (void)buffer; (void)buffer_size; (void)options; return GOXEL_ERROR_INVALID_OPERATION; }
const char *goxel_get_last_error(goxel_context_t *ctx) { (void)ctx; return NULL; }
goxel_error_t goxel_get_memory_usage(goxel_context_t *ctx, size_t *bytes_used, size_t *bytes_allocated) { (void)ctx; (void)bytes_used; (void)bytes_allocated; return GOXEL_ERROR_INVALID_OPERATION; }
