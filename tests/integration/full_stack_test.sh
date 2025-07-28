#!/bin/bash

# Goxel v14.0 Full Stack Integration Test Suite
# This script orchestrates comprehensive end-to-end testing of the daemon architecture

set -e  # Exit on any error

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
DEFAULT_SOCKET="/tmp/goxel_test.sock"
DEFAULT_PID_FILE="/tmp/goxel_test.pid"
DEFAULT_LOG_FILE="/tmp/goxel_test.log"
TEST_DURATION=3600  # 1 hour for long-running tests
DEFAULT_PORT=9876

# Test directories
TEST_DIR="$(dirname "$0")"
PROJECT_ROOT="$(cd "$TEST_DIR/../.." && pwd)"
RESULTS_DIR="$TEST_DIR/results"
LOGS_DIR="$TEST_DIR/logs"

# Binary paths
DAEMON_BIN="$PROJECT_ROOT/goxel-headless"
TEST_BIN="$TEST_DIR/test_e2e_workflow"
SCENARIO_BIN="$TEST_DIR/test_specific_scenarios"

# Test stages
DO_CLEANUP=1
DO_BUILD=1
DO_BASIC=1
DO_STRESS=1
DO_PERFORMANCE=1
DO_CLIENT=1
DO_REPORT=1

# Results tracking
TESTS_PASSED=0
TESTS_FAILED=0
TEST_RESULTS=()

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

print_header() {
    echo -e "\n${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}\n"
}

print_section() {
    echo -e "\n${YELLOW}=== $1 ===${NC}"
}

print_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "Required command '$1' not found"
        return 1
    fi
    return 0
}

cleanup() {
    print_section "Cleanup"
    
    # Kill any running daemon processes
    if [ -f "$DEFAULT_PID_FILE" ]; then
        PID=$(cat "$DEFAULT_PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            print_info "Stopping daemon (PID: $PID)"
            kill -TERM "$PID" 2>/dev/null || true
            sleep 1
            kill -KILL "$PID" 2>/dev/null || true
        fi
    fi
    
    # Clean up test files
    rm -f "$DEFAULT_SOCKET" "$DEFAULT_PID_FILE" "$DEFAULT_LOG_FILE"
    rm -f /tmp/goxel_test_*.{sock,pid,log,gox,obj}
    
    print_success "Cleanup completed"
}

ensure_directories() {
    mkdir -p "$RESULTS_DIR"
    mkdir -p "$LOGS_DIR"
}

# ============================================================================
# TEST EXECUTION FUNCTIONS
# ============================================================================

run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="${3:-0}"
    local timeout="${4:-60}"
    
    print_info "Running: $test_name"
    
    local start_time=$(date +%s)
    local test_log="$LOGS_DIR/${test_name// /_}.log"
    
    # Run test with timeout
    if timeout "$timeout" bash -c "$test_command" > "$test_log" 2>&1; then
        local exit_code=$?
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        if [ $exit_code -eq $expected_result ]; then
            print_success "$test_name (${duration}s)"
            TEST_RESULTS+=("PASS: $test_name (${duration}s)")
            ((TESTS_PASSED++))
            return 0
        else
            print_error "$test_name - Wrong exit code: $exit_code (expected: $expected_result)"
            TEST_RESULTS+=("FAIL: $test_name - Wrong exit code")
            ((TESTS_FAILED++))
            return 1
        fi
    else
        print_error "$test_name - Test failed or timed out"
        TEST_RESULTS+=("FAIL: $test_name - Failed/Timeout")
        ((TESTS_FAILED++))
        return 1
    fi
}

# ============================================================================
# BUILD TESTS
# ============================================================================

build_tests() {
    print_header "Building Test Suite"
    
    # Build daemon if needed
    if [ ! -f "$DAEMON_BIN" ]; then
        print_info "Building daemon..."
        cd "$PROJECT_ROOT"
        if ! scons headless=1 daemon=1 -j8; then
            print_error "Failed to build daemon"
            return 1
        fi
    fi
    
    # Build integration tests
    print_info "Building integration tests..."
    cd "$TEST_DIR"
    if ! make clean all; then
        print_error "Failed to build integration tests"
        return 1
    fi
    
    print_success "Build completed successfully"
    return 0
}

# ============================================================================
# BASIC CONNECTIVITY TESTS
# ============================================================================

