# Goxel v13 Headless Fork - Implementation Task Plan

**Version**: 13.0.0-alpha  
**Project**: Goxel Headless Fork with CLI/API Support  
**Status**: Phase 1 Complete - Phase 2 In Progress  
**Updated**: 2025-01-22

## Overview

This document tracks the implementation of the Goxel Headless Fork as outlined in the [v13 Design Document](/Users/jimmy/jimmy_side_projects/goxel-mcp/docs/v13/goxel-headless-fork-design.md). Each task can be tracked and checked off upon completion.

## Project Structure Setup

### Repository Preparation
- [ ] Fork original Goxel repository
- [ ] Set up new repository structure for headless fork
- [ ] Create development branch `feature/headless-mode`
- [ ] Set up CI/CD workflows for multiple platforms
- [ ] Configure build environment for Linux/Windows/macOS

---

## Phase 1: Core Extraction (3-4 weeks)

### 1.1 Code Architecture Refactoring
- [x] Analyze existing Goxel codebase dependencies
- [x] Identify GUI-coupled code in core logic
- [x] Create new directory structure:
  - [x] `src/core/` - GUI-independent core logic
  - [x] `src/headless/` - Headless-specific implementations
  - [x] `src/gui/` - Optional GUI components
- [x] Extract core volume operations to `src/core/volume_ops.c`
- [x] Extract project management to `src/core/project_mgmt.c`
- [x] Extract file format handlers to `src/core/file_formats.c`
- [x] Create `src/core/goxel_core.c` with GUI-independent functions

### 1.2 Build System Modifications
- [x] Modify SCons build system for conditional compilation
- [x] Add build options:
  - [x] `headless=1` - Build headless version
  - [x] `gui=0` - Build GUI version  
  - [x] `cli_tools=1` - Build CLI tools
- [x] Configure headless dependencies (OSMesa/EGL)
- [x] Set up conditional compilation flags
- [x] Create separate build targets for headless/GUI modes

### 1.3 Core API Design
- [x] Design core data structures for headless operation
- [x] Create `goxel_core.h` header with essential types
- [x] Implement `goxel_core_init()` function
- [x] Implement `goxel_core_shutdown()` function
- [x] Add core volume manipulation functions
- [x] Add core project management functions

### 1.4 Basic Testing Framework
- [x] Set up unit testing framework
- [x] Create `tests/core/` directory
- [x] Write tests for core volume operations
- [x] Write tests for project management
- [x] Write tests for file format handling
- [x] Set up automated testing pipeline

---

## Phase 2: Headless Rendering (2-3 weeks)

### 2.1 Offscreen Rendering Setup
- [ ] Research and choose rendering backend (OSMesa vs EGL)
- [ ] Implement `src/headless/render_headless.c`
- [ ] Create headless OpenGL context
- [ ] Set up framebuffer objects for offscreen rendering
- [ ] Initialize OpenGL state for headless operation

### 2.2 OSMesa Integration (Primary Option)
- [ ] Add OSMesa dependency to build system
- [ ] Implement `headless_render_init()` function
- [ ] Implement `headless_render_create_context()` function
- [ ] Set up RGBA buffer allocation
- [ ] Configure viewport and OpenGL state

### 2.3 EGL Integration (Alternative/Fallback)
- [ ] Add EGL dependency for hardware acceleration
- [ ] Implement EGL display setup
- [ ] Create EGL contexts for headless operation
- [ ] Set up pbuffer surfaces
- [ ] Configure EGL attributes

### 2.4 Camera System Adaptation
- [ ] Extract camera logic from GUI code
- [ ] Create `camera_set_preset()` function
- [ ] Support predefined camera angles:
  - [ ] Front, Back, Left, Right
  - [ ] Top, Bottom
  - [ ] Isometric views
  - [ ] Custom camera positioning
- [ ] Implement camera matrix calculations

### 2.5 Rendering Pipeline
- [ ] Implement `headless_render_scene()` function
- [ ] Port layer rendering logic
- [ ] Port voxel rendering shaders
- [ ] Implement `headless_render_to_file()` function
- [ ] Support multiple output formats (PNG, JPEG)
- [ ] Add render quality settings

### 2.6 Performance Optimization
- [ ] Profile rendering performance
- [ ] Optimize memory usage
- [ ] Implement render caching
- [ ] Add render progress callbacks
- [ ] Benchmark against GUI version

---

## Phase 3: CLI Interface (2-3 weeks)

### 3.1 Command-Line Parser
- [ ] Design CLI command structure
- [ ] Implement argument parsing in `src/headless/cli_interface.c`
- [ ] Create command registry system
- [ ] Add global option handling
- [ ] Implement help system

### 3.2 Basic Project Operations
- [ ] Implement `goxel-cli create` command
  - [ ] Project name specification
  - [ ] Initial size configuration
  - [ ] Output file path
