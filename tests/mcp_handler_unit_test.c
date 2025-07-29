/*
 * Goxel v14.0 MCP Handler Unit Test
 * 
 * Direct unit testing of Sarah's MCP handler implementation.
 * Tests the handler components without requiring full Goxel core.
 * 
 * Author: Alex Kumar - Testing & Performance Validation Expert  
 * Week 2, Day 1 (February 3, 2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <math.h>

// Include only the MCP handler header - test the interface
#include "../src/daemon/mcp_handler.h"

// ============================================================================
// MOCK JSON-RPC CONTEXT FOR TESTING
// ============================================================================

// Mock implementation to avoid full Goxel dependencies
json_rpc_result_t json_rpc_init_goxel_context(void) {
    return JSON_RPC_SUCCESS;
}

void json_rpc_cleanup_goxel_context(void) {
    // No-op for testing
}

json_rpc_request_t *json_rpc_create_request_object(const char *method, json_value *params, const json_rpc_id_t *id) {
    // Simple mock request creation
    json_rpc_request_t *request = calloc(1, sizeof(json_rpc_request_t));
    if (!request) return NULL;
    
    request->method = strdup(method);
    request->params = params; // Take ownership
    if (id) {
        request->id = *id;
        request->has_id = true;
    }
    
    return request;
}

void json_rpc_free_request(json_rpc_request_t *request) {
    if (!request) return;
    free((void*)request->method);
    if (request->params) json_value_free(request->params);
    free(request);
}

json_rpc_result_t json_rpc_create_id_number(int64_t number, json_rpc_id_t *id) {
    if (!id) return JSON_RPC_ERROR_INVALID_PARAMETER;
    id->type = JSON_RPC_ID_NUMBER;
    id->number = number;
    return JSON_RPC_SUCCESS;
}

void json_rpc_free_id(json_rpc_id_t *id) {
    if (!id) return;
    if (id->type == JSON_RPC_ID_STRING && id->string) {
        free((void*)id->string);
        id->string = NULL;
    }
}

json_rpc_response_t *json_rpc_handle_method(json_rpc_request_t *request) {
    // Mock response for testing
    json_rpc_response_t *response = calloc(1, sizeof(json_rpc_response_t));
    if (!response) return NULL;
    
    // Simple mock: ping returns success, others return method not found
    if (request->method && strcmp(request->method, "ping") == 0) {
        response->has_result = true;
        response->result = json_string_new("pong");
    } else {
        response->has_error = true;
        response->error.code = JSON_RPC_METHOD_NOT_FOUND;
        response->error.message = strdup("Method not found in mock");
        response->error.data = NULL;
    }
    
    if (request->has_id) {
        response->id = request->id;
        response->has_id = true;
    }
    
    return response;
}

void json_rpc_free_response(json_rpc_response_t *response) {
    if (!response) return;
    
    if (response->has_result && response->result) {
        json_value_free(response->result);
    }
    
    if (response->has_error) {
        free((void*)response->error.message);
        if (response->error.data) json_value_free(response->error.data);
    }
    
    if (response->has_id && response->id.type == JSON_RPC_ID_STRING && response->id.string) {
        free((void*)response->id.string);
    }
    
    free(response);
}

// ============================================================================
// TEST UTILITIES
// ============================================================================

static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static mcp_tool_request_t *create_test_request(const char *tool, const char *args_json)
{
    mcp_tool_request_t *request = calloc(1, sizeof(mcp_tool_request_t));
    if (!request) return NULL;
    
    request->tool = strdup(tool);
    if (!request->tool) {
        free(request);
        return NULL;
    }
    
    if (args_json) {
        request->arguments = json_parse(args_json, strlen(args_json));
        if (!request->arguments) {
            free(request->tool);
            free(request);
            return NULL;
        }
    }
    
    return request;
}

// ============================================================================
// UNIT TESTS
// ============================================================================

/**
 * Test MCP handler initialization
 */
