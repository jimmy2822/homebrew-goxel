# Migration Guide: Goxel v13.4 to v14.0

**Status**: ⏸️ MIGRATION NOT POSSIBLE - v14.0 NOT FUNCTIONAL  
**Version**: 14.0.0-alpha  
**Last Updated**: January 27, 2025

## 🚨 Critical Notice

**DO NOT ATTEMPT MIGRATION**

The v14.0 daemon has no implemented functionality. This guide documents the planned migration path for when v14.0 is complete. Currently:

- ✅ v13.4 CLI: Fully functional, production-ready
- ❌ v14.0 Daemon: No working methods, alpha infrastructure only

**Recommendation: Continue using v13.4 for all work.**

## 📊 Version Comparison

### v13.4 CLI (Current Production)
| Feature | Status | Notes |
|---------|--------|-------|
| Create Projects | ✅ Working | `goxel-headless create` |
| Add/Remove Voxels | ✅ Working | Full voxel operations |
| Export Models | ✅ Working | All formats supported |
| Layer Management | ✅ Working | v13.3 fixes applied |
| Performance | ✅ Optimized | 520% faster with persistent mode |
| Stability | ✅ Production | Zero known critical bugs |
| Documentation | ✅ Complete | Comprehensive guides |

### v14.0 Daemon (Alpha)
| Feature | Status | Notes |
|---------|--------|-------|
| Create Projects | ❌ Not Implemented | Method doesn't exist |
| Add/Remove Voxels | ❌ Not Implemented | No voxel operations |
| Export Models | ❌ Not Implemented | No export functionality |
| Layer Management | ❌ Not Implemented | No layer methods |
| Performance | ❓ Unknown | Can't measure without methods |
| Stability | ⚠️ Alpha | Infrastructure only |
| Documentation | ⚠️ Misleading | Claims don't match reality |

## 🎯 Migration Readiness Assessment

### Can I Migrate Now?
**NO** - There's nothing to migrate to. The v14.0 daemon provides no functionality.

### When Can I Migrate?
Based on current development state, earliest realistic migration timeline:

- **Q2 2025**: Basic functionality might be available
- **Q3 2025**: Feature parity with v13.4 possible
- **Q4 2025**: Production-ready migration path

### Should I Plan for Migration?
Not yet. Focus on optimizing v13.4 usage:

```bash
# v13.4 persistent mode is already fast
./goxel-headless --interactive << EOF
create project.gox
add-voxel 0 -16 0 255 0 0 255
export project.obj
exit
EOF
```

## 🔄 Conceptual Migration Path (FUTURE)

When v14.0 is functional, migration will involve:

### 1. Architecture Changes

**v13.4 CLI Architecture:**
```
Application → CLI Call → New Process → Goxel Core → File I/O → Result
```

**v14.0 Daemon Architecture (Planned):**
```
Application → JSON-RPC → Persistent Daemon → Shared Memory → Result
```

### 2. API Translation

**v13.4 CLI Commands:**
```bash
# Current working approach
./goxel-headless create test.gox
./goxel-headless add-voxel 0 -16 0 255 0 0 255
./goxel-headless export test.obj
```

**v14.0 JSON-RPC (Planned):**
```json
// This doesn't work yet
{
  "jsonrpc": "2.0",
  "method": "goxel.create_project",
  "params": {"name": "test"},
  "id": 1
}
```

### 3. Performance Comparison (Theoretical)

**v13.4 Performance (Actual):**
- Startup: 9.88ms (optimized)
- Per operation: 2.70ms (persistent mode)
- Batch 5 ops: 13.51ms total

**v14.0 Performance (Claimed but Unverified):**
- Startup: 0ms (already running)
- Per operation: 2.1ms (projected)
- Batch 5 ops: 10.5ms (theoretical)

**Real improvement: ~22% (not 700%)**

## 🛠️ Migration Tools (NOT AVAILABLE)

### Planned Compatibility Layer
```javascript
// THIS DOESN'T EXIST YET
class V13CompatibilityAdapter {
  constructor(daemon) {
    this.daemon = daemon;
  }
  
  async runCommand(args) {
    // Map CLI args to daemon methods
    const [command, ...params] = args;
    
    switch(command) {
      case 'create':
        return await this.daemon.createProject({
          filePath: params[0]
        });
      // ... more mappings
    }
  }
}
```

### Planned Migration Script
```bash
#!/bin/bash
# THIS IS CONCEPTUAL ONLY

# Check v14 daemon availability
if ! goxel-daemon --status > /dev/null 2>&1; then
  echo "v14 daemon not running, using v13.4 CLI"
  exec goxel-headless "$@"
fi

# Attempt daemon operation
if ! goxel-client "$@" 2>/dev/null; then
  echo "Daemon method failed, falling back to v13.4"
  exec goxel-headless "$@"
fi
```

