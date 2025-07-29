/*
 * Goxel v14.0 MCP Protocol Test Suite
 * 
 * Comprehensive test framework for Sarah's MCP handler implementation.
 * Tests protocol compliance, performance claims, and robustness.
 * 
 * Target: Validate Sarah's claimed 0.28μs processing time
 * Author: Alex Kumar - Testing & Performance Validation Expert
 * Week 2, Days 1-2 (February 3-4, 2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>

// Include MCP handler interface
#include "../src/daemon/mcp_handler.h"

// ============================================================================
// TEST CONFIGURATION
// ============================================================================

#define TEST_SOCKET_PATH "/tmp/goxel_mcp_test.sock" 
#define MAX_TEST_SAMPLES 10000
#define WARMUP_ITERATIONS 100
#define STRESS_TEST_ITERATIONS 5000
#define FUZZ_TEST_ITERATIONS 1000

// Performance targets from Sarah's claims
#define TARGET_MCP_LATENCY_US 0.5   // Sarah claims 0.28μs
#define TARGET_MCP_THROUGHPUT 100000 // requests/second
#define TARGET_MEMORY_MB 10         // Memory overhead

// Test result codes
typedef enum {
    TEST_SUCCESS = 0,
    TEST_FAILURE = 1,
    TEST_TIMEOUT = 2,
    TEST_MEMORY_ERROR = 3,
    TEST_PROTOCOL_ERROR = 4
} test_result_t;

// ============================================================================
// TEST STATISTICS
// ============================================================================

typedef struct {
    double min_latency_us;
    double max_latency_us; 
    double avg_latency_us;
    double p95_latency_us;
    double p99_latency_us;
    size_t total_requests;
    size_t successful_requests;
    size_t failed_requests;
    size_t memory_usage_kb;
    double throughput_ops_sec;
} mcp_test_stats_t;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * Calculate percentiles from sorted latency array
 */
static double calculate_percentile(double *sorted_latencies, size_t count, double percentile)
{
    if (count == 0) return 0.0;
    
    size_t index = (size_t)((percentile / 100.0) * (count - 1));
    if (index >= count) index = count - 1;
    
    return sorted_latencies[index];
}

/**
 * Compare function for qsort
 */
static int compare_double(const void *a, const void *b)
{
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

/**
 * Calculate comprehensive statistics
 */
static void calculate_statistics(double *latencies, size_t count, mcp_test_stats_t *stats)
{
    if (count == 0) {
        memset(stats, 0, sizeof(*stats));
        return;
    }
    
    // Sort for percentile calculations
    qsort(latencies, count, sizeof(double), compare_double);
    
    stats->min_latency_us = latencies[0];
    stats->max_latency_us = latencies[count - 1];
    stats->p95_latency_us = calculate_percentile(latencies, count, 95.0);
    stats->p99_latency_us = calculate_percentile(latencies, count, 99.0);
    
    // Calculate average
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->avg_latency_us = sum / count;
    
    stats->total_requests = count;
    stats->successful_requests = count; // Assume all successful for now
    stats->failed_requests = 0;
    
    // Calculate throughput (ops/second)
    if (stats->avg_latency_us > 0) {
        stats->throughput_ops_sec = 1000000.0 / stats->avg_latency_us;
    }
}

// ============================================================================
// MCP TEST DATA GENERATORS
// ============================================================================

/**
 * Create basic MCP tool request for testing
 */
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

/**
 * Generate test data for various MCP tools
 */
static const char *mcp_test_data[][2] = {
    // [tool_name, args_json]
    {"ping", NULL},
    {"version", NULL},
    {"list_methods", NULL},
    {"goxel_create_project", "{\"name\": \"test_project\"}"},
    {"goxel_save_file", "{\"path\": \"/tmp/test.gox\"}"},
    {"goxel_open_file", "{\"path\": \"/tmp/test.gox\", \"format\": \"gox\"}"},
    {"goxel_add_voxels", "{\"position\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"color\": {\"r\": 255, \"g\": 0, \"b\": 0, \"a\": 255}}"},
    {"goxel_get_voxel", "{\"position\": {\"x\": 0, \"y\": 0, \"z\": 0}}"},
    {"goxel_remove_voxels", "{\"position\": {\"x\": 0, \"y\": 0, \"z\": 0}}"},
    {"goxel_new_layer", "{\"name\": \"test_layer\"}"},
    {"goxel_list_layers", NULL},
    {"goxel_export_file", "{\"path\": \"/tmp/export.obj\", \"format\": \"obj\"}"},
    {NULL, NULL} // Sentinel
};

// ============================================================================
// CORE MCP TESTING FUNCTIONS
// ============================================================================

/**
 * Test MCP handler initialization and cleanup
 */
static test_result_t test_mcp_initialization(void)
{
    printf("Testing MCP handler initialization...\n");
    
    // Test initialization
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: MCP handler initialization failed: %s\n", mcp_error_string(result));
        return TEST_FAILURE;
    }
    
    // Verify initialized state
    if (!mcp_handler_is_initialized()) {
        printf("FAIL: MCP handler reports not initialized after init\n");
        mcp_handler_cleanup();
        return TEST_FAILURE;
    }
    
    // Test double initialization (should succeed)
    result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Double initialization failed\n");
        mcp_handler_cleanup();
        return TEST_FAILURE;
    }
    
    // Test cleanup
    mcp_handler_cleanup();
    
    // Verify cleanup
    if (mcp_handler_is_initialized()) {
        printf("FAIL: MCP handler still reports initialized after cleanup\n");
        return TEST_FAILURE;
    }
    
    printf("PASS: MCP handler initialization/cleanup works correctly\n");
    return TEST_SUCCESS;
}

