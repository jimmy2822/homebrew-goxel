#!/bin/bash

# Build script for Goxel with OSMesa support

echo "Building Goxel with OSMesa support..."

# Set PKG_CONFIG_PATH to find OSMesa
export PKG_CONFIG_PATH="/opt/homebrew/opt/osmesa/lib/pkgconfig:$PKG_CONFIG_PATH"

# Verify OSMesa is found
if pkg-config --exists osmesa; then
    echo "✅ OSMesa found via pkg-config"
    echo "OSMesa flags: $(pkg-config --cflags --libs osmesa)"
else
    echo "❌ OSMesa not found via pkg-config"
    exit 1
fi

# Clean and build
echo "Cleaning previous build..."
scons -c

echo "Building with OSMesa..."
scons daemon=1 headless=1

echo "Build complete!"