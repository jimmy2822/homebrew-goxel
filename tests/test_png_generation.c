/*
 * Goxel PNG Generation Test
 * 
 * Simple test to verify PNG rendering functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/goxel_png_test.sock"
#define BUFFER_SIZE 8192

// Helper function to send JSON-RPC request
static int send_request(int sock, const char* method, const char* params_str, int id) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}\n",
        method, params_str, id);
    
    printf("Request: %s", request);
    
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
    
    // Check for error in response
    if (strstr(response, "\"error\"")) {
        printf("ERROR in response!\n");
        return -1;
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

int main(int argc, char** argv) {
    printf("Goxel PNG Generation Test\n");
    printf("========================\n\n");
    
    // Check if daemon is running
    if (access(SOCKET_PATH, F_OK) != 0) {
        printf("ERROR: Goxel daemon not running at %s\n", SOCKET_PATH);
        printf("Please start the daemon with:\n");
        printf("  ./goxel-daemon --foreground --socket %s\n", SOCKET_PATH);
        return 1;
    }
    
    int sock;
    int id = 1;
    
    // Create project
    printf("Creating project...\n");
    sock = create_connection();
    if (sock < 0) return 1;
    if (send_request(sock, "goxel.create_project", "[\"PNGTest\", 32, 32, 32]", id++) < 0) {
        close(sock);
        return 1;
    }
    close(sock);
    
    // Add just a few voxels to create a simple shape
    printf("\nAdding test voxels...\n");
    
    // Create a simple 3x3x3 cube
    for (int x = 15; x <= 17; x++) {
        for (int y = 15; y <= 17; y++) {
            for (int z = 15; z <= 17; z++) {
                sock = create_connection();
                if (sock < 0) return 1;
                
                char params[256];
                snprintf(params, sizeof(params), "[%d, %d, %d, 255, 0, 0, 255]", x, y, z);
                
                if (send_request(sock, "goxel.add_voxel", params, id++) < 0) {
                    close(sock);
                    return 1;
                }
                close(sock);
            }
        }
    }
    
    printf("\n27 voxels added (3x3x3 red cube)\n");
    
    // Save as .gox file
    printf("\nSaving project...\n");
    sock = create_connection();
    if (sock < 0) return 1;
    if (send_request(sock, "goxel.save_project", "[\"test.gox\"]", id++) < 0) {
        close(sock);
        return 1;
    }
    close(sock);
    
    // Render to PNG
    printf("\nRendering to PNG...\n");
    sock = create_connection();
    if (sock < 0) return 1;
    if (send_request(sock, "goxel.render_scene", "[\"test.png\", 512, 512]", id++) < 0) {
        close(sock);
        return 1;
    }
    close(sock);
    
    // Verify output files
    printf("\nVerifying output files...\n");
    
    if (access("test.gox", F_OK) == 0) {
        printf("✓ test.gox created successfully\n");
    } else {
        printf("✗ test.gox not found\n");
        return 1;
    }
    
    if (access("test.png", F_OK) == 0) {
        printf("✓ test.png created successfully\n");
        
        // Check file size to ensure it's not empty
        FILE* fp = fopen("test.png", "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fclose(fp);
            printf("  PNG file size: %ld bytes\n", size);
            
            if (size < 100) {
                printf("  WARNING: PNG file seems too small\n");
                return 1;
            }
        }
    } else {
        printf("✗ test.png not found\n");
        return 1;
    }
    
    printf("\nTest PASSED!\n");
    return 0;
}