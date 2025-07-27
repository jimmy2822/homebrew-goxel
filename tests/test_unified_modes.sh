#!/bin/bash
# Test script for unified binary mode detection
# Tests that the unified binary correctly routes to GUI, headless, and daemon modes

echo "=== Testing Unified Binary Mode Detection ==="
echo

BINARY="./goxel-unified"

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Unified binary not found at $BINARY"
    echo "Build with: scons -f SConstruct.unified"
    exit 1
fi

echo "1. Testing default mode (should be GUI):"
$BINARY --help 2>&1 | head -n 5
echo

echo "2. Testing headless mode (--headless flag):"
$BINARY --headless --help 2>&1 | head -n 5
echo

echo "3. Testing daemon mode (--daemon flag):"
$BINARY --daemon --help 2>&1 | head -n 5
echo

echo "4. Testing symlink compatibility (goxel-headless):"
# Create temporary symlink
ln -sf goxel-unified goxel-headless
./goxel-headless --help 2>&1 | head -n 5
rm -f goxel-headless
echo

echo "5. Testing environment variable (GOXEL_HEADLESS=1):"
GOXEL_HEADLESS=1 $BINARY --help 2>&1 | head -n 5
echo

echo "=== Mode Detection Test Complete ==="