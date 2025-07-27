#!/bin/bash
#
# Goxel v14.0 Daemon Architecture - Automated Benchmark Execution Script
# 
# This script provides comprehensive automated execution of all performance
# benchmarks with reporting, logging, and CI integration capabilities.
#
# Usage: ./run_benchmarks.sh [options]
#   --quick          Run quick benchmark suite (5 min)
#   --full           Run full benchmark suite (30 min)
#   --comparison     Run comparison tests only
#   --stress         Run stress tests only
#   --daemon-pid PID Specify daemon PID for memory profiling
#   --output DIR     Specify output directory for results
#   --ci             Run in CI mode (machine-readable output)
#   --help           Show this help message

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PERFORMANCE_DIR="$PROJECT_ROOT/tests/performance"
DEFAULT_OUTPUT_DIR="$PROJECT_ROOT/benchmark_results"
DAEMON_SOCKET="/tmp/goxel_daemon_test.sock"
CLI_BINARY="$PROJECT_ROOT/goxel-headless"

# Test configuration
QUICK_DURATION=5
FULL_DURATION=30
DEFAULT_ITERATIONS=100

# Colors for output (disabled in CI mode)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Global variables
CI_MODE=false
QUICK_MODE=false
FULL_MODE=false
COMPARISON_ONLY=false
STRESS_ONLY=false
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"
DAEMON_PID=""
VERBOSE=true
START_TIME=""

# Logging functions
log_info() {
    if [ "$CI_MODE" = true ]; then
        echo "INFO: $1"
    else
        echo -e "${BLUE}[INFO]${NC} $1"
    fi
}

log_success() {
    if [ "$CI_MODE" = true ]; then
        echo "SUCCESS: $1"
    else
        echo -e "${GREEN}[SUCCESS]${NC} $1"
    fi
}

log_warning() {
    if [ "$CI_MODE" = true ]; then
        echo "WARNING: $1"
    else
        echo -e "${YELLOW}[WARNING]${NC} $1"
    fi
}

log_error() {
    if [ "$CI_MODE" = true ]; then
        echo "ERROR: $1" >&2
    else
        echo -e "${RED}[ERROR]${NC} $1" >&2
    fi
}

log_test() {
    if [ "$CI_MODE" = true ]; then
        echo "TEST: $1"
    else
        echo -e "${PURPLE}[TEST]${NC} $1"
    fi
}

# Help function
show_help() {
    cat << EOF
Goxel v14.0 Daemon Architecture - Automated Benchmark Suite

USAGE:
    $0 [OPTIONS]

OPTIONS:
    --quick                 Run quick benchmark suite (~5 minutes)
    --full                  Run full benchmark suite (~30 minutes)
    --comparison            Run performance comparison tests only
    --stress                Run stress tests only
    --daemon-pid PID        Specify daemon PID for memory profiling
    --output DIR            Output directory for results (default: benchmark_results)
    --ci                    CI mode - machine readable output, no colors
    --verbose               Enable verbose output (default)
    --quiet                 Disable verbose output
    --help                  Show this help message

EXAMPLES:
    $0 --quick              # Quick 5-minute benchmark
    $0 --full --output ./results
    $0 --comparison --ci    # Comparison tests in CI mode
    $0 --stress --daemon-pid 1234

TARGETS:
    - Latency: <2.1ms average request latency
    - Throughput: >1000 voxel operations/second
    - Memory: <50MB daemon memory usage
    - Concurrency: Support 10+ concurrent clients
    - Improvement: >700% vs v13.4 CLI mode

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --quick)
                QUICK_MODE=true
                shift
                ;;
            --full)
                FULL_MODE=true
                shift
                ;;
            --comparison)
                COMPARISON_ONLY=true
                shift
                ;;
            --stress)
                STRESS_ONLY=true
                shift
                ;;
            --daemon-pid)
                DAEMON_PID="$2"
                shift 2
                ;;
            --output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --ci)
                CI_MODE=true
                VERBOSE=false
                shift
                ;;
            --verbose)
                VERBOSE=true
                shift
                ;;
            --quiet)
                VERBOSE=false
                shift
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
    
    # Set default mode if none specified
    if [ "$QUICK_MODE" = false ] && [ "$FULL_MODE" = false ] && 
       [ "$COMPARISON_ONLY" = false ] && [ "$STRESS_ONLY" = false ]; then
        QUICK_MODE=true
    fi
}

