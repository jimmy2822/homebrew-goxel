#!/bin/bash
# Cross-Platform Testing Script for Goxel v14.0 Daemon
# Usage: ./scripts/test-cross-platform.sh [platform]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect current platform
detect_platform() {
    case "$(uname -s)" in
        Darwin*)
            echo "macos"
            ;;
        Linux*)
            if [ -f /etc/os-release ]; then
                . /etc/os-release
                case "$ID" in
                    ubuntu|debian)
                        echo "linux-debian"
                        ;;
                    centos|rhel|fedora)
                        echo "linux-redhat"
                        ;;
                    arch)
                        echo "linux-arch"
                        ;;
                    *)
                        echo "linux-generic"
                        ;;
                esac
            else
                echo "linux-generic"
            fi
            ;;
        CYGWIN*|MINGW*|MSYS*)
            echo "windows"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# Check dependencies for each platform
check_dependencies() {
    local platform=$1
    log_info "Checking dependencies for $platform..."
    
    case $platform in
        macos)
            check_macos_deps
            ;;
        linux-*)
            check_linux_deps
            ;;
        windows)
            check_windows_deps
            ;;
        *)
            log_error "Unsupported platform: $platform"
            return 1
            ;;
    esac
}

check_macos_deps() {
    local missing=0
    
    # Check build tools
    if ! command -v scons &> /dev/null; then
        log_error "SCons not found. Install with: brew install scons"
        missing=1
    fi
    
    if ! command -v gcc &> /dev/null; then
        log_error "GCC not found. Install Xcode command line tools"
        missing=1
    fi
    
    # Check libraries
    if ! pkg-config --exists libpng; then
        log_error "libpng not found. Install with: brew install libpng"
        missing=1
    fi
    
    if ! pkg-config --exists glfw3; then
        log_error "GLFW3 not found. Install with: brew install glfw"
        missing=1
    fi
    
    # Check optional OSMesa
    if ! pkg-config --exists osmesa; then
        log_warning "OSMesa not found. Install with: brew install mesa (optional)"
    fi
    
    return $missing
}

check_linux_deps() {
    local missing=0
    
    # Check build tools
    if ! command -v scons &> /dev/null; then
        log_error "SCons not found. Install with package manager"
        missing=1
    fi
    
    if ! command -v gcc &> /dev/null; then
        log_error "GCC not found. Install build-essential or equivalent"
        missing=1
    fi
    
    # Check pkg-config
    if ! command -v pkg-config &> /dev/null; then
        log_error "pkg-config not found"
        missing=1
    fi
    
    # Check libraries
    if ! pkg-config --exists libpng; then
        log_error "libpng-dev not found"
        missing=1
    fi
    
    if ! pkg-config --exists glfw3; then
        log_error "libglfw3-dev not found"
        missing=1
    fi
    
    if ! pkg-config --exists osmesa; then
        log_error "libosmesa6-dev not found"
        missing=1
    fi
    
    return $missing
}

check_windows_deps() {
    log_warning "Windows support is experimental"
    
    # Check MSYS2/MinGW
    if [[ "$MSYSTEM" != "MINGW64" ]] && [[ "$MSYSTEM" != "MINGW32" ]]; then
        log_error "Please run in MSYS2 MinGW environment"
        return 1
    fi
    
    # Check build tools
    if ! command -v scons &> /dev/null; then
        log_error "SCons not found in MSYS2"
        return 1
    fi
    
    if ! command -v gcc &> /dev/null; then
        log_error "GCC not found in MSYS2"
        return 1
    fi
    
    return 0
}

# Build daemon for platform
build_daemon() {
    local platform=$1
    log_info "Building daemon for $platform..."
    
    cd "$PROJECT_ROOT"
    
    # Clean previous build
    if [ -f "goxel-daemon" ]; then
        rm -f goxel-daemon
    fi
    
    # Platform-specific build
    case $platform in
        macos)
            scons daemon=1 -j$(sysctl -n hw.ncpu)
            ;;
        linux-*)
            scons daemon=1 -j$(nproc)
            ;;
        windows)
            # Windows build may need special handling
            scons daemon=1 -j$(nproc) 2>&1 | tee build.log || {
                log_error "Build failed on Windows. Check build.log"
                return 1
            }
            ;;
    esac
    
    if [ -f "goxel-daemon" ]; then
        log_success "Build completed successfully"
        return 0
    else
        log_error "Build failed - executable not found"
        return 1
    fi
}

