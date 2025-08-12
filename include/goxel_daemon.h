/* Goxel Daemon API - Public Interface
 *
 * Copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
 * 
 * This file provides the public C API for the Goxel Daemon library,
 * enabling programmatic access to voxel editing operations without GUI dependencies.
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

#ifndef GOXEL_DAEMON_H
#define GOXEL_DAEMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// VERSION AND BUILD INFORMATION
// ============================================================================

#define GOXEL_VERSION_MAJOR 0
#define GOXEL_VERSION_MINOR 16
#define GOXEL_VERSION_PATCH 3
#define GOXEL_VERSION_STRING "0.16.3"
#define GOXEL_API_VERSION 1

// ============================================================================
// ERROR CODES AND RETURN TYPES
// ============================================================================

/**
 * Error codes returned by Goxel Daemon API functions.
 * Success is indicated by GOXEL_SUCCESS (0), all other values indicate errors.
 */
typedef enum {
    GOXEL_SUCCESS = 0,              /**< Operation completed successfully */
    GOXEL_ERROR_INVALID_CONTEXT,    /**< Invalid or NULL context provided */
    GOXEL_ERROR_INVALID_PARAMETER,  /**< Invalid parameter value */
    GOXEL_ERROR_FILE_NOT_FOUND,     /**< File does not exist */
    GOXEL_ERROR_FILE_ACCESS,        /**< Cannot read/write file */
    GOXEL_ERROR_UNSUPPORTED_FORMAT, /**< File format not supported */
    GOXEL_ERROR_OUT_OF_MEMORY,      /**< Memory allocation failed */
    GOXEL_ERROR_INVALID_OPERATION,  /**< Operation not valid in current state */
    GOXEL_ERROR_LAYER_NOT_FOUND,    /**< Specified layer does not exist */
    GOXEL_ERROR_RENDER_FAILED,      /**< Rendering operation failed */
    GOXEL_ERROR_SCRIPT_FAILED,      /**< Script execution failed */
    GOXEL_ERROR_INIT_FAILED,        /**< Context initialization failed */
    GOXEL_ERROR_UNKNOWN = -1        /**< Unknown or unspecified error */
} goxel_error_t;

// ============================================================================
// CONTEXT MANAGEMENT
// ============================================================================

/**
 * Opaque context handle for Goxel operations.
 * All operations require a valid context.
 */
typedef struct goxel_context goxel_context_t;

/**
 * Creates a new Goxel context for daemon operations.
 * 
 * @return Pointer to new context, or NULL if creation failed
 * @note Caller is responsible for destroying the context with goxel_destroy_context()
 */
goxel_context_t *goxel_create_context(void);

/**
 * Initializes the Goxel context for use.
 * Must be called before any other operations.
 * 
 * @param ctx Pointer to context created with goxel_create_context()
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_init_context(goxel_context_t *ctx);

/**
 * Destroys a Goxel context and frees all associated resources.
 * 
 * @param ctx Pointer to context to destroy (may be NULL)
 * @note Context pointer becomes invalid after this call
 */
void goxel_destroy_context(goxel_context_t *ctx);

// ============================================================================
// PROJECT MANAGEMENT
// ============================================================================

