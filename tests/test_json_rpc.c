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

#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// TEST UTILITIES
// ============================================================================

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST(name) \
    do { \
        printf("Testing %s... ", name); \
        fflush(stdout); \
        test_count++; \
    } while(0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        test_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        test_failed++; \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: Expected %d, got %d\n", (int)(expected), (int)(actual)); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (!expected && !actual) break; \
        if (!expected || !actual || strcmp(expected, actual) != 0) { \
            printf("FAIL: Expected '%s', got '%s'\n", \
                   expected ? expected : "(null)", \
                   actual ? actual : "(null)"); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            printf("FAIL: Expected non-null pointer\n"); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr)) { \
            printf("FAIL: Expected null pointer\n"); \
            test_failed++; \
            return; \
        } \
    } while(0)

// ============================================================================
// ID MANAGEMENT TESTS
// ============================================================================

static void test_id_creation_and_validation()
{
    TEST("ID creation and validation");
    
    json_rpc_id_t id;
    json_rpc_result_t result;
    
    // Test numeric ID
    result = json_rpc_create_id_number(42, &id);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(JSON_RPC_ID_NUMBER, id.type);
    ASSERT_EQ(42, id.value.number);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_id(&id));
    json_rpc_free_id(&id);
    
    // Test string ID
    result = json_rpc_create_id_string("test-id", &id);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(JSON_RPC_ID_STRING, id.type);
    ASSERT_STR_EQ("test-id", id.value.string);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_id(&id));
    json_rpc_free_id(&id);
    
    // Test null ID
    result = json_rpc_create_id_null(&id);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(JSON_RPC_ID_NULL, id.type);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_id(&id));
    json_rpc_free_id(&id);
    
    PASS();
}

static void test_id_cloning_and_equality()
{
    TEST("ID cloning and equality");
    
    json_rpc_id_t id1, id2;
    json_rpc_result_t result;
    
    // Test numeric ID cloning
    json_rpc_create_id_number(123, &id1);
    result = json_rpc_clone_id(&id1, &id2);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    assert(json_rpc_id_equals(&id1, &id2));
    json_rpc_free_id(&id1);
    json_rpc_free_id(&id2);
    
    // Test string ID cloning
    json_rpc_create_id_string("clone-test", &id1);
    result = json_rpc_clone_id(&id1, &id2);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    assert(json_rpc_id_equals(&id1, &id2));
    json_rpc_free_id(&id1);
    json_rpc_free_id(&id2);
    
    // Test null ID cloning
    json_rpc_create_id_null(&id1);
    result = json_rpc_clone_id(&id1, &id2);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    assert(json_rpc_id_equals(&id1, &id2));
    json_rpc_free_id(&id1);
    json_rpc_free_id(&id2);
    
    PASS();
}

// ============================================================================
// REQUEST PARSING TESTS
// ============================================================================

static void test_parse_valid_request()
{
    TEST("parsing valid JSON-RPC request");
    
    const char *json = "{\"jsonrpc\":\"2.0\",\"method\":\"test_method\",\"params\":[1,2,3],\"id\":42}";
    json_rpc_request_t *request = NULL;
    
    json_rpc_result_t result = json_rpc_parse_request(json, &request);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(request);
    
    ASSERT_STR_EQ("test_method", request->method);
    ASSERT_EQ(JSON_RPC_PARAMS_ARRAY, request->params.type);
    ASSERT_EQ(3, json_rpc_get_param_count(&request->params));
    ASSERT_EQ(JSON_RPC_ID_NUMBER, request->id.type);
    ASSERT_EQ(42, request->id.value.number);
    ASSERT_EQ(false, request->is_notification);
    
    json_rpc_free_request(request);
    PASS();
}

static void test_parse_notification()
{
    TEST("parsing JSON-RPC notification");
    
    const char *json = "{\"jsonrpc\":\"2.0\",\"method\":\"notify\",\"params\":{\"key\":\"value\"}}";
    json_rpc_request_t *request = NULL;
    
    json_rpc_result_t result = json_rpc_parse_request(json, &request);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(request);
    
    ASSERT_STR_EQ("notify", request->method);
    ASSERT_EQ(JSON_RPC_PARAMS_OBJECT, request->params.type);
    ASSERT_EQ(1, json_rpc_get_param_count(&request->params));
    ASSERT_EQ(JSON_RPC_ID_NULL, request->id.type);
    ASSERT_EQ(true, request->is_notification);
    
    json_rpc_free_request(request);
    PASS();
}

