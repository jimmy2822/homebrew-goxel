# Goxel Documentation

## Goxel v15.3 Documentation

This directory contains comprehensive documentation for Goxel v15.3, which resolves all critical issues including the save_project hanging bug and provides stable production-ready daemon functionality.

### 🚀 v15.3 Documentation

1. **[v15.3 Status Report](v15-daemon-status.md)**  
   Current production-ready status with all fixes applied

2. **[v15 Overview](v15/README.md)**  
   Key improvements and architectural enhancements

3. **[Quick Start Guide](v15/quick_start_guide.md)**  
   Get started with Goxel v15.3 daemon

4. **[API Reference](v15/daemon_api_reference.md)**  
   Complete JSON-RPC API documentation (all 25 methods working)

5. **[Migration Guide](v15/migration_guide.md)**  
   Upgrade from v14.x to v15.3

### 📚 Historical Documentation (v14.0)

1. **[v14.0 Memory Issues](daemon-memory-issue-summary.md)**  
   Analysis of v14.0 limitations (resolved in v15.0)

2. **[Architecture Analysis](daemon-memory-architecture-analysis.md)**  
   Technical analysis of v14.0 issues

### 🔧 Quick Start

Get started with Goxel v15.3 (production-ready):
1. Install via Homebrew: `brew install jimmy/goxel/goxel-daemon`
2. Start daemon: `brew services start goxel-daemon`
3. Test save_project (now working!): See [Status Report](v15-daemon-status.md)
4. Read the [Quick Start Guide](v15/quick_start_guide.md)
5. Check [API Reference](v15/daemon_api_reference.md) for all 25 methods

### 📈 Version History

- **v15.3**: ✅ **STABLE** - save_project fix, all critical issues resolved
- **v15.2**: ✅ Connection reuse working, memory fixes
- **v15.1**: ✅ Performance improvements
- **v15.0**: ✅ Production-ready with full multi-request support
- **v14.0**: Historical - Single-operation limitation
- **v13.x**: Legacy versions without daemon support

### 🎉 Recent Major Fix (v15.3)
- **Save_Project Hanging**: RESOLVED - Previously hung indefinitely, now responds in 0.00s
- **OpenGL Preview**: Fixed daemon mode detection to skip graphics operations
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