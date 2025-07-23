#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ext_src/stb/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    // Create a simple 100x100 test image - red square
    int width = 100, height = 100;
    unsigned char *buffer = malloc(width * height * 4); // RGBA
    
    // Fill with red color
    for (int i = 0; i < width * height; i++) {
        buffer[i*4 + 0] = 255; // R
        buffer[i*4 + 1] = 0;   // G  
        buffer[i*4 + 2] = 0;   // B
        buffer[i*4 + 3] = 255; // A
    }
    
    // Write to PNG file using STB directly
    int result = stbi_write_png("goxel_test/png_test.png", width, height, 4, buffer, width * 4);
    
    if (result) {
        printf("✅ PNG test file created successfully: goxel_test/png_test.png\n");
    } else {
        printf("❌ Failed to create PNG file\n");
    }
    
    free(buffer);
    return result ? 0 : 1;
}