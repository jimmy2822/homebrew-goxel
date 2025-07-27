# Phase 1: Binary Unification Integration Plan
**Agent-1: Marcus Rivera**  
**Date: January 27, 2025**

## Executive Summary
This document outlines the plan for unifying the `goxel` GUI and `goxel-headless` CLI executables into a single binary that supports both modes via a `--headless` flag.

## Current Architecture Analysis

### 1. Entry Points
- **GUI Mode**: `src/main.c` - Uses GLFW/OpenGL for windowed rendering
- **Headless Mode**: `src/headless/main_cli.c` - Uses OSMesa for offscreen rendering

### 2. Build System
- **SConstruct**: Already has conditional logic for headless vs GUI builds
- **Source Selection**: Different source files compiled based on build flags
- **Platform Support**: Both modes support Linux, macOS, Windows

### 3. Key Dependencies
- **GUI Mode**: GLFW3, OpenGL, ImGui, native file dialogs
- **Headless Mode**: OSMesa (optional), CLI interface, headless renderer

### 4. Initialization Flow
- **GUI**: GLFW init → OpenGL context → goxel_init() → event loop
- **Headless**: CLI context → goxel_core_init() → command processing

## Integration Strategy

### Design Principles
1. **Minimal Code Duplication**: Share as much code as possible between modes
2. **Clean Separation**: Use compile-time flags for mode-specific code
3. **Runtime Detection**: Parse `--headless` flag early to select mode
4. **Backward Compatibility**: Support legacy `goxel-headless` via symlink

### Unified Architecture
```
main.c (unified entry)
├── parse_args() - detect --headless flag
├── if (headless_mode)
│   └── headless_main() - CLI initialization
└── else
    └── gui_main() - GLFW/GUI initialization
```

## Implementation Plan

### Task 1.1: Codebase Analysis (Day 1-2) ✓
- [x] Analyzed GUI and headless entry points
- [x] Identified shared vs mode-specific code
- [x] Documented build system configuration
- [x] Created integration roadmap

### Task 1.2: Mode Detection Implementation (Day 3-4)
1. **Create Unified Main Entry**
   - Merge `src/main.c` and `src/headless/main_cli.c`
   - Add early `--headless` flag detection
   - Branch to appropriate initialization

2. **Argument Parsing**
   - Unify command-line parsing logic
   - Support both GUI and CLI arguments
   - Handle mode-specific options correctly

3. **Initialization Branching**
   ```c
   int main(int argc, char **argv) {
       bool headless = detect_headless_mode(argc, argv);
       
       if (headless) {
           return headless_main(argc, argv);
       } else {
           return gui_main(argc, argv);
       }
   }
   ```

### Task 1.3: Build System Unification (Day 5-7)
1. **Update SConstruct**
   - Remove separate headless/gui build targets
   - Create single `goxel` target with both modes
   - Add `UNIFIED_BUILD` define
   - Conditional compilation for mode-specific code

2. **Source File Organization**
   - Always include core functionality
   - Conditionally compile GUI-only code with `#ifndef HEADLESS_MODE`
   - Conditionally compile CLI-only code with `#ifdef HEADLESS_MODE`

3. **Dependency Management**
   - Link all required libraries for both modes
   - Use dlopen/dlsym for optional dependencies (OSMesa)
   - Handle missing dependencies gracefully

### Task 1.4: Initial Integration Testing (Day 8-10)
1. **Compilation Tests**
   - Verify unified binary builds on all platforms
   - Check binary size and dependencies

2. **Mode Switching Tests**
   - Test `goxel` (GUI mode)
   - Test `goxel --headless` (CLI mode)
   - Test legacy symlink compatibility

3. **Regression Testing**
   - Ensure GUI functionality unchanged
   - Verify all CLI commands work
   - Check performance impact

## Technical Considerations

### 1. Shared Code
- Core voxel engine (`src/core/`)
- File I/O and formats (`src/formats/`)
- Math and utility functions (`src/utils/`)

### 2. Mode-Specific Code
- **GUI Only**: ImGui panels, GLFW input, OpenGL rendering
- **CLI Only**: Command parsing, OSMesa rendering, batch processing

### 3. Conditional Compilation
```c
#ifdef HEADLESS_MODE
    // CLI-specific code
#else
    // GUI-specific code
#endif
```

### 4. Runtime Mode Detection
```c
bool detect_headless_mode(int argc, char **argv) {
    // Check for --headless flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            return true;
        }
    }
    
    // Check if invoked as goxel-headless (symlink)
    const char *progname = basename(argv[0]);
    if (strstr(progname, "headless") != NULL) {
        return true;
    }
    
    return false;
}
```

## Integration Points

### With Agent-2 (Aisha - Daemon Architecture)
- Provide hooks for daemon initialization in headless mode
- Support persistent process mode for daemon
- Coordinate on socket/IPC implementation

### With Agent-4 (James - Testing)
- Update test framework to test both modes
- Create mode-specific test suites
- Ensure CI tests unified binary

## Success Criteria
1. Single `goxel` binary supports both GUI and headless modes
2. `--headless` flag correctly switches to CLI mode
3. No regression in existing functionality
4. Binary size increase < 10%
5. Clean, maintainable code structure

## Next Steps
1. Begin Task 1.2 - Implement mode detection and unified main entry
2. Create feature branch for integration work
3. Set up test harness for both modes