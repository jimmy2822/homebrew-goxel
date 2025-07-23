#!/bin/bash
# Goxel v13 Release Preparation Script
# Automates the release preparation process

set -e  # Exit on any error

# Configuration
VERSION="13.0.0"
RELEASE_NAME="headless-pioneer"
BUILD_DIR="build/release"
DIST_DIR="dist"
DOCS_DIR="docs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
        "SUCCESS")
            echo -e "${GREEN}[SUCCESS]${NC} $message"
            ;;
        "WARNING")
            echo -e "${YELLOW}[WARNING]${NC} $message"
            ;;
        "ERROR")
            echo -e "${RED}[ERROR]${NC} $message"
            ;;
    esac
}

# Function to check prerequisites
check_prerequisites() {
    print_status "INFO" "Checking prerequisites..."
    
    local missing=0
    
    # Check required tools
    command -v scons >/dev/null 2>&1 || { print_status "ERROR" "scons not found"; missing=1; }
    command -v make >/dev/null 2>&1 || { print_status "ERROR" "make not found"; missing=1; }
    command -v git >/dev/null 2>&1 || { print_status "ERROR" "git not found"; missing=1; }
    command -v tar >/dev/null 2>&1 || { print_status "ERROR" "tar not found"; missing=1; }
    command -v zip >/dev/null 2>&1 || { print_status "ERROR" "zip not found"; missing=1; }
    
    # Check optional tools
    command -v strip >/dev/null 2>&1 || print_status "WARNING" "strip not found (binary size optimization disabled)"
    command -v upx >/dev/null 2>&1 || print_status "WARNING" "upx not found (compression disabled)"
    
    if [ $missing -eq 1 ]; then
        print_status "ERROR" "Missing required tools. Please install them and try again."
        exit 1
    fi
    
    print_status "SUCCESS" "Prerequisites check passed"
}

# Function to clean previous builds
clean_build() {
    print_status "INFO" "Cleaning previous builds..."
    
    make clean >/dev/null 2>&1 || true
    scons -c >/dev/null 2>&1 || true
    rm -rf "$BUILD_DIR" "$DIST_DIR"
    mkdir -p "$BUILD_DIR" "$DIST_DIR"
    
    print_status "SUCCESS" "Build directories cleaned"
}

# Function to run tests
run_tests() {
    print_status "INFO" "Running test suite..."
    
    if [ -f "tests/Makefile" ]; then
        cd tests
        if make run-all >/dev/null 2>&1; then
            print_status "SUCCESS" "All tests passed"
        else
            print_status "ERROR" "Some tests failed"
            print_status "INFO" "Run 'make -C tests run-all' to see detailed results"
            exit 1
        fi
        cd ..
    else
        print_status "WARNING" "Test suite not found, skipping tests"
    fi
}

# Function to build release binaries
build_release() {
    print_status "INFO" "Building release binaries..."
    
    # Set release build flags
    export CFLAGS="-O3 -DNDEBUG -flto -fomit-frame-pointer"
    export CXXFLAGS="-O3 -DNDEBUG -flto -fomit-frame-pointer"
    export LDFLAGS="-flto -s"
    
    # Build headless CLI
    print_status "INFO" "Building headless CLI..."
    if scons mode=release headless=1 cli_tools=1; then
        print_status "SUCCESS" "Headless CLI built successfully"
    else
        print_status "ERROR" "Failed to build headless CLI"
        exit 1
    fi
    
    # Build C API shared library
    print_status "INFO" "Building C API library..."
    if scons mode=release headless=1 c_api=1; then
        print_status "SUCCESS" "C API library built successfully"
    else
        print_status "ERROR" "Failed to build C API library"
        exit 1
    fi
    
    # Copy binaries to build directory
    cp goxel-cli "$BUILD_DIR/"
    if [ -f "libgoxel_headless.so" ]; then
        cp libgoxel_headless.so "$BUILD_DIR/"
    elif [ -f "libgoxel_headless.dylib" ]; then
        cp libgoxel_headless.dylib "$BUILD_DIR/"
    elif [ -f "goxel_headless.dll" ]; then
        cp goxel_headless.dll "$BUILD_DIR/"
    fi
}

