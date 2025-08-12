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

#include "json_rpc.h"
#include "test_methods.h"
#include "bulk_voxel_ops.h"
#include "color_analysis.h"
#include "worker_pool.h"
#include "project_mutex.h"
#include "render_manager.h"
#include "../core/utils/json.h"
#include "../../ext_src/json/json-builder.h"
#include "../../ext_src/json/json.h"
#include "../log.h"
#include "../core/goxel_core.h"
#include "../goxel.h"
#include "../script.h"
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

// ============================================================================
// UTILITY MACROS
// ============================================================================

#define SAFE_FREE(ptr) do { \
    if (ptr) { \
        free(ptr); \
        ptr = NULL; \
    } \
} while(0)

#define SAFE_STRDUP(str) ((str) ? strdup(str) : NULL)

// ============================================================================
// ERROR HANDLING IMPLEMENTATION
// ============================================================================

const char *json_rpc_result_string(json_rpc_result_t result)
{
    switch (result) {
        case JSON_RPC_SUCCESS: return "Success";
        case JSON_RPC_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case JSON_RPC_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case JSON_RPC_ERROR_PARSE_FAILED: return "JSON parsing failed";
        case JSON_RPC_ERROR_INVALID_JSON: return "Invalid JSON structure";
        case JSON_RPC_ERROR_MISSING_FIELD: return "Required field missing";
        case JSON_RPC_ERROR_INVALID_VERSION: return "Invalid JSON-RPC version";
        case JSON_RPC_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case JSON_RPC_ERROR_UNKNOWN:
        default: return "Unknown error";
    }
}

const char *json_rpc_error_message(int32_t error_code)
{
    switch (error_code) {
        case JSON_RPC_PARSE_ERROR: return "Parse error";
        case JSON_RPC_INVALID_REQUEST: return "Invalid Request";
        case JSON_RPC_METHOD_NOT_FOUND: return "Method not found";
        case JSON_RPC_INVALID_PARAMS: return "Invalid params";
        case JSON_RPC_INTERNAL_ERROR: return "Internal error";
        default:
            if (json_rpc_is_server_error(error_code)) {
                return "Server error";
            } else if (json_rpc_is_application_error(error_code)) {
                return "Application error";
            } else {
                return "Unknown error";
            }
    }
}

bool json_rpc_is_server_error(int32_t error_code)
{
    return error_code >= JSON_RPC_SERVER_ERROR_START && 
           error_code <= JSON_RPC_SERVER_ERROR_END;
}

bool json_rpc_is_application_error(int32_t error_code)
{
    // Application errors are outside the reserved ranges:
    // Reserved: -32768 to -32000 (server errors)
    // Reserved: -32700 to -32600 (standard errors)  
    return (error_code > -32000) || (error_code < -32768);
}

// ============================================================================
// ID MANAGEMENT IMPLEMENTATION
// ============================================================================

