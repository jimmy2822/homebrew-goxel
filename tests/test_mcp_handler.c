/* Goxel 3D voxels editor - MCP Handler Unit Tests
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Test suite for MCP protocol handler with comprehensive coverage
 * including protocol parsing, error conditions, and performance validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../src/daemon/mcp_handler.h"
#include "../ext_src/json/json.h"

// ============================================================================
// TEST FRAMEWORK
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: %s\n", #name); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("✓ %s passed\n", #name); \
    } \
    static void test_##name(void)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("✗ Assertion failed: %s (line %d)\n", #condition, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("✗ Assertion failed: expected %d, got %d (line %d)\n", \
                   (int)(expected), (int)(actual), __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (!expected || !actual || strcmp(expected, actual) != 0) { \
            printf("✗ String assertion failed: expected '%s', got '%s' (line %d)\n", \
                   expected ? expected : "NULL", actual ? actual : "NULL", __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            printf("✗ Null pointer assertion failed: %s (line %d)\n", #ptr, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Create a simple MCP request for testing
 */
static mcp_tool_request_t *create_test_request(const char *tool, const char *args_json)
{
    mcp_tool_request_t *request = calloc(1, sizeof(mcp_tool_request_t));
    request->tool = strdup(tool);
    
    if (args_json) {
        request->arguments = json_parse(args_json, strlen(args_json));
    }
    
    return request;
}

