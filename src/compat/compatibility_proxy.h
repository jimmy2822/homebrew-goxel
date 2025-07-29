/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel Compatibility Layer - Zero-Downtime Migration Support
 * 
 * This module provides transparent compatibility between old and new daemon
 * architectures, enabling zero-downtime migration for existing users.
 */

#ifndef COMPATIBILITY_PROXY_H
#define COMPATIBILITY_PROXY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <pthread.h>
#include "../utils/json.h"
#include "../daemon/json_rpc.h"

// ============================================================================
// PROTOCOL DETECTION
// ============================================================================

/**
 * Protocol types for compatibility layer
 */
typedef enum {
    COMPAT_PROTOCOL_UNKNOWN = 0,
    COMPAT_PROTOCOL_LEGACY_MCP,       /**< Old MCP server format */
    COMPAT_PROTOCOL_LEGACY_TYPESCRIPT, /**< Old TypeScript client format */
    COMPAT_PROTOCOL_LEGACY_JSONRPC,   /**< Old direct JSON-RPC format */
    COMPAT_PROTOCOL_NATIVE_MCP,       /**< New MCP format */
    COMPAT_PROTOCOL_NATIVE_JSONRPC    /**< New JSON-RPC format */
} compat_protocol_type_t;

/**
 * Protocol detection result
 */
typedef struct {
    compat_protocol_type_t type;
    bool is_legacy;
    const char *version_hint;
    double confidence;  /**< 0.0-1.0 detection confidence */
} compat_protocol_detection_t;

// ============================================================================
// TRANSLATION MAPPINGS
// ============================================================================

/**
 * Method name translation mapping
 */
typedef struct {
    const char *legacy_name;
    const char *new_name;
    const char *legacy_version;  /**< Version when this mapping was valid */
    bool deprecated;
} compat_method_mapping_t;

/**
 * Parameter transformation function
 */
typedef json_rpc_result_t (*compat_param_transformer_t)(
    const json_value *legacy_params,
    json_value **new_params,
    void *context
);

/**
 * Response transformation function  
 */
typedef json_rpc_result_t (*compat_response_transformer_t)(
    const json_value *new_response,
    json_value **legacy_response,
    void *context
);

/**
 * Complete method translation mapping
 */
typedef struct {
    const char *legacy_method;
    const char *new_method;
    compat_param_transformer_t param_transformer;
    compat_response_transformer_t response_transformer;
    const char *description;
    bool send_deprecation_warning;
} compat_translation_mapping_t;

// ============================================================================
// PROXY CONFIGURATION
// ============================================================================

/**
 * Compatibility proxy configuration
 */
typedef struct {
    // Legacy endpoints to emulate
    char legacy_mcp_socket[256];      /**< Old MCP server socket path */
    char legacy_daemon_socket[256];   /**< Old daemon socket path */
    
    // New daemon endpoint
    char new_daemon_socket[256];      /**< New unified daemon socket */
    
    // Compatibility behavior
    bool enable_deprecation_warnings;
    uint32_t warning_frequency;       /**< Warning every N requests */
    bool log_translation_stats;
    bool emulate_timing_behavior;     /**< Match old response timing */
    
    // Performance settings
    uint32_t max_concurrent_clients;
    uint32_t translation_cache_size;
    uint32_t connection_timeout_ms;
    
    // Logging and telemetry
    char log_file[256];
    bool telemetry_enabled;
    char telemetry_endpoint[512];
} compat_proxy_config_t;

// ============================================================================
// TELEMETRY AND STATISTICS
// ============================================================================

/**
 * Migration telemetry data
 */
typedef struct {
    uint64_t total_requests;
    uint64_t legacy_mcp_requests;
    uint64_t legacy_typescript_requests;
    uint64_t legacy_jsonrpc_requests;
    uint64_t native_requests;
    
    uint64_t translation_successes;
    uint64_t translation_errors;
    uint64_t deprecation_warnings_sent;
    
    uint64_t total_translation_time_us;
    double avg_translation_time_us;
    
    time_t first_legacy_request;
    time_t last_legacy_request;
    
    // Per-client tracking
    uint32_t unique_legacy_clients;
    uint32_t migrated_clients;
} compat_migration_stats_t;

// ============================================================================
// CLIENT CONNECTION CONTEXT
// ============================================================================

/**
 * Client connection context for compatibility tracking
 */
typedef struct {
    int client_fd;
    compat_protocol_type_t detected_protocol;
    bool is_legacy_client;
    
    // Client identification
    char client_id[64];               /**< Generated client identifier */
    char user_agent[256];             /**< Client identification string */
    
    // Statistics for this client
    uint64_t requests_translated;
    uint64_t warnings_sent;
    time_t first_request;
    time_t last_request;
    
    // Connection to new daemon
    int daemon_fd;
    bool daemon_connected;
    
    // Translation cache for this client
    void *translation_cache;
} compat_client_context_t;

// ============================================================================
// PROXY SERVER CONTEXT
// ============================================================================

/**
 * Main compatibility proxy server context
 */
typedef struct {
    compat_proxy_config_t config;
    compat_migration_stats_t stats;
    
    // Server sockets
    int legacy_mcp_server_fd;
    int legacy_daemon_server_fd;
    
    // Client management
    compat_client_context_t *clients;
    uint32_t max_clients;
    uint32_t active_clients;
    
    // Translation mappings
    const compat_translation_mapping_t *method_mappings;
    size_t mapping_count;
    
    // Runtime state
    bool running;
    pthread_mutex_t stats_mutex;
    pthread_mutex_t clients_mutex;
} compat_proxy_server_t;

