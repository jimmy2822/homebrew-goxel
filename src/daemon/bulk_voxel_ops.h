/* Goxel 3D voxels editor
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

#ifndef BULK_VOXEL_OPS_H
#define BULK_VOXEL_OPS_H

#include "../core/goxel_core.h"
#include "json_rpc.h"
#include <stdbool.h>

// Maximum voxels per chunk for streaming responses
#define BULK_VOXELS_CHUNK_SIZE 10000

// Compression types for bulk data
typedef enum {
    BULK_COMPRESS_NONE = 0,
    BULK_COMPRESS_GZIP = 1,
    BULK_COMPRESS_LZ4 = 2
} bulk_compress_type_t;

// Bulk operation context for worker threads
typedef struct {
    goxel_core_context_t *goxel_ctx;
    json_rpc_request_t *request;
    json_rpc_response_t *response;
    
    // Operation parameters
    int min[3];           // Min coordinates for region
    int max[3];           // Max coordinates for region
    int layer_id;         // Layer ID (-1 for all layers)
    uint8_t color_filter[4]; // Color filter (alpha 0 = no filter)
    bool use_color_filter;
    
    // Streaming/pagination
    int offset;           // Current offset for pagination
    int limit;            // Max voxels per response
    bool has_more;        // More data available
    
    // Compression
    bulk_compress_type_t compression;
    
    // Statistics
    uint64_t total_voxels;
    uint64_t processed_voxels;
    uint64_t start_time_us;
} bulk_voxel_context_t;

// Bulk voxel data structure
typedef struct {
    int x, y, z;
    uint8_t rgba[4];
} bulk_voxel_t;

// Bulk operation result
typedef struct {
    bulk_voxel_t *voxels;
    size_t count;
    size_t capacity;
    bool truncated;       // True if results were truncated
    int bbox[2][3];      // Actual bounding box of results
} bulk_voxel_result_t;

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

/**
 * Get all voxels in a region.
 * 
 * @param ctx Goxel core context
 * @param min Min coordinates (inclusive)
 * @param max Max coordinates (inclusive)
 * @param layer_id Layer ID or -1 for active layer
 * @param color_filter Optional color filter (NULL for no filter)
 * @param offset Start offset for pagination
 * @param limit Maximum voxels to return (0 for no limit)
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int bulk_get_voxels_region(goxel_core_context_t *ctx,
                          const int min[3], const int max[3],
                          int layer_id,
                          const uint8_t color_filter[4],
                          int offset, int limit,
                          bulk_voxel_result_t *result);

/**
 * Get all voxels in a layer.
 * 
 * @param ctx Goxel core context
 * @param layer_id Layer ID or -1 for active layer
 * @param color_filter Optional color filter (NULL for no filter)
 * @param offset Start offset for pagination
 * @param limit Maximum voxels to return (0 for no limit)
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int bulk_get_layer_voxels(goxel_core_context_t *ctx,
                         int layer_id,
                         const uint8_t color_filter[4],
                         int offset, int limit,
                         bulk_voxel_result_t *result);

/**
 * Get bounding box of voxels.
 * 
 * @param ctx Goxel core context
 * @param layer_id Layer ID, -1 for active layer, -2 for all layers
 * @param exact If true, compute exact bounds (slower)
 * @param bbox Output bounding box [min][max]
 * @return 0 on success, error code on failure
 */
int bulk_get_bounding_box(goxel_core_context_t *ctx,
                         int layer_id,
                         bool exact,
                         int bbox[2][3]);

/**
 * Free bulk voxel result.
 * 
 * @param result Result to free
 */
void bulk_voxel_result_free(bulk_voxel_result_t *result);

// ============================================================================
// WORKER THREAD FUNCTIONS
// ============================================================================

/**
 * Worker function for bulk voxel operations.
 * Used by the worker pool for non-blocking execution.
 * 
 * @param request_data Bulk voxel context
 * @param worker_id Worker thread ID
 * @param context Worker pool context
 * @return 0 on success, error code on failure
 */
int bulk_voxel_worker(void *request_data, int worker_id, void *context);

/**
 * Cleanup function for bulk voxel operations.
 * 
 * @param request_data Bulk voxel context to cleanup
 */
void bulk_voxel_cleanup(void *request_data);

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

/**
 * Convert bulk voxel result to JSON.
 * 
 * @param result Voxel result to convert
 * @param compression Compression type
 * @param include_metadata Include metadata in response
 * @return JSON value or NULL on error
 */
json_value *bulk_voxel_result_to_json(const bulk_voxel_result_t *result,
                                      bulk_compress_type_t compression,
                                      bool include_metadata);

/**
 * Convert bounding box to JSON.
 * 
 * @param bbox Bounding box [min][max]
 * @param is_empty True if volume is empty
 * @return JSON value or NULL on error
 */
json_value *bulk_bbox_to_json(const int bbox[2][3], bool is_empty);

#endif // BULK_VOXEL_OPS_H