#!/bin/bash

# Script to build OSMesa for macOS
# Based on the available information for building Mesa with OSMesa support

set -e

MESA_VERSION="23.3.6"
BUILD_DIR="/tmp/osmesa_build"
INSTALL_PREFIX="/opt/homebrew/opt/osmesa"

echo "Building OSMesa for macOS..."

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Install dependencies
echo "Installing dependencies..."
brew install meson ninja python@3.13 llvm

# Install Python dependencies
echo "Installing Python dependencies..."
pip3 install mako

# Download Mesa source
if [ ! -f "mesa-${MESA_VERSION}.tar.xz" ]; then
    echo "Downloading Mesa ${MESA_VERSION}..."
    curl -L -O "https://archive.mesa3d.org/mesa-${MESA_VERSION}.tar.xz"
fi

# Extract source
echo "Extracting source..."
tar -xf "mesa-${MESA_VERSION}.tar.xz"
cd "mesa-${MESA_VERSION}"

# Configure build with OSMesa
echo "Configuring build..."
meson setup build \
    --prefix="$INSTALL_PREFIX" \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers= \
    -Dosmesa=true \
    -Degl=disabled \
    -Dgles1=disabled \
    -Dgles2=disabled \
    -Dglx=disabled \
    -Dplatforms= \
    -Dshared-glapi=enabled \
    -Dgbm=disabled \
    -Dzlib=enabled

# Build
echo "Building..."
meson compile -C build

# Install (will require sudo)
echo "Installing to $INSTALL_PREFIX..."
echo "This will require sudo permissions..."
sudo meson install -C build

# Update pkg-config path
echo "Updating PKG_CONFIG_PATH..."
export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"

echo "OSMesa installation complete!"
echo "Add the following to your shell configuration:"
echo "export PKG_CONFIG_PATH=\"$INSTALL_PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "export LD_LIBRARY_PATH=\"$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH\""