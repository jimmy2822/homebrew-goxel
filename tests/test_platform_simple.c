/* Simple Platform Test for Goxel v13 Cross-platform Validation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple test that just checks basic compilation and linking
int main() {
    printf("Goxel v13 Platform Test\n");
    printf("Platform: ");
    
#ifdef __APPLE__
    printf("macOS\n");
#elif defined(__linux__)
    printf("Linux\n");
#elif defined(_WIN32)
    printf("Windows\n");
#else
    printf("Unknown\n");
#endif

    printf("Compiler: ");
#ifdef __clang__
    printf("Clang\n");
#elif defined(__GNUC__)
    printf("GCC\n");
#elif defined(_MSC_VER)
    printf("MSVC\n");
#else
    printf("Unknown\n");
#endif

    printf("Architecture: ");
#ifdef __x86_64__
    printf("x86_64\n");
#elif defined(__aarch64__) || defined(__arm64__)
    printf("ARM64\n");
#elif defined(__i386__)
    printf("x86\n");
#else
    printf("Unknown\n");
#endif

    printf("Basic compilation test: PASS\n");
    return 0;
}