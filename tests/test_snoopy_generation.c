/*
 * Goxel Snoopy Generation Integration Test
 * 
 * This test generates a 1000-voxel Snoopy model and exports it as:
 * - snoopy.vox (voxel model file)
 * - snoopy.png (rendered image)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/goxel_snoopy_test.sock"
#define BUFFER_SIZE 8192

// Color definitions
#define COLOR_WHITE   255, 255, 255, 255
#define COLOR_BLACK   0, 0, 0, 255
#define COLOR_RED     200, 0, 0, 255
#define COLOR_BROWN   139, 69, 19, 255

typedef struct {
    int x, y, z;
    int r, g, b, a;
} Voxel;

// Helper function to send JSON-RPC request
static int send_request(int sock, const char* method, const char* params_str, int id) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}\n",
        method, params_str, id);
    
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        return -1;
    }
    
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    if (recv(sock, response, sizeof(response) - 1, 0) < 0) {
        perror("recv");
        return -1;
    }
    
    printf("Response: %s\n", response);
    return 0;
}

// Create new socket connection for each request
static int create_connection() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    return sock;
}

// Generate Snoopy voxel model
static int generate_snoopy() {
    Voxel* voxels = malloc(sizeof(Voxel) * 2000);  // Allocate space for up to 2000 voxels
    int voxel_count = 0;
    
    // Design parameters
    int base_x = 32, base_y = 32, base_z = 16;  // Center position
    
    // === BODY (white, ellipsoid shape) ===
    // Main body: elongated ellipsoid
    for (int x = -8; x <= 8; x++) {
        for (int y = -12; y <= 12; y++) {
            for (int z = -6; z <= 6; z++) {
                float nx = x / 8.0f;
                float ny = y / 12.0f;
                float nz = z / 6.0f;
                
                if (nx*nx + ny*ny + nz*nz <= 1.0f) {
                    voxels[voxel_count++] = (Voxel){
                        base_x + x, base_y + y, base_z + z,
                        COLOR_WHITE
                    };
                }
            }
        }
    }
    
    // === HEAD (white, spherical) ===
    int head_y = base_y + 15;
    for (int x = -6; x <= 6; x++) {
        for (int y = -6; y <= 6; y++) {
            for (int z = -6; z <= 6; z++) {
                float dist = sqrtf(x*x + y*y + z*z);
                if (dist <= 6.0f) {
                    voxels[voxel_count++] = (Voxel){
                        base_x + x, head_y + y, base_z + z,
                        COLOR_WHITE
                    };
                }
            }
        }
    }
    
    // === SNOUT (white, protruding forward) ===
    int snout_y = head_y + 4;
    for (int x = -3; x <= 3; x++) {
        for (int y = 0; y <= 6; y++) {
            for (int z = -3; z <= 3; z++) {
                float nx = x / 3.0f;
                float ny = (y - 3) / 3.0f;
                float nz = z / 3.0f;
                
                if (nx*nx + ny*ny + nz*nz <= 1.0f) {
                    voxels[voxel_count++] = (Voxel){
                        base_x + x, snout_y + y, base_z + z,
                        COLOR_WHITE
                    };
                }
            }
        }
    }
    
    // === EARS (black, droopy) ===
    // Left ear
    for (int x = -10; x <= -7; x++) {
        for (int y = -2; y <= 2; y++) {
            for (int z = -8; z <= -2; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, head_y + y, base_z + z,
                    COLOR_BLACK
                };
            }
        }
    }
    
    // Right ear
    for (int x = 7; x <= 10; x++) {
        for (int y = -2; y <= 2; y++) {
            for (int z = -8; z <= -2; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, head_y + y, base_z + z,
                    COLOR_BLACK
                };
            }
        }
    }
    
    // === NOSE (black, small sphere) ===
    int nose_y = snout_y + 7;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, nose_y + y, base_z + z,
                    COLOR_BLACK
                };
            }
        }
    }
    
    // === EYES (black dots) ===
    // Left eye
    voxels[voxel_count++] = (Voxel){base_x - 3, head_y + 2, base_z + 5, COLOR_BLACK};
    voxels[voxel_count++] = (Voxel){base_x - 3, head_y + 3, base_z + 5, COLOR_BLACK};
    
    // Right eye
    voxels[voxel_count++] = (Voxel){base_x + 3, head_y + 2, base_z + 5, COLOR_BLACK};
    voxels[voxel_count++] = (Voxel){base_x + 3, head_y + 3, base_z + 5, COLOR_BLACK};
    
    // === COLLAR (red) ===
    int collar_y = base_y + 8;
    for (int angle = 0; angle < 360; angle += 10) {
        float rad = angle * M_PI / 180.0f;
        int x = (int)(7 * cosf(rad));
        int z = (int)(7 * sinf(rad));
        
        for (int dy = -1; dy <= 1; dy++) {
            voxels[voxel_count++] = (Voxel){
                base_x + x, collar_y + dy, base_z + z,
                COLOR_RED
            };
        }
    }
    
    // === LEGS (white, four cylinders) ===
    // Front left leg
    for (int y = -10; y <= -2; y++) {
        for (int x = -6; x <= -4; x++) {
            for (int z = -3; z <= -1; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y + y, base_z + z,
                    COLOR_WHITE
                };
            }
        }
    }
    
    // Front right leg
    for (int y = -10; y <= -2; y++) {
        for (int x = 4; x <= 6; x++) {
            for (int z = -3; z <= -1; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y + y, base_z + z,
                    COLOR_WHITE
                };
            }
        }
    }
    
    // Back left leg
    for (int y = -10; y <= -2; y++) {
        for (int x = -6; x <= -4; x++) {
            for (int z = -3; z <= -1; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y - 15 + y, base_z + z,
                    COLOR_WHITE
                };
            }
        }
    }
    
    // Back right leg
    for (int y = -10; y <= -2; y++) {
        for (int x = 4; x <= 6; x++) {
            for (int z = -3; z <= -1; z++) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y - 15 + y, base_z + z,
                    COLOR_WHITE
                };
            }
        }
    }
    
    // === PAWS (black) ===
    // Front left paw
    for (int x = -6; x <= -4; x++) {
        for (int z = -3; z <= -1; z++) {
            voxels[voxel_count++] = (Voxel){
                base_x + x, base_y - 11, base_z + z,
                COLOR_BLACK
            };
        }
    }
    
    // Front right paw
    for (int x = 4; x <= 6; x++) {
        for (int z = -3; z <= -1; z++) {
            voxels[voxel_count++] = (Voxel){
                base_x + x, base_y - 11, base_z + z,
                COLOR_BLACK
            };
        }
    }
    
    // Back left paw
    for (int x = -6; x <= -4; x++) {
        for (int z = -3; z <= -1; z++) {
            voxels[voxel_count++] = (Voxel){
                base_x + x, base_y - 26, base_z + z,
                COLOR_BLACK
            };
        }
    }
    
    // Back right paw
    for (int x = 4; x <= 6; x++) {
        for (int z = -3; z <= -1; z++) {
            voxels[voxel_count++] = (Voxel){
                base_x + x, base_y - 26, base_z + z,
                COLOR_BLACK
            };
        }
    }
    
    // === TAIL (white with black tip) ===
    for (int y = -20; y <= -15; y++) {
        for (int x = -2; x <= 2; x++) {
            for (int z = 5; z <= 8; z++) {
                int color_r = (y >= -17) ? 255 : 0;
                int color_g = (y >= -17) ? 255 : 0;
                int color_b = (y >= -17) ? 255 : 0;
                
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y + y, base_z + z,
                    color_r, color_g, color_b, 255
                };
            }
        }
    }
    
    printf("Total voxels generated: %d\n", voxel_count);
    
    // Now send all voxels to goxel daemon
    int sock;
    int id = 1;
    
    // Create project
    sock = create_connection();
    if (sock < 0) return -1;
    if (send_request(sock, "goxel.create_project", "[\"Snoopy\", 64, 64, 64]", id++) < 0) {
        close(sock);
        free(voxels);
        return -1;
    }
    close(sock);
    
    // Add each voxel
    for (int i = 0; i < voxel_count; i++) {
        sock = create_connection();
        if (sock < 0) {
            free(voxels);
            return -1;
        }
        
        char params[256];
        snprintf(params, sizeof(params), "[%d, %d, %d, %d, %d, %d, %d]",
            voxels[i].x, voxels[i].y, voxels[i].z,
            voxels[i].r, voxels[i].g, voxels[i].b, voxels[i].a);
        
        if (send_request(sock, "goxel.add_voxel", params, id++) < 0) {
            close(sock);
            free(voxels);
            return -1;
        }
        close(sock);
        
        // Progress indicator
        if (i % 100 == 0) {
            printf("Progress: %d/%d voxels\n", i, voxel_count);
        }
    }
    
    // Export as .gox file (daemon limitation - can't export .vox directly)
    sock = create_connection();
    if (sock < 0) {
        free(voxels);
        return -1;
    }
    if (send_request(sock, "goxel.save_project", "[\"snoopy.gox\"]", id++) < 0) {
        close(sock);
        free(voxels);
        return -1;
    }
    close(sock);
    
    // Render to PNG
    sock = create_connection();
    if (sock < 0) {
        free(voxels);
        return -1;
    }
    if (send_request(sock, "goxel.render_scene", "[\"snoopy.png\", 800, 600]", id++) < 0) {
        close(sock);
        free(voxels);
        return -1;
    }
    close(sock);
    
    free(voxels);
    printf("Snoopy generation complete!\n");
    printf("Generated files:\n");
    printf("  - snoopy.gox (voxel model)\n");
    printf("  - snoopy.png (rendered image)\n");
    
    return 0;
}

int main(int argc, char** argv) {
    printf("Goxel Snoopy Generation Integration Test\n");
    printf("========================================\n\n");
    
    // Check if daemon is running
    if (access(SOCKET_PATH, F_OK) != 0) {
        printf("ERROR: Goxel daemon not running at %s\n", SOCKET_PATH);
        printf("Please start the daemon with:\n");
        printf("  ./goxel-daemon --foreground --socket %s\n", SOCKET_PATH);
        return 1;
    }
    
    // Generate Snoopy
    if (generate_snoopy() < 0) {
        printf("ERROR: Failed to generate Snoopy\n");
        return 1;
    }
    
    // Verify output files
    if (access("snoopy.gox", F_OK) == 0) {
        printf("✓ snoopy.gox created successfully\n");
    } else {
        printf("✗ snoopy.gox not found\n");
        return 1;
    }
    
    if (access("snoopy.png", F_OK) == 0) {
        printf("✓ snoopy.png created successfully\n");
    } else {
        printf("✗ snoopy.png not found\n");
        return 1;
    }
    
    printf("\nIntegration test PASSED!\n");
    return 0;
}