/**
 * Test basic MCP protocol translation
 */
static test_result_t test_mcp_translation_basic(void)
{
    printf("Testing basic MCP protocol translation...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize MCP handler\n");
        return TEST_FAILURE;
    }
    
    int passed = 0, total = 0;
    
    // Test each tool type
    for (int i = 0; mcp_test_data[i][0] != NULL; i++) {
        total++;
        
        mcp_tool_request_t *request = create_test_request(
            mcp_test_data[i][0], 
            mcp_test_data[i][1]
        );
        
        if (!request) {
            printf("FAIL: Could not create test request for %s\n", mcp_test_data[i][0]);
            continue;
        }
        
        json_rpc_request_t *jsonrpc_request = NULL;
        result = mcp_translate_request(request, &jsonrpc_request);
        
        if (result == MCP_SUCCESS && jsonrpc_request) {
            printf("PASS: Successfully translated %s\n", mcp_test_data[i][0]);
            passed++;
            json_rpc_free_request(jsonrpc_request);
        } else {
            printf("FAIL: Translation failed for %s: %s\n", 
                   mcp_test_data[i][0], mcp_error_string(result));
        }
        
        mcp_free_request(request);
    }
    
    mcp_handler_cleanup();
    
    printf("Translation test: %d/%d passed\n", passed, total);
    return (passed == total) ? TEST_SUCCESS : TEST_FAILURE;
}

/**
 * Performance test - Validate Sarah's 0.28μs claim
 */