json_rpc_result_t json_rpc_create_id_number(int64_t number, json_rpc_id_t *id)
{
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    id->type = JSON_RPC_ID_NUMBER;
    id->value.number = number;
    
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_create_id_string(const char *string, json_rpc_id_t *id)
{
    if (!id || !string) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    id->type = JSON_RPC_ID_STRING;
    id->value.string = strdup(string);
    if (!id->value.string) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_create_id_null(json_rpc_id_t *id)
{
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    id->type = JSON_RPC_ID_NULL;
    id->value.number = 0; // Clear the union
    
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_clone_id(const json_rpc_id_t *src, json_rpc_id_t *dst)
{
    if (!src || !dst) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    dst->type = src->type;
    
    switch (src->type) {
        case JSON_RPC_ID_NULL:
            dst->value.number = 0;
            break;
            
        case JSON_RPC_ID_NUMBER:
            dst->value.number = src->value.number;
            break;
            
        case JSON_RPC_ID_STRING:
            if (src->value.string) {
                dst->value.string = strdup(src->value.string);
                if (!dst->value.string) {
                    return JSON_RPC_ERROR_OUT_OF_MEMORY;
                }
            } else {
                dst->value.string = NULL;
            }
            break;
            
        default:
            return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    return JSON_RPC_SUCCESS;
}

void json_rpc_free_id(json_rpc_id_t *id)
{
    if (!id) return;
    
    if (id->type == JSON_RPC_ID_STRING) {
        SAFE_FREE(id->value.string);
    }
    
    id->type = JSON_RPC_ID_NULL;
    id->value.number = 0;
}

bool json_rpc_id_equals(const json_rpc_id_t *id1, const json_rpc_id_t *id2)
{
    if (!id1 || !id2) return false;
    if (id1->type != id2->type) return false;
    
    switch (id1->type) {
        case JSON_RPC_ID_NULL:
            return true;
            
        case JSON_RPC_ID_NUMBER:
            return id1->value.number == id2->value.number;
            
        case JSON_RPC_ID_STRING:
            if (!id1->value.string && !id2->value.string) return true;
            if (!id1->value.string || !id2->value.string) return false;
            return strcmp(id1->value.string, id2->value.string) == 0;
            
        default:
            return false;
    }
}

json_rpc_result_t json_rpc_validate_id(const json_rpc_id_t *id)
{
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    switch (id->type) {
        case JSON_RPC_ID_NULL:
        case JSON_RPC_ID_NUMBER:
            return JSON_RPC_SUCCESS;
            
        case JSON_RPC_ID_STRING:
            // String IDs must not be NULL (use JSON_RPC_ID_NULL for null)
            return id->value.string ? JSON_RPC_SUCCESS : JSON_RPC_ERROR_INVALID_PARAMETER;
            
        default:
            return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
}

// ============================================================================
// PARAMETER ACCESS HELPERS
// ============================================================================

json_rpc_result_t json_rpc_get_param_by_index(const json_rpc_params_t *params,
                                              int index, json_value **value)
{
    if (!params || !value || index < 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *value = NULL;
    
    if (params->type != JSON_RPC_PARAMS_ARRAY) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (!params->data || params->data->type != json_array) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (index >= (int)params->data->u.array.length) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *value = params->data->u.array.values[index];
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_get_param_by_name(const json_rpc_params_t *params,
                                             const char *name, json_value **value)
{
    if (!params || !name || !value) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *value = NULL;
    
    if (params->type != JSON_RPC_PARAMS_OBJECT) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (!params->data || params->data->type != json_object) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Search for the named parameter
    for (unsigned int i = 0; i < params->data->u.object.length; i++) {
        json_object_entry *entry = &params->data->u.object.values[i];
        if (strcmp(entry->name, name) == 0) {
            *value = entry->value;
            return JSON_RPC_SUCCESS;
        }
    }
    
    return JSON_RPC_ERROR_MISSING_FIELD;
}

int json_rpc_get_param_count(const json_rpc_params_t *params)
{
    if (!params || !params->data) return 0;
    
    switch (params->type) {
        case JSON_RPC_PARAMS_NONE:
            return 0;
            
        case JSON_RPC_PARAMS_ARRAY:
            if (params->data->type == json_array) {
                return (int)params->data->u.array.length;
            }
            break;
            
        case JSON_RPC_PARAMS_OBJECT:
            if (params->data->type == json_object) {
                return (int)params->data->u.object.length;
            }
            break;
    }
    
    return -1; // Error
}

bool json_rpc_params_valid(const json_rpc_params_t *params)
{
    if (!params) return false;
    
    switch (params->type) {
        case JSON_RPC_PARAMS_NONE:
            return params->data == NULL;
            
        case JSON_RPC_PARAMS_ARRAY:
            return params->data && params->data->type == json_array;
            
        case JSON_RPC_PARAMS_OBJECT:
            return params->data && params->data->type == json_object;
            
        default:
            return false;
    }
}

// ============================================================================
// JSON VALUE CLONING HELPERS
// ============================================================================

static json_value *clone_json_value(const json_value *src)
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
            json_value *arr = json_array_new(src->u.array.length);
            for (unsigned int i = 0; i < src->u.array.length; i++) {
                json_value *cloned_item = clone_json_value(src->u.array.values[i]);
                if (cloned_item) {
                    json_array_push(arr, cloned_item);
                }
            }
            return arr;
        }
        
        case json_object: {
            json_value *obj = json_object_new(src->u.object.length);
            for (unsigned int i = 0; i < src->u.object.length; i++) {
                json_object_entry *entry = &src->u.object.values[i];
                json_value *cloned_value = clone_json_value(entry->value);
                if (cloned_value) {
                    json_object_push(obj, entry->name, cloned_value);
                }
            }
            return obj;
        }
        
        default:
            return NULL;
    }
}

// ============================================================================
// JSON PARSING HELPERS
// ============================================================================

static json_rpc_result_t parse_id_from_json(json_value *json_id, json_rpc_id_t *id)
{
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    if (!json_id || json_id->type == json_null) {
        return json_rpc_create_id_null(id);
    }
    
    if (json_id->type == json_integer) {
        return json_rpc_create_id_number(json_id->u.integer, id);
    }
    
    if (json_id->type == json_string) {
        return json_rpc_create_id_string(json_id->u.string.ptr, id);
    }
    
    return JSON_RPC_ERROR_INVALID_JSON;
}

static json_value *create_id_json(const json_rpc_id_t *id)
{
    if (!id) return NULL;
    
    switch (id->type) {
        case JSON_RPC_ID_NULL:
            return json_null_new();
            
        case JSON_RPC_ID_NUMBER:
            return json_integer_new(id->value.number);
            
        case JSON_RPC_ID_STRING:
            return json_string_new(id->value.string);
            
        default:
            return NULL;
    }
}

static json_rpc_result_t parse_params_from_json(json_value *json_params, 
                                               json_rpc_params_t *params)
{
    if (!params) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    // Initialize params
    params->type = JSON_RPC_PARAMS_NONE;
    params->data = NULL;
    
    if (!json_params || json_params->type == json_null) {
        return JSON_RPC_SUCCESS; // No parameters is valid
    }
    
    if (json_params->type == json_array) {
        params->type = JSON_RPC_PARAMS_ARRAY;
        // Clone the params to avoid use-after-free when root is freed
        params->data = clone_json_value(json_params);
        if (!params->data) {
            return JSON_RPC_ERROR_OUT_OF_MEMORY;
        }
        return JSON_RPC_SUCCESS;
    }
    
    if (json_params->type == json_object) {
        params->type = JSON_RPC_PARAMS_OBJECT;
        // Clone the params to avoid use-after-free when root is freed
        params->data = clone_json_value(json_params);
        if (!params->data) {
            return JSON_RPC_ERROR_OUT_OF_MEMORY;
        }
        return JSON_RPC_SUCCESS;
    }
    
    return JSON_RPC_ERROR_INVALID_JSON;
}

static json_value *create_params_json(const json_rpc_params_t *params)
{
    if (!params || params->type == JSON_RPC_PARAMS_NONE) {
        return NULL; // Omit params field for no parameters
    }
    
    // Clone the params data to avoid double-free when serialized JSON is freed
    return clone_json_value(params->data);
}

// ============================================================================
// REQUEST PARSING IMPLEMENTATION
// ============================================================================

json_rpc_result_t json_rpc_parse_request(const char *json_str,
                                        json_rpc_request_t **request)
{
    if (!json_str || !request) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *request = NULL;
    
    // Parse JSON
    json_value *root = json_parse(json_str, strlen(json_str));
    if (!root) {
        return JSON_RPC_ERROR_PARSE_FAILED;
    }
    
    if (root->type != json_object) {
        json_value_free(root);
        return JSON_RPC_ERROR_INVALID_JSON;
    }
    
    // Allocate request structure
    json_rpc_request_t *req = calloc(1, sizeof(json_rpc_request_t));
    if (!req) {
        json_value_free(root);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    json_rpc_result_t result = JSON_RPC_SUCCESS;
    json_value *json_version = NULL;
    json_value *json_method = NULL;
    json_value *json_params = NULL;
    json_value *json_id = NULL;
    
    // Find required and optional fields
    for (unsigned int i = 0; i < root->u.object.length; i++) {
        json_object_entry *entry = &root->u.object.values[i];
        
        if (strcmp(entry->name, "jsonrpc") == 0) {
            json_version = entry->value;
        } else if (strcmp(entry->name, "method") == 0) {
            json_method = entry->value;
        } else if (strcmp(entry->name, "params") == 0) {
            json_params = entry->value;
        } else if (strcmp(entry->name, "id") == 0) {
            json_id = entry->value;
        }
    }
    
    // Validate JSON-RPC version
    if (!json_version || json_version->type != json_string ||
        strcmp(json_version->u.string.ptr, JSON_RPC_VERSION) != 0) {
        result = JSON_RPC_ERROR_INVALID_VERSION;
        goto cleanup;
    }
    
    // Validate method
    if (!json_method || json_method->type != json_string) {
        result = JSON_RPC_ERROR_MISSING_FIELD;
        goto cleanup;
    }
    
    req->method = strdup(json_method->u.string.ptr);
    if (!req->method) {
        result = JSON_RPC_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    // Parse parameters
    result = parse_params_from_json(json_params, &req->params);
    if (result != JSON_RPC_SUCCESS) {
        goto cleanup;
    }
    
    // Parse ID (determines if this is a notification)
    if (json_id) {
        result = parse_id_from_json(json_id, &req->id);
        if (result != JSON_RPC_SUCCESS) {
            goto cleanup;
        }
        req->is_notification = false;
    } else {
        json_rpc_create_id_null(&req->id);
        req->is_notification = true;
    }
    
    *request = req;
    json_value_free(root); // Now safe to free root since we cloned params in parse_params_from_json
    return JSON_RPC_SUCCESS;
    
cleanup:
    if (result != JSON_RPC_SUCCESS) {
        json_rpc_free_request(req);
    }
    json_value_free(root);
    return result;
}

// ============================================================================
// RESPONSE PARSING IMPLEMENTATION
// ============================================================================

json_rpc_result_t json_rpc_parse_response(const char *json_str,
                                         json_rpc_response_t **response)
{
    if (!json_str || !response) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *response = NULL;
    
    // Parse JSON
    json_value *root = json_parse(json_str, strlen(json_str));
    if (!root) {
        return JSON_RPC_ERROR_PARSE_FAILED;
    }
    
    if (root->type != json_object) {
        json_value_free(root);
        return JSON_RPC_ERROR_INVALID_JSON;
    }
    
    // Allocate response structure
    json_rpc_response_t *resp = calloc(1, sizeof(json_rpc_response_t));
    if (!resp) {
        json_value_free(root);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    json_rpc_result_t result = JSON_RPC_SUCCESS;
    json_value *json_version = NULL;
    json_value *json_result = NULL;
    json_value *json_error = NULL;
    json_value *json_id = NULL;
    
    // Find fields
    for (unsigned int i = 0; i < root->u.object.length; i++) {
        json_object_entry *entry = &root->u.object.values[i];
        
        if (strcmp(entry->name, "jsonrpc") == 0) {
            json_version = entry->value;
        } else if (strcmp(entry->name, "result") == 0) {
            json_result = entry->value;
        } else if (strcmp(entry->name, "error") == 0) {
            json_error = entry->value;
        } else if (strcmp(entry->name, "id") == 0) {
            json_id = entry->value;
        }
    }
    
    // Validate JSON-RPC version
    if (!json_version || json_version->type != json_string ||
        strcmp(json_version->u.string.ptr, JSON_RPC_VERSION) != 0) {
        result = JSON_RPC_ERROR_INVALID_VERSION;
        goto cleanup;
    }
    
    // Parse ID
    result = parse_id_from_json(json_id, &resp->id);
    if (result != JSON_RPC_SUCCESS) {
        goto cleanup;
    }
    
    // Must have either result or error, but not both
    if (json_result && json_error) {
        result = JSON_RPC_ERROR_INVALID_JSON;
        goto cleanup;
    }
    
    if (!json_result && !json_error) {
        result = JSON_RPC_ERROR_MISSING_FIELD;
        goto cleanup;
    }
    
    if (json_result) {
        resp->has_result = true;
        resp->has_error = false;
        resp->result = clone_json_value(json_result);
        if (!resp->result) {
            result = JSON_RPC_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
    } else {
        resp->has_result = false;
        resp->has_error = true;
        
        // Parse error object
        if (json_error->type != json_object) {
            result = JSON_RPC_ERROR_INVALID_JSON;
            goto cleanup;
        }
        
        json_value *error_code = NULL;
        json_value *error_message = NULL;
        json_value *error_data = NULL;
        
        for (unsigned int i = 0; i < json_error->u.object.length; i++) {
            json_object_entry *entry = &json_error->u.object.values[i];
            
            if (strcmp(entry->name, "code") == 0) {
                error_code = entry->value;
            } else if (strcmp(entry->name, "message") == 0) {
                error_message = entry->value;
            } else if (strcmp(entry->name, "data") == 0) {
                error_data = entry->value;
            }
        }
        
        if (!error_code || error_code->type != json_integer) {
            result = JSON_RPC_ERROR_MISSING_FIELD;
            goto cleanup;
        }
        
        if (!error_message || error_message->type != json_string) {
            result = JSON_RPC_ERROR_MISSING_FIELD;
            goto cleanup;
        }
        
        resp->error.code = (int32_t)error_code->u.integer;
        resp->error.message = strdup(error_message->u.string.ptr);
        if (!resp->error.message) {
            result = JSON_RPC_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
        
        if (error_data) {
            resp->error.data = clone_json_value(error_data);
        } else {
            resp->error.data = NULL;
        }
    }
    
    *response = resp;
    json_value_free(root); // Now safe to free root since we cloned the needed parts
    return JSON_RPC_SUCCESS;
    
cleanup:
    if (result != JSON_RPC_SUCCESS) {
        json_rpc_free_response(resp);
    }
    json_value_free(root);
    return result;
}

// ============================================================================
// SERIALIZATION IMPLEMENTATION
// ============================================================================

json_rpc_result_t json_rpc_serialize_request(const json_rpc_request_t *request,
                                            char **json_str)
{
    if (!request || !json_str) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *json_str = NULL;
    
    // Validate request
    json_rpc_result_t result = json_rpc_validate_request(request);
    if (result != JSON_RPC_SUCCESS) {
        return result;
    }
    
    // Create JSON object
    json_value *root = json_object_new(4); // Allocate for jsonrpc, method, params, id
    if (!root) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Add jsonrpc version
    json_object_push(root, "jsonrpc", json_string_new(JSON_RPC_VERSION));
    
    // Add method
    json_object_push(root, "method", json_string_new(request->method));
    
    // Add params if present
    json_value *params_json = create_params_json(&request->params);
    if (params_json) {
        json_object_push(root, "params", params_json);
    }
    
    // Add id if not a notification
    if (!request->is_notification) {
        json_value *id_json = create_id_json(&request->id);
        if (id_json) {
            json_object_push(root, "id", id_json);
        }
    }
    
    // Serialize to string
    size_t length = json_measure(root);
    char *buffer = malloc(length + 1);
    if (!buffer) {
        json_value_free(root);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Zero out the buffer to be safe
    memset(buffer, 0, length + 1);
    json_serialize(buffer, root);
    json_value_free(root);
    
    *json_str = buffer;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_serialize_response(const json_rpc_response_t *response,
                                             char **json_str)
{
    if (!response || !json_str) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *json_str = NULL;
    
    // Validate response
    json_rpc_result_t result = json_rpc_validate_response(response);
    if (result != JSON_RPC_SUCCESS) {
        return result;
    }
    
    // Create JSON object
    json_value *root = json_object_new(3); // Allocate for jsonrpc, result/error, id
    if (!root) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Add jsonrpc version
    json_object_push(root, "jsonrpc", json_string_new(JSON_RPC_VERSION));
    
    // Add result or error
    if (response->has_result) {
        if (response->result) {
            // Clone result to avoid double-free when serialized JSON is freed
            json_value *result_clone = clone_json_value(response->result);
            json_object_push(root, "result", result_clone);
        } else {
            json_object_push(root, "result", json_null_new());
        }
    } else {
        // Create error object
        json_value *error_obj = json_object_new(3);
        json_object_push(error_obj, "code", json_integer_new(response->error.code));
        json_object_push(error_obj, "message", 
                        json_string_new(response->error.message ? response->error.message : ""));
        
        if (response->error.data) {
            // Clone error data to avoid double-free when serialized JSON is freed
            json_value *error_data_clone = clone_json_value(response->error.data);
            json_object_push(error_obj, "data", error_data_clone);
        }
        
        json_object_push(root, "error", error_obj);
    }
    
    // Add id
    json_value *id_json = create_id_json(&response->id);
    if (id_json) {
        json_object_push(root, "id", id_json);
    }
    
    // Serialize to string
    size_t length = json_measure(root);
    char *buffer = malloc(length + 1);
    if (!buffer) {
        json_value_free(root);
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    // Zero out the buffer to be safe
    memset(buffer, 0, length + 1);
    json_serialize(buffer, root);
    json_value_free(root);
    
    *json_str = buffer;
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// REQUEST/RESPONSE CREATION IMPLEMENTATION
// ============================================================================

json_rpc_request_t *json_rpc_create_request_array(const char *method,
                                                 json_value *params_array,
                                                 const json_rpc_id_t *id)
{
    if (!method || !id) return NULL;
    
    json_rpc_request_t *request = calloc(1, sizeof(json_rpc_request_t));
    if (!request) return NULL;
    
    request->method = strdup(method);
    if (!request->method) {
        free(request);
        return NULL;
    }
    
    if (params_array) {
        if (params_array->type != json_array) {
            json_rpc_free_request(request);
            return NULL;
        }
        request->params.type = JSON_RPC_PARAMS_ARRAY;
        request->params.data = params_array;
    } else {
        request->params.type = JSON_RPC_PARAMS_NONE;
        request->params.data = NULL;
    }
    
    if (json_rpc_clone_id(id, &request->id) != JSON_RPC_SUCCESS) {
        json_rpc_free_request(request);
        return NULL;
    }
    
    request->is_notification = (id->type == JSON_RPC_ID_NULL);
    
    return request;
}

json_rpc_request_t *json_rpc_create_request_object(const char *method,
                                                  json_value *params_object,
                                                  const json_rpc_id_t *id)
{
    if (!method || !id) return NULL;
    
    json_rpc_request_t *request = calloc(1, sizeof(json_rpc_request_t));
    if (!request) return NULL;
    
    request->method = strdup(method);
    if (!request->method) {
        free(request);
        return NULL;
    }
    
    if (params_object) {
        if (params_object->type != json_object) {
            json_rpc_free_request(request);
            return NULL;
        }
        request->params.type = JSON_RPC_PARAMS_OBJECT;
        request->params.data = params_object;
    } else {
        request->params.type = JSON_RPC_PARAMS_NONE;
        request->params.data = NULL;
    }
    
    if (json_rpc_clone_id(id, &request->id) != JSON_RPC_SUCCESS) {
        json_rpc_free_request(request);
        return NULL;
    }
    
    request->is_notification = (id->type == JSON_RPC_ID_NULL);
    
    return request;
}

json_rpc_request_t *json_rpc_create_notification(const char *method,
                                                json_value *params,
                                                bool is_array)
{
    if (!method) return NULL;
    
    json_rpc_request_t *request = calloc(1, sizeof(json_rpc_request_t));
    if (!request) return NULL;
    
    request->method = strdup(method);
    if (!request->method) {
        free(request);
        return NULL;
    }
    
    if (params) {
        if (is_array) {
            if (params->type != json_array) {
                json_rpc_free_request(request);
                return NULL;
            }
            request->params.type = JSON_RPC_PARAMS_ARRAY;
        } else {
            if (params->type != json_object) {
                json_rpc_free_request(request);
                return NULL;
            }
            request->params.type = JSON_RPC_PARAMS_OBJECT;
        }
        request->params.data = params;
    } else {
        request->params.type = JSON_RPC_PARAMS_NONE;
        request->params.data = NULL;
    }
    
    json_rpc_create_id_null(&request->id);
    request->is_notification = true;
    
    return request;
}

json_rpc_response_t *json_rpc_create_response_result(json_value *result,
                                                    const json_rpc_id_t *id)
{
    if (!id) return NULL;
    
    json_rpc_response_t *response = calloc(1, sizeof(json_rpc_response_t));
    if (!response) return NULL;
    
    if (json_rpc_clone_id(id, &response->id) != JSON_RPC_SUCCESS) {
        free(response);
        return NULL;
    }
    
    response->has_result = true;
    response->has_error = false;
    response->result = result;
    
    return response;
}

json_rpc_response_t *json_rpc_create_response_error(int32_t error_code,
                                                   const char *error_message,
                                                   json_value *error_data,
                                                   const json_rpc_id_t *id)
{
    if (!error_message || !id) return NULL;
    
    json_rpc_response_t *response = calloc(1, sizeof(json_rpc_response_t));
    if (!response) return NULL;
    
    if (json_rpc_clone_id(id, &response->id) != JSON_RPC_SUCCESS) {
        free(response);
        return NULL;
    }
    
    response->has_result = false;
    response->has_error = true;
    response->error.code = error_code;
    response->error.message = strdup(error_message);
    response->error.data = error_data;
    
    if (!response->error.message) {
        json_rpc_free_response(response);
        return NULL;
    }
    
    return response;
}

// ============================================================================
// MEMORY MANAGEMENT IMPLEMENTATION
// ============================================================================

void json_rpc_free_request(json_rpc_request_t *request)
{
    if (!request) return;
    
    LOG_D("Freeing request: method=%s, params.type=%d, params.data=%p", 
          request->method ? request->method : "(null)", 
          request->params.type, request->params.data);
    
    SAFE_FREE(request->method);
    
    // Free parameters data if owned
    if (request->params.data) {
        LOG_D("About to free params.data at %p", request->params.data);
        json_value_free(request->params.data);
        LOG_D("Freed params.data");
    }
    
    json_rpc_free_id(&request->id);
    
    LOG_D("About to free request structure at %p", request);
    free(request);
    LOG_D("Request freed successfully");
}

void json_rpc_free_response(json_rpc_response_t *response)
{
    if (!response) return;
    
    json_rpc_free_id(&response->id);
    
    if (response->has_result && response->result) {
        json_value_free(response->result);
    }
    
    if (response->has_error) {
        SAFE_FREE(response->error.message);
        if (response->error.data) {
            json_value_free(response->error.data);
        }
    }
    
    free(response);
}

// ============================================================================
// VALIDATION IMPLEMENTATION
// ============================================================================

json_rpc_result_t json_rpc_validate_request(const json_rpc_request_t *request)
{
    if (!request) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    // Validate method name
    if (!request->method || strlen(request->method) == 0) {
        return JSON_RPC_ERROR_MISSING_FIELD;
    }
    
    if (strlen(request->method) >= JSON_RPC_MAX_METHOD_NAME) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Method names starting with "rpc." are reserved
    if (strncmp(request->method, "rpc.", 4) == 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Validate ID
    json_rpc_result_t id_result = json_rpc_validate_id(&request->id);
    if (id_result != JSON_RPC_SUCCESS) {
        return id_result;
    }
    
    // Validate notification consistency
    if (request->is_notification && request->id.type != JSON_RPC_ID_NULL) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (!request->is_notification && request->id.type == JSON_RPC_ID_NULL) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Validate parameters
    if (!json_rpc_params_valid(&request->params)) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t json_rpc_validate_response(const json_rpc_response_t *response)
{
    if (!response) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    // Validate ID
    json_rpc_result_t id_result = json_rpc_validate_id(&response->id);
    if (id_result != JSON_RPC_SUCCESS) {
        return id_result;
    }
    
    // Must have either result or error, but not both
    if (response->has_result == response->has_error) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Validate error structure if present
    if (response->has_error) {
        if (!response->error.message) {
            return JSON_RPC_ERROR_MISSING_FIELD;
        }
        
        if (strlen(response->error.message) >= JSON_RPC_MAX_ERROR_MESSAGE) {
            return JSON_RPC_ERROR_INVALID_PARAMETER;
        }
    }
    
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// GOXEL API METHOD IMPLEMENTATIONS
// ============================================================================

// Global context for Goxel core operations
static goxel_core_context_t *g_goxel_context = NULL;

// Global render manager for file transfer architecture
static render_manager_t *g_render_manager = NULL;

/**
 * Method handler function type
 */
typedef json_rpc_response_t *(*method_handler_t)(const json_rpc_request_t *request);

/**
 * Method registry entry
 */
typedef struct {
    const char *name;
    method_handler_t handler;
    const char *description;
} method_registry_entry_t;

// Include test methods
#include "test_methods.h"

// Forward declarations for method handlers
static json_rpc_response_t *handle_goxel_create_project(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_load_project(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_save_project(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_add_voxel(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_remove_voxel(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_get_voxel(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_export_model(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_get_status(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_list_layers(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_create_layer(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_paint_voxels(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_flood_fill(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_procedural_shape(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_delete_layer(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_merge_layers(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_set_layer_visibility(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_render_scene(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_batch_operations(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_execute_script(const json_rpc_request_t *request);

// Render management handlers
static json_rpc_response_t *handle_goxel_get_render_info(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_cleanup_render(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_list_renders(const json_rpc_request_t *request);

// Bulk voxel reading handlers
static json_rpc_response_t *handle_goxel_get_voxels_region(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_get_layer_voxels(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_get_bounding_box(const json_rpc_request_t *request);

// Color analysis handlers
static json_rpc_response_t *handle_goxel_get_color_histogram(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_find_voxels_by_color(const json_rpc_request_t *request);
static json_rpc_response_t *handle_goxel_get_unique_colors(const json_rpc_request_t *request);

// Method registry
static const method_registry_entry_t g_method_registry[] = {
    // Goxel API methods - File operations
    {"goxel.create_project", handle_goxel_create_project, "Create a new voxel project"},
    {"goxel.load_project", handle_goxel_load_project, "Load a project from file"},
    {"goxel.save_project", handle_goxel_save_project, "Save project to file"},
    {"goxel.export_model", handle_goxel_export_model, "Export model to specified format"},
    {"goxel.render_scene", handle_goxel_render_scene, "Render scene to image"},
    
    // Voxel operations
    {"goxel.add_voxel", handle_goxel_add_voxel, "Add a voxel at specified position"},
    {"goxel.remove_voxel", handle_goxel_remove_voxel, "Remove a voxel at specified position"},
    {"goxel.get_voxel", handle_goxel_get_voxel, "Get voxel information at specified position"},
    {"goxel.paint_voxels", handle_goxel_paint_voxels, "Paint existing voxels with new color"},
    {"goxel.flood_fill", handle_goxel_flood_fill, "Fill connected voxels of same color"},
    {"goxel.procedural_shape", handle_goxel_procedural_shape, "Generate procedural shapes"},
    {"goxel.batch_operations", handle_goxel_batch_operations, "Perform multiple voxel operations efficiently"},
    
    // Layer management
    {"goxel.list_layers", handle_goxel_list_layers, "List all layers in current project"},
    {"goxel.create_layer", handle_goxel_create_layer, "Create a new layer"},
    {"goxel.delete_layer", handle_goxel_delete_layer, "Delete specified layer"},
    {"goxel.merge_layers", handle_goxel_merge_layers, "Merge two or more layers"},
    {"goxel.set_layer_visibility", handle_goxel_set_layer_visibility, "Show or hide layer"},
    
    // System operations
    {"goxel.get_status", handle_goxel_get_status, "Get current Goxel status and info"},
    
    // Script operations
    {"goxel.execute_script", handle_goxel_execute_script, "Execute JavaScript code asynchronously"},
    
    // Bulk voxel reading operations
    {"goxel.get_voxels_region", handle_goxel_get_voxels_region, "Get all voxels in a box region"},
    {"goxel.get_layer_voxels", handle_goxel_get_layer_voxels, "Get all voxels in a layer"},
    {"goxel.get_bounding_box", handle_goxel_get_bounding_box, "Get bounding box of voxels"},
    
    // Color analysis operations
    {"goxel.get_color_histogram", handle_goxel_get_color_histogram, "Generate color distribution analysis"},
    {"goxel.find_voxels_by_color", handle_goxel_find_voxels_by_color, "Find all voxels matching a color"},
    {"goxel.get_unique_colors", handle_goxel_get_unique_colors, "List all unique colors used"},
    
    // Render management operations (Phase 2: v0.16 Render Transfer Architecture)
    {"goxel.get_render_info", handle_goxel_get_render_info, "Get information about a specific render file"},
    {"goxel.cleanup_render", handle_goxel_cleanup_render, "Manually cleanup a specific render file"},
    {"goxel.list_renders", handle_goxel_list_renders, "List all active renders with their metadata"}
};

static const size_t g_method_registry_size = sizeof(g_method_registry) / sizeof(g_method_registry[0]);

// ============================================================================
// UTILITY FUNCTIONS FOR METHOD HANDLERS
// ============================================================================

// Forward declaration for JSON helper
static json_value *json_object_get_helper(const json_value *obj, const char *key);

static int get_int_param(const json_rpc_params_t *params, int index, const char *name)
{
    json_value *value = NULL;
    json_rpc_result_t result;
    
    if (params->type == JSON_RPC_PARAMS_ARRAY) {
        result = json_rpc_get_param_by_index(params, index, &value);
    } else if (params->type == JSON_RPC_PARAMS_OBJECT) {
        result = json_rpc_get_param_by_name(params, name, &value);
    } else {
        return 0;
    }
    
    if (result != JSON_RPC_SUCCESS || !value || value->type != json_integer) {
        return 0;
    }
    
    return (int)value->u.integer;
}

static const char *get_string_param(const json_rpc_params_t *params, int index, const char *name)
{
    json_value *value = NULL;
    json_rpc_result_t result;
    
    if (params->type == JSON_RPC_PARAMS_ARRAY) {
        result = json_rpc_get_param_by_index(params, index, &value);
    } else if (params->type == JSON_RPC_PARAMS_OBJECT) {
        result = json_rpc_get_param_by_name(params, name, &value);
    } else {
        return NULL;
    }
    
    if (result != JSON_RPC_SUCCESS || !value || value->type != json_string) {
        return NULL;
    }
    
    return value->u.string.ptr;
}

static bool get_bool_param(const json_rpc_params_t *params, int index, const char *name, bool default_value)
{
    json_value *value = NULL;
    json_rpc_result_t result;
    
    if (params->type == JSON_RPC_PARAMS_ARRAY) {
        result = json_rpc_get_param_by_index(params, index, &value);
    } else if (params->type == JSON_RPC_PARAMS_OBJECT) {
        result = json_rpc_get_param_by_name(params, name, &value);
    } else {
        return default_value;
    }
    
    if (result != JSON_RPC_SUCCESS || !value || value->type != json_boolean) {
        return default_value;
    }
    
    return value->u.boolean;
}

// Enhanced color parsing function that supports multiple formats
// Supports: integers [r,g,b,a], floats [0.0-1.0], hex strings "#RRGGBBAA", arrays, objects
static json_rpc_result_t parse_color_param(const json_rpc_params_t *params, int start_index, 
                                           const char *color_name, uint8_t rgba[4], 
                                           char *error_msg, size_t error_msg_size)
{
    if (!rgba) return JSON_RPC_ERROR_INVALID_PARAMETER;
    
    // Default to opaque white
    rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;
    
    json_value *value = NULL;
    json_rpc_result_t result;
    
    // First, try to get color as a named parameter (object-style)
    if (params->type == JSON_RPC_PARAMS_OBJECT && color_name) {
        result = json_rpc_get_param_by_name(params, color_name, &value);
        if (result == JSON_RPC_SUCCESS && value) {
            // Handle different color formats in object mode
            
            if (value->type == json_array && value->u.array.length >= 3) {
                // Color as array: [r, g, b, a?]
                for (int i = 0; i < 4 && i < (int)value->u.array.length; i++) {
                    json_value *component = value->u.array.values[i];
                    if (component->type == json_integer) {
                        int val = (int)component->u.integer;
                        rgba[i] = (uint8_t)(val < 0 ? 0 : val > 255 ? 255 : val);
                    } else if (component->type == json_double) {
                        // Assume float format (0.0-1.0)
                        double val = component->u.dbl;
                        rgba[i] = (uint8_t)(val * 255.0);
                    }
                }
                if (value->u.array.length == 3) rgba[3] = 255; // Default alpha
                return JSON_RPC_SUCCESS;
            }
            
            else if (value->type == json_string) {
                // Hex color string: "#RRGGBB" or "#RRGGBBAA"
                const char *hex = value->u.string.ptr;
                if (hex && hex[0] == '#') {
                    int len = strlen(hex);
                    if (len == 7 || len == 9) { // #RRGGBB or #RRGGBBAA
                        unsigned int r, g, b, a = 255;
                        if (len == 7) {
                            if (sscanf(hex, "#%02x%02x%02x", &r, &g, &b) == 3) {
                                rgba[0] = (uint8_t)r; rgba[1] = (uint8_t)g; 
                                rgba[2] = (uint8_t)b; rgba[3] = (uint8_t)a;
                                return JSON_RPC_SUCCESS;
                            }
                        } else {
                            if (sscanf(hex, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
                                rgba[0] = (uint8_t)r; rgba[1] = (uint8_t)g;
                                rgba[2] = (uint8_t)b; rgba[3] = (uint8_t)a;
                                return JSON_RPC_SUCCESS;
                            }
                        }
                    }
                }
                if (error_msg) snprintf(error_msg, error_msg_size, "Invalid hex color format");
                return JSON_RPC_ERROR_INVALID_PARAMETER;
            }
            
            else if (value->type == json_object) {
                // Color as object: {"r": 255, "g": 0, "b": 0, "a": 255}
                json_value *r_val = json_object_get_helper(value, "r");
                json_value *g_val = json_object_get_helper(value, "g");
                json_value *b_val = json_object_get_helper(value, "b");
                json_value *a_val = json_object_get_helper(value, "a");
                
                if (r_val && g_val && b_val) {
                    rgba[0] = (r_val->type == json_integer) ? (uint8_t)r_val->u.integer : 
                              (r_val->type == json_double) ? (uint8_t)(r_val->u.dbl * 255.0) : 0;
                    rgba[1] = (g_val->type == json_integer) ? (uint8_t)g_val->u.integer :
                              (g_val->type == json_double) ? (uint8_t)(g_val->u.dbl * 255.0) : 0;
                    rgba[2] = (b_val->type == json_integer) ? (uint8_t)b_val->u.integer :
                              (b_val->type == json_double) ? (uint8_t)(b_val->u.dbl * 255.0) : 0;
                    rgba[3] = a_val ? 
                              ((a_val->type == json_integer) ? (uint8_t)a_val->u.integer :
                               (a_val->type == json_double) ? (uint8_t)(a_val->u.dbl * 255.0) : 255) : 255;
                    return JSON_RPC_SUCCESS;
                }
                if (error_msg) snprintf(error_msg, error_msg_size, "Color object missing r, g, b fields");
                return JSON_RPC_ERROR_INVALID_PARAMETER;
            }
        }
    }
    
    // Fall back to positional parameters (array-style): [x, y, z, r, g, b, a]
    if (params->type == JSON_RPC_PARAMS_ARRAY) {
        bool any_found = false;
        
        for (int i = 0; i < 4; i++) {
            json_value *component = NULL;
            result = json_rpc_get_param_by_index(params, start_index + i, &component);
            
            if (result == JSON_RPC_SUCCESS && component) {
                any_found = true;
                
                if (component->type == json_integer) {
                    int val = (int)component->u.integer;
                    rgba[i] = (uint8_t)(val < 0 ? 0 : val > 255 ? 255 : val);
                } else if (component->type == json_double) {
                    // Handle float values (assume 0.0-1.0 range)
                    double val = component->u.dbl;
                    if (val >= 0.0 && val <= 1.0) {
                        rgba[i] = (uint8_t)(val * 255.0);
                    } else {
                        // Assume integer range passed as float
                        rgba[i] = (uint8_t)(val < 0 ? 0 : val > 255 ? 255 : (int)val);
                    }
                } else if (component->type == json_string && i == 0) {
                    // Allow hex string as first color parameter
                    const char *hex = component->u.string.ptr;
                    if (hex && hex[0] == '#') {
                        int len = strlen(hex);
                        if (len == 7 || len == 9) {
                            unsigned int r, g, b, a = 255;
                            if (len == 7) {
                                if (sscanf(hex, "#%02x%02x%02x", &r, &g, &b) == 3) {
                                    rgba[0] = (uint8_t)r; rgba[1] = (uint8_t)g;
                                    rgba[2] = (uint8_t)b; rgba[3] = (uint8_t)a;
                                    return JSON_RPC_SUCCESS;
                                }
                            } else {
                                if (sscanf(hex, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
                                    rgba[0] = (uint8_t)r; rgba[1] = (uint8_t)g;
                                    rgba[2] = (uint8_t)b; rgba[3] = (uint8_t)a;
                                    return JSON_RPC_SUCCESS;
                                }
                            }
                        }
                    }
                    if (error_msg) snprintf(error_msg, error_msg_size, "Invalid hex color in array");
                    return JSON_RPC_ERROR_INVALID_PARAMETER;
                } else {
                    // Unknown type, use default
                    rgba[i] = (i == 3) ? 255 : 0; // Default alpha to 255, RGB to 0
                }
            } else {
                // Parameter not found, use default
                rgba[i] = (i == 3) ? 255 : 0; // Default alpha to 255, RGB to 0
            }
        }
        
        if (any_found) {
            return JSON_RPC_SUCCESS;
        }
    }
    
    // No color parameters found, use defaults
    if (error_msg) snprintf(error_msg, error_msg_size, "No color parameters found");
    return JSON_RPC_SUCCESS; // Still success with defaults
}

// Helper functions removed - not used in current implementation

// ============================================================================
// RENDER TRANSFER HELPER FUNCTIONS (Phase 2: v0.16)
// ============================================================================

/**
 * Helper function to read PNG image dimensions from file header.
 * Returns 0 on success, -1 on failure.
 */
static int get_png_dimensions(const char *file_path, int *width, int *height)
{
    if (!file_path || !width || !height) {
        return -1;
    }
    
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        return -1;
    }
    
    // PNG files start with 8 bytes signature, then IHDR chunk
    // IHDR chunk structure: length(4) + type(4) + width(4) + height(4) + ...
    unsigned char buffer[24];
    
    if (fread(buffer, 1, 24, fp) != 24) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    
    // Check PNG signature
    if (buffer[0] != 0x89 || buffer[1] != 'P' || buffer[2] != 'N' || buffer[3] != 'G' ||
        buffer[4] != 0x0D || buffer[5] != 0x0A || buffer[6] != 0x1A || buffer[7] != 0x0A) {
        return -1;
    }
    
    // Check IHDR chunk type
    if (buffer[12] != 'I' || buffer[13] != 'H' || buffer[14] != 'D' || buffer[15] != 'R') {
        return -1;
    }
    
    // Extract dimensions (big-endian format)
    *width = (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19];
    *height = (buffer[20] << 24) | (buffer[21] << 16) | (buffer[22] << 8) | buffer[23];
    
    return 0;
}

/**
 * Helper function to get file size.
 */
static long get_file_size(const char *file_path)
{
    if (!file_path) return -1;
    
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    
    return size;
}

// ============================================================================
// METHOD HANDLER IMPLEMENTATIONS
// ============================================================================

// Test method implementations are now in test_methods.c

static json_rpc_response_t *handle_goxel_create_project(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Acquire exclusive project lock
    char request_id[32];
    if (request->id.type == JSON_RPC_ID_NUMBER) {
        snprintf(request_id, sizeof(request_id), "req_%lld", 
                 (long long)request->id.value.number);
    } else if (request->id.type == JSON_RPC_ID_STRING) {
        snprintf(request_id, sizeof(request_id), "req_%s", 
                 request->id.value.string);
    } else {
        snprintf(request_id, sizeof(request_id), "req_null");
    }
    
    if (project_lock_acquire(request_id) != 0) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
            "Another project operation is in progress", 
            NULL, &request->id);
    }
    
    // QUICK FIX: Complete cleanup before creating new project
    extern goxel_t goxel;
    
    // Step 1: Clear ALL global state
    if (goxel.image) {
        image_delete(goxel.image);
        goxel.image = NULL;
    }
    
    // Step 2: Reset context
    if (g_goxel_context->image) {
        // Don't delete if it's the same as global (already deleted)
        if (g_goxel_context->image != goxel.image) {
            image_delete(g_goxel_context->image);
        }
        g_goxel_context->image = NULL;
    }
    
    // Step 3: Clear any cached materials, cameras, etc.
    goxel.image = NULL;
    goxel.tool = NULL;
    goxel.tool_volume = NULL;
    if (goxel.layers_volume_) {
        volume_delete(goxel.layers_volume_);
        goxel.layers_volume_ = NULL;
    }
    if (goxel.render_volume_) {
        volume_delete(goxel.render_volume_);
        goxel.render_volume_ = NULL;
    }
    
    // Continue with original logic...
    const char *name = get_string_param(&request->params, 0, "name");
    int width = get_int_param(&request->params, 1, "width");
    int height = get_int_param(&request->params, 2, "height"); 
    int depth = get_int_param(&request->params, 3, "depth");
    
    // Use defaults if not specified
    if (width <= 0) width = 64;
    if (height <= 0) height = 64;
    if (depth <= 0) depth = 64;
    if (!name) name = "New Project";
    
    LOG_D("Creating project: %s (%dx%dx%d)", name, width, height, depth);
    
    int result = goxel_core_create_project(g_goxel_context, name, width, height, depth);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to create project: error code %d", result);
        project_lock_release(); // Release lock on error
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 1,
                                             error_msg, NULL, &request->id);
    }
    
    // Update project state
    g_project_state.has_active_project = true;
    strncpy(g_project_state.project_id, name ? name : "unnamed", 
            sizeof(g_project_state.project_id) - 1);
    g_project_state.last_activity = time(NULL);
    
    // IMPORTANT: Do NOT sync goxel.image here as it can cause double-free
    // The global goxel.image should remain independent from context->image
    // Only sync when absolutely necessary for operations that require it
    
    // Release lock after successful creation to allow subsequent requests
    // This fixes the single-request-per-session limitation
    project_lock_release();
    
    json_value *result_obj = json_object_new(5);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "name", json_string_new(name));
    json_object_push(result_obj, "width", json_integer_new(width));
    json_object_push(result_obj, "height", json_integer_new(height));
    json_object_push(result_obj, "depth", json_integer_new(depth));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_load_project(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: path (required)
    const char *path = get_string_param(&request->params, 0, "path");
    if (!path) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Missing required parameter: path",
                                             NULL, &request->id);
    }
    
    LOG_D("Loading project: %s", path);
    
    int result = goxel_core_load_project(g_goxel_context, path);
    if (result != 0) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to load project '%s': error code %d", path, result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 2,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "path", json_string_new(path));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_save_project(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: path (required)
    const char *path = get_string_param(&request->params, 0, "path");
    if (!path) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Missing required parameter: path",
                                             NULL, &request->id);
    }
    
    LOG_D("Saving project: %s", path);
    
    int result = goxel_core_save_project(g_goxel_context, path);
    if (result != 0) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to save project '%s': error code %d", path, result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 3,
                                             error_msg, NULL, &request->id);
    }
    
    // Don't release project lock or clear state - save doesn't close the project
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "path", json_string_new(path));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_add_voxel(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Enhanced parameter parsing supporting multiple formats:
    // Array format: [x, y, z, r, g, b, a?, layer_id?] or [x, y, z, "#RRGGBBAA", layer_id?]
    // Object format: {"x": 10, "y": 10, "z": 10, "color": [255, 0, 0, 255]} or {"x": 10, "y": 10, "z": 10, "color": "#FF0000FF"}
    
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    int layer_id = get_int_param(&request->params, 7, "layer_id"); // Default to 0 if not found
    
    // Use flexible color parsing
    uint8_t rgba[4];
    char color_error[256] = {0};
    json_rpc_result_t color_result = parse_color_param(&request->params, 3, "color", rgba, 
                                                       color_error, sizeof(color_error));
    
    if (color_result != JSON_RPC_SUCCESS) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             color_error[0] ? color_error : "Invalid color format",
                                             NULL, &request->id);
    }
    
    LOG_D("Adding voxel at (%d, %d, %d) with color (%d, %d, %d, %d) to layer %d", 
          x, y, z, rgba[0], rgba[1], rgba[2], rgba[3], layer_id);
    
    int result = goxel_core_add_voxel(g_goxel_context, x, y, z, rgba, layer_id);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add voxel: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 4,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(6);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "x", json_integer_new(x));
    json_object_push(result_obj, "y", json_integer_new(y));
    json_object_push(result_obj, "z", json_integer_new(z));
    json_object_push(result_obj, "layer_id", json_integer_new(layer_id));
    
    json_value *color_obj = json_array_new(4);
    json_array_push(color_obj, json_integer_new(rgba[0]));
    json_array_push(color_obj, json_integer_new(rgba[1]));
    json_array_push(color_obj, json_integer_new(rgba[2]));
    json_array_push(color_obj, json_integer_new(rgba[3]));
    json_object_push(result_obj, "color", color_obj);
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_remove_voxel(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: x, y, z, layer_id (optional, default 0)
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    int layer_id = get_int_param(&request->params, 3, "layer_id");
    
    LOG_D("Removing voxel at (%d, %d, %d) from layer %d", x, y, z, layer_id);
    
    int result = goxel_core_remove_voxel(g_goxel_context, x, y, z, layer_id);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to remove voxel: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 5,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(5);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "x", json_integer_new(x));
    json_object_push(result_obj, "y", json_integer_new(y));
    json_object_push(result_obj, "z", json_integer_new(z));
    json_object_push(result_obj, "layer_id", json_integer_new(layer_id));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_get_voxel(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: x, y, z
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    
    uint8_t rgba[4] = {0, 0, 0, 0};
    
    LOG_D("Getting voxel at (%d, %d, %d)", x, y, z);
    
    int result = goxel_core_get_voxel(g_goxel_context, x, y, z, rgba);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to get voxel: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 6,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(5);
    json_object_push(result_obj, "x", json_integer_new(x));
    json_object_push(result_obj, "y", json_integer_new(y));
    json_object_push(result_obj, "z", json_integer_new(z));
    json_object_push(result_obj, "exists", json_boolean_new(rgba[3] > 0));
    
    json_value *color_obj = json_array_new(4);
    json_array_push(color_obj, json_integer_new(rgba[0]));
    json_array_push(color_obj, json_integer_new(rgba[1]));
    json_array_push(color_obj, json_integer_new(rgba[2]));
    json_array_push(color_obj, json_integer_new(rgba[3]));
    json_object_push(result_obj, "color", color_obj);
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_export_model(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: path (required), format (optional, auto-detected from extension)
    const char *path = get_string_param(&request->params, 0, "path");
    const char *format = get_string_param(&request->params, 1, "format");
    
    if (!path) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Missing required parameter: path",
                                             NULL, &request->id);
    }
    
    LOG_D("Exporting model to: %s (format: %s)", path, format ? format : "auto");
    
    int result = goxel_core_export_project(g_goxel_context, path, format);
    if (result != 0) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to export model '%s': error code %d", path, result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 7,
                                             error_msg, NULL, &request->id);
    }
    
    // Don't release project lock or clear state - export doesn't close the project
    
    json_value *result_obj = json_object_new(3);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "path", json_string_new(path));
    if (format) {
        json_object_push(result_obj, "format", json_string_new(format));
    }
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_get_status(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    LOG_D("Getting Goxel status");
    
    int layer_count = goxel_core_get_layer_count(g_goxel_context);
    bool read_only = goxel_core_is_read_only(g_goxel_context);
    
    int width = 0, height = 0, depth = 0;
    goxel_core_get_project_bounds(g_goxel_context, &width, &height, &depth);
    
    json_value *result_obj = json_object_new(6);
    json_object_push(result_obj, "version", json_string_new(GOXEL_VERSION_STR));
    json_object_push(result_obj, "layer_count", json_integer_new(layer_count));
    json_object_push(result_obj, "read_only", json_boolean_new(read_only));
    json_object_push(result_obj, "width", json_integer_new(width));
    json_object_push(result_obj, "height", json_integer_new(height));
    json_object_push(result_obj, "depth", json_integer_new(depth));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_list_layers(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    LOG_D("Listing layers");
    
    int layer_count = goxel_core_get_layer_count(g_goxel_context);
    
    json_value *layers_array = json_array_new(0);
    
    // Note: This is a simplified implementation. In a real implementation,
    // you would iterate through actual layers and get their properties
    for (int i = 0; i < layer_count; i++) {
        json_value *layer_obj = json_object_new(3);
        json_object_push(layer_obj, "id", json_integer_new(i));
        
        char layer_name[64];
        snprintf(layer_name, sizeof(layer_name), "Layer %d", i);
        json_object_push(layer_obj, "name", json_string_new(layer_name));
        json_object_push(layer_obj, "visible", json_boolean_new(1));
        
        json_array_push(layers_array, layer_obj);
    }
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "count", json_integer_new(layer_count));
    json_object_push(result_obj, "layers", layers_array);
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_create_layer(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: name (optional), r, g, b (optional, default white), visible (optional, default true)
    const char *name = get_string_param(&request->params, 0, "name");
    int r = get_int_param(&request->params, 1, "r");
    int g = get_int_param(&request->params, 2, "g");
    int b = get_int_param(&request->params, 3, "b");
    bool visible = get_bool_param(&request->params, 4, "visible", true);
    
    if (!name) name = "New Layer";
    
    // Use default white color if not specified
    if (r <= 0 && g <= 0 && b <= 0) {
        r = g = b = 255;
    }
    
    // Validate color values
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Invalid color values (must be 0-255)",
                                             NULL, &request->id);
    }
    
    uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 255};
    
    LOG_D("Creating layer: %s with color (%d, %d, %d), visible: %s", 
          name, r, g, b, visible ? "true" : "false");
    
    int result = goxel_core_create_layer(g_goxel_context, name, rgba, visible ? 1 : 0);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to create layer: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 8,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(4);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "name", json_string_new(name));
    json_object_push(result_obj, "visible", json_boolean_new(visible));
    
    json_value *color_obj = json_array_new(3);
    json_array_push(color_obj, json_integer_new(r));
    json_array_push(color_obj, json_integer_new(g));
    json_array_push(color_obj, json_integer_new(b));
    json_object_push(result_obj, "color", color_obj);
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_paint_voxels(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: x, y, z, rgba[4], layer_id (optional)
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    int r = get_int_param(&request->params, 3, "r");
    int g = get_int_param(&request->params, 4, "g");
    int b = get_int_param(&request->params, 5, "b");
    int a = get_int_param(&request->params, 6, "a");
    int layer_id = get_int_param(&request->params, 7, "layer_id");
    
    if (a <= 0) a = 255; // Default alpha
    if (layer_id <= 0) layer_id = 1; // Default layer
    
    uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    LOG_D("Painting voxel at (%d, %d, %d) with color (%d, %d, %d, %d) on layer %d", 
          x, y, z, r, g, b, a, layer_id);
    
    int result = goxel_core_paint_voxel(g_goxel_context, x, y, z, rgba, layer_id);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to paint voxel: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 9,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "painted", json_integer_new(1));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_flood_fill(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: x, y, z, rgba[4], layer_id (optional)
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    int r = get_int_param(&request->params, 3, "r");
    int g = get_int_param(&request->params, 4, "g");
    int b = get_int_param(&request->params, 5, "b");
    int a = get_int_param(&request->params, 6, "a");
    int layer_id = get_int_param(&request->params, 7, "layer_id");
    
    if (a <= 0) a = 255;
    if (layer_id <= 0) layer_id = 1;
    
    uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    LOG_D("Flood filling from (%d, %d, %d) with color (%d, %d, %d, %d) on layer %d", 
          x, y, z, r, g, b, a, layer_id);
    
    // TODO: Implement flood fill in core - using simple voxel addition for now
    int result = goxel_core_add_voxel(g_goxel_context, x, y, z, rgba, layer_id);
    int voxels_filled = (result == 0) ? 1 : 0;
    if (result != 0) {
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 10,
                                             "Failed to perform flood fill",
                                             NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "voxels_filled", json_integer_new(voxels_filled));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_procedural_shape(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: shape_type, parameters, rgba[4], layer_id (optional)
    const char *shape_type = get_string_param(&request->params, 0, "shape_type");
    int size_x = get_int_param(&request->params, 1, "size_x");
    int size_y = get_int_param(&request->params, 2, "size_y");
    int size_z = get_int_param(&request->params, 3, "size_z");
    int center_x = get_int_param(&request->params, 4, "center_x");
    int center_y = get_int_param(&request->params, 5, "center_y");
    int center_z = get_int_param(&request->params, 6, "center_z");
    int r = get_int_param(&request->params, 7, "r");
    int g = get_int_param(&request->params, 8, "g");
    int b = get_int_param(&request->params, 9, "b");
    int a = get_int_param(&request->params, 10, "a");
    int layer_id = get_int_param(&request->params, 11, "layer_id");
    
    if (!shape_type) shape_type = "cube";
    if (size_x <= 0) size_x = 10;
    if (size_y <= 0) size_y = 10;
    if (size_z <= 0) size_z = 10;
    if (a <= 0) a = 255;
    if (layer_id <= 0) layer_id = 1;
    
    uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    LOG_D("Creating %s shape at (%d, %d, %d) size (%d, %d, %d) color (%d, %d, %d, %d) on layer %d", 
          shape_type, center_x, center_y, center_z, size_x, size_y, size_z, r, g, b, a, layer_id);
    
    // TODO: Implement procedural shapes in core - creating simple cube for now
    int voxels_created = 0;
    if (strcmp(shape_type, "cube") == 0) {
        for (int dx = -size_x/2; dx < size_x/2; dx++) {
            for (int dy = -size_y/2; dy < size_y/2; dy++) {
                for (int dz = -size_z/2; dz < size_z/2; dz++) {
                    int result = goxel_core_add_voxel(g_goxel_context, 
                                                     center_x + dx, center_y + dy, center_z + dz, 
                                                     rgba, layer_id);
                    if (result == 0) voxels_created++;
                }
            }
        }
    } else {
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 11,
                                             "Shape type not implemented yet",
                                             NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(3);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "shape_type", json_string_new(shape_type));
    json_object_push(result_obj, "voxels_created", json_integer_new(voxels_created));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_delete_layer(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: layer_id or layer_name
    int layer_id = get_int_param(&request->params, 0, "layer_id");
    const char *layer_name = get_string_param(&request->params, 1, "layer_name");
    
    if (layer_id <= 0 && !layer_name) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify either layer_id or layer_name",
                                             NULL, &request->id);
    }
    
    LOG_D("Deleting layer: ID=%d, Name=%s", layer_id, layer_name ? layer_name : "null");
    
    int result = goxel_core_delete_layer(g_goxel_context, layer_id, layer_name);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to delete layer: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 12,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(1);
    json_object_push(result_obj, "success", json_boolean_new(1));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_merge_layers(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: source_layer_id, target_layer_id
    int source_layer_id = get_int_param(&request->params, 0, "source_layer_id");
    int target_layer_id = get_int_param(&request->params, 1, "target_layer_id");
    
    if (source_layer_id <= 0 || target_layer_id <= 0) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify valid source_layer_id and target_layer_id",
                                             NULL, &request->id);
    }
    
    LOG_D("Merging layer %d into layer %d", source_layer_id, target_layer_id);
    
    int result = goxel_core_merge_layers(g_goxel_context, source_layer_id, target_layer_id, NULL, NULL);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to merge layers: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 13,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(1);
    json_object_push(result_obj, "success", json_boolean_new(1));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_set_layer_visibility(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: layer_id, visible
    int layer_id = get_int_param(&request->params, 0, "layer_id");
    bool visible = get_bool_param(&request->params, 1, "visible", true);
    
    if (layer_id <= 0) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify valid layer_id",
                                             NULL, &request->id);
    }
    
    LOG_D("Setting layer %d visibility to %s", layer_id, visible ? "visible" : "hidden");
    
    int result = goxel_core_set_layer_visibility(g_goxel_context, layer_id, NULL, visible ? 1 : 0);
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to set layer visibility: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 14,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "visible", json_boolean_new(visible));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_render_scene(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters with backward compatibility support
    // Legacy format: [output_path, width, height]
    // New format: {"width": 800, "height": 600, "options": {"return_mode": "file_path"}}
    
    const char *output_path = NULL;
    int width = 512, height = 512;
    const char *return_mode = NULL;
    const char *format = "png";
    uint8_t background_color[4] = {240, 240, 240, 255}; // Default gray
    bool has_custom_background = false;
    
    // Check if using legacy array format or new object format
    if (request->params.type == JSON_RPC_PARAMS_ARRAY) {
        // Legacy format: [output_path, width, height]
        output_path = get_string_param(&request->params, 0, "output_path");
        width = get_int_param(&request->params, 1, "width");
        height = get_int_param(&request->params, 2, "height");
        
        if (width <= 0) width = 512;
        if (height <= 0) height = 512;
        
        LOG_D("Using legacy render_scene format");
    } else {
        // New object format
        width = get_int_param(&request->params, -1, "width");
        height = get_int_param(&request->params, -1, "height");
        output_path = get_string_param(&request->params, -1, "output_path");
        
        if (width <= 0) width = 512;
        if (height <= 0) height = 512;
        
        // Check for options object
        json_value *options = NULL;
        json_rpc_result_t opt_result = json_rpc_get_param_by_name(&request->params, "options", &options);
        if (opt_result == JSON_RPC_SUCCESS && options && options->type == json_object) {
            json_value *return_mode_val = json_object_get_helper(options, "return_mode");
            if (return_mode_val && return_mode_val->type == json_string) {
                return_mode = return_mode_val->u.string.ptr;
            }
            
            // Parse background_color array [r, g, b, a]
            json_value *bg_color_val = json_object_get_helper(options, "background_color");
            if (bg_color_val && bg_color_val->type == json_array && bg_color_val->u.array.length >= 3) {
                // Extract RGB(A) values
                for (int i = 0; i < 4 && i < bg_color_val->u.array.length; i++) {
                    json_value *color_val = bg_color_val->u.array.values[i];
                    if (color_val && color_val->type == json_integer) {
                        int color_comp = (int)color_val->u.integer;
                        background_color[i] = (uint8_t)(color_comp < 0 ? 0 : color_comp > 255 ? 255 : color_comp);
                    }
                }
                // Default alpha to 255 if not provided
                if (bg_color_val->u.array.length < 4) {
                    background_color[3] = 255;
                }
                has_custom_background = true;
                LOG_D("Custom background color: [%d,%d,%d,%d]", 
                      background_color[0], background_color[1], background_color[2], background_color[3]);
            }
        }
        
        LOG_D("Using new render_scene format with return_mode: %s", return_mode ? return_mode : "default");
    }
    
    // Determine the render mode
    bool use_file_transfer = false;
    char managed_file_path[1024] = {0};
    
    if (return_mode && strcmp(return_mode, "file_path") == 0) {
        // New file transfer mode - use render manager
        if (!g_render_manager) {
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Render manager not available, file transfer mode disabled",
                                                 NULL, &request->id);
        }
        
        use_file_transfer = true;
        
        // Generate managed file path
        render_manager_error_t rm_result = render_manager_create_path(g_render_manager, 
                                                                     NULL, // session_id - auto-generated
                                                                     format,
                                                                     managed_file_path,
                                                                     sizeof(managed_file_path));
        if (rm_result != RENDER_MGR_SUCCESS) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Failed to create render path: %s", 
                     render_manager_error_string(rm_result));
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 error_msg, NULL, &request->id);
        }
        
        output_path = managed_file_path;
        LOG_D("Using managed file path: %s", output_path);
    }
    
    LOG_D("Rendering scene %dx%d format %s to %s (mode: %s)", 
          width, height, format, output_path ? output_path : "memory", 
          use_file_transfer ? "file_transfer" : "legacy");
    
    // Render the scene
    int result;
    if (output_path) {
        result = goxel_core_render_to_file(g_goxel_context, output_path, width, height, format, 90, NULL, has_custom_background ? background_color : NULL);
    } else {
        result = -1; // Buffer rendering not implemented
    }
    
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to render scene: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 15,
                                             error_msg, NULL, &request->id);
    }
    
    // Prepare response based on mode
    if (use_file_transfer) {
        // Register the render with the manager
        // Extract session_id from generated path (format: render_timestamp_sessionid_hash.ext)
        char session_id[64] = "default";
        const char *basename = strrchr(output_path, '/');
        if (basename) {
            basename++; // Skip the '/'
            // Parse: render_1754861551_auto1754861551_d01945aa.png
            const char *second_underscore = strchr(basename, '_');
            if (second_underscore) {
                second_underscore = strchr(second_underscore + 1, '_');
                if (second_underscore) {
                    const char *third_underscore = strchr(second_underscore + 1, '_');
                    if (third_underscore) {
                        size_t session_len = third_underscore - (second_underscore + 1);
                        if (session_len < sizeof(session_id) - 1) {
                            strncpy(session_id, second_underscore + 1, session_len);
                            session_id[session_len] = '\0';
                        }
                    }
                }
            }
        }
        
        render_manager_error_t rm_result = render_manager_register(g_render_manager,
                                                                  output_path,
                                                                  session_id,
                                                                  format,
                                                                  width,
                                                                  height);
        if (rm_result != RENDER_MGR_SUCCESS) {
            LOG_W("Failed to register render with manager: %s", render_manager_error_string(rm_result));
        }
        
        // Get file metadata
        long file_size = get_file_size(output_path);
        int actual_width, actual_height;
        if (get_png_dimensions(output_path, &actual_width, &actual_height) != 0) {
            actual_width = width;
            actual_height = height;
        }
        
        // Calculate checksum
        char checksum[128] = {0};
        render_manager_error_t checksum_result = render_manager_calculate_checksum(output_path, 
                                                                                   checksum, 
                                                                                   sizeof(checksum));
        if (checksum_result != RENDER_MGR_SUCCESS) {
            snprintf(checksum, sizeof(checksum), "sha256:unavailable");
        }
        
        // Create enhanced response for file transfer mode
        json_value *result_obj = json_object_new(2);
        json_object_push(result_obj, "success", json_boolean_new(1));
        
        json_value *file_obj = json_object_new(8);
        json_object_push(file_obj, "path", json_string_new(output_path));
        json_object_push(file_obj, "size", json_integer_new(file_size));
        json_object_push(file_obj, "format", json_string_new(format));
        
        json_value *dimensions_obj = json_object_new(2);
        json_object_push(dimensions_obj, "width", json_integer_new(actual_width));
        json_object_push(dimensions_obj, "height", json_integer_new(actual_height));
        json_object_push(file_obj, "dimensions", dimensions_obj);
        
        json_object_push(file_obj, "checksum", json_string_new(checksum));
        
        time_t now = time(NULL);
        json_object_push(file_obj, "created_at", json_integer_new(now));
        json_object_push(file_obj, "expires_at", json_integer_new(now + 3600)); // 1 hour TTL
        
        json_object_push(result_obj, "file", file_obj);
        
        return json_rpc_create_response_result(result_obj, &request->id);
    } else {
        // Legacy response format
        json_value *result_obj = json_object_new(4);
        json_object_push(result_obj, "success", json_boolean_new(1));
        json_object_push(result_obj, "width", json_integer_new(width));
        json_object_push(result_obj, "height", json_integer_new(height));
        json_object_push(result_obj, "format", json_string_new(format));
        
        if (output_path) {
            json_object_push(result_obj, "output_path", json_string_new(output_path));
        } else {
            json_object_push(result_obj, "note", json_string_new("Buffer rendering not yet implemented"));
        }
        
        return json_rpc_create_response_result(result_obj, &request->id);
    }
}

static json_rpc_response_t *handle_goxel_batch_operations(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parameters: operations (array of operation objects)
    json_value *operations = NULL;
    json_rpc_result_t result = json_rpc_get_param_by_name(&request->params, "operations", &operations);
    
    if (result != JSON_RPC_SUCCESS || !operations || operations->type != json_array) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify operations array",
                                             NULL, &request->id);
    }
    
    int total_operations = (int)operations->u.array.length;
    int successful_operations = 0;
    
    LOG_D("Processing batch of %d operations", total_operations);
    
    // Process operations individually for now
    // TODO: Optimize to use goxel_core_add_voxels_batch when operations are uniform
    
    for (unsigned int i = 0; i < operations->u.array.length; i++) {
        json_value *op = operations->u.array.values[i];
        if (!op || op->type != json_object) continue;
        
        json_value *op_type = json_object_get_helper(op, "type");
        if (!op_type || op_type->type != json_string) continue;
        
        const char *type = op_type->u.string.ptr;
        int op_result = -1;
        
        if (strcmp(type, "add_voxel") == 0) {
            json_value *x_val = json_object_get_helper(op, "x");
            json_value *y_val = json_object_get_helper(op, "y");
            json_value *z_val = json_object_get_helper(op, "z");
            json_value *r_val = json_object_get_helper(op, "r");
            json_value *g_val = json_object_get_helper(op, "g");
            json_value *b_val = json_object_get_helper(op, "b");
            json_value *a_val = json_object_get_helper(op, "a");
            
            if (x_val && y_val && z_val && r_val && g_val && b_val) {
                int x = (int)x_val->u.integer;
                int y = (int)y_val->u.integer;
                int z = (int)z_val->u.integer;
                uint8_t rgba[4] = {
                    (uint8_t)r_val->u.integer,
                    (uint8_t)g_val->u.integer,
                    (uint8_t)b_val->u.integer,
                    a_val ? (uint8_t)a_val->u.integer : 255
                };
                op_result = goxel_core_add_voxel(g_goxel_context, x, y, z, rgba, 1);
            }
        }
        // Add more operation types as needed
        
        if (op_result == 0) {
            successful_operations++;
        }
    }
    
    json_value *result_obj = json_object_new(3);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "total_operations", json_integer_new(total_operations));
    json_object_push(result_obj, "successful_operations", json_integer_new(successful_operations));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

// ============================================================================
// SCRIPT EXECUTION HANDLER
// ============================================================================

// Structure to pass script execution data to worker thread
typedef struct {
    char *script_code;
    char *script_path;
    char *source_name;
    json_rpc_id_t request_id;
    pthread_mutex_t result_mutex;  // Embedded, not pointer
    pthread_cond_t result_cond;    // Embedded, not pointer
    int result_code;
    char *result_message;
    bool completed;
} script_execution_request_t;

// Worker pool for script execution (initialized elsewhere)
extern worker_pool_t *g_script_worker_pool;

// Global mutex for script execution (QuickJS is not thread-safe)
static pthread_mutex_t g_script_mutex = PTHREAD_MUTEX_INITIALIZER;

// Process function for script execution in worker thread
int process_script_execution(void *request_data, int worker_id, void *context)
{
    script_execution_request_t *script_req = (script_execution_request_t *)request_data;
    int result = -1;
    
    LOG_D("Worker %d received script_req %p", worker_id, script_req);
    LOG_D("Worker %d executing script: %s", worker_id, 
          script_req->source_name ? script_req->source_name : "<anonymous>");
    
    // In daemon mode, we don't call script_init() as it tries to load
    // script files that may not exist. The QuickJS runtime will be
    // initialized automatically on first use by script_run_from_string/file.
    
    // Serialize script execution (QuickJS runtime is not thread-safe)
    pthread_mutex_lock(&g_script_mutex);
    
    // Execute the script
    if (script_req->script_code) {
        // Execute from string
        LOG_D("About to execute script: '%s'", script_req->script_code);
        result = script_run_from_string(script_req->script_code, script_req->source_name);
        LOG_D("Script execution returned: %d", result);
    } else if (script_req->script_path) {
        // Execute from file
        LOG_D("About to execute script file: '%s'", script_req->script_path);
        result = script_run_from_file(script_req->script_path, 0, NULL);
        LOG_D("Script file execution returned: %d", result);
    }
    
    pthread_mutex_unlock(&g_script_mutex);
    
    LOG_D("Script mutex unlocked, about to store result");
    
    // Store result
    pthread_mutex_lock(&script_req->result_mutex);
    LOG_D("Acquired result mutex, storing result %d", result);
    script_req->result_code = result;
    LOG_D("Set result_code to %d, verifying: result_code=%d", result, script_req->result_code);
    if (result == 0) {
        script_req->result_message = strdup("Script executed successfully");
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Script execution failed with code %d", result);
        script_req->result_message = strdup(error_msg);
    }
    LOG_D("Set result_message to: %s", script_req->result_message);
    script_req->completed = true;
    LOG_D("About to signal completion, result_code=%d, message=%s", 
          script_req->result_code, script_req->result_message);
    pthread_cond_signal(&script_req->result_cond);
    pthread_mutex_unlock(&script_req->result_mutex);
    LOG_D("Result stored and signaled, final check: result_code=%d", script_req->result_code);
    
    return result;
}

// Cleanup function for script execution request  
void cleanup_script_execution(void *request_data)
{
    script_execution_request_t *script_req = (script_execution_request_t *)request_data;
    if (script_req) {
        SAFE_FREE(script_req->script_code);
        SAFE_FREE(script_req->script_path);
        SAFE_FREE(script_req->source_name);
        SAFE_FREE(script_req->result_message);
        // Destroy synchronization primitives
        pthread_mutex_destroy(&script_req->result_mutex);
        pthread_cond_destroy(&script_req->result_cond);
        free(script_req);
    }
}

static json_rpc_response_t *handle_goxel_execute_script(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Check if worker pool is available
    if (!g_script_worker_pool || !worker_pool_is_running(g_script_worker_pool)) {
        LOG_W("Script worker pool not available, executing synchronously");
        
        // Fallback to synchronous execution
        json_value *script_val = NULL;
        json_value *path_val = NULL;
        json_value *name_val = NULL;
        
        json_rpc_get_param_by_name(&request->params, "script", &script_val);
        json_rpc_get_param_by_name(&request->params, "path", &path_val);
        json_rpc_get_param_by_name(&request->params, "name", &name_val);
        
        if (!script_val && !path_val) {
            return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                                 "Must specify either 'script' (code) or 'path' parameter",
                                                 NULL, &request->id);
        }
        
        int result = -1;
        const char *source_name = name_val ? name_val->u.string.ptr : "<inline>";
        
        if (script_val && script_val->type == json_string) {
            result = script_run_from_string(script_val->u.string.ptr, source_name);
        } else if (path_val && path_val->type == json_string) {
            result = script_run_from_file(path_val->u.string.ptr, 0, NULL);
        }
        
        json_value *result_obj = json_object_new(3);
        json_object_push(result_obj, "success", json_boolean_new(result == 0));
        json_object_push(result_obj, "code", json_integer_new(result));
        json_object_push(result_obj, "message", 
                        json_string_new(result == 0 ? "Script executed successfully" : "Script execution failed"));
        
        return json_rpc_create_response_result(result_obj, &request->id);
    }
    
    // Asynchronous execution with worker pool
    json_value *script_val = NULL;
    json_value *path_val = NULL;
    json_value *name_val = NULL;
    json_value *timeout_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "script", &script_val);
    json_rpc_get_param_by_name(&request->params, "path", &path_val);
    json_rpc_get_param_by_name(&request->params, "name", &name_val);
    json_rpc_get_param_by_name(&request->params, "timeout_ms", &timeout_val);
    
    if (!script_val && !path_val) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify either 'script' (code) or 'path' parameter",
                                             NULL, &request->id);
    }
    
    // Create script execution request
    script_execution_request_t *script_req = calloc(1, sizeof(script_execution_request_t));
    if (!script_req) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    // Initialize synchronization primitives
    pthread_mutex_init(&script_req->result_mutex, NULL);
    pthread_cond_init(&script_req->result_cond, NULL);
    script_req->request_id = request->id;
    script_req->completed = false;
    
    // Set script parameters
    if (script_val && script_val->type == json_string) {
        script_req->script_code = strdup(script_val->u.string.ptr);
    }
    if (path_val && path_val->type == json_string) {
        script_req->script_path = strdup(path_val->u.string.ptr);
    }
    if (name_val && name_val->type == json_string) {
        script_req->source_name = strdup(name_val->u.string.ptr);
    } else {
        script_req->source_name = strdup(script_req->script_path ? script_req->script_path : "<inline>");
    }
    
    // Submit to worker pool
    LOG_D("Submitting script_req %p to worker pool", script_req);
    worker_pool_error_t pool_result = worker_pool_submit_request(g_script_worker_pool, 
                                                                script_req, 
                                                                WORKER_PRIORITY_NORMAL);
    
    if (pool_result != WORKER_POOL_SUCCESS) {
        cleanup_script_execution(script_req);
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Failed to submit script execution request",
                                             NULL, &request->id);
    }
    
    // Wait for completion with timeout
    int timeout_ms = 30000; // Default 30 seconds
    if (timeout_val && timeout_val->type == json_integer) {
        timeout_ms = (int)timeout_val->u.integer;
        if (timeout_ms <= 0 || timeout_ms > 300000) { // Max 5 minutes
            timeout_ms = 30000;
        }
    }
    
    struct timespec timeout_spec;
    clock_gettime(CLOCK_REALTIME, &timeout_spec);
    timeout_spec.tv_sec += timeout_ms / 1000;
    timeout_spec.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (timeout_spec.tv_nsec >= 1000000000L) {
        timeout_spec.tv_sec++;
        timeout_spec.tv_nsec -= 1000000000L;
    }
    
    pthread_mutex_lock(&script_req->result_mutex);
    int wait_result = 0;
    while (!script_req->completed && wait_result == 0) {
        wait_result = pthread_cond_timedwait(&script_req->result_cond, &script_req->result_mutex, &timeout_spec);
    }
    
    json_rpc_response_t *response;
    if (wait_result == ETIMEDOUT) {
        response = json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Script execution timeout",
                                                 NULL, &request->id);
    } else {
        LOG_D("Wait completed, checking script_req state");
        LOG_D("script_req=%p, completed=%d", script_req, script_req->completed);
        LOG_D("Creating response with script_req %p, result_code=%d, message=%s", 
              script_req, script_req->result_code, 
              script_req->result_message ? script_req->result_message : "NULL");
        
        // Double-check the values before creating response
        int final_code = script_req->result_code;
        const char *final_message = script_req->result_message;
        LOG_D("Final values: code=%d, message=%s", final_code, final_message ? final_message : "NULL");
        
        json_value *result_obj = json_object_new(3);
        json_object_push(result_obj, "success", json_boolean_new(final_code == 0));
        json_object_push(result_obj, "code", json_integer_new(final_code));
        json_object_push(result_obj, "message", 
                        json_string_new(final_message ? final_message : "Unknown result"));
        response = json_rpc_create_response_result(result_obj, &request->id);
    }
    
    pthread_mutex_unlock(&script_req->result_mutex);
    
    // Clean up the request data manually since we disabled automatic cleanup
    cleanup_script_execution(script_req);
    
    return response;
}

// ============================================================================
// BULK VOXEL READING HANDLERS
// ============================================================================

// External worker pool (initialized in daemon main)
extern worker_pool_t *g_worker_pool;

static json_rpc_response_t *handle_goxel_get_voxels_region(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *min_val = NULL;
    json_value *max_val = NULL;
    json_value *layer_id_val = NULL;
    json_value *color_filter_val = NULL;
    json_value *offset_val = NULL;
    json_value *limit_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "min", &min_val);
    json_rpc_get_param_by_name(&request->params, "max", &max_val);
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "color_filter", &color_filter_val);
    json_rpc_get_param_by_name(&request->params, "offset", &offset_val);
    json_rpc_get_param_by_name(&request->params, "limit", &limit_val);
    
    // Validate required parameters
    if (!min_val || min_val->type != json_array || min_val->u.array.length != 3) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Parameter 'min' must be an array of 3 integers",
                                             NULL, &request->id);
    }
    
    if (!max_val || max_val->type != json_array || max_val->u.array.length != 3) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Parameter 'max' must be an array of 3 integers",
                                             NULL, &request->id);
    }
    
    // Create bulk voxel context
    bulk_voxel_context_t *ctx = calloc(1, sizeof(bulk_voxel_context_t));
    if (!ctx) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    // Extract min/max coordinates
    for (int i = 0; i < 3; i++) {
        json_value *min_elem = min_val->u.array.values[i];
        json_value *max_elem = max_val->u.array.values[i];
        
        if (!min_elem || min_elem->type != json_integer ||
            !max_elem || max_elem->type != json_integer) {
            free(ctx);
            return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                                 "Min/max arrays must contain integers",
                                                 NULL, &request->id);
        }
        
        ctx->min[i] = (int)min_elem->u.integer;
        ctx->max[i] = (int)max_elem->u.integer;
    }
    
    // Optional parameters
    ctx->layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    ctx->offset = offset_val ? (int)offset_val->u.integer : 0;
    ctx->limit = limit_val ? (int)limit_val->u.integer : BULK_VOXELS_CHUNK_SIZE;
    
    // Parse color filter if provided
    if (color_filter_val && color_filter_val->type == json_array && 
        color_filter_val->u.array.length >= 3) {
        ctx->use_color_filter = true;
        for (int i = 0; i < 4; i++) {
            if (i < color_filter_val->u.array.length) {
                json_value *c = color_filter_val->u.array.values[i];
                ctx->color_filter[i] = c ? (uint8_t)c->u.integer : 0;
            } else {
                ctx->color_filter[i] = (i == 3) ? 255 : 0; // Default alpha to 255
            }
        }
    }
    
    ctx->goxel_ctx = g_goxel_context;
    ctx->request = (json_rpc_request_t *)request; // Safe cast - we won't modify
    
    // Use worker pool if available for large operations
    if (g_worker_pool && worker_pool_is_running(g_worker_pool)) {
        int ret = bulk_voxel_worker(ctx, 0, NULL);
        if (ret != 0 || !ctx->response) {
            free(ctx);
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Bulk operation failed",
                                                 NULL, &request->id);
        }
        
        json_rpc_response_t *response = ctx->response;
        free(ctx);
        return response;
    } else {
        // Synchronous fallback
        bulk_voxel_result_t result = {0};
        int ret = bulk_get_voxels_region(g_goxel_context,
                                         ctx->min, ctx->max,
                                         ctx->layer_id,
                                         ctx->use_color_filter ? ctx->color_filter : NULL,
                                         ctx->offset, ctx->limit,
                                         &result);
        
        if (ret != 0) {
            bulk_voxel_result_free(&result);
            free(ctx);
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Failed to get voxels",
                                                 NULL, &request->id);
        }
        
        json_value *json_result = bulk_voxel_result_to_json(&result, 
                                                            BULK_COMPRESS_NONE,
                                                            true);
        bulk_voxel_result_free(&result);
        free(ctx);
        
        if (!json_result) {
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Failed to convert result to JSON",
                                                 NULL, &request->id);
        }
        
        return json_rpc_create_response_result(json_result, &request->id);
    }
}

