#!/bin/bash

# Goxel v14.0 Comprehensive Integration Test Suite Runner
# 
# This automated test runner executes the complete v14.0 integration test suite:
# - End-to-End workflow testing
# - Performance comparison (v13.4 vs v14.0)  
# - Stress testing with concurrent clients
# - Memory leak and resource usage validation
# - CI/CD integration with proper exit codes
# - Comprehensive reporting in multiple formats
#
# Usage:
#   ./run_v14_test_suite.sh [OPTIONS]
# 
# Options:
#   --quick          Run quick test suite (faster, fewer samples)
#   --full           Run full test suite (comprehensive, more samples)
#   --e2e-only       Run only end-to-end tests
#   --perf-only      Run only performance tests  
#   --stress-only    Run only stress tests
#   --memory-only    Run only memory tests
#   --valgrind       Enable valgrind for memory testing
#   --ci             CI mode (machine-readable output)
#   --output DIR     Output directory for reports
#   --help           Show this help

set -e  # Exit on any error

# ============================================================================
# CONFIGURATION
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GOXEL_ROOT="$(dirname "$SCRIPT_DIR")"
TEST_OUTPUT_DIR="${SCRIPT_DIR}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Test configuration
QUICK_MODE=false
FULL_MODE=false
CI_MODE=false
VALGRIND_MODE=false
ENABLE_E2E=true
ENABLE_PERF=true  
ENABLE_STRESS=true
ENABLE_MEMORY=true

# Performance test parameters
QUICK_SAMPLES=50
FULL_SAMPLES=500
STRESS_CLIENTS=10
STRESS_DURATION=30
MEMORY_DURATION=60

# Colors for output (disabled in CI mode)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

log_info() {
    if [[ "$CI_MODE" == "true" ]]; then
        echo "INFO: $1"
    else
        echo -e "${CYAN}[INFO]${NC} $1"
    fi
}

log_success() {
    if [[ "$CI_MODE" == "true" ]]; then
        echo "SUCCESS: $1"
    else
        echo -e "${GREEN}[SUCCESS]${NC} $1"
    fi
}

log_warning() {
    if [[ "$CI_MODE" == "true" ]]; then
        echo "WARNING: $1"
    else
        echo -e "${YELLOW}[WARNING]${NC} $1"
    fi
}

log_error() {
    if [[ "$CI_MODE" == "true" ]]; then
        echo "ERROR: $1"
    else
        echo -e "${RED}[ERROR]${NC} $1"
    fi
}

log_section() {
    if [[ "$CI_MODE" == "true" ]]; then
        echo "SECTION: $1"
        echo "========================================"
    else
        echo -e "\n${BLUE}=== $1 ===${NC}"
    fi
}

show_progress() {
    local current=$1
    local total=$2
    local test_name=$3
    
    if [[ "$CI_MODE" == "true" ]]; then
        echo "PROGRESS: $test_name $current/$total"
    else
        local percent=$((current * 100 / total))
        printf "\r${CYAN}[PROGRESS]${NC} $test_name: $current/$total ($percent%%)    "
        if [[ $current -eq $total ]]; then
            echo ""
        fi
    fi
}

cleanup_test_files() {
    log_info "Cleaning up test artifacts..."
    rm -f /tmp/goxel_*.sock /tmp/goxel_*.pid /tmp/goxel_*.log
    rm -f /tmp/test_*.gox /tmp/test_*.obj /tmp/test_*.ply
    rm -f /tmp/stress_test_*.gox /tmp/goxel_valgrind.log
}

# ============================================================================
# DEPENDENCY CHECKING
# ============================================================================