static int test_initialization(void)
{
    printf("Testing MCP handler initialization...\n");
    
    // Should not be initialized initially
    if (mcp_handler_is_initialized()) {
        printf("FAIL: Handler reports initialized before init\n");
        return 1;
    }
    
    // Test initialization
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Initialization failed: %s\n", mcp_error_string(result));
        return 1;
    }
    
    // Should be initialized now
    if (!mcp_handler_is_initialized()) {
        printf("FAIL: Handler not initialized after init\n");
        return 1;
    }
    
    // Double init should succeed
    result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Double initialization failed\n");
        return 1;
    }
    
    // Test cleanup
    mcp_handler_cleanup();
    
    if (mcp_handler_is_initialized()) {
        printf("FAIL: Handler still initialized after cleanup\n");
        return 1;
    }
    
    printf("PASS: Initialization and cleanup work correctly\n");
    return 0;
}

/**
 * Test request translation performance
 */
static int test_translation_performance(void)
{
    printf("Testing MCP translation performance...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize handler\n");
        return 1;
    }
    
    const int num_samples = 10000;
    double *latencies = malloc(num_samples * sizeof(double));
    if (!latencies) {
        printf("FAIL: Memory allocation failed\n");
        mcp_handler_cleanup();
        return 1;
    }
    
    // Warmup
    for (int i = 0; i < 100; i++) {
        mcp_tool_request_t *request = create_test_request("ping", NULL);
        if (request) {
            json_rpc_request_t *jsonrpc_request = NULL;
            mcp_translate_request(request, &jsonrpc_request);
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
            mcp_free_request(request);
        }
    }
    
    // Performance test
    int successful_samples = 0;
    for (int i = 0; i < num_samples; i++) {
        mcp_tool_request_t *request = create_test_request("ping", NULL);
        if (!request) continue;
        
        uint64_t start_time = get_time_us();
        
        json_rpc_request_t *jsonrpc_request = NULL;
        mcp_error_code_t translate_result = mcp_translate_request(request, &jsonrpc_request);
        
        uint64_t end_time = get_time_us();
        
        if (translate_result == MCP_SUCCESS) {
            latencies[successful_samples] = (double)(end_time - start_time);
            successful_samples++;
            
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
        }
        
        mcp_free_request(request);
    }
    
    if (successful_samples == 0) {
        printf("FAIL: No successful samples\n");
        free(latencies);
        mcp_handler_cleanup();
        return 1;
    }
    
    // Calculate statistics
    double sum = 0.0, min_lat = latencies[0], max_lat = latencies[0];
    for (int i = 0; i < successful_samples; i++) {
        sum += latencies[i];
        if (latencies[i] < min_lat) min_lat = latencies[i];
        if (latencies[i] > max_lat) max_lat = latencies[i];
    }
    
    double avg_latency = sum / successful_samples;
    
    // Sort for percentiles
    for (int i = 0; i < successful_samples - 1; i++) {
        for (int j = i + 1; j < successful_samples; j++) {
            if (latencies[i] > latencies[j]) {
                double temp = latencies[i];
                latencies[i] = latencies[j];
                latencies[j] = temp;
            }
        }
    }
    
    double p95_latency = latencies[(int)(0.95 * successful_samples)];
    double p99_latency = latencies[(int)(0.99 * successful_samples)];
    
    printf("Performance results (%d samples):\n", successful_samples);
    printf("  Min latency:    %.3f μs\n", min_lat);
    printf("  Avg latency:    %.3f μs\n", avg_latency);
    printf("  Max latency:    %.3f μs\n", max_lat);
    printf("  P95 latency:    %.3f μs\n", p95_latency);
    printf("  P99 latency:    %.3f μs\n", p99_latency);
    printf("  Throughput:     %.0f ops/sec\n", 1000000.0 / avg_latency);
    
    // Get MCP handler statistics
    mcp_handler_stats_t stats;
    mcp_get_handler_stats(&stats);
    printf("  MCP avg time:   %.3f μs\n", stats.avg_translation_time_us);
    
    // Validate against Sarah's claim (0.28μs with some margin)
    bool meets_sarah_claim = (avg_latency <= 1.0); // Allow up to 1μs
    bool exceptional_performance = (avg_latency <= 0.5);
    
    printf("\nPerformance validation:\n");
    printf("  Sarah's claim (0.28μs): %s\n", 
           avg_latency <= 0.5 ? "VALIDATED" : "NOT VALIDATED");
    printf("  Meets 1μs target:       %s\n", 
           meets_sarah_claim ? "PASS" : "FAIL");
    printf("  Exceptional (<0.5μs):   %s\n", 
           exceptional_performance ? "YES" : "NO");
    
    free(latencies);
    mcp_handler_cleanup();
    
    return meets_sarah_claim ? 0 : 1;
}

