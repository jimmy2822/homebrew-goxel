# Goxel v14.6 Unified Test Framework Makefile
# Author: James O'Brien (Agent-4)
# Date: January 2025

# Directories
GOXEL_ROOT = ../..
TEST_ROOT = .
FRAMEWORK_DIR = $(TEST_ROOT)/framework
UNIT_DIR = $(TEST_ROOT)/unit
INTEGRATION_DIR = $(TEST_ROOT)/integration
PERFORMANCE_DIR = $(TEST_ROOT)/performance
STRESS_DIR = $(TEST_ROOT)/stress
SECURITY_DIR = $(TEST_ROOT)/security
FIXTURES_DIR = $(TEST_ROOT)/fixtures
MOCKS_DIR = $(TEST_ROOT)/mocks

# Compiler settings
CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -Wextra -O2 -g -pthread
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -pthread
INCLUDES = -I$(FRAMEWORK_DIR) -I$(GOXEL_ROOT)/src -I$(GOXEL_ROOT)/src/core \
           -I$(GOXEL_ROOT)/src/daemon -I$(GOXEL_ROOT)/src/json_rpc \
           -I$(GOXEL_ROOT)/include

# Libraries
LIBS = -lpthread -lm

# Framework files
FRAMEWORK_SRCS = $(FRAMEWORK_DIR)/test_framework.c
FRAMEWORK_OBJS = $(FRAMEWORK_SRCS:.c=.o)

# Test categories
UNIT_TESTS = test_daemon_core test_json_rpc_parser test_connection_pool \
             test_health_monitor test_message_queue
INTEGRATION_TESTS = test_daemon_lifecycle test_client_integration \
                    test_e2e_workflow test_multi_client
PERFORMANCE_TESTS = test_latency test_throughput test_memory test_scalability
STRESS_TESTS = test_concurrent_clients test_long_running test_resource_limits
SECURITY_TESTS = test_input_validation test_auth test_dos_protection

# All test executables
ALL_TESTS = $(UNIT_TESTS) $(INTEGRATION_TESTS) $(PERFORMANCE_TESTS) \
            $(STRESS_TESTS) $(SECURITY_TESTS)

# Test results directory
RESULTS_DIR = $(TEST_ROOT)/results
REPORTS_DIR = $(TEST_ROOT)/reports

# Default target
all: framework tests

# Build test framework
framework: $(FRAMEWORK_OBJS)
	@echo "Test framework built successfully"

# Build all tests
tests: unit integration performance stress security

# Unit tests
unit: $(UNIT_TESTS)
	@echo "Unit tests built successfully"

# Integration tests
integration: $(INTEGRATION_TESTS)
	@echo "Integration tests built successfully"

# Performance tests
performance: $(PERFORMANCE_TESTS)
	@echo "Performance tests built successfully"

# Stress tests
stress: $(STRESS_TESTS)
	@echo "Stress tests built successfully"

# Security tests
security: $(SECURITY_TESTS)
	@echo "Security tests built successfully"

# Generic test build rule
%: $(FRAMEWORK_OBJS)
	@if [ -f $(UNIT_DIR)/$@.c ]; then \
		echo "Building unit test: $@"; \
		$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(UNIT_DIR)/$@.c $(FRAMEWORK_OBJS) $(LIBS); \
	elif [ -f $(INTEGRATION_DIR)/$@.c ]; then \
		echo "Building integration test: $@"; \
		$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(INTEGRATION_DIR)/$@.c $(FRAMEWORK_OBJS) $(LIBS); \
	elif [ -f $(PERFORMANCE_DIR)/$@.c ]; then \
		echo "Building performance test: $@"; \
		$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(PERFORMANCE_DIR)/$@.c $(FRAMEWORK_OBJS) $(LIBS); \
	elif [ -f $(STRESS_DIR)/$@.c ]; then \
		echo "Building stress test: $@"; \
		$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(STRESS_DIR)/$@.c $(FRAMEWORK_OBJS) $(LIBS); \
	elif [ -f $(SECURITY_DIR)/$@.c ]; then \
		echo "Building security test: $@"; \
		$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SECURITY_DIR)/$@.c $(FRAMEWORK_OBJS) $(LIBS); \
	fi

# Compile framework object files
$(FRAMEWORK_DIR)/%.o: $(FRAMEWORK_DIR)/%.c $(FRAMEWORK_DIR)/test_framework.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Create directories
dirs:
	@mkdir -p $(RESULTS_DIR) $(REPORTS_DIR)

# Run all tests
run-all: dirs tests
	@echo "Running all v14.6 tests..."
	@./run_v146_tests.sh --all

