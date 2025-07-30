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

#include "color_analysis.h"
#include "bulk_voxel_ops.h"
#include "../core/utils/json.h"
#include "../core/utils/vec.h"
#include "../core/utils/box.h"
#include "../../ext_src/json/json-builder.h"
#include "../../ext_src/json/json.h"
#include "../log.h"
#include "../core/volume.h"
#include "../core/layer.h"
#include "../core/image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include "../../ext_src/uthash/utlist.h"

// Hash table for efficient color counting
typedef struct color_node {
    uint8_t rgba[4];
    uint64_t count;
    struct color_node *next;
} color_node_t;

typedef struct {
    color_node_t **buckets;
    size_t bucket_count;
    size_t total_colors;
    pthread_mutex_t lock;
} color_hash_table_t;

// ============================================================================
// HASH TABLE FUNCTIONS
// ============================================================================

static uint32_t color_hash(const uint8_t rgba[4], size_t bucket_count)
{
    // Simple hash combining all color channels
    uint32_t hash = rgba[0] * 73856093u + rgba[1] * 19349663u + 
                    rgba[2] * 83492791u + rgba[3] * 25165843u;
    return hash % bucket_count;
}

static color_hash_table_t *color_hash_table_create(size_t bucket_count)
{
    color_hash_table_t *table = calloc(1, sizeof(color_hash_table_t));
    if (!table) return NULL;
    
    table->bucket_count = bucket_count;
    table->buckets = calloc(bucket_count, sizeof(color_node_t *));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    pthread_mutex_init(&table->lock, NULL);
    return table;
}

static void color_hash_table_free(color_hash_table_t *table)
{
    if (!table) return;
    
    for (size_t i = 0; i < table->bucket_count; i++) {
        color_node_t *node = table->buckets[i];
        while (node) {
            color_node_t *next = node->next;
            free(node);
            node = next;
        }
    }
    
    free(table->buckets);
    pthread_mutex_destroy(&table->lock);
    free(table);
}

static int color_hash_table_increment(color_hash_table_t *table,
                                     const uint8_t rgba[4])
{
    uint32_t hash = color_hash(rgba, table->bucket_count);
    
    pthread_mutex_lock(&table->lock);
    
    // Search for existing color
    color_node_t *node = table->buckets[hash];
    while (node) {
        if (memcmp(node->rgba, rgba, 4) == 0) {
            node->count++;
            pthread_mutex_unlock(&table->lock);
            return 0;
        }
        node = node->next;
    }
    
    // Add new color
    node = malloc(sizeof(color_node_t));
    if (!node) {
        pthread_mutex_unlock(&table->lock);
        return -1;
    }
    
    memcpy(node->rgba, rgba, 4);
    node->count = 1;
    node->next = table->buckets[hash];
    table->buckets[hash] = node;
    table->total_colors++;
    
    pthread_mutex_unlock(&table->lock);
    return 0;
}

