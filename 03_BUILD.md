# 03_BUILD - Goxel Build Instructions

## Prerequisites

### Linux/BSD
```bash
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev
```

### macOS
```bash
brew install scons glfw tre
```

### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw mingw-w64-x86_64-libtre-git scons make
```

## Build Commands

### v14 Daemon (Recommended)
```bash
# Debug build
scons daemon=1

# Release build (optimized)
scons mode=release daemon=1

# Clean
scons -c
```

### GUI Version (Legacy)
```bash
# Debug build
make
# or: scons

# Release build
make release

# Clean
make clean
```

## Running

### Daemon Mode
```bash
# Start daemon
./goxel-daemon --foreground --socket /tmp/goxel.sock

# Test with TypeScript client
cd src/mcp-client
npm install
npm test
```

### GUI Mode
```bash
./goxel
```

## Platform-Specific Notes

### macOS
- Requires Xcode command line tools
- ARM64 (Apple Silicon) fully supported
- May need to allow unsigned binaries in Security settings

### Linux
- Tested on Ubuntu 20.04+, CentOS 7+
- Requires OpenGL 3.3+ support
- For headless mode, OSMesa is used

### Windows
- Native Windows build requires MSYS2
- WSL2 fully supported for daemon mode
- GUI requires proper OpenGL drivers

## Troubleshooting

### Socket Permission Issues
```bash
# Ensure socket directory exists and has correct permissions
mkdir -p /tmp
chmod 755 /tmp
```

### Missing Dependencies
Check that all required libraries are installed:
```bash
# Linux
ldd ./goxel-daemon

# macOS
otool -L ./goxel-daemon
```

### Build Errors
- Clean build directory: `scons -c`
- Check compiler version: GCC 7+ or Clang 10+
- Verify all dependencies installed