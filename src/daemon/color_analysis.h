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

#ifndef COLOR_ANALYSIS_H
#define COLOR_ANALYSIS_H

#include "../core/goxel_core.h"
#include "bulk_voxel_ops.h"
#include "json_rpc.h"
#include <stdbool.h>

// Color histogram bin size (for grouping similar colors)
#define COLOR_HISTOGRAM_BIN_SIZE 8  // Group colors within 8 RGB values

// Maximum unique colors to track (for memory efficiency)
#define MAX_UNIQUE_COLORS 65536

// Color tolerance for similarity matching (0-255 per channel)
#define DEFAULT_COLOR_TOLERANCE 10

// Color entry in histogram
typedef struct {
    uint8_t rgba[4];      // Color value
    uint64_t count;       // Number of voxels with this color
    float percentage;     // Percentage of total voxels
} color_histogram_entry_t;

// Color histogram result
typedef struct {
    color_histogram_entry_t *entries;
    size_t count;
    size_t capacity;
    uint64_t total_voxels;
    bool binned;          // True if colors were grouped into bins
    int bin_size;         // Bin size used for grouping
} color_histogram_t;

// Voxel location for color search results
typedef struct {
    int x, y, z;
    int layer_id;
    char layer_name[64];
} voxel_location_t;

// Color search result
typedef struct {
    uint8_t target_color[4];
    uint8_t tolerance[4];     // Per-channel tolerance
    voxel_location_t *locations;
    size_t count;
    size_t capacity;
    bool truncated;           // True if results were truncated
} color_search_result_t;

// Unique colors result
typedef struct {
    uint8_t (*colors)[4];     // Array of unique RGBA colors
    size_t count;
    size_t capacity;
    int layer_id;             // Layer analyzed (-2 for all)
    bool sorted_by_count;     // True if sorted by usage count
} unique_colors_result_t;

// Color analysis context for worker threads
typedef struct {
    goxel_core_context_t *goxel_ctx;
    json_rpc_request_t *request;
    json_rpc_response_t *response;
    
    // Analysis type
    enum {
        COLOR_ANALYSIS_HISTOGRAM,
        COLOR_ANALYSIS_FIND_BY_COLOR,
        COLOR_ANALYSIS_UNIQUE_COLORS
    } analysis_type;
    
    // Common parameters
    int layer_id;             // Layer ID (-1 for active, -2 for all)
    int region_min[3];        // Min coordinates for region
    int region_max[3];        // Max coordinates for region
    bool use_region;          // True if analyzing specific region
    
    // Histogram parameters
    int bin_size;             // Bin size for histogram (0 for exact)
    bool sort_by_count;       // Sort histogram by count
    int top_n;                // Return only top N colors (0 for all)
    
    // Color search parameters
    uint8_t target_color[4];  // Color to search for
    uint8_t tolerance[4];     // Per-channel tolerance
    int max_results;          // Maximum results to return
    bool include_locations;   // Include voxel locations
    
    // Unique colors parameters
    bool merge_similar;       // Merge similar colors
    int merge_threshold;      // Threshold for merging (RGB distance)
    
    // Performance parameters
    bool use_cache;           // Use cached results if available
    uint64_t cache_key;       // Cache key for this analysis
    
    // Progress tracking
    uint64_t total_voxels;
    uint64_t processed_voxels;
    uint64_t start_time_us;
    
    // Results
    union {
        color_histogram_t *histogram;
        color_search_result_t *search_result;
        unique_colors_result_t *unique_colors;
    } result;
} color_analysis_context_t;

// ============================================================================
// CORE ANALYSIS FUNCTIONS
// ============================================================================

/**
 * Generate color histogram for voxels.
 * 
 * @param ctx Goxel core context
 * @param layer_id Layer ID (-1 for active, -2 for all)
 * @param region_min Optional region min coordinates (NULL for all)
 * @param region_max Optional region max coordinates (NULL for all)
 * @param bin_size Bin size for grouping colors (0 for exact)
 * @param sort_by_count Sort results by count
 * @param top_n Return only top N colors (0 for all)
 * @param histogram Output histogram
 * @return 0 on success, error code on failure
 */
int color_analysis_histogram(goxel_core_context_t *ctx,
                            int layer_id,
                            const int region_min[3],
                            const int region_max[3],
                            int bin_size,
                            bool sort_by_count,
                            int top_n,
                            color_histogram_t *histogram);

/**
 * Find all voxels matching a color.
 * 
 * @param ctx Goxel core context
 * @param layer_id Layer ID (-1 for active, -2 for all)
 * @param region_min Optional region min coordinates (NULL for all)
 * @param region_max Optional region max coordinates (NULL for all)
 * @param target_color Color to search for
 * @param tolerance Per-channel tolerance (NULL for exact match)
 * @param max_results Maximum results to return (0 for all)
 * @param include_locations Include voxel locations in results
 * @param result Output search result
 * @return 0 on success, error code on failure
 */
