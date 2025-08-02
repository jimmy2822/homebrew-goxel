#include "tdd_framework.h"
#include <string.h>
#include <stdbool.h>

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
    
    // TODO: Implement voxel addition logic
    return NULL; // This will make the test fail (red light)
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
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}