# Streamlined Test Makefile for Goxel v13.1 Headless API
# Essential tests only - using existing build system

# Use existing build to create test executables
GOXEL_CLI = ../goxel-headless
SIMPLE_PERF_TEST = simple_perf_test.py

# Essential tests - minimal but comprehensive coverage
TESTS = test_cli_functionality test_performance

all: $(TESTS)

# Test CLI functionality using the built executable
test_cli_functionality:
	@echo "Testing CLI functionality..."
	@echo "#!/bin/bash" > test_cli_functionality
	@echo "set -e" >> test_cli_functionality
	@echo "echo 'Testing create command...'" >> test_cli_functionality
	@echo "$(GOXEL_CLI) create /tmp/test_create.gox || exit 1" >> test_cli_functionality
	@echo "echo 'Testing help command...'" >> test_cli_functionality
	@echo "$(GOXEL_CLI) --help > /dev/null || exit 1" >> test_cli_functionality
	@echo "echo 'Testing version command...'" >> test_cli_functionality
	@echo "$(GOXEL_CLI) --version > /dev/null || exit 1" >> test_cli_functionality
	@echo "echo 'All CLI tests passed!'" >> test_cli_functionality
	@chmod +x test_cli_functionality

# Test performance using existing Python script
test_performance:
	@echo "Testing performance..."
	@echo "#!/bin/bash" > test_performance
	@echo "set -e" >> test_performance
	@echo "echo 'Running performance tests...'" >> test_performance
	@echo "python3 $(SIMPLE_PERF_TEST) || exit 1" >> test_performance
	@echo "echo 'Performance tests passed!'" >> test_performance
	@chmod +x test_performance

# Run essential tests
run-cli-functionality: test_cli_functionality
	@echo "Running CLI functionality tests..."
	@./test_cli_functionality

run-performance: test_performance
	@echo "Running performance tests..."
	@./test_performance

# Run all essential tests
run-all: $(TESTS)
	@echo "Running ALL Goxel v13.1 Essential Tests..."
	@echo "========================================"
	@echo ""
	@$(MAKE) run-cli-functionality
	@echo ""
	@$(MAKE) run-performance
	@echo ""
	@echo "========================================"
	@echo "All essential tests completed!"

# Check if CLI executable exists
check-cli:
	@if [ ! -f "$(GOXEL_CLI)" ]; then \
		echo "ERROR: Goxel CLI executable not found at $(GOXEL_CLI)"; \
		echo "Please build it first with: scons headless=1 cli_tools=1"; \
		exit 1; \
	fi

# Clean test artifacts
clean:
	rm -f test_cli_functionality test_performance
	rm -f /tmp/test_*.gox /tmp/test_*.png /tmp/test_*.obj /tmp/test_*.ply

# Quick test - just CLI functionality
quick: check-cli test_cli_functionality run-cli-functionality

.PHONY: all run-cli-functionality run-performance run-all check-cli clean quick