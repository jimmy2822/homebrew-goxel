/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#include "compatibility_proxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

// ============================================================================
// INTERNAL CONSTANTS
// ============================================================================

#define COMPAT_MAX_MESSAGE_SIZE 65536
#define COMPAT_MAX_CLIENTS 100
#define COMPAT_TRANSLATION_CACHE_SIZE 1000
#define COMPAT_PROTOCOL_DETECTION_BUFFER 1024

// Protocol detection patterns
#define LEGACY_MCP_PATTERN "{\"tool\""
#define LEGACY_TYPESCRIPT_PATTERN "{\"method\":\"add_voxel\""
#define LEGACY_JSONRPC_PATTERN "{\"jsonrpc\":\"2.0\",\"method\":\"goxel."
#define NATIVE_MCP_PATTERN "{\"tool\":\"goxel_"
#define NATIVE_JSONRPC_PATTERN "{\"jsonrpc\":\"2.0\",\"method\":\"goxel."

// ============================================================================
// TRANSLATION MAPPING TABLES
// ============================================================================

/**
 * Method mappings for TypeScript client compatibility
 */
static const compat_translation_mapping_t g_typescript_mappings[] = {
    {
        "add_voxel", "goxel.add_voxels",
        compat_transform_ts_add_voxel_params,
        compat_transform_response_to_legacy_ts,
        "Add voxel operation (TypeScript client legacy)",
        true
    },
    {
        "remove_voxel", "goxel.remove_voxel", 
        compat_transform_ts_add_voxel_params,  // Same parameter structure
        compat_transform_response_to_legacy_ts,
        "Remove voxel operation (TypeScript client legacy)",
        true
    },
    {
        "load_project", "goxel.open_file",
        compat_transform_ts_load_project_params,
        compat_transform_response_to_legacy_ts,
        "Load project operation (TypeScript client legacy)",
        true
    },
    {
        "export_model", "goxel.export_file",
        compat_transform_ts_export_model_params,
        compat_transform_response_to_legacy_ts,
        "Export model operation (TypeScript client legacy)",
        true
    },
    {
        "create_project", "goxel.create_project",
        NULL, // No parameter transformation needed
        compat_transform_response_to_legacy_ts,
        "Create project operation (TypeScript client legacy)",
        true
    },
    {
        "list_layers", "goxel.list_layers",
        NULL,
        compat_transform_response_to_legacy_ts,
        "List layers operation (TypeScript client legacy)",
        true
    },
};

/**
 * Method mappings for legacy MCP server compatibility
 */
static const compat_translation_mapping_t g_legacy_mcp_mappings[] = {
    {
        "goxel_create_project", "goxel.create_project",
        NULL,
        compat_transform_response_to_legacy_mcp,
        "Create project (Legacy MCP)",
        true
    },
    {
        "goxel_add_voxels", "goxel.add_voxels",
        compat_transform_legacy_mcp_params,
        compat_transform_response_to_legacy_mcp,
        "Add voxels (Legacy MCP)",
        true
    },
    {
        "goxel_open_file", "goxel.open_file",
        compat_transform_legacy_mcp_params, 
        compat_transform_response_to_legacy_mcp,
        "Open file (Legacy MCP)",
        true
    },
    {
        "goxel_export_file", "goxel.export_file",
        compat_transform_legacy_mcp_params,
        compat_transform_response_to_legacy_mcp,
        "Export file (Legacy MCP)",
        true
    },
};

static const size_t g_typescript_mappings_count = 
    sizeof(g_typescript_mappings) / sizeof(g_typescript_mappings[0]);
static const size_t g_legacy_mcp_mappings_count = 
    sizeof(g_legacy_mcp_mappings) / sizeof(g_legacy_mcp_mappings[0]);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Helper to get current time in microseconds
 */
static uint64_t get_time_microseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * Helper to get JSON object member safely
 */
static json_value *json_object_get_safe(const json_value *obj, const char *key)
{
    if (!obj || obj->type != json_object || !key) return NULL;
    
    for (unsigned int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, key) == 0) {
            return obj->u.object.values[i].value;
        }
    }
    return NULL;
}

/**
 * Helper to clone JSON value safely
 */