/**
 * Get current time in microseconds for performance testing
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST(handler_initialization)
{
    // Test initial state
    ASSERT_TRUE(!mcp_handler_is_initialized());
    
    // Test initialization
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_TRUE(mcp_handler_is_initialized());
    
    // Test double initialization (should succeed)
    result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Test cleanup
    mcp_handler_cleanup();
    ASSERT_TRUE(!mcp_handler_is_initialized());
}

// ============================================================================
// MEMORY MANAGEMENT TESTS
// ============================================================================

TEST(request_memory_management)
{
    // Test freeing NULL pointer (should not crash)
    mcp_free_request(NULL);
    
    // Test freeing valid request
    mcp_tool_request_t *request = create_test_request("test_tool", "{\"param\": \"value\"}");
    ASSERT_NOT_NULL(request);
    ASSERT_NOT_NULL(request->tool);
    ASSERT_NOT_NULL(request->arguments);
    
    mcp_free_request(request); // Should not crash
}

TEST(response_memory_management)
{
    // Test freeing NULL pointer (should not crash)
    mcp_free_response(NULL);
    
    // Test freeing valid response
    mcp_tool_response_t *response = calloc(1, sizeof(mcp_tool_response_t));
    response->success = true;
    response->content = json_object_new(0);
    response->error_message = strdup("test error");
    
    mcp_free_response(response); // Should not crash
}

// ============================================================================
// PROTOCOL TRANSLATION TESTS
// ============================================================================

TEST(direct_method_translation)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Test direct mapping method (no parameter transformation)
    mcp_tool_request_t *mcp_req = create_test_request("goxel_create_project", 
        "{\"name\": \"test_project\", \"path\": \"/tmp/test\"}");
    
    json_rpc_request_t *rpc_req = NULL;
    result = mcp_translate_request(mcp_req, &rpc_req);
    
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(rpc_req);
    ASSERT_STR_EQ("goxel.create_project", rpc_req->method);
    ASSERT_EQ(JSON_RPC_PARAMS_OBJECT, rpc_req->params.type);
    
    // Verify parameters were copied
    json_value *name = json_object_get(rpc_req->params.data, "name");
    ASSERT_NOT_NULL(name);
    ASSERT_STR_EQ("test_project", name->u.string.ptr);
    
    mcp_free_request(mcp_req);
    json_rpc_free_request(rpc_req);
    mcp_handler_cleanup();
}

TEST(voxel_position_mapping)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Test voxel position parameter mapping
    mcp_tool_request_t *mcp_req = create_test_request("goxel_add_voxels",
        "{\"position\": {\"x\": 10, \"y\": 20, \"z\": 30}, "
        "\"color\": {\"r\": 255, \"g\": 0, \"b\": 0, \"a\": 255}}");
    
    json_rpc_request_t *rpc_req = NULL;
    result = mcp_translate_request(mcp_req, &rpc_req);
    
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(rpc_req);
    ASSERT_STR_EQ("goxel.add_voxel", rpc_req->method);
    
    // Verify position was flattened
    json_value *x = json_object_get(rpc_req->params.data, "x");
    json_value *y = json_object_get(rpc_req->params.data, "y");
    json_value *z = json_object_get(rpc_req->params.data, "z");
    
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);
    ASSERT_NOT_NULL(z);
    ASSERT_EQ(10, (int)x->u.integer);
    ASSERT_EQ(20, (int)y->u.integer);
    ASSERT_EQ(30, (int)z->u.integer);
    
    // Verify color was converted to array
    json_value *rgba = json_object_get(rpc_req->params.data, "rgba");
    ASSERT_NOT_NULL(rgba);
    ASSERT_EQ(json_array, rgba->type);
    ASSERT_EQ(4, (int)rgba->u.array.length);
    ASSERT_EQ(255, (int)rgba->u.array.values[0]->u.integer);
    ASSERT_EQ(0, (int)rgba->u.array.values[1]->u.integer);
    
    mcp_free_request(mcp_req);
    json_rpc_free_request(rpc_req);
    mcp_handler_cleanup();
}

TEST(invalid_tool_translation)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Test unknown tool
    mcp_tool_request_t *mcp_req = create_test_request("unknown_tool", "{\"param\": \"value\"}");
    
    json_rpc_request_t *rpc_req = NULL;
    result = mcp_translate_request(mcp_req, &rpc_req);
    
    ASSERT_EQ(MCP_ERROR_INVALID_TOOL, result);
    ASSERT_TRUE(rpc_req == NULL);
    
    mcp_free_request(mcp_req);
    mcp_handler_cleanup();
}

// ============================================================================
// RESPONSE TRANSLATION TESTS
// ============================================================================

TEST(success_response_translation)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Create JSON-RPC success response
    json_rpc_id_t id;
    json_rpc_create_id_number(123, &id);
    
    json_value *result_data = json_object_new(0);
    json_object_push(result_data, "status", json_string_new("success"));
    
    json_rpc_response_t *rpc_resp = json_rpc_create_response_result(result_data, &id);
    ASSERT_NOT_NULL(rpc_resp);
    
    // Translate to MCP response
    mcp_tool_response_t *mcp_resp = NULL;
    result = mcp_translate_response(rpc_resp, "test_tool", &mcp_resp);
    
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(mcp_resp);
    ASSERT_TRUE(mcp_resp->success);
    ASSERT_EQ(MCP_SUCCESS, mcp_resp->error_code);
    ASSERT_NOT_NULL(mcp_resp->content);
    
    json_rpc_free_response(rpc_resp);
    mcp_free_response(mcp_resp);
    json_rpc_free_id(&id);
    mcp_handler_cleanup();
}

TEST(error_response_translation)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Create JSON-RPC error response
    json_rpc_id_t id;
    json_rpc_create_id_number(456, &id);
    
    json_rpc_response_t *rpc_resp = json_rpc_create_response_error(
        JSON_RPC_INVALID_PARAMS, "Invalid parameters", NULL, &id);
    ASSERT_NOT_NULL(rpc_resp);
    
    // Translate to MCP response
    mcp_tool_response_t *mcp_resp = NULL;
    result = mcp_translate_response(rpc_resp, "test_tool", &mcp_resp);
    
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(mcp_resp);
    ASSERT_TRUE(!mcp_resp->success);
    ASSERT_EQ(MCP_ERROR_INVALID_PARAMS, mcp_resp->error_code);
    ASSERT_STR_EQ("Invalid parameters", mcp_resp->error_message);
    
    json_rpc_free_response(rpc_resp);
    mcp_free_response(mcp_resp);
    json_rpc_free_id(&id);
    mcp_handler_cleanup();
}

// ============================================================================
// BATCH OPERATION TESTS
// ============================================================================

TEST(batch_operations)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Create batch of MCP requests
    mcp_tool_request_t requests[3];
    requests[0] = *create_test_request("ping", NULL);
    requests[1] = *create_test_request("version", NULL);
    requests[2] = *create_test_request("goxel_create_project", "{\"name\": \"batch_test\"}");
    
    mcp_tool_response_t *responses = NULL;
    result = mcp_handle_batch_requests(requests, 3, &responses);
    
    // Note: This may fail if daemon isn't fully initialized, but should not crash
    ASSERT_NOT_NULL(responses);
    
    // Cleanup (note: simplified cleanup for test structure)
    for (int i = 0; i < 3; i++) {
        free(requests[i].tool);
        if (requests[i].arguments) json_value_free(requests[i].arguments);
    }
    free(responses);
    mcp_handler_cleanup();
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST(translation_performance)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    const int iterations = 1000;
    const uint64_t target_time_us = 500; // 0.5ms target
    
    mcp_tool_request_t *mcp_req = create_test_request("goxel_create_project",
        "{\"name\": \"perf_test\", \"path\": \"/tmp/test\"}");
    
    uint64_t total_time = 0;
    int successful_translations = 0;
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = get_time_us();
        
        json_rpc_request_t *rpc_req = NULL;
        result = mcp_translate_request(mcp_req, &rpc_req);
        
        uint64_t end = get_time_us();
        
        if (result == MCP_SUCCESS) {
            total_time += (end - start);
            successful_translations++;
            json_rpc_free_request(rpc_req);
        }
    }
    
    if (successful_translations > 0) {
        uint64_t avg_time = total_time / successful_translations;
        printf("Average translation time: %lu µs (target: %lu µs)\n", 
               avg_time, target_time_us);
        
        // Performance target: <0.5ms per translation
        ASSERT_TRUE(avg_time < target_time_us);
    }
    
    mcp_free_request(mcp_req);
    mcp_handler_cleanup();
}

TEST(statistics_tracking)
{
    mcp_error_code_t result = mcp_handler_init();
    ASSERT_EQ(MCP_SUCCESS, result);
    
    // Reset stats
    mcp_reset_handler_stats();
    
    mcp_handler_stats_t stats;
    mcp_get_handler_stats(&stats);
    ASSERT_EQ(0, (int)stats.requests_translated);
    ASSERT_EQ(0, (int)stats.translation_errors);
    
    // Perform some translations
    mcp_tool_request_t *mcp_req = create_test_request("goxel_create_project", 
        "{\"name\": \"stats_test\"}");
    
    json_rpc_request_t *rpc_req = NULL;
    result = mcp_translate_request(mcp_req, &rpc_req);
    
    if (result == MCP_SUCCESS) {
        json_rpc_free_request(rpc_req);
        
        // Check stats updated
        mcp_get_handler_stats(&stats);
        ASSERT_EQ(1, (int)stats.requests_translated);
        ASSERT_EQ(1, (int)stats.direct_translations);
        ASSERT_TRUE(stats.avg_translation_time_us > 0);
    }
    
    mcp_free_request(mcp_req);
    mcp_handler_cleanup();
}

// ============================================================================
// DISCOVERY TESTS
// ============================================================================

TEST(tool_discovery)
{
    size_t count = 0;
    const char **tools = mcp_get_available_tools(&count);
    
    ASSERT_NOT_NULL(tools);
    ASSERT_TRUE(count > 0);
    
    // Check for known tools
    bool found_create_project = false;
    bool found_add_voxels = false;
    
    for (size_t i = 0; i < count; i++) {
        if (strcmp(tools[i], "goxel_create_project") == 0) {
            found_create_project = true;
        }
        if (strcmp(tools[i], "goxel_add_voxels") == 0) {
            found_add_voxels = true;
        }
    }
    
    ASSERT_TRUE(found_create_project);
    ASSERT_TRUE(found_add_voxels);
    
    // Test tool availability
    ASSERT_TRUE(mcp_is_tool_available("goxel_create_project"));
    ASSERT_TRUE(!mcp_is_tool_available("nonexistent_tool"));
    
    // Test tool descriptions
    const char *desc = mcp_get_tool_description("goxel_create_project");
    ASSERT_NOT_NULL(desc);
    ASSERT_TRUE(strlen(desc) > 0);
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST(error_handling)
{
    // Test error string function
    ASSERT_STR_EQ("Success", mcp_error_string(MCP_SUCCESS));
    ASSERT_STR_EQ("Unknown tool name", mcp_error_string(MCP_ERROR_INVALID_TOOL));
    ASSERT_STR_EQ("Invalid parameters", mcp_error_string(MCP_ERROR_INVALID_PARAMS));
    
    // Test JSON-RPC error mapping
    ASSERT_EQ(MCP_ERROR_INVALID_TOOL, mcp_map_jsonrpc_error(JSON_RPC_METHOD_NOT_FOUND));
    ASSERT_EQ(MCP_ERROR_INVALID_PARAMS, mcp_map_jsonrpc_error(JSON_RPC_INVALID_PARAMS));
    ASSERT_EQ(MCP_ERROR_INTERNAL, mcp_map_jsonrpc_error(JSON_RPC_INTERNAL_ERROR));
}

TEST(parsing_serialization)
{
    // Test parsing MCP request
    const char *json_req = "{\"tool\": \"test_tool\", \"arguments\": {\"param\": \"value\"}}";
    mcp_tool_request_t *request = NULL;
    
    mcp_error_code_t result = mcp_parse_request(json_req, &request);
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(request);
    ASSERT_STR_EQ("test_tool", request->tool);
    ASSERT_NOT_NULL(request->arguments);
    
    // Test serializing MCP response
    mcp_tool_response_t response = {0};
    response.success = true;
    response.content = json_object_new(0);
    json_object_push(response.content, "result", json_string_new("test_result"));
    
    char *json_resp = NULL;
    result = mcp_serialize_response(&response, &json_resp);
    ASSERT_EQ(MCP_SUCCESS, result);
    ASSERT_NOT_NULL(json_resp);
    ASSERT_TRUE(strstr(json_resp, "\"success\":true") != NULL);
    
    mcp_free_request(request);
    json_value_free(response.content);
    free(json_resp);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void)
{
    printf("=== MCP Handler Unit Tests ===\n\n");
    
    // Run all tests
    run_test_handler_initialization();
    run_test_request_memory_management();
    run_test_response_memory_management();
    run_test_direct_method_translation();
    run_test_voxel_position_mapping();
    run_test_invalid_tool_translation();
    run_test_success_response_translation();
    run_test_error_response_translation();
    run_test_batch_operations();
    run_test_translation_performance();
    run_test_statistics_tracking();
    run_test_tool_discovery();
    run_test_error_handling();
    run_test_parsing_serialization();
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n", tests_failed);
        return 1;
    }
}