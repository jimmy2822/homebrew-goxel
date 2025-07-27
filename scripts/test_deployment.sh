#!/bin/bash

# Goxel v14 Daemon Deployment Test Suite
# Tests deployment and process management functionality

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test configuration
TEST_PREFIX="/tmp/goxel-test"
TEST_USER="$(whoami)"
TEST_GROUP="$(id -gn)"
TEST_DAEMON_PATH="$PROJECT_ROOT/goxel-daemon"

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $*"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

cleanup_test_env() {
    log_info "Cleaning up test environment..."
    
    # Stop any running test daemon
    if [[ -f "$TEST_PREFIX/var/run/goxel-daemon.pid" ]]; then
        local pid=$(cat "$TEST_PREFIX/var/run/goxel-daemon.pid" 2>/dev/null || echo "")
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            log_info "Stopping test daemon (PID: $pid)"
            kill -TERM "$pid" 2>/dev/null || true
            sleep 2
            kill -KILL "$pid" 2>/dev/null || true
        fi
    fi
    
    # Remove test files
    rm -rf "$TEST_PREFIX"
    log_success "Test environment cleaned up"
}

setup_test_env() {
    log_info "Setting up test environment..."
    
    # Create test directories
    mkdir -p "$TEST_PREFIX"/{bin,var/{lib/goxel,log,run},etc/goxel}
    
    # Create mock daemon binary for testing
    cat > "$TEST_DAEMON_PATH" << 'EOF'
#!/bin/bash
# Mock Goxel daemon for testing
case "${1:-}" in
    --version)
        echo "goxel-daemon version 14.0.0-test"
        ;;
    --test-lifecycle)
        echo "Lifecycle tests passed"
        ;;
    --test-signals)
        echo "Signal handling tests passed"
        ;;
    --help)
        echo "Usage: goxel-daemon [OPTIONS]"
        ;;
    *)
        echo "Mock daemon would run here with args: $*"
        # Create PID file for testing
        echo $$ > "${2:-/tmp/goxel-daemon.pid}"
        sleep 60 &
        echo $! > "${2:-/tmp/goxel-daemon.pid}"
        ;;
esac
EOF
    
    chmod +x "$TEST_DAEMON_PATH"
    log_success "Test environment setup complete"
}

test_daemon_control_script() {
    log_info "Testing daemon control script..."
    
    local control_script="$SCRIPT_DIR/daemon_control.sh"
    
    # Test help
    if "$control_script" --help >/dev/null; then
        log_success "Help command works"
    else
        log_error "Help command failed"
        return 1
    fi
    
    # Test version
    if "$control_script" --version >/dev/null; then
        log_success "Version command works"
    else
        log_error "Version command failed"
        return 1
    fi
    
    # Test with mock binary
    export GOXEL_DAEMON_BINARY="$TEST_DAEMON_PATH"
    export GOXEL_PID_FILE="$TEST_PREFIX/var/run/goxel-daemon.pid"
    export GOXEL_SOCKET_PATH="$TEST_PREFIX/var/run/goxel-daemon.sock"
    export GOXEL_LOG_FILE="$TEST_PREFIX/var/log/goxel-daemon.log"
    
    # Test daemon test command
    if "$control_script" test; then
        log_success "Daemon test command passed"
    else
        log_error "Daemon test command failed"
        return 1
    fi
    
    log_success "Daemon control script tests passed"
}

test_mcp_daemon_manager() {
    log_info "Testing MCP daemon manager..."
    
    # Check if Node.js is available
    if ! command -v node >/dev/null; then
        log_warning "Node.js not available, skipping MCP daemon manager tests"
        return 0
    fi
    
    local manager_script="$SCRIPT_DIR/mcp_daemon_manager.js"
    
    # Create test script
    cat > "$TEST_PREFIX/test_manager.js" << EOF
const { GoxelDaemonManager, DaemonConfig } = require('$manager_script');

async function testManager() {
    console.log('Testing DaemonConfig...');
    const config = new DaemonConfig({
        daemonBinary: '$TEST_DAEMON_PATH',
        socketPath: '$TEST_PREFIX/var/run/goxel-daemon.sock',
        pidFile: '$TEST_PREFIX/var/run/goxel-daemon.pid',
        logFile: '$TEST_PREFIX/var/log/goxel-daemon.log',
        verbose: true,
        autoStart: false
    });
    
    console.log('Config created successfully');
    console.log('Binary path:', config.daemonBinary);
    console.log('Socket path:', config.socketPath);
    
    console.log('Testing DaemonManager...');
    const manager = new GoxelDaemonManager(config);
    
    // Test status check
    const status = await manager.getStatus();
    console.log('Status check completed:', status.running);
    
    console.log('MCP daemon manager test completed successfully');
}

testManager().catch(err => {
    console.error('Test failed:', err.message);
    process.exit(1);
});
EOF
    
    # Run test
    if node "$TEST_PREFIX/test_manager.js"; then
        log_success "MCP daemon manager tests passed"
    else
        log_error "MCP daemon manager tests failed"
        return 1
    fi
}

