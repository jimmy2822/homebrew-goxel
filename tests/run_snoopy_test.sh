#!/bin/bash

# Goxel Snoopy Generation Test Runner
# This script runs the Snoopy generation integration test

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SOCKET_PATH="/tmp/goxel_snoopy_test.sock"
DAEMON_PID=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    
    # Kill daemon if running
    if [ -n "$DAEMON_PID" ]; then
        echo "Stopping daemon (PID: $DAEMON_PID)..."
        kill $DAEMON_PID 2>/dev/null || true
        wait $DAEMON_PID 2>/dev/null || true
    fi
    
    # Remove socket file
    rm -f "$SOCKET_PATH"
    
    # Clean up temporary files
    rm -f snoopy.gox snoopy.vox snoopy.png
}

# Set up trap for cleanup
trap cleanup EXIT

echo -e "${GREEN}=== Goxel Snoopy Generation Test ===${NC}"
echo

# Check if goxel-daemon exists
if [ ! -x "$PROJECT_DIR/goxel-daemon" ]; then
    echo -e "${RED}ERROR: goxel-daemon not found!${NC}"
    echo "Please build it with: scons daemon=1"
    exit 1
fi

# Check if regular goxel exists for conversion
if [ ! -x "$PROJECT_DIR/goxel" ]; then
    echo -e "${RED}ERROR: goxel not found!${NC}"
    echo "Please build it with: scons"
    exit 1
fi

# Build the test if needed
echo "Building Snoopy generation test..."
cd "$PROJECT_DIR"
gcc -o tests/test_snoopy_generation tests/test_snoopy_generation.c -Wall -Werror -lm

# Start the daemon
echo "Starting goxel-daemon..."
"$PROJECT_DIR/goxel-daemon" --foreground --socket "$SOCKET_PATH" &
DAEMON_PID=$!
echo "Daemon started with PID: $DAEMON_PID"

# Wait for daemon to start
sleep 2

# Check if daemon is running
if ! kill -0 $DAEMON_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Daemon failed to start${NC}"
    exit 1
fi

# Run the test
echo
echo "Running Snoopy generation test..."
cd "$PROJECT_DIR"
if ./tests/test_snoopy_generation; then
    echo -e "${GREEN}✓ Test completed successfully${NC}"
else
    echo -e "${RED}✗ Test failed${NC}"
    exit 1
fi

# Convert .gox to .vox using custom converter
echo
echo "Converting snoopy.gox to snoopy.vox..."
if [ -f "snoopy.gox" ]; then
    # Build the converter if needed
    if [ ! -x "tests/convert_gox_to_vox" ] || [ "tests/convert_gox_to_vox.c" -nt "tests/convert_gox_to_vox" ]; then
        echo "Building .gox to .vox converter..."
        gcc -o tests/convert_gox_to_vox tests/convert_gox_to_vox.c -Wall -Werror
    fi
    
    # Run the converter
    ./tests/convert_gox_to_vox snoopy.gox snoopy.vox
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Conversion completed${NC}"
    else
        echo -e "${RED}ERROR: Conversion failed${NC}"
        exit 1
    fi
else
    echo -e "${RED}ERROR: snoopy.gox not found${NC}"
    exit 1
fi

# Verify output files
echo
echo "Verifying output files..."

if [ -f "snoopy.gox" ]; then
    echo -e "${GREEN}✓ snoopy.gox created successfully${NC}"
    ls -lh snoopy.gox
else
    echo -e "${RED}✗ snoopy.gox not found${NC}"
    exit 1
fi

if [ -f "snoopy.vox" ]; then
    echo -e "${GREEN}✓ snoopy.vox created (placeholder)${NC}"
    echo -e "${YELLOW}  Note: Real .vox export requires GUI or format implementation${NC}"
else
    echo -e "${RED}✗ snoopy.vox not found${NC}"
    exit 1
fi

if [ -f "snoopy.png" ]; then
    echo -e "${GREEN}✓ snoopy.png created successfully${NC}"
    ls -lh snoopy.png
    
    # Check if the PNG is valid and not empty
    if [ -s "snoopy.png" ]; then
        file_info=$(file snoopy.png)
        if [[ $file_info == *"PNG image data"* ]]; then
            echo -e "${GREEN}  PNG file is valid${NC}"
            echo "  File info: $file_info"
        else
            echo -e "${RED}  WARNING: File doesn't appear to be a valid PNG${NC}"
        fi
    else
        echo -e "${RED}  WARNING: PNG file is empty${NC}"
    fi
else
    echo -e "${RED}✗ snoopy.png not found${NC}"
    exit 1
fi

echo
echo -e "${GREEN}=== All tests passed! ===${NC}"
echo
echo "Generated files:"
echo "  - snoopy.gox (native Goxel format)"
echo "  - snoopy.vox (placeholder - real conversion needs implementation)"
echo "  - snoopy.png (rendered image)"
echo
echo -e "${YELLOW}Note: The daemon currently only supports .gox format export.${NC}"
echo -e "${YELLOW}      Full .vox support requires using the GUI application.${NC}"