test_basic_connectivity() {
    print_header "Basic Connectivity Tests"
    
    # Test 1: Daemon starts and creates socket
    run_test "Daemon Startup" "$SCENARIO_BIN -t startup"
    
    # Test 2: Client can connect
    run_test "Single Client Connection" "$SCENARIO_BIN -t connect"
    
    # Test 3: Multiple clients can connect
    run_test "Multiple Client Connections" "$SCENARIO_BIN -t multi_connect"
    
    # Test 4: Graceful shutdown
    run_test "Graceful Shutdown" "$SCENARIO_BIN -t shutdown"
}

# ============================================================================
# JSON-RPC METHOD TESTS
# ============================================================================

test_json_rpc_methods() {
    print_header "JSON-RPC Method Tests"
    
    # Basic methods
    run_test "Echo Method" "$SCENARIO_BIN -t method_echo"
    run_test "Version Method" "$SCENARIO_BIN -t method_version"
    run_test "Status Method" "$SCENARIO_BIN -t method_status"
    run_test "Ping Method" "$TEST_BIN -t method_ping"
    
    # Voxel operations
    run_test "Create Project" "$SCENARIO_BIN -t method_create_project"
    run_test "Add Voxel" "$SCENARIO_BIN -t method_add_voxel"
    run_test "Get Voxel" "$TEST_BIN -t method_get_voxel"
    run_test "Remove Voxel" "$TEST_BIN -t method_remove_voxel"
    
    # File operations
    run_test "Save Project" "$TEST_BIN -t method_save_project"
    run_test "Load Project" "$TEST_BIN -t method_load_project"
    run_test "Export Model" "$TEST_BIN -t method_export_model"
    
    # Batch operations
    run_test "Batch Requests" "$TEST_BIN -t method_batch"
    
    # Error handling
    run_test "Invalid Method" "$TEST_BIN -t error_invalid_method"
    run_test "Invalid Parameters" "$TEST_BIN -t error_invalid_params"
}

# ============================================================================
# TYPESCRIPT CLIENT TESTS
# ============================================================================

test_typescript_client() {
    print_header "TypeScript Client Integration Tests"
    
    # Check if Node.js is available
    if ! check_command "node"; then
        print_warning "Node.js not found, skipping TypeScript tests"
        return 0
    fi
    
    # Run TypeScript client tests
    cd "$PROJECT_ROOT/src/mcp-client"
    
    # Install dependencies if needed
    if [ ! -d "node_modules" ]; then
        print_info "Installing Node.js dependencies..."
        npm install
    fi
    
    # Run client tests
    run_test "Client Connection Test" "npm test -- daemon_client.test.ts"
    run_test "Connection Pool Test" "npm test -- connection_pool.test.ts"
    run_test "Health Monitor Test" "npm test -- health_monitor.test.ts"
    
    cd "$TEST_DIR"
}

# ============================================================================
# PERFORMANCE TESTS
# ============================================================================

test_performance() {
    print_header "Performance Validation Tests"
    
    # Run performance benchmarks
    cd "$TEST_DIR/../performance"
    
    run_test "Latency Benchmark" "./latency_benchmark" 0 120
    run_test "Throughput Test" "./throughput_test" 0 120
    run_test "Memory Profiling" "./memory_profiling" 0 180
    
    # Python benchmark comparison
    if check_command "python3"; then
        run_test "Performance Comparison" "python3 benchmark_v14.py --mode daemon" 0 300
    fi
    
    cd "$TEST_DIR"
}

# ============================================================================
# STRESS TESTS
# ============================================================================

test_stress() {
    print_header "Stress Testing"
    
    # Concurrent client stress test
    run_test "10 Concurrent Clients" "$SCENARIO_BIN -t stress_10_clients" 0 180
    run_test "50 Concurrent Clients" "$SCENARIO_BIN -t stress_50_clients" 0 300
    
    # High load stress test
    run_test "1000 Operations Sequential" "$TEST_BIN -t stress_1000_ops" 0 300
    run_test "Burst Load Test" "$TEST_BIN -t stress_burst" 0 180
    
    # Memory leak detection
    run_test "Memory Leak Test (5 min)" "$TEST_BIN -t memory_leak -d 300" 0 360
    
    # Long-running stability test (optional)
    if [ "$DO_LONG_TEST" = "1" ]; then
        run_test "Long Running Test (1 hour)" "$TEST_BIN -t stability -d 3600" 0 3700
    fi
}

# ============================================================================
# REPORT GENERATION
# ============================================================================

