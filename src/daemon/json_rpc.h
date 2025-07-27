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

#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// JSON-RPC 2.0 SPECIFICATION CONSTANTS
// ============================================================================

#define JSON_RPC_VERSION "2.0"          /**< JSON-RPC protocol version */
#define JSON_RPC_MAX_METHOD_NAME 128    /**< Maximum method name length */
#define JSON_RPC_MAX_ERROR_MESSAGE 512  /**< Maximum error message length */

// ============================================================================
// JSON-RPC 2.0 ERROR CODES (per specification)
// ============================================================================

/**
 * Standard JSON-RPC 2.0 error codes as defined in the specification.
 */
typedef enum {
    // Pre-defined errors
    JSON_RPC_PARSE_ERROR = -32700,      /**< Invalid JSON was received */
    JSON_RPC_INVALID_REQUEST = -32600,  /**< JSON sent is not valid Request object */
    JSON_RPC_METHOD_NOT_FOUND = -32601, /**< Method does not exist / is not available */
    JSON_RPC_INVALID_PARAMS = -32602,   /**< Invalid method parameter(s) */
    JSON_RPC_INTERNAL_ERROR = -32603,   /**< Internal JSON-RPC error */
    
    // Server error range -32000 to -32099 (implementation defined)
    JSON_RPC_SERVER_ERROR_START = -32099,
    JSON_RPC_SERVER_ERROR_END = -32000,
    
    // Application specific errors (outside reserved range)
    JSON_RPC_APPLICATION_ERROR = -1000  /**< Application-specific error base */
} json_rpc_error_code_t;

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

/**
 * JSON-RPC error codes for library operations.
 */
typedef enum {
    JSON_RPC_SUCCESS = 0,               /**< Operation completed successfully */
    JSON_RPC_ERROR_INVALID_PARAMETER,   /**< Invalid parameter provided */
    JSON_RPC_ERROR_OUT_OF_MEMORY,       /**< Memory allocation failed */
    JSON_RPC_ERROR_PARSE_FAILED,        /**< JSON parsing failed */
    JSON_RPC_ERROR_INVALID_JSON,        /**< Invalid JSON structure */
    JSON_RPC_ERROR_MISSING_FIELD,       /**< Required field missing */
    JSON_RPC_ERROR_INVALID_VERSION,     /**< Unsupported JSON-RPC version */
    JSON_RPC_ERROR_BUFFER_TOO_SMALL,    /**< Output buffer too small */
    JSON_RPC_ERROR_UNKNOWN = -1         /**< Unknown error */
} json_rpc_result_t;

/**
 * JSON-RPC request/response ID types.
 */
typedef enum {
    JSON_RPC_ID_NULL,                   /**< Null ID (notification) */
    JSON_RPC_ID_NUMBER,                 /**< Numeric ID */
    JSON_RPC_ID_STRING                  /**< String ID */
} json_rpc_id_type_t;

/**
 * JSON-RPC ID union for different ID types.
 */
typedef union {
    int64_t number;                     /**< Numeric ID value */
    char *string;                       /**< String ID value (allocated) */
} json_rpc_id_value_t;

/**
 * JSON-RPC ID structure.
 */
typedef struct {
    json_rpc_id_type_t type;            /**< ID type */
    json_rpc_id_value_t value;          /**< ID value */
} json_rpc_id_t;

/**
 * JSON-RPC parameter types.
 */
typedef enum {
    JSON_RPC_PARAMS_NONE,               /**< No parameters */
    JSON_RPC_PARAMS_ARRAY,              /**< Positional parameters (array) */
    JSON_RPC_PARAMS_OBJECT              /**< Named parameters (object) */
} json_rpc_params_type_t;

/**
 * Forward declarations for JSON structures.
 * Use the external JSON library's type directly.
 */
#include "../../ext_src/json/json.h"

/**
 * JSON-RPC parameters structure.
 */
typedef struct {
    json_rpc_params_type_t type;        /**< Parameter type */
    json_value *data;                   /**< Parameter data (owned) */
} json_rpc_params_t;

/**
 * JSON-RPC error structure.
 */
typedef struct {
    int32_t code;                       /**< Error code */
    char *message;                      /**< Error message (allocated) */
    json_value *data;                   /**< Additional error data (optional, owned) */
} json_rpc_error_t;

/**
 * JSON-RPC request structure.
 */