check_dependencies() {
    log_section "Dependency Check"
    
    local missing_deps=()
    
    # Check for required executables
    if [[ ! -f "$GOXEL_ROOT/goxel-headless" ]]; then
        missing_deps+=("goxel-headless executable not found - run 'scons headless=1 cli_tools=1'")
    fi
    
    # Check for Python (performance tests)
    if ! command -v python3 &> /dev/null; then
        missing_deps+=("python3 is required for performance testing")
    fi
    
    # Check for required Python modules
    if ! python3 -c "import psutil" &> /dev/null; then
        missing_deps+=("Python psutil module is required - run 'pip3 install psutil'")
    fi
    
    # Check for valgrind if requested
    if [[ "$VALGRIND_MODE" == "true" ]]; then
        if ! command -v valgrind &> /dev/null; then
            missing_deps+=("valgrind is required for memory leak testing")
        fi
    fi
    
    # Check for build tools
    if [[ "$ENABLE_E2E" == "true" || "$ENABLE_STRESS" == "true" || "$ENABLE_MEMORY" == "true" ]]; then
        if ! command -v gcc &> /dev/null; then
            missing_deps+=("gcc is required for C test compilation")
        fi
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        log_error "Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            log_error "  - $dep"
        done
        exit 1
    fi
    
    log_success "All dependencies satisfied"
}

# ============================================================================
# TEST COMPILATION
# ============================================================================

compile_tests() {
    log_section "Test Compilation"
    
    local compile_errors=0
    
    # Create output directories
    mkdir -p "$TEST_OUTPUT_DIR"
    mkdir -p "$SCRIPT_DIR/integration"
    mkdir -p "$SCRIPT_DIR/stress"
    
    # Compile E2E tests
    if [[ "$ENABLE_E2E" == "true" ]]; then
        log_info "Compiling E2E integration tests..."
        if ! gcc -std=c99 -Wall -Wextra -pthread \
            -I"$GOXEL_ROOT/src" \
            "$SCRIPT_DIR/integration/test_e2e_workflow.c" \
            -o "$SCRIPT_DIR/test_e2e_workflow" 2>/dev/null; then
            log_error "Failed to compile E2E tests"
            ((compile_errors++))
        fi
    fi
    
    # Compile stress tests
    if [[ "$ENABLE_STRESS" == "true" ]]; then
        log_info "Compiling stress tests..."
        if ! gcc -std=c99 -Wall -Wextra -pthread \
            -I"$GOXEL_ROOT/src" \
            "$SCRIPT_DIR/stress/concurrent_clients.c" \
            -o "$SCRIPT_DIR/concurrent_clients" 2>/dev/null; then
            log_error "Failed to compile stress tests"
            ((compile_errors++))
        fi
    fi
    
    # Compile memory tests
    if [[ "$ENABLE_MEMORY" == "true" ]]; then
        log_info "Compiling memory tests..."
        if ! gcc -std=c99 -Wall -Wextra -pthread \
            -I"$GOXEL_ROOT/src" \
            "$SCRIPT_DIR/integration/memory_tests.c" \
            -o "$SCRIPT_DIR/memory_tests" 2>/dev/null; then
            log_error "Failed to compile memory tests"
            ((compile_errors++))
        fi
    fi
    
    if [[ $compile_errors -gt 0 ]]; then
        log_error "Test compilation failed ($compile_errors errors)"
        exit 1
    fi
    
    log_success "All tests compiled successfully"
}

# ============================================================================
# TEST EXECUTION FUNCTIONS
# ============================================================================

run_e2e_tests() {
    if [[ "$ENABLE_E2E" != "true" ]]; then
        return 0
    fi
    
    log_section "End-to-End Integration Tests"
    
    cleanup_test_files
    
    local e2e_output="$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log"
    
    log_info "Running comprehensive E2E workflow tests..."
    if "$SCRIPT_DIR/test_e2e_workflow" 2>&1 | tee "$e2e_output"; then
        log_success "E2E tests passed"
        return 0
    else
        log_error "E2E tests failed - see $e2e_output"
        return 1
    fi
}

run_performance_tests() {
    if [[ "$ENABLE_PERF" != "true" ]]; then
        return 0
    fi
    
    log_section "Performance Comparison Tests"
    
    cleanup_test_files
    
    local samples
    if [[ "$QUICK_MODE" == "true" ]]; then
        samples=$QUICK_SAMPLES
    elif [[ "$FULL_MODE" == "true" ]]; then
        samples=$FULL_SAMPLES
    else
        samples=$QUICK_SAMPLES
    fi
    
    local perf_output="$TEST_OUTPUT_DIR/performance_results_$TIMESTAMP.json"
    local perf_log="$TEST_OUTPUT_DIR/performance_log_$TIMESTAMP.log"
    
    log_info "Running performance comparison tests ($samples samples)..."
    
    cd "$SCRIPT_DIR/performance"
    if python3 benchmark_v14.py \
        --samples "$samples" \
        --concurrent-clients "$STRESS_CLIENTS" \
        --operations-per-client 50 \
        --output "$perf_output" \
        --cli-path "$GOXEL_ROOT/goxel-headless" 2>&1 | tee "$perf_log"; then
        log_success "Performance tests passed"
        cd "$SCRIPT_DIR"
        return 0
    else
        log_error "Performance tests failed - see $perf_log"
        cd "$SCRIPT_DIR"
        return 1
    fi
}