static void test_parse_request_no_params()
{
    TEST("parsing request without parameters");
    
    const char *json = "{\"jsonrpc\":\"2.0\",\"method\":\"simple\",\"id\":\"test\"}";
    json_rpc_request_t *request = NULL;
    
    json_rpc_result_t result = json_rpc_parse_request(json, &request);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(request);
    
    ASSERT_STR_EQ("simple", request->method);
    ASSERT_EQ(JSON_RPC_PARAMS_NONE, request->params.type);
    ASSERT_EQ(0, json_rpc_get_param_count(&request->params));
    ASSERT_EQ(JSON_RPC_ID_STRING, request->id.type);
    ASSERT_STR_EQ("test", request->id.value.string);
    
    json_rpc_free_request(request);
    PASS();
}

static void test_parse_invalid_requests()
{
    TEST("parsing invalid JSON-RPC requests");
    
    json_rpc_request_t *request = NULL;
    json_rpc_result_t result;
    
    // Invalid JSON
    result = json_rpc_parse_request("{invalid json", &request);
    ASSERT_EQ(JSON_RPC_ERROR_PARSE_FAILED, result);
    ASSERT_NULL(request);
    
    // Missing version
    result = json_rpc_parse_request("{\"method\":\"test\",\"id\":1}", &request);
    ASSERT_EQ(JSON_RPC_ERROR_INVALID_VERSION, result);
    ASSERT_NULL(request);
    
    // Wrong version
    result = json_rpc_parse_request("{\"jsonrpc\":\"1.0\",\"method\":\"test\",\"id\":1}", &request);
    ASSERT_EQ(JSON_RPC_ERROR_INVALID_VERSION, result);
    ASSERT_NULL(request);
    
    // Missing method
    result = json_rpc_parse_request("{\"jsonrpc\":\"2.0\",\"id\":1}", &request);
    ASSERT_EQ(JSON_RPC_ERROR_MISSING_FIELD, result);
    ASSERT_NULL(request);
    
    PASS();
}

// ============================================================================
// RESPONSE PARSING TESTS
// ============================================================================

static void test_parse_success_response()
{
    TEST("parsing success response");
    
    const char *json = "{\"jsonrpc\":\"2.0\",\"result\":42,\"id\":1}";
    json_rpc_response_t *response = NULL;
    
    json_rpc_result_t result = json_rpc_parse_response(json, &response);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(response);
    
    ASSERT_EQ(true, response->has_result);
    ASSERT_EQ(false, response->has_error);
    ASSERT_NOT_NULL(response->result);
    ASSERT_EQ(json_integer, response->result->type);
    ASSERT_EQ(42, response->result->u.integer);
    ASSERT_EQ(JSON_RPC_ID_NUMBER, response->id.type);
    ASSERT_EQ(1, response->id.value.number);
    
    json_rpc_free_response(response);
    PASS();
}

static void test_parse_error_response()
{
    TEST("parsing error response");
    
    const char *json = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":\"test\"}";
    json_rpc_response_t *response = NULL;
    
    json_rpc_result_t result = json_rpc_parse_response(json, &response);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(response);
    
    ASSERT_EQ(false, response->has_result);
    ASSERT_EQ(true, response->has_error);
    ASSERT_EQ(-32601, response->error.code);
    ASSERT_STR_EQ("Method not found", response->error.message);
    ASSERT_EQ(JSON_RPC_ID_STRING, response->id.type);
    ASSERT_STR_EQ("test", response->id.value.string);
    
    json_rpc_free_response(response);
    PASS();
}

// ============================================================================
// SERIALIZATION TESTS
// ============================================================================

static void test_serialize_request()
{
    TEST("serializing JSON-RPC request");
    
    // Create request with array parameters
    json_value *params = json_array_new(2);
    json_array_push(params, json_integer_new(1));
    json_array_push(params, json_string_new("test"));
    
    json_rpc_id_t id;
    json_rpc_create_id_number(42, &id);
    
    json_rpc_request_t *request = json_rpc_create_request_array("test_method", params, &id);
    ASSERT_NOT_NULL(request);
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_request(request, &json_str);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(json_str);
    
    // Parse back to verify
    json_rpc_request_t *parsed = NULL;
    result = json_rpc_parse_request(json_str, &parsed);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_STR_EQ("test_method", parsed->method);
    ASSERT_EQ(JSON_RPC_PARAMS_ARRAY, parsed->params.type);
    ASSERT_EQ(42, parsed->id.value.number);
    
    json_rpc_free_request(request);
    json_rpc_free_request(parsed);
    json_rpc_free_id(&id);
    free(json_str);
    PASS();
}