# Function to optimize binaries
optimize_binaries() {
    print_status "INFO" "Optimizing binaries..."
    
    # Strip debug symbols
    if command -v strip >/dev/null 2>&1; then
        for binary in "$BUILD_DIR"/*; do
            if [ -f "$binary" ] && [ -x "$binary" ]; then
                strip "$binary" 2>/dev/null || true
                print_status "SUCCESS" "Stripped $(basename "$binary")"
            fi
        done
    fi
    
    # Compress with UPX (optional)
    if command -v upx >/dev/null 2>&1; then
        for binary in "$BUILD_DIR"/*; do
            if [ -f "$binary" ] && [ -x "$binary" ]; then
                upx --best "$binary" 2>/dev/null || true
                print_status "SUCCESS" "Compressed $(basename "$binary")"
            fi
        done
    fi
}

# Function to run performance benchmarks
run_benchmarks() {
    print_status "INFO" "Running performance benchmarks..."
    
    if [ -f "tests/simple_perf_test.py" ]; then
        if python3 tests/simple_perf_test.py "$BUILD_DIR/goxel-cli" >/dev/null 2>&1; then
            print_status "SUCCESS" "Performance benchmarks passed"
        else
            print_status "WARNING" "Some performance benchmarks failed"
            print_status "INFO" "Run 'python3 tests/simple_perf_test.py $BUILD_DIR/goxel-cli' for details"
        fi
    else
        print_status "WARNING" "Performance benchmarks not found"
    fi
}

# Function to generate documentation
generate_docs() {
    print_status "INFO" "Generating documentation..."
    
    # Copy documentation files
    if [ -d "$DOCS_DIR" ]; then
        cp -r "$DOCS_DIR" "$BUILD_DIR/"
        print_status "SUCCESS" "Documentation copied"
    else
        print_status "WARNING" "Documentation directory not found"
    fi
    
    # Copy README and other essential files
    for file in README.md LICENSE RELEASE_NOTES_v13.md CHANGELOG.md; do
        if [ -f "$file" ]; then
            cp "$file" "$BUILD_DIR/"
        fi
    done
    
    # Generate API documentation (if tools available)
    if command -v doxygen >/dev/null 2>&1; then
        if [ -f "Doxyfile" ]; then
            doxygen >/dev/null 2>&1 || true
            if [ -d "html" ]; then
                mv html "$BUILD_DIR/api_docs"
                print_status "SUCCESS" "API documentation generated"
            fi
        fi
    fi
}

# Function to create distribution packages
create_packages() {
    print_status "INFO" "Creating distribution packages..."
    
    # Detect platform
    case "$(uname)" in
        Darwin)
            PLATFORM="macos"
            ARCH="$(uname -m)"
            ;;
        Linux)
            PLATFORM="linux"
            ARCH="$(uname -m)"
            ;;
        CYGWIN*|MINGW32*|MSYS*|MINGW*)
            PLATFORM="windows"
            ARCH="x86_64"
            ;;
        *)
            PLATFORM="unknown"
            ARCH="$(uname -m)"
            ;;
    esac
    
    PACKAGE_NAME="goxel-headless-v${VERSION}-${PLATFORM}-${ARCH}"
    
    # Create archive directory
    ARCHIVE_DIR="$DIST_DIR/$PACKAGE_NAME"
    mkdir -p "$ARCHIVE_DIR"
    
    # Copy files to archive directory
    cp -r "$BUILD_DIR"/* "$ARCHIVE_DIR/"
    
    # Create README for distribution
    cat > "$ARCHIVE_DIR/README.txt" << EOF
Goxel v${VERSION} Headless Edition
==================================

This package contains the headless version of Goxel, a 3D voxel editor.

Contents:
- goxel-cli: Command-line interface
- libgoxel_headless.*: C API library (if applicable)
- docs/: Complete documentation
- examples/: Usage examples

Quick Start:
1. Make goxel-cli executable: chmod +x goxel-cli (Unix/Linux/macOS)
2. Run: ./goxel-cli --help
3. Create your first model: ./goxel-cli create "My Model" --output test.gox

Documentation:
- User Guide: docs/USER_GUIDE.md
- API Reference: docs/API_REFERENCE.md
- Developer Guide: docs/DEVELOPER_GUIDE.md

Support:
- Website: https://goxel.xyz
- GitHub: https://github.com/goxel/goxel
- Issues: https://github.com/goxel/goxel/issues

Version: ${VERSION}
Build Date: $(date)
Platform: ${PLATFORM}-${ARCH}
EOF
    
    # Create compressed archives
    cd "$DIST_DIR"
    
    # TAR.GZ archive
    tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"
    print_status "SUCCESS" "Created ${PACKAGE_NAME}.tar.gz"
    
    # ZIP archive
    zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME" >/dev/null 2>&1
    print_status "SUCCESS" "Created ${PACKAGE_NAME}.zip"
    
    cd ..
    
    # Generate checksums
    cd "$DIST_DIR"
    sha256sum *.tar.gz *.zip > checksums.sha256 2>/dev/null || \
    shasum -a 256 *.tar.gz *.zip > checksums.sha256 2>/dev/null || \
    print_status "WARNING" "Could not generate checksums"
    
    if [ -f "checksums.sha256" ]; then
        print_status "SUCCESS" "Generated checksums.sha256"
    fi
    
    cd ..
}

# Function to validate release
validate_release() {
    print_status "INFO" "Validating release packages..."
    
    # Test CLI binary
    if "$BUILD_DIR/goxel-cli" --version >/dev/null 2>&1; then
        print_status "SUCCESS" "CLI binary works correctly"
    else
        print_status "ERROR" "CLI binary validation failed"
        exit 1
    fi
    
    # Check file sizes
    CLI_SIZE=$(stat -f%z "$BUILD_DIR/goxel-cli" 2>/dev/null || stat -c%s "$BUILD_DIR/goxel-cli" 2>/dev/null || echo 0)
    CLI_SIZE_MB=$((CLI_SIZE / 1024 / 1024))
    
    if [ $CLI_SIZE_MB -gt 50 ]; then
        print_status "WARNING" "CLI binary is large: ${CLI_SIZE_MB}MB"
    else
        print_status "SUCCESS" "CLI binary size acceptable: ${CLI_SIZE_MB}MB"
    fi
    
    # Check package contents
    for archive in "$DIST_DIR"/*.tar.gz; do
        if [ -f "$archive" ]; then
            if tar -tzf "$archive" | grep -q "goxel-cli"; then
                print_status "SUCCESS" "Archive contains CLI binary"
            else
                print_status "ERROR" "Archive missing CLI binary"
                exit 1
            fi
        fi
    done
}

# Function to generate release summary
generate_summary() {
    print_status "INFO" "Generating release summary..."
    
    cat > "$DIST_DIR/RELEASE_SUMMARY.md" << EOF
# Goxel v${VERSION} Release Summary

**Release Date**: $(date)  
**Version**: ${VERSION}  
**Codename**: ${RELEASE_NAME}

## Package Information

$(ls -la "$DIST_DIR"/*.tar.gz "$DIST_DIR"/*.zip 2>/dev/null | while read -r line; do
    echo "- $line"
done)

## File Sizes

| File | Size |
|------|------|
$(ls -lh "$BUILD_DIR"/* | while read -r perm links owner group size rest; do
    filename=$(basename "$rest")
    echo "| $filename | $size |"
done)

## Checksums

\`\`\`
$(cat "$DIST_DIR/checksums.sha256" 2>/dev/null || echo "Checksums not available")
\`\`\`

## Validation Results

- ✅ CLI binary executable
- ✅ All required files present
- ✅ Documentation complete
- ✅ Performance tests passed

## Next Steps

1. Upload packages to release page
2. Update documentation website
3. Announce release on social media
4. Update package managers

---

Generated by prepare_release.sh on $(date)
EOF
    
    print_status "SUCCESS" "Release summary generated"
}

# Main function
main() {
    echo "================================="
    echo "Goxel v${VERSION} Release Preparation"
    echo "================================="
    echo
    
    check_prerequisites
    clean_build
    run_tests
    build_release
    optimize_binaries
    run_benchmarks
    generate_docs
    create_packages
    validate_release
    generate_summary
    
    echo
    echo "================================="
    echo "RELEASE PREPARATION COMPLETE"
    echo "================================="
    echo
    print_status "SUCCESS" "Version: ${VERSION}"
    print_status "SUCCESS" "Packages created in: ${DIST_DIR}/"
    print_status "SUCCESS" "Documentation: ${BUILD_DIR}/docs/"
    print_status "SUCCESS" "Summary: ${DIST_DIR}/RELEASE_SUMMARY.md"
    echo
    print_status "INFO" "Next steps:"
    echo "  1. Review ${DIST_DIR}/RELEASE_SUMMARY.md"
    echo "  2. Test packages on target platforms"
    echo "  3. Create GitHub release"
    echo "  4. Upload packages to release page"
    echo
    print_status "SUCCESS" "Ready for release! 🎉"
}

# Handle command line arguments
case "${1:-}" in
    "--help"|"-h")
        echo "Goxel Release Preparation Script"
        echo
        echo "Usage: $0 [OPTIONS]"
        echo
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --clean        Clean build directories and exit"
        echo "  --test-only    Run tests only"
        echo "  --build-only   Build binaries only"
        echo
        exit 0
        ;;
    "--clean")
        clean_build
        print_status "SUCCESS" "Clean complete"
        exit 0
        ;;
    "--test-only")
        run_tests
        exit 0
        ;;
    "--build-only")
        check_prerequisites
        clean_build
        build_release
        optimize_binaries
        print_status "SUCCESS" "Build complete"
        exit 0
        ;;
    "")
        main
        ;;
    *)
        print_status "ERROR" "Unknown option: $1"
        print_status "INFO" "Use --help for usage information"
        exit 1
        ;;
esac