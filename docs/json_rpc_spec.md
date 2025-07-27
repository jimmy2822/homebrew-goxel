# JSON-RPC 2.0 Specification Implementation

## Overview

This document describes the JSON-RPC 2.0 specification implementation for the Goxel v14.0 Daemon Architecture. The implementation provides a complete, standards-compliant JSON-RPC 2.0 parser and generator foundation.

## Standards Compliance

This implementation fully conforms to the [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification) including:

- **Protocol Version**: Strict "2.0" version checking
- **Request Format**: Support for both positional and named parameters
- **Response Format**: Proper result/error response handling  
- **Error Codes**: All standard error codes (-32768 to -32000 range)
- **Notifications**: Requests without ID (no response expected)
- **ID Types**: Support for numeric, string, and null IDs

## API Reference

### Core Data Structures

#### Request Structure
```c
typedef struct {
    char *method;                       // Method name (allocated)
    json_rpc_params_t params;           // Method parameters
    json_rpc_id_t id;                   // Request ID
    bool is_notification;               // True if notification
} json_rpc_request_t;
```

#### Response Structure
```c
typedef struct {
    json_rpc_id_t id;                   // Response ID (matches request)
    bool has_result;                    // True if response has result
    bool has_error;                     // True if response has error
    json_value *result;                 // Response result (owned)
    json_rpc_error_t error;             // Response error
} json_rpc_response_t;
```

#### Error Structure
```c
typedef struct {
    int32_t code;                       // Error code
    char *message;                      // Error message (allocated)
    json_value *data;                   // Additional error data (optional)
} json_rpc_error_t;
```

### Standard Error Codes

| Code | Message | Description |
|------|---------|-------------|
| -32700 | Parse error | Invalid JSON was received |
| -32600 | Invalid Request | JSON sent is not valid Request object |
| -32601 | Method not found | Method does not exist / is not available |
| -32602 | Invalid params | Invalid method parameter(s) |
| -32603 | Internal error | Internal JSON-RPC error |
| -32000 to -32099 | Server error | Implementation defined server errors |

### Parsing Functions

#### Parse Request
```c
json_rpc_result_t json_rpc_parse_request(const char *json_str,
                                        json_rpc_request_t **request);
```
Parses a JSON-RPC request from JSON string.

**Parameters:**
- `json_str`: JSON string to parse
- `request`: Pointer to store parsed request (caller must free)

**Returns:** `JSON_RPC_SUCCESS` on success, error code on failure

#### Parse Response
```c
json_rpc_result_t json_rpc_parse_response(const char *json_str,
                                         json_rpc_response_t **response);
```
Parses a JSON-RPC response from JSON string.

### Serialization Functions

#### Serialize Request
```c
json_rpc_result_t json_rpc_serialize_request(const json_rpc_request_t *request,
                                            char **json_str);
```
Serializes a JSON-RPC request to JSON string.

#### Serialize Response
```c
json_rpc_result_t json_rpc_serialize_response(const json_rpc_response_t *response,
                                             char **json_str);
```
Serializes a JSON-RPC response to JSON string.

### Creation Functions

#### Create Request with Array Parameters
```c
json_rpc_request_t *json_rpc_create_request_array(const char *method,
                                                 json_value *params_array,
                                                 const json_rpc_id_t *id);
```

#### Create Request with Object Parameters
```c
json_rpc_request_t *json_rpc_create_request_object(const char *method,
                                                  json_value *params_object,
                                                  const json_rpc_id_t *id);
```

#### Create Notification
```c
json_rpc_request_t *json_rpc_create_notification(const char *method,
                                                json_value *params,
                                                bool is_array);
```

#### Create Success Response
```c
json_rpc_response_t *json_rpc_create_response_result(json_value *result,
                                                    const json_rpc_id_t *id);
```

#### Create Error Response
```c
json_rpc_response_t *json_rpc_create_response_error(int32_t error_code,
                                                   const char *error_message,
                                                   json_value *error_data,
                                                   const json_rpc_id_t *id);
```

### Parameter Access Helpers

#### Get Parameter by Index
```c
json_rpc_result_t json_rpc_get_param_by_index(const json_rpc_params_t *params,
                                              int index, json_value **value);
```
Gets a parameter by index from array parameters.

#### Get Parameter by Name
```c
json_rpc_result_t json_rpc_get_param_by_name(const json_rpc_params_t *params,
                                             const char *name, json_value **value);
```
Gets a parameter by name from object parameters.

#### Get Parameter Count
```c
int json_rpc_get_param_count(const json_rpc_params_t *params);
```
Gets the number of parameters.

### ID Management

#### Create Numeric ID
```c
json_rpc_result_t json_rpc_create_id_number(int64_t number, json_rpc_id_t *id);
```

#### Create String ID
```c
json_rpc_result_t json_rpc_create_id_string(const char *string, json_rpc_id_t *id);
```

#### Create Null ID
```c
json_rpc_result_t json_rpc_create_id_null(json_rpc_id_t *id);
```

#### Clone ID
```c
json_rpc_result_t json_rpc_clone_id(const json_rpc_id_t *src, json_rpc_id_t *dst);
```

#### Compare IDs
```c
bool json_rpc_id_equals(const json_rpc_id_t *id1, const json_rpc_id_t *id2);
```

### Memory Management

