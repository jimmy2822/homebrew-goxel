/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#ifndef MCP_HANDLER_H
#define MCP_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "json_rpc.h"
#include "../../ext_src/json/json.h"
#include "../../ext_src/json/json-builder.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MCP PROTOCOL CONSTANTS
// ============================================================================

#define MCP_MAX_TOOL_NAME 128       /**< Maximum MCP tool name length */
#define MCP_MAX_ERROR_MESSAGE 512   /**< Maximum error message length */
#define MCP_PROTOCOL_VERSION "1.0"  /**< MCP protocol version */
#define MCP_MAX_BATCH_SIZE 100      /**< Maximum batch operation size */

// ============================================================================
// MCP DATA STRUCTURES
// ============================================================================

/**
 * MCP error codes
 */
typedef enum {
    MCP_SUCCESS = 0,                /**< Operation successful */
    MCP_ERROR_INVALID_TOOL = -1001, /**< Unknown tool name */
    MCP_ERROR_INVALID_PARAMS = -1002, /**< Invalid parameters */
    MCP_ERROR_INTERNAL = -1003,     /**< Internal error */
    MCP_ERROR_NOT_IMPLEMENTED = -1004, /**< Tool not implemented */
    MCP_ERROR_TRANSLATION = -1005,  /**< Protocol translation error */
    MCP_ERROR_OUT_OF_MEMORY = -1006, /**< Memory allocation failed */
    MCP_ERROR_BATCH_TOO_LARGE = -1007, /**< Batch size exceeds limit */
} mcp_error_code_t;

/**
 * MCP tool request structure
 */
typedef struct {
    char *tool;                     /**< Tool name (allocated) */
    json_value *arguments;          /**< Tool arguments (owned) */
    void *context;                  /**< Optional context data */
} mcp_tool_request_t;

/**
 * MCP tool response structure
 */
typedef struct {
    bool success;                   /**< Operation success */
    json_value *content;            /**< Response content (owned) */
    mcp_error_code_t error_code;    /**< Error code if failed */
    char *error_message;            /**< Error message (allocated) */
} mcp_tool_response_t;

/**
 * Parameter mapping function type
 * Transforms MCP arguments to JSON-RPC params
 */
typedef json_rpc_result_t (*mcp_param_mapper_fn)(
    const json_value *mcp_args,
    json_value **jsonrpc_params
);

/**
 * Result mapping function type
 * Transforms JSON-RPC result to MCP content
 */
typedef mcp_error_code_t (*mcp_result_mapper_fn)(
    const json_value *jsonrpc_result,
    json_value **mcp_content
);

/**
 * Method mapping entry
 */
typedef struct {
    const char *mcp_tool;           /**< MCP tool name */
    const char *jsonrpc_method;     /**< JSON-RPC method name */
    mcp_param_mapper_fn param_mapper; /**< Parameter mapper (NULL for direct) */
    mcp_result_mapper_fn result_mapper; /**< Result mapper (NULL for direct) */
    const char *description;        /**< Tool description */
} mcp_method_mapping_t;

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================

/**
 * Initialize MCP handler subsystem
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_handler_init(void);

/**
 * Cleanup MCP handler subsystem
 */
void mcp_handler_cleanup(void);

/**
 * Check if MCP handler is initialized
 * @return true if initialized, false otherwise
 */
bool mcp_handler_is_initialized(void);

// ============================================================================
// PROTOCOL TRANSLATION
// ============================================================================

/**
 * Translate MCP tool request to JSON-RPC request
 * 
 * @param mcp_request MCP tool request
 * @param jsonrpc_request Output JSON-RPC request (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_translate_request(
    const mcp_tool_request_t *mcp_request,
    json_rpc_request_t **jsonrpc_request
);

/**
 * Translate JSON-RPC response to MCP tool response
 * 
 * @param jsonrpc_response JSON-RPC response
 * @param mcp_tool_name Original MCP tool name
 * @param mcp_response Output MCP response (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_translate_response(
    const json_rpc_response_t *jsonrpc_response,
    const char *mcp_tool_name,
    mcp_tool_response_t **mcp_response
);

/**
 * Handle MCP tool request directly (combines translation and execution)
 * 
 * @param mcp_request MCP tool request
 * @param mcp_response Output MCP response (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_handle_tool_request(
    const mcp_tool_request_t *mcp_request,
    mcp_tool_response_t **mcp_response
);

/**
 * Handle MCP tool request with explicit context (for concurrent processing)
 * 
 * @param mcp_request MCP tool request
 * @param goxel_context Dedicated Goxel context for this request
 * @param mcp_response Output MCP response (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_handle_tool_request_with_context(
    const mcp_tool_request_t *mcp_request,
    void *goxel_context,
    mcp_tool_response_t **mcp_response
);

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

/**
 * Handle batch MCP requests
 * 
 * @param requests Array of MCP requests
 * @param count Number of requests
 * @param responses Output array of responses (caller must free)
 * @return MCP_SUCCESS if all succeed, error code if any fail
 */
