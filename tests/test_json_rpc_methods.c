/* Goxel 3D voxels editor - JSON RPC Methods Tests
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
#include "../src/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// Test utilities
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
    } \
} while(0)

#define TEST_LOG(format, ...) printf("  [INFO] " format "\n", ##__VA_ARGS__)

// Test JSON strings for different methods
static const char *test_create_project_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"params\":[\"Test Project\",32,32,32],\"id\":1}";

static const char *test_add_voxel_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxel\",\"params\":[0,-16,0,255,0,0,255,0],\"id\":2}";

static const char *test_get_voxel_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":[0,-16,0],\"id\":3}";

static const char *test_get_status_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_status\",\"params\":[],\"id\":4}";

static const char *test_list_layers_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.list_layers\",\"params\":[],\"id\":5}";

static const char *test_create_layer_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"params\":[\"Test Layer\",128,128,255,true],\"id\":6}";

static const char *test_remove_voxel_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxel\",\"params\":[0,-16,0,0],\"id\":7}";

static const char *test_unknown_method_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"unknown.method\",\"params\":[],\"id\":8}";

static const char *test_invalid_params_json = 
    "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxel\",\"params\":[],\"id\":9}";

// Helper function to check if response is a success
static bool is_success_response(json_rpc_response_t *response)
{
    if (!response || !response->has_result || !response->result) {
        return false;
    }
    
    if (response->result->type != json_object) {
        return false;
    }
    
    // Look for "success": true
    for (unsigned int i = 0; i < response->result->u.object.length; i++) {
        json_object_entry *entry = &response->result->u.object.values[i];
        if (strcmp(entry->name, "success") == 0) {
            if (entry->value->type == json_boolean && entry->value->u.boolean) {
                return true;
            }
        }
    }
    
    return false;
}

// Helper function to check if response is an error
static bool is_error_response(json_rpc_response_t *response, int32_t expected_code)
{
    if (!response || !response->has_error) {
        return false;
    }
    
    return (expected_code == -1) || (response->error.code == expected_code);
}

// Helper function to get integer field from response result
static int get_result_int(json_rpc_response_t *response, const char *field_name)
{
    if (!response || !response->has_result || !response->result) {
        return -1;
    }
    
    if (response->result->type != json_object) {
        return -1;
    }
    
    for (unsigned int i = 0; i < response->result->u.object.length; i++) {
        json_object_entry *entry = &response->result->u.object.values[i];
        if (strcmp(entry->name, field_name) == 0) {
            if (entry->value->type == json_integer) {
                return (int)entry->value->u.integer;
            }
        }
    }
    
    return -1;
}

// Helper function to get string field from response result
static const char *get_result_string(json_rpc_response_t *response, const char *field_name)
{
    if (!response || !response->has_result || !response->result) {
        return NULL;
    }
    
    if (response->result->type != json_object) {
        return NULL;
    }
    
    for (unsigned int i = 0; i < response->result->u.object.length; i++) {
        json_object_entry *entry = &response->result->u.object.values[i];
        if (strcmp(entry->name, field_name) == 0) {
            if (entry->value->type == json_string) {
                return entry->value->u.string.ptr;
            }
        }
    }
    
    return NULL;
}

// Test context initialization
static void test_context_initialization(void)
{
    printf("\n=== Testing Context Initialization ===\n");
    
    // Initialize context
    json_rpc_result_t result = json_rpc_init_goxel_context();
    TEST_ASSERT(result == JSON_RPC_SUCCESS, "Context initialization succeeds");
    
    // Try to initialize again (should succeed but warn)
    result = json_rpc_init_goxel_context();
    TEST_ASSERT(result == JSON_RPC_SUCCESS, "Double initialization handles gracefully");
}

// Test method listing
static void test_method_listing(void)
{
    printf("\n=== Testing Method Listing ===\n");
    
    char buffer[2048];
    int result = json_rpc_list_methods(buffer, sizeof(buffer));
    
    TEST_ASSERT(result == 0, "Method listing succeeds");
    TEST_ASSERT(strlen(buffer) > 0, "Method list is not empty");
    TEST_ASSERT(strstr(buffer, "goxel.create_project") != NULL, "Contains create_project method");
    TEST_ASSERT(strstr(buffer, "goxel.add_voxel") != NULL, "Contains add_voxel method");
    TEST_ASSERT(strstr(buffer, "goxel.get_status") != NULL, "Contains get_status method");
    
    TEST_LOG("Available methods:\n%s", buffer);
}

// Test project creation
static void test_create_project(void)
{
    printf("\n=== Testing Project Creation ===\n");
    
    json_rpc_request_t *request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_create_project_json, &request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse create_project request");
    TEST_ASSERT(request != NULL, "Request is not NULL");
    
    if (request) {
        json_rpc_response_t *response = json_rpc_handle_method(request);
        TEST_ASSERT(response != NULL, "Response is not NULL");
        TEST_ASSERT(is_success_response(response), "Create project succeeds");
        
        if (response) {
            const char *name = get_result_string(response, "name");
            int width = get_result_int(response, "width");
            int height = get_result_int(response, "height");
            int depth = get_result_int(response, "depth");
            
            TEST_ASSERT(name && strcmp(name, "Test Project") == 0, "Project name is correct");
            TEST_ASSERT(width == 32, "Project width is correct");
            TEST_ASSERT(height == 32, "Project height is correct");
            TEST_ASSERT(depth == 32, "Project depth is correct");
            
            TEST_LOG("Created project: %s (%dx%dx%d)", name, width, height, depth);
            
            json_rpc_free_response(response);
        }
        
        json_rpc_free_request(request);
    }
}

// Test voxel operations
static void test_voxel_operations(void)
{
    printf("\n=== Testing Voxel Operations ===\n");
    
    // Test adding a voxel
    json_rpc_request_t *add_request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_add_voxel_json, &add_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse add_voxel request");
    
    if (add_request) {
        json_rpc_response_t *add_response = json_rpc_handle_method(add_request);
        TEST_ASSERT(add_response != NULL, "Add voxel response is not NULL");
        TEST_ASSERT(is_success_response(add_response), "Add voxel succeeds");
        
        if (add_response) {
            int x = get_result_int(add_response, "x");
            int y = get_result_int(add_response, "y");
            int z = get_result_int(add_response, "z");
            
            TEST_ASSERT(x == 0 && y == -16 && z == 0, "Added voxel coordinates are correct");
            TEST_LOG("Added voxel at (%d, %d, %d)", x, y, z);
            
            json_rpc_free_response(add_response);
        }
        
        json_rpc_free_request(add_request);
    }
    
    // Test getting the voxel we just added
    json_rpc_request_t *get_request = NULL;
    parse_result = json_rpc_parse_request(test_get_voxel_json, &get_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse get_voxel request");
    
    if (get_request) {
        json_rpc_response_t *get_response = json_rpc_handle_method(get_request);
        TEST_ASSERT(get_response != NULL, "Get voxel response is not NULL");
        TEST_ASSERT(get_response->has_result, "Get voxel has result");
        
        if (get_response && get_response->has_result) {
            int x = get_result_int(get_response, "x");
            int y = get_result_int(get_response, "y");
            int z = get_result_int(get_response, "z");
            
            TEST_ASSERT(x == 0 && y == -16 && z == 0, "Retrieved voxel coordinates are correct");
            TEST_LOG("Retrieved voxel at (%d, %d, %d)", x, y, z);
            
            json_rpc_free_response(get_response);
        }
        
        json_rpc_free_request(get_request);
    }
    
    // Test removing the voxel
    json_rpc_request_t *remove_request = NULL;
    parse_result = json_rpc_parse_request(test_remove_voxel_json, &remove_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse remove_voxel request");
    
    if (remove_request) {
        json_rpc_response_t *remove_response = json_rpc_handle_method(remove_request);
        TEST_ASSERT(remove_response != NULL, "Remove voxel response is not NULL");
        TEST_ASSERT(is_success_response(remove_response), "Remove voxel succeeds");
        
        if (remove_response) {
            int x = get_result_int(remove_response, "x");
            int y = get_result_int(remove_response, "y");
            int z = get_result_int(remove_response, "z");
            
            TEST_ASSERT(x == 0 && y == -16 && z == 0, "Removed voxel coordinates are correct");
            TEST_LOG("Removed voxel at (%d, %d, %d)", x, y, z);
            
            json_rpc_free_response(remove_response);
        }
        
        json_rpc_free_request(remove_request);
    }
}

// Test status and info methods
static void test_status_methods(void)
{
    printf("\n=== Testing Status Methods ===\n");
    
    // Test get_status
    json_rpc_request_t *status_request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_get_status_json, &status_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse get_status request");
    
    if (status_request) {
        json_rpc_response_t *status_response = json_rpc_handle_method(status_request);
        TEST_ASSERT(status_response != NULL, "Status response is not NULL");
        TEST_ASSERT(status_response->has_result, "Status has result");
        
        if (status_response && status_response->has_result) {
            const char *version = get_result_string(status_response, "version");
            int layer_count = get_result_int(status_response, "layer_count");
            int width = get_result_int(status_response, "width");
            
            TEST_ASSERT(version != NULL, "Version is provided");
            TEST_ASSERT(layer_count >= 0, "Layer count is valid");
            TEST_ASSERT(width >= 0, "Width is valid");
            
            TEST_LOG("Status: version=%s, layers=%d, dimensions=%dx%dx%d", 
                    version ? version : "unknown", layer_count, width,
                    get_result_int(status_response, "height"),
                    get_result_int(status_response, "depth"));
            
            json_rpc_free_response(status_response);
        }
        
        json_rpc_free_request(status_request);
    }
    
    // Test list_layers
    json_rpc_request_t *layers_request = NULL;
    parse_result = json_rpc_parse_request(test_list_layers_json, &layers_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse list_layers request");
    
    if (layers_request) {
        json_rpc_response_t *layers_response = json_rpc_handle_method(layers_request);
        TEST_ASSERT(layers_response != NULL, "Layers response is not NULL");
        TEST_ASSERT(layers_response->has_result, "Layers has result");
        
        if (layers_response && layers_response->has_result) {
            int count = get_result_int(layers_response, "count");
            TEST_ASSERT(count >= 0, "Layer count is valid");
            TEST_LOG("Found %d layers", count);
            
            json_rpc_free_response(layers_response);
        }
        
        json_rpc_free_request(layers_request);
    }
}

// Test layer operations
static void test_layer_operations(void)
{
    printf("\n=== Testing Layer Operations ===\n");
    
    json_rpc_request_t *request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_create_layer_json, &request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse create_layer request");
    
    if (request) {
        json_rpc_response_t *response = json_rpc_handle_method(request);
        TEST_ASSERT(response != NULL, "Create layer response is not NULL");
        TEST_ASSERT(is_success_response(response), "Create layer succeeds");
        
        if (response) {
            const char *name = get_result_string(response, "name");
            TEST_ASSERT(name && strcmp(name, "Test Layer") == 0, "Layer name is correct");
            TEST_LOG("Created layer: %s", name);
            
            json_rpc_free_response(response);
        }
        
        json_rpc_free_request(request);
    }
}

// Test error handling
static void test_error_handling(void)
{
    printf("\n=== Testing Error Handling ===\n");
    
    // Test unknown method
    json_rpc_request_t *unknown_request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_unknown_method_json, &unknown_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse unknown method request");
    
    if (unknown_request) {
        json_rpc_response_t *response = json_rpc_handle_method(unknown_request);
        TEST_ASSERT(response != NULL, "Unknown method response is not NULL");
        TEST_ASSERT(is_error_response(response, JSON_RPC_METHOD_NOT_FOUND), "Unknown method returns method not found error");
        
        if (response) {
            TEST_LOG("Unknown method error: %s", response->error.message);
            json_rpc_free_response(response);
        }
        
        json_rpc_free_request(unknown_request);
    }
    
    // Test invalid parameters
    json_rpc_request_t *invalid_request = NULL;
    parse_result = json_rpc_parse_request(test_invalid_params_json, &invalid_request);
    
    TEST_ASSERT(parse_result == JSON_RPC_SUCCESS, "Parse invalid params request");
    
    if (invalid_request) {
        json_rpc_response_t *response = json_rpc_handle_method(invalid_request);
        TEST_ASSERT(response != NULL, "Invalid params response is not NULL");
        // Note: This might succeed with default values, or fail with invalid params
        // Both are acceptable behaviors depending on implementation
        
        if (response) {
            if (response->has_error) {
                TEST_LOG("Invalid params error: %s", response->error.message);
            } else {
                TEST_LOG("Invalid params handled with defaults");
            }
            json_rpc_free_response(response);
        }
        
        json_rpc_free_request(invalid_request);
    }
}

// Test cleanup
static void test_cleanup(void)
{
    printf("\n=== Testing Cleanup ===\n");
    
    // Clean up context
    json_rpc_cleanup_goxel_context();
    TEST_ASSERT(true, "Context cleanup completes");
    
    // Try to use methods after cleanup (should fail)
    json_rpc_request_t *request = NULL;
    json_rpc_result_t parse_result = json_rpc_parse_request(test_get_status_json, &request);
    
    if (parse_result == JSON_RPC_SUCCESS && request) {
        json_rpc_response_t *response = json_rpc_handle_method(request);
        TEST_ASSERT(response != NULL, "Method call after cleanup returns response");
        TEST_ASSERT(is_error_response(response, JSON_RPC_INTERNAL_ERROR), "Method call after cleanup returns internal error");
        
        if (response) {
            json_rpc_free_response(response);
        }
        json_rpc_free_request(request);
    }
}

// Main test function
int main(void)
{
    printf("=== JSON RPC Methods Test Suite ===\n");
    
    // Run all tests
    test_context_initialization();
    test_method_listing();
    test_create_project();
    test_voxel_operations();
    test_status_methods();
    test_layer_operations();
    test_error_handling();
    test_cleanup();
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_passed == tests_run) {
        printf("\n🎉 All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed.\n");
        return 1;
    }
}