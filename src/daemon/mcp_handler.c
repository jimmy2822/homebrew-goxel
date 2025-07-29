/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#include "mcp_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

static bool g_mcp_initialized = false;
static mcp_handler_stats_t g_stats = {0};

// Static ID counter for request generation
static int64_t g_request_id_counter = 1;

// ============================================================================
// METHOD MAPPING TABLE
// ============================================================================

static const mcp_method_mapping_t g_method_mappings[] = {
    // File operations - Direct mapping
    {"goxel_create_project", "goxel.create_project", NULL, NULL, 
     "Create a new Goxel project"},
    {"goxel_save_file", "goxel.save_project", NULL, NULL,
     "Save Goxel project to file"},
    {"goxel_export_file", "goxel.export_model", NULL, NULL,
     "Export model to various formats"},
    
    // File operations - With parameter mapping
    {"goxel_open_file", "goxel.load_project", mcp_map_open_file_params, NULL,
     "Open Goxel project or 3D file"},
    
    // Voxel operations - Basic
    {"goxel_get_voxel", "goxel.get_voxel", mcp_map_voxel_position_params, NULL,
     "Get voxel color at position"},
    {"goxel_add_voxels", "goxel.add_voxel", mcp_map_voxel_position_params, NULL,
     "Add voxel at specified position"},
    {"goxel_remove_voxels", "goxel.remove_voxel", mcp_map_voxel_position_params, NULL,
     "Remove voxel at specified position"},
     
    // Voxel operations - Batch
    {"goxel_batch_voxel_operations", "goxel.batch_operations", mcp_map_batch_voxel_params, NULL,
     "Perform multiple voxel operations efficiently"},
    
    // Layer operations - Direct mapping
    {"goxel_new_layer", "goxel.create_layer", NULL, NULL,
     "Create new layer"},
    {"goxel_list_layers", "goxel.list_layers", NULL, NULL,
     "List all layers in project"},
     
    // System operations - Direct mapping
    {"ping", "ping", NULL, NULL, "Health check"},
    {"version", "version", NULL, NULL, "Get version information"},
    {"list_methods", "list_methods", NULL, NULL, "List available methods"},
};

static const size_t g_method_mappings_count = sizeof(g_method_mappings) / sizeof(g_method_mappings[0]);

// ============================================================================
// JSON HELPER FUNCTIONS
// ============================================================================

/**
 * Helper to get object member by name
 */
static json_value *json_object_get_helper(const json_value *obj, const char *key) {
    if (!obj || obj->type != json_object || !key) return NULL;
    
    for (unsigned int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, key) == 0) {
            return obj->u.object.values[i].value;
        }
    }
    return NULL;
}

/**
 * Helper to safely clone a JSON value
 */
static json_value *json_value_clone(json_value *src) {
    if (!src) return NULL;
    
    switch (src->type) {
        case json_string:
            return json_string_new(src->u.string.ptr);
        case json_integer:
            return json_integer_new(src->u.integer);
        case json_double:
            return json_double_new(src->u.dbl);
        case json_boolean:
            return json_boolean_new(src->u.boolean);
        case json_null:
            return json_null_new();
        case json_object: {
            json_value *clone = json_object_new(src->u.object.length);
            for (unsigned int i = 0; i < src->u.object.length; i++) {
                json_object_push(clone, 
                    src->u.object.values[i].name,
                    json_value_clone(src->u.object.values[i].value));
            }
            return clone;
        }
        case json_array: {
            json_value *clone = json_array_new(src->u.array.length);
            for (unsigned int i = 0; i < src->u.array.length; i++) {
                clone->u.array.values[i] = json_value_clone(src->u.array.values[i]);
            }
            return clone;
        }
        default:
            return NULL;
    }
}

/**
 * Helper to set array value at specific index
 */
static void json_array_set(json_value *array, unsigned int index, json_value *value) {
    if (!array || array->type != json_array || index >= array->u.array.length) return;
    array->u.array.values[index] = value;
}

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * Get current time in microseconds for timing
 */