#### Free Request
```c
void json_rpc_free_request(json_rpc_request_t *request);
```
Frees a JSON-RPC request and all associated resources.

#### Free Response
```c
void json_rpc_free_response(json_rpc_response_t *response);
```
Frees a JSON-RPC response and all associated resources.

#### Free ID
```c
void json_rpc_free_id(json_rpc_id_t *id);
```
Frees resources associated with a JSON-RPC ID.

### Validation Functions

#### Validate Request
```c
json_rpc_result_t json_rpc_validate_request(const json_rpc_request_t *request);
```
Validates a JSON-RPC request structure.

#### Validate Response
```c
json_rpc_result_t json_rpc_validate_response(const json_rpc_response_t *response);
```
Validates a JSON-RPC response structure.

## Usage Examples

### Basic Request Parsing
```c
const char *json = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":[1,2,3],\"id\":1}";
json_rpc_request_t *request = NULL;

json_rpc_result_t result = json_rpc_parse_request(json, &request);
if (result == JSON_RPC_SUCCESS) {
    printf("Method: %s\n", request->method);
    printf("Param count: %d\n", json_rpc_get_param_count(&request->params));
    
    json_rpc_free_request(request);
}
```

### Creating and Serializing Response
```c
// Create success response
json_value *result_data = json_string_new("Hello, World!");
json_rpc_id_t id;
json_rpc_create_id_number(42, &id);

json_rpc_response_t *response = json_rpc_create_response_result(result_data, &id);

// Serialize to JSON
char *json_str = NULL;
json_rpc_serialize_response(response, &json_str);
printf("Response: %s\n", json_str);

// Cleanup
json_rpc_free_response(response);
json_rpc_free_id(&id);
free(json_str);
```

### Error Handling
```c
// Create error response
json_rpc_id_t id;
json_rpc_create_id_string("req-123", &id);

json_rpc_response_t *error_response = json_rpc_create_response_error(
    JSON_RPC_METHOD_NOT_FOUND, 
    "Method not found", 
    NULL, 
    &id
);

char *error_json = NULL;
json_rpc_serialize_response(error_response, &error_json);
printf("Error: %s\n", error_json);

json_rpc_free_response(error_response);
json_rpc_free_id(&id);
free(error_json);
```

### Parameter Access
```c
// Array parameters
json_value *param = NULL;
json_rpc_get_param_by_index(&request->params, 0, &param);
if (param && param->type == json_string) {
    printf("First param: %s\n", param->u.string.ptr);
}

// Object parameters  
json_rpc_get_param_by_name(&request->params, "username", &param);
if (param && param->type == json_string) {
    printf("Username: %s\n", param->u.string.ptr);
}
```

## Error Handling

The implementation uses a comprehensive error handling system:

### Result Codes
- `JSON_RPC_SUCCESS`: Operation completed successfully
- `JSON_RPC_ERROR_INVALID_PARAMETER`: Invalid parameter provided
- `JSON_RPC_ERROR_OUT_OF_MEMORY`: Memory allocation failed
- `JSON_RPC_ERROR_PARSE_FAILED`: JSON parsing failed
- `JSON_RPC_ERROR_INVALID_JSON`: Invalid JSON structure
- `JSON_RPC_ERROR_MISSING_FIELD`: Required field missing
- `JSON_RPC_ERROR_INVALID_VERSION`: Unsupported JSON-RPC version
- `JSON_RPC_ERROR_BUFFER_TOO_SMALL`: Output buffer too small

### Error Messages
```c
const char *json_rpc_result_string(json_rpc_result_t result);
const char *json_rpc_error_message(int32_t error_code);
```

### Error Classification
```c
bool json_rpc_is_server_error(int32_t error_code);
bool json_rpc_is_application_error(int32_t error_code);
```

## Implementation Notes

### Memory Management
- All structures use proper memory allocation and cleanup
- JSON values are properly cloned to avoid use-after-free issues
- String parameters are duplicated to ensure ownership
- Comprehensive cleanup functions prevent memory leaks

### Thread Safety
- The implementation is stateless and thread-safe
- No global variables or shared state
- Each operation is independent

### Performance Considerations
- Efficient JSON parsing using the existing JSON library
- Minimal memory allocations
- Fast parameter access helpers
- Optimized validation functions

### Limitations
- Single request/response processing (no batch support in current version)
- Maximum method name length: 128 characters
- Maximum error message length: 512 characters

## Building and Testing

### Build Requirements
- C99 compiler with GNU extensions
- External JSON parser and builder libraries (included in `ext_src/json/`)

### Build Commands
```bash
# Build test executable
make -f tests/Makefile.daemon

# Run tests
make -f tests/Makefile.daemon test

# Run with memory checking
make -f tests/Makefile.daemon test-valgrind
```

### Test Coverage
The implementation includes comprehensive tests covering:
- All standard error codes
- Request/response parsing and serialization
- Parameter access (positional and named)
- ID management (numeric, string, null)
- Memory management and cleanup
- Edge cases and boundary conditions
- Specification compliance verification

## Future Enhancements

Planned improvements for future versions:
- Batch request/response support
- Streaming JSON parser for large payloads
- Custom memory allocator support
- Performance optimizations
- Additional validation options

## References

- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [JSON Parser Library Documentation](../ext_src/json/)
- [Goxel v14.0 Daemon Architecture](../GOXEL_V14_VERSION_PLAN.md)