static json_rpc_response_t *handle_goxel_get_layer_voxels(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *layer_id_val = NULL;
    json_value *color_filter_val = NULL;
    json_value *offset_val = NULL;
    json_value *limit_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "color_filter", &color_filter_val);
    json_rpc_get_param_by_name(&request->params, "offset", &offset_val);
    json_rpc_get_param_by_name(&request->params, "limit", &limit_val);
    
    // Create bulk voxel context
    bulk_voxel_context_t *ctx = calloc(1, sizeof(bulk_voxel_context_t));
    if (!ctx) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    // Optional parameters
    ctx->layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    ctx->offset = offset_val ? (int)offset_val->u.integer : 0;
    ctx->limit = limit_val ? (int)limit_val->u.integer : BULK_VOXELS_CHUNK_SIZE;
    
    // Parse color filter if provided
    if (color_filter_val && color_filter_val->type == json_array && 
        color_filter_val->u.array.length >= 3) {
        ctx->use_color_filter = true;
        for (int i = 0; i < 4; i++) {
            if (i < color_filter_val->u.array.length) {
                json_value *c = color_filter_val->u.array.values[i];
                ctx->color_filter[i] = c ? (uint8_t)c->u.integer : 0;
            } else {
                ctx->color_filter[i] = (i == 3) ? 255 : 0; // Default alpha to 255
            }
        }
    }
    
    ctx->goxel_ctx = g_goxel_context;
    ctx->request = (json_rpc_request_t *)request;
    
    // Use worker pool if available
    if (g_worker_pool && worker_pool_is_running(g_worker_pool)) {
        int ret = bulk_voxel_worker(ctx, 0, NULL);
        if (ret != 0 || !ctx->response) {
            free(ctx);
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Bulk operation failed",
                                                 NULL, &request->id);
        }
        
        json_rpc_response_t *response = ctx->response;
        free(ctx);
        return response;
    } else {
        // Synchronous fallback
        bulk_voxel_result_t result = {0};
        int ret = bulk_get_layer_voxels(g_goxel_context,
                                        ctx->layer_id,
                                        ctx->use_color_filter ? ctx->color_filter : NULL,
                                        ctx->offset, ctx->limit,
                                        &result);
        
        if (ret != 0) {
            bulk_voxel_result_free(&result);
            free(ctx);
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Failed to get layer voxels",
                                                 NULL, &request->id);
        }
        
        json_value *json_result = bulk_voxel_result_to_json(&result, 
                                                            BULK_COMPRESS_NONE,
                                                            true);
        bulk_voxel_result_free(&result);
        free(ctx);
        
        if (!json_result) {
            return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                                 "Failed to convert result to JSON",
                                                 NULL, &request->id);
        }
        
        return json_rpc_create_response_result(json_result, &request->id);
    }
}

