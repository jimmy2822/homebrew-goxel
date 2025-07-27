#!/bin/bash
# Goxel v14.6 Unified Test Runner
# Author: James O'Brien (Agent-4)
# Date: January 2025

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
REPORTS_DIR="$SCRIPT_DIR/reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Test configuration
QUICK_MODE=false
ALL_TESTS=false
RUN_UNIT=false
RUN_INTEGRATION=false
RUN_PERFORMANCE=false
RUN_STRESS=false
RUN_SECURITY=false
MEMCHECK=false
VERBOSE=false
CI_MODE=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --all              Run all test categories"
    echo "  --unit             Run unit tests only"
    echo "  --integration      Run integration tests only"
    echo "  --performance      Run performance tests only"
    echo "  --stress           Run stress tests only"
    echo "  --security         Run security tests only"
    echo "  --quick            Run quick test suite (subset)"
    echo "  --memcheck         Run with valgrind memory check"
    echo "  --verbose          Enable verbose output"
    echo "  --ci               CI mode (no colors, machine-readable)"
    echo "  --help             Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --all           # Run all tests"
    echo "  $0 --unit --integration  # Run unit and integration tests"
    echo "  $0 --performance --verbose  # Run performance tests with details"
    echo "  $0 --quick         # Run quick CI test suite"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            ALL_TESTS=true
            shift
            ;;
        --unit)
            RUN_UNIT=true
            shift
            ;;
        --integration)
            RUN_INTEGRATION=true
            shift
            ;;
        --performance)
            RUN_PERFORMANCE=true
            shift
            ;;
        --stress)
            RUN_STRESS=true
            shift
            ;;
        --security)
            RUN_SECURITY=true
            shift
            ;;
        --quick)
            QUICK_MODE=true
            shift
            ;;
        --memcheck)
            MEMCHECK=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --ci)
            CI_MODE=true
            RED=''
            GREEN=''
            YELLOW=''
            BLUE=''
            MAGENTA=''
            CYAN=''
            NC=''
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Setup directories
mkdir -p "$RESULTS_DIR" "$REPORTS_DIR"

# Log functions
log_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Test execution function
run_test() {
    local test_name=$1
    local test_path=$2
    local category=$3
    local log_file="$RESULTS_DIR/${category}_${test_name}_${TIMESTAMP}.log"
    
    if [[ ! -f "$test_path" ]]; then
        log_warning "Test not found: $test_path"
        return 1
    fi
    
    log_info "Running $category test: $test_name"
    
    local start_time=$(date +%s)
    
    if [[ "$MEMCHECK" == "true" ]]; then
        valgrind --leak-check=full --show-leak-kinds=all \
                 --log-file="$RESULTS_DIR/valgrind_${test_name}_${TIMESTAMP}.log" \
                 "$test_path" > "$log_file" 2>&1
        local exit_code=$?
    else
        if [[ "$VERBOSE" == "true" ]]; then
            "$test_path" 2>&1 | tee "$log_file"
            local exit_code=${PIPESTATUS[0]}
        else
            "$test_path" > "$log_file" 2>&1
            local exit_code=$?
        fi
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    if [[ $exit_code -eq 0 ]]; then
        log_success "$test_name passed (${duration}s)"
        return 0
    else
        log_error "$test_name failed (${duration}s) - see $log_file"
        return 1
    fi
}

# Run test category
run_category() {
    local category=$1
    local tests=()
    
    case $category in
        unit)
            tests=(test_daemon_core test_json_rpc_parser test_connection_pool)
            if [[ "$QUICK_MODE" == "true" ]]; then
                tests=(test_daemon_core)
            fi
            ;;
        integration)
            tests=(test_daemon_lifecycle test_e2e_workflow)
            if [[ "$QUICK_MODE" == "true" ]]; then
                tests=(test_daemon_lifecycle)
            fi
            ;;
        performance)
            tests=(test_latency test_throughput)
            ;;
        stress)
            tests=(test_concurrent_clients)
            ;;
        security)
            tests=(test_input_validation)
            ;;
    esac
    
    local passed=0
    local failed=0
    
    echo ""
    echo -e "${BLUE}=== Running $category tests ===${NC}"
    echo ""
    
    for test in "${tests[@]}"; do
        if run_test "$test" "$SCRIPT_DIR/$test" "$category"; then
            ((passed++))
        else
            ((failed++))
        fi
    done
    
    echo ""
    echo "$category tests: $passed passed, $failed failed"
    
    return $failed
}

