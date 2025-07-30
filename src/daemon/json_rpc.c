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
#include "../utils/json.h"
#include "../../ext_src/json/json-builder.h"
#include "../log.h"
#include "../core/goxel_core.h"
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

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
        params->data = json_params;
        return JSON_RPC_SUCCESS;
    }
    
    if (json_params->type == json_object) {
        params->type = JSON_RPC_PARAMS_OBJECT;
        params->data = json_params;
        return JSON_RPC_SUCCESS;
    }
    
    return JSON_RPC_ERROR_INVALID_JSON;
}

static json_value *create_params_json(const json_rpc_params_t *params)
{
    if (!params || params->type == JSON_RPC_PARAMS_NONE) {
        return NULL; // Omit params field for no parameters
    }
    
    return params->data; // Return the stored JSON value
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
    
    // Clone parameters to avoid use-after-free
    if (req->params.data) {
        json_value *cloned_params = clone_json_value(req->params.data);
        if (!cloned_params) {
            result = JSON_RPC_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
        req->params.data = cloned_params;
    }
    
    *request = req;
    json_value_free(root); // Now safe to free root since we cloned the needed parts
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
            json_object_push(root, "result", response->result);
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
            json_object_push(error_obj, "data", response->error.data);
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
    
    SAFE_FREE(request->method);
    
    // Free parameters data if owned
    if (request->params.data) {
        json_value_free(request->params.data);
    }
    
    json_rpc_free_id(&request->id);
    
    free(request);
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
    {"goxel.get_status", handle_goxel_get_status, "Get current Goxel status and info"}
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

// Helper functions removed - not used in current implementation

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
    
    // Parameters: name (optional), width (optional, default 64), height (optional, default 64), depth (optional, default 64)
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
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 1,
                                             error_msg, NULL, &request->id);
    }
    
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
    
    // Parameters: x, y, z, r, g, b, a (optional, default 255), layer_id (optional, default 0)
    int x = get_int_param(&request->params, 0, "x");
    int y = get_int_param(&request->params, 1, "y");
    int z = get_int_param(&request->params, 2, "z");
    int r = get_int_param(&request->params, 3, "r");
    int g = get_int_param(&request->params, 4, "g");
    int b = get_int_param(&request->params, 5, "b");
    int a = get_int_param(&request->params, 6, "a");
    int layer_id = get_int_param(&request->params, 7, "layer_id");
    
    // Validate color values
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return json_rpc_create_response_error(JSON_RPC_INVALID_PARAMS,
                                             "Invalid color values (must be 0-255)",
                                             NULL, &request->id);
    }
    
    if (a <= 0) a = 255; // Default alpha
    if (a > 255) a = 255;
    
    uint8_t rgba[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    LOG_D("Adding voxel at (%d, %d, %d) with color (%d, %d, %d, %d) to layer %d", 
          x, y, z, r, g, b, a, layer_id);
    
    int result = goxel_core_add_voxel(g_goxel_context, x, y, z, (uint8_t*)rgba, layer_id);
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
    json_array_push(color_obj, json_integer_new(r));
    json_array_push(color_obj, json_integer_new(g));
    json_array_push(color_obj, json_integer_new(b));
    json_array_push(color_obj, json_integer_new(a));
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
    
    // Parameters: width, height, format (optional), output_path (optional)
    int width = get_int_param(&request->params, 0, "width");
    int height = get_int_param(&request->params, 1, "height");
    const char *format = get_string_param(&request->params, 2, "format");
    const char *output_path = get_string_param(&request->params, 3, "output_path");
    
    if (width <= 0) width = 512;
    if (height <= 0) height = 512;
    if (!format) format = "png";
    
    LOG_D("Rendering scene %dx%d format %s to %s", width, height, format, output_path ? output_path : "memory");
    
    int result;
    if (output_path) {
        // Render to file
        result = goxel_core_render_to_file(g_goxel_context, output_path, width, height, format, 90, NULL);
    } else {
        // Render to buffer - placeholder implementation
        result = -1; // Not implemented yet
    }
    if (result != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to render scene: error code %d", result);
        return json_rpc_create_response_error(JSON_RPC_APPLICATION_ERROR - 15,
                                             error_msg, NULL, &request->id);
    }
    
    json_value *result_obj = json_object_new(4);
    json_object_push(result_obj, "success", json_boolean_new(1));
    json_object_push(result_obj, "width", json_integer_new(width));
    json_object_push(result_obj, "height", json_integer_new(height));
    json_object_push(result_obj, "format", json_string_new(format));
    
    if (output_path) {
        json_object_push(result_obj, "output_path", json_string_new(output_path));
    } else {
        // TODO: Implement buffer rendering using goxel_core_render_to_buffer
        json_object_push(result_obj, "note", json_string_new("Buffer rendering not yet implemented"));
    }
    
    return json_rpc_create_response_result(result_obj, &request->id);
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
    
    LOG_I("Goxel context initialized successfully");
    return JSON_RPC_SUCCESS;
}

void json_rpc_cleanup_goxel_context(void)
{
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