static json_rpc_response_t *handle_goxel_get_bounding_box(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *layer_id_val = NULL;
    json_value *exact_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "exact", &exact_val);
    
    int layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    bool exact = exact_val ? (exact_val->type == json_boolean && exact_val->u.boolean) : true;
    
    int bbox[2][3];
    int ret = bulk_get_bounding_box(g_goxel_context, layer_id, exact, bbox);
    
    if (ret < 0) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Failed to get bounding box",
                                             NULL, &request->id);
    }
    
    json_value *result = bulk_bbox_to_json(bbox, ret == 1);
    if (!result) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Failed to convert result to JSON",
                                             NULL, &request->id);
    }
    
    return json_rpc_create_response_result(result, &request->id);
}

// ============================================================================
// COLOR ANALYSIS HANDLERS
// ============================================================================

static json_rpc_response_t *handle_goxel_get_color_histogram(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *layer_id_val = NULL;
    json_value *region_val = NULL;
    json_value *bin_size_val = NULL;
    json_value *sort_by_count_val = NULL;
    json_value *top_n_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "region", &region_val);
    json_rpc_get_param_by_name(&request->params, "bin_size", &bin_size_val);
    json_rpc_get_param_by_name(&request->params, "sort_by_count", &sort_by_count_val);
    json_rpc_get_param_by_name(&request->params, "top_n", &top_n_val);
    
    // Create color analysis context
    color_analysis_context_t *ctx = calloc(1, sizeof(color_analysis_context_t));
    if (!ctx) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    ctx->goxel_ctx = g_goxel_context;
    ctx->request = (json_rpc_request_t *)request;
    ctx->analysis_type = COLOR_ANALYSIS_HISTOGRAM;
    ctx->layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    ctx->bin_size = bin_size_val ? (int)bin_size_val->u.integer : 0;
    ctx->sort_by_count = sort_by_count_val ? sort_by_count_val->u.boolean : true;
    ctx->top_n = top_n_val ? (int)top_n_val->u.integer : 0;
    
    // Parse region if provided
    if (region_val && region_val->type == json_object) {
        json_value *min_val = json_object_get_helper(region_val, "min");
        json_value *max_val = json_object_get_helper(region_val, "max");
        
        if (min_val && min_val->type == json_array && min_val->u.array.length >= 3 &&
            max_val && max_val->type == json_array && max_val->u.array.length >= 3) {
            ctx->use_region = true;
            for (int i = 0; i < 3; i++) {
                ctx->region_min[i] = (int)min_val->u.array.values[i]->u.integer;
                ctx->region_max[i] = (int)max_val->u.array.values[i]->u.integer;
            }
        }
    }
    
    // Execute analysis in worker thread
    color_analysis_worker(ctx, 0, NULL);
    
    json_rpc_response_t *response = ctx->response;
    color_analysis_cleanup(ctx);
    
    return response;
}