generate_report() {
    print_header "Generating Test Report"
    
    local report_file="$RESULTS_DIR/integration_test_report_$(date +%Y%m%d_%H%M%S).md"
    local total_tests=$((TESTS_PASSED + TESTS_FAILED))
    local success_rate=0
    if [ $total_tests -gt 0 ]; then
        success_rate=$(awk "BEGIN {printf \"%.1f\", 100.0 * $TESTS_PASSED / $total_tests}")
    fi
    
    cat > "$report_file" << EOF
# Goxel v14.0 Integration Test Report

**Date**: $(date)
**Host**: $(hostname)
**Platform**: $(uname -s) $(uname -r)

## Summary

- **Total Tests**: $total_tests
- **Passed**: $TESTS_PASSED
- **Failed**: $TESTS_FAILED
- **Success Rate**: ${success_rate}%

## Test Results

EOF
    
    # Add individual test results
    for result in "${TEST_RESULTS[@]}"; do
        echo "- $result" >> "$report_file"
    done
    
    # Add performance metrics if available
    if [ -f "$TEST_DIR/../performance/performance_results.json" ]; then
        echo -e "\n## Performance Metrics\n" >> "$report_file"
        echo '```json' >> "$report_file"
        cat "$TEST_DIR/../performance/performance_results.json" >> "$report_file"
        echo '```' >> "$report_file"
    fi
    
    # Add recommendations
    echo -e "\n## Recommendations\n" >> "$report_file"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo "✅ **All tests passed!** The v14 daemon is ready for production deployment." >> "$report_file"
    else
        echo "⚠️ **Some tests failed.** Please review the failures before deployment:" >> "$report_file"
        echo "" >> "$report_file"
        grep "^FAIL:" <<< "${TEST_RESULTS[@]}" >> "$report_file" || true
    fi
    
    print_success "Report generated: $report_file"
    
    # Display summary
    echo -e "\n${BLUE}Test Summary:${NC}"
    echo -e "  Total: $total_tests"
    echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
    else
        echo -e "  Failed: 0"
    fi
    echo -e "  Success Rate: ${success_rate}%"
}

# ============================================================================
# MAIN TEST ORCHESTRATION
# ============================================================================

main() {
    print_header "Goxel v14.0 Full Stack Integration Test Suite"
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --no-cleanup) DO_CLEANUP=0 ;;
            --no-build) DO_BUILD=0 ;;
            --no-basic) DO_BASIC=0 ;;
            --no-stress) DO_STRESS=0 ;;
            --no-performance) DO_PERFORMANCE=0 ;;
            --no-client) DO_CLIENT=0 ;;
            --no-report) DO_REPORT=0 ;;
            --long-test) DO_LONG_TEST=1 ;;
            --help)
                echo "Usage: $0 [options]"
                echo "Options:"
                echo "  --no-cleanup      Skip cleanup phase"
                echo "  --no-build        Skip build phase"
                echo "  --no-basic        Skip basic connectivity tests"
                echo "  --no-stress       Skip stress tests"
                echo "  --no-performance  Skip performance tests"
                echo "  --no-client       Skip TypeScript client tests"
                echo "  --no-report       Skip report generation"
                echo "  --long-test       Include 1-hour stability test"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
        shift
    done
    
    # Ensure directories exist
    ensure_directories
    
    # Initial cleanup
    if [ $DO_CLEANUP -eq 1 ]; then
        cleanup
    fi
    
    # Build tests
    if [ $DO_BUILD -eq 1 ]; then
        if ! build_tests; then
            print_error "Build failed, aborting tests"
            exit 1
        fi
    fi
    
    # Run test suites
    if [ $DO_BASIC -eq 1 ]; then
        test_basic_connectivity
        test_json_rpc_methods
    fi
    
    if [ $DO_CLIENT -eq 1 ]; then
        test_typescript_client
    fi
    
    if [ $DO_PERFORMANCE -eq 1 ]; then
        test_performance
    fi
    
    if [ $DO_STRESS -eq 1 ]; then
        test_stress
    fi
    
    # Generate report
    if [ $DO_REPORT -eq 1 ]; then
        generate_report
    fi
    
    # Final cleanup
    if [ $DO_CLEANUP -eq 1 ]; then
        cleanup
    fi
    
    # Exit with appropriate code
    if [ $TESTS_FAILED -eq 0 ]; then
        print_success "\nAll tests passed!"
        exit 0
    else
        print_error "\n$TESTS_FAILED tests failed"
        exit 1
    fi
}

# Run main function
main "$@"