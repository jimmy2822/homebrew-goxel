# 02_README - Goxel Project Overview

## What is Goxel?

Goxel is a cross-platform 3D voxel editor that enables creation of voxel graphics (3D images formed of cubes). 

**Version**: 14.0.0 (Daemon Architecture)  
**License**: GNU GPL3 (Commercial licenses available)  
**Website**: https://goxel.xyz

## Key Features

- **Voxel Editing**: 24-bit RGB colors with alpha channel
- **Unlimited Scene Size**: No restrictions on creation size
- **Multi-layer Support**: Complex compositions with layer management
- **Cross-Platform**: Linux, BSD, Windows, macOS
- **Multiple Export Formats**: OBJ, PLY, PNG, Magica Voxel, Qubicle, STL, glTF

## v14 Daemon Architecture

Goxel v14 introduces enterprise-grade daemon architecture:

- **JSON-RPC Server**: Unix socket-based for high performance
- **Concurrent Processing**: Worker pool for parallel operations  
- **TypeScript Client**: Production-ready client library
- **MCP Integration**: AI tool ecosystem compatibility
- **Performance**: 683% improvement over sequential operations

## Quick Links

- **Architecture**: [01_ARCHITECTURE.md](01_ARCHITECTURE.md)
- **Build Guide**: [03_BUILD.md](03_BUILD.md)
- **API Reference**: [04_API.md](04_API.md)
- **Quick Start**: [05_QUICKSTART.md](05_QUICKSTART.md)

## Project Structure

```
goxel/
├── src/daemon/      # Daemon server components
├── src/headless/    # Headless rendering API
├── src/core/        # Shared voxel engine
├── src/mcp-client/  # TypeScript client
└── src/gui/         # GUI application (legacy)
```

## Community

- **GitHub**: https://github.com/guillaumechereau/goxel
- **Issues**: Report bugs and request features
- **Author**: Guillaume Chereau <guillaume@noctua-software.com>