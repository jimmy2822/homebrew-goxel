/* 
 * MCP Handler Integration Demo for Alex Kumar
 * Demonstrates the core interface that will be delivered by Day 2, 2:00 PM
 * 
 * Sarah Chen - Lead MCP Protocol Integration Specialist
 * Week 2, Day 1 - February 3, 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/daemon/mcp_handler.h"

// ============================================================================  
// JSON HELPER FUNCTIONS (temporary for demo)
// ============================================================================

static json_value *json_object_get_helper(json_value *obj, const char *key) {
    if (!obj || obj->type != json_object) return NULL;
    
    for (unsigned int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, key) == 0) {
            return obj->u.object.values[i].value;
        }
    }
    return NULL;
}

// ============================================================================
// DEMO FUNCTIONS
// ============================================================================

void demo_mcp_handler_interface(void) {
    printf("=== MCP Handler Interface Demo ===\n\n");
    
    // 1. Initialization
    printf("1. Initializing MCP Handler...\n");
    mcp_error_code_t result = mcp_handler_init();
    printf("   Result: %s\n", mcp_error_string(result));
    printf("   Initialized: %s\n\n", mcp_handler_is_initialized() ? "true" : "false");
    
    // 2. Tool Discovery
    printf("2. Available Tools:\n");
    size_t tool_count = 0;
    const char **tools = mcp_get_available_tools(&tool_count);
    for (size_t i = 0; i < tool_count && i < 5; i++) { // Show first 5
        printf("   - %s: %s\n", tools[i], 
               mcp_get_tool_description(tools[i]));
    }
    printf("   Total tools: %zu\n\n", tool_count);
    
    // 3. Request Translation Demo
    printf("3. Request Translation Example:\n");
    
    // Create a simple MCP request
    mcp_tool_request_t *mcp_req = calloc(1, sizeof(mcp_tool_request_t));
    mcp_req->tool = strdup("goxel_create_project");
    
    // Create simple arguments using json-builder
    json_value *args = json_object_new(2);
    json_object_push(args, "name", json_string_new("demo_project"));
    json_object_push(args, "path", json_string_new("/tmp/demo"));
    mcp_req->arguments = args;
    
    // Translate to JSON-RPC
    json_rpc_request_t *rpc_req = NULL;
    result = mcp_translate_request(mcp_req, &rpc_req);
    
    printf("   MCP Tool: %s\n", mcp_req->tool);
    printf("   Translation Result: %s\n", mcp_error_string(result));
    
    if (result == MCP_SUCCESS && rpc_req) {
        printf("   JSON-RPC Method: %s\n", rpc_req->method);
        printf("   Has Parameters: %s\n", 
               rpc_req->params.type != JSON_RPC_PARAMS_NONE ? "yes" : "no");
        
        // Clean up
        json_rpc_free_request(rpc_req);
    }
    
    mcp_free_request(mcp_req);
    
    // 4. Performance Statistics
    printf("\n4. Performance Statistics:\n");
    mcp_handler_stats_t stats;
    mcp_get_handler_stats(&stats);
    printf("   Requests Translated: %llu\n", stats.requests_translated);
    printf("   Direct Translations: %llu\n", stats.direct_translations);
    printf("   Average Time: %.2f µs\n", stats.avg_translation_time_us);
    
    // 5. Cleanup
    printf("\n5. Cleanup:\n");
    mcp_handler_cleanup();
    printf("   Handler cleaned up\n");
    printf("   Initialized: %s\n", mcp_handler_is_initialized() ? "true" : "false");
}

void demo_error_handling(void) {
    printf("\n=== Error Handling Demo ===\n\n");
    
    mcp_handler_init();
    
    // Test with invalid tool
    mcp_tool_request_t *bad_req = calloc(1, sizeof(mcp_tool_request_t));
    bad_req->tool = strdup("nonexistent_tool");
    
    json_rpc_request_t *rpc_req = NULL;
    mcp_error_code_t result = mcp_translate_request(bad_req, &rpc_req);
    
    printf("Invalid tool test:\n");
    printf("   Tool: %s\n", bad_req->tool);
    printf("   Error Code: %d\n", result);
    printf("   Error Message: %s\n", mcp_error_string(result));
    printf("   Request Created: %s\n", rpc_req ? "yes" : "no");
    
    mcp_free_request(bad_req);
    mcp_handler_cleanup();
}

void demo_performance_benchmark(void) {
    printf("\n=== Performance Benchmark Demo ===\n\n");
    
    mcp_handler_init();
    
    // Simple performance test
    const int iterations = 100;
    printf("Running %d translation operations...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        mcp_tool_request_t *req = calloc(1, sizeof(mcp_tool_request_t));
        req->tool = strdup("ping");
        
        json_rpc_request_t *rpc_req = NULL;
        mcp_error_code_t result = mcp_translate_request(req, &rpc_req);
        
        if (result == MCP_SUCCESS && rpc_req) {
            json_rpc_free_request(rpc_req);
        }
        
        mcp_free_request(req);
    }
    
    // Show final statistics
    mcp_handler_stats_t stats;
    mcp_get_handler_stats(&stats);
    printf("Performance Results:\n");
    printf("   Total Requests: %llu\n", stats.requests_translated);
    printf("   Average Time: %.2f µs\n", stats.avg_translation_time_us);
    printf("   Target: <500 µs (%.1fx faster than target)\n", 
           500.0 / stats.avg_translation_time_us);
    
    mcp_handler_cleanup();
}

// ============================================================================
// MAIN DEMO RUNNER
// ============================================================================

int main(void) {
    printf("MCP Handler Integration Demo\n");
    printf("For Alex Kumar - Performance Testing Integration\n");
    printf("Sarah Chen - Week 2, Day 1\n");
    printf("==========================================\n\n");
    
    demo_mcp_handler_interface();
    demo_error_handling(); 
    demo_performance_benchmark();
    
    printf("\n=== Demo Complete ===\n");
    printf("Ready for integration with daemon worker pool!\n");
    printf("Interface delivered as promised for Day 2, 2:00 PM\n\n");
    
    return 0;
}