## 📋 Pre-Migration Checklist

Before considering migration (when v14.0 is ready):

### Technical Requirements
- [ ] All v13.4 commands have v14.0 equivalents
- [ ] Performance improvements verified
- [ ] Client libraries available for your language
- [ ] Monitoring and logging in place

### Testing Requirements
- [ ] Feature parity testing complete
- [ ] Performance benchmarks validated
- [ ] Integration tests passing
- [ ] Rollback plan prepared

### Operational Requirements
- [ ] Team trained on new architecture
- [ ] Documentation updated
- [ ] Support processes defined
- [ ] Gradual rollout plan

**Current Checklist Score: 0/12 ❌**

## 🚨 Common Migration Issues (ANTICIPATED)

### Issue 1: Methods Not Implemented
**Current State**: This is not an "issue" - it's the reality. No methods work.

**Future Solution**: Wait for implementation.

### Issue 2: Different Error Handling
**v13.4 CLI**: Exit codes and stderr
**v14.0 Daemon**: JSON-RPC error objects

**Future Adapter Code**:
```javascript
function translateError(cliError) {
  // Map CLI exit codes to JSON-RPC errors
  const errorMap = {
    1: { code: -32001, message: "Project not found" },
    2: { code: -32002, message: "Invalid parameters" },
    // ... more mappings
  };
  return errorMap[cliError.code] || { code: -32603, message: "Internal error" };
}
```

### Issue 3: State Management Differences
**v13.4**: Stateless, file-based
**v14.0**: Stateful, memory-based

**Considerations**:
- Project lifecycle management
- Memory usage patterns
- Concurrent access handling

## 📊 Risk Assessment

### Migration Risks (When Available)

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Feature gaps | High | High | Thorough testing |
| Performance regression | Medium | High | Benchmark everything |
| Breaking changes | Medium | Medium | Compatibility layer |
| Operational complexity | High | Medium | Training and docs |

### Current Risk: 100% - Nothing Works

## 🎯 Recommendations by Role

### For Developers
1. **Keep using v13.4** - It works great
2. **Optimize current usage** - Use persistent mode
3. **Monitor v14.0 progress** - But don't plan on it yet
4. **Contribute to v14.0** - Help implement methods

### For System Administrators
1. **No action required** - v14.0 isn't deployable
2. **Document v13.4 usage** - It's your long-term solution
3. **Ignore v14.0 claims** - Marketing exceeds reality

### For Project Managers
1. **Adjust timelines** - Add 6-9 months minimum
2. **Keep v13.4 plans** - It's your production system
3. **Track actual progress** - Not documentation claims

### For End Users
1. **Nothing changes** - Keep using current tools
2. **v13.4 is excellent** - No need to wait for v14.0
3. **Performance is fine** - v13.4 optimizations work well

## 📈 Migration Timeline (REALISTIC)

### Current State (January 2025)
- v13.4: Production ready ✅
- v14.0: Infrastructure only ❌

### Q2 2025 (Optimistic)
- v14.0: Basic methods working
- Migration: Not recommended

### Q3 2025 (Possible)
- v14.0: Feature parity
- Migration: Early adopters only

### Q4 2025 (Realistic)
- v14.0: Production ready
- Migration: Gradual rollout

### 2026
- Full migration possible
- v13.4 deprecation planning

## 🔧 Staying with v13.4

Given v14.0's state, optimize your v13.4 usage:

### Use Persistent Mode
```bash
# 520% faster than individual calls
./goxel-headless --interactive < commands.txt
```

### Batch Operations
```bash
# Create command file
cat > batch.gox << EOF
create model.gox
add-voxel 0 -16 0 255 0 0 255
add-voxel 1 -16 0 0 255 0 255
save model.gox
export model.obj
EOF

# Execute efficiently
./goxel-headless --interactive < batch.gox
```

### MCP Integration
```javascript
// This works today with v13.4
const result = await goxelMCP.callTool('create_voxel', {
  output_file: 'model.gox'
});
```

## 📝 Summary

**Current Reality:**
- v13.4 CLI: Fully functional, optimized, production-ready
- v14.0 Daemon: Infrastructure only, no functionality

**Migration Status:**
- Not possible
- Not recommended
- Not needed

**Action Items:**
1. Continue using v13.4
2. Ignore v14.0 documentation claims
3. Check back in 6-9 months

---

*This migration guide will be updated when v14.0 has actual functionality to migrate to. For now, v13.4 remains the only viable option.*