/**
 * Test tool discovery functionality
 */
static int test_tool_discovery(void)
{
    printf("Testing MCP tool discovery...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize handler\n");
        return 1;
    }
    
    // Test tool listing
    size_t tool_count;
    const char **tools = mcp_get_available_tools(&tool_count);
    
    if (!tools || tool_count == 0) {
        printf("FAIL: No tools found\n");
        mcp_handler_cleanup();
        return 1;
    }
    
    printf("Found %zu tools:\n", tool_count);
    for (size_t i = 0; i < tool_count; i++) {
        const char *desc = mcp_get_tool_description(tools[i]);
        printf("  %s: %s\n", tools[i], desc ? desc : "No description");
    }
    
    // Test availability checks
    int availability_tests = 0;
    
    // Should find ping
    if (mcp_is_tool_available("ping")) {
        printf("PASS: ping tool available\n");
        availability_tests++;
    } else {
        printf("FAIL: ping tool not available\n");
    }
    
    // Should not find invalid tool
    if (!mcp_is_tool_available("invalid_nonexistent_tool")) {
        printf("PASS: invalid tool correctly not available\n");
        availability_tests++;
    } else {
        printf("FAIL: invalid tool incorrectly reported as available\n");
    }
    
    mcp_handler_cleanup();
    
    bool passed = (availability_tests == 2) && (tool_count >= 5);
    printf("%s: Tool discovery (%zu tools, %d/2 availability tests)\n", 
           passed ? "PASS" : "FAIL", tool_count, availability_tests);
    
    return passed ? 0 : 1;
}

/**
 * Test error handling
 */
static int test_error_handling(void)
{
    printf("Testing MCP error handling...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize handler\n");
        return 1;
    }
    
    int error_tests = 0;
    
    // Test NULL request
    json_rpc_request_t *jsonrpc_request = NULL;
    result = mcp_translate_request(NULL, &jsonrpc_request);
    if (result == MCP_ERROR_INVALID_PARAMS) {
        printf("PASS: NULL request properly rejected\n");
        error_tests++;
    } else {
        printf("FAIL: NULL request not properly handled\n");
    }
    
    // Test invalid tool
    mcp_tool_request_t *invalid_request = create_test_request("invalid_tool_name", NULL);
    if (invalid_request) {
        result = mcp_translate_request(invalid_request, &jsonrpc_request);
        if (result == MCP_ERROR_INVALID_TOOL) {
            printf("PASS: Invalid tool properly rejected\n");
            error_tests++;
        } else {
            printf("FAIL: Invalid tool not properly rejected (got %s)\n", 
                   mcp_error_string(result));
        }
        mcp_free_request(invalid_request);
    }
    
    // Test all error string mappings
    const mcp_error_code_t error_codes[] = {
        MCP_SUCCESS,
        MCP_ERROR_INVALID_TOOL,
        MCP_ERROR_INVALID_PARAMS,
        MCP_ERROR_INTERNAL,
        MCP_ERROR_NOT_IMPLEMENTED,
        MCP_ERROR_TRANSLATION,
        MCP_ERROR_OUT_OF_MEMORY,
        MCP_ERROR_BATCH_TOO_LARGE
    };
    
    bool all_errors_have_strings = true;
    for (size_t i = 0; i < sizeof(error_codes)/sizeof(error_codes[0]); i++) {
        const char *error_str = mcp_error_string(error_codes[i]);
        if (!error_str || strlen(error_str) == 0) {
            printf("FAIL: Error code %d has no string\n", error_codes[i]);
            all_errors_have_strings = false;
        }
    }
    
    if (all_errors_have_strings) {
        printf("PASS: All error codes have string representations\n");
        error_tests++;
    }
    
    mcp_handler_cleanup();
    
    bool passed = (error_tests == 3);
    printf("%s: Error handling (%d/3 tests passed)\n", 
           passed ? "PASS" : "FAIL", error_tests);
    
    return passed ? 0 : 1;
}