static int color_hash_table_to_histogram(color_hash_table_t *table,
                                        color_histogram_t *histogram)
{
    histogram->capacity = table->total_colors;
    histogram->entries = calloc(histogram->capacity, 
                               sizeof(color_histogram_entry_t));
    if (!histogram->entries) return -1;
    
    size_t index = 0;
    for (size_t i = 0; i < table->bucket_count; i++) {
        color_node_t *node = table->buckets[i];
        while (node) {
            memcpy(histogram->entries[index].rgba, node->rgba, 4);
            histogram->entries[index].count = node->count;
            histogram->total_voxels += node->count;
            index++;
            node = node->next;
        }
    }
    
    histogram->count = index;
    
    // Calculate percentages
    for (size_t i = 0; i < histogram->count; i++) {
        histogram->entries[i].percentage = 
            (float)histogram->entries[i].count / histogram->total_voxels * 100.0f;
    }
    
    return 0;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int color_distance_rgb(const uint8_t c1[4], const uint8_t c2[4])
{
    int dr = (int)c1[0] - (int)c2[0];
    int dg = (int)c1[1] - (int)c2[1];
    int db = (int)c1[2] - (int)c2[2];
    return (int)sqrt(dr * dr + dg * dg + db * db);
}

bool color_matches(const uint8_t color[4],
                  const uint8_t target[4],
                  const uint8_t tolerance[4])
{
    if (!tolerance) {
        return memcmp(color, target, 4) == 0;
    }
    
    for (int i = 0; i < 4; i++) {
        int diff = abs((int)color[i] - (int)target[i]);
        if (diff > tolerance[i]) {
            return false;
        }
    }
    return true;
}

void color_to_bin(const uint8_t color[4], int bin_size, uint8_t binned[4])
{
    if (bin_size <= 1) {
        memcpy(binned, color, 4);
        return;
    }
    
    for (int i = 0; i < 3; i++) {  // Only bin RGB, not alpha
        binned[i] = (color[i] / bin_size) * bin_size + bin_size / 2;
    }
    binned[3] = color[3];  // Keep original alpha
}

static uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// ============================================================================
// COMPARISON FUNCTIONS FOR SORTING
// ============================================================================

static int compare_histogram_by_count(const void *a, const void *b)
{
    const color_histogram_entry_t *e1 = a;
    const color_histogram_entry_t *e2 = b;
    // Sort descending by count
    if (e1->count > e2->count) return -1;
    if (e1->count < e2->count) return 1;
    return 0;
}

static int compare_colors_by_rgb(const void *a, const void *b)
{
    const uint8_t *c1 = *(const uint8_t (*)[4])a;
    const uint8_t *c2 = *(const uint8_t (*)[4])b;
    
    // Compare RGB values to sort colors
    for (int i = 0; i < 3; i++) {
        if (c1[i] < c2[i]) return -1;
        if (c1[i] > c2[i]) return 1;
    }
    return 0;
}

// ============================================================================
// CORE ANALYSIS FUNCTIONS
// ============================================================================

int color_analysis_histogram(goxel_core_context_t *ctx,
                            int layer_id,
                            const int region_min[3],
                            const int region_max[3],
                            int bin_size,
                            bool sort_by_count,
                            int top_n,
                            color_histogram_t *histogram)
{
    if (!ctx || !histogram) return -1;
    
    memset(histogram, 0, sizeof(*histogram));
    histogram->bin_size = bin_size;
    histogram->binned = (bin_size > 1);
    
    // Create hash table for color counting
    color_hash_table_t *hash_table = color_hash_table_create(4096);
    if (!hash_table) return -1;
    
    // Get voxels using bulk operations
    bulk_voxel_result_t bulk_result = {0};
    int ret;
    
    if (region_min && region_max) {
        ret = bulk_get_voxels_region(ctx, region_min, region_max, layer_id,
                                    NULL, 0, 0, &bulk_result);
    } else {
        ret = bulk_get_layer_voxels(ctx, layer_id, NULL, 0, 0, &bulk_result);
    }
    
    if (ret != 0) {
        color_hash_table_free(hash_table);
        return ret;
    }
    
    // Count colors
    for (size_t i = 0; i < bulk_result.count; i++) {
        uint8_t color[4];
        memcpy(color, bulk_result.voxels[i].rgba, 4);
        
        // Apply binning if requested
        if (bin_size > 1) {
            uint8_t binned[4];
            color_to_bin(color, bin_size, binned);
            memcpy(color, binned, 4);
        }
        
        if (color_hash_table_increment(hash_table, color) != 0) {
            bulk_voxel_result_free(&bulk_result);
            color_hash_table_free(hash_table);
            return -1;
        }
    }
    
    bulk_voxel_result_free(&bulk_result);
    
    // Convert hash table to histogram
    ret = color_hash_table_to_histogram(hash_table, histogram);
    color_hash_table_free(hash_table);
    
    if (ret != 0) return ret;
    
    // Sort if requested
    if (sort_by_count && histogram->count > 1) {
        qsort(histogram->entries, histogram->count,
              sizeof(color_histogram_entry_t), compare_histogram_by_count);
    }
    
    // Limit to top N if requested
    if (top_n > 0 && histogram->count > (size_t)top_n) {
        histogram->count = top_n;
        // Recalculate percentages for top N only
        uint64_t top_total = 0;
        for (size_t i = 0; i < histogram->count; i++) {
            top_total += histogram->entries[i].count;
        }
        for (size_t i = 0; i < histogram->count; i++) {
            histogram->entries[i].percentage = 
                (float)histogram->entries[i].count / top_total * 100.0f;
        }
    }
    
    return 0;
}

int color_analysis_find_by_color(goxel_core_context_t *ctx,
                                int layer_id,
                                const int region_min[3],
                                const int region_max[3],
                                const uint8_t target_color[4],
                                const uint8_t tolerance[4],
                                int max_results,
                                bool include_locations,
                                color_search_result_t *result)
{
    if (!ctx || !target_color || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    memcpy(result->target_color, target_color, 4);
    if (tolerance) {
        memcpy(result->tolerance, tolerance, 4);
    }
    
    // Get voxels using bulk operations
    bulk_voxel_result_t bulk_result = {0};
    int ret;
    
    if (region_min && region_max) {
        ret = bulk_get_voxels_region(ctx, region_min, region_max, layer_id,
                                    NULL, 0, 0, &bulk_result);
    } else {
        ret = bulk_get_layer_voxels(ctx, layer_id, NULL, 0, 0, &bulk_result);
    }
    
    if (ret != 0) return ret;
    
    // Pre-allocate result locations if needed
    if (include_locations) {
        result->capacity = (max_results > 0) ? max_results : bulk_result.count;
        result->locations = calloc(result->capacity, sizeof(voxel_location_t));
        if (!result->locations) {
            bulk_voxel_result_free(&bulk_result);
            return -1;
        }
    }
    
    // Find matching voxels
    for (size_t i = 0; i < bulk_result.count; i++) {
        if (color_matches(bulk_result.voxels[i].rgba, target_color, tolerance)) {
            if (include_locations && result->count < result->capacity) {
                result->locations[result->count].x = bulk_result.voxels[i].x;
                result->locations[result->count].y = bulk_result.voxels[i].y;
                result->locations[result->count].z = bulk_result.voxels[i].z;
                result->locations[result->count].layer_id = layer_id;
                
                // Get layer name if possible
                if (layer_id >= 0) {
                    layer_t *layer = NULL;
                    layer_t *l;
                    DL_FOREACH(ctx->image->layers, l) {
                        if (l->id == layer_id) {
                            layer = l;
                            break;
                        }
                    }
                    if (layer && layer->name[0]) {
                        strncpy(result->locations[result->count].layer_name,
                               layer->name, 63);
                    }
                }
            }
            
            result->count++;
            
            if (max_results > 0 && result->count >= (size_t)max_results) {
                result->truncated = true;
                break;
            }
        }
    }
    
    bulk_voxel_result_free(&bulk_result);
    return 0;
}

int color_analysis_unique_colors(goxel_core_context_t *ctx,
                                int layer_id,
                                const int region_min[3],
                                const int region_max[3],
                                bool merge_similar,
                                int merge_threshold,
                                bool sort_by_count,
                                unique_colors_result_t *result)
{
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->layer_id = layer_id;
    result->sorted_by_count = sort_by_count;
    
    // First get color histogram
    color_histogram_t histogram = {0};
    int ret = color_analysis_histogram(ctx, layer_id, region_min, region_max,
                                      merge_similar ? merge_threshold : 0,
                                      sort_by_count, 0, &histogram);
    if (ret != 0) return ret;
    
    // Extract unique colors
    result->capacity = histogram.count;
    result->colors = calloc(result->capacity, 4);
    if (!result->colors) {
        color_histogram_free(&histogram);
        return -1;
    }
    
    for (size_t i = 0; i < histogram.count; i++) {
        memcpy(result->colors[result->count], histogram.entries[i].rgba, 4);
        result->count++;
    }
    
    color_histogram_free(&histogram);
    
    // Sort by RGB if not sorted by count
    if (!sort_by_count && result->count > 1) {
        qsort(result->colors, result->count, 4, compare_colors_by_rgb);
    }
    
    return 0;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void color_histogram_free(color_histogram_t *histogram)
{
    if (!histogram) return;
    free(histogram->entries);
    memset(histogram, 0, sizeof(*histogram));
}

void color_search_result_free(color_search_result_t *result)
{
    if (!result) return;
    free(result->locations);
    memset(result, 0, sizeof(*result));
}

void unique_colors_result_free(unique_colors_result_t *result)
{
    if (!result) return;
    free(result->colors);
    memset(result, 0, sizeof(*result));
}

// ============================================================================
// WORKER THREAD FUNCTIONS
// ============================================================================

int color_analysis_worker(void *request_data, int worker_id, void *context)
{
    color_analysis_context_t *ctx = (color_analysis_context_t *)request_data;
    if (!ctx) return -1;
    
    ctx->start_time_us = get_time_us();
    int ret = 0;
    
    switch (ctx->analysis_type) {
        case COLOR_ANALYSIS_HISTOGRAM:
            ctx->result.histogram = calloc(1, sizeof(color_histogram_t));
            if (!ctx->result.histogram) return -1;
            
            ret = color_analysis_histogram(
                ctx->goxel_ctx, ctx->layer_id,
                ctx->use_region ? ctx->region_min : NULL,
                ctx->use_region ? ctx->region_max : NULL,
                ctx->bin_size, ctx->sort_by_count, ctx->top_n,
                ctx->result.histogram);
            break;
            
        case COLOR_ANALYSIS_FIND_BY_COLOR:
            ctx->result.search_result = calloc(1, sizeof(color_search_result_t));
            if (!ctx->result.search_result) return -1;
            
            ret = color_analysis_find_by_color(
                ctx->goxel_ctx, ctx->layer_id,
                ctx->use_region ? ctx->region_min : NULL,
                ctx->use_region ? ctx->region_max : NULL,
                ctx->target_color, ctx->tolerance,
                ctx->max_results, ctx->include_locations,
                ctx->result.search_result);
            break;
            
        case COLOR_ANALYSIS_UNIQUE_COLORS:
            ctx->result.unique_colors = calloc(1, sizeof(unique_colors_result_t));
            if (!ctx->result.unique_colors) return -1;
            
            ret = color_analysis_unique_colors(
                ctx->goxel_ctx, ctx->layer_id,
                ctx->use_region ? ctx->region_min : NULL,
                ctx->use_region ? ctx->region_max : NULL,
                ctx->merge_similar, ctx->merge_threshold,
                ctx->sort_by_count,
                ctx->result.unique_colors);
            break;
    }
    
    // Build JSON response
    if (ret == 0) {
        json_value *result_json = NULL;
        
        switch (ctx->analysis_type) {
            case COLOR_ANALYSIS_HISTOGRAM:
                result_json = color_histogram_to_json(ctx->result.histogram, true);
                break;
            case COLOR_ANALYSIS_FIND_BY_COLOR:
                result_json = color_search_result_to_json(ctx->result.search_result, true);
                break;
            case COLOR_ANALYSIS_UNIQUE_COLORS:
                result_json = unique_colors_result_to_json(ctx->result.unique_colors, true);
                break;
        }
        
        if (result_json) {
            ctx->response = json_rpc_create_response_result(
                result_json, &ctx->request->id);
            json_value_free(result_json);
        } else {
            ret = -1;
        }
    }
    
    if (ret != 0) {
        ctx->response = json_rpc_create_response_error(
            JSON_RPC_INTERNAL_ERROR, "Color analysis failed", 
            NULL, &ctx->request->id);
    }
    
    return ret;
}

void color_analysis_cleanup(void *request_data)
{
    color_analysis_context_t *ctx = (color_analysis_context_t *)request_data;
    if (!ctx) return;
    
    switch (ctx->analysis_type) {
        case COLOR_ANALYSIS_HISTOGRAM:
            if (ctx->result.histogram) {
                color_histogram_free(ctx->result.histogram);
                free(ctx->result.histogram);
            }
            break;
        case COLOR_ANALYSIS_FIND_BY_COLOR:
            if (ctx->result.search_result) {
                color_search_result_free(ctx->result.search_result);
                free(ctx->result.search_result);
            }
            break;
        case COLOR_ANALYSIS_UNIQUE_COLORS:
            if (ctx->result.unique_colors) {
                unique_colors_result_free(ctx->result.unique_colors);
                free(ctx->result.unique_colors);
            }
            break;
    }
    
    free(ctx);
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

json_value *color_histogram_to_json(const color_histogram_t *histogram,
                                   bool include_metadata)
{
    json_value *root = json_object_new(0);
    json_value *entries_array = json_array_new(histogram->count);
    
    // Add histogram entries
    for (size_t i = 0; i < histogram->count; i++) {
        json_value *entry = json_object_new(0);
        
        // Color as hex string
        char hex[10];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                histogram->entries[i].rgba[0],
                histogram->entries[i].rgba[1],
                histogram->entries[i].rgba[2],
                histogram->entries[i].rgba[3]);
        json_object_push(entry, "color", json_string_new(hex));
        
        // RGB array
        json_value *rgb = json_array_new(4);
        for (int j = 0; j < 4; j++) {
            json_array_push(rgb, json_integer_new(histogram->entries[i].rgba[j]));
        }
        json_object_push(entry, "rgba", rgb);
        
        // Count and percentage
        json_object_push(entry, "count", 
                        json_integer_new(histogram->entries[i].count));
        json_object_push(entry, "percentage",
                        json_double_new(histogram->entries[i].percentage));
        
        json_array_push(entries_array, entry);
    }
    
    json_object_push(root, "histogram", entries_array);
    json_object_push(root, "total_voxels", 
                    json_integer_new(histogram->total_voxels));
    json_object_push(root, "unique_colors",
                    json_integer_new(histogram->count));
    
    if (include_metadata) {
        json_value *metadata = json_object_new(0);
        json_object_push(metadata, "binned", 
                        json_boolean_new(histogram->binned));
        if (histogram->binned) {
            json_object_push(metadata, "bin_size",
                            json_integer_new(histogram->bin_size));
        }
        json_object_push(root, "metadata", metadata);
    }
    
    return root;
}

json_value *color_search_result_to_json(const color_search_result_t *result,
                                       bool include_metadata)
{
    json_value *root = json_object_new(0);
    
    // Target color
    char hex[10];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
            result->target_color[0], result->target_color[1],
            result->target_color[2], result->target_color[3]);
    json_object_push(root, "target_color", json_string_new(hex));
    
    // Match count
    json_object_push(root, "match_count", json_integer_new(result->count));
    json_object_push(root, "truncated", json_boolean_new(result->truncated));
    
    // Locations if included
    if (result->locations && result->capacity > 0) {
        json_value *locations = json_array_new(result->count);
        
        for (size_t i = 0; i < result->count && i < result->capacity; i++) {
            json_value *loc = json_object_new(0);
            json_object_push(loc, "x", 
                            json_integer_new(result->locations[i].x));
            json_object_push(loc, "y",
                            json_integer_new(result->locations[i].y));
            json_object_push(loc, "z",
                            json_integer_new(result->locations[i].z));
            
            if (result->locations[i].layer_name[0]) {
                json_object_push(loc, "layer",
                                json_string_new(result->locations[i].layer_name));
            }
            json_object_push(loc, "layer_id",
                            json_integer_new(result->locations[i].layer_id));
            
            json_array_push(locations, loc);
        }
        
        json_object_push(root, "locations", locations);
    }
    
    if (include_metadata) {
        json_value *metadata = json_object_new(0);
        
        // Tolerance used
        json_value *tolerance = json_array_new(4);
        for (int i = 0; i < 4; i++) {
            json_array_push(tolerance, json_integer_new(result->tolerance[i]));
        }
        json_object_push(metadata, "tolerance", tolerance);
        
        json_object_push(root, "metadata", metadata);
    }
    
    return root;
}

json_value *unique_colors_result_to_json(const unique_colors_result_t *result,
                                        bool include_metadata)
{
    json_value *root = json_object_new(0);
    json_value *colors_array = json_array_new(result->count);
    
    // Add colors
    for (size_t i = 0; i < result->count; i++) {
        json_value *color_obj = json_object_new(0);
        
        // Hex string
        char hex[10];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                result->colors[i][0], result->colors[i][1],
                result->colors[i][2], result->colors[i][3]);
        json_object_push(color_obj, "hex", json_string_new(hex));
        
        // RGB array
        json_value *rgba = json_array_new(4);
        for (int j = 0; j < 4; j++) {
            json_array_push(rgba, json_integer_new(result->colors[i][j]));
        }
        json_object_push(color_obj, "rgba", rgba);
        
        json_array_push(colors_array, color_obj);
    }
    
    json_object_push(root, "colors", colors_array);
    json_object_push(root, "count", json_integer_new(result->count));
    
    if (include_metadata) {
        json_value *metadata = json_object_new(0);
        json_object_push(metadata, "layer_id",
                        json_integer_new(result->layer_id));
        json_object_push(metadata, "sorted_by_count",
                        json_boolean_new(result->sorted_by_count));
        json_object_push(root, "metadata", metadata);
    }
    
    return root;
}

// ============================================================================
// CACHING FUNCTIONS (PLACEHOLDER)
// ============================================================================

uint64_t color_analysis_cache_key(int layer_id,
                                 const int region_min[3],
                                 const int region_max[3],
                                 const void *extra_data,
                                 size_t extra_size)
{
    // Simple hash for cache key
    uint64_t key = layer_id;
    
    if (region_min && region_max) {
        for (int i = 0; i < 3; i++) {
            key = key * 31 + region_min[i];
            key = key * 31 + region_max[i];
        }
    }
    
    if (extra_data && extra_size > 0) {
        const uint8_t *bytes = extra_data;
        for (size_t i = 0; i < extra_size; i++) {
            key = key * 31 + bytes[i];
        }
    }
    
    return key;
}

bool color_analysis_cache_valid(goxel_core_context_t *ctx,
                               uint64_t cache_key,
                               uint64_t timestamp)
{
    // For now, always return false (no caching)
    // TODO: Implement proper cache validation based on voxel modification times
    (void)ctx;
    (void)cache_key;
    (void)timestamp;
    return false;
}