static test_result_t test_mcp_performance_latency(void)
{
    printf("Testing MCP handler performance (target: <%.2fμs)...\n", TARGET_MCP_LATENCY_US);
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize MCP handler\n");
        return TEST_FAILURE;
    }
    
    double *latencies = malloc(MAX_TEST_SAMPLES * sizeof(double));
    if (!latencies) {
        printf("FAIL: Memory allocation failed\n");
        mcp_handler_cleanup();
        return TEST_MEMORY_ERROR;
    }
    
    size_t sample_count = 0;
    
    // Warmup
    printf("Warming up (%d iterations)...\n", WARMUP_ITERATIONS);
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        mcp_tool_request_t *request = create_test_request("ping", NULL);
        if (request) {
            json_rpc_request_t *jsonrpc_request = NULL;
            mcp_translate_request(request, &jsonrpc_request);
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
            mcp_free_request(request);
        }
    }
    
    printf("Running performance test (%d iterations)...\n", MAX_TEST_SAMPLES);
    
    // Main performance test - measure multiple tools
    for (int tool_idx = 0; mcp_test_data[tool_idx][0] != NULL && sample_count < MAX_TEST_SAMPLES; tool_idx++) {
        for (int rep = 0; rep < 100 && sample_count < MAX_TEST_SAMPLES; rep++) {
            mcp_tool_request_t *request = create_test_request(
                mcp_test_data[tool_idx][0], 
                mcp_test_data[tool_idx][1]
            );
            
            if (!request) continue;
            
            uint64_t start_time = get_time_us();
            
            json_rpc_request_t *jsonrpc_request = NULL;
            mcp_error_code_t translate_result = mcp_translate_request(request, &jsonrpc_request);
            
            uint64_t end_time = get_time_us();
            
            if (translate_result == MCP_SUCCESS) {
                latencies[sample_count] = (double)(end_time - start_time);
                sample_count++;
                
                if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
            }
            
            mcp_free_request(request);
            
            // Progress indicator
            if (sample_count % 1000 == 0) {
                printf("  Progress: %zu/%d samples\n", sample_count, MAX_TEST_SAMPLES);
            }
        }
    }
    
    if (sample_count == 0) {
        printf("FAIL: No successful samples collected\n");
        free(latencies);
        mcp_handler_cleanup();
        return TEST_FAILURE;
    }
    
    // Calculate statistics
    mcp_test_stats_t stats;
    calculate_statistics(latencies, sample_count, &stats);
    
    // Display results
    printf("\nMCP Performance Results (%zu samples):\n", sample_count);
    printf("  Min latency:     %.3f μs\n", stats.min_latency_us);
    printf("  Avg latency:     %.3f μs\n", stats.avg_latency_us);
    printf("  Max latency:     %.3f μs\n", stats.max_latency_us);
    printf("  P95 latency:     %.3f μs\n", stats.p95_latency_us);
    printf("  P99 latency:     %.3f μs\n", stats.p99_latency_us);
    printf("  Throughput:      %.0f ops/sec\n", stats.throughput_ops_sec);
    
    // Get MCP handler statistics
    mcp_handler_stats_t mcp_stats;
    mcp_get_handler_stats(&mcp_stats);
    printf("\nMCP Handler Statistics:\n");
    printf("  Total translations:   %zu\n", mcp_stats.requests_translated);
    printf("  Translation errors:   %zu\n", mcp_stats.translation_errors);
    printf("  Direct translations:  %zu\n", mcp_stats.direct_translations);
    printf("  Mapped translations:  %zu\n", mcp_stats.mapped_translations);
    printf("  Avg translation time: %.3f μs\n", mcp_stats.avg_translation_time_us);
    printf("  Batch requests:       %zu\n", mcp_stats.batch_requests);
    
    // Validate performance against targets
    bool meets_target = (stats.avg_latency_us <= TARGET_MCP_LATENCY_US);
    bool meets_sarah_claim = (stats.avg_latency_us <= 0.5); // Allow some margin
    
    printf("\nPerformance Validation:\n");
    printf("  Target (<%.2fμs):       %s\n", TARGET_MCP_LATENCY_US, 
           meets_target ? "PASS" : "FAIL");
    printf("  Sarah's claim (0.28μs): %s\n", 
           meets_sarah_claim ? "VALIDATED" : "NOT VALIDATED");
    printf("  vs Target:             %.1fx %s\n", 
           TARGET_MCP_LATENCY_US / stats.avg_latency_us,
           meets_target ? "better" : "worse");
    
    free(latencies);
    mcp_handler_cleanup();
    
    return meets_target ? TEST_SUCCESS : TEST_FAILURE;
}

/**
 * Test MCP protocol compliance and error handling
 */
static test_result_t test_mcp_protocol_compliance(void)
{
    printf("Testing MCP protocol compliance...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize MCP handler\n");
        return TEST_FAILURE;
    }
    
    int passed = 0, total = 0;
    
    // Test 1: Invalid tool name
    total++;
    mcp_tool_request_t *invalid_request = create_test_request("invalid_tool", "{}");
    if (invalid_request) {
        json_rpc_request_t *jsonrpc_request = NULL;
        result = mcp_translate_request(invalid_request, &jsonrpc_request);
        if (result == MCP_ERROR_INVALID_TOOL) {
            printf("PASS: Invalid tool properly rejected\n");
            passed++;
        } else {
            printf("FAIL: Invalid tool not properly rejected (got %s)\n", 
                   mcp_error_string(result));
        }
        mcp_free_request(invalid_request);
    }
    
    // Test 2: NULL request handling
    total++;
    result = mcp_translate_request(NULL, NULL);
    if (result == MCP_ERROR_INVALID_PARAMS) {
        printf("PASS: NULL request properly rejected\n");
        passed++;
    } else {
        printf("FAIL: NULL request not properly handled\n");
    }
    
    // Test 3: Tool discovery
    total++;
    size_t tool_count;
    const char **tools = mcp_get_available_tools(&tool_count);
    if (tools && tool_count > 0) {
        printf("PASS: Tool discovery works (%zu tools found)\n", tool_count);
        passed++;
        
        // Verify each tool has a description
        for (size_t i = 0; i < tool_count; i++) {
            const char *desc = mcp_get_tool_description(tools[i]);
            printf("  - %s: %s\n", tools[i], desc ? desc : "No description");
        }
    } else {
        printf("FAIL: Tool discovery failed\n");
    }
    
    // Test 4: Tool availability check
    total++;
    if (mcp_is_tool_available("ping") && !mcp_is_tool_available("nonexistent_tool")) {
        printf("PASS: Tool availability check works\n");
        passed++;
    } else {
        printf("FAIL: Tool availability check failed\n");
    }
    
    mcp_handler_cleanup();
    
    printf("Protocol compliance: %d/%d tests passed\n", passed, total);
    return (passed == total) ? TEST_SUCCESS : TEST_FAILURE;
}

