/* Test the exact serialization sequence that's failing */

#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("Testing response serialization sequence...\n");
    
    // Create success response (like in the failing test)
    json_value *result_data = json_string_new("success");
    printf("Created result data\n");
    
    json_rpc_id_t id;
    json_rpc_create_id_string("test-id", &id);
    printf("Created ID\n");
    
    json_rpc_response_t *response = json_rpc_create_response_result(result_data, &id);
    if (!response) {
        printf("FAILED to create response\n");
        return 1;
    }
    printf("Created response\n");
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    printf("Serialization result: %d\n", result);
    
    if (result == JSON_RPC_SUCCESS && json_str) {
        printf("Serialized JSON: %s\n", json_str);
        
        // Parse back to verify (this might be where it crashes)
        printf("Parsing back...\n");
        json_rpc_response_t *parsed = NULL;
        result = json_rpc_parse_response(json_str, &parsed);
        printf("Parse back result: %d\n", result);
        
        if (result == JSON_RPC_SUCCESS && parsed) {
            printf("Parse back successful\n");
            printf("  has_result: %s\n", parsed->has_result ? "true" : "false");
            printf("  ID type: %d\n", parsed->id.type);
            if (parsed->id.type == JSON_RPC_ID_STRING) {
                printf("  ID value: %s\n", parsed->id.value.string);
            }
            
            printf("Freeing parsed response...\n");
            json_rpc_free_response(parsed);
            printf("Parsed response freed\n");
        } else {
            printf("Parse back failed\n");
        }
        
        printf("Freeing JSON string...\n");
        free(json_str);
        printf("JSON string freed\n");
    } else {
        printf("Serialization failed\n");
    }
    
    printf("Freeing original response...\n");
    json_rpc_free_response(response);
    printf("Original response freed\n");
    
    printf("Freeing ID...\n");
    json_rpc_free_id(&id);
    printf("ID freed\n");
    
    printf("Test completed successfully\n");
    return 0;
}