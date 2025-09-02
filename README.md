# Homebrew Goxel

Homebrew tap for Goxel Daemon - High-performance voxel editor daemon with JSON-RPC API.

## Installation

```bash
brew tap jimmy/goxel
brew install goxel-daemon
```

## Quick Start

```bash
# Start the daemon service
brew services start goxel-daemon

# Test connection
python3 /opt/homebrew/opt/goxel-daemon/share/goxel/examples/homebrew_test_client.py

# Manual operation
goxel-daemon --foreground --socket /tmp/goxel.sock
```

## Features

- **v0.18.6**: Architecture simplified - binary protocol dead code removed (140 lines eliminated)
- **Clean JSON-RPC only architecture** - no confusion
- **MCP method mappings fixed** - render_scene works correctly
- **All daemon functions tested and operational**
- **Rendering pipeline 100% operational** with perfect colors
- **All formats verified**: .gox, .vox, .obj, .ply, .txt, .pov

## Documentation

- Socket location: `/opt/homebrew/var/run/goxel/goxel.sock`
- Logs: `/opt/homebrew/var/log/goxel/goxel-daemon.log`
- Examples: `/opt/homebrew/opt/goxel-daemon/share/goxel/examples/`

## Version History

- **v0.18.6** (Sep 2025): Architecture cleanup - Remove binary protocol confusion
- **v0.18.5** (Sep 2025): Complete rendering pipeline fixed
- **v0.18.3** (Aug 2025): Protocol detection improvements
- **v0.17.4** (Aug 2025): MCP connection reuse and thread safety
- **v0.17.3** (Aug 2025): Multi-angle rendering and massive model support

---

**Official Website**: https://goxel.xyz