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

#include "bulk_voxel_ops.h"
#include "../log.h"
#include "../core/utils/vec.h"
#include "../core/utils/box.h"
#include "../core/utils/json.h"
#include "../../ext_src/json/json-builder.h"
#include "../../ext_src/json/json.h"
#include "../../ext_src/uthash/utlist.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static bool color_matches_filter(const uint8_t voxel[4], const uint8_t filter[4])
{
    if (!filter || filter[3] == 0) return true; // No filter
    
    return voxel[0] == filter[0] && 
           voxel[1] == filter[1] && 
           voxel[2] == filter[2] &&
           (filter[3] == 255 || voxel[3] == filter[3]);
}

static int ensure_result_capacity(bulk_voxel_result_t *result, size_t needed)
{
    if (result->count + needed <= result->capacity) return 0;
    
    size_t new_capacity = result->capacity ? result->capacity * 2 : 1024;
    while (new_capacity < result->count + needed) {
        new_capacity *= 2;
    }
    
    bulk_voxel_t *new_voxels = realloc(result->voxels, 
                                       new_capacity * sizeof(bulk_voxel_t));
    if (!new_voxels) {
        LOG_E("Failed to allocate memory for %zu voxels", new_capacity);
        return -1;
    }
    
    result->voxels = new_voxels;
    result->capacity = new_capacity;
    return 0;
}