static json_value *json_value_clone_safe(json_value *src)
{
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
                    json_value_clone_safe(src->u.object.values[i].value));
            }
            return clone;
        }
        case json_array: {
            json_value *clone = json_array_new(src->u.array.length);
            for (unsigned int i = 0; i < src->u.array.length; i++) {
                clone->u.array.values[i] = json_value_clone_safe(src->u.array.values[i]);
            }
            return clone;
        }
        default:
            return NULL;
    }
}

// ============================================================================
// PROTOCOL DETECTION IMPLEMENTATION
// ============================================================================

json_rpc_result_t compat_detect_protocol(
    const char *data,
    size_t length,
    compat_protocol_detection_t *detection)
{
    if (!data || !detection || length == 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize detection result
    detection->type = COMPAT_PROTOCOL_UNKNOWN;
    detection->is_legacy = false;
    detection->version_hint = NULL;
    detection->confidence = 0.0;
    
    // Ensure we have a null-terminated string for pattern matching
    char *buffer = malloc(length + 1);
    if (!buffer) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    
    memcpy(buffer, data, length);
    buffer[length] = '\0';
    
    // Try to parse as JSON first
    json_value *json = json_parse(buffer, length);
    if (!json) {
        free(buffer);
        return JSON_RPC_SUCCESS; // Unknown but valid result
    }
    
    if (json->type != json_object) {
        json_value_free(json);
        free(buffer);
        return JSON_RPC_SUCCESS;
    }
    
    // Check for MCP tool pattern
    json_value *tool = json_object_get_safe(json, "tool");
    if (tool && tool->type == json_string) {
        const char *tool_name = tool->u.string.ptr;
        
        if (strncmp(tool_name, "goxel_", 6) == 0) {
            // Legacy MCP format (tools prefixed with goxel_)
            detection->type = COMPAT_PROTOCOL_LEGACY_MCP;
            detection->is_legacy = true;
            detection->confidence = 0.8;
        } else {
            // Native MCP format (new tool names without goxel_ prefix)
            detection->type = COMPAT_PROTOCOL_NATIVE_MCP;
            detection->is_legacy = false;
            detection->confidence = 0.9;
        }
        
        json_value_free(json);
        free(buffer);
        return JSON_RPC_SUCCESS;
    }
    
    // Check for JSON-RPC pattern
    json_value *jsonrpc = json_object_get_safe(json, "jsonrpc");
    json_value *method = json_object_get_safe(json, "method");
    
    if (jsonrpc && method && 
        jsonrpc->type == json_string && method->type == json_string) {
        
        const char *method_name = method->u.string.ptr;
        
        if (strncmp(method_name, "goxel.", 6) == 0) {
            // Native JSON-RPC format
            detection->type = COMPAT_PROTOCOL_NATIVE_JSONRPC;
            detection->is_legacy = false;
            detection->confidence = 0.9;
        } else if (strcmp(method_name, "add_voxel") == 0 ||
                   strcmp(method_name, "load_project") == 0 ||
                   strcmp(method_name, "export_model") == 0) {
            // Legacy TypeScript client format
            detection->type = COMPAT_PROTOCOL_LEGACY_TYPESCRIPT;
            detection->is_legacy = true;
            detection->confidence = 0.8;
        } else {
            // Legacy JSON-RPC format
            detection->type = COMPAT_PROTOCOL_LEGACY_JSONRPC;
            detection->is_legacy = true;
            detection->confidence = 0.6;
        }
    }
    
    json_value_free(json);
    free(buffer);
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_detect_client_type(
    int client_fd,
    const char *initial_data,
    size_t data_length,
    compat_client_context_t *context)
{
    if (!initial_data || !context || data_length == 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize client context
    memset(context, 0, sizeof(compat_client_context_t));
    context->client_fd = client_fd;
    context->daemon_fd = -1;
    context->first_request = time(NULL);
    
    // Generate client ID from file descriptor and timestamp
    snprintf(context->client_id, sizeof(context->client_id), 
             "client_%d_%ld", client_fd, context->first_request);
    
    // Detect protocol
    compat_protocol_detection_t detection;
    json_rpc_result_t result = compat_detect_protocol(
        initial_data, data_length, &detection);
    
    if (result != JSON_RPC_SUCCESS) {
        return result;
    }
    
    context->detected_protocol = detection.type;
    context->is_legacy_client = detection.is_legacy;
    
    // Set user agent based on detected protocol
    switch (detection.type) {
        case COMPAT_PROTOCOL_LEGACY_MCP:
            strcpy(context->user_agent, "Legacy-MCP-Server/1.0");
            break;
        case COMPAT_PROTOCOL_LEGACY_TYPESCRIPT:
            strcpy(context->user_agent, "TypeScript-Client/14.0-legacy");
            break;
        case COMPAT_PROTOCOL_LEGACY_JSONRPC:
            strcpy(context->user_agent, "JSON-RPC-Client/legacy");
            break;
        case COMPAT_PROTOCOL_NATIVE_MCP:
            strcpy(context->user_agent, "Native-MCP/14.0");
            break;
        case COMPAT_PROTOCOL_NATIVE_JSONRPC:
            strcpy(context->user_agent, "Native-JSON-RPC/14.0");
            break;
        default:
            strcpy(context->user_agent, "Unknown-Client");
            break;
    }
    
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// TRANSLATION FUNCTIONS
// ============================================================================

/**
 * Find translation mapping for legacy method
 */
static const compat_translation_mapping_t *find_translation_mapping(
    const char *legacy_method,
    compat_protocol_type_t protocol_type)
{
    if (!legacy_method) return NULL;
    
    const compat_translation_mapping_t *mappings = NULL;
    size_t count = 0;
    
    switch (protocol_type) {
        case COMPAT_PROTOCOL_LEGACY_TYPESCRIPT:
            mappings = g_typescript_mappings;
            count = g_typescript_mappings_count;
            break;
        case COMPAT_PROTOCOL_LEGACY_MCP:
            mappings = g_legacy_mcp_mappings;
            count = g_legacy_mcp_mappings_count;
            break;
        default:
            return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (strcmp(mappings[i].legacy_method, legacy_method) == 0) {
            return &mappings[i];
        }
    }
    
    return NULL;
}

json_rpc_result_t compat_translate_request(
    const json_value *legacy_request,
    compat_protocol_type_t protocol_type,
    json_value **new_request,
    compat_client_context_t *context)
{
    if (!legacy_request || !new_request || !context) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (legacy_request->type != json_object) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Extract method name based on protocol type
    const char *method_name = NULL;
    json_value *params = NULL;
    json_value *id = NULL;
    
    if (protocol_type == COMPAT_PROTOCOL_LEGACY_MCP) {
        // MCP format: {"tool": "method_name", "arguments": {...}}
        json_value *tool = json_object_get_safe(legacy_request, "tool");
        json_value *arguments = json_object_get_safe(legacy_request, "arguments");
        
        if (!tool || tool->type != json_string) {
            return JSON_RPC_ERROR_INVALID_PARAMETER;
        }
        
        method_name = tool->u.string.ptr;
        params = arguments;
    } else {
        // JSON-RPC format: {"jsonrpc": "2.0", "method": "...", "params": {...}, "id": ...}
        json_value *method = json_object_get_safe(legacy_request, "method");
        params = json_object_get_safe(legacy_request, "params");
        id = json_object_get_safe(legacy_request, "id");
        
        if (!method || method->type != json_string) {
            return JSON_RPC_ERROR_INVALID_PARAMETER;
        }
        
        method_name = method->u.string.ptr;
    }
    
    // Find translation mapping
    const compat_translation_mapping_t *mapping = 
        find_translation_mapping(method_name, protocol_type);
    
    if (!mapping) {
        // No translation needed - pass through with protocol conversion
        *new_request = json_value_clone_safe((json_value*)legacy_request);
        return JSON_RPC_SUCCESS;
    }
    
    // Apply parameter transformation if needed
    json_value *transformed_params = NULL;
    if (mapping->param_transformer && params) {
        json_rpc_result_t result = mapping->param_transformer(
            params, &transformed_params, context);
        if (result != JSON_RPC_SUCCESS) {
            return result;
        }
    } else if (params) {
        transformed_params = json_value_clone_safe(params);
    }
    
    // Create new request in unified format
    json_value *request = json_object_new(0);
    json_object_push(request, "jsonrpc", json_string_new("2.0"));
    json_object_push(request, "method", json_string_new(mapping->new_method));
    
    if (transformed_params) {
        json_object_push(request, "params", transformed_params);
    }
    
    if (id) {
        json_object_push(request, "id", json_value_clone_safe(id));
    } else {
        // Generate ID for MCP requests
        static int request_id_counter = 1;
        json_object_push(request, "id", json_integer_new(request_id_counter++));
    }
    
    *new_request = request;
    context->requests_translated++;
    
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_translate_response(
    const json_value *new_response,
    compat_protocol_type_t protocol_type,
    json_value **legacy_response,
    compat_client_context_t *context)
{
    if (!new_response || !legacy_response || !context) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // For native protocols, pass through unchanged
    if (protocol_type == COMPAT_PROTOCOL_NATIVE_MCP ||
        protocol_type == COMPAT_PROTOCOL_NATIVE_JSONRPC) {
        *legacy_response = json_value_clone_safe((json_value*)new_response);
        return JSON_RPC_SUCCESS;
    }
    
    // Apply protocol-specific response transformation
    if (protocol_type == COMPAT_PROTOCOL_LEGACY_MCP) {
        return compat_transform_response_to_legacy_mcp(
            new_response, legacy_response, context);
    } else {
        return compat_transform_response_to_legacy_ts(
            new_response, legacy_response, context);
    }
}

// ============================================================================
// BUILT-IN PARAMETER TRANSFORMERS
// ============================================================================

json_rpc_result_t compat_transform_ts_add_voxel_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context)
{
    if (!legacy_params || !new_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (legacy_params->type != json_object) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    json_value *params = json_object_new(0);
    
    // Transform flat parameters to structured format
    // Old: {x: 10, y: 20, z: 30, rgba: [255,0,0,255]}
    // New: {position: {x,y,z}, color: {r,g,b,a}, brush: {...}}
    
    json_value *x = json_object_get_safe(legacy_params, "x");
    json_value *y = json_object_get_safe(legacy_params, "y");
    json_value *z = json_object_get_safe(legacy_params, "z");
    
    if (x && y && z) {
        json_value *position = json_object_new(0);
        json_object_push(position, "x", json_value_clone_safe(x));
        json_object_push(position, "y", json_value_clone_safe(y));
        json_object_push(position, "z", json_value_clone_safe(z));
        json_object_push(params, "position", position);
    }
    
    json_value *rgba = json_object_get_safe(legacy_params, "rgba");
    if (rgba && rgba->type == json_array && rgba->u.array.length >= 4) {
        json_value *color = json_object_new(0);
        json_object_push(color, "r", json_value_clone_safe(rgba->u.array.values[0]));
        json_object_push(color, "g", json_value_clone_safe(rgba->u.array.values[1]));
        json_object_push(color, "b", json_value_clone_safe(rgba->u.array.values[2]));
        json_object_push(color, "a", json_value_clone_safe(rgba->u.array.values[3]));
        json_object_push(params, "color", color);
    }
    
    // Add default brush if not specified
    json_value *brush = json_object_new(0);
    json_object_push(brush, "shape", json_string_new("cube"));
    json_object_push(brush, "size", json_integer_new(1));
    json_object_push(params, "brush", brush);
    
    *new_params = params;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_transform_ts_load_project_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context)
{
    if (!legacy_params || !new_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Simple parameter rename: path -> path (no change needed)
    *new_params = json_value_clone_safe((json_value*)legacy_params);
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_transform_ts_export_model_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context)
{
    if (!legacy_params || !new_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Parameter rename: output_path -> path, add format if not specified
    json_value *params = json_object_new(0);
    
    json_value *output_path = json_object_get_safe(legacy_params, "output_path");
    if (output_path) {
        json_object_push(params, "path", json_value_clone_safe(output_path));
    } else {
        json_value *path = json_object_get_safe(legacy_params, "path");
        if (path) {
            json_object_push(params, "path", json_value_clone_safe(path));
        }
    }
    
    json_value *format = json_object_get_safe(legacy_params, "format");
    if (format) {
        json_object_push(params, "format", json_value_clone_safe(format));
    } else {
        json_object_push(params, "format", json_string_new("obj"));
    }
    
    *new_params = params;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_transform_legacy_mcp_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context)
{
    if (!legacy_params || !new_params) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Legacy MCP parameters are typically already compatible
    *new_params = json_value_clone_safe((json_value*)legacy_params);
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// RESPONSE TRANSFORMERS
// ============================================================================

json_rpc_result_t compat_transform_response_to_legacy_mcp(
    const json_value *new_response,
    json_value **legacy_response,
    void *context)
{
    if (!new_response || !legacy_response) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Transform JSON-RPC response to MCP response format
    // JSON-RPC: {"jsonrpc": "2.0", "result": {...}, "id": 1}
    // MCP: {"success": true, "content": {...}}
    
    json_value *response = json_object_new(0);
    
    json_value *error = json_object_get_safe(new_response, "error");
    if (error) {
        // Error response
        json_object_push(response, "success", json_boolean_new(false));
        
        json_value *message = json_object_get_safe(error, "message");
        if (message) {
            json_object_push(response, "error_message", json_value_clone_safe(message));
        }
        
        json_value *code = json_object_get_safe(error, "code");
        if (code) {
            json_object_push(response, "error_code", json_value_clone_safe(code));
        }
    } else {
        // Success response
        json_object_push(response, "success", json_boolean_new(true));
        
        json_value *result = json_object_get_safe(new_response, "result");
        if (result) {
            json_object_push(response, "content", json_value_clone_safe(result));
        }
    }
    
    *legacy_response = response;
    return JSON_RPC_SUCCESS;
}

json_rpc_result_t compat_transform_response_to_legacy_ts(
    const json_value *new_response,
    json_value **legacy_response,
    void *context)
{
    if (!new_response || !legacy_response) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // TypeScript client expects standard JSON-RPC format, so minimal transformation
    *legacy_response = json_value_clone_safe((json_value*)new_response);
    return JSON_RPC_SUCCESS;
}

// ============================================================================
// UTILITY FUNCTION IMPLEMENTATIONS
// ============================================================================

void compat_get_default_config(compat_proxy_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(compat_proxy_config_t));
    
    strcpy(config->legacy_mcp_socket, "/tmp/mcp-server.sock");
    strcpy(config->legacy_daemon_socket, "/tmp/goxel-daemon.sock");
    strcpy(config->new_daemon_socket, "/tmp/goxel-mcp-daemon.sock");
    
    config->enable_deprecation_warnings = true;
    config->warning_frequency = 100;
    config->log_translation_stats = true;
    config->emulate_timing_behavior = false;
    
    config->max_concurrent_clients = COMPAT_MAX_CLIENTS;
    config->translation_cache_size = COMPAT_TRANSLATION_CACHE_SIZE;
    config->connection_timeout_ms = 5000;
    
    strcpy(config->log_file, "/tmp/goxel-compatibility.log");
    config->telemetry_enabled = false;
}

const char *compat_protocol_type_string(compat_protocol_type_t protocol_type)
{
    switch (protocol_type) {
        case COMPAT_PROTOCOL_UNKNOWN: return "Unknown";
        case COMPAT_PROTOCOL_LEGACY_MCP: return "Legacy-MCP";
        case COMPAT_PROTOCOL_LEGACY_TYPESCRIPT: return "Legacy-TypeScript";
        case COMPAT_PROTOCOL_LEGACY_JSONRPC: return "Legacy-JSON-RPC";
        case COMPAT_PROTOCOL_NATIVE_MCP: return "Native-MCP";
        case COMPAT_PROTOCOL_NATIVE_JSONRPC: return "Native-JSON-RPC";
        default: return "Invalid";
    }
}

bool compat_is_legacy_protocol(compat_protocol_type_t protocol_type)
{
    return protocol_type == COMPAT_PROTOCOL_LEGACY_MCP ||
           protocol_type == COMPAT_PROTOCOL_LEGACY_TYPESCRIPT ||
           protocol_type == COMPAT_PROTOCOL_LEGACY_JSONRPC;
}

json_rpc_result_t compat_validate_config(const compat_proxy_config_t *config)
{
    if (!config) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Check required socket paths
    if (strlen(config->legacy_mcp_socket) == 0 ||
        strlen(config->new_daemon_socket) == 0) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    // Check reasonable limits
    if (config->max_concurrent_clients == 0 ||
        config->max_concurrent_clients > 10000) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    if (config->connection_timeout_ms < 1000 ||
        config->connection_timeout_ms > 300000) {
        return JSON_RPC_ERROR_INVALID_PARAMETER;
    }
    
    return JSON_RPC_SUCCESS;
}