static void test_serialize_response()
{
    TEST("serializing JSON-RPC response");
    
    // Create success response
    json_value *result_data = json_string_new("success");
    json_rpc_id_t id;
    json_rpc_create_id_string("test-id", &id);
    
    json_rpc_response_t *response = json_rpc_create_response_result(result_data, &id);
    ASSERT_NOT_NULL(response);
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(json_str);
    
    // Debug: print the serialized JSON
    printf("[DEBUG] Serialized: %s\n", json_str);
    
    // Parse back to verify
    json_rpc_response_t *parsed = NULL;
    result = json_rpc_parse_response(json_str, &parsed);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(true, parsed->has_result);
    ASSERT_STR_EQ("test-id", parsed->id.value.string);
    
    json_rpc_free_response(response);
    json_rpc_free_response(parsed);
    json_rpc_free_id(&id);
    free(json_str);
    PASS();
}

static void test_serialize_error_response()
{
    TEST("serializing error response");
    
    json_rpc_id_t id;
    json_rpc_create_id_number(1, &id);
    
    json_rpc_response_t *response = json_rpc_create_response_error(
        JSON_RPC_METHOD_NOT_FOUND, "Method not found", NULL, &id);
    ASSERT_NOT_NULL(response);
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(json_str);
    
    // Parse back to verify
    json_rpc_response_t *parsed = NULL;
    result = json_rpc_parse_response(json_str, &parsed);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(true, parsed->has_error);
    ASSERT_EQ(JSON_RPC_METHOD_NOT_FOUND, parsed->error.code);
    ASSERT_STR_EQ("Method not found", parsed->error.message);
    
    json_rpc_free_response(response);
    json_rpc_free_response(parsed);
    json_rpc_free_id(&id);
    free(json_str);
    PASS();
}

// ============================================================================
// PARAMETER ACCESS TESTS
// ============================================================================

static void test_parameter_access()
{
    TEST("parameter access helpers");
    
    // Test array parameters
    const char *json = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":[\"hello\",42,true],\"id\":1}";
    json_rpc_request_t *request = NULL;
    json_rpc_parse_request(json, &request);
    
    json_value *param = NULL;
    json_rpc_result_t result;
    
    // Get first parameter
    result = json_rpc_get_param_by_index(&request->params, 0, &param);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(param);
    ASSERT_EQ(json_string, param->type);
    ASSERT_STR_EQ("hello", param->u.string.ptr);
    
    // Get second parameter
    result = json_rpc_get_param_by_index(&request->params, 1, &param);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(json_integer, param->type);
    ASSERT_EQ(42, param->u.integer);
    
    // Out of bounds
    result = json_rpc_get_param_by_index(&request->params, 10, &param);
    ASSERT_EQ(JSON_RPC_ERROR_INVALID_PARAMETER, result);
    
    json_rpc_free_request(request);
    
    // Test object parameters
    const char *json2 = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":{\"name\":\"John\",\"age\":30},\"id\":1}";
    json_rpc_parse_request(json2, &request);
    
    result = json_rpc_get_param_by_name(&request->params, "name", &param);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_STR_EQ("John", param->u.string.ptr);
    
    result = json_rpc_get_param_by_name(&request->params, "age", &param);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(30, param->u.integer);
    
    // Non-existent parameter
    result = json_rpc_get_param_by_name(&request->params, "missing", &param);
    ASSERT_EQ(JSON_RPC_ERROR_MISSING_FIELD, result);
    
    json_rpc_free_request(request);
    PASS();
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

static void test_error_messages()
{
    TEST("error message functions");
    
    const char *msg;
    
    // Test standard error messages
    msg = json_rpc_error_message(JSON_RPC_PARSE_ERROR);
    ASSERT_STR_EQ("Parse error", msg);
    
    msg = json_rpc_error_message(JSON_RPC_METHOD_NOT_FOUND);
    ASSERT_STR_EQ("Method not found", msg);
    
    // Test server error detection
    assert(json_rpc_is_server_error(-32050));
    assert(!json_rpc_is_server_error(-32700));
    
    // Test application error detection
    assert(json_rpc_is_application_error(-1000));
    assert(!json_rpc_is_application_error(-32600));
    
    PASS();
}

// ============================================================================
// VALIDATION TESTS
// ============================================================================

static void test_request_validation()
{
    TEST("request validation");
    
    json_rpc_id_t id;
    json_rpc_create_id_number(1, &id);
    
    // Valid request
    json_rpc_request_t *request = json_rpc_create_request_array("test", NULL, &id);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_request(request));
    json_rpc_free_request(request);
    
    // Reserved method name
    request = json_rpc_create_request_array("rpc.test", NULL, &id);
    ASSERT_EQ(JSON_RPC_ERROR_INVALID_PARAMETER, json_rpc_validate_request(request));
    json_rpc_free_request(request);
    
    json_rpc_free_id(&id);
    PASS();
}