mcp_error_code_t mcp_handle_batch_requests(
    const mcp_tool_request_t *requests,
    size_t count,
    mcp_tool_response_t **responses
);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * Free MCP tool request
 * @param request Request to free (may be NULL)
 */
void mcp_free_request(mcp_tool_request_t *request);

/**
 * Free MCP tool response
 * @param response Response to free (may be NULL)
 */
void mcp_free_response(mcp_tool_response_t *response);

// ============================================================================
// DISCOVERY AND INTROSPECTION
// ============================================================================

/**
 * Get available MCP tools
 * @param count Output parameter for tool count
 * @return Array of tool names (do not free)
 */
const char **mcp_get_available_tools(size_t *count);

/**
 * Get tool description
 * @param tool_name Tool name
 * @return Tool description or NULL if not found
 */
const char *mcp_get_tool_description(const char *tool_name);

/**
 * Check if tool is available
 * @param tool_name Tool name
 * @return true if available, false otherwise
 */
bool mcp_is_tool_available(const char *tool_name);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Get human-readable error message
 * @param error_code Error code
 * @return Error message (do not free)
 */
const char *mcp_error_string(mcp_error_code_t error_code);

/**
 * Map JSON-RPC error to MCP error
 * @param jsonrpc_error JSON-RPC error code
 * @return Corresponding MCP error code
 */
mcp_error_code_t mcp_map_jsonrpc_error(int32_t jsonrpc_error);

// ============================================================================
// PARAMETER MAPPING HELPERS
// ============================================================================

/**
 * Standard parameter mappers for common transformations
 */

/**
 * Map file open parameters (rename path handling)
 */
json_rpc_result_t mcp_map_open_file_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params
);

/**
 * Map voxel position parameters (nested to flat)
 */
json_rpc_result_t mcp_map_voxel_position_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params
);

/**
 * Map color parameters (object to array)
 */
json_rpc_result_t mcp_map_color_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params
);

/**
 * Map batch voxel operations
 */
json_rpc_result_t mcp_map_batch_voxel_params(
    const json_value *mcp_args,
    json_value **jsonrpc_params
);

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

/**
 * MCP handler statistics
 */
typedef struct {
    uint64_t requests_translated;    /**< Total requests translated */
    uint64_t responses_translated;   /**< Total responses translated */
    uint64_t translation_errors;     /**< Translation error count */
    uint64_t total_translation_time_us; /**< Total time in microseconds */
    double avg_translation_time_us;  /**< Average translation time */
    uint64_t batch_requests;         /**< Total batch requests */
    uint64_t direct_translations;    /**< Direct translations (no mapping) */
    uint64_t mapped_translations;    /**< Translations with parameter mapping */
} mcp_handler_stats_t;

/**
 * Get handler statistics
 * @param stats Output statistics structure
 */
void mcp_get_handler_stats(mcp_handler_stats_t *stats);

/**
 * Reset handler statistics
 */
void mcp_reset_handler_stats(void);

// ============================================================================
// PARSING AND SERIALIZATION HELPERS
// ============================================================================

/**
 * Parse MCP request from JSON string
 * @param json_str JSON string
 * @param request Output request (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_parse_request(const char *json_str, mcp_tool_request_t **request);

/**
 * Serialize MCP response to JSON string
 * @param response Response to serialize
 * @param json_str Output JSON string (caller must free)
 * @return MCP_SUCCESS on success, error code on failure
 */
mcp_error_code_t mcp_serialize_response(const mcp_tool_response_t *response, char **json_str);

#ifdef __cplusplus
}
#endif

#endif // MCP_HANDLER_H