typedef struct {
    char *method;                       /**< Method name (allocated) */
    json_rpc_params_t params;           /**< Method parameters */
    json_rpc_id_t id;                   /**< Request ID */
    bool is_notification;               /**< True if notification (no response expected) */
} json_rpc_request_t;

/**
 * JSON-RPC response structure.
 */
typedef struct {
    json_rpc_id_t id;                   /**< Response ID (matches request) */
    bool has_result;                    /**< True if response has result */
    bool has_error;                     /**< True if response has error */
    json_value *result;                 /**< Response result (owned) */
    json_rpc_error_t error;             /**< Response error */
} json_rpc_response_t;

// ============================================================================
// PARSING AND SERIALIZATION
// ============================================================================

/**
 * Parses a JSON-RPC request from JSON string.
 * 
 * @param json_str JSON string to parse
 * @param request Pointer to store parsed request (caller must free)
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_parse_request(const char *json_str,
                                        json_rpc_request_t **request);

/**
 * Parses a JSON-RPC response from JSON string.
 * 
 * @param json_str JSON string to parse
 * @param response Pointer to store parsed response (caller must free)
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_parse_response(const char *json_str,
                                         json_rpc_response_t **response);

/**
 * Serializes a JSON-RPC request to JSON string.
 * 
 * @param request Request to serialize
 * @param json_str Pointer to store JSON string (caller must free)
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_serialize_request(const json_rpc_request_t *request,
                                            char **json_str);

/**
 * Serializes a JSON-RPC response to JSON string.
 * 
 * @param response Response to serialize
 * @param json_str Pointer to store JSON string (caller must free)
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_serialize_response(const json_rpc_response_t *response,
                                             char **json_str);

// ============================================================================
// REQUEST/RESPONSE CREATION
// ============================================================================

/**
 * Creates a new JSON-RPC request with positional parameters.
 * 
 * @param method Method name
 * @param params_array JSON array of parameters (will be owned by request)
 * @param id Request ID (will be copied)
 * @return New request instance, or NULL on failure
 */
json_rpc_request_t *json_rpc_create_request_array(const char *method,
                                                 json_value *params_array,
                                                 const json_rpc_id_t *id);

/**
 * Creates a new JSON-RPC request with named parameters.
 * 
 * @param method Method name
 * @param params_object JSON object of parameters (will be owned by request)
 * @param id Request ID (will be copied)
 * @return New request instance, or NULL on failure
 */
json_rpc_request_t *json_rpc_create_request_object(const char *method,
                                                  json_value *params_object,
                                                  const json_rpc_id_t *id);

/**
 * Creates a new JSON-RPC notification (request without ID).
 * 
 * @param method Method name
 * @param params Parameters (array or object, will be owned by request)
 * @param is_array True if params is array, false if object
 * @return New request instance, or NULL on failure
 */
json_rpc_request_t *json_rpc_create_notification(const char *method,
                                                json_value *params,
                                                bool is_array);

/**
 * Creates a new JSON-RPC success response.
 * 
 * @param result Result data (will be owned by response)
 * @param id Response ID (will be copied)
 * @return New response instance, or NULL on failure
 */
json_rpc_response_t *json_rpc_create_response_result(json_value *result,
                                                    const json_rpc_id_t *id);

/**
 * Creates a new JSON-RPC error response.
 * 
 * @param error_code Error code
 * @param error_message Error message
 * @param error_data Additional error data (optional, will be owned by response)
 * @param id Response ID (will be copied)
 * @return New response instance, or NULL on failure
 */
json_rpc_response_t *json_rpc_create_response_error(int32_t error_code,
                                                   const char *error_message,
                                                   json_value *error_data,
                                                   const json_rpc_id_t *id);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * Frees a JSON-RPC request and all associated resources.
 * 
 * @param request Request to free (may be NULL)
 */
void json_rpc_free_request(json_rpc_request_t *request);

/**
 * Frees a JSON-RPC response and all associated resources.
 * 
 * @param response Response to free (may be NULL)
 */
void json_rpc_free_response(json_rpc_response_t *response);

/**
 * Clones a JSON-RPC ID structure.
 * 
 * @param src Source ID to clone
 * @param dst Destination ID structure
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_clone_id(const json_rpc_id_t *src, json_rpc_id_t *dst);

/**
 * Frees resources associated with a JSON-RPC ID.
 * 
 * @param id ID structure to clean up
 */
void json_rpc_free_id(json_rpc_id_t *id);

