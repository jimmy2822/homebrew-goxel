#!/bin/bash
# Streamlined Test Runner for Goxel v13.1 Headless API
# Essential tests only - production-grade core functionality

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
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
    esac
}

# Function to run a test and capture results
run_test() {
    local test_name=$1
    local test_command=$2
    
    print_status "INFO" "Running $test_name..."
    
    if eval "$test_command"; then
        print_status "PASS" "$test_name"
        ((PASSED_TESTS++))
    else
        print_status "FAIL" "$test_name"
        ((FAILED_TESTS++))
    fi
    ((TOTAL_TESTS++))
}

# Main test execution
main() {
    echo "========================================================"
    echo "Goxel v13.1 Headless API - Essential Test Suite"
    echo "Production-Grade Core Functionality Testing"
    echo "========================================================"
    echo ""
    
    # Change to tests directory
    cd "$(dirname "$0")"
    
    # Check if CLI executable exists
    if [ ! -f "../goxel-headless" ]; then
        print_status "FAIL" "Goxel CLI executable not found"
        echo "Please build it first with: scons headless=1 cli_tools=1"
        exit 1
    fi
    
    print_status "INFO" "Found Goxel CLI executable"
    echo ""
    
    # Build test scripts
    print_status "INFO" "Building test scripts..."
    if make clean > /dev/null 2>&1 && make all > /dev/null 2>&1; then
        print_status "PASS" "Test scripts built"
    else
        print_status "FAIL" "Failed to build test scripts"
        exit 1
    fi
    echo ""
    
    # Run tests
    print_status "INFO" "Executing essential tests..."
    echo ""
    
    run_test "CLI Functionality Tests" "./test_cli_functionality"
    run_test "Performance Tests" "./test_performance"
    
    echo ""
    echo "========================================================"
    echo "TEST SUMMARY"
    echo "========================================================"
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed: $PASSED_TESTS"
    echo "Failed: $FAILED_TESTS"
    echo ""
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        print_status "PASS" "ALL ESSENTIAL TESTS PASSED! 🎉"
        echo ""
        echo "v13.1 Essential Test Coverage: ✅ COMPLETE"
        echo "Production-grade functionality validated!"
        exit 0
    else
        print_status "FAIL" "$FAILED_TESTS test(s) failed"
        echo ""
        echo "Please review failed tests and fix issues."
        exit 1
    fi
}

# Run main function
main "$@"