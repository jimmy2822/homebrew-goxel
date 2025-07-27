/*
 * Test data fixtures for Goxel v14.6 testing
 * Author: James O'Brien (Agent-4)
 */

#ifndef GOXEL_TEST_DATA_H
#define GOXEL_TEST_DATA_H

#include <stdint.h>
#include <stddef.h>

// Test voxel data patterns
static const uint8_t TEST_VOXEL_CUBE_3x3x3[] = {
    // Layer 1 (y = -1)
    255, 0, 0, 255,    255, 0, 0, 255,    255, 0, 0, 255,
    255, 0, 0, 255,    255, 0, 0, 255,    255, 0, 0, 255,
    255, 0, 0, 255,    255, 0, 0, 255,    255, 0, 0, 255,
    
    // Layer 2 (y = 0)
    0, 255, 0, 255,    0, 255, 0, 255,    0, 255, 0, 255,
    0, 255, 0, 255,    0, 255, 0, 255,    0, 255, 0, 255,
    0, 255, 0, 255,    0, 255, 0, 255,    0, 255, 0, 255,
    
    // Layer 3 (y = 1)
    0, 0, 255, 255,    0, 0, 255, 255,    0, 0, 255, 255,
    0, 0, 255, 255,    0, 0, 255, 255,    0, 0, 255, 255,
    0, 0, 255, 255,    0, 0, 255, 255,    0, 0, 255, 255,
};

// Test JSON-RPC requests
static const char *TEST_JSON_CREATE_REQUEST = 
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"create\","
    "\"params\":{\"file\":\"/tmp/test.gox\"}}";

static const char *TEST_JSON_ADD_VOXEL_REQUEST = 
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"add_voxel\","
    "\"params\":{\"x\":0,\"y\":-16,\"z\":0,\"r\":255,\"g\":0,\"b\":0,\"a\":255}}";

static const char *TEST_JSON_EXPORT_REQUEST = 
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"export\","
    "\"params\":{\"input\":\"/tmp/test.gox\",\"output\":\"/tmp/test.obj\",\"format\":\"obj\"}}";

static const char *TEST_JSON_BATCH_REQUEST = 
    "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"create\",\"params\":{\"file\":\"/tmp/batch.gox\"}},"
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"add_voxel\",\"params\":{\"x\":0,\"y\":-16,\"z\":0,\"r\":255,\"g\":0,\"b\":0,\"a\":255}},"
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"add_voxel\",\"params\":{\"x\":1,\"y\":-16,\"z\":0,\"r\":0,\"g\":255,\"b\":0,\"a\":255}}]";

// Expected responses
static const char *TEST_JSON_SUCCESS_RESPONSE = 
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"success\":true}}";

static const char *TEST_JSON_ERROR_RESPONSE = 
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}";

// Performance test data sizes
typedef enum {
    TEST_SIZE_TINY = 10,        // 10 voxels
    TEST_SIZE_SMALL = 100,      // 100 voxels
    TEST_SIZE_MEDIUM = 1000,    // 1K voxels
    TEST_SIZE_LARGE = 10000,    // 10K voxels
    TEST_SIZE_HUGE = 100000     // 100K voxels
} test_data_size_t;

// Stress test configurations
typedef struct {
    int client_count;
    int requests_per_client;
    int think_time_ms;
    int duration_seconds;
    bool random_operations;
} stress_test_config_t;

static const stress_test_config_t STRESS_CONFIG_LIGHT = {
    .client_count = 5,
    .requests_per_client = 100,
    .think_time_ms = 10,
    .duration_seconds = 30,
    .random_operations = false
};

static const stress_test_config_t STRESS_CONFIG_MEDIUM = {
    .client_count = 20,
    .requests_per_client = 500,
    .think_time_ms = 5,
    .duration_seconds = 60,
    .random_operations = true
};

static const stress_test_config_t STRESS_CONFIG_HEAVY = {
    .client_count = 50,
    .requests_per_client = 1000,
    .think_time_ms = 1,
    .duration_seconds = 120,
    .random_operations = true
};

// Helper functions
static inline void generate_test_voxels(uint8_t *buffer, size_t count) {
    for (size_t i = 0; i < count; i++) {
        size_t offset = i * 4;
        buffer[offset + 0] = (uint8_t)(i * 7 % 256);     // R
        buffer[offset + 1] = (uint8_t)(i * 11 % 256);    // G
        buffer[offset + 2] = (uint8_t)(i * 13 % 256);    // B
        buffer[offset + 3] = 255;                         // A
    }
}

static inline const char* get_test_size_name(test_data_size_t size) {
    switch (size) {
        case TEST_SIZE_TINY:   return "tiny";
        case TEST_SIZE_SMALL:  return "small";
        case TEST_SIZE_MEDIUM: return "medium";
        case TEST_SIZE_LARGE:  return "large";
        case TEST_SIZE_HUGE:   return "huge";
        default: return "unknown";
    }
}

#endif // GOXEL_TEST_DATA_H