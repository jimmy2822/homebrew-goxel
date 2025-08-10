# Goxel v0.15.3 Documentation

Welcome to Goxel v0.15.3 documentation. This version introduces significant improvements to the daemon architecture, addressing the limitations of v14.0.

## Key Improvements in v0.15.3

### Architecture Enhancements
- **Multi-Request Support**: Complete architectural refactor enables true multi-request handling
- **Concurrent Operations**: Proper request queuing and processing without hangs
- **Enhanced Stability**: Improved memory management and thread safety
- **Production Ready**: Resolved single-operation limitation from v14.0

### Performance Improvements
- Optimized protocol detection and routing
- Efficient socket communication for multiple connections
- Enhanced mutex protection preventing race conditions
- Automatic cleanup for optimal memory usage
- Consistent request processing speed

## Documentation Structure

- **API Reference**: Complete JSON-RPC API documentation
- **Quick Start Guide**: Getting started with v0.15.3
- **Migration Guide**: Upgrading from v14.x to v0.15.3
- **Deployment Guide**: Production deployment best practices
- **Integration Examples**: Client library examples and patterns

## Status

**Version**: 0.15.3  
**Status**: Stable Production Release  
**Last Updated**: August 8, 2025

### Recent Updates (v0.15.3)
- **Script Execution**: Resolved script execution issues for full JavaScript automation support

For the main project documentation, see [CLAUDE.md](/CLAUDE.md).