# Environment validation
validate_environment() {
    log_info "Validating benchmark environment..."
    
    # Check if performance tests are compiled
    local tests_to_check=("latency_benchmark" "throughput_test" "memory_profiling" "stress_test" "comparison_framework")
    local missing_tests=()
    
    for test in "${tests_to_check[@]}"; do
        if [ ! -x "$PERFORMANCE_DIR/$test" ]; then
            missing_tests+=("$test")
        fi
    done
    
    if [ ${#missing_tests[@]} -gt 0 ]; then
        log_error "Missing compiled performance tests: ${missing_tests[*]}"
        log_info "Please compile tests with: make -C tests/performance"
        return 1
    fi
    
    # Check CLI binary
    if [ ! -x "$CLI_BINARY" ]; then
        log_warning "CLI binary not found at $CLI_BINARY"
        log_info "Some comparison tests may be skipped"
    fi
    
    # Check daemon connection
    if [ -S "$DAEMON_SOCKET" ]; then
        log_success "Daemon socket found at $DAEMON_SOCKET"
    else
        log_warning "Daemon socket not found at $DAEMON_SOCKET"
        log_info "Please ensure daemon is running for full test suite"
    fi
    
    # Create output directory
    mkdir -p "$OUTPUT_DIR"
    if [ ! -w "$OUTPUT_DIR" ]; then
        log_error "Cannot write to output directory: $OUTPUT_DIR"
        return 1
    fi
    
    log_success "Environment validation complete"
    return 0
}

# Build performance tests if needed
build_performance_tests() {
    log_info "Building performance tests..."
    
    if [ ! -f "$PERFORMANCE_DIR/Makefile" ]; then
        # Create a simple Makefile for performance tests
        cat > "$PERFORMANCE_DIR/Makefile" << 'EOF'
CC = gcc
CFLAGS = -Wall -O2 -std=c99 -pthread
LDFLAGS = -pthread -lm

TESTS = latency_benchmark throughput_test memory_profiling stress_test comparison_framework

all: $(TESTS)

latency_benchmark: latency_benchmark.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

throughput_test: throughput_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

memory_profiling: memory_profiling.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

stress_test: stress_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

comparison_framework: comparison_framework.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TESTS)

.PHONY: all clean
EOF
    fi
    
    cd "$PERFORMANCE_DIR"
    if make all; then
        log_success "Performance tests built successfully"
    else
        log_error "Failed to build performance tests"
        return 1
    fi
    cd - > /dev/null
    
    return 0
}

# Run latency benchmark
run_latency_benchmark() {
    log_test "Running latency benchmark..."
    
    local iterations=$DEFAULT_ITERATIONS
    if [ "$QUICK_MODE" = true ]; then
        iterations=50
    fi
    
    local output_file="$OUTPUT_DIR/latency_results.txt"
    
    if "$PERFORMANCE_DIR/latency_benchmark" $iterations > "$output_file" 2>&1; then
        log_success "Latency benchmark completed"
        if [ "$VERBOSE" = true ]; then
            tail -n 10 "$output_file"
        fi
        return 0
    else
        log_error "Latency benchmark failed"
        return 1
    fi
}

# Run throughput test
run_throughput_test() {
    log_test "Running throughput benchmark..."
    
    local duration=$QUICK_DURATION
    if [ "$FULL_MODE" = true ]; then
        duration=$FULL_DURATION
    fi
    
    local output_file="$OUTPUT_DIR/throughput_results.txt"
    
    if "$PERFORMANCE_DIR/throughput_test" $duration > "$output_file" 2>&1; then
        log_success "Throughput benchmark completed"
        if [ "$VERBOSE" = true ]; then
            tail -n 10 "$output_file"
        fi
        return 0
    else
        log_error "Throughput benchmark failed"
        return 1
    fi
}

# Run memory profiling
run_memory_profiling() {
    log_test "Running memory profiling..."
    
    local duration=$QUICK_DURATION
    if [ "$FULL_MODE" = true ]; then
        duration=$FULL_DURATION
    fi
    
    local output_file="$OUTPUT_DIR/memory_results.txt"
    local args="$duration"
    
    if [ -n "$DAEMON_PID" ]; then
        log_info "Using specified daemon PID: $DAEMON_PID"
    fi
    
    if "$PERFORMANCE_DIR/memory_profiling" $args > "$output_file" 2>&1; then
        log_success "Memory profiling completed"
        if [ "$VERBOSE" = true ] && [ -f "$OUTPUT_DIR/daemon_memory_profile.csv" ]; then
            log_info "Memory profile data saved to daemon_memory_profile.csv"
        fi
        return 0
    else
        log_error "Memory profiling failed"
        return 1
    fi
}

# Run stress test
run_stress_test() {
    log_test "Running stress tests..."
    
    local duration=$QUICK_DURATION
    if [ "$FULL_MODE" = true ]; then
        duration=$FULL_DURATION
    fi
    
    local output_file="$OUTPUT_DIR/stress_results.txt"
    
    if "$PERFORMANCE_DIR/stress_test" $duration > "$output_file" 2>&1; then
        log_success "Stress tests completed"
        if [ "$VERBOSE" = true ]; then
            tail -n 15 "$output_file"
        fi
        return 0
    else
        log_error "Stress tests failed"
        return 1
    fi
}

# Run comparison framework
run_comparison_tests() {
    log_test "Running performance comparison tests..."
    
    local output_file="$OUTPUT_DIR/comparison_results.txt"
    
    if "$PERFORMANCE_DIR/comparison_framework" > "$output_file" 2>&1; then
        log_success "Performance comparison completed"
        if [ "$VERBOSE" = true ]; then
            tail -n 15 "$output_file"
        fi
        return 0
    else
        log_error "Performance comparison failed"
        return 1
    fi
}

# Generate comprehensive report
generate_report() {
    log_info "Generating comprehensive benchmark report..."
    
    local report_file="$OUTPUT_DIR/benchmark_report.txt"
    local summary_file="$OUTPUT_DIR/benchmark_summary.json"
    
    {
        echo "Goxel v14.0 Daemon Architecture - Benchmark Report"
        echo "=================================================="
        echo "Generated: $(date)"
        echo "Duration: $(($(date +%s) - START_TIME)) seconds"
        echo "Mode: $(if [ "$QUICK_MODE" = true ]; then echo "Quick"; elif [ "$FULL_MODE" = true ]; then echo "Full"; else echo "Custom"; fi)"
        echo ""
        
        # Include results from each test
        for result_file in "$OUTPUT_DIR"/*_results.txt; do
            if [ -f "$result_file" ]; then
                echo "=== $(basename "$result_file" .txt | tr '_' ' ' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) substr($i,2)}1') ==="
                tail -n 20 "$result_file"
                echo ""
            fi
        done
        
    } > "$report_file"
    
    # Generate JSON summary for CI integration
    {
        echo "{"
        echo "  \"timestamp\": \"$(date -Iseconds)\","
        echo "  \"duration_seconds\": $(($(date +%s) - START_TIME)),"
        echo "  \"mode\": \"$(if [ "$QUICK_MODE" = true ]; then echo "quick"; elif [ "$FULL_MODE" = true ]; then echo "full"; else echo "custom"; fi)\","
        echo "  \"tests_run\": ["
        
        local first=true
        for result_file in "$OUTPUT_DIR"/*_results.txt; do
            if [ -f "$result_file" ]; then
                if [ "$first" = false ]; then echo ","; fi
                echo -n "    \"$(basename "$result_file" .txt)\""
                first=false
            fi
        done
        
        echo ""
        echo "  ],"
        echo "  \"output_directory\": \"$OUTPUT_DIR\""
        echo "}"
    } > "$summary_file"
    
    log_success "Benchmark report generated: $report_file"
    log_info "JSON summary available: $summary_file"
}

# Main execution function
main() {
    START_TIME=$(date +%s)
    
    # Parse arguments
    parse_args "$@"
    
    # Print header
    if [ "$CI_MODE" = false ]; then
        echo -e "${CYAN}"
        echo "╔══════════════════════════════════════════════════════════════╗"
        echo "║           Goxel v14.0 Daemon Performance Benchmark          ║"
        echo "╚══════════════════════════════════════════════════════════════╝"
        echo -e "${NC}"
    else
        echo "BENCHMARK_START: $(date -Iseconds)"
    fi
    
    log_info "Starting benchmark suite in $(pwd)"
    log_info "Output directory: $OUTPUT_DIR"
    
    # Validate environment
    if ! validate_environment; then
        log_error "Environment validation failed"
        exit 1
    fi
    
    # Build tests if needed
    if ! build_performance_tests; then
        log_error "Failed to build performance tests"
        exit 1
    fi
    
    # Track test results
    local tests_run=0
    local tests_passed=0
    
    # Run selected test suites
    if [ "$COMPARISON_ONLY" = true ]; then
        run_comparison_tests && tests_passed=$((tests_passed + 1))
        tests_run=$((tests_run + 1))
    elif [ "$STRESS_ONLY" = true ]; then
        run_stress_test && tests_passed=$((tests_passed + 1))
        tests_run=$((tests_run + 1))
    else
        # Run full test suite
        if run_latency_benchmark; then
            tests_passed=$((tests_passed + 1))
        fi
        tests_run=$((tests_run + 1))
        
        if run_throughput_test; then
            tests_passed=$((tests_passed + 1))
        fi
        tests_run=$((tests_run + 1))
        
        if run_memory_profiling; then
            tests_passed=$((tests_passed + 1))
        fi
        tests_run=$((tests_run + 1))
        
        if run_stress_test; then
            tests_passed=$((tests_passed + 1))
        fi
        tests_run=$((tests_run + 1))
        
        if run_comparison_tests; then
            tests_passed=$((tests_passed + 1))
        fi
        tests_run=$((tests_run + 1))
    fi
    
    # Generate final report
    generate_report
    
    # Print summary
    local duration=$(($(date +%s) - START_TIME))
    
    if [ "$CI_MODE" = true ]; then
        echo "BENCHMARK_COMPLETE: $(date -Iseconds)"
        echo "TESTS_RUN: $tests_run"
        echo "TESTS_PASSED: $tests_passed"
        echo "DURATION_SECONDS: $duration"
        echo "SUCCESS_RATE: $(echo "scale=1; $tests_passed * 100 / $tests_run" | bc -l)%"
    else
        echo ""
        echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${CYAN}║                    BENCHMARK SUMMARY                         ║${NC}"
        echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
        echo ""
        echo -e "  Tests Run:     ${PURPLE}$tests_run${NC}"
        echo -e "  Tests Passed:  ${GREEN}$tests_passed${NC}"
        echo -e "  Duration:      ${BLUE}${duration}s${NC}"
        echo -e "  Success Rate:  $(if [ $tests_passed -eq $tests_run ]; then echo -e "${GREEN}"; else echo -e "${YELLOW}"; fi)$(echo "scale=1; $tests_passed * 100 / $tests_run" | bc -l)%${NC}"
        echo ""
        
        if [ $tests_passed -eq $tests_run ]; then
            echo -e "  ${GREEN}🎉 All benchmarks completed successfully!${NC}"
        else
            echo -e "  ${YELLOW}⚠️  Some benchmarks failed. Check logs for details.${NC}"
        fi
        echo ""
    fi
    
    # Exit with appropriate code
    if [ $tests_passed -eq $tests_run ]; then
        exit 0
    else
        exit 1
    fi
}

# Run main function with all arguments
main "$@"