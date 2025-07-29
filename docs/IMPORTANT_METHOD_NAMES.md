# IMPORTANT: Goxel Daemon JSON-RPC Method Names

## Correct Method Name Format

All Goxel daemon JSON-RPC methods MUST use the `goxel.` prefix. Methods without this prefix will return a "Method not found" error.

### ✅ CORRECT Method Names

- `goxel.create_project`
- `goxel.load_project`
- `goxel.save_project`
- `goxel.add_voxel`
- `goxel.remove_voxel`
- `goxel.get_voxel`
- `goxel.export_model`
- `goxel.get_status`
- `goxel.list_layers`
- `goxel.create_layer`
- `goxel.get_version`
- `goxel.render_scene`

### ❌ INCORRECT Method Names (These will NOT work)

- `create_project` → Use `goxel.create_project`
- `add_voxel` → Use `goxel.add_voxel`
- `save_project` → Use `goxel.save_project`
- `export_model` → Use `goxel.export_model`

## Example Usage

### Correct Request:
```json
{
  "jsonrpc": "2.0",
  "method": "goxel.create_project",
  "params": {"name": "my_project"},
  "id": 1
}
```

### Incorrect Request (will fail):
```json
{
  "jsonrpc": "2.0",
  "method": "create_project",
  "params": {"name": "my_project"},
  "id": 1
}
```

### Error Response for Incorrect Method:
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found: create_project"
  },
  "id": 1
}
```

## Why the Prefix?

The `goxel.` prefix is used to:
1. Follow JSON-RPC naming conventions for namespaced methods
2. Avoid conflicts with built-in RPC methods (like `echo`, `ping`)
3. Allow future extensibility with other method namespaces
4. Make it clear these are Goxel-specific methods

## Client Library Examples

### Python:
```python
# CORRECT
client.call('goxel.create_project', {'name': 'test'})

# INCORRECT
client.call('create_project', {'name': 'test'})  # Will fail!
```

### JavaScript/TypeScript:
```javascript
// CORRECT
await client.request('goxel.add_voxel', {x: 0, y: 0, z: 0, color: [255, 0, 0, 255]});

// INCORRECT
await client.request('add_voxel', {x: 0, y: 0, z: 0, color: [255, 0, 0, 255]});  // Will fail!
```

### Command Line (nc):
```bash
# CORRECT
echo '{"jsonrpc":"2.0","method":"goxel.create_project","params":{"name":"test"},"id":1}' | nc -U /tmp/goxel.sock

# INCORRECT
echo '{"jsonrpc":"2.0","method":"create_project","params":{"name":"test"},"id":1}' | nc -U /tmp/goxel.sock
```

## Troubleshooting

If you're getting "Method not found" errors:
1. Check that you're using the `goxel.` prefix
2. Verify the exact spelling of the method name
3. Use `goxel.get_status` to test connectivity (it requires no parameters)
4. Check the daemon logs for more details

## Full Method List

See [daemon_api_reference.md](v14/daemon_api_reference.md) for the complete list of available methods and their parameters.

---
Last Updated: January 2025