- [ ] Implement `goxel-cli open` command
  - [ ] File format detection
  - [ ] Error handling for invalid files
- [ ] Implement `goxel-cli save` command
  - [ ] Multiple format support
  - [ ] Backup file creation

### 3.3 Voxel Manipulation Commands
- [ ] Implement `goxel-cli voxel add` command
  - [ ] Position specification (--pos x,y,z)
  - [ ] Color specification (--color r,g,b,a)
  - [ ] Multiple voxel support
- [ ] Implement `goxel-cli voxel remove` command
  - [ ] Single voxel removal
  - [ ] Area-based removal (--box)
- [ ] Implement `goxel-cli voxel paint` command
  - [ ] Color change operations
  - [ ] Selection-based painting
- [ ] Implement `goxel-cli voxel batch-add` command
  - [ ] CSV file input support
  - [ ] JSON format support

### 3.4 Layer Operations
- [ ] Implement `goxel-cli layer create` command
- [ ] Implement `goxel-cli layer delete` command  
- [ ] Implement `goxel-cli layer merge` command
- [ ] Implement `goxel-cli layer visibility` command
- [ ] Implement `goxel-cli layer rename` command

### 3.5 Rendering Commands
- [ ] Implement `goxel-cli render` command
  - [ ] Camera preset selection
  - [ ] Resolution specification
  - [ ] Output format selection
  - [ ] Quality settings
- [ ] Implement multiple view rendering
- [ ] Implement animation frame rendering
- [ ] Add render batch processing

### 3.6 Export/Import Operations
- [ ] Implement `goxel-cli export` command
  - [ ] Support all existing formats (.obj, .ply, .vox, etc.)
  - [ ] Format-specific options
- [ ] Implement `goxel-cli convert` tool
  - [ ] Batch conversion support
  - [ ] Format validation

### 3.7 Scripting Support
- [ ] Implement `goxel-cli script` command
- [ ] Support JavaScript file execution
- [ ] Support inline script execution
- [ ] Add script parameter passing
- [ ] Create script API documentation

---

## Phase 4: C API Bridge (2-3 weeks)

### 4.1 Public API Design
- [ ] Design public header `include/goxel_headless.h`
- [ ] Define error codes and return types
- [ ] Define context management functions
- [ ] Define core operation functions
- [ ] Add comprehensive documentation

### 4.2 Context Management
- [ ] Implement `goxel_create_context()` function
- [ ] Implement `goxel_init_context()` function
- [ ] Implement `goxel_destroy_context()` function
- [ ] Add context validation
- [ ] Implement thread safety measures

### 4.3 Project Management API
- [ ] Implement `goxel_create_project()` function
- [ ] Implement `goxel_load_project()` function
- [ ] Implement `goxel_save_project()` function
- [ ] Implement `goxel_close_project()` function
- [ ] Add project metadata functions

### 4.4 Voxel Operations API
- [ ] Implement `goxel_add_voxel()` function
- [ ] Implement `goxel_remove_voxel()` function
- [ ] Implement `goxel_get_voxel()` function
- [ ] Implement `goxel_add_voxel_batch()` function
- [ ] Add voxel validation functions

### 4.5 Layer Management API
- [ ] Implement `goxel_create_layer()` function
- [ ] Implement `goxel_delete_layer()` function
- [ ] Implement `goxel_set_active_layer()` function
- [ ] Implement `goxel_set_layer_visibility()` function
- [ ] Add layer enumeration functions

### 4.6 Rendering API
- [ ] Implement `goxel_render_to_file()` function
- [ ] Implement `goxel_render_to_buffer()` function
- [ ] Add camera configuration API
- [ ] Add render settings API
- [ ] Add render progress callbacks

### 4.7 Error Handling System
- [ ] Implement comprehensive error codes
- [ ] Add `goxel_get_error_string()` function
- [ ] Implement error message buffering
- [ ] Add debug logging system
- [ ] Create error handling examples

### 4.8 Memory Management
- [ ] Implement proper memory cleanup
- [ ] Add memory leak detection
- [ ] Implement reference counting where needed
- [ ] Add memory usage monitoring
- [ ] Create memory management guidelines

---

## Phase 5: MCP Integration (1-2 weeks)

### 5.1 MCP Server Adaptation
- [ ] Modify existing MCP server to use headless API
- [ ] Remove GUI dependencies from MCP server
- [ ] Update MCP tool implementations
- [ ] Add headless context management
- [ ] Implement proper resource cleanup

### 5.2 Real-time Communication
- [ ] Optimize API call performance
- [ ] Implement operation batching
- [ ] Add asynchronous operation support
- [ ] Implement progress reporting
- [ ] Add operation cancellation