/**
 * Creates a new voxel project with specified dimensions.
 * 
 * @param ctx Valid Goxel context
 * @param name Project name (optional, may be NULL)
 * @param width Initial project width in voxels (must be > 0)
 * @param height Initial project height in voxels (must be > 0)
 * @param depth Initial project depth in voxels (must be > 0)
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_create_project(goxel_context_t *ctx, const char *name, 
                                   int width, int height, int depth);

/**
 * Loads a voxel project from file.
 * Supports all standard Goxel file formats (.gox, .vox, .qb, etc.)
 * 
 * @param ctx Valid Goxel context
 * @param path Path to project file
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_load_project(goxel_context_t *ctx, const char *path);

/**
 * Saves the current project to file.
 * Format is determined by file extension.
 * 
 * @param ctx Valid Goxel context
 * @param path Output file path
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_save_project(goxel_context_t *ctx, const char *path);

/**
 * Saves the current project in specified format.
 * 
 * @param ctx Valid Goxel context
 * @param path Output file path
 * @param format Format string ("gox", "vox", "qb", etc.)
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_save_project_format(goxel_context_t *ctx, const char *path, 
                                        const char *format);

/**
 * Closes the current project and resets context state.
 * 
 * @param ctx Valid Goxel context
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_close_project(goxel_context_t *ctx);

/**
 * Gets the bounds of the current project.
 * 
 * @param ctx Valid Goxel context
 * @param width Pointer to store project width
 * @param height Pointer to store project height
 * @param depth Pointer to store project depth
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_get_project_bounds(goxel_context_t *ctx, int *width, 
                                       int *height, int *depth);

// ============================================================================
// VOXEL OPERATIONS
// ============================================================================

/**
 * Color representation for voxels.
 * Components are in RGBA order with 8 bits per component.
 */
typedef struct {
    uint8_t r, g, b, a;
} goxel_color_t;

/**
 * 3D position representation.
 */
typedef struct {
    int x, y, z;
} goxel_pos_t;

/**
 * 3D bounding box representation.
 */
typedef struct {
    goxel_pos_t min;
    goxel_pos_t max;
} goxel_box_t;

/**
 * Adds a voxel at the specified position with given color.
 * 
 * @param ctx Valid Goxel context
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param color Voxel color (RGBA)
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_add_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              const goxel_color_t *color);

/**
 * Removes a voxel at the specified position.
 * 
 * @param ctx Valid Goxel context
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_remove_voxel(goxel_context_t *ctx, int x, int y, int z);

/**
 * Gets the color of a voxel at the specified position.
 * 
 * @param ctx Valid Goxel context
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param color Pointer to store voxel color
 * @return GOXEL_SUCCESS if voxel exists, GOXEL_ERROR_INVALID_PARAMETER if empty
 */
goxel_error_t goxel_get_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              goxel_color_t *color);

/**
 * Adds multiple voxels from array data.
 * 
 * @param ctx Valid Goxel context
 * @param positions Array of voxel positions
 * @param colors Array of voxel colors (same length as positions)
 * @param count Number of voxels to add
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_add_voxel_batch(goxel_context_t *ctx, 
                                    const goxel_pos_t *positions,
                                    const goxel_color_t *colors,
                                    size_t count);

/**
 * Removes voxels within a bounding box.
 * 
 * @param ctx Valid Goxel context
 * @param box Bounding box defining removal area
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_remove_voxels_in_box(goxel_context_t *ctx, 
                                         const goxel_box_t *box);

/**
 * Changes the color of a voxel at the specified position.
 * Voxel must already exist at the position.
 * 
 * @param ctx Valid Goxel context
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param color New color for the voxel
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_paint_voxel(goxel_context_t *ctx, int x, int y, int z, 
                                const goxel_color_t *color);

// ============================================================================
// LAYER MANAGEMENT
// ============================================================================

/**
 * Layer identifier type.
 */
typedef int goxel_layer_id_t;

