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

#include "test_methods.h"
#include "json_rpc.h"
#include "../utils/json.h"
#include "../log.h"
#include "../core/goxel_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ============================================================================
// TEST METHOD IMPLEMENTATIONS
// ============================================================================

/**
 * Echo method - returns the input parameters
 * Useful for testing JSON-RPC communication
 */
json_rpc_response_t *handle_echo(const json_rpc_request_t *request)
{
    LOG_D("Handling echo method");
    
    // Simply return the params as the result
    json_value *result = NULL;
    
    if (request->params.data) {
        // Clone the params data
        result = json_value_clone(request->params.data);
        if (!result) {
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Failed to clone parameters",
                                                 NULL, &request->id);
        }
    } else {
        // No params, return empty object
        result = json_object_new(0);
    }
    
    return json_rpc_create_response_result(result, &request->id);
}

/**
 * Version method - returns Goxel version information
 */
json_rpc_response_t *handle_version(const json_rpc_request_t *request)
{
    LOG_D("Handling version method");
    
    json_value *result = json_object_new(3);
    json_object_push(result, "version", json_string_new(GOXEL_VERSION_STR));
    json_object_push(result, "type", json_string_new("daemon"));
    json_object_push(result, "protocol", json_string_new("JSON-RPC 2.0"));
    
    return json_rpc_create_response_result(result, &request->id);
}

/**
 * Status method - returns daemon status information
 */
json_rpc_response_t *handle_status(const json_rpc_request_t *request)
{
    LOG_D("Handling status method");
    
    json_value *result = json_object_new(8);
    
    // Basic status info
    json_object_push(result, "status", json_string_new("running"));
    json_object_push(result, "pid", json_integer_new(getpid()));
    
    // Timestamps
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    json_object_push(result, "current_time", json_string_new(time_str));
    
    // System info
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        json_object_push(result, "hostname", json_string_new(hostname));
    }
    
    // Goxel context status
    bool goxel_initialized = json_rpc_is_goxel_initialized();
    json_object_push(result, "goxel_initialized", json_boolean_new(goxel_initialized));
    
    // Available methods count
    json_object_push(result, "methods_available", json_integer_new(json_rpc_get_method_count()));
    
    // Memory usage (simplified)
    json_object_push(result, "memory_allocated", json_integer_new(0)); // TODO: Track actual memory
    
    // Uptime
    json_object_push(result, "uptime_seconds", json_integer_new(0)); // TODO: Track actual uptime
    
    return json_rpc_create_response_result(result, &request->id);
}

/**
 * List methods - returns all available JSON-RPC methods
 */
json_rpc_response_t *handle_list_methods(const json_rpc_request_t *request)
{
    LOG_D("Handling list_methods");
    
    // Get method list from json_rpc module
    // Increased buffer size to handle more methods
    char buffer[16384]; // 16KB should be plenty for method list
    if (json_rpc_list_methods(buffer, sizeof(buffer)) != 0) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Method list too large or internal error",
                                             NULL, &request->id);
    }
    
    // Parse the method list and convert to JSON array
    json_value *methods_array = json_array_new(0);
    char *line = strtok(buffer, "\n");
    
    while (line) {
        // Parse "method_name - description" format
        char *dash = strstr(line, " - ");
        if (dash) {
            *dash = '\0';
            const char *method_name = line;
            const char *description = dash + 3;
            
            json_value *method_obj = json_object_new(2);
            json_object_push(method_obj, "method", json_string_new(method_name));
            json_object_push(method_obj, "description", json_string_new(description));
            json_array_push(methods_array, method_obj);
        }
        
        line = strtok(NULL, "\n");
    }
    
    json_value *result = json_object_new(2);
    json_object_push(result, "count", json_integer_new(json_array_length(methods_array)));
    json_object_push(result, "methods", methods_array);
    
    return json_rpc_create_response_result(result, &request->id);
}

/**
 * Ping method - simple health check
 */
json_rpc_response_t *handle_ping(const json_rpc_request_t *request)
{
    LOG_D("Handling ping method");
    
    json_value *result = json_object_new(2);
    json_object_push(result, "pong", json_boolean_new(1));
    
    // Add timestamp
    time_t now = time(NULL);
    json_object_push(result, "timestamp", json_integer_new(now));
    
    return json_rpc_create_response_result(result, &request->id);
}

// ============================================================================
// BATCH VOXEL OPERATIONS
// ============================================================================

/**
 * Add multiple voxels in a single operation (performance optimization)
 */
