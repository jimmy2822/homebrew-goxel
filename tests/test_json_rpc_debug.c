/* Simple debug test for JSON-RPC serialization */

#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("Testing response serialization...\n");
    
    // Create success response
    json_value *result_data = json_string_new("success");
    json_rpc_id_t id;
    json_rpc_create_id_string("test-id", &id);
    
    printf("Created result data and ID\n");
    
    json_rpc_response_t *response = json_rpc_create_response_result(result_data, &id);
    if (!response) {
        printf("FAIL: Could not create response\n");
        return 1;
    }
    
    printf("Created response structure\n");
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    
    printf("Serialization result: %d\n", result);
    
    if (result == JSON_RPC_SUCCESS && json_str) {
        printf("Serialized JSON: %s\n", json_str);
        free(json_str);
    } else {
        printf("FAIL: Serialization failed\n");
    }
    
    json_rpc_free_response(response);
    json_rpc_free_id(&id);
    
    printf("Test completed\n");
    return 0;
}