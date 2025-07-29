# Goxel Simplified Architecture - Task Breakdown

## Project Goal
Transform the current multi-layer architecture into a streamlined single-daemon solution with direct MCP protocol support.

**Current**: `[MCP Client] → [MCP Server] → [TypeScript Client] → [Goxel Daemon]`  
**Target**: `[MCP Client] → [Goxel MCP-Daemon]`

## Phase 1: Analysis & Planning (Week 1)

### 1.1 Architecture Analysis
- [ ] Document current data flow between all 4 components
- [ ] Identify redundant transformations and network hops
- [ ] Measure baseline performance metrics (latency, throughput)
- [ ] Map MCP protocol requirements to existing JSON-RPC methods

### 1.2 Dependency Assessment
- [ ] Analyze MCP protocol implementation requirements
- [ ] Review existing JSON-RPC handler structure in goxel-daemon
- [ ] Identify shared code between MCP server and daemon
- [ ] Document external library dependencies for MCP

### 1.3 Risk Assessment
- [ ] Identify breaking changes for existing clients
- [ ] Plan backward compatibility strategy
- [ ] Document rollback procedures
- [ ] Create migration timeline

## Phase 2: MCP Protocol Integration (Week 2-3)

### 2.1 Core MCP Handler
- [ ] Create `src/daemon/mcp_handler.c` for MCP protocol processing
- [ ] Implement MCP message parsing and validation
- [ ] Add MCP-to-internal command translation layer
- [ ] Create unified response formatter for MCP

### 2.2 Protocol Router
- [ ] Modify `daemon_main.c` to support dual-mode operation
- [ ] Implement protocol detection (MCP vs JSON-RPC)
- [ ] Create command-line flag `--protocol=[mcp|jsonrpc|auto]`
- [ ] Add configuration for default protocol mode

### 2.3 MCP Server Socket
- [ ] Extend socket server to handle MCP connections
- [ ] Implement MCP handshake and session management
- [ ] Add MCP-specific error handling
- [ ] Create MCP connection pooling

## Phase 3: Feature Parity (Week 3-4)

### 3.1 Method Mapping
- [ ] Map all existing MCP server methods to daemon internals
- [ ] Implement missing MCP-specific features in daemon
- [ ] Add MCP metadata support (capabilities, resources)
- [ ] Ensure all tool integrations work correctly

### 3.2 Performance Optimization
- [ ] Remove intermediate serialization/deserialization steps
- [ ] Implement direct memory mapping for large operations
- [ ] Add MCP-specific caching layer
- [ ] Optimize worker pool for MCP workloads

### 3.3 Testing Infrastructure
- [ ] Create MCP protocol test suite
- [ ] Port existing MCP server tests to daemon
- [ ] Add performance comparison tests
- [ ] Implement integration tests with real MCP clients

## Phase 4: Migration Tools (Week 5)

### 4.1 Compatibility Layer
- [ ] Create legacy adapter for existing MCP server users
- [ ] Implement configuration migration tool
- [ ] Add deprecation warnings to old components
- [ ] Document breaking changes and workarounds

### 4.2 Client Updates
- [ ] Update MCP client examples for direct connection
- [ ] Modify documentation for new architecture
- [ ] Create migration guide for existing users
- [ ] Update all integration tutorials

### 4.3 Deployment Scripts
- [ ] Update systemd/launchd service files
- [ ] Modify Docker configurations
- [ ] Create unified deployment script
- [ ] Add health check for MCP mode

## Phase 5: Cleanup & Optimization (Week 6)

### 5.1 Code Removal
- [ ] Archive deprecated MCP server code
- [ ] Remove intermediate TypeScript client
- [ ] Clean up unused dependencies
- [ ] Consolidate duplicate functionality

### 5.2 Documentation Update
- [ ] Archive v13/v14 documentation to `/archive/`
- [ ] Create new simplified architecture guide
- [ ] Update API reference for MCP mode
- [ ] Consolidate README files

### 5.3 Performance Validation
- [ ] Run comprehensive benchmarks
- [ ] Compare with baseline metrics
- [ ] Document performance improvements
- [ ] Create performance tuning guide

## Task Allocation Strategy

### Core Development Tasks (C/C++)
**Estimated: 45-50 tasks**
- MCP protocol handler implementation
- Socket server modifications  
- Protocol routing logic
- Performance optimizations

### Testing & Validation
**Estimated: 20-25 tasks**
- Unit tests for MCP handler
- Integration tests
- Performance benchmarks
- Backward compatibility tests

### Documentation & Migration
**Estimated: 15-20 tasks**
- Architecture documentation
- Migration guides
- API updates
- Deployment instructions

## Success Criteria

### Performance Targets
- **Latency**: <1ms for basic operations (80% reduction)
- **Throughput**: >10,000 ops/sec (100% improvement)
- **Memory**: <50MB overhead (60% reduction)
- **Startup**: <100ms (90% reduction)

### Quality Metrics
- **Test Coverage**: >95% for MCP components
- **Memory Leaks**: Zero (verified with Valgrind)
- **Compatibility**: 100% feature parity with current system
- **Documentation**: Complete migration guide

## Risk Mitigation

### Technical Risks
1. **Protocol Incompatibility**
   - Mitigation: Extensive protocol testing suite
   - Fallback: Dual-mode operation support

2. **Performance Regression**
   - Mitigation: Continuous benchmarking
   - Fallback: Keep JSON-RPC optimization

3. **Breaking Changes**
   - Mitigation: Compatibility layer
   - Fallback: Extended deprecation period

### Timeline Risks
1. **Scope Creep**
   - Mitigation: Strict phase boundaries
   - Fallback: Defer non-critical features

2. **Integration Issues**
   - Mitigation: Early integration testing
   - Fallback: Phased rollout

## Implementation Priority

### Must Have (P0)
- Direct MCP protocol support
- Feature parity with current system
- Performance improvements
- Basic migration tools

### Should Have (P1)
- Advanced caching
- Enhanced monitoring
- Automated migration
- Performance profiling

### Nice to Have (P2)
- Additional protocol support
- Advanced debugging tools
- Extended metrics
- Plugin architecture

## Deliverables

### Week 1
- Complete analysis document
- Risk assessment report
- Detailed implementation plan

### Week 2-3
- Working MCP handler
- Dual-mode daemon
- Initial test suite

### Week 4
- Feature-complete implementation
- Performance validation
- Migration tools

### Week 5
- Updated documentation
- Deployment packages
- Migration guide

### Week 6
- Cleaned codebase
- Final benchmarks
- Release candidate

## Next Steps

1. **Immediate Actions**
   - Set up development branch `feature/simplified-mcp`
   - Create project tracking board
   - Schedule architecture review meeting

2. **Week 1 Focus**
   - Complete current state analysis
   - Design MCP handler interface
   - Set up test infrastructure

3. **Communication**
   - Weekly progress updates
   - Bi-weekly stakeholder reviews
   - Daily stand-ups during critical phases

---

**Document Version**: 1.0  
**Created**: January 2025  
**Status**: Planning Phase  
**Target Completion**: 6 weeks