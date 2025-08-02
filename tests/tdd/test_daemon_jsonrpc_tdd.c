#include "tdd_framework.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char* data;
    size_t size;
} buffer_t;

typedef struct {
    char* method;
    char* params_json;
    int id;
} jsonrpc_request_t;

typedef struct {
    bool success;
    char* result_json;
    char* error_message;
    int id;
} jsonrpc_response_t;

jsonrpc_request_t* parse_jsonrpc_request(const char* json) {
    if (!json) return NULL;
    
    jsonrpc_request_t* req = malloc(sizeof(jsonrpc_request_t));
    if (!req) return NULL;
    
    req->method = NULL;
    req->params_json = NULL;
    req->id = -1;
    
    const char* method_start = strstr(json, "\"method\"");
    if (method_start) {
        method_start = strchr(method_start + 8, '\"');
        if (method_start) {
            method_start++;
            const char* method_end = strchr(method_start, '\"');
            if (method_end) {
                size_t len = method_end - method_start;
                req->method = malloc(len + 1);
                strncpy(req->method, method_start, len);
                req->method[len] = '\0';
            }
        }
    }
    
    const char* params_start = strstr(json, "\"params\"");
    if (params_start) {
        params_start = strchr(params_start + 8, ':');
        if (params_start) {
            params_start++;
            while (*params_start == ' ') params_start++;
            
            if (*params_start == '{') {
                const char* params_end = params_start + 1;
                int brace_count = 1;
                while (brace_count > 0 && *params_end) {
                    if (*params_end == '{') brace_count++;
                    else if (*params_end == '}') brace_count--;
                    params_end++;
                }
                
                if (brace_count == 0) {
                    size_t len = params_end - params_start;
                    req->params_json = malloc(len + 1);
                    strncpy(req->params_json, params_start, len);
                    req->params_json[len] = '\0';
                }
            }
        }
    }
    
    const char* id_start = strstr(json, "\"id\"");
    if (id_start) {
        id_start = strchr(id_start + 4, ':');
        if (id_start) {
            req->id = atoi(id_start + 1);
        }
    }
    
    return req;
}

void free_jsonrpc_request(jsonrpc_request_t* req) {
    if (req) {
        free(req->method);
        free(req->params_json);
        free(req);
    }
}

jsonrpc_response_t* create_success_response(int id, const char* result) {
    jsonrpc_response_t* resp = malloc(sizeof(jsonrpc_response_t));
    if (!resp) return NULL;
    
    resp->success = true;
    resp->id = id;
    resp->error_message = NULL;
    
    if (result) {
        resp->result_json = strdup(result);
    } else {
        resp->result_json = strdup("\"success\"");
    }
    
    return resp;
}

jsonrpc_response_t* create_error_response(int id, const char* error) {
    jsonrpc_response_t* resp = malloc(sizeof(jsonrpc_response_t));
    if (!resp) return NULL;
    
    resp->success = false;
    resp->id = id;
    resp->result_json = NULL;
    resp->error_message = strdup(error);
    
    return resp;
}

void free_jsonrpc_response(jsonrpc_response_t* resp) {
    if (resp) {
        free(resp->result_json);
        free(resp->error_message);
        free(resp);
    }
}

char* serialize_jsonrpc_response(jsonrpc_response_t* resp) {
    if (!resp) return NULL;
    
    char buffer[1024];
    if (resp->success) {
        snprintf(buffer, sizeof(buffer),
                 "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%d}",
                 resp->result_json, resp->id);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"%s\"},\"id\":%d}",
                 resp->error_message, resp->id);
    }
    
    return strdup(buffer);
}

jsonrpc_response_t* handle_create_project(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.create_project") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    return create_success_response(req->id, "{\"project_id\":\"test-123\"}");
}

jsonrpc_response_t* handle_add_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.add_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Simple voxel counting - look for "position" occurrences
    int voxel_count = 0;
    const char* pos = req->params_json;
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10; // Move past "position"
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to add");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"added\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_remove_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.remove_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Simple voxel counting - look for "position" occurrences
    int voxel_count = 0;
    const char* pos = req->params_json;
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10; // Move past "position"
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to remove");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"removed\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_paint_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.paint_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to validate and count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Count voxels with both position and color
    int voxel_count = 0;
    const char* pos = req->params_json;
    
    // Check if there are any voxels in the array
    const char* voxel_array = strstr(req->params_json, "\"voxels\"");
    if (voxel_array) {
        const char* array_start = strchr(voxel_array, '[');
        if (array_start) {
            const char* array_end = strchr(array_start, ']');
            if (array_end && array_end - array_start <= 2) {
                // Empty array
                return create_error_response(req->id, "No voxels to paint");
            }
        }
    }
    
    // Count voxels that have position
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10;
    }
    
    // Check if all voxels have colors
    int color_count = 0;
    pos = req->params_json;
    while ((pos = strstr(pos, "\"color\"")) != NULL) {
        color_count++;
        pos += 7;
    }
    
    if (voxel_count > color_count) {
        return create_error_response(req->id, "Missing color for voxel");
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to paint");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"painted\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

int test_parse_valid_request() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"id\":42}";
    
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    TEST_ASSERT(req != NULL, "Request should be parsed");
    TEST_ASSERT_STR_EQ("goxel.create_project", req->method);
    TEST_ASSERT_EQ(42, req->id);
    
    free_jsonrpc_request(req);
    return 1;
}

