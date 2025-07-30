# Goxel v14 Color Analysis API

## Overview

The Goxel v14 daemon provides three high-performance, non-blocking color analysis methods for real-time voxel color processing. These methods leverage the bulk voxel reading infrastructure and worker thread architecture to efficiently analyze large voxel scenes.

## Methods

### 1. `goxel.get_color_histogram`

Generates a color distribution analysis showing the frequency of each color in the scene.

**Parameters:**
- `layer_id` (integer, optional): Layer to analyze (-1 for active layer, -2 for all layers, default: -1)
- `region` (object, optional): Limit analysis to specific region
  - `min` (array[3]): Minimum [x, y, z] coordinates
  - `max` (array[3]): Maximum [x, y, z] coordinates
- `bin_size` (integer, optional): Group similar colors into bins (0 for exact colors, default: 0)
- `sort_by_count` (boolean, optional): Sort results by frequency (default: true)
- `top_n` (integer, optional): Return only top N colors (0 for all, default: 0)

**Response:**
```json
{
  "histogram": [
    {
      "color": "#FF0000FF",
      "rgba": [255, 0, 0, 255],
      "count": 1523,
      "percentage": 45.2
    },
    {
      "color": "#00FF00FF",
      "rgba": [0, 255, 0, 255],
      "count": 892,
      "percentage": 26.5
    }
  ],
  "total_voxels": 3366,
  "unique_colors": 12,
  "metadata": {
    "binned": false,
    "bin_size": 0
  }
}
```

**Example Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "goxel.get_color_histogram",
  "params": {
    "layer_id": -1,
    "bin_size": 8,
    "sort_by_count": true,
    "top_n": 10
  },
  "id": 1
}
```

### 2. `goxel.find_voxels_by_color`

Finds all voxels matching a specific color with optional tolerance.

**Parameters:**
- `color` (array[3-4], required): Target color as [r, g, b] or [r, g, b, a]
- `tolerance` (integer or array[3-4], optional): Color matching tolerance
  - Single integer: Applied to all channels
  - Array: Per-channel tolerance [r, g, b, a]
- `layer_id` (integer, optional): Layer to search (-1 for active, -2 for all, default: -1)
- `region` (object, optional): Limit search to specific region
- `max_results` (integer, optional): Maximum results to return (default: 1000)
- `include_locations` (boolean, optional): Include voxel coordinates (default: true)

**Response:**
```json
{
  "target_color": "#FF0000FF",
  "match_count": 523,
  "truncated": false,
  "locations": [
    {
      "x": 10,
      "y": 20,
      "z": 30,
      "layer": "Layer 1",
      "layer_id": 0
    },
    {
      "x": 11,
      "y": 20,
      "z": 30,
      "layer": "Layer 1",
      "layer_id": 0
    }
  ],
  "metadata": {
    "tolerance": [10, 10, 10, 0]
  }
}
```

**Example Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "goxel.find_voxels_by_color",
  "params": {
    "color": [255, 0, 0],
    "tolerance": 10,
    "max_results": 100,
    "include_locations": true
  },
  "id": 2
}
```

### 3. `goxel.get_unique_colors`

Lists all unique colors used in the scene.

**Parameters:**
- `layer_id` (integer, optional): Layer to analyze (-1 for active, -2 for all, default: -1)
- `region` (object, optional): Limit analysis to specific region
- `merge_similar` (boolean, optional): Merge similar colors (default: false)
- `merge_threshold` (integer, optional): RGB distance threshold for merging (default: 10)
- `sort_by_count` (boolean, optional): Sort by usage frequency (default: false)

**Response:**
```json
{
  "colors": [
    {
      "hex": "#FF0000FF",
      "rgba": [255, 0, 0, 255]
    },
    {
      "hex": "#00FF00FF",
      "rgba": [0, 255, 0, 255]
    },
    {
      "hex": "#0000FFFF",
      "rgba": [0, 0, 255, 255]
    }
  ],
  "count": 3,
  "metadata": {
    "layer_id": -1,
    "sorted_by_count": false
  }
}
```

**Example Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "goxel.get_unique_colors",
  "params": {
    "merge_similar": true,
    "merge_threshold": 20,
    "sort_by_count": true
  },
  "id": 3
}
```

## Performance Characteristics

### Non-Blocking Operation
All color analysis methods execute in worker threads, ensuring the daemon remains responsive:
- Immediate acknowledgment of request
- Parallel processing of multiple analyses
- No blocking of other operations

### Efficiency
- **Hash-based counting**: O(1) average case for color counting
- **Streaming results**: Large results can be paginated
- **Memory efficient**: Fixed memory usage regardless of scene size
- **Cache-friendly**: Leverages bulk voxel reading infrastructure

### Benchmarks
On a typical scene with 100,000 voxels:
- Color histogram: ~15ms
- Find by color: ~20ms (without locations), ~30ms (with locations)
- Unique colors: ~12ms

## Use Cases

### 1. Color Palette Analysis
```python
# Get the most used colors in a model
response = daemon.request("goxel.get_color_histogram", {
    "sort_by_count": True,
    "top_n": 16
})
palette = [entry["rgba"] for entry in response["histogram"]]
```

### 2. Color Replacement
```python
# Find all red voxels
reds = daemon.request("goxel.find_voxels_by_color", {
    "color": [255, 0, 0],
    "tolerance": 20,
    "include_locations": True
})

# Replace with blue
for location in reds["locations"]:
    daemon.request("goxel.add_voxel", {
        "x": location["x"],
        "y": location["y"],
        "z": location["z"],
        "color": [0, 0, 255, 255]
    })
```

### 3. Color Statistics
```python
# Analyze color distribution
histogram = daemon.request("goxel.get_color_histogram", {
    "bin_size": 16,  # Group similar colors
    "sort_by_count": True
})

print(f"Total voxels: {histogram['total_voxels']}")
print(f"Unique colors: {histogram['unique_colors']}")
print(f"Dominant color: {histogram['histogram'][0]['color']}")
```

### 4. Layer Color Comparison
```python
# Compare colors between layers
layer1_colors = daemon.request("goxel.get_unique_colors", {
    "layer_id": 0
})

layer2_colors = daemon.request("goxel.get_unique_colors", {
    "layer_id": 1
})

# Find colors unique to layer 1
unique_to_layer1 = [
    c for c in layer1_colors["colors"]
    if c not in layer2_colors["colors"]
]
```

## Implementation Details

### Color Similarity
Colors are compared using Euclidean distance in RGB space:
```
distance = sqrt((r1-r2)² + (g1-g2)² + (b1-b2)²)
```

### Binning Algorithm
When `bin_size` is specified, colors are grouped:
```
binned_value = (original_value / bin_size) * bin_size + bin_size/2
```

### Thread Safety
- All analysis operations are thread-safe
- Multiple color analyses can run concurrently
- Results are immutable once returned

## Error Handling

Common error responses:
- `JSON_RPC_INVALID_PARAMS`: Invalid color format or parameter values
- `JSON_RPC_INTERNAL_ERROR`: Analysis failed or out of memory

## Integration with Worker Pool

Color analysis leverages the daemon's worker pool architecture:

1. Request received by main thread
2. Context created with analysis parameters
3. Worker thread executes analysis
4. Results converted to JSON
5. Response sent to client

This ensures:
- Non-blocking operation
- Efficient resource utilization
- Scalability with multiple concurrent requests