run_stress_tests() {
    if [[ "$ENABLE_STRESS" != "true" ]]; then
        return 0
    fi
    
    log_section "Stress Tests"
    
    cleanup_test_files
    
    local stress_output="$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log"
    
    log_info "Running concurrent client stress tests ($STRESS_CLIENTS clients, ${STRESS_DURATION}s)..."
    if "$SCRIPT_DIR/concurrent_clients" "$STRESS_CLIENTS" "$STRESS_DURATION" "medium" 2>&1 | tee "$stress_output"; then
        log_success "Stress tests passed"
        return 0
    else
        log_error "Stress tests failed - see $stress_output"
        return 1
    fi
}

run_memory_tests() {
    if [[ "$ENABLE_MEMORY" != "true" ]]; then
        return 0
    fi
    
    log_section "Memory Leak Tests"
    
    cleanup_test_files
    
    local memory_output="$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log"
    local memory_args=(
        "--duration" "$MEMORY_DURATION"
    )
    
    if [[ "$VALGRIND_MODE" == "true" ]]; then
        memory_args+=("--valgrind")
        log_info "Running memory tests with valgrind (${MEMORY_DURATION}s)..."
    else
        log_info "Running memory tests (${MEMORY_DURATION}s)..."
    fi
    
    if [[ "$FULL_MODE" == "true" ]]; then
        memory_args+=("--large-datasets")
    fi
    
    if "$SCRIPT_DIR/memory_tests" "${memory_args[@]}" 2>&1 | tee "$memory_output"; then
        log_success "Memory tests passed"
        return 0
    else
        log_error "Memory tests failed - see $memory_output"
        return 1
    fi
}

# ============================================================================
# REPORT GENERATION
# ============================================================================

