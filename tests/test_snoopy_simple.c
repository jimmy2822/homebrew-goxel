/*
 * Goxel Simple Snoopy Generation Integration Test
 * 
 * This test generates a simplified ~200 voxel Snoopy model for faster testing
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
    
    // Only print responses for key operations
    if (strstr(method, "create_project") || strstr(method, "save_project") || strstr(method, "render_scene")) {
        printf("Response: %s\n", response);
    }
    
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

// Generate simplified Snoopy voxel model (~200 voxels)
static int generate_simple_snoopy() {
    Voxel* voxels = malloc(sizeof(Voxel) * 500);
    int voxel_count = 0;
    
    // Design parameters
    int base_x = 32, base_y = 32, base_z = 32;
    
    // === BODY (white, simple box) ===
    for (int x = -4; x <= 4; x += 2) {
        for (int y = -8; y <= 0; y += 2) {
            for (int z = -3; z <= 3; z += 2) {
                voxels[voxel_count++] = (Voxel){
                    base_x + x, base_y + y, base_z + z,
                    COLOR_WHITE
                };
            }
        }
    }
    
    // === HEAD (white, small sphere) ===
    int head_y = base_y + 5;
    for (int x = -3; x <= 3; x += 2) {
        for (int y = -3; y <= 3; y += 2) {
            for (int z = -3; z <= 3; z += 2) {
                if (abs(x) + abs(y) + abs(z) <= 6) {
                    voxels[voxel_count++] = (Voxel){
                        base_x + x, head_y + y, base_z + z,
                        COLOR_WHITE
                    };
                }
            }
        }
    }
    
    // === SNOUT (white, simple protrusion) ===
    for (int y = 0; y <= 4; y += 2) {
        voxels[voxel_count++] = (Voxel){
            base_x, head_y + 3 + y, base_z,
            COLOR_WHITE
        };
    }
    
    // === EARS (black) ===
    // Left ear
    for (int z = -4; z <= -2; z += 2) {
        voxels[voxel_count++] = (Voxel){
            base_x - 5, head_y, base_z + z,
            COLOR_BLACK
        };
    }
    
    // Right ear
    for (int z = -4; z <= -2; z += 2) {
        voxels[voxel_count++] = (Voxel){
            base_x + 5, head_y, base_z + z,
            COLOR_BLACK
        };
    }
    
    // === NOSE (black) ===
    voxels[voxel_count++] = (Voxel){
        base_x, head_y + 8, base_z,
        COLOR_BLACK
    };
    
    // === EYES (black) ===
    voxels[voxel_count++] = (Voxel){base_x - 2, head_y + 2, base_z + 3, COLOR_BLACK};
    voxels[voxel_count++] = (Voxel){base_x + 2, head_y + 2, base_z + 3, COLOR_BLACK};
    
    // === COLLAR (red, simple ring) ===
    int collar_y = base_y + 1;
    for (int x = -4; x <= 4; x += 4) {
        voxels[voxel_count++] = (Voxel){
            base_x + x, collar_y, base_z - 3,
            COLOR_RED
        };
        voxels[voxel_count++] = (Voxel){
            base_x + x, collar_y, base_z + 3,
            COLOR_RED
        };
    }
    for (int z = -2; z <= 2; z += 4) {
        voxels[voxel_count++] = (Voxel){
            base_x - 4, collar_y, base_z + z,
            COLOR_RED
        };
        voxels[voxel_count++] = (Voxel){
            base_x + 4, collar_y, base_z + z,
            COLOR_RED
        };
    }
    
    // === LEGS (white, simple) ===
    // Four legs
    int leg_positions[4][2] = {{-3, -2}, {3, -2}, {-3, 2}, {3, 2}};
    for (int i = 0; i < 4; i++) {
        for (int y = -12; y <= -9; y += 3) {
            voxels[voxel_count++] = (Voxel){
                base_x + leg_positions[i][0], 
                base_y + y, 
                base_z + leg_positions[i][1],
                COLOR_WHITE
            };
        }
        // Paws (black)
        voxels[voxel_count++] = (Voxel){
            base_x + leg_positions[i][0], 
            base_y - 13, 
            base_z + leg_positions[i][1],
            COLOR_BLACK
        };
    }
    
    // === TAIL (white with black tip) ===
    for (int y = -8; y <= -6; y += 2) {
        voxels[voxel_count++] = (Voxel){
            base_x, base_y + y, base_z + 5,
            (y == -6) ? 0 : 255, (y == -6) ? 0 : 255, (y == -6) ? 0 : 255, 255
        };
    }
    
    printf("Total voxels to generate: %d\n", voxel_count);
    
    // Now send all voxels to goxel daemon
    int sock;
    int id = 1;
    
    // Create project
    sock = create_connection();
    if (sock < 0) return -1;
    if (send_request(sock, "goxel.create_project", "[\"SimpleSnoopy\", 64, 64, 64]", id++) < 0) {
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
        if (i % 20 == 0) {
            printf("Progress: %d/%d voxels\n", i, voxel_count);
        }
    }
    
    // Save as .gox file
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
    printf("Simple Snoopy generation complete!\n");
    
    return 0;
}

int main(int argc, char** argv) {
    printf("Goxel Simple Snoopy Generation Test\n");
    printf("===================================\n\n");
    
    // Check if daemon is running
    if (access(SOCKET_PATH, F_OK) != 0) {
        printf("ERROR: Goxel daemon not running at %s\n", SOCKET_PATH);
        printf("Please start the daemon with:\n");
        printf("  ./goxel-daemon --foreground --socket %s\n", SOCKET_PATH);
        return 1;
    }
    
    // Generate simplified Snoopy
    if (generate_simple_snoopy() < 0) {
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
    
    printf("\nSimple test PASSED!\n");
    return 0;
}