# Goxel v15.0 Documentation

Welcome to Goxel v15.0 documentation. This version introduces significant improvements to the daemon architecture, addressing the limitations of v14.0.

## Key Improvements in v15.0

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
- **Quick Start Guide**: Getting started with v15.0
- **Migration Guide**: Upgrading from v14.x to v15.0
- **Deployment Guide**: Production deployment best practices
- **Integration Examples**: Client library examples and patterns

## Status

**Version**: 15.0.0-beta  
**Status**: Beta - Complete architectural refactor for multi-tenant support  
**Last Updated**: August 2, 2025

For the main project documentation, see [CLAUDE.md](/CLAUDE.md).