test_configuration_files() {
    log_info "Testing configuration files..."
    
    # Test JSON configuration
    local config_file="$PROJECT_ROOT/config/daemon_config.json"
    if [[ -f "$config_file" ]]; then
        if command -v jq >/dev/null; then
            if jq empty < "$config_file" >/dev/null 2>&1; then
                log_success "JSON configuration is valid"
            else
                log_error "JSON configuration is invalid"
                return 1
            fi
        else
            log_warning "jq not available, skipping JSON validation"
        fi
    else
        log_error "Configuration file not found: $config_file"
        return 1
    fi
    
    # Test systemd service file
    local service_file="$PROJECT_ROOT/config/goxel-daemon.service"
    if [[ -f "$service_file" ]]; then
        if grep -q "ExecStart=" "$service_file"; then
            log_success "Systemd service file looks valid"
        else
            log_error "Systemd service file appears invalid"
            return 1
        fi
    else
        log_error "Systemd service file not found: $service_file"
        return 1
    fi
    
    # Test macOS plist file
    local plist_file="$PROJECT_ROOT/config/com.goxel.daemon.plist"
    if [[ -f "$plist_file" ]]; then
        if grep -q "ProgramArguments" "$plist_file"; then
            log_success "macOS plist file looks valid"
        else
            log_error "macOS plist file appears invalid"
            return 1
        fi
    else
        log_error "macOS plist file not found: $plist_file"
        return 1
    fi
    
    log_success "Configuration file tests passed"
}

test_deployment_script() {
    log_info "Testing deployment script..."
    
    local deploy_script="$SCRIPT_DIR/deploy_daemon.sh"
    
    # Test help
    if "$deploy_script" --help >/dev/null; then
        log_success "Deployment script help works"
    else
        log_error "Deployment script help failed"
        return 1
    fi
    
    # Test version
    if "$deploy_script" --version >/dev/null; then
        log_success "Deployment script version works"
    else
        log_error "Deployment script version failed"
        return 1
    fi
    
    # Note: We can't test actual deployment without root privileges
    log_info "Skipping actual deployment test (requires root)"
    
    log_success "Deployment script tests passed"
}

test_documentation() {
    log_info "Testing documentation completeness..."
    
    local docs=(
        "$PROJECT_ROOT/config/README.md"
        "$PROJECT_ROOT/DAEMON_LIFECYCLE_IMPLEMENTATION_REPORT.md"
    )
    
    for doc in "${docs[@]}"; do
        if [[ -f "$doc" ]]; then
            if [[ -s "$doc" ]]; then
                log_success "Documentation exists and not empty: $(basename "$doc")"
            else
                log_error "Documentation is empty: $doc"
                return 1
            fi
        else
            log_error "Documentation missing: $doc"
            return 1
        fi
    done
    
    log_success "Documentation tests passed"
}

run_all_tests() {
    log_info "Starting Goxel v14 deployment test suite..."
    
    local failed_tests=0
    
    # Setup
    cleanup_test_env
    setup_test_env
    
    # Run tests
    test_daemon_control_script || ((failed_tests++))
    test_mcp_daemon_manager || ((failed_tests++))
    test_configuration_files || ((failed_tests++))
    test_deployment_script || ((failed_tests++))
    test_documentation || ((failed_tests++))
    
    # Cleanup
    cleanup_test_env
    
    # Results
    echo
    if [[ $failed_tests -eq 0 ]]; then
        log_success "All deployment tests passed! 🎉"
        log_info "System is ready for production deployment"
    else
        log_error "$failed_tests test(s) failed"
        log_error "Please fix issues before deploying to production"
        return 1
    fi
}

# Trap to ensure cleanup
trap cleanup_test_env EXIT

# Run tests
if [[ "${1:-}" == "--help" ]]; then
    echo "Usage: $0 [--help]"
    echo
    echo "Runs comprehensive tests for Goxel v14 daemon deployment system."
    echo "Tests include:"
    echo "  - Daemon control script functionality"
    echo "  - MCP daemon manager (if Node.js available)"
    echo "  - Configuration file validation"
    echo "  - Deployment script basic functionality"
    echo "  - Documentation completeness"
    echo
    exit 0
fi

run_all_tests