generate_summary_report() {
    log_section "Test Summary Report"
    
    local summary_file="$TEST_OUTPUT_DIR/test_summary_$TIMESTAMP.json"
    local html_report="$TEST_OUTPUT_DIR/test_report_$TIMESTAMP.html"
    
    # Count results
    local total_tests=0
    local passed_tests=0
    local failed_tests=0
    
    if [[ "$ENABLE_E2E" == "true" ]]; then
        ((total_tests++))
        if [[ -f "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log" ]] && grep -q "ALL INTEGRATION TESTS PASSED" "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log"; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
    fi
    
    if [[ "$ENABLE_PERF" == "true" ]]; then
        ((total_tests++))
        if [[ -f "$TEST_OUTPUT_DIR/performance_results_$TIMESTAMP.json" ]]; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
    fi
    
    if [[ "$ENABLE_STRESS" == "true" ]]; then
        ((total_tests++))
        if [[ -f "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log"; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
    fi
    
    if [[ "$ENABLE_MEMORY" == "true" ]]; then
        ((total_tests++))
        if [[ -f "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log"; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
    fi
    
    # Generate JSON summary
    cat > "$summary_file" << EOF
{
    "test_run": {
        "timestamp": "$TIMESTAMP",
        "date": "$(date -u +"%Y-%m-%d %H:%M:%S UTC")",
        "mode": "$(if [[ "$QUICK_MODE" == "true" ]]; then echo "quick"; elif [[ "$FULL_MODE" == "true" ]]; then echo "full"; else echo "standard"; fi)",
        "ci_mode": $CI_MODE,
        "valgrind_enabled": $VALGRIND_MODE
    },
    "results": {
        "total_test_suites": $total_tests,
        "passed_test_suites": $passed_tests,
        "failed_test_suites": $failed_tests,
        "success_rate": $(if [[ $total_tests -gt 0 ]]; then echo "scale=1; $passed_tests * 100 / $total_tests" | bc; else echo "0"; fi)
    },
    "test_suites": {
        "e2e_tests": {
            "enabled": $ENABLE_E2E,
            "passed": $(if [[ "$ENABLE_E2E" == "true" ]] && [[ -f "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log" ]] && grep -q "ALL INTEGRATION TESTS PASSED" "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log"; then echo "true"; else echo "false"; fi)
        },
        "performance_tests": {
            "enabled": $ENABLE_PERF,
            "passed": $(if [[ "$ENABLE_PERF" == "true" ]] && [[ -f "$TEST_OUTPUT_DIR/performance_results_$TIMESTAMP.json" ]]; then echo "true"; else echo "false"; fi)
        },
        "stress_tests": {
            "enabled": $ENABLE_STRESS,
            "passed": $(if [[ "$ENABLE_STRESS" == "true" ]] && [[ -f "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log"; then echo "true"; else echo "false"; fi)
        },
        "memory_tests": {
            "enabled": $ENABLE_MEMORY,
            "passed": $(if [[ "$ENABLE_MEMORY" == "true" ]] && [[ -f "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log"; then echo "true"; else echo "false"; fi)
        }
    },
    "artifacts": {
        "output_directory": "$TEST_OUTPUT_DIR",
        "log_files": [
$(ls "$TEST_OUTPUT_DIR"/*_$TIMESTAMP.log 2>/dev/null | sed 's/.*/"&"/' | tr '\n' ',' | sed 's/,$//')
        ],
        "data_files": [
$(ls "$TEST_OUTPUT_DIR"/*_$TIMESTAMP.json 2>/dev/null | sed 's/.*/"&"/' | tr '\n' ',' | sed 's/,$//')
        ]
    }
}
EOF
    
    # Print summary
    log_info "Test execution completed!"
    echo ""
    echo "📊 GOXEL v14.0 INTEGRATION TEST SUMMARY"
    echo "========================================"
    echo "Timestamp: $(date)"
    echo "Test Mode: $(if [[ "$QUICK_MODE" == "true" ]]; then echo "Quick"; elif [[ "$FULL_MODE" == "true" ]]; then echo "Full"; else echo "Standard"; fi)"
    echo ""
    echo "Results:"
    echo "  Total Test Suites: $total_tests"
    echo "  Passed: $passed_tests"
    echo "  Failed: $failed_tests"
    echo "  Success Rate: $(if [[ $total_tests -gt 0 ]]; then echo "scale=1; $passed_tests * 100 / $total_tests" | bc; else echo "0"; fi)%"
    echo ""
    echo "Test Suite Results:"
    if [[ "$ENABLE_E2E" == "true" ]]; then
        if [[ -f "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log" ]] && grep -q "ALL INTEGRATION TESTS PASSED" "$TEST_OUTPUT_DIR/e2e_results_$TIMESTAMP.log"; then
            echo "  E2E Tests: ✅ PASSED"
        else
            echo "  E2E Tests: ❌ FAILED"
        fi
    fi
    
    if [[ "$ENABLE_PERF" == "true" ]]; then
        if [[ -f "$TEST_OUTPUT_DIR/performance_results_$TIMESTAMP.json" ]]; then
            echo "  Performance Tests: ✅ PASSED"
        else
            echo "  Performance Tests: ❌ FAILED"
        fi
    fi
    
    if [[ "$ENABLE_STRESS" == "true" ]]; then
        if [[ -f "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/stress_results_$TIMESTAMP.log"; then
            echo "  Stress Tests: ✅ PASSED"
        else
            echo "  Stress Tests: ❌ FAILED"
        fi
    fi
    
    if [[ "$ENABLE_MEMORY" == "true" ]]; then
        if [[ -f "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log" ]] && grep -q "PASSED" "$TEST_OUTPUT_DIR/memory_results_$TIMESTAMP.log"; then
            echo "  Memory Tests: ✅ PASSED"
        else
            echo "  Memory Tests: ❌ FAILED"
        fi
    fi
    
    echo ""
    echo "📁 Output Files:"
    echo "  Summary: $summary_file"
    echo "  Results Directory: $TEST_OUTPUT_DIR"
    
    if [[ "$VALGRIND_MODE" == "true" ]] && [[ -f "/tmp/goxel_valgrind.log" ]]; then
        echo "  Valgrind Log: /tmp/goxel_valgrind.log"
    fi
    
    echo ""
    
    # Final assessment
    if [[ $failed_tests -eq 0 ]]; then
        log_success "🎉 ALL TESTS PASSED - v14.0 IS READY FOR DEPLOYMENT!"
        return 0
    else
        log_error "❌ SOME TESTS FAILED - REVIEW LOGS BEFORE DEPLOYMENT"
        return 1
    fi
}

# ============================================================================
# ARGUMENT PARSING
# ============================================================================

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --quick)
                QUICK_MODE=true
                FULL_MODE=false
                STRESS_DURATION=15
                MEMORY_DURATION=30
                shift
                ;;
            --full)
                FULL_MODE=true
                QUICK_MODE=false
                STRESS_DURATION=60
                MEMORY_DURATION=120
                shift
                ;;
            --e2e-only)
                ENABLE_E2E=true
                ENABLE_PERF=false
                ENABLE_STRESS=false
                ENABLE_MEMORY=false
                shift
                ;;
            --perf-only)
                ENABLE_E2E=false
                ENABLE_PERF=true
                ENABLE_STRESS=false
                ENABLE_MEMORY=false
                shift
                ;;
            --stress-only)
                ENABLE_E2E=false
                ENABLE_PERF=false
                ENABLE_STRESS=true
                ENABLE_MEMORY=false
                shift
                ;;
            --memory-only)
                ENABLE_E2E=false
                ENABLE_PERF=false
                ENABLE_STRESS=false
                ENABLE_MEMORY=true
                shift
                ;;
            --valgrind)
                VALGRIND_MODE=true
                shift
                ;;
            --ci)
                CI_MODE=true
                # Disable colors in CI mode
                RED=''
                GREEN=''
                YELLOW=''
                BLUE=''
                MAGENTA=''
                CYAN=''
                NC=''
                shift
                ;;
            --output)
                if [[ -n $2 ]]; then
                    TEST_OUTPUT_DIR="$2"
                    shift 2
                else
                    log_error "Option --output requires a directory argument"
                    exit 1
                fi
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

show_help() {
    echo "Goxel v14.0 Comprehensive Integration Test Suite"
    echo "================================================"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --quick          Run quick test suite (faster, fewer samples)"
    echo "  --full           Run full test suite (comprehensive, more samples)"
    echo "  --e2e-only       Run only end-to-end tests"
    echo "  --perf-only      Run only performance tests"
    echo "  --stress-only    Run only stress tests"
    echo "  --memory-only    Run only memory tests"
    echo "  --valgrind       Enable valgrind for memory testing"
    echo "  --ci             CI mode (machine-readable output)"
    echo "  --output DIR     Output directory for reports"
    echo "  --help           Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                          # Run standard test suite"
    echo "  $0 --quick                  # Run quick tests"
    echo "  $0 --full --valgrind        # Run comprehensive tests with valgrind"
    echo "  $0 --perf-only --output ./results  # Performance tests only"
    echo "  $0 --ci                     # CI-friendly mode"
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

main() {
    # Parse command line arguments
    parse_arguments "$@"
    
    # Show banner
    if [[ "$CI_MODE" != "true" ]]; then
        echo -e "${BLUE}"
        echo "🧪 GOXEL v14.0 COMPREHENSIVE INTEGRATION TEST SUITE"
        echo "===================================================="
        echo -e "${NC}"
        echo "Validating daemon architecture performance and reliability"
        echo ""
    fi
    
    # Check dependencies and compile tests
    check_dependencies
    compile_tests
    
    # Track overall results
    local overall_result=0
    
    # Execute test suites
    run_e2e_tests || overall_result=1
    run_performance_tests || overall_result=1
    run_stress_tests || overall_result=1
    run_memory_tests || overall_result=1
    
    # Generate final report
    generate_summary_report || overall_result=1
    
    # Final cleanup
    cleanup_test_files
    
    exit $overall_result
}

# Run main function with all arguments
main "$@"