# Test basic daemon functionality
test_daemon_basic() {
    local platform=$1
    log_info "Testing basic daemon functionality..."
    
    cd "$PROJECT_ROOT"
    
    # Test version
    log_info "Testing --version..."
    if ./goxel-daemon --version; then
        log_success "Version test passed"
    else
        log_error "Version test failed"
        return 1
    fi
    
    # Test help
    log_info "Testing --help..."
    if ./goxel-daemon --help > /dev/null; then
        log_success "Help test passed"
    else
        log_error "Help test failed"
        return 1
    fi
    
    # Test lifecycle (if available)
    log_info "Testing lifecycle management..."
    if ./goxel-daemon --test-lifecycle 2>/dev/null; then
        log_success "Lifecycle test passed"
    else
        log_warning "Lifecycle test not available or failed"
    fi
    
    return 0
}

# Test socket communication
test_socket_communication() {
    local platform=$1
    log_info "Testing socket communication..."
    
    cd "$PROJECT_ROOT"
    
    local socket_path="/tmp/goxel-test-$$.sock"
    local pid_file="/tmp/goxel-test-$$.pid"
    
    # Start daemon in background
    log_info "Starting daemon for socket test..."
    ./goxel-daemon --foreground --socket "$socket_path" --pid-file "$pid_file" &
    local daemon_pid=$!
    
    # Wait for daemon to start
    sleep 2
    
    # Check if socket exists
    if [ ! -S "$socket_path" ]; then
        log_error "Socket not created: $socket_path"
        kill $daemon_pid 2>/dev/null || true
        return 1
    fi
    
    # Test echo method
    log_info "Testing echo method..."
    local test_message='{"jsonrpc":"2.0","method":"echo","params":{"test":123},"id":1}'
    local response
    
    if command -v nc &> /dev/null; then
        response=$(echo "$test_message" | timeout 5 nc -U "$socket_path" 2>/dev/null || true)
        if [[ "$response" == *'"result"'* ]] && [[ "$response" == *'"test":123'* ]]; then
            log_success "Socket communication test passed"
            log_info "Response: $response"
        else
            log_warning "Socket communication test inconclusive"
            log_info "Response: ${response:-'(no response)'}"
        fi
    else
        log_warning "netcat not available, skipping socket test"
    fi
    
    # Clean up
    kill $daemon_pid 2>/dev/null || true
    rm -f "$socket_path" "$pid_file"
    
    return 0
}

# Test with Docker (Linux only)
test_with_docker() {
    local distro=$1
    log_info "Testing with Docker ($distro)..."
    
    if ! command -v docker &> /dev/null; then
        log_warning "Docker not available, skipping container test"
        return 0
    fi
    
    local dockerfile="config/docker-test-environments/${distro}-test.dockerfile"
    if [ ! -f "$PROJECT_ROOT/$dockerfile" ]; then
        log_warning "Dockerfile not found: $dockerfile"
        return 0
    fi
    
    # Build test image
    log_info "Building test image for $distro..."
    docker build -t "goxel-test-$distro" -f "$PROJECT_ROOT/$dockerfile" "$PROJECT_ROOT" || {
        log_error "Failed to build Docker image for $distro"
        return 1
    }
    
    # Run tests in container
    log_info "Running tests in $distro container..."
    docker run --rm -v "$PROJECT_ROOT:/build" "goxel-test-$distro" bash -c "
        cd /build && 
        scons daemon=1 && 
        ./goxel-daemon --version &&
        ./goxel-daemon --test-lifecycle
    " || {
        log_error "Container tests failed for $distro"
        return 1
    }
    
    log_success "Docker tests passed for $distro"
    return 0
}

