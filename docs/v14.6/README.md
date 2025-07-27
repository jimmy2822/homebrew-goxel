# Goxel v14.6 Documentation

## Overview

Welcome to the Goxel v14.6 documentation! This version introduces a unified headless daemon architecture that delivers >700% performance improvement over v13.4 while maintaining complete backward compatibility.

## Documentation Structure

### 📚 User Documentation
- **[Quick Start Guide](quick-start.md)** - Get up and running in minutes
- **[User Guide](user-guide.md)** - Comprehensive usage documentation
- **[Migration Guide](migration-guide.md)** - Upgrade from v13.4 to v14.6
- **[Examples](examples/)** - Code samples and tutorials

### 🔧 Developer Documentation
- **[Developer Guide](developer-guide.md)** - Architecture and contributing
- **[API Reference](api-reference.md)** - Complete JSON-RPC API documentation
- **[Client Libraries](client-libraries.md)** - TypeScript/Python/C++ clients
- **[Plugin Development](plugin-guide.md)** - Extend Goxel functionality

### 🚀 Deployment & Operations
- **[Installation Guide](installation.md)** - Platform-specific installation
- **[Deployment Guide](deployment-guide.md)** - Production deployment
- **[Performance Tuning](performance-tuning.md)** - Optimization guide
- **[Troubleshooting](troubleshooting.md)** - Common issues and solutions

### 📋 Reference Documentation
- **[Configuration Reference](config-reference.md)** - All configuration options
- **[Command Reference](command-reference.md)** - CLI commands and options
- **[Error Codes](error-codes.md)** - Complete error code reference
- **[Changelog](CHANGELOG.md)** - Version history and changes

## Key Features in v14.6

### 🎯 Unified Binary Architecture
- Single `goxel` executable for GUI and headless modes
- `goxel --headless` for daemon/CLI operations
- Seamless mode switching without separate binaries

### ⚡ Performance Improvements
- **700%+ faster** than v13.4 CLI mode
- Persistent daemon eliminates startup overhead
- Connection pooling for efficient resource usage
- Concurrent request processing

### 🔌 Enhanced Integration
- JSON-RPC 2.0 protocol for all operations
- Native client libraries (TypeScript, Python, C++)
- MCP (Model Context Protocol) integration
- Plugin system for custom extensions

### 🛠 Developer Experience
- Comprehensive API documentation
- Interactive examples and tutorials
- Debugging and profiling tools
- Automated migration utilities

## Quick Links

- **Get Started**: [Quick Start Guide](quick-start.md)
- **API Docs**: [JSON-RPC API Reference](api-reference.md)
- **Examples**: [Code Examples](examples/)
- **Support**: [GitHub Issues](https://github.com/goxel/goxel/issues)

## Version Compatibility

| Feature | v13.4 | v14.6 |
|---------|-------|-------|
| CLI Commands | ✅ | ✅ (100% compatible) |
| File Formats | ✅ | ✅ (full support) |
| MCP Integration | ✅ | ✅ (enhanced) |
| Performance | Baseline | 700%+ improvement |
| Daemon Mode | ❌ | ✅ |
| Client Libraries | ❌ | ✅ |

## Documentation Standards

All documentation follows these principles:
- **Clear**: Simple language, avoid jargon
- **Complete**: Cover all features and edge cases
- **Current**: Updated with each release
- **Practical**: Include real-world examples
- **Accessible**: Multiple formats (HTML, PDF, Markdown)

---

*Last Updated: January 2025*  
*Goxel v14.6 - Unified Headless Architecture*