int test_parse_null_request() {
    jsonrpc_request_t* req = parse_jsonrpc_request(NULL);
    TEST_ASSERT(req == NULL, "NULL input should return NULL");
    return 1;
}

int test_create_success_response() {
    jsonrpc_response_t* resp = create_success_response(123, "{\"status\":\"ok\"}");
    
    TEST_ASSERT(resp != NULL, "Response should be created");
    TEST_ASSERT(resp->success == true, "Should be success");
    TEST_ASSERT_EQ(123, resp->id);
    TEST_ASSERT_STR_EQ("{\"status\":\"ok\"}", resp->result_json);
    TEST_ASSERT(resp->error_message == NULL, "No error message for success");
    
    free_jsonrpc_response(resp);
    return 1;
}

int test_create_error_response() {
    jsonrpc_response_t* resp = create_error_response(456, "Something went wrong");
    
    TEST_ASSERT(resp != NULL, "Response should be created");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_EQ(456, resp->id);
    TEST_ASSERT(resp->result_json == NULL, "No result for error");
    TEST_ASSERT_STR_EQ("Something went wrong", resp->error_message);
    
    free_jsonrpc_response(resp);
    return 1;
}

int test_serialize_success_response() {
    jsonrpc_response_t* resp = create_success_response(1, "\"done\"");
    char* json = serialize_jsonrpc_response(resp);
    
    TEST_ASSERT(json != NULL, "JSON should be created");
    TEST_ASSERT(strstr(json, "\"jsonrpc\":\"2.0\"") != NULL, "Should have jsonrpc version");
    TEST_ASSERT(strstr(json, "\"result\":\"done\"") != NULL, "Should have result");
    TEST_ASSERT(strstr(json, "\"id\":1") != NULL, "Should have id");
    
    free(json);
    free_jsonrpc_response(resp);
    return 1;
}

int test_serialize_error_response() {
    jsonrpc_response_t* resp = create_error_response(2, "Not found");
    char* json = serialize_jsonrpc_response(resp);
    
    TEST_ASSERT(json != NULL, "JSON should be created");
    TEST_ASSERT(strstr(json, "\"error\"") != NULL, "Should have error");
    TEST_ASSERT(strstr(json, "\"message\":\"Not found\"") != NULL, "Should have error message");
    TEST_ASSERT(strstr(json, "\"id\":2") != NULL, "Should have id");
    
    free(json);
    free_jsonrpc_response(resp);
    return 1;
}

int test_handle_create_project_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"id\":99}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_project(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "project_id") != NULL, "Should have project_id");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_project_wrong_method() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.unknown\",\"id\":99}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_project(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_STR_EQ("Invalid method", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"}]},\"id\":1}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(1, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "added") != NULL, "Should indicate voxels were added");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_multiple() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"},{\"position\":[1,0,0],\"color\":\"#00FF00\"},{\"position\":[2,0,0],\"color\":\"#0000FF\"}]},\"id\":2}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":3") != NULL, "Should report 3 voxels added");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_empty_array() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[]},\"id\":3}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to add", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[{\"position\":[5,5,5]}]},\"id\":10}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(10, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "removed") != NULL, "Should indicate voxels were removed");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_multiple() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0]},{\"position\":[1,1,1]},{\"position\":[2,2,2]},{\"position\":[3,3,3]}]},\"id\":11}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":4") != NULL, "Should report 4 voxels removed");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_empty() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[]},\"id\":12}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to remove", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_invalid_method() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.invalid\",\"id\":13}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_STR_EQ("Invalid method", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[10,10,10],\"color\":\"#00FF00\"}]},\"id\":20}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(20, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "painted") != NULL, "Should indicate voxels were painted");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_gradient() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"},{\"position\":[0,0,1],\"color\":\"#FF7F00\"},{\"position\":[0,0,2],\"color\":\"#FFFF00\"},{\"position\":[0,0,3],\"color\":\"#00FF00\"},{\"position\":[0,0,4],\"color\":\"#0000FF\"}]},\"id\":21}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":5") != NULL, "Should report 5 voxels painted");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_no_color() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0]}]},\"id\":22}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for missing color");
    TEST_ASSERT_STR_EQ("Missing color for voxel", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_empty() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[]},\"id\":23}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to paint", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int main() {
    TEST_SUITE_BEGIN();
    
    RUN_TEST(test_parse_valid_request);
    RUN_TEST(test_parse_null_request);
    RUN_TEST(test_create_success_response);
    RUN_TEST(test_create_error_response);
    RUN_TEST(test_serialize_success_response);
    RUN_TEST(test_serialize_error_response);
    RUN_TEST(test_handle_create_project_valid);
    RUN_TEST(test_handle_create_project_wrong_method);
    RUN_TEST(test_handle_add_voxels_single);
    RUN_TEST(test_handle_add_voxels_multiple);
    RUN_TEST(test_handle_add_voxels_empty_array);
    RUN_TEST(test_handle_remove_voxels_single);
    RUN_TEST(test_handle_remove_voxels_multiple);
    RUN_TEST(test_handle_remove_voxels_empty);
    RUN_TEST(test_handle_remove_voxels_invalid_method);
    RUN_TEST(test_handle_paint_voxels_single);
    RUN_TEST(test_handle_paint_voxels_gradient);
    RUN_TEST(test_handle_paint_voxels_no_color);
    RUN_TEST(test_handle_paint_voxels_empty);
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}