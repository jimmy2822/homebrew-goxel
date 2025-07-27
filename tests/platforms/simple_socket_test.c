/**
 * Simple socket creation test for macOS ARM64.
 * Tests basic Unix domain socket creation without threading.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

int main() {
    const char *socket_path = "/tmp/goxel_simple_test.sock";
    
    printf("=== Simple Socket Test ===\n");
    printf("Testing socket creation on macOS ARM64\n");
    printf("Socket path: %s\n\n", socket_path);
    
    // Remove existing socket
    unlink(socket_path);
    
    // Step 1: Create socket
    printf("1. Creating socket...\n");
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("   FAILED: %s (errno=%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("   SUCCESS: fd=%d\n", fd);
    
    // Step 2: Prepare address
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    // Step 3: Bind socket
    printf("2. Binding socket to %s...\n", socket_path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("   FAILED: %s (errno=%d)\n", strerror(errno), errno);
        close(fd);
        return 1;
    }
    printf("   SUCCESS\n");
    
    // Step 4: Check if file exists
    printf("3. Checking if socket file exists...\n");
    struct stat st;
    if (stat(socket_path, &st) == 0) {
        printf("   SUCCESS: File exists\n");
        printf("   - Type: %s\n", S_ISSOCK(st.st_mode) ? "Socket" : "Other");
        printf("   - Mode: %o\n", st.st_mode & 0777);
        printf("   - Size: %lld bytes\n", (long long)st.st_size);
    } else {
        printf("   FAILED: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    
    // Step 5: Listen
    printf("4. Starting to listen...\n");
    if (listen(fd, 5) == -1) {
        printf("   FAILED: %s (errno=%d)\n", strerror(errno), errno);
        close(fd);
        unlink(socket_path);
        return 1;
    }
    printf("   SUCCESS\n");
    
    // Step 6: List files in /tmp to verify
    printf("5. Listing /tmp/goxel* files:\n");
    system("ls -la /tmp/goxel* 2>/dev/null || echo '   No goxel files found'");
    
    // Cleanup
    printf("\n6. Cleaning up...\n");
    close(fd);
    unlink(socket_path);
    printf("   Done\n");
    
    printf("\n=== TEST PASSED ===\n");
    printf("Unix domain sockets work correctly on this system.\n");
    
    return 0;
}