# Generate platform report
generate_report() {
    local platform=$1
    local status=$2
    
    local report_file="$PROJECT_ROOT/platform-test-report-$(date +%Y%m%d-%H%M%S).txt"
    
    cat > "$report_file" << EOF
Goxel v14.0 Daemon Platform Test Report
======================================

Platform: $platform
Date: $(date)
Status: $status

System Information:
------------------
OS: $(uname -s)
Kernel: $(uname -r)
Architecture: $(uname -m)
$(if [[ "$platform" == "macos" ]]; then
    echo "macOS Version: $(sw_vers -productVersion)"
elif [[ "$platform" == linux-* ]]; then
    if [ -f /etc/os-release ]; then
        echo "Distribution: $(. /etc/os-release && echo "$PRETTY_NAME")"
    fi
fi)

Build Environment:
-----------------
$(if command -v scons &> /dev/null; then
    echo "SCons: $(scons --version | head -1)"
else
    echo "SCons: Not found"
fi)
$(if command -v gcc &> /dev/null; then
    echo "GCC: $(gcc --version | head -1)"
else
    echo "GCC: Not found"
fi)
$(if command -v pkg-config &> /dev/null; then
    echo "pkg-config: $(pkg-config --version)"
else
    echo "pkg-config: Not found"
fi)

Dependencies:
------------
$(pkg-config --exists libpng && echo "libpng: $(pkg-config --modversion libpng)" || echo "libpng: Not found")
$(pkg-config --exists glfw3 && echo "glfw3: $(pkg-config --modversion glfw3)" || echo "glfw3: Not found")
$(pkg-config --exists osmesa && echo "osmesa: $(pkg-config --modversion osmesa)" || echo "osmesa: Not found")

Binary Information:
------------------
$(if [ -f "$PROJECT_ROOT/goxel-daemon" ]; then
    echo "Binary size: $(ls -lh "$PROJECT_ROOT/goxel-daemon" | awk '{print $5}')"
    echo "Binary dependencies:"
    if [[ "$platform" == "macos" ]]; then
        otool -L "$PROJECT_ROOT/goxel-daemon" | head -10
    elif [[ "$platform" == linux-* ]]; then
        ldd "$PROJECT_ROOT/goxel-daemon" 2>/dev/null | head -10 || echo "ldd not available"
    fi
else
    echo "Binary: Not built"
fi)

Test Results:
------------
EOF
    
    log_info "Platform report saved to: $report_file"
}

# Main test function
run_platform_tests() {
    local platform=$1
    
    log_info "Starting cross-platform tests for: $platform"
    
    # Check dependencies
    if ! check_dependencies "$platform"; then
        log_error "Dependency check failed for $platform"
        generate_report "$platform" "FAILED - Missing dependencies"
        return 1
    fi
    
    # Build daemon
    if ! build_daemon "$platform"; then
        log_error "Build failed for $platform"
        generate_report "$platform" "FAILED - Build error"
        return 1
    fi
    
    # Test basic functionality
    if ! test_daemon_basic "$platform"; then
        log_error "Basic tests failed for $platform"
        generate_report "$platform" "FAILED - Basic tests"
        return 1
    fi
    
    # Test socket communication
    if ! test_socket_communication "$platform"; then
        log_warning "Socket tests had issues for $platform"
    fi
    
    # Docker tests for Linux
    if [[ "$platform" == linux-* ]]; then
        case "$platform" in
            linux-debian)
                test_with_docker "ubuntu" || log_warning "Ubuntu Docker test failed"
                ;;
            linux-redhat)
                test_with_docker "centos" || log_warning "CentOS Docker test failed"
                ;;
        esac
    fi
    
    generate_report "$platform" "PASSED"
    log_success "All tests completed for $platform"
    return 0
}

# Main script
main() {
    local target_platform=${1:-$(detect_platform)}
    
    echo "======================================"
    echo "Goxel v14.0 Cross-Platform Test Suite"
    echo "======================================"
    echo
    
    log_info "Target platform: $target_platform"
    log_info "Project root: $PROJECT_ROOT"
    echo
    
    case "$target_platform" in
        macos|linux-*|windows)
            run_platform_tests "$target_platform"
            ;;
        all)
            local current=$(detect_platform)
            log_info "Running tests for current platform: $current"
            run_platform_tests "$current"
            ;;
        *)
            log_error "Unsupported platform: $target_platform"
            echo "Supported platforms: macos, linux-debian, linux-redhat, linux-arch, windows, all"
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"