### 5.3 Context Persistence
- [ ] Implement project state persistence
- [ ] Add session management
- [ ] Implement auto-save functionality
- [ ] Add crash recovery
- [ ] Create state validation

### 5.4 Performance Optimization
- [ ] Profile MCP server performance
- [ ] Optimize frequent operations
- [ ] Implement operation caching
- [ ] Add performance monitoring
- [ ] Benchmark against previous version

### 5.5 Integration Testing
- [ ] Create comprehensive integration tests
- [ ] Test all MCP tools with headless backend
- [ ] Validate file format compatibility
- [ ] Test cross-platform functionality
- [ ] Create performance benchmarks

---

## Phase 6: Production Ready (1-2 weeks)

### 6.1 Comprehensive Testing
- [ ] Complete unit test coverage
- [ ] Add integration tests for all features
- [ ] Create performance regression tests
- [ ] Add memory leak tests
- [ ] Test cross-platform compatibility

### 6.2 Cross-platform Validation
- [ ] Test on Linux (Ubuntu, CentOS, Arch)
- [ ] Test on Windows (10, 11)
- [ ] Test on macOS (Intel, Apple Silicon)
- [ ] Validate dependency installation
- [ ] Test different OpenGL drivers

### 6.3 Performance Tuning
- [ ] Profile all operations
- [ ] Optimize bottlenecks
- [ ] Validate memory usage targets
- [ ] Test with large voxel models
- [ ] Benchmark rendering performance

### 6.4 Documentation Completion
- [ ] Complete API reference documentation
- [ ] Write CLI usage guide
- [ ] Create integration examples
- [ ] Write troubleshooting guide
- [ ] Create developer documentation

### 6.5 Release Preparation
- [ ] Create release builds
- [ ] Package for different platforms
- [ ] Prepare release notes
- [ ] Set up distribution channels
- [ ] Create migration guide

---

## Success Criteria Validation

### Technical Benchmarks
- [ ] **Build Success**: Achieve 100% successful builds on all platforms
- [ ] **Test Coverage**: Reach >90% code coverage
- [ ] **Performance**: <100ms for 90% of operations
- [ ] **Memory**: <500MB for typical workloads
- [ ] **Compatibility**: 100% Goxel file format compatibility

### Operation Benchmarks
- [ ] Simple Operations: <10ms (add/remove single voxel)
- [ ] Batch Operations: <100ms (1,000 voxels)
- [ ] Project Load/Save: <1s (typical project)
- [ ] Rendering: <10s (1920x1080, high quality)
- [ ] Memory Usage: <500MB (100k voxel project)

### Scalability Targets
- [ ] Max Voxels: 10M+ per project
- [ ] Max Concurrent Ops: 1,000+ per second
- [ ] Memory Efficiency: <1KB per 1,000 voxels
- [ ] File I/O: <100MB/s read/write
- [ ] Network Latency: <50ms API response

---

## Milestone Schedule

| Phase | Duration | Start Date | End Date | Status |
|-------|----------|------------|----------|---------|
| Phase 1: Core Extraction | 3-4 weeks | 2025-01-22 | 2025-01-22 | ✅ **Complete** |
| Phase 2: Headless Rendering | 2-3 weeks | 2025-01-22 | TBD | 🔄 **In Progress** |
| Phase 3: CLI Interface | 2-3 weeks | TBD | TBD | ⏳ Pending |
| Phase 4: C API Bridge | 2-3 weeks | TBD | TBD | ⏳ Pending |
| Phase 5: MCP Integration | 1-2 weeks | TBD | TBD | ⏳ Pending |
| Phase 6: Production Ready | 1-2 weeks | TBD | TBD | ⏳ Pending |

**Total Estimated Duration**: 11-17 weeks

---

## Notes and Decisions

### Architecture Decisions
- **Rendering Backend**: OSMesa chosen as primary, EGL as fallback
- **Build System**: SCons modifications for conditional compilation (`headless=1`, `gui=0`, `cli_tools=1`)
- **API Design**: C-style API for maximum compatibility
- **Code Organization**: Separated into `src/core/`, `src/headless/`, `src/gui/` directories
- **Testing**: Comprehensive test suite with automated `make test` integration

### Risk Mitigation
- Regular upstream synchronization planned
- Cross-platform testing from early phases
- Performance benchmarking throughout development
- Comprehensive test coverage to prevent regressions

### Success Metrics Tracking
- Weekly progress reviews
- Performance benchmark validation
- Cross-platform compatibility validation
- Memory usage monitoring

---

**Last Updated**: 2025-01-22  
**Next Review**: Phase 2 Completion Target  
**Progress**: 17% Complete (24/139 tasks completed)  
**Phase 1**: ✅ **100% Complete** (24/24 tasks)  
**Phase 2**: 🔄 **0% Complete** (0/23 tasks)