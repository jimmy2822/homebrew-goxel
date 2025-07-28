/* Test just the response serialization alone */

#include "../src/daemon/json_rpc.h"
#include "../ext_src/json/json-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: Expected %d, got %d\n", (int)(expected), (int)(actual)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (!expected && !actual) break; \
        if (!expected || !actual || strcmp(expected, actual) != 0) { \
            printf("FAIL: Expected '%s', got '%s'\n", \
                   expected ? expected : "(null)", \
                   actual ? actual : "(null)"); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            printf("FAIL: Expected non-null pointer\n"); \
            exit(1); \
        } \
    } while(0)

int main(void)
{
    printf("Testing single response serialization...\n");
    
    // Create success response
    json_value *result_data = json_string_new("success");
    printf("1. Created result data\n");
    
    json_rpc_id_t id;
    json_rpc_create_id_string("test-id", &id);
    printf("2. Created ID\n");
    
    json_rpc_response_t *response = json_rpc_create_response_result(result_data, &id);
    ASSERT_NOT_NULL(response);
    printf("3. Created response\n");
    
    char *json_str = NULL;
    json_rpc_result_t result = json_rpc_serialize_response(response, &json_str);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_NOT_NULL(json_str);
    printf("4. Serialized response: %s\n", json_str);
    
    // Parse back to verify
    json_rpc_response_t *parsed = NULL;
    result = json_rpc_parse_response(json_str, &parsed);
    ASSERT_EQ(JSON_RPC_SUCCESS, result);
    ASSERT_EQ(true, parsed->has_result);
    ASSERT_STR_EQ("test-id", parsed->id.value.string);
    printf("5. Parsed back successfully\n");
    
    json_rpc_free_response(response);
    printf("6. Freed original response\n");
    
    json_rpc_free_response(parsed);
    printf("7. Freed parsed response\n");
    
    json_rpc_free_id(&id);
    printf("8. Freed ID\n");
    
    free(json_str);
    printf("9. Freed JSON string\n");
    
    printf("SUCCESS: Response serialization test passed!\n");
    return 0;
}