# Run unit tests only
run-unit: dirs unit
	@echo "Running unit tests..."
	@./run_v146_tests.sh --unit

# Run integration tests only
run-integration: dirs integration
	@echo "Running integration tests..."
	@./run_v146_tests.sh --integration

# Run performance tests only
run-performance: dirs performance
	@echo "Running performance tests..."
	@./run_v146_tests.sh --performance

# Run stress tests only
run-stress: dirs stress
	@echo "Running stress tests..."
	@./run_v146_tests.sh --stress

# Run security tests only
run-security: dirs security
	@echo "Running security tests..."
	@./run_v146_tests.sh --security

# Quick test (subset for CI)
run-quick: dirs
	@echo "Running quick test suite..."
	@./run_v146_tests.sh --quick

# Generate test report
report:
	@echo "Generating test report..."
	@python3 generate_report.py --format html --output $(REPORTS_DIR)/test_report.html

# Benchmark v13.4 baseline
benchmark-baseline: dirs
	@echo "Benchmarking v13.4 performance..."
	@python3 benchmark_v134.py --output $(RESULTS_DIR)/v134_baseline.json

# Compare with v13.4
compare: dirs performance
	@echo "Comparing v14.6 with v13.4..."
	@./run_v146_tests.sh --performance
	@python3 compare_versions.py \
		--baseline $(RESULTS_DIR)/v134_baseline.json \
		--current $(RESULTS_DIR)/v146_performance.json \
		--output $(REPORTS_DIR)/comparison_report.html

# Valgrind memory check
memcheck: dirs tests
	@echo "Running memory leak detection..."
	@./run_v146_tests.sh --memcheck

# Coverage analysis
coverage: CFLAGS += --coverage
coverage: clean tests
	@echo "Running coverage analysis..."
	@./run_v146_tests.sh --all
	@gcov *.c
	@lcov --capture --directory . --output-file coverage.info
	@genhtml coverage.info --output-directory $(REPORTS_DIR)/coverage

# Static analysis
analyze:
	@echo "Running static analysis..."
	@cppcheck --enable=all --suppress=missingIncludeSystem \
		--error-exitcode=1 \
		-I$(INCLUDES) \
		$(FRAMEWORK_DIR) $(UNIT_DIR) $(INTEGRATION_DIR) \
		$(PERFORMANCE_DIR) $(STRESS_DIR) $(SECURITY_DIR)

# Clean build artifacts
clean:
	@echo "Cleaning test artifacts..."
	@rm -f $(FRAMEWORK_OBJS) $(ALL_TESTS)
	@rm -f *.gcda *.gcno *.gcov coverage.info
	@rm -rf $(RESULTS_DIR) $(REPORTS_DIR)
	@rm -f /tmp/goxel_test* /tmp/test_*

# Install test dependencies
deps:
	@echo "Installing test dependencies..."
	@pip3 install psutil matplotlib pandas jinja2

# Help
help:
	@echo "Goxel v14.6 Test Framework"
	@echo "=========================="
	@echo ""
	@echo "Build targets:"
	@echo "  all              - Build all tests"
	@echo "  framework        - Build test framework only"
	@echo "  unit             - Build unit tests"
	@echo "  integration      - Build integration tests"
	@echo "  performance      - Build performance tests"
	@echo "  stress           - Build stress tests"
	@echo "  security         - Build security tests"
	@echo ""
	@echo "Run targets:"
	@echo "  run-all          - Run all tests"
	@echo "  run-unit         - Run unit tests only"
	@echo "  run-integration  - Run integration tests only"
	@echo "  run-performance  - Run performance tests only"
	@echo "  run-stress       - Run stress tests only"
	@echo "  run-security     - Run security tests only"
	@echo "  run-quick        - Run quick test suite (CI)"
	@echo ""
	@echo "Analysis targets:"
	@echo "  benchmark-baseline - Benchmark v13.4 performance"
	@echo "  compare          - Compare v14.6 with v13.4"
	@echo "  memcheck         - Run valgrind memory check"
	@echo "  coverage         - Generate code coverage report"
	@echo "  analyze          - Run static analysis"
	@echo "  report           - Generate test report"
	@echo ""
	@echo "Other targets:"
	@echo "  clean            - Clean build artifacts"
	@echo "  deps             - Install test dependencies"
	@echo "  help             - Show this help"

.PHONY: all framework tests unit integration performance stress security \
        run-all run-unit run-integration run-performance run-stress \
        run-security run-quick report benchmark-baseline compare \
        memcheck coverage analyze clean deps help dirs