static uint64_t get_time_microseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * Generate unique request ID
 */
static int64_t generate_request_id(void)
{
    return __sync_fetch_and_add(&g_request_id_counter, 1);
}

/**
 * Find method mapping by MCP tool name
 */
static const mcp_method_mapping_t *find_method_mapping(const char *mcp_tool)
{
    if (!mcp_tool) return NULL;
    
    for (size_t i = 0; i < g_method_mappings_count; i++) {
        if (strcmp(g_method_mappings[i].mcp_tool, mcp_tool) == 0) {
            return &g_method_mappings[i];
        }
    }
    return NULL;
}

/**
 * Update statistics with timing information
 */
static void update_translation_stats(uint64_t start_time, bool is_error)
{
    uint64_t end_time = get_time_microseconds();
    uint64_t duration = end_time - start_time;
    
    __sync_fetch_and_add(&g_stats.total_translation_time_us, duration);
    
    if (is_error) {
        __sync_fetch_and_add(&g_stats.translation_errors, 1);
    } else {
        __sync_fetch_and_add(&g_stats.requests_translated, 1);
    }
    
    // Update average (simple approximation)
    g_stats.avg_translation_time_us = 
        (double)g_stats.total_translation_time_us / 
        (g_stats.requests_translated + g_stats.translation_errors);
}

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================

mcp_error_code_t mcp_handler_init(void)
{
    if (g_mcp_initialized) {
        return MCP_SUCCESS;
    }
    
    // Initialize JSON-RPC context if needed  
    json_rpc_result_t result = json_rpc_init_goxel_context();
    if (result != JSON_RPC_SUCCESS) {
        return MCP_ERROR_INTERNAL;
    }
    
    // Reset statistics
    memset(&g_stats, 0, sizeof(g_stats));
    
    g_mcp_initialized = true;
    return MCP_SUCCESS;
}

void mcp_handler_cleanup(void)
{
    if (!g_mcp_initialized) {
        return;
    }
    
    json_rpc_cleanup_goxel_context();
    g_mcp_initialized = false;
}