# Main execution
main() {
    log_info "Goxel v14.6 Test Suite"
    log_info "Timestamp: $(date)"
    
    # Check for test binaries
    if [[ ! -f "$SCRIPT_DIR/test_daemon_core" ]] && [[ "$RUN_UNIT" == "true" || "$ALL_TESTS" == "true" ]]; then
        log_error "Test binaries not found. Run 'make tests' first."
        exit 1
    fi
    
    # Determine what to run
    if [[ "$ALL_TESTS" == "true" ]]; then
        RUN_UNIT=true
        RUN_INTEGRATION=true
        RUN_PERFORMANCE=true
        RUN_STRESS=true
        RUN_SECURITY=true
    elif [[ "$QUICK_MODE" == "true" ]]; then
        RUN_UNIT=true
        RUN_INTEGRATION=true
    fi
    
    # If nothing selected, show usage
    if [[ "$RUN_UNIT" != "true" && "$RUN_INTEGRATION" != "true" && \
          "$RUN_PERFORMANCE" != "true" && "$RUN_STRESS" != "true" && \
          "$RUN_SECURITY" != "true" ]]; then
        log_error "No tests selected"
        usage
        exit 1
    fi
    
    # Track results
    local total_failed=0
    
    # Run selected test categories
    if [[ "$RUN_UNIT" == "true" ]]; then
        run_category "unit" || ((total_failed+=$?))
    fi
    
    if [[ "$RUN_INTEGRATION" == "true" ]]; then
        run_category "integration" || ((total_failed+=$?))
    fi
    
    if [[ "$RUN_PERFORMANCE" == "true" ]]; then
        run_category "performance" || ((total_failed+=$?))
    fi
    
    if [[ "$RUN_STRESS" == "true" ]]; then
        run_category "stress" || ((total_failed+=$?))
    fi
    
    if [[ "$RUN_SECURITY" == "true" ]]; then
        run_category "security" || ((total_failed+=$?))
    fi
    
    # Generate summary
    echo ""
    echo -e "${BLUE}=== Test Summary ===${NC}"
    echo ""
    
    if [[ $total_failed -eq 0 ]]; then
        log_success "All tests passed!"
        
        # Generate success report
        cat > "$RESULTS_DIR/summary_${TIMESTAMP}.json" << EOF
{
    "timestamp": "$(date -u +"%Y-%m-%d %H:%M:%S UTC")",
    "result": "PASS",
    "total_failures": 0,
    "categories_run": {
        "unit": $RUN_UNIT,
        "integration": $RUN_INTEGRATION,
        "performance": $RUN_PERFORMANCE,
        "stress": $RUN_STRESS,
        "security": $RUN_SECURITY
    },
    "quick_mode": $QUICK_MODE,
    "memcheck": $MEMCHECK
}
EOF
    else
        log_error "Tests failed: $total_failed"
        
        # Generate failure report
        cat > "$RESULTS_DIR/summary_${TIMESTAMP}.json" << EOF
{
    "timestamp": "$(date -u +"%Y-%m-%d %H:%M:%S UTC")",
    "result": "FAIL",
    "total_failures": $total_failed,
    "categories_run": {
        "unit": $RUN_UNIT,
        "integration": $RUN_INTEGRATION,
        "performance": $RUN_PERFORMANCE,
        "stress": $RUN_STRESS,
        "security": $RUN_SECURITY
    },
    "quick_mode": $QUICK_MODE,
    "memcheck": $MEMCHECK
}
EOF
    fi
    
    echo ""
    log_info "Results saved to: $RESULTS_DIR"
    
    exit $total_failed
}

# Run main
main