// ============================================================================
// PARAMETER ACCESS HELPERS
// ============================================================================

/**
 * Gets a parameter by index from array parameters.
 * 
 * @param params Parameter structure
 * @param index Parameter index
 * @param value Pointer to store parameter value
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_get_param_by_index(const json_rpc_params_t *params,
                                              int index, json_value **value);

/**
 * Gets a parameter by name from object parameters.
 * 
 * @param params Parameter structure
 * @param name Parameter name
 * @param value Pointer to store parameter value
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_get_param_by_name(const json_rpc_params_t *params,
                                             const char *name, json_value **value);

/**
 * Gets the number of parameters.
 * 
 * @param params Parameter structure
 * @return Number of parameters, or -1 on error
 */
int json_rpc_get_param_count(const json_rpc_params_t *params);

/**
 * Checks if parameters are valid for the given type.
 * 
 * @param params Parameter structure
 * @return true if valid, false otherwise
 */
bool json_rpc_params_valid(const json_rpc_params_t *params);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * Validates a JSON-RPC request structure.
 * 
 * @param request Request to validate
 * @return JSON_RPC_SUCCESS if valid, error code otherwise
 */
json_rpc_result_t json_rpc_validate_request(const json_rpc_request_t *request);

/**
 * Validates a JSON-RPC response structure.
 * 
 * @param response Response to validate
 * @return JSON_RPC_SUCCESS if valid, error code otherwise
 */
json_rpc_result_t json_rpc_validate_response(const json_rpc_response_t *response);

/**
 * Validates a JSON-RPC ID.
 * 
 * @param id ID to validate
 * @return JSON_RPC_SUCCESS if valid, error code otherwise
 */
json_rpc_result_t json_rpc_validate_id(const json_rpc_id_t *id);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Creates a numeric ID.
 * 
 * @param number Numeric value
 * @param id ID structure to initialize
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_create_id_number(int64_t number, json_rpc_id_t *id);

/**
 * Creates a string ID.
 * 
 * @param string String value (will be copied)
 * @param id ID structure to initialize
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_create_id_string(const char *string, json_rpc_id_t *id);

/**
 * Creates a null ID (for notifications).
 * 
 * @param id ID structure to initialize
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_create_id_null(json_rpc_id_t *id);

/**
 * Compares two JSON-RPC IDs for equality.
 * 
 * @param id1 First ID
 * @param id2 Second ID
 * @return true if equal, false otherwise
 */
bool json_rpc_id_equals(const json_rpc_id_t *id1, const json_rpc_id_t *id2);

/**
 * Gets a human-readable error message for a result code.
 * 
 * @param result Result code
 * @return Pointer to error message string (do not free)
 */
const char *json_rpc_result_string(json_rpc_result_t result);

/**
 * Gets the standard error message for a JSON-RPC error code.
 * 
 * @param error_code Error code
 * @return Pointer to error message string (do not free)
 */
const char *json_rpc_error_message(int32_t error_code);

/**
 * Checks if an error code is in the server error range.
 * 
 * @param error_code Error code to check
 * @return true if server error, false otherwise
 */
bool json_rpc_is_server_error(int32_t error_code);

/**
 * Checks if an error code is application-defined.
 * 
 * @param error_code Error code to check
 * @return true if application error, false otherwise
 */
bool json_rpc_is_application_error(int32_t error_code);

// ============================================================================
// GOXEL API METHOD HANDLING
// ============================================================================

/**
 * Initializes the Goxel context for API operations.
 * Must be called before any Goxel API methods can be used.
 * 
 * @return JSON_RPC_SUCCESS on success, error code on failure
 */
json_rpc_result_t json_rpc_init_goxel_context(void);

/**
 * Cleans up the Goxel context and releases resources.
 */
void json_rpc_cleanup_goxel_context(void);

/**
 * Handles a JSON-RPC method call by dispatching to the appropriate handler.
 * 
 * @param request JSON-RPC request to handle
 * @return JSON-RPC response (caller must free with json_rpc_free_response)
 */
json_rpc_response_t *json_rpc_handle_method(const json_rpc_request_t *request);

/**
 * Lists all available Goxel API methods with descriptions.
 * 
 * @param buffer Buffer to write method list to
 * @param buffer_size Size of the buffer
 * @return 0 on success, -1 if buffer too small
 */
int json_rpc_list_methods(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // JSON_RPC_H