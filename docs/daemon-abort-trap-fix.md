# Daemon Abort Trap Fix - Root Cause Analysis

## Issue Description

The Goxel daemon was experiencing an abort trap crash when processing JSON-RPC requests. The crash occurred during request cleanup, specifically when freeing the request structure.

## Root Cause

The abort trap was caused by a **use-after-free bug** in the JSON-RPC request parsing logic.

### The Problem Flow

1. **JSON Parsing**: When `json_rpc_parse_request()` was called, it parsed the incoming JSON string into a `json_value` tree structure (stored in `root`).

2. **Parameter Storage**: The `parse_params_from_json()` function was storing direct pointers to the parameter data within the `root` JSON tree:
   ```c
   // BEFORE FIX - Storing pointer to data owned by root
   if (json_params->type == json_array) {
       params->type = JSON_RPC_PARAMS_ARRAY;
       params->data = json_params;  // <-- Just storing pointer!
       return JSON_RPC_SUCCESS;
   }
   ```

3. **Premature Cleanup**: In `daemon_main.c`, immediately after parsing the request, the JSON string was freed:
   ```c
   json_rpc_result_t parse_result = json_rpc_parse_request(json_str, &rpc_request);
   free(json_str);  // <-- This happens while request still holds pointers into root
   ```

4. **Root Cleanup**: Inside `json_rpc_parse_request()`, after extracting data, the root JSON structure was freed:
   ```c
   json_value_free(root);  // <-- This frees the data that params.data points to!
   ```

5. **Double Free**: Later, when `json_rpc_free_request()` was called, it tried to free `params.data`:
   ```c
   if (request->params.data) {
       json_value_free(request->params.data);  // <-- CRASH! This memory was already freed
   }
   ```

## The Fix

The solution was to **clone the parameter data** immediately when parsing, ensuring the request owns its own copy:

```c
// AFTER FIX - Cloning the data
if (json_params->type == json_array) {
    params->type = JSON_RPC_PARAMS_ARRAY;
    // Clone the params to avoid use-after-free when root is freed
    params->data = clone_json_value(json_params);
    if (!params->data) {
        return JSON_RPC_ERROR_OUT_OF_MEMORY;
    }
    return JSON_RPC_SUCCESS;
}
```

## Key Lessons

1. **Ownership is Critical**: When parsing complex data structures, be clear about who owns what memory. The request should own all its data, not just hold pointers into temporary parse trees.

2. **Clone Early**: If data needs to survive beyond the lifetime of its source, clone it immediately during parsing rather than trying to fix it later.

3. **Defensive Cleanup**: The attempt to null out `params.data` in the cleanup path was a band-aid that couldn't fix the fundamental ownership issue.

## Testing

After the fix, all 217 TDD tests pass successfully, confirming the daemon can now handle JSON-RPC requests without crashing.

## Files Modified

- `/src/daemon/json_rpc.c`: Modified `parse_params_from_json()` to clone parameters
- Removed redundant cloning logic that was attempting to fix the issue post-parsing
- Updated cleanup logic to handle properly owned data

## Prevention

To prevent similar issues:

1. Always clone data that needs to outlive its source
2. Document ownership clearly in function comments
3. Use valgrind or address sanitizer during development to catch use-after-free bugs early
4. Write comprehensive tests that exercise cleanup paths