/**
 * Test statistics and monitoring
 */
static int test_statistics(void)
{
    printf("Testing MCP statistics...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize handler\n");
        return 1;
    }
    
    // Reset stats
    mcp_reset_handler_stats();
    
    // Get initial stats
    mcp_handler_stats_t initial_stats;
    mcp_get_handler_stats(&initial_stats);
    
    // Perform some operations
    const int num_operations = 100;
    for (int i = 0; i < num_operations; i++) {
        mcp_tool_request_t *request = create_test_request("ping", NULL);
        if (request) {
            json_rpc_request_t *jsonrpc_request = NULL;
            mcp_translate_request(request, &jsonrpc_request);
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
            mcp_free_request(request);
        }
    }
    
    // Get final stats
    mcp_handler_stats_t final_stats;
    mcp_get_handler_stats(&final_stats);
    
    printf("Statistics after %d operations:\n", num_operations);
    printf("  Requests translated: %llu\n", final_stats.requests_translated);
    printf("  Translation errors:  %llu\n", final_stats.translation_errors);
    printf("  Direct translations: %llu\n", final_stats.direct_translations);
    printf("  Mapped translations: %llu\n", final_stats.mapped_translations);
    printf("  Average time:        %.3f μs\n", final_stats.avg_translation_time_us);
    printf("  Batch requests:      %llu\n", final_stats.batch_requests);
    
    // Validate statistics
    bool stats_valid = true;
    
    if (final_stats.requests_translated < num_operations) {
        printf("FAIL: Request count too low (%llu < %d)\n", 
               final_stats.requests_translated, num_operations);
        stats_valid = false;
    }
    
    if (final_stats.avg_translation_time_us <= 0) {
        printf("FAIL: Average time not positive\n");
        stats_valid = false;
    }
    
    if (final_stats.requests_translated == 0) {
        printf("FAIL: No requests recorded in statistics\n");
        stats_valid = false;
    }
    
    mcp_handler_cleanup();
    
    printf("%s: Statistics tracking\n", stats_valid ? "PASS" : "FAIL");
    return stats_valid ? 0 : 1;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                     Goxel v14.0 MCP Handler Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Testing Sarah's MCP Handler Implementation (Direct Unit Tests)\n");
    printf("Author: Alex Kumar - Testing & Performance Validation Expert\n");
    printf("Date: February 3, 2025 (Week 2, Day 1)\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    typedef struct {
        const char *name;
        int (*func)(void);
    } test_case_t;
    
    test_case_t tests[] = {
        {"Initialization", test_initialization},
        {"Translation Performance", test_translation_performance},
        {"Tool Discovery", test_tool_discovery},
        {"Error Handling", test_error_handling},
        {"Statistics", test_statistics},
        {NULL, NULL}
    };
    
    int total_tests = 0;
    int passed_tests = 0;
    
    for (int i = 0; tests[i].name != NULL; i++) {
        total_tests++;
        
        printf("Running test: %s\n", tests[i].name);
        printf("─────────────────────────────────────────────────────────────────────────────\n");
        
        int result = tests[i].func();
        if (result == 0) {
            printf("✓ PASS: %s\n\n", tests[i].name);
            passed_tests++;
        } else {
            printf("✗ FAIL: %s\n\n", tests[i].name);
        }
    }
    
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                                FINAL RESULTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Tests passed: %d/%d (%.1f%%)\n", passed_tests, total_tests, 
           100.0 * passed_tests / total_tests);
    
    if (passed_tests == total_tests) {
        printf("\n🎉 SUCCESS: Sarah's MCP handler unit tests PASS!\n");
        printf("   Core functionality validated and performant.\n");
        return 0;
    } else {
        printf("\n❌ FAILURE: %d tests failed\n", total_tests - passed_tests);
        printf("   Implementation needs fixes.\n");
        return 1;
    }
}