int color_analysis_find_by_color(goxel_core_context_t *ctx,
                                int layer_id,
                                const int region_min[3],
                                const int region_max[3],
                                const uint8_t target_color[4],
                                const uint8_t tolerance[4],
                                int max_results,
                                bool include_locations,
                                color_search_result_t *result);

/**
 * Get all unique colors used in voxels.
 * 
 * @param ctx Goxel core context
 * @param layer_id Layer ID (-1 for active, -2 for all)
 * @param region_min Optional region min coordinates (NULL for all)
 * @param region_max Optional region max coordinates (NULL for all)
 * @param merge_similar Merge similar colors
 * @param merge_threshold RGB distance threshold for merging
 * @param sort_by_count Sort by usage count
 * @param result Output unique colors
 * @return 0 on success, error code on failure
 */
int color_analysis_unique_colors(goxel_core_context_t *ctx,
                                int layer_id,
                                const int region_min[3],
                                const int region_max[3],
                                bool merge_similar,
                                int merge_threshold,
                                bool sort_by_count,
                                unique_colors_result_t *result);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Calculate RGB distance between two colors.
 * 
 * @param c1 First color RGBA
 * @param c2 Second color RGBA
 * @return Euclidean distance in RGB space
 */
int color_distance_rgb(const uint8_t c1[4], const uint8_t c2[4]);

/**
 * Check if color matches target within tolerance.
 * 
 * @param color Color to check
 * @param target Target color
 * @param tolerance Per-channel tolerance (NULL for exact match)
 * @return true if matches, false otherwise
 */
bool color_matches(const uint8_t color[4],
                  const uint8_t target[4],
                  const uint8_t tolerance[4]);

/**
 * Bin a color to reduce unique colors.
 * 
 * @param color Input color
 * @param bin_size Bin size (1-128)
 * @param binned Output binned color
 */
void color_to_bin(const uint8_t color[4], int bin_size, uint8_t binned[4]);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * Free color histogram.
 * 
 * @param histogram Histogram to free
 */
void color_histogram_free(color_histogram_t *histogram);

/**
 * Free color search result.
 * 
 * @param result Result to free
 */
void color_search_result_free(color_search_result_t *result);

/**
 * Free unique colors result.
 * 
 * @param result Result to free
 */
void unique_colors_result_free(unique_colors_result_t *result);

// ============================================================================
// WORKER THREAD FUNCTIONS
// ============================================================================

/**
 * Worker function for color analysis operations.
 * Used by the worker pool for non-blocking execution.
 * 
 * @param request_data Color analysis context
 * @param worker_id Worker thread ID
 * @param context Worker pool context
 * @return 0 on success, error code on failure
 */
int color_analysis_worker(void *request_data, int worker_id, void *context);

/**
 * Cleanup function for color analysis operations.
 * 
 * @param request_data Color analysis context to cleanup
 */
void color_analysis_cleanup(void *request_data);

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

/**
 * Convert color histogram to JSON.
 * 
 * @param histogram Histogram to convert
 * @param include_metadata Include analysis metadata
 * @return JSON value or NULL on error
 */
json_value *color_histogram_to_json(const color_histogram_t *histogram,
                                   bool include_metadata);

/**
 * Convert color search result to JSON.
 * 
 * @param result Search result to convert
 * @param include_metadata Include search metadata
 * @return JSON value or NULL on error
 */
json_value *color_search_result_to_json(const color_search_result_t *result,
                                       bool include_metadata);

/**
 * Convert unique colors result to JSON.
 * 
 * @param result Unique colors to convert
 * @param include_metadata Include analysis metadata
 * @return JSON value or NULL on error
 */
json_value *unique_colors_result_to_json(const unique_colors_result_t *result,
                                        bool include_metadata);

// ============================================================================
// CACHING FUNCTIONS
// ============================================================================

/**
 * Generate cache key for color analysis request.
 * 
 * @param layer_id Layer ID
 * @param region_min Region min (can be NULL)
 * @param region_max Region max (can be NULL)
 * @param extra_data Extra data for key generation
 * @param extra_size Size of extra data
 * @return Cache key
 */
uint64_t color_analysis_cache_key(int layer_id,
                                 const int region_min[3],
                                 const int region_max[3],
                                 const void *extra_data,
                                 size_t extra_size);

/**
 * Check if cached result is still valid.
 * 
 * @param ctx Goxel core context
 * @param cache_key Cache key
 * @param timestamp Timestamp of cached result
 * @return true if valid, false if stale
 */
bool color_analysis_cache_valid(goxel_core_context_t *ctx,
                               uint64_t cache_key,
                               uint64_t timestamp);

#endif // COLOR_ANALYSIS_H