static json_rpc_response_t *handle_goxel_find_voxels_by_color(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *color_val = NULL;
    json_value *tolerance_val = NULL;
    json_value *layer_id_val = NULL;
    json_value *region_val = NULL;
    json_value *max_results_val = NULL;
    json_value *include_locations_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "color", &color_val);
    json_rpc_get_param_by_name(&request->params, "tolerance", &tolerance_val);
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "region", &region_val);
    json_rpc_get_param_by_name(&request->params, "max_results", &max_results_val);
    json_rpc_get_param_by_name(&request->params, "include_locations", &include_locations_val);
    
    // Validate required color parameter
    if (!color_val || color_val->type != json_array || color_val->u.array.length < 3) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Color parameter must be an array of [r,g,b] or [r,g,b,a]",
                                             NULL, &request->id);
    }
    
    // Create color analysis context
    color_analysis_context_t *ctx = calloc(1, sizeof(color_analysis_context_t));
    if (!ctx) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    ctx->goxel_ctx = g_goxel_context;
    ctx->request = (json_rpc_request_t *)request;
    ctx->analysis_type = COLOR_ANALYSIS_FIND_BY_COLOR;
    ctx->layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    ctx->max_results = max_results_val ? (int)max_results_val->u.integer : 1000;
    ctx->include_locations = include_locations_val ? include_locations_val->u.boolean : true;
    
    // Parse color
    for (int i = 0; i < 3; i++) {
        ctx->target_color[i] = (uint8_t)color_val->u.array.values[i]->u.integer;
    }
    ctx->target_color[3] = (color_val->u.array.length >= 4) ? 
        (uint8_t)color_val->u.array.values[3]->u.integer : 255;
    
    // Parse tolerance if provided
    if (tolerance_val) {
        if (tolerance_val->type == json_integer) {
            // Single value for all channels
            uint8_t tol = (uint8_t)tolerance_val->u.integer;
            for (int i = 0; i < 4; i++) {
                ctx->tolerance[i] = tol;
            }
        } else if (tolerance_val->type == json_array && tolerance_val->u.array.length >= 3) {
            // Per-channel tolerance
            for (int i = 0; i < 3; i++) {
                ctx->tolerance[i] = (uint8_t)tolerance_val->u.array.values[i]->u.integer;
            }
            ctx->tolerance[3] = (tolerance_val->u.array.length >= 4) ?
                (uint8_t)tolerance_val->u.array.values[3]->u.integer : 0;
        }
    }
    
    // Parse region if provided
    if (region_val && region_val->type == json_object) {
        json_value *min_val = json_object_get_helper(region_val, "min");
        json_value *max_val = json_object_get_helper(region_val, "max");
        
        if (min_val && min_val->type == json_array && min_val->u.array.length >= 3 &&
            max_val && max_val->type == json_array && max_val->u.array.length >= 3) {
            ctx->use_region = true;
            for (int i = 0; i < 3; i++) {
                ctx->region_min[i] = (int)min_val->u.array.values[i]->u.integer;
                ctx->region_max[i] = (int)max_val->u.array.values[i]->u.integer;
            }
        }
    }
    
    // Execute analysis in worker thread
    color_analysis_worker(ctx, 0, NULL);
    
    json_rpc_response_t *response = ctx->response;
    color_analysis_cleanup(ctx);
    
    return response;
}

