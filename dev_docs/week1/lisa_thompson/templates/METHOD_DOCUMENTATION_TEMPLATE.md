# Method Documentation Template

## method.name

**Description**: Brief one-line description of what this method does.

**Since**: Version X.Y.Z

### Request

**Protocol**: MCP | JSON-RPC

**Parameters**:

| Parameter | Type | Required | Description | Default |
|-----------|------|----------|-------------|---------|
| param1 | string | Yes | Description of param1 | - |
| param2 | integer | No | Description of param2 | 0 |
| param3 | object | No | Description of param3 | {} |

**Example Request**:

```json
{
    "jsonrpc": "2.0",
    "method": "method.name",
    "params": {
        "param1": "value1",
        "param2": 42,
        "param3": {
            "field1": "value"
        }
    },
    "id": 1
}
```

### Response

**Success Response**:

```json
{
    "jsonrpc": "2.0",
    "result": {
        "status": "success",
        "data": {
            "field1": "value1",
            "field2": 123
        }
    },
    "id": 1
}
```

**Response Fields**:

| Field | Type | Description |
|-------|------|-------------|
| status | string | Operation status |
| data | object | Result data |
| data.field1 | string | Description |
| data.field2 | integer | Description |

### Errors

| Error Code | Description | When It Occurs |
|------------|-------------|----------------|
| -32600 | Invalid Request | Malformed request |
| -32601 | Method not found | Unknown method |
| -32602 | Invalid params | Parameter validation failed |
| 1001 | Custom error | Specific condition |

**Error Response Example**:

```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32602,
        "message": "Invalid params",
        "data": {
            "field": "param1",
            "reason": "Cannot be empty"
        }
    },
    "id": 1
}
```

### Implementation Notes

**Performance**: O(n) where n is the number of elements
**Memory**: Allocates temporary buffer of size X
**Thread Safety**: Thread-safe / Not thread-safe

### Code Example

```c
// C API usage
result_t* res = method_name("value1", 42, NULL);
if (res->status == SUCCESS) {
    printf("Result: %s\n", res->data.field1);
}
method_result_free(res);
```

```typescript
// TypeScript client usage
const result = await client.methodName({
    param1: "value1",
    param2: 42
});
console.log(result.data.field1);
```

### Related Methods

- [related.method1](related_method1.md) - Does something similar
- [related.method2](related_method2.md) - Often used together

### Changelog

| Version | Changes |
|---------|---------|
| 15.0.0 | Initial implementation |
| 15.1.0 | Added param3 support |

---

**Category**: [Category Name]  
**Last Updated**: YYYY-MM-DD  
**Author**: Name