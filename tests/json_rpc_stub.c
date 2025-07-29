/* Minimal JSON-RPC stub for MCP handler testing */
#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_goxel_initialized = false;

json_rpc_result_t json_rpc_init_goxel_context(void) {
    g_goxel_initialized = true;
    return JSON_RPC_SUCCESS;
}

void json_rpc_cleanup_goxel_context(void) {
    g_goxel_initialized = false;
}

bool json_rpc_is_goxel_initialized(void) {
    return g_goxel_initialized;
}

json_rpc_response_t *json_rpc_handle_method(const json_rpc_request_t *request) {
    if (!request) return NULL;
    
    json_rpc_id_t id = request->id;
    
    // Simple stub responses for testing
    if (strcmp(request->method, "ping") == 0) {
        json_value *result = json_string_new("pong");
        return json_rpc_create_response_result(result, &id);
    } else if (strcmp(request->method, "version") == 0) {
        json_value *result = json_object_new(0);
        json_object_push(result, "version", json_string_new("14.0.0-test"));
        return json_rpc_create_response_result(result, &id);
    } else if (strstr(request->method, "goxel.") == request->method) {
        // Goxel methods - return simple success
        json_value *result = json_object_new(0);
        json_object_push(result, "status", json_string_new("success"));
        return json_rpc_create_response_result(result, &id);
    } else {
        // Unknown method
        return json_rpc_create_response_error(
            JSON_RPC_METHOD_NOT_FOUND, "Method not found", NULL, &id);
    }
}

// Additional JSON-RPC functions needed by MCP handler
json_rpc_result_t json_rpc_create_id_number(int64_t number, json_rpc_id_t *id) {
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    id->type = JSON_RPC_ID_NUMBER;
    id->value.number = number;
    return JSON_RPC_SUCCESS;
}

void json_rpc_free_id(json_rpc_id_t *id) {
    if (!id) return;
    if (id->type == JSON_RPC_ID_STRING) {
        free(id->value.string);
    }
}

json_rpc_request_t *json_rpc_create_request_object(const char *method,
                                                  json_value *params_object,
                                                  const json_rpc_id_t *id) {
    json_rpc_request_t *req = calloc(1, sizeof(json_rpc_request_t));
    if (!req) return NULL;
    
    req->method = strdup(method);
    req->params.type = JSON_RPC_PARAMS_OBJECT;
    req->params.data = params_object;
    req->id = *id;
    req->is_notification = (id->type == JSON_RPC_ID_NULL);
    
    return req;
}

void json_rpc_free_request(json_rpc_request_t *request) {
    if (!request) return;
    free(request->method);
    if (request->params.data) json_value_free(request->params.data);
    json_rpc_free_id(&request->id);
    free(request);
}

void json_rpc_free_response(json_rpc_response_t *response) {
    if (!response) return;
    if (response->result) json_value_free(response->result);
    if (response->error.message) free(response->error.message);
    if (response->error.data) json_value_free(response->error.data);
    json_rpc_free_id(&response->id);
    free(response);
}

json_rpc_response_t *json_rpc_create_response_result(json_value *result,
                                                    const json_rpc_id_t *id) {
    json_rpc_response_t *resp = calloc(1, sizeof(json_rpc_response_t));
    if (!resp) return NULL;
    
    resp->id = *id;
    resp->has_result = true;
    resp->has_error = false;
    resp->result = result;
    
    return resp;
}

json_rpc_response_t *json_rpc_create_response_error(int32_t error_code,
                                                   const char *error_message,
                                                   json_value *error_data,
                                                   const json_rpc_id_t *id) {
    json_rpc_response_t *resp = calloc(1, sizeof(json_rpc_response_t));
    if (!resp) return NULL;
    
    resp->id = *id;
    resp->has_result = false;
    resp->has_error = true;
    resp->error.code = error_code;
    resp->error.message = error_message ? strdup(error_message) : NULL;
    resp->error.data = error_data;
    
    return resp;
}