static json_rpc_response_t *handle_goxel_get_unique_colors(const json_rpc_request_t *request)
{
    if (!g_goxel_context) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Goxel context not initialized",
                                             NULL, &request->id);
    }
    
    // Parse parameters
    json_value *layer_id_val = NULL;
    json_value *region_val = NULL;
    json_value *merge_similar_val = NULL;
    json_value *merge_threshold_val = NULL;
    json_value *sort_by_count_val = NULL;
    
    json_rpc_get_param_by_name(&request->params, "layer_id", &layer_id_val);
    json_rpc_get_param_by_name(&request->params, "region", &region_val);
    json_rpc_get_param_by_name(&request->params, "merge_similar", &merge_similar_val);
    json_rpc_get_param_by_name(&request->params, "merge_threshold", &merge_threshold_val);
    json_rpc_get_param_by_name(&request->params, "sort_by_count", &sort_by_count_val);
    
    // Create color analysis context
    color_analysis_context_t *ctx = calloc(1, sizeof(color_analysis_context_t));
    if (!ctx) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Out of memory",
                                             NULL, &request->id);
    }
    
    ctx->goxel_ctx = g_goxel_context;
    ctx->request = (json_rpc_request_t *)request;
    ctx->analysis_type = COLOR_ANALYSIS_UNIQUE_COLORS;
    ctx->layer_id = layer_id_val ? (int)layer_id_val->u.integer : -1;
    ctx->merge_similar = merge_similar_val ? merge_similar_val->u.boolean : false;
    ctx->merge_threshold = merge_threshold_val ? (int)merge_threshold_val->u.integer : 10;
    ctx->sort_by_count = sort_by_count_val ? sort_by_count_val->u.boolean : false;
    
    // Parse region if provided
    if (region_val && region_val->type == json_object) {
        json_value *min_val = json_object_get_helper(region_val, "min");
        json_value *max_val = json_object_get_helper(region_val, "max");
        
        if (min_val && min_val->type == json_array && min_val->u.array.length >= 3 &&
            max_val && max_val->type == json_array && max_val->u.array.length >= 3) {
            ctx->use_region = true;
            for (int i = 0; i < 3; i++) {
                ctx->region_min[i] = (int)min_val->u.array.values[i]->u.integer;
                ctx->region_max[i] = (int)max_val->u.array.values[i]->u.integer;
            }
        }
    }
    
    // Execute analysis in worker thread
    color_analysis_worker(ctx, 0, NULL);
    
    json_rpc_response_t *response = ctx->response;
    color_analysis_cleanup(ctx);
    
    return response;
}

