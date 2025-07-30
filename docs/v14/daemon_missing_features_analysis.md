# Goxel v14 Daemon Missing Features Analysis

## Executive Summary

The Goxel v14 daemon currently lacks two critical features for color processing operations:
1. **Script Execution Support** - No JSON-RPC method to execute JavaScript scripts
2. **Bulk Voxel Reading** - Limited to single voxel queries, no efficient bulk reading

## Current State Analysis

### Existing Daemon Methods (19 total)

#### File Operations
- `goxel.create_project` - Create new voxel project
- `goxel.load_project` - Load project from file  
- `goxel.save_project` - Save project to file
- `goxel.export_model` - Export to various formats
- `goxel.render_scene` - Render scene to image

#### Voxel Operations
- `goxel.add_voxel` - Add single voxel
- `goxel.remove_voxel` - Remove single voxel
- `goxel.get_voxel` - **Get single voxel (EXISTS but inefficient for bulk)**
- `goxel.paint_voxels` - Paint existing voxels
- `goxel.flood_fill` - Fill connected voxels
- `goxel.procedural_shape` - Generate shapes
- `goxel.batch_operations` - Batch voxel operations

#### Layer Management
- `goxel.list_layers` - List all layers
- `goxel.create_layer` - Create new layer
- `goxel.delete_layer` - Delete layer
- `goxel.merge_layers` - Merge layers
- `goxel.set_layer_visibility` - Show/hide layer

#### System Operations
- `goxel.get_status` - Get current status

### JavaScript/QuickJS Support Status

**Main Application**: ✅ Full QuickJS integration exists
- `src/script.c` - Script execution engine
- `src/script.h` - Public API with:
  - `script_run_from_file()`
  - `script_run_from_string()`
  - `script_execute()`
- `src/quickjs.c` - QuickJS bindings

**Daemon**: ❌ No script execution methods exposed via JSON-RPC

### Voxel Reading Capabilities

**Core Engine**: ✅ Full voxel iteration support
- `volume_get_at()` - Get voxel at position
- `volume_get_iterator()` - Create iterator
- `volume_iter()` - Iterate through voxels
- `volume_get_bbox()` - Get bounding box

**Daemon API**: ⚠️ Limited to single voxel queries
- `goxel.get_voxel` - Only gets one voxel at a time
- No bulk reading methods
- No iteration support
- No bounding box queries

## Missing Features for Color Processing

### 1. Script Execution Methods (Priority: HIGH)

Missing JSON-RPC methods:
- `goxel.execute_script` - Run script from string
- `goxel.execute_script_file` - Run script from file
- `goxel.list_scripts` - List available scripts

Benefits:
- Reuse existing color processing scripts
- Complex algorithms without daemon changes
- User-defined processing logic

### 2. Bulk Voxel Reading Methods (Priority: HIGH)

Missing JSON-RPC methods:
- `goxel.get_voxels_region` - Get all voxels in a box region
- `goxel.get_layer_voxels` - Get all voxels in a layer
- `goxel.get_voxel_count` - Count voxels with optional filter
- `goxel.get_bounding_box` - Get layer/project bounds

Benefits:
- Efficient color analysis
- Histogram generation
- Pattern detection
- Batch processing

### 3. Color Analysis Methods (Priority: MEDIUM) ✅ IMPLEMENTED

~~Missing~~ **Implemented** JSON-RPC methods:
- ✅ `goxel.get_color_histogram` - Color distribution analysis
- ✅ `goxel.find_voxels_by_color` - Find all voxels matching color
- ✅ `goxel.get_unique_colors` - List all unique colors used

**Implementation Details:**
- Non-blocking execution using worker threads
- Hash-based efficient color counting
- Support for color binning and similarity matching
- Per-channel tolerance for color searching
- Full documentation in `docs/v14/color_analysis_api.md`
- Test suite in `tests/integration/test_color_analysis.py`

## Implementation Plan

### Phase 1: Script Execution Support (Week 1)

1. **Add Script Execution Handler** (`handle_goxel_execute_script`)
   - Accept script string or file path
   - Execute via existing `script_run_from_string()`
   - Return execution result/output

2. **Add Script Management Methods**
   - List available scripts
   - Load script from assets

3. **Security Considerations**
   - Sandbox script execution
   - Resource limits
   - Safe API exposure

### Phase 2: Bulk Voxel Reading (Week 2)

1. **Add Region Reading** (`handle_goxel_get_voxels_region`)
   - Accept min/max coordinates
   - Use `volume_get_box_iterator()`
   - Return array of voxels with positions

2. **Add Layer Iteration** (`handle_goxel_get_layer_voxels`)
   - Accept layer ID
   - Use `volume_get_iterator()`
   - Optional color filtering

3. **Optimize for Large Datasets**
   - Streaming/pagination support
   - Compression options
   - Memory limits

### Phase 3: Color Analysis (Week 3)

1. **Built-in Analysis Methods**
   - Histogram generation
   - Color counting
   - Pattern matching

2. **Performance Optimization**
   - Parallel processing
   - Caching results
   - Incremental updates

## Alternative Approaches

### Option 1: Extend Existing Methods
- Add `bulk` parameter to `get_voxel`
- Add `script` parameter to operations
- Pros: Fewer new methods
- Cons: Complex parameter handling

### Option 2: Plugin System
- Load dynamic libraries
- Custom processing modules
- Pros: Extensible
- Cons: Complex implementation

### Option 3: Direct Memory Access
- Expose shared memory regions
- External processes read directly
- Pros: Maximum performance
- Cons: Platform-specific, security risks

## Recommendation

Implement **Phase 1 (Script Execution)** and **Phase 2 (Bulk Reading)** as priority. This provides:

1. **Immediate Solution**: Users can run color processing scripts
2. **Performance**: Bulk reading for efficient analysis
3. **Flexibility**: Custom algorithms without daemon changes
4. **Compatibility**: Reuse existing scripts

## Technical Requirements

### Dependencies
- No new dependencies (QuickJS already integrated)
- Existing volume iteration APIs
- JSON array/object handling

### API Compatibility
- Maintain backward compatibility
- Follow existing method naming
- Consistent parameter formats

### Performance Targets
- Script execution: <100ms startup
- Bulk reading: >100k voxels/second
- Memory usage: <100MB for 1M voxels

## Conclusion

The daemon has strong foundations but lacks critical features for color processing. Adding script execution and bulk voxel reading would enable full feature parity with the GUI application while maintaining the daemon's performance advantages.

**Estimated Development Time**: 3 weeks
**Risk Level**: Low (reusing existing code)
**Impact**: High (enables all color processing workflows)