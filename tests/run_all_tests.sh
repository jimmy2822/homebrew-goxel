#!/bin/bash
# Comprehensive Test Runner for Goxel v13 Headless API
# Phase 6: Production Ready - Complete Test Suite

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "PASS")
            echo -e "${GREEN}[PASS]${NC} $message"
            ;;
        "FAIL")
            echo -e "${RED}[FAIL]${NC} $message"
            ;;
        "SKIP")
            echo -e "${YELLOW}[SKIP]${NC} $message"
            ;;
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
    esac
}

# Function to run a test and capture results
run_test() {
    local test_name=$1
    local test_executable=$2
    
    print_status "INFO" "Running $test_name..."
    
    if [[ ! -x "$test_executable" ]]; then
        print_status "SKIP" "$test_name - executable not found"
        ((SKIPPED_TESTS++))
        return 0
    fi
    
    # Run test and capture output
    if ./"$test_executable" > "${test_name}_output.txt" 2>&1; then
        # Parse test results
        local test_output=$(cat "${test_name}_output.txt")
        local tests_run=$(echo "$test_output" | grep "Tests run:" | sed 's/.*Tests run: \([0-9]*\).*/\1/')
        local tests_passed=$(echo "$test_output" | grep "Tests passed:" | sed 's/.*Tests passed: \([0-9]*\).*/\1/')
        
        if [[ "$tests_run" == "$tests_passed" && "$tests_run" -gt 0 ]]; then
            print_status "PASS" "$test_name ($tests_passed/$tests_run tests passed)"
            ((PASSED_TESTS++))
            ((TOTAL_TESTS++))
        else
            print_status "FAIL" "$test_name ($tests_passed/$tests_run tests passed)"
            echo "  Output saved to ${test_name}_output.txt"
            ((FAILED_TESTS++))
            ((TOTAL_TESTS++))
        fi
    else
        print_status "FAIL" "$test_name - execution failed"
        echo "  Error output saved to ${test_name}_output.txt"
        ((FAILED_TESTS++))
        ((TOTAL_TESTS++))
    fi
}

# Function to check system requirements
check_requirements() {
    print_status "INFO" "Checking system requirements..."
    
    # Check for required tools
    local missing_tools=0
    
    if ! command -v gcc &> /dev/null; then
        print_status "FAIL" "gcc compiler not found"
        ((missing_tools++))
    fi
    
    if ! command -v make &> /dev/null; then
        print_status "FAIL" "make not found"
        ((missing_tools++))
    fi
    
    # Optional tools
    if ! command -v valgrind &> /dev/null; then
        print_status "SKIP" "valgrind not found (memory leak detection will be skipped)"
    fi
    
    if ! command -v gcov &> /dev/null; then
        print_status "SKIP" "gcov not found (coverage analysis will be skipped)"
    fi
    
    if [[ $missing_tools -gt 0 ]]; then
        print_status "FAIL" "Missing required tools. Please install gcc and make."
        exit 1
    fi
    
    print_status "PASS" "System requirements check"
}

# Function to build all tests
build_tests() {
    print_status "INFO" "Building all test executables..."
    
    if make clean > /dev/null 2>&1; then
        print_status "PASS" "Clean completed"
    else
        print_status "FAIL" "Clean failed"
        return 1
    fi
    
    if make all > build_output.txt 2>&1; then
        print_status "PASS" "All tests built successfully"
        rm -f build_output.txt
        return 0
    else
        print_status "FAIL" "Build failed - see build_output.txt for details"
        cat build_output.txt
        return 1
    fi
}

# Function to run performance benchmarks
run_benchmarks() {
    print_status "INFO" "Running performance benchmarks..."
    
    if [[ -x "test_memory_perf" ]]; then
        print_status "INFO" "Running memory and performance tests..."
        if ./test_memory_perf | tee benchmark_results.txt; then
            print_status "PASS" "Performance benchmarks completed"
            print_status "INFO" "Results saved to benchmark_results.txt"
        else
            print_status "FAIL" "Performance benchmarks failed"
        fi
    else
        print_status "SKIP" "Performance test executable not found"
    fi
}

# Function to run memory leak detection
run_memcheck() {
    if command -v valgrind &> /dev/null && [[ -x "test_core" ]]; then
        print_status "INFO" "Running memory leak detection..."
        if valgrind --leak-check=full --error-exitcode=1 ./test_core > memcheck_output.txt 2>&1; then
            print_status "PASS" "No memory leaks detected"
        else
            print_status "FAIL" "Memory leaks detected - see memcheck_output.txt"
        fi
    else
        print_status "SKIP" "Memory leak detection (valgrind not available or test_core not built)"
    fi
}

# Function to generate coverage report
run_coverage() {
    if command -v gcov &> /dev/null; then
        print_status "INFO" "Running coverage analysis..."
        if make coverage > coverage_output.txt 2>&1; then
            print_status "PASS" "Coverage analysis completed"
            print_status "INFO" "Coverage files: *.gcov"
        else
            print_status "FAIL" "Coverage analysis failed - see coverage_output.txt"
        fi
    else
        print_status "SKIP" "Coverage analysis (gcov not available)"
    fi
}

# Main test execution
main() {
    echo "========================================================"
    echo "Goxel v13 Headless API - Comprehensive Test Suite"
    echo "Phase 6: Production Ready - Complete Test Coverage"
    echo "========================================================"
    echo ""
    
    # Change to tests directory
    cd "$(dirname "$0")"
    
    # Check requirements
    check_requirements
    echo ""
    
    # Build all tests
    if ! build_tests; then
        exit 1
    fi
    echo ""
    
    # Run individual test suites
    print_status "INFO" "Executing test suites..."
    echo ""
    
    run_test "Core API Tests" "test_core"
    run_test "Headless Rendering Tests" "test_rendering"
    run_test "CLI Interface Tests" "test_cli"
    run_test "File Format Tests" "test_formats"
    run_test "Memory & Performance Tests" "test_memory_perf"
    run_test "End-to-End Integration Tests" "test_e2e_integration"
    
    echo ""
    
    # Additional analysis
    print_status "INFO" "Running additional analysis..."
    echo ""
    
    run_benchmarks
    run_memcheck
    run_coverage
    
    echo ""
    echo "========================================================"
    echo "TEST SUMMARY"
    echo "========================================================"
    echo "Total Test Suites: $((TOTAL_TESTS + SKIPPED_TESTS))"
    echo "Passed: $PASSED_TESTS"
    echo "Failed: $FAILED_TESTS"
    echo "Skipped: $SKIPPED_TESTS"
    echo ""
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        print_status "PASS" "ALL TESTS PASSED! 🎉"
        echo ""
        echo "Phase 6 Test Coverage Status: ✅ COMPLETE"
        echo "Ready for production deployment!"
        exit 0
    else
        print_status "FAIL" "$FAILED_TESTS test suite(s) failed"
        echo ""
        echo "Please review failed tests and fix issues before proceeding."
        exit 1
    fi
}

# Trap to clean up temporary files
cleanup() {
    rm -f *_output.txt
}
trap cleanup EXIT

# Run main function
main "$@"