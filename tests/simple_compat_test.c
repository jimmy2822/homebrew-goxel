/* Simple Compatibility Proxy Test
 * 
 * Tests basic functionality without complex JSON operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/compat/compatibility_proxy.h"

int main() {
    printf("=== Simple Compatibility Proxy Test ===\n\n");
    
    // Test 1: Protocol Detection
    printf("Test 1: Protocol Detection\n");
    
    const char *legacy_mcp = "{\"tool\":\"goxel_add_voxels\"}";
    const char *legacy_ts = "{\"jsonrpc\":\"2.0\",\"method\":\"add_voxel\"}";
    const char *native_rpc = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\"}";
    
    compat_protocol_detection_t detection;
    
    // Test legacy MCP detection
    if (compat_detect_protocol(legacy_mcp, strlen(legacy_mcp), &detection) == JSON_RPC_SUCCESS) {
        printf("  ✓ Legacy MCP detected: %s (confidence: %.2f)\n", 
               compat_protocol_type_string(detection.type), detection.confidence);
        assert(detection.type == COMPAT_PROTOCOL_LEGACY_MCP);
        assert(detection.is_legacy == true);
    } else {
        printf("  ❌ Legacy MCP detection failed\n");
        return 1;
    }
    
    // Test legacy TypeScript detection
    if (compat_detect_protocol(legacy_ts, strlen(legacy_ts), &detection) == JSON_RPC_SUCCESS) {
        printf("  ✓ Legacy TypeScript detected: %s (confidence: %.2f)\n",
               compat_protocol_type_string(detection.type), detection.confidence);
        assert(detection.type == COMPAT_PROTOCOL_LEGACY_TYPESCRIPT);
        assert(detection.is_legacy == true);
    } else {
        printf("  ❌ Legacy TypeScript detection failed\n");
        return 1;
    }
    
    // Test native JSON-RPC detection
    if (compat_detect_protocol(native_rpc, strlen(native_rpc), &detection) == JSON_RPC_SUCCESS) {
        printf("  ✓ Native JSON-RPC detected: %s (confidence: %.2f)\n",
               compat_protocol_type_string(detection.type), detection.confidence);
        assert(detection.type == COMPAT_PROTOCOL_NATIVE_JSONRPC);
        assert(detection.is_legacy == false);
    } else {
        printf("  ❌ Native JSON-RPC detection failed\n");
        return 1;
    }
    
    // Test 2: Configuration
    printf("\nTest 2: Configuration\n");
    
    compat_proxy_config_t config;
    compat_get_default_config(&config);
    
    printf("  ✓ Default config loaded\n");
    printf("    Legacy MCP socket: %s\n", config.legacy_mcp_socket);
    printf("    Legacy daemon socket: %s\n", config.legacy_daemon_socket);
    printf("    New daemon socket: %s\n", config.new_daemon_socket);
    printf("    Deprecation warnings: %s\n", config.enable_deprecation_warnings ? "enabled" : "disabled");
    
    if (compat_validate_config(&config) == JSON_RPC_SUCCESS) {
        printf("  ✓ Configuration validation passed\n");
    } else {
        printf("  ❌ Configuration validation failed\n");
        return 1;
    }
    
    // Test 3: Utility Functions
    printf("\nTest 3: Utility Functions\n");
    
    printf("  Protocol type strings:\n");
    printf("    LEGACY_MCP: %s\n", compat_protocol_type_string(COMPAT_PROTOCOL_LEGACY_MCP));
    printf("    LEGACY_TYPESCRIPT: %s\n", compat_protocol_type_string(COMPAT_PROTOCOL_LEGACY_TYPESCRIPT));
    printf("    NATIVE_JSONRPC: %s\n", compat_protocol_type_string(COMPAT_PROTOCOL_NATIVE_JSONRPC));
    
    assert(compat_is_legacy_protocol(COMPAT_PROTOCOL_LEGACY_MCP) == true);
    assert(compat_is_legacy_protocol(COMPAT_PROTOCOL_LEGACY_TYPESCRIPT) == true);
    assert(compat_is_legacy_protocol(COMPAT_PROTOCOL_NATIVE_JSONRPC) == false);
    printf("  ✓ Protocol type checks working\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("\nCompatibility layer basic functionality verified:\n");
    printf("✅ Protocol detection working\n");
    printf("✅ Configuration management working\n");
    printf("✅ Utility functions working\n");
    printf("\nReady for integration with Sarah's MCP handler and Michael's daemon!\n");
    
    return 0;
}