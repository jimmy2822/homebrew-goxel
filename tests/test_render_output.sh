#!/bin/bash

# Test daemon rendering output

SOCKET_PATH="/tmp/goxel_render_test.sock"
OUTPUT_PATH="/tmp/test_render_dark_bg.png"

# Cleanup
rm -f $SOCKET_PATH $OUTPUT_PATH

# Start daemon
echo "Starting daemon..."
./goxel-daemon --foreground --socket $SOCKET_PATH &
DAEMON_PID=$!
sleep 1

# Create project and add voxel
echo '{"jsonrpc":"2.0","method":"goxel.create_project","params":["Test",32,32,32],"id":1}' | nc -U $SOCKET_PATH
echo '{"jsonrpc":"2.0","method":"goxel.add_voxel","params":[16,16,16,255,0,0,255],"id":2}' | nc -U $SOCKET_PATH
echo '{"jsonrpc":"2.0","method":"goxel.add_voxel","params":[17,16,16,0,255,0,255],"id":3}' | nc -U $SOCKET_PATH
echo '{"jsonrpc":"2.0","method":"goxel.add_voxel","params":[15,16,16,0,0,255,255],"id":4}' | nc -U $SOCKET_PATH

# Render scene
echo "Rendering scene..."
echo '{"jsonrpc":"2.0","method":"goxel.render_scene","params":["'$OUTPUT_PATH'",400,400],"id":5}' | nc -U $SOCKET_PATH

# Stop daemon
kill $DAEMON_PID
wait $DAEMON_PID 2>/dev/null

# Check output
if [ -f "$OUTPUT_PATH" ]; then
    echo "✅ Rendered image created: $OUTPUT_PATH"
    ls -la $OUTPUT_PATH
    echo "Opening image..."
    open $OUTPUT_PATH
else
    echo "❌ No rendered image found"
fi