// ============================================================================
// JSON UTILITY FUNCTIONS
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

// ============================================================================
// HELPER FUNCTIONS FOR TEST METHODS
// ============================================================================

bool json_rpc_is_goxel_initialized(void)
{
    return g_goxel_context != NULL;
}

int json_rpc_get_method_count(void)
{
    size_t test_method_count = 0;
    get_test_methods(&test_method_count);
    return (int)(g_method_registry_size + test_method_count);
}

int json_rpc_add_voxel_internal(int x, int y, int z, const uint8_t rgba[4], int layer_id)
{
    if (!g_goxel_context) {
        return -1;
    }
    // Cast away const - goxel_core_add_voxel doesn't modify the array
    return goxel_core_add_voxel(g_goxel_context, x, y, z, (uint8_t*)rgba, layer_id);
}

// ============================================================================
// RENDER MANAGEMENT METHODS (Phase 2: v0.16 Render Transfer Architecture)
// ============================================================================

static json_rpc_response_t *handle_goxel_get_render_info(const json_rpc_request_t *request)
{
    if (!g_render_manager) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Render manager not available",
                                             NULL, &request->id);
    }
    
    // Get file path parameter
    const char *file_path = get_string_param(&request->params, 0, "path");
    if (!file_path) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify path parameter",
                                             NULL, &request->id);
    }
    
    LOG_D("Getting render info for path: %s", file_path);
    
    // Get render info from manager
    render_info_t *info = NULL;
    render_manager_error_t rm_result = render_manager_get_render_info(g_render_manager, file_path, &info);
    
    if (rm_result != RENDER_MGR_SUCCESS || !info) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Render not found: %s", 
                 render_manager_error_string(rm_result));
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 20,
                                             error_msg, NULL, &request->id);
    }
    
    // Build response with render information
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    
    json_value *render_obj = json_object_new(8);
    json_object_push(render_obj, "path", json_string_new(info->file_path));
    json_object_push(render_obj, "size", json_integer_new(info->file_size));
    json_object_push(render_obj, "format", json_string_new(info->format));
    
    json_value *dimensions_obj = json_object_new(2);
    json_object_push(dimensions_obj, "width", json_integer_new(info->width));
    json_object_push(dimensions_obj, "height", json_integer_new(info->height));
    json_object_push(render_obj, "dimensions", dimensions_obj);
    
    json_object_push(render_obj, "checksum", json_string_new(info->checksum ? info->checksum : "unavailable"));
    json_object_push(render_obj, "created_at", json_integer_new(info->created_at));
    json_object_push(render_obj, "expires_at", json_integer_new(info->expires_at));
    json_object_push(render_obj, "session_id", json_string_new(info->session_id ? info->session_id : "unknown"));
    
    json_object_push(result_obj, "render", render_obj);
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_cleanup_render(const json_rpc_request_t *request)
{
    if (!g_render_manager) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Render manager not available",
                                             NULL, &request->id);
    }
    
    // Get file path parameter
    const char *file_path = get_string_param(&request->params, 0, "path");
    if (!file_path) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Must specify path parameter",
                                             NULL, &request->id);
    }
    
    LOG_D("Cleaning up render: %s", file_path);
    
    // Remove render from manager
    render_manager_error_t rm_result = render_manager_remove_render(g_render_manager, file_path);
    
    if (rm_result != RENDER_MGR_SUCCESS) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to cleanup render: %s", 
                 render_manager_error_string(rm_result));
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 21,
                                             error_msg, NULL, &request->id);
    }
    
    // Build response
    json_value *result_obj = json_object_new(2);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "message", json_string_new("Render cleaned up successfully"));
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

