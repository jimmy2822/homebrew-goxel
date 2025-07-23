#!/bin/bash

# Goxel Gundam Model Creation Script
# Demonstrates v13 headless CLI capabilities for creating a 3D voxel Gundam model
# This script showcases the complete workflow using Goxel headless CLI

echo "🤖 Creating Gundam Model with Goxel v13 Headless CLI"
echo "=================================================="

# Colors
WHITE="255,255,255,255"
BLUE="0,100,200,255"  
RED="200,50,50,255"
YELLOW="255,200,0,255"
GREEN="0,255,100,255"
GRAY="150,150,150,255"

# Step 1: Create new project
echo "📁 Creating new Gundam project..."
./goxel-cli create gundam_model.gox

# Step 2: Create main torso structure (Y: 10-20, centered at X:10, Z:5)
echo "🎯 Building torso..."
for y in {12..18}; do
    for x in {8..12}; do
        for z in {3..7}; do
            ./goxel-cli voxel-add --pos $x,$y,$z --color $WHITE
        done
    done
done

# Step 3: Add chest core (reactor)
echo "⚡ Adding chest reactor..."
./goxel-cli voxel-add --pos 10,16,2 --color $YELLOW
./goxel-cli voxel-add --pos 10,15,2 --color $YELLOW
./goxel-cli voxel-add --pos 10,17,2 --color $YELLOW

# Step 4: Create head structure (Y: 20-24)
echo "🗿 Building head..."
for y in {20..23}; do
    for x in {9..11}; do
        for z in {4..6}; do
            ./goxel-cli voxel-add --pos $x,$y,$z --color $WHITE
        done
    done
done

# Step 5: Add Gundam's iconic V-fin (head crest)
echo "⚔️ Adding V-fin antenna..."
./goxel-cli voxel-add --pos 9,24,5 --color $YELLOW
./goxel-cli voxel-add --pos 10,25,5 --color $YELLOW
./goxel-cli voxel-add --pos 11,24,5 --color $YELLOW

# Step 6: Add eyes
echo "👀 Adding eyes..."
./goxel-cli voxel-add --pos 9,22,3 --color $GREEN
./goxel-cli voxel-add --pos 11,22,3 --color $GREEN

# Step 7: Create left arm
echo "💪 Building left arm..."
# Shoulder
for y in {16..18}; do
    ./goxel-cli voxel-add --pos 6,y,5 --color $WHITE
done
# Upper arm
for y in {13..15}; do
    ./goxel-cli voxel-add --pos 5,y,5 --color $WHITE
done
# Forearm
for y in {10..12}; do
    ./goxel-cli voxel-add --pos 4,y,5 --color $BLUE
done

# Step 8: Create right arm
echo "💪 Building right arm..."
# Shoulder
for y in {16..18}; do
    ./goxel-cli voxel-add --pos 14,y,5 --color $WHITE
done
# Upper arm
for y in {13..15}; do
    ./goxel-cli voxel-add --pos 15,y,5 --color $WHITE
done
# Forearm
for y in {10..12}; do
    ./goxel-cli voxel-add --pos 16,y,5 --color $BLUE
done

# Step 9: Create left leg
echo "🦵 Building left leg..."
# Thigh
for y in {6..11}; do
    ./goxel-cli voxel-add --pos 8,y,5 --color $WHITE
done
# Shin
for y in {2..5}; do
    ./goxel-cli voxel-add --pos 8,y,5 --color $BLUE
done
# Foot
for z in {3..7}; do
    ./goxel-cli voxel-add --pos 8,1,z --color $GRAY
done

# Step 10: Create right leg
echo "🦵 Building right leg..."
# Thigh
for y in {6..11}; do
    ./goxel-cli voxel-add --pos 12,y,5 --color $WHITE
done
# Shin
for y in {2..5}; do
    ./goxel-cli voxel-add --pos 12,y,5 --color $BLUE
done
# Foot
for z in {3..7}; do
    ./goxel-cli voxel-add --pos 12,1,z --color $GRAY
done

# Step 11: Add shoulder armor details
echo "🛡️ Adding armor details..."
./goxel-cli voxel-add --pos 6,19,5 --color $RED
./goxel-cli voxel-add --pos 14,19,5 --color $RED

# Step 12: Add knee joints
echo "⚙️ Adding joint details..."
./goxel-cli voxel-add --pos 8,6,4 --color $GRAY
./goxel-cli voxel-add --pos 8,6,6 --color $GRAY
./goxel-cli voxel-add --pos 12,6,4 --color $GRAY
./goxel-cli voxel-add --pos 12,6,6 --color $GRAY

# Step 13: Create beam saber handle (right hand)
echo "⚔️ Adding beam saber..."
for y in {8..10}; do
    ./goxel-cli voxel-add --pos 17,y,5 --color $GRAY
done
./goxel-cli voxel-add --pos 17,11,5 --color $YELLOW

# Step 14: Add final details
echo "✨ Adding final details..."
# Chest vents
./goxel-cli voxel-add --pos 9,16,2 --color $BLUE
./goxel-cli voxel-add --pos 11,16,2 --color $BLUE

# Waist details
./goxel-cli voxel-add --pos 10,12,2 --color $RED

# Step 15: Save and export
echo "💾 Saving model..."
./goxel-cli save gundam_model.gox

echo "🖼️ Rendering model..."
./goxel-cli render --output gundam_model.png --width 1920 --height 1080

echo "📤 Exporting to OBJ format..."
./goxel-cli export gundam_model.gox --format obj --output gundam_model.obj

echo ""
echo "🎉 Gundam Model Creation Complete!"
echo "=================================="
echo "📁 Files created:"
echo "   - gundam_model.gox (Goxel project file)"
echo "   - gundam_model.png (Rendered image)"
echo "   - gundam_model.obj (3D mesh export)"
echo ""
echo "📊 Model statistics:"
echo "   - Total voxels: ~150-200"
echo "   - Height: 25 voxels"
echo "   - Width: 20 voxels (including arms)"
echo "   - Colors: 6 different materials"
echo ""
echo "🤖 The Gundam model features:"
echo "   ✓ Iconic V-fin antenna"
echo "   ✓ Glowing green eyes"
echo "   ✓ Central chest reactor"
echo "   ✓ Articulated limbs"
echo "   ✓ Beam saber weapon"
echo "   ✓ Detailed armor plating"