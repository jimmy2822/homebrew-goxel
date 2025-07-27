# Method: `{METHOD_NAME}`

## Overview
{Brief description of what this method does}

## Synopsis
```json
{
  "jsonrpc": "2.0",
  "method": "{METHOD_NAME}",
  "params": {
    // parameters here
  },
  "id": 1
}
```

## Parameters

| Parameter | Type | Required | Description | Default |
|-----------|------|----------|-------------|---------|
| `param1` | string | Yes | Description of parameter | - |
| `param2` | integer | No | Description of parameter | 0 |
| `param3` | boolean | No | Description of parameter | false |

### Parameter Details

#### `param1`
- **Type**: string
- **Required**: Yes
- **Description**: Detailed explanation of this parameter
- **Valid Values**: Any valid string
- **Example**: `"example_value"`

## Return Value

### Success Response
```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "success",
    "data": {
      // response data
    }
  },
  "id": 1
}
```

### Response Fields
| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Operation status ("success" or "error") |
| `data` | object | Method-specific response data |

## Errors

| Error Code | Message | Description |
|------------|---------|-------------|
| -32602 | Invalid params | Parameter validation failed |
| -30001 | Project not found | No active project |
| -30005 | Operation failed | Generic operation failure |

## Examples

### Example 1: Basic Usage
```json
// Request
{
  "jsonrpc": "2.0",
  "method": "{METHOD_NAME}",
  "params": {
    "param1": "value1"
  },
  "id": 1
}

// Response
{
  "jsonrpc": "2.0",
  "result": {
    "status": "success",
    "data": {}
  },
  "id": 1
}
```

### Example 2: With Optional Parameters
```json
// Request
{
  "jsonrpc": "2.0",
  "method": "{METHOD_NAME}",
  "params": {
    "param1": "value1",
    "param2": 42,
    "param3": true
  },
  "id": 2
}
```

### Example 3: Error Handling
```json
// Request with invalid parameters
{
  "jsonrpc": "2.0",
  "method": "{METHOD_NAME}",
  "params": {
    "param1": 123  // Wrong type
  },
  "id": 3
}

// Error Response
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": "param1 must be a string"
  },
  "id": 3
}
```

## Client Examples

### TypeScript
```typescript
const result = await client.call('{METHOD_NAME}', {
  param1: 'value1',
  param2: 42
});
```

### Python
```python
result = client.{method_name}(
    param1='value1',
    param2=42
)
```

### CLI
```bash
goxel --headless {cli-command} value1 --param2 42
```

## Notes

- {Any special considerations}
- {Performance implications}
- {Related methods}

## See Also

- [`related_method1`](../api-reference.md#related_method1)
- [`related_method2`](../api-reference.md#related_method2)
- [Client Libraries](../client-libraries.md)

---

*Added in version: 14.6.0*