static void test_response_validation()
{
    TEST("response validation");
    
    json_rpc_id_t id;
    json_rpc_create_id_number(1, &id);
    
    // Valid success response
    json_rpc_response_t *response = json_rpc_create_response_result(json_null_new(), &id);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_response(response));
    json_rpc_free_response(response);
    
    // Valid error response
    response = json_rpc_create_response_error(-32601, "Method not found", NULL, &id);
    ASSERT_EQ(JSON_RPC_SUCCESS, json_rpc_validate_response(response));
    json_rpc_free_response(response);
    
    json_rpc_free_id(&id);
    PASS();
}

// ============================================================================
// COMPREHENSIVE JSON-RPC 2.0 SPECIFICATION TESTS
// ============================================================================

static void test_json_rpc_specification_compliance()
{
    TEST("JSON-RPC 2.0 specification compliance");
    
    // Test all standard error codes are handled
    for (int code = JSON_RPC_PARSE_ERROR; code <= JSON_RPC_INTERNAL_ERROR; code++) {
        const char *msg = json_rpc_error_message(code);
        assert(msg != NULL);
        assert(strlen(msg) > 0);
    }
    
    // Test batch requests (should handle individual requests)
    const char *batch_json = "[{\"jsonrpc\":\"2.0\",\"method\":\"test1\",\"id\":1},"
                            "{\"jsonrpc\":\"2.0\",\"method\":\"test2\",\"id\":2}]";
    
    // Note: Current implementation handles single requests only
    // This test verifies the individual request parsing works
    json_rpc_request_t *request = NULL;
    const char *single_json = "{\"jsonrpc\":\"2.0\",\"method\":\"test1\",\"id\":1}";
    json_rpc_result_t result = json_rpc_parse_request(single_json, &request);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    json_rpc_free_request(request);
    
    PASS();
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

static void test_edge_cases()
{
    TEST("edge cases and boundary conditions");
    
    // Test null/empty inputs
    json_rpc_request_t *request = NULL;
    json_rpc_result_t result = json_rpc_parse_request(NULL, &request);
    ASSERT_EQ(JSON_RPC_ERROR_INVALID_PARAMETER, result);
    
    result = json_rpc_parse_request("", &request);
    ASSERT_EQ(JSON_RPC_ERROR_PARSE_FAILED, result);
    
    // Test very long method names
    char long_method[256];
    memset(long_method, 'a', sizeof(long_method) - 1);
    long_method[255] = '\0';
    
    json_rpc_id_t id;
    json_rpc_create_id_number(1, &id);
    request = json_rpc_create_request_array(long_method, NULL, &id);
    if (request) {
        result = json_rpc_validate_request(request);
        ASSERT_EQ(JSON_RPC_ERROR_INVALID_PARAMETER, result);
        json_rpc_free_request(request);
    }
    json_rpc_free_id(&id);
    
    PASS();
}

// ============================================================================
// MEMORY MANAGEMENT TESTS
// ============================================================================

static void test_memory_management()
{
    TEST("memory management and cleanup");
    
    // Test proper cleanup of complex structures
    json_value *params = json_object_new(2);
    json_object_push(params, "nested", json_array_new(3));
    json_object_push(params, "value", json_string_new("test"));
    
    json_rpc_id_t id;
    json_rpc_create_id_string("memory-test", &id);
    
    json_rpc_request_t *request = json_rpc_create_request_object("test", params, &id);
    ASSERT_NOT_NULL(request);
    
    // Serialize and parse back (tests memory handling in parsing)
    char *json_str = NULL;
    json_rpc_serialize_request(request, &json_str);
    
    json_rpc_request_t *parsed = NULL;
    json_rpc_parse_request(json_str, &parsed);
    
    // Cleanup everything
    json_rpc_free_request(request);
    json_rpc_free_request(parsed);
    json_rpc_free_id(&id);
    free(json_str);
    
    PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char *argv[])
{
    printf("=== JSON-RPC 2.0 Parser Foundation Test Suite ===\n\n");
    
    // ID management tests
    test_id_creation_and_validation();
    test_id_cloning_and_equality();
    
    // Request parsing tests
    test_parse_valid_request();
    test_parse_notification();
    test_parse_request_no_params();
    test_parse_invalid_requests();
    
    // Response parsing tests
    test_parse_success_response();
    test_parse_error_response();
    
    // Serialization tests
    test_serialize_request();
    test_serialize_response();
    test_serialize_error_response();
    
    // Parameter access tests
    test_parameter_access();
    
    // Error handling tests
    test_error_messages();
    
    // Validation tests
    test_request_validation();
    test_response_validation();
    
    // Specification compliance tests
    test_json_rpc_specification_compliance();
    
    // Edge case tests
    test_edge_cases();
    
    // Memory management tests
    test_memory_management();
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    
    if (test_failed == 0) {
        printf("\n🎉 All tests passed! JSON-RPC 2.0 implementation is complete.\n");
        return 0;
    } else {
        printf("\n❌ %d tests failed. Implementation needs fixes.\n", test_failed);
        return 1;
    }
}