// ============================================================================
// CORE API FUNCTIONS
// ============================================================================

/**
 * Initialize compatibility proxy system
 * 
 * @param config Proxy configuration
 * @param server Output server context
 * @return Success/error code
 */
json_rpc_result_t compat_proxy_init(
    const compat_proxy_config_t *config,
    compat_proxy_server_t **server
);

/**
 * Start compatibility proxy server
 * 
 * @param server Proxy server context
 * @return Success/error code
 */
json_rpc_result_t compat_proxy_start(compat_proxy_server_t *server);

/**
 * Stop compatibility proxy server
 * 
 * @param server Proxy server context
 */
void compat_proxy_stop(compat_proxy_server_t *server);

/**
 * Cleanup compatibility proxy resources
 * 
 * @param server Proxy server context to cleanup
 */
void compat_proxy_cleanup(compat_proxy_server_t *server);

// ============================================================================
// PROTOCOL DETECTION FUNCTIONS
// ============================================================================

/**
 * Detect protocol type from incoming data
 * 
 * @param data Raw socket data
 * @param length Data length
 * @param detection Output detection result
 * @return Success/error code
 */
json_rpc_result_t compat_detect_protocol(
    const char *data,
    size_t length,
    compat_protocol_detection_t *detection
);

/**
 * Auto-detect client type from connection patterns
 * 
 * @param client_fd Client socket descriptor
 * @param initial_data First received data
 * @param data_length Data length
 * @param context Output client context
 * @return Success/error code
 */
json_rpc_result_t compat_detect_client_type(
    int client_fd,
    const char *initial_data,
    size_t data_length,
    compat_client_context_t *context
);

// ============================================================================
// TRANSLATION FUNCTIONS
// ============================================================================

/**
 * Translate legacy request to new format
 * 
 * @param legacy_request Input legacy request
 * @param protocol_type Detected protocol type
 * @param new_request Output translated request
 * @param context Client context for cache/stats
 * @return Success/error code
 */
json_rpc_result_t compat_translate_request(
    const json_value *legacy_request,
    compat_protocol_type_t protocol_type,
    json_value **new_request,
    compat_client_context_t *context
);

/**
 * Translate new response to legacy format
 * 
 * @param new_response Input new format response
 * @param protocol_type Target legacy protocol
 * @param legacy_response Output translated response
 * @param context Client context for cache/stats
 * @return Success/error code
 */
json_rpc_result_t compat_translate_response(
    const json_value *new_response,
    compat_protocol_type_t protocol_type,
    json_value **legacy_response,
    compat_client_context_t *context
);

// ============================================================================
// BUILT-IN TRANSFORMERS
// ============================================================================

// TypeScript client parameter transformers
json_rpc_result_t compat_transform_ts_add_voxel_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context
);

json_rpc_result_t compat_transform_ts_load_project_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context
);

json_rpc_result_t compat_transform_ts_export_model_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context
);

// Legacy MCP parameter transformers
json_rpc_result_t compat_transform_legacy_mcp_params(
    const json_value *legacy_params,
    json_value **new_params,
    void *context
);

// Response transformers
json_rpc_result_t compat_transform_response_to_legacy_mcp(
    const json_value *new_response,
    json_value **legacy_response,
    void *context
);

json_rpc_result_t compat_transform_response_to_legacy_ts(
    const json_value *new_response,
    json_value **legacy_response,
    void *context
);

// ============================================================================
// TELEMETRY AND MONITORING
// ============================================================================

/**
 * Record request for telemetry
 * 
 * @param server Proxy server context
 * @param protocol_type Protocol type of request
 * @param method Method name
 * @param client_context Client context
 */
void compat_record_request(
    compat_proxy_server_t *server,
    compat_protocol_type_t protocol_type,
    const char *method,
    compat_client_context_t *client_context
);

/**
 * Send deprecation warning to client
 * 
 * @param client_context Client context
 * @param method Method being deprecated
 * @return Success/error code
 */
json_rpc_result_t compat_send_deprecation_warning(
    compat_client_context_t *client_context,
    const char *method
);

/**
 * Get migration statistics
 * 
 * @param server Proxy server context
 * @param stats Output statistics
 */
void compat_get_migration_stats(
    const compat_proxy_server_t *server,
    compat_migration_stats_t *stats
);

/**
 * Export telemetry data for external analysis
 * 
 * @param server Proxy server context
 * @param output_path Path to write telemetry data
 * @return Success/error code
 */
json_rpc_result_t compat_export_telemetry(
    const compat_proxy_server_t *server,
    const char *output_path
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get default compatibility proxy configuration
 * 
 * @param config Output default configuration
 */
void compat_get_default_config(compat_proxy_config_t *config);

/**
 * Load compatibility configuration from file
 * 
 * @param config_path Path to configuration file
 * @param config Output loaded configuration  
 * @return Success/error code
 */
json_rpc_result_t compat_load_config(
    const char *config_path,
    compat_proxy_config_t *config
);

/**
 * Validate compatibility configuration
 * 
 * @param config Configuration to validate
 * @return Success/error code
 */
json_rpc_result_t compat_validate_config(
    const compat_proxy_config_t *config
);

/**
 * Get string representation of protocol type
 * 
 * @param protocol_type Protocol type
 * @return String representation
 */
const char *compat_protocol_type_string(compat_protocol_type_t protocol_type);

/**
 * Check if protocol type is legacy
 * 
 * @param protocol_type Protocol type to check
 * @return true if legacy, false if native
 */
bool compat_is_legacy_protocol(compat_protocol_type_t protocol_type);

#endif // COMPATIBILITY_PROXY_H