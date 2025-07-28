/*
 * Goxel v14.0 CLI Baseline Performance Tool
 * Minimal implementation for benchmark comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("Goxel CLI Baseline v14.0\n");
        return 0;
    }
    
    printf("CLI baseline tool for performance comparison\n");
    return 0;
}