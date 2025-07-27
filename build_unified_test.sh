#!/bin/bash
# Quick build script to test unified binary compilation
# This creates a minimal test binary to verify mode detection works

echo "=== Building Unified Binary Test ==="

# Clean any previous test builds
rm -f goxel-unified-test

# Compile just the unified main with stubs enabled
gcc -o goxel-unified-test \
    -DUNIFIED_BUILD_STUB \
    -DGOXEL_UNIFIED_BUILD=1 \
    src/main_unified.c \
    -I. \
    -Isrc

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo
    echo "Testing mode detection:"
    echo
    
    echo "1. Default (GUI) mode:"
    ./goxel-unified-test
    echo
    
    echo "2. Headless mode:"
    ./goxel-unified-test --headless
    echo
    
    echo "3. Daemon mode:"
    ./goxel-unified-test --daemon
    echo
    
    echo "4. Symlink test:"
    ln -sf goxel-unified-test goxel-headless-test
    ./goxel-headless-test
    rm -f goxel-headless-test
    echo
    
    # Clean up
    rm -f goxel-unified-test
else
    echo "Build failed!"
    exit 1
fi