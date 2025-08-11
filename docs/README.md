# Goxel Documentation

## Goxel v0.16.0 Documentation

This directory contains comprehensive documentation for Goxel v0.16.0, featuring the revolutionary file-path render transfer architecture that delivers 90% memory reduction and 50% faster transfers while maintaining 100% backward compatibility.

### 🎉 v0.16.0 Documentation (Latest)

1. **[v16 Overview](v16/README.md)**  
   Revolutionary file-path render transfer architecture

2. **[API Reference](v16/API_REFERENCE.md)**  
   Complete JSON-RPC API documentation (29 methods including render management)

3. **[Migration Guide](v16/MIGRATION_GUIDE.md)**  
   Zero-breaking-change upgrade from v0.15 to v0.16

4. **[Release Notes](v16/RELEASE_NOTES.md)**  
   What's new in v0.16.0

5. **[Architecture Document](v16-render-transfer-milestone.md)**  
   Technical specification of the render transfer system

### 📚 v0.15.3 Documentation (Previous Stable)

1. **[v15.3 Status Report](v15-daemon-status.md)**  
   Production-ready status with save_project fix

2. **[v15 Overview](v15/README.md)**  
   Connection reuse and performance improvements

3. **[Quick Start Guide](v15/quick_start_guide.md)**  
   Get started with Goxel v15.3 daemon

4. **[API Reference](v15/daemon_api_reference.md)**  
   JSON-RPC API documentation (25 methods)

5. **[Migration Guide](v15/migration_guide.md)**  
   Upgrade from v14.x to v15.3

### 📚 Historical Documentation (v14.0)

1. **[v14.0 Memory Issues](daemon-memory-issue-summary.md)**  
   Analysis of v14.0 limitations (resolved in v15.0)

2. **[Architecture Analysis](daemon-memory-architecture-analysis.md)**  
   Technical analysis of v14.0 issues

### 🔧 Quick Start

Get started with Goxel v0.16.0 (latest):
1. Install via Homebrew: `brew install goxel`
2. Start daemon: `brew services start goxel`
3. Use new file-path render mode: See [v16 Overview](v16/README.md)
4. Read the [Migration Guide](v16/MIGRATION_GUIDE.md) for upgrade instructions
5. Check [API Reference](v16/API_REFERENCE.md) for all 29 methods

### 📈 Version History

- **v0.16.0**: 🎉 **LATEST** - File-path render transfer, 90% memory reduction
- **v0.15.3**: ✅ **STABLE** - save_project fix, all critical issues resolved
- **v0.15.2**: ✅ Connection reuse working, memory fixes
- **v0.15.1**: ✅ Performance improvements
- **v0.15.0**: ✅ Production-ready with full multi-request support
- **v0.14.0**: Historical - Single-operation limitation
- **v0.13.x**: Legacy versions without daemon support

### 🎉 Major Improvements (v0.16.0)
- **File-Path Render Transfer**: 90% memory reduction, 50% faster transfers
- **Automatic Cleanup**: TTL-based file management prevents disk exhaustion
- **New API Methods**: Four render management methods added
- **100% Backward Compatible**: All v0.15 code works unchanged

### Previous Fixes (v0.15.3)
- **Save_Project Hanging**: RESOLVED - Now responds instantly
- **OpenGL Preview**: Fixed daemon mode detection
- **Homebrew Package**: Updated with working binary

### 🏗️ Architecture Documents

- API documentation
- Protocol specifications  
- Deployment guides
- Performance tuning

### 🧪 Testing & Validation

- Test strategies
- Benchmark results
- Integration examples
- Troubleshooting guides

---

For the latest project status, see [CLAUDE.md](../CLAUDE.md) in the root directory.