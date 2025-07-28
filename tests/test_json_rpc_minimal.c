/* Minimal JSON-RPC test to isolate the issue */

#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("Testing minimal JSON-RPC parsing...\n");
    
    // Test response parsing (which was crashing)
    const char *json = "{\"jsonrpc\":\"2.0\",\"result\":42,\"id\":1}";
    json_rpc_response_t *response = NULL;
    
    printf("Parsing response JSON: %s\n", json);
    
    json_rpc_result_t result = json_rpc_parse_response(json, &response);
    printf("Parse result: %d\n", result);
    
    if (result == JSON_RPC_SUCCESS && response) {
        printf("SUCCESS: Response parsed\n");
        printf("  has_result: %s\n", response->has_result ? "true" : "false");
        printf("  has_error: %s\n", response->has_error ? "true" : "false");
        printf("  ID type: %d\n", response->id.type);
        
        if (response->has_result && response->result) {
            printf("  Result type: %d\n", response->result->type);
            if (response->result->type == json_integer) {
                printf("  Result value: %lld\n", response->result->u.integer);
            }
        }
        
        json_rpc_free_response(response);
        printf("Response freed successfully\n");
    } else {
        printf("FAILED to parse response\n");
    }
    
    printf("Test completed\n");
    return 0;
}