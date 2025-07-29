# Goxel Simplified Architecture - Quick Reference Card

## 🎯 Project Goal
**From**: `[MCP Client] → [MCP Server] → [TypeScript Client] → [Goxel Daemon]`  
**To**: `[MCP Client] → [Goxel MCP-Daemon]`

## 📊 Key Metrics

| Metric | Current | Target | Improvement |
|--------|---------|--------|-------------|
| **Latency** | 5-10ms | <1ms | 80% ⬇️ |
| **Memory** | 125MB | 50MB | 60% ⬇️ |
| **Services** | 3 | 1 | 67% ⬇️ |
| **Throughput** | 5K ops/s | 10K ops/s | 100% ⬆️ |

## 🏗️ Architecture Changes

### Remove ❌
- Node.js MCP Server
- TypeScript Client Layer  
- GUI Components (ImGui, GLFW)
- 13 external libraries

### Keep ✅
- Core Voxel Engine
- File Format Support
- Headless Rendering
- Worker Pool

### Add 🆕
- Native MCP Handler
- Protocol Router
- libuv for async I/O

## 📁 Key File Locations

```
src/daemon/
├── mcp_handler.c     # NEW: MCP protocol
├── daemon_main.c     # MODIFY: Add MCP
├── socket_server.c   # MODIFY: Dual protocol
└── worker_pool.c     # KEEP: No changes

src/core/
├── goxel_core.c      # KEEP: Core engine
├── image.c           # KEEP: Layer system
└── volume.c          # KEEP: Voxel storage
```

## 🔧 Build Commands

```bash
# Current (v14)
scons daemon=1

# Target (Simplified)
scons mcp=1 headless=1

# Dual-mode (Transition)
scons mcp=1 jsonrpc=1 protocol=auto
```

## 🚀 Development Phases

1. **Week 1**: Analysis & Planning ✅
2. **Week 2-3**: MCP Integration 
3. **Week 3-4**: Feature Parity
4. **Week 5**: Migration Tools
5. **Week 6**: Cleanup & Launch

## 👥 Team Contacts

| Role | Name | Focus Area |
|------|------|------------|
| **Architecture** | Sarah Chen | MCP handler design |
| **Performance** | Michael Rodriguez | Optimization |
| **Testing** | Alex Kumar | Test framework |
| **Migration** | David Park | Compatibility |
| **Documentation** | Lisa Thompson | Wiki & guides |

## 🛠️ Common Commands

```bash
# Start daemon (current)
./goxel-daemon --socket /tmp/goxel.sock

# Start daemon (target)
./goxel-mcp-daemon --protocol=mcp

# Test connection
echo '{"method":"ping"}' | nc -U /tmp/goxel.sock

# Run benchmarks
./scripts/run_benchmarks.py
```

## 📈 Success Criteria

- ✅ <1ms latency for basic operations
- ✅ >10,000 operations per second
- ✅ <50MB memory footprint
- ✅ 100% feature parity
- ✅ Zero breaking changes (with compatibility layer)

## 🔍 Debugging

```bash
# Enable debug logging
export GOXEL_LOG_LEVEL=DEBUG

# Protocol trace
export GOXEL_TRACE_PROTOCOL=1

# Performance profiling
export GOXEL_PROFILE=1
```

## 📚 Documentation Links

- [Architecture Overview](wiki/01_ARCHITECTURE_OVERVIEW.md)
- [Dependencies](dependencies/DEPENDENCY_ANALYSIS.md)
- [Documentation Standards](standards/DOCUMENTATION_STANDARDS.md)
- [Glossary](wiki/GLOSSARY.md)

## ⚠️ Critical Paths

1. **MCP Protocol Parser** - Must be zero-copy
2. **Worker Pool Integration** - Preserve concurrency
3. **Memory Management** - No leaks allowed
4. **Backward Compatibility** - Dual-mode required

---

**Version**: 1.0 | **Updated**: 2025-01-29 | **Print**: Letter/A4