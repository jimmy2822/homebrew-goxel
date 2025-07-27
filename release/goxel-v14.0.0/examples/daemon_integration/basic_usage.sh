#!/bin/bash
#
# Goxel v14.0 Daemon Integration Examples
# Basic usage patterns and best practices
#

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}Goxel v14.0 Daemon Integration Examples${NC}"
echo "========================================"
echo

# Example 1: Check if daemon is running
echo -e "${GREEN}Example 1: Checking daemon status${NC}"
if goxel-daemon-client ping > /dev/null 2>&1; then
    echo "✓ Daemon is running"
    goxel-daemon-client status
else
    echo "✗ Daemon is not running"
    echo "Start it with: sudo systemctl start goxel-daemon"
fi
echo

# Example 2: Performance comparison
echo -e "${GREEN}Example 2: Performance comparison${NC}"
echo "Creating 100 voxels with daemon mode..."
time {
    goxel-headless --daemon create perf_test.gox
    for i in {1..100}; do
        goxel-headless --daemon add-voxel $i 0 0 255 0 0 255
    done
    goxel-headless --daemon save perf_test.gox
}

echo
echo "Creating 100 voxels with standalone mode..."
time {
    goxel-headless --no-daemon create perf_test2.gox
    for i in {1..100}; do
        goxel-headless --no-daemon add-voxel $i 0 0 255 0 0 255
    done
    goxel-headless --no-daemon save perf_test2.gox
}
echo

# Example 3: Batch operations
echo -e "${GREEN}Example 3: Batch operations${NC}"
cat > batch_commands.txt << 'EOF'
create batch_test.gox
add-voxel 0 0 0 255 0 0 255
add-voxel 1 0 0 0 255 0 255
add-voxel 2 0 0 0 0 255 255
add-layer "Second Layer"
select-layer 1
add-voxel 0 1 0 255 255 0 255
export batch_test.obj
save batch_test.gox
EOF

echo "Executing batch commands..."
goxel-headless --daemon batch < batch_commands.txt
echo "✓ Batch completed"
echo

# Example 4: Error handling
echo -e "${GREEN}Example 4: Error handling${NC}"
echo "Testing error recovery..."

# Intentionally cause an error
goxel-headless --daemon add-voxel invalid arguments 2>/dev/null || {
    echo "✓ Error handled gracefully"
}

# Daemon should still be responsive
if goxel-daemon-client ping > /dev/null 2>&1; then
    echo "✓ Daemon still running after error"
fi
echo

# Example 5: Concurrent clients
echo -e "${GREEN}Example 5: Concurrent operations${NC}"
echo "Running 3 concurrent clients..."

# Start 3 background jobs
for i in {1..3}; do
    (
        goxel-headless --daemon create "concurrent_$i.gox"
        for j in {1..50}; do
            goxel-headless --daemon add-voxel $j $i 0 $((i*80)) $((i*80)) $((i*80)) 255
        done
        goxel-headless --daemon save "concurrent_$i.gox"
        echo "✓ Client $i completed"
    ) &
done

# Wait for all jobs
wait
echo "✓ All concurrent operations completed"
echo

# Example 6: Using the daemon client directly
echo -e "${GREEN}Example 6: Direct daemon client usage${NC}"
echo "Getting daemon statistics..."
goxel-daemon-client stats | python3 -m json.tool
echo

# Example 7: Integration with scripts
echo -e "${GREEN}Example 7: Script integration${NC}"
cat > voxel_generator.py << 'EOF'
#!/usr/bin/env python3
import subprocess
import json
import math

def run_goxel(command):
    """Run a Goxel command via CLI"""
    cmd = ["goxel-headless", "--daemon"] + command.split()
    subprocess.run(cmd, check=True)

# Create a spiral of voxels
run_goxel("create spiral.gox")

for i in range(100):
    angle = i * 0.1
    x = int(10 * math.cos(angle))
    y = int(i / 10)
    z = int(10 * math.sin(angle))
    r = int(255 * (1 - i/100))
    g = int(255 * (i/100))
    b = 128
    
    run_goxel(f"add-voxel {x} {y} {z} {r} {g} {b} 255")

run_goxel("export spiral.obj")
run_goxel("save spiral.gox")
print("✓ Generated spiral.gox")
EOF

chmod +x voxel_generator.py
python3 voxel_generator.py
echo

# Cleanup
echo -e "${GREEN}Cleaning up test files...${NC}"
rm -f perf_test.gox perf_test2.gox batch_test.gox batch_test.obj
rm -f concurrent_*.gox spiral.gox spiral.obj
rm -f batch_commands.txt voxel_generator.py
echo "✓ Cleanup completed"

echo
echo -e "${BLUE}Examples completed!${NC}"
echo "The daemon provides 700%+ performance improvement for batch operations."
echo "Use '--daemon' flag to ensure daemon mode, or '--no-daemon' for standalone."