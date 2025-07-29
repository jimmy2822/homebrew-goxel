#!/bin/bash
# Quick MCP to GUI Workflow Demo
# Demonstrates creating a model via daemon and opening in GUI

echo "🚀 Quick MCP → GUI Demo"
echo "======================"

# Check if daemon is running
if [ ! -S "/tmp/goxel.sock" ]; then
    echo "❌ Daemon not running. Start with:"
    echo "   ./goxel-daemon --foreground --socket /tmp/goxel.sock"
    exit 1
fi

# Check if GUI exists
if [ ! -f "./goxel" ]; then
    echo "❌ GUI not built. Build with:"
    echo "   make"
    exit 1
fi

echo "✅ Prerequisites met"
echo ""

echo "📊 Creating model via daemon (simulating MCP)..."
# Run the Python workflow script
python3 examples/mcp_to_gui_workflow.py

echo ""
echo "🎯 Workflow Summary:"
echo "1. ✅ Model created using daemon JSON-RPC API"
echo "2. ✅ Saved as .gox file"
echo "3. ✅ Opened in GUI for visual editing"
echo ""
echo "🔄 This is exactly how MCP tools would work:"
echo "   MCP Tool → JSON-RPC Call → Daemon → File → GUI"