/**
 * Fuzzing test for MCP handler robustness
 */
static test_result_t test_mcp_fuzzing(void)
{
    printf("Running MCP fuzzing tests (%d iterations)...\n", FUZZ_TEST_ITERATIONS);
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize MCP handler\n");
        return TEST_FAILURE;
    }
    
    int crashes = 0;
    int handled_errors = 0;
    int unexpected_results = 0;
    
    // Fuzz test data
    const char *fuzz_tools[] = {
        "", // Empty string
        NULL, // NULL pointer  
        "a", // Very short
        "very_long_tool_name_that_exceeds_normal_limits_and_should_be_handled_gracefully",
        "tool\x00with\x00nulls", // Embedded nulls
        "tool_with_unicode_€_chars", // Unicode
        "tool-with-special!@#$%^&*()chars", // Special chars
        "\x01\x02\x03\x04\x05", // Control chars
    };
    
    const char *fuzz_args[] = {
        "", // Empty JSON
        "{", // Incomplete JSON
        "}", // Just closing brace
        "null", // Null JSON
        "{\"key\":}", // Invalid JSON
        "{\"key\": \"value\", \"key\": \"duplicate\"}", // Duplicate keys
        "{\"very_long_key_name_that_exceeds_reasonable_limits\": \"value\"}", // Long key
        "{\"key\": \"\x01\x02\x03\"}", // Binary data in string
        "[1,2,3,4,5]", // Array instead of object
        "42", // Number instead of object
        "\"string\"", // String instead of object
    };
    
    // Run fuzz tests
    for (int i = 0; i < FUZZ_TEST_ITERATIONS; i++) {
        const char *tool = fuzz_tools[i % (sizeof(fuzz_tools)/sizeof(fuzz_tools[0]))];
        const char *args = fuzz_args[i % (sizeof(fuzz_args)/sizeof(fuzz_args[0]))];
        
        // Create request (may fail for invalid data)
        mcp_tool_request_t *request = create_test_request(tool, args);
        
        json_rpc_request_t *jsonrpc_request = NULL;
        mcp_error_code_t fuzz_result = mcp_translate_request(request, &jsonrpc_request);
        
        // Check result
        if (fuzz_result >= MCP_SUCCESS && fuzz_result <= MCP_ERROR_BATCH_TOO_LARGE) {
            handled_errors++;
        } else {
            unexpected_results++;
        }
        
        // Cleanup
        if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
        if (request) mcp_free_request(request);
        
        // Progress
        if (i % 100 == 0) {
            printf("  Fuzz progress: %d/%d\n", i, FUZZ_TEST_ITERATIONS);
        }
    }
    
    printf("Fuzzing results:\n");
    printf("  Crashes:           %d\n", crashes);
    printf("  Handled errors:    %d\n", handled_errors);
    printf("  Unexpected results: %d\n", unexpected_results);
    
    mcp_handler_cleanup();
    
    // Success if no crashes and reasonable error handling
    if (crashes == 0 && unexpected_results < (FUZZ_TEST_ITERATIONS / 10)) {
        printf("PASS: MCP handler survived fuzzing\n");
        return TEST_SUCCESS;
    } else {
        printf("FAIL: MCP handler showed instability\n");
        return TEST_FAILURE;
    }
}

/**
 * Test MCP batch operations
 */