bool mcp_handler_is_initialized(void)
{
    return g_mcp_initialized;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void mcp_free_request(mcp_tool_request_t *request)
{
    if (!request) return;
    
    free(request->tool);
    if (request->arguments) {
        json_value_free(request->arguments);
    }
    free(request);
}

void mcp_free_response(mcp_tool_response_t *response)
{
    if (!response) return;
    
    if (response->content) {
        json_value_free(response->content);
    }
    free(response->error_message);
    free(response);
}

// ============================================================================
// PROTOCOL TRANSLATION CORE
// ============================================================================

mcp_error_code_t mcp_translate_request(
    const mcp_tool_request_t *mcp_request,
    json_rpc_request_t **jsonrpc_request)
{
    if (!g_mcp_initialized) {
        return MCP_ERROR_INTERNAL;
    }
    
    if (!mcp_request || !jsonrpc_request) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    uint64_t start_time = get_time_microseconds();
    
    // Find method mapping
    const mcp_method_mapping_t *mapping = find_method_mapping(mcp_request->tool);
    if (!mapping) {
        update_translation_stats(start_time, true);
        return MCP_ERROR_INVALID_TOOL;
    }
    
    // Create request ID
    json_rpc_id_t id;
    json_rpc_result_t result = json_rpc_create_id_number(generate_request_id(), &id);
    if (result != JSON_RPC_SUCCESS) {
        update_translation_stats(start_time, true);
        return MCP_ERROR_INTERNAL;
    }
    
    json_value *params = NULL;
    
    if (mapping->param_mapper && mcp_request->arguments) {
        // Custom parameter mapping
        __sync_fetch_and_add(&g_stats.mapped_translations, 1);
        result = mapping->param_mapper(mcp_request->arguments, &params);
        if (result != JSON_RPC_SUCCESS) {
            json_rpc_free_id(&id);
            update_translation_stats(start_time, true);
            return MCP_ERROR_TRANSLATION;
        }
    } else if (mcp_request->arguments) {
        // Direct parameter passing - zero-copy clone
        __sync_fetch_and_add(&g_stats.direct_translations, 1);
        params = json_value_clone(mcp_request->arguments);
        if (!params) {
            json_rpc_free_id(&id);
            update_translation_stats(start_time, true);
            return MCP_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // Create JSON-RPC request
    if (params) {
        *jsonrpc_request = json_rpc_create_request_object(
            mapping->jsonrpc_method, params, &id);
    } else {
        // Create request without parameters
        json_value *empty_params = json_object_new(0);
        *jsonrpc_request = json_rpc_create_request_object(
            mapping->jsonrpc_method, empty_params, &id);
    }
    
    json_rpc_free_id(&id);
    
    if (!*jsonrpc_request) {
        if (params) json_value_free(params);
        update_translation_stats(start_time, true);
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    update_translation_stats(start_time, false);
    return MCP_SUCCESS;
}

mcp_error_code_t mcp_translate_response(
    const json_rpc_response_t *jsonrpc_response,
    const char *mcp_tool_name,
    mcp_tool_response_t **mcp_response)
{
    if (!g_mcp_initialized) {
        return MCP_ERROR_INTERNAL;
    }
    
    if (!jsonrpc_response || !mcp_response) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    uint64_t start_time = get_time_microseconds();
    
    // Create MCP response
    mcp_tool_response_t *response = calloc(1, sizeof(mcp_tool_response_t));
    if (!response) {
        update_translation_stats(start_time, true);
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    if (jsonrpc_response->has_error) {
        // Error response
        response->success = false;
        response->error_code = mcp_map_jsonrpc_error(jsonrpc_response->error.code);
        
        if (jsonrpc_response->error.message) {
            response->error_message = strdup(jsonrpc_response->error.message);
        }
        
        // Include error data if present
        if (jsonrpc_response->error.data) {
            response->content = json_value_clone(jsonrpc_response->error.data);
        }
    } else if (jsonrpc_response->has_result) {
        // Success response
        response->success = true;
        response->error_code = MCP_SUCCESS;
        
        // Clone result for content - zero-copy when possible
        if (jsonrpc_response->result) {
            response->content = json_value_clone(jsonrpc_response->result);
            if (!response->content) {
                mcp_free_response(response);
                update_translation_stats(start_time, true);
                return MCP_ERROR_OUT_OF_MEMORY;
            }
        }
    } else {
        // Invalid response
        response->success = false;
        response->error_code = MCP_ERROR_INTERNAL;
        response->error_message = strdup("Invalid JSON-RPC response format");
    }
    
    *mcp_response = response;
    __sync_fetch_and_add(&g_stats.responses_translated, 1);
    update_translation_stats(start_time, false);
    return MCP_SUCCESS;
}

mcp_error_code_t mcp_handle_tool_request(
    const mcp_tool_request_t *mcp_request,
    mcp_tool_response_t **mcp_response)
{
    if (!g_mcp_initialized) {
        return MCP_ERROR_INTERNAL;
    }
    
    if (!mcp_request || !mcp_response) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    // Translate MCP request to JSON-RPC
    json_rpc_request_t *jsonrpc_request = NULL;
    mcp_error_code_t result = mcp_translate_request(mcp_request, &jsonrpc_request);
    if (result != MCP_SUCCESS) {
        return result;
    }
    
    // Execute JSON-RPC request
    json_rpc_response_t *jsonrpc_response = json_rpc_handle_method(jsonrpc_request);
    if (!jsonrpc_response) {
        json_rpc_free_request(jsonrpc_request);
        return MCP_ERROR_INTERNAL;
    }
    
    // Translate JSON-RPC response to MCP
    result = mcp_translate_response(jsonrpc_response, mcp_request->tool, mcp_response);
    
    // Cleanup
    json_rpc_free_request(jsonrpc_request);
    json_rpc_free_response(jsonrpc_response);
    
    return result;
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

mcp_error_code_t mcp_handle_batch_requests(
    const mcp_tool_request_t *requests,
    size_t count,
    mcp_tool_response_t **responses)
{
    if (!g_mcp_initialized) {
        return MCP_ERROR_INTERNAL;
    }
    
    if (!requests || !responses || count == 0) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    if (count > MCP_MAX_BATCH_SIZE) {
        return MCP_ERROR_BATCH_TOO_LARGE;
    }
    
    __sync_fetch_and_add(&g_stats.batch_requests, 1);
    
    // Allocate response array
    mcp_tool_response_t *batch_responses = calloc(count, sizeof(mcp_tool_response_t));
    if (!batch_responses) {
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    mcp_error_code_t overall_result = MCP_SUCCESS;
    
    // Process each request
    for (size_t i = 0; i < count; i++) {
        mcp_tool_response_t *response = NULL;
        mcp_error_code_t result = mcp_handle_tool_request(&requests[i], &response);
        
        if (result == MCP_SUCCESS && response) {
            // Copy response data
            batch_responses[i] = *response;
            // Don't free the response data, just the structure
            free(response);
        } else {
            // Create error response
            batch_responses[i].success = false;
            batch_responses[i].error_code = result;
            batch_responses[i].error_message = strdup(mcp_error_string(result));
            overall_result = result; // Track that we had failures
        }
    }
    
    *responses = batch_responses;
    return overall_result;
}

// ============================================================================
// DISCOVERY AND INTROSPECTION
// ============================================================================

const char **mcp_get_available_tools(size_t *count)
{
    if (!count) return NULL;
    
    static const char *tool_names[sizeof(g_method_mappings) / sizeof(g_method_mappings[0])];
    static bool initialized = false;
    
    if (!initialized) {
        for (size_t i = 0; i < g_method_mappings_count; i++) {
            tool_names[i] = g_method_mappings[i].mcp_tool;
        }
        initialized = true;
    }
    
    *count = g_method_mappings_count;
    return tool_names;
}

const char *mcp_get_tool_description(const char *tool_name)
{
    const mcp_method_mapping_t *mapping = find_method_mapping(tool_name);
    return mapping ? mapping->description : NULL;
}

bool mcp_is_tool_available(const char *tool_name)
{
    return find_method_mapping(tool_name) != NULL;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

const char *mcp_error_string(mcp_error_code_t error_code)
{
    switch (error_code) {
        case MCP_SUCCESS: return "Success";
        case MCP_ERROR_INVALID_TOOL: return "Unknown tool name";
        case MCP_ERROR_INVALID_PARAMS: return "Invalid parameters";
        case MCP_ERROR_INTERNAL: return "Internal error";
        case MCP_ERROR_NOT_IMPLEMENTED: return "Tool not implemented";
        case MCP_ERROR_TRANSLATION: return "Protocol translation error";
        case MCP_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case MCP_ERROR_BATCH_TOO_LARGE: return "Batch size exceeds limit";
        default: return "Unknown error";
    }
}

mcp_error_code_t mcp_map_jsonrpc_error(int32_t jsonrpc_error)
{
    switch (jsonrpc_error) {
        case JSON_RPC_PARSE_ERROR:
        case JSON_RPC_INVALID_REQUEST:
            return MCP_ERROR_INVALID_PARAMS;
        case JSON_RPC_METHOD_NOT_FOUND:
            return MCP_ERROR_INVALID_TOOL;
        case JSON_RPC_INVALID_PARAMS:
            return MCP_ERROR_INVALID_PARAMS;
        case JSON_RPC_INTERNAL_ERROR:
        default:
            return MCP_ERROR_INTERNAL;
    }
}

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

void mcp_get_handler_stats(mcp_handler_stats_t *stats)
{
    if (!stats) return;
    *stats = g_stats;
}

void mcp_reset_handler_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}

// ============================================================================
// PARAMETER MAPPING IMPLEMENTATIONS
// ============================================================================

json_rpc_result_t mcp_map_open_file_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params)
{
    if (!mcp_args || mcp_args->type != json_object || !jsonrpc_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Create new parameter object
    json_value *params = json_object_new(0);
    if (!params) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Map path parameter (direct copy)
    json_value *path = json_object_get_helper(mcp_args, "path");
    if (path) {
        json_object_push(params, "path", json_value_clone(path));
    }
    
    // Map format parameter (direct copy)  
    json_value *format = json_object_get_helper(mcp_args, "format");
    if (format) {
        json_object_push(params, "format", json_value_clone(format));
    }
    
    *jsonrpc_params = params;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t mcp_map_voxel_position_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params)
{
    if (!mcp_args || mcp_args->type != json_object || !jsonrpc_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Create new parameter object
    json_value *params = json_object_new(0);
    if (!params) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Extract position object
    json_value *position = json_object_get_helper(mcp_args, "position");
    if (position && position->type == json_object) {
        // Flatten position {x, y, z} to individual parameters
        json_value *x = json_object_get_helper(position, "x");
        json_value *y = json_object_get_helper(position, "y");
        json_value *z = json_object_get_helper(position, "z");
        
        if (x) json_object_push(params, "x", json_value_clone(x));
        if (y) json_object_push(params, "y", json_value_clone(y));
        if (z) json_object_push(params, "z", json_value_clone(z));
    }
    
    // Map color parameter if present
    json_value *color = json_object_get_helper(mcp_args, "color");
    if (color && color->type == json_object) {
        // Convert color object {r, g, b, a} to rgba array
        json_value *rgba = json_array_new(4);
        
        json_value *r = json_object_get_helper(color, "r");
        json_value *g = json_object_get_helper(color, "g");
        json_value *b = json_object_get_helper(color, "b");
        json_value *a = json_object_get_helper(color, "a");
        
        json_array_set(rgba, 0, r ? json_value_clone(r) : json_integer_new(255));
        json_array_set(rgba, 1, g ? json_value_clone(g) : json_integer_new(255));
        json_array_set(rgba, 2, b ? json_value_clone(b) : json_integer_new(255));
        json_array_set(rgba, 3, a ? json_value_clone(a) : json_integer_new(255));
        
        json_object_push(params, "rgba", rgba);
    }
    
    // Copy other parameters directly
    for (unsigned int i = 0; i < mcp_args->u.object.length; i++) {
        const char *key = mcp_args->u.object.values[i].name;
        if (strcmp(key, "position") != 0 && strcmp(key, "color") != 0) {
            json_value *value = mcp_args->u.object.values[i].value;
            json_object_push(params, key, json_value_clone(value));
        }
    }
    
    *jsonrpc_params = params;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t mcp_map_color_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params)
{
    if (!mcp_args || mcp_args->type != json_object || !jsonrpc_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Create new parameter object
    json_value *params = json_object_new(0);
    if (!params) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Convert color object to rgba array
    json_value *color = json_object_get_helper(mcp_args, "color");
    if (color && color->type == json_object) {
        json_value *rgba = json_array_new(4);
        
        json_value *r = json_object_get_helper(color, "r");
        json_value *g = json_object_get_helper(color, "g");
        json_value *b = json_object_get_helper(color, "b");
        json_value *a = json_object_get_helper(color, "a");
        
        json_array_set(rgba, 0, r ? json_value_clone(r) : json_integer_new(255));
        json_array_set(rgba, 1, g ? json_value_clone(g) : json_integer_new(255));
        json_array_set(rgba, 2, b ? json_value_clone(b) : json_integer_new(255));
        json_array_set(rgba, 3, a ? json_value_clone(a) : json_integer_new(255));
        
        json_object_push(params, "rgba", rgba);
    }
    
    // Copy other parameters directly
    for (unsigned int i = 0; i < mcp_args->u.object.length; i++) {
        const char *key = mcp_args->u.object.values[i].name;
        if (strcmp(key, "color") != 0) {
            json_value *value = mcp_args->u.object.values[i].value;
            json_object_push(params, key, json_value_clone(value));
        }
    }
    
    *jsonrpc_params = params;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t mcp_map_batch_voxel_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params)
{
    if (!mcp_args || mcp_args->type != json_object || !jsonrpc_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    json_value *operations = json_object_get_helper(mcp_args, "operations");
    if (!operations || operations->type != json_array) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Create batch operations array
    json_value *batch = json_array_new(operations->u.array.length);
    if (!batch) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Convert each operation
    for (unsigned int i = 0; i < operations->u.array.length; i++) {
        json_value *op = operations->u.array.values[i];
        if (op->type != json_object) continue;
        
        json_value *batch_op = json_object_new(0);
        
        // Map operation type
        json_value *type = json_object_get_helper(op, "type");
        if (type) {
            json_object_push(batch_op, "type", json_value_clone(type));
        }
        
        // Map position and color using existing mappers
        json_rpc_result_t result = mcp_map_voxel_position_params(op, &batch_op);
        if (result != JSON_RPC_SUCCESS) {
            json_value_free(batch_op);
            json_value_free(batch);
            return result;
        }
        
        json_array_push(batch, batch_op);
    }
    
    // Create final parameters
    json_value *params = json_object_new(0);
    json_object_push(params, "operations", batch);
    
    *jsonrpc_params = params;
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// PARSING AND SERIALIZATION HELPERS
// ============================================================================

mcp_error_code_t mcp_parse_request(const char *json_str, mcp_tool_request_t **request)
{
    if (!json_str || !request) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    // Parse JSON
    json_value *json = json_parse(json_str, strlen(json_str));
    if (!json || json->type != json_object) {
        if (json) json_value_free(json);
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    // Create request structure
    mcp_tool_request_t *req = calloc(1, sizeof(mcp_tool_request_t));
    if (!req) {
        json_value_free(json);
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    // Extract tool name
    json_value *tool = json_object_get_helper(json, "tool");
    if (tool && tool->type == json_string) {
        req->tool = strdup(tool->u.string.ptr);
    }
    
    // Extract arguments
    json_value *args = json_object_get_helper(json, "arguments");
    if (args) {
        req->arguments = json_value_clone(args);
    }
    
    json_value_free(json);
    
    if (!req->tool) {
        mcp_free_request(req);
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    *request = req;
    return MCP_SUCCESS;
}

mcp_error_code_t mcp_serialize_response(const mcp_tool_response_t *response, char **json_str)
{
    if (!response || !json_str) {
        return MCP_ERROR_INVALID_PARAMS;
    }
    
    // Create response object
    json_value *json = json_object_new(0);
    if (!json) {
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    // Add success field
    json_object_push(json, "success", json_boolean_new(response->success));
    
    // Add content or error
    if (response->success && response->content) {
        json_object_push(json, "content", json_value_clone(response->content));
    } else if (!response->success) {
        json_object_push(json, "error_code", json_integer_new(response->error_code));
        if (response->error_message) {
            json_object_push(json, "error_message", 
                json_string_new(response->error_message));
        }
        if (response->content) {
            json_object_push(json, "error_data", json_value_clone(response->content));
        }
    }
    
    // Serialize to string
    char *str = malloc(json_measure(json));
    if (!str) {
        json_value_free(json);
        return MCP_ERROR_OUT_OF_MEMORY;
    }
    
    json_serialize(str, json);
    json_value_free(json);
    
    *json_str = str;
    return MCP_SUCCESS;
}