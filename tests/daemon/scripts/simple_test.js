// Simple test script for Goxel daemon script execution

console.log("Script started");

// Test basic voxel operations
var x = 50, y = 50, z = 50;
var color = [255, 128, 0, 255]; // Orange

console.log("Adding voxel at (" + x + ", " + y + ", " + z + ")");
var result = goxel.addVoxel(x, y, z, color);

if (result === 0) {
    console.log("Successfully added voxel");
} else {
    console.log("Failed to add voxel, error code: " + result);
}

// Test getting voxel info
var voxelInfo = goxel.getVoxel(x, y, z);
if (voxelInfo) {
    console.log("Retrieved voxel color: [" + voxelInfo.r + ", " + voxelInfo.g + ", " + voxelInfo.b + ", " + voxelInfo.a + "]");
}

console.log("Script completed successfully");