static json_rpc_response_t *handle_goxel_list_renders(const json_rpc_request_t *request)
{
    if (!g_render_manager) {
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             "Render manager not available",
                                             NULL, &request->id);
    }
    
    LOG_D("Listing all active renders");
    
    // Get list of renders from manager
    render_info_t **renders = NULL;
    int count = 0;
    render_manager_error_t rm_result = render_manager_list_renders(g_render_manager, &renders, &count);
    
    if (rm_result != RENDER_MGR_SUCCESS) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to list renders: %s", 
                 render_manager_error_string(rm_result));
        return json_rpc_create_response_error(JSON_RPC_INTERNAL_ERROR,
                                             error_msg, NULL, &request->id);
    }
    
    // Build response with renders array
    json_value *result_obj = json_object_new(3);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "count", json_integer_new(count));
    
    json_value *renders_array = json_array_new(count);
    
    for (int i = 0; i < count; i++) {
        render_info_t *info = renders[i];
        if (!info) continue;
        
        json_value *render_obj = json_object_new(8);
        json_object_push(render_obj, "path", json_string_new(info->file_path));
        json_object_push(render_obj, "size", json_integer_new(info->file_size));
        json_object_push(render_obj, "format", json_string_new(info->format));
        
        json_value *dimensions_obj = json_object_new(2);
        json_object_push(dimensions_obj, "width", json_integer_new(info->width));
        json_object_push(dimensions_obj, "height", json_integer_new(info->height));
        json_object_push(render_obj, "dimensions", dimensions_obj);
        
        json_object_push(render_obj, "checksum", json_string_new(info->checksum ? info->checksum : "unavailable"));
        json_object_push(render_obj, "created_at", json_integer_new(info->created_at));
        json_object_push(render_obj, "expires_at", json_integer_new(info->expires_at));
        json_object_push(render_obj, "session_id", json_string_new(info->session_id ? info->session_id : "unknown"));
        
        json_array_push(renders_array, render_obj);
    }
    
    json_object_push(result_obj, "renders", renders_array);
    
    // Free the array (but not the render_info_t structs - they're managed by render_manager)
    if (renders) {
        free(renders);
    }
    
    return json_rpc_create_response_result(result_obj, &request->id);
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

json_rpc_result_t json_rpc_init_goxel_context(void)
{
    if (g_goxel_context) {
        LOG_W("Goxel context already initialized");
        return JSON_RPC_SUCCESS;
    }
    
    g_goxel_context = goxel_core_create_context();
    if (!g_goxel_context) {
        LOG_E("Failed to create Goxel context");
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    int result = goxel_core_init(g_goxel_context);
    if (result != 0) {
        LOG_E("Failed to initialize Goxel context: %d", result);
        goxel_core_destroy_context(g_goxel_context);
        g_goxel_context = NULL;
        return JSON_RPC_ERROR_UNKNOWN;
    }
    
    // Initialize render manager for file transfer architecture
    g_render_manager = render_manager_create(NULL, 0, 0);  // Use default settings
    if (!g_render_manager) {
        LOG_W("Failed to initialize render manager, file transfer mode will be unavailable");
    } else {
        LOG_I("Render manager initialized successfully");
        
        // Start background cleanup thread
        const char *env_cleanup_interval = getenv(RENDER_MANAGER_ENV_CLEANUP_INTERVAL);
        int cleanup_interval = env_cleanup_interval ? atoi(env_cleanup_interval) : 300; // Default 5 minutes
        
        g_render_manager->cleanup_thread = render_manager_start_cleanup_thread(
            g_render_manager, cleanup_interval);
        
        if (g_render_manager->cleanup_thread) {
            LOG_I("Render cleanup thread started (interval: %d seconds)", cleanup_interval);
        } else {
            LOG_W("Failed to start render cleanup thread, manual cleanup may be required");
        }
    }
    
    LOG_I("Goxel context initialized successfully");
    return JSON_RPC_SUCCESS;
}

void json_rpc_cleanup_goxel_context(void)
{
    // Clean up render manager
    if (g_render_manager) {
        // Stop cleanup thread first
        if (g_render_manager->cleanup_thread) {
            render_manager_stop_cleanup_thread(g_render_manager->cleanup_thread);
            g_render_manager->cleanup_thread = NULL;
            LOG_I("Render cleanup thread stopped");
        }
        
        render_manager_destroy(g_render_manager, true);  // Clean up files on shutdown
        g_render_manager = NULL;
        LOG_I("Render manager cleaned up");
    }
    
    if (g_goxel_context) {
        goxel_core_shutdown(g_goxel_context);
        goxel_core_destroy_context(g_goxel_context);
        g_goxel_context = NULL;
        LOG_I("Goxel context cleaned up");
    }
}

json_rpc_response_t *json_rpc_handle_method(const json_rpc_request_t *request)
{
    if (!request || !request->method) {
        json_rpc_id_t null_id;
        json_rpc_create_id_null(&null_id);
        return json_rpc_create_response_error(JSON_RPC_INVALID_REQUEST,
                                             "Invalid request structure",
                                             NULL, &null_id);
    }
    
    // Try test methods first (includes echo, version, status)
    json_rpc_response_t *response = handle_test_method(request->method, request);
    if (response) {
        LOG_D("Handling test method: %s", request->method);
        return response;
    }
    
    // Find the method handler in main registry
    for (size_t i = 0; i < g_method_registry_size; i++) {
        if (strcmp(request->method, g_method_registry[i].name) == 0) {
            LOG_D("Handling method: %s", request->method);
            return g_method_registry[i].handler(request);
        }
    }
    
    // Method not found
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Method not found: %s", request->method);
    return json_rpc_create_response_error(JSON_RPC_METHOD_NOT_FOUND,
                                         error_msg, NULL, &request->id);
}

// Helper function to serialize JSON value to string
static char *json_value_to_string(json_value *value)
{
    if (!value) return NULL;
    
    size_t size = json_measure(value);
    char *str = malloc(size);
    if (!str) return NULL;
    
    json_serialize(str, value);
    return str;
}

json_rpc_result_t json_rpc_handle_batch(const char *json_str, char **response_str)
{
    if (!json_str || !response_str) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    *response_str = NULL;
    
    // Parse the JSON
    json_settings settings = { 0 };
    char error[json_error_max];
    json_value *root = json_parse_ex(&settings, json_str, strlen(json_str), error);
    
    if (!root) {
        // Parse error - return single error response
        json_rpc_response_t *error_resp = json_rpc_create_response_error(
            JSON_RPC_PARSE_ERROR, "Parse error", NULL, NULL);
        json_rpc_serialize_response(error_resp, response_str);
        json_rpc_free_response(error_resp);
        return JSON_RPC_SUCCESS;
    }
    
    json_value *responses = NULL;
    bool is_batch = (root->type == json_array);
    
    if (is_batch) {
        // Batch request - process each request
        responses = json_array_new(root->u.array.length);
        
        for (unsigned int i = 0; i < root->u.array.length; i++) {
            json_value *req_val = root->u.array.values[i];
            char *req_str = json_value_to_string(req_val);
            
            if (!req_str) continue;
            
            json_rpc_request_t *request = NULL;
            json_rpc_result_t parse_result = json_rpc_parse_request(req_str, &request);
            
            if (parse_result == JSON_RPC_SUCCESS && request) {
                // Process the request
                json_rpc_response_t *response = json_rpc_handle_method(request);
                
                if (response) {
                    // Serialize response to JSON value
                    char *resp_str = NULL;
                    if (json_rpc_serialize_response(response, &resp_str) == JSON_RPC_SUCCESS && resp_str) {
                        json_value *resp_val = json_parse(resp_str, strlen(resp_str));
                        if (resp_val) {
                            json_array_push(responses, resp_val);
                        }
                        free(resp_str);
                    }
                    json_rpc_free_response(response);
                }
                
                json_rpc_free_request(request);
            } else {
                // Invalid request - add error response
                json_rpc_response_t *error_resp = json_rpc_create_response_error(
                    JSON_RPC_INVALID_REQUEST, "Invalid Request", NULL, NULL);
                char *error_str = NULL;
                if (json_rpc_serialize_response(error_resp, &error_str) == JSON_RPC_SUCCESS && error_str) {
                    json_value *error_val = json_parse(error_str, strlen(error_str));
                    if (error_val) {
                        json_array_push(responses, error_val);
                    }
                    free(error_str);
                }
                json_rpc_free_response(error_resp);
            }
            
            free(req_str);
        }
        
        // Serialize the response array
        *response_str = json_value_to_string(responses);
        json_value_free(responses);
    } else {
        // Single request
        json_rpc_request_t *request = NULL;
        json_rpc_result_t parse_result = json_rpc_parse_request(json_str, &request);
        
        if (parse_result == JSON_RPC_SUCCESS && request) {
            json_rpc_response_t *response = json_rpc_handle_method(request);
            if (response) {
                json_rpc_serialize_response(response, response_str);
                json_rpc_free_response(response);
            }
            json_rpc_free_request(request);
        } else {
            // Invalid request
            json_rpc_response_t *error_resp = json_rpc_create_response_error(
                JSON_RPC_INVALID_REQUEST, "Invalid Request", NULL, NULL);
            json_rpc_serialize_response(error_resp, response_str);
            json_rpc_free_response(error_resp);
        }
    }
    
    json_value_free(root);
    return JSON_RPC_SUCCESS;
}

int json_rpc_list_methods(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    size_t offset = 0;
    
    // List test methods first
    size_t test_count = 0;
    const test_method_entry_t *test_methods = get_test_methods(&test_count);
    
    for (size_t i = 0; i < test_count; i++) {
        int written = snprintf(buffer + offset, buffer_size - offset,
                              "%s - %s\n",
                              test_methods[i].name,
                              test_methods[i].description);
        
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            return -1; // Buffer too small
        }
        
        offset += written;
    }
    
    // List main registry methods
    for (size_t i = 0; i < g_method_registry_size; i++) {
        int written = snprintf(buffer + offset, buffer_size - offset,
                              "%s - %s\n",
                              g_method_registry[i].name,
                              g_method_registry[i].description);
        
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            return -1; // Buffer too small
        }
        
        offset += written;
    }
    
    return 0;
}