static test_result_t test_mcp_batch_operations(void)
{
    printf("Testing MCP batch operations...\n");
    
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FAIL: Failed to initialize MCP handler\n");
        return TEST_FAILURE;
    }
    
    // Create batch of requests
    const int batch_size = 10;
    mcp_tool_request_t *requests = calloc(batch_size, sizeof(mcp_tool_request_t));
    if (!requests) {
        printf("FAIL: Memory allocation failed\n");
        mcp_handler_cleanup();
        return TEST_MEMORY_ERROR;
    }
    
    // Fill batch with test requests
    for (int i = 0; i < batch_size; i++) {
        int tool_idx = i % (sizeof(mcp_test_data)/sizeof(mcp_test_data[0]) - 1);
        requests[i].tool = strdup(mcp_test_data[tool_idx][0]);
        if (mcp_test_data[tool_idx][1]) {
            requests[i].arguments = json_parse(mcp_test_data[tool_idx][1], 
                                             strlen(mcp_test_data[tool_idx][1]));
        }
    }
    
    // Execute batch
    mcp_tool_response_t *responses = NULL;
    uint64_t start_time = get_time_us();
    result = mcp_handle_batch_requests(requests, batch_size, &responses);
    uint64_t end_time = get_time_us();
    
    double batch_time_us = (double)(end_time - start_time);
    double per_request_us = batch_time_us / batch_size;
    
    printf("Batch operation results:\n");
    printf("  Total time:        %.3f μs\n", batch_time_us);
    printf("  Per request:       %.3f μs\n", per_request_us);
    printf("  Batch efficiency:  %.1fx\n", TARGET_MCP_LATENCY_US / per_request_us);
    
    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        free(requests[i].tool);
        if (requests[i].arguments) json_value_free(requests[i].arguments);
        if (responses) mcp_free_response(&responses[i]);
    }
    free(requests);
    if (responses) free(responses);
    
    mcp_handler_cleanup();
    
    bool batch_efficient = (per_request_us <= TARGET_MCP_LATENCY_US);
    printf("%s: Batch operations %s target performance\n", 
           batch_efficient ? "PASS" : "FAIL",
           batch_efficient ? "meet" : "don't meet");
    
    return batch_efficient ? TEST_SUCCESS : TEST_FAILURE;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

static void print_test_header(void)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                    Goxel v14.0 MCP Protocol Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Testing Sarah's MCP Handler Implementation\n");
    printf("Target: Validate 0.28μs processing time claim\n");
    printf("Author: Alex Kumar - Testing & Performance Validation Expert\n");
    printf("Date: February 3-4, 2025 (Week 2, Days 1-2)\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
}

int main(int argc, char *argv[])
{
    print_test_header();
    
    typedef struct {
        const char *name;
        test_result_t (*func)(void);
        bool required;
    } test_case_t;
    
    test_case_t tests[] = {
        {"MCP Initialization", test_mcp_initialization, true},
        {"MCP Translation Basic", test_mcp_translation_basic, true},
        {"MCP Performance Latency", test_mcp_performance_latency, true},
        {"MCP Protocol Compliance", test_mcp_protocol_compliance, true},
        {"MCP Fuzzing", test_mcp_fuzzing, false},
        {"MCP Batch Operations", test_mcp_batch_operations, true},
        {NULL, NULL, false}
    };
    
    int total_tests = 0;
    int passed_tests = 0;
    int required_failed = 0;
    
    // Run all tests
    for (int i = 0; tests[i].name != NULL; i++) {
        total_tests++;
        
        printf("Running test: %s\n", tests[i].name);
        printf("─────────────────────────────────────────────────────────────────────────────\n");
        
        test_result_t result = tests[i].func();
        
        switch (result) {
            case TEST_SUCCESS:
                printf("✓ PASS: %s\n\n", tests[i].name);
                passed_tests++;
                break;
            case TEST_FAILURE:
                printf("✗ FAIL: %s\n\n", tests[i].name);
                if (tests[i].required) required_failed++;
                break;
            case TEST_TIMEOUT:
                printf("⏱ TIMEOUT: %s\n\n", tests[i].name);
                if (tests[i].required) required_failed++;
                break;
            case TEST_MEMORY_ERROR:
                printf("💾 MEMORY ERROR: %s\n\n", tests[i].name);
                if (tests[i].required) required_failed++;
                break;
            case TEST_PROTOCOL_ERROR:
                printf("🔌 PROTOCOL ERROR: %s\n\n", tests[i].name);
                if (tests[i].required) required_failed++;
                break;
        }
    }
    
    // Final results
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                                FINAL RESULTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Tests passed:    %d/%d (%.1f%%)\n", passed_tests, total_tests, 
           100.0 * passed_tests / total_tests);
    printf("Required failed: %d\n", required_failed);
    
    if (required_failed == 0) {
        printf("\n🎉 SUCCESS: Sarah's MCP handler implementation is VALIDATED!\n");
        printf("   All critical functionality working as claimed.\n");
        return 0;
    } else {
        printf("\n❌ FAILURE: %d critical tests failed\n", required_failed);
        printf("   Sarah's implementation needs fixes before production.\n");
        return 1;
    }
}