json_rpc_response_t *handle_add_voxels(const json_rpc_request_t *request)
{
    LOG_D("Handling add_voxels method");
    
    if (!json_rpc_is_goxel_initialized()) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: voxels (array of voxel objects)
    json_value *voxels_param = NULL;
    json_rpc_result_t result;
    
    if (request->params.type == JSON_RPC_PARAMS_ARRAY) {
        result = json_rpc_get_param_by_index(&request->params, 0, &voxels_param);
    } else if (request->params.type == JSON_RPC_PARAMS_OBJECT) {
        result = json_rpc_get_param_by_name(&request->params, "voxels", &voxels_param);
    } else {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Missing voxels parameter",
                                             NULL, &request->id);
    }
    
    if (result != JSON_RPC_SUCCESS || !voxels_param || voxels_param->type != json_array) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Invalid voxels parameter (must be array)",
                                             NULL, &request->id);
    }
    
    int success_count = 0;
    int failed_count = 0;
    
    // Process each voxel
    for (unsigned int i = 0; i < json_array_length(voxels_param); i++) {
        json_value *voxel = json_array_get(voxels_param, i);
        if (!voxel || voxel->type != json_object) {
            failed_count++;
            continue;
        }
        
        // Extract voxel properties
        json_value *x_val = json_object_get(voxel, "x");
        json_value *y_val = json_object_get(voxel, "y");
        json_value *z_val = json_object_get(voxel, "z");
        json_value *r_val = json_object_get(voxel, "r");
        json_value *g_val = json_object_get(voxel, "g");
        json_value *b_val = json_object_get(voxel, "b");
        json_value *a_val = json_object_get(voxel, "a");
        
        if (!x_val || !y_val || !z_val || !r_val || !g_val || !b_val ||
            x_val->type != json_integer || y_val->type != json_integer ||
            z_val->type != json_integer || r_val->type != json_integer ||
            g_val->type != json_integer || b_val->type != json_integer) {
            failed_count++;
            continue;
        }
        
        int x = (int)x_val->u.integer;
        int y = (int)y_val->u.integer;
        int z = (int)z_val->u.integer;
        int r = (int)r_val->u.integer;
        int g = (int)g_val->u.integer;
        int b = (int)b_val->u.integer;
        int a = (a_val && a_val->type == json_integer) ? (int)a_val->u.integer : 255;
        
        // Validate color values
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
            failed_count++;
            continue;
        }
        
        uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
        
        // Add voxel using the existing API
        if (json_rpc_add_voxel_internal(x, y, z, rgba, 0) == 0) {
            success_count++;
        } else {
            failed_count++;
        }
    }
    
    // Create response
    json_value *result_obj = json_object_new(3);
    json_object_push(result_obj, "success", json_boolean_new(failed_count == 0));
    json_object_push(result_obj, "added", json_integer_new(success_count));
    json_object_push(result_obj, "failed", json_integer_new(failed_count));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

// ============================================================================
// TEST METHOD REGISTRY
// ============================================================================

static const test_method_entry_t test_methods[] = {
    {"echo", handle_echo, "Echo back the input parameters"},
    {"version", handle_version, "Get Goxel version information"},
    {"status", handle_status, "Get daemon status information"},
    {"ping", handle_ping, "Simple health check"},
    {"list_methods", handle_list_methods, "List all available methods"},
    {"add_voxels", handle_add_voxels, "Add multiple voxels in batch"}
};

static const size_t test_method_count = sizeof(test_methods) / sizeof(test_methods[0]);

const test_method_entry_t *get_test_methods(size_t *count)
{
    if (count) {
        *count = test_method_count;
    }
    return test_methods;
}

json_rpc_response_t *handle_test_method(const char *method_name, 
                                       const json_rpc_request_t *request)
{
    for (size_t i = 0; i < test_method_count; i++) {
        if (strcmp(method_name, test_methods[i].name) == 0) {
            return test_methods[i].handler(request);
        }
    }
    
    return NULL; // Method not found
}

// ============================================================================
// JSON VALUE UTILITIES
// ============================================================================

json_value *json_value_clone(const json_value *src)
{
    if (!src) return NULL;
    
    switch (src->type) {
        case json_null:
            return json_null_new();
            
        case json_boolean:
            return json_boolean_new(src->u.boolean);
            
        case json_integer:
            return json_integer_new(src->u.integer);
            
        case json_double:
            return json_double_new(src->u.dbl);
            
        case json_string:
            return json_string_new(src->u.string.ptr);
            
        case json_array: {
            json_value *arr = json_array_new(json_array_length(src));
            for (unsigned int i = 0; i < json_array_length(src); i++) {
                json_value *cloned = json_value_clone(json_array_get(src, i));
                if (cloned) {
                    json_array_push(arr, cloned);
                }
            }
            return arr;
        }
        
        case json_object: {
            json_value *obj = json_object_new(json_object_length(src));
            for (unsigned int i = 0; i < json_object_length(src); i++) {
                const char *key = json_object_get_key(src, i);
                json_value *val = json_object_get_value(src, i);
                json_value *cloned = json_value_clone(val);
                if (cloned) {
                    json_object_push(obj, key, cloned);
                }
            }
            return obj;
        }
        
        default:
            return NULL;
    }
}

// Helper functions for json_value access
unsigned int json_array_length(const json_value *array)
{
    if (!array || array->type != json_array) return 0;
    return array->u.array.length;
}

json_value *json_array_get(const json_value *array, unsigned int index)
{
    if (!array || array->type != json_array || index >= array->u.array.length) {
        return NULL;
    }
    return array->u.array.values[index];
}

unsigned int json_object_length(const json_value *object)
{
    if (!object || object->type != json_object) return 0;
    return object->u.object.length;
}

const char *json_object_get_key(const json_value *object, unsigned int index)
{
    if (!object || object->type != json_object || index >= object->u.object.length) {
        return NULL;
    }
    return object->u.object.values[index].name;
}

json_value *json_object_get_value(const json_value *object, unsigned int index)
{
    if (!object || object->type != json_object || index >= object->u.object.length) {
        return NULL;
    }
    return object->u.object.values[index].value;
}

json_value *json_object_get(const json_value *object, const char *key)
{
    if (!object || object->type != json_object || !key) return NULL;
    
    for (unsigned int i = 0; i < object->u.object.length; i++) {
        if (strcmp(object->u.object.values[i].name, key) == 0) {
            return object->u.object.values[i].value;
        }
    }
    
    return NULL;
}