static void update_bbox(int bbox[2][3], int x, int y, int z)
{
    if (x < bbox[0][0]) bbox[0][0] = x;
    if (y < bbox[0][1]) bbox[0][1] = y;
    if (z < bbox[0][2]) bbox[0][2] = z;
    if (x > bbox[1][0]) bbox[1][0] = x;
    if (y > bbox[1][1]) bbox[1][1] = y;
    if (z > bbox[1][2]) bbox[1][2] = z;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

int bulk_get_voxels_region(goxel_core_context_t *ctx,
                          const int min[3], const int max[3],
                          int layer_id,
                          const uint8_t color_filter[4],
                          int offset, int limit,
                          bulk_voxel_result_t *result)
{
    if (!ctx || !min || !max || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Initialize bounding box to invalid state
    result->bbox[0][0] = result->bbox[0][1] = result->bbox[0][2] = INT_MAX;
    result->bbox[1][0] = result->bbox[1][1] = result->bbox[1][2] = INT_MIN;
    
    // Get the layer
    layer_t *layer = NULL;
    if (layer_id == -1) {
        layer = ctx->image->active_layer;
    } else {
        DL_FOREACH(ctx->image->layers, layer) {
            if (layer->id == layer_id) break;
        }
    }
    
    if (!layer) {
        LOG_E("Layer %d not found", layer_id);
        return -1;
    }
    
    // Create box for iteration
    float box[4][4];
    mat4_copy(mat4_identity, box);
    box[0][0] = max[0] - min[0] + 1;
    box[1][1] = max[1] - min[1] + 1;
    box[2][2] = max[2] - min[2] + 1;
    box[3][0] = (min[0] + max[0]) / 2.0f;
    box[3][1] = (min[1] + max[1]) / 2.0f;
    box[3][2] = (min[2] + max[2]) / 2.0f;
    
    // Get iterator for the box region
    volume_iterator_t iter = volume_get_box_iterator(layer->volume, box, 0);
    
    int pos[3];
    uint8_t rgba[4];
    int skipped = 0;
    
    while (volume_iter(&iter, pos)) {
        // Check if position is within bounds
        if (pos[0] < min[0] || pos[0] > max[0] ||
            pos[1] < min[1] || pos[1] > max[1] ||
            pos[2] < min[2] || pos[2] > max[2]) {
            continue;
        }
        
        volume_get_at(layer->volume, &iter, pos, rgba);
        
        // Skip empty voxels
        if (rgba[3] == 0) continue;
        
        // Apply color filter
        if (!color_matches_filter(rgba, color_filter)) continue;
        
        // Handle pagination offset
        if (skipped < offset) {
            skipped++;
            continue;
        }
        
        // Check limit
        if (limit > 0 && result->count >= (size_t)limit) {
            result->truncated = true;
            break;
        }
        
        // Ensure capacity
        if (ensure_result_capacity(result, 1) != 0) {
            bulk_voxel_result_free(result);
            return -1;
        }
        
        // Add voxel
        bulk_voxel_t *voxel = &result->voxels[result->count++];
        voxel->x = pos[0];
        voxel->y = pos[1];
        voxel->z = pos[2];
        memcpy(voxel->rgba, rgba, 4);
        
        // Update bounding box
        update_bbox(result->bbox, pos[0], pos[1], pos[2]);
    }
    
    // Fix bounding box if no voxels found
    if (result->count == 0) {
        memset(result->bbox, 0, sizeof(result->bbox));
    }
    
    return 0;
}

int bulk_get_layer_voxels(goxel_core_context_t *ctx,
                         int layer_id,
                         const uint8_t color_filter[4],
                         int offset, int limit,
                         bulk_voxel_result_t *result)
{
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Initialize bounding box to invalid state
    result->bbox[0][0] = result->bbox[0][1] = result->bbox[0][2] = INT_MAX;
    result->bbox[1][0] = result->bbox[1][1] = result->bbox[1][2] = INT_MIN;
    
    // Get the layer
    layer_t *layer = NULL;
    if (layer_id == -1) {
        layer = ctx->image->active_layer;
    } else {
        DL_FOREACH(ctx->image->layers, layer) {
            if (layer->id == layer_id) break;
        }
    }
    
    if (!layer) {
        LOG_E("Layer %d not found", layer_id);
        return -1;
    }
    
    // Get iterator for the entire layer
    volume_iterator_t iter = volume_get_iterator(layer->volume, 0);
    
    int pos[3];
    uint8_t rgba[4];
    int skipped = 0;
    
    while (volume_iter(&iter, pos)) {
        volume_get_at(layer->volume, &iter, pos, rgba);
        
        // Skip empty voxels
        if (rgba[3] == 0) continue;
        
        // Apply color filter
        if (!color_matches_filter(rgba, color_filter)) continue;
        
        // Handle pagination offset
        if (skipped < offset) {
            skipped++;
            continue;
        }
        
        // Check limit
        if (limit > 0 && result->count >= (size_t)limit) {
            result->truncated = true;
            break;
        }
        
        // Ensure capacity
        if (ensure_result_capacity(result, 1) != 0) {
            bulk_voxel_result_free(result);
            return -1;
        }
        
        // Add voxel
        bulk_voxel_t *voxel = &result->voxels[result->count++];
        voxel->x = pos[0];
        voxel->y = pos[1];
        voxel->z = pos[2];
        memcpy(voxel->rgba, rgba, 4);
        
        // Update bounding box
        update_bbox(result->bbox, pos[0], pos[1], pos[2]);
    }
    
    // Fix bounding box if no voxels found
    if (result->count == 0) {
        memset(result->bbox, 0, sizeof(result->bbox));
    }
    
    return 0;
}

int bulk_get_bounding_box(goxel_core_context_t *ctx,
                         int layer_id,
                         bool exact,
                         int bbox[2][3])
{
    if (!ctx || !bbox) return -1;
    
    memset(bbox, 0, sizeof(int) * 6);
    
    if (layer_id == -2) {
        // Get bounding box of entire image (all layers)
        bool found = false;
        int temp_bbox[2][3];
        
        layer_t *layer;
        DL_FOREACH(ctx->image->layers, layer) {
            if (volume_get_bbox(layer->volume, temp_bbox, exact)) {
                if (!found) {
                    memcpy(bbox, temp_bbox, sizeof(int) * 6);
                    found = true;
                } else {
                    // Expand bbox to include this layer
                    for (int i = 0; i < 3; i++) {
                        if (temp_bbox[0][i] < bbox[0][i]) bbox[0][i] = temp_bbox[0][i];
                        if (temp_bbox[1][i] > bbox[1][i]) bbox[1][i] = temp_bbox[1][i];
                    }
                }
            }
        }
        
        return found ? 0 : 1; // Return 1 if empty
    } else {
        // Get bounding box of specific layer
        layer_t *layer = NULL;
        
        if (layer_id == -1) {
            layer = ctx->image->active_layer;
        } else {
            DL_FOREACH(ctx->image->layers, layer) {
                if (layer->id == layer_id) break;
            }
        }
        
        if (!layer) {
            LOG_E("Layer %d not found", layer_id);
            return -1;
        }
        
        return volume_get_bbox(layer->volume, bbox, exact) ? 0 : 1;
    }
}

void bulk_voxel_result_free(bulk_voxel_result_t *result)
{
    if (result && result->voxels) {
        free(result->voxels);
        result->voxels = NULL;
        result->count = 0;
        result->capacity = 0;
    }
}

// ============================================================================
// WORKER THREAD FUNCTIONS
// ============================================================================

int bulk_voxel_worker(void *request_data, int worker_id, void *context)
{
    bulk_voxel_context_t *ctx = (bulk_voxel_context_t *)request_data;
    if (!ctx) return -1;
    
    LOG_D("Worker %d: Processing bulk voxel operation", worker_id);
    
    // Record start time
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ctx->start_time_us = start.tv_sec * 1000000ULL + start.tv_nsec / 1000;
    
    // Perform the operation based on request method
    const char *method = ctx->request->method;
    bulk_voxel_result_t result = {0};
    int ret = -1;
    
    if (strcmp(method, "goxel.get_voxels_region") == 0) {
        ret = bulk_get_voxels_region(ctx->goxel_ctx, 
                                     ctx->min, ctx->max,
                                     ctx->layer_id,
                                     ctx->use_color_filter ? ctx->color_filter : NULL,
                                     ctx->offset, ctx->limit,
                                     &result);
    } else if (strcmp(method, "goxel.get_layer_voxels") == 0) {
        ret = bulk_get_layer_voxels(ctx->goxel_ctx,
                                    ctx->layer_id,
                                    ctx->use_color_filter ? ctx->color_filter : NULL,
                                    ctx->offset, ctx->limit,
                                    &result);
    } else if (strcmp(method, "goxel.get_bounding_box") == 0) {
        int bbox[2][3];
        ret = bulk_get_bounding_box(ctx->goxel_ctx,
                                    ctx->layer_id,
                                    true, // Always exact for now
                                    bbox);
        
        // Create response
        if (ret >= 0) {
            json_value *json_result = bulk_bbox_to_json(bbox, ret == 1);
            if (json_result) {
                ctx->response = json_rpc_create_response_result(json_result, &ctx->request->id);
            }
        }
        
        return ret == 0 || ret == 1 ? 0 : -1;
    }
    
    if (ret == 0) {
        // Convert result to JSON
        json_value *json_result = bulk_voxel_result_to_json(&result, 
                                                            ctx->compression,
                                                            true);
        if (json_result) {
            ctx->response = json_rpc_create_response_result(json_result, &ctx->request->id);
        } else {
            ret = -1;
        }
    }
    
    // Cleanup
    bulk_voxel_result_free(&result);
    
    // Record stats
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed_us = (end.tv_sec - start.tv_sec) * 1000000ULL + 
                         (end.tv_nsec - start.tv_nsec) / 1000;
    
    LOG_D("Worker %d: Bulk operation completed in %llu us", worker_id, elapsed_us);
    
    return ret;
}

void bulk_voxel_cleanup(void *request_data)
{
    bulk_voxel_context_t *ctx = (bulk_voxel_context_t *)request_data;
    if (ctx) {
        // Response is handled by the caller
        free(ctx);
    }
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

json_value *bulk_voxel_result_to_json(const bulk_voxel_result_t *result,
                                      bulk_compress_type_t compression,
                                      bool include_metadata)
{
    if (!result) return NULL;
    
    json_value *root = json_object_new(4);
    if (!root) return NULL;
    
    // Add voxels array
    json_value *voxels_array = json_array_new(result->count);
    if (!voxels_array) {
        json_builder_free(root);
        return NULL;
    }
    
    for (size_t i = 0; i < result->count; i++) {
        const bulk_voxel_t *voxel = &result->voxels[i];
        
        json_value *voxel_obj = json_object_new(4);
        if (!voxel_obj) continue;
        
        json_object_push(voxel_obj, "x", json_integer_new(voxel->x));
        json_object_push(voxel_obj, "y", json_integer_new(voxel->y));
        json_object_push(voxel_obj, "z", json_integer_new(voxel->z));
        
        json_value *color_array = json_array_new(4);
        json_array_push(color_array, json_integer_new(voxel->rgba[0]));
        json_array_push(color_array, json_integer_new(voxel->rgba[1]));
        json_array_push(color_array, json_integer_new(voxel->rgba[2]));
        json_array_push(color_array, json_integer_new(voxel->rgba[3]));
        
        json_object_push(voxel_obj, "color", color_array);
        json_array_push(voxels_array, voxel_obj);
    }
    
    json_object_push(root, "voxels", voxels_array);
    json_object_push(root, "count", json_integer_new(result->count));
    
    if (include_metadata) {
        json_object_push(root, "truncated", json_boolean_new(result->truncated));
        
        // Add bounding box if we have voxels
        if (result->count > 0) {
            json_value *bbox_obj = json_object_new(2);
            
            json_value *min_array = json_array_new(3);
            json_array_push(min_array, json_integer_new(result->bbox[0][0]));
            json_array_push(min_array, json_integer_new(result->bbox[0][1]));
            json_array_push(min_array, json_integer_new(result->bbox[0][2]));
            
            json_value *max_array = json_array_new(3);
            json_array_push(max_array, json_integer_new(result->bbox[1][0]));
            json_array_push(max_array, json_integer_new(result->bbox[1][1]));
            json_array_push(max_array, json_integer_new(result->bbox[1][2]));
            
            json_object_push(bbox_obj, "min", min_array);
            json_object_push(bbox_obj, "max", max_array);
            json_object_push(root, "bbox", bbox_obj);
        }
    }
    
    // TODO: Implement compression if needed
    if (compression != BULK_COMPRESS_NONE) {
        json_object_push(root, "compressed", json_boolean_new(false));
    }
    
    return root;
}

json_value *bulk_bbox_to_json(const int bbox[2][3], bool is_empty)
{
    json_value *root = json_object_new(3);
    if (!root) return NULL;
    
    json_object_push(root, "empty", json_boolean_new(is_empty));
    
    if (!is_empty) {
        json_value *min_array = json_array_new(3);
        json_array_push(min_array, json_integer_new(bbox[0][0]));
        json_array_push(min_array, json_integer_new(bbox[0][1]));
        json_array_push(min_array, json_integer_new(bbox[0][2]));
        
        json_value *max_array = json_array_new(3);
        json_array_push(max_array, json_integer_new(bbox[1][0]));
        json_array_push(max_array, json_integer_new(bbox[1][1]));
        json_array_push(max_array, json_integer_new(bbox[1][2]));
        
        json_object_push(root, "min", min_array);
        json_object_push(root, "max", max_array);
        
        // Calculate dimensions
        json_value *dims_array = json_array_new(3);
        json_array_push(dims_array, json_integer_new(bbox[1][0] - bbox[0][0] + 1));
        json_array_push(dims_array, json_integer_new(bbox[1][1] - bbox[0][1] + 1));
        json_array_push(dims_array, json_integer_new(bbox[1][2] - bbox[0][2] + 1));
        json_object_push(root, "dimensions", dims_array);
    }
    
    return root;
}