/**
 * Creates a new layer with specified properties.
 * 
 * @param ctx Valid Goxel context
 * @param name Layer name (optional, may be NULL)
 * @param color Default layer color (optional, may be NULL)
 * @param visible Whether layer is initially visible
 * @param layer_id Pointer to store new layer ID
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_create_layer(goxel_context_t *ctx, const char *name,
                                 const goxel_color_t *color, bool visible,
                                 goxel_layer_id_t *layer_id);

/**
 * Deletes a layer by ID.
 * 
 * @param ctx Valid Goxel context
 * @param layer_id Layer ID to delete
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_delete_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id);

/**
 * Sets the active layer for subsequent operations.
 * 
 * @param ctx Valid Goxel context
 * @param layer_id Layer ID to activate
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_set_active_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id);

/**
 * Gets the currently active layer ID.
 * 
 * @param ctx Valid Goxel context
 * @param layer_id Pointer to store active layer ID
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_get_active_layer(goxel_context_t *ctx, goxel_layer_id_t *layer_id);

/**
 * Sets layer visibility.
 * 
 * @param ctx Valid Goxel context
 * @param layer_id Layer ID
 * @param visible Whether layer should be visible
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_set_layer_visibility(goxel_context_t *ctx, 
                                         goxel_layer_id_t layer_id, bool visible);

/**
 * Gets the number of layers in the project.
 * 
 * @param ctx Valid Goxel context
 * @param count Pointer to store layer count
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_get_layer_count(goxel_context_t *ctx, int *count);

// ============================================================================
// RENDERING
// ============================================================================

/**
 * Camera preset types for rendering.
 */
typedef enum {
    GOXEL_CAMERA_FRONT,
    GOXEL_CAMERA_BACK,
    GOXEL_CAMERA_LEFT,
    GOXEL_CAMERA_RIGHT,
    GOXEL_CAMERA_TOP,
    GOXEL_CAMERA_BOTTOM,
    GOXEL_CAMERA_ISOMETRIC
} goxel_camera_preset_t;

/**
 * Render output formats.
 */
typedef enum {
    GOXEL_FORMAT_PNG,
    GOXEL_FORMAT_JPEG,
    GOXEL_FORMAT_BMP
} goxel_render_format_t;

/**
 * Rendering options structure.
 */
typedef struct {
    int width;                        /**< Output image width */
    int height;                       /**< Output image height */
    goxel_camera_preset_t camera;     /**< Camera preset */
    goxel_render_format_t format;     /**< Output format */
    int quality;                      /**< Quality (1-100 for JPEG) */
} goxel_render_options_t;

/**
 * Renders the current project to a file.
 * 
 * @param ctx Valid Goxel context
 * @param output_path Output file path
 * @param options Rendering options
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_render_to_file(goxel_context_t *ctx, const char *output_path,
                                   const goxel_render_options_t *options);

/**
 * Renders the current project to a memory buffer.
 * 
 * @param ctx Valid Goxel context
 * @param buffer Pointer to store allocated buffer (caller must free)
 * @param buffer_size Pointer to store buffer size in bytes
 * @param options Rendering options
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_render_to_buffer(goxel_context_t *ctx, uint8_t **buffer,
                                     size_t *buffer_size,
                                     const goxel_render_options_t *options);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Gets a human-readable error message for an error code.
 * 
 * @param error Error code
 * @return Pointer to error message string (do not free)
 */
const char *goxel_get_error_string(goxel_error_t error);

/**
 * Gets the last error message from context operations.
 * 
 * @param ctx Valid Goxel context
 * @return Pointer to error message string (do not free), or NULL if no error
 */
const char *goxel_get_last_error(goxel_context_t *ctx);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * Gets current memory usage statistics.
 * 
 * @param ctx Valid Goxel context
 * @param bytes_used Pointer to store bytes currently used
 * @param bytes_allocated Pointer to store total bytes allocated
 * @return GOXEL_SUCCESS on success, error code on failure
 */
goxel_error_t goxel_get_memory_usage(goxel_context_t *ctx, size_t *bytes_used,
                                     size_t *bytes_allocated);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Gets the library version information.
 * 
 * @param major Pointer to store major version (may be NULL)
 * @param minor Pointer to store minor version (may be NULL)
 * @param patch Pointer to store patch version (may be NULL)
 * @return Version string
 */
const char *goxel_get_version(int *major, int *minor, int *patch);

/**
 * Checks if the library was compiled with specific feature support.
 * 
 * @param feature Feature name ("osmesa", "scripting", etc.)
 * @return true if feature is supported, false otherwise
 */
bool goxel_has_feature(const char *feature);

#ifdef __cplusplus
}
#endif

#endif // GOXEL_DAEMON_H