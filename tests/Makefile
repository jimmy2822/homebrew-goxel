# Comprehensive Test Makefile for Goxel v13 Headless API
# Phase 6: Production Ready - Complete Test Suite

CC = gcc
CFLAGS = -std=gnu99 -Wall -g -O0 -I../src -I../ext_src -I../include
LIBS = -lm -ldl -pthread

# Common includes and external libraries
EXT_SOURCES = \
    ../ext_src/stb/stb_image.c \
    ../ext_src/uthash/uthash.c \
    ../src/xxhash.c

# Core sources for headless testing (no GUI dependencies)
CORE_SOURCES = \
    ../src/core/goxel_core.c \
    ../src/core/project_mgmt.c \
    ../src/core/volume_ops.c \
    ../src/core/volume_utils.c \
    ../src/core/image.c \
    ../src/core/layer.c \
    ../src/core/material.c \
    ../src/core/shape.c \
    ../src/core/palette.c \
    ../src/core/file_format.c \
    ../src/core/utils/box.c \
    ../src/core/utils/cache.c \
    ../src/core/utils/color.c \
    ../src/core/utils/geometry.c \
    ../src/core/utils/img.c \
    ../src/core/utils/path.c \
    ../src/core/utils/vec.c \
    ../src/core/utils/json.c \
    ../src/core/utils/b64.c \
    ../src/core/utils/ini.c \
    ../src/volume.c \
    ../src/volume_utils.c

# Headless-specific sources
HEADLESS_SOURCES = \
    ../src/headless/render_headless.c \
    ../src/headless/camera_headless.c \
    ../src/headless/cli_interface.c \
    ../src/headless/cli_commands.c

# Test executables
TESTS = test_core test_rendering test_cli test_formats test_memory_perf test_e2e_integration test_cli_execution

# Test sources
TEST_CORE_SOURCES = core/test_goxel_core.c
TEST_RENDERING_SOURCES = core/test_headless_rendering.c
TEST_CLI_SOURCES = core/test_cli_interface.c
TEST_CLI_EXECUTION_SOURCES = core/test_cli_commands_execution.c
TEST_FORMATS_SOURCES = core/test_file_formats.c
TEST_MEMORY_PERF_SOURCES = core/test_memory_perf.c
TEST_E2E_SOURCES = integration/test_e2e_headless.c

all: $(TESTS)

# Individual test targets
test_core: $(TEST_CORE_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_CORE_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES) $(LIBS)

test_rendering: $(TEST_RENDERING_SOURCES) $(CORE_SOURCES) $(HEADLESS_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_RENDERING_SOURCES) $(CORE_SOURCES) $(HEADLESS_SOURCES) $(EXT_SOURCES) $(LIBS)

test_cli: $(TEST_CLI_SOURCES) $(CORE_SOURCES) $(HEADLESS_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_CLI_SOURCES) $(CORE_SOURCES) $(HEADLESS_SOURCES) $(EXT_SOURCES) $(LIBS)

test_cli_execution: $(TEST_CLI_EXECUTION_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_CLI_EXECUTION_SOURCES) $(LIBS)

test_formats: $(TEST_FORMATS_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_FORMATS_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES) $(LIBS)

test_memory_perf: $(TEST_MEMORY_PERF_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_MEMORY_PERF_SOURCES) $(CORE_SOURCES) $(EXT_SOURCES) $(LIBS)

test_e2e_integration: $(TEST_E2E_SOURCES) ../src/headless/goxel_headless_api.c $(CORE_SOURCES) $(EXT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_E2E_SOURCES) ../src/headless/goxel_headless_api.c $(CORE_SOURCES) $(EXT_SOURCES) $(LIBS)

# Comprehensive test runner
test_comprehensive: all
	@echo "Running Goxel v13 Comprehensive Test Suite"
	@echo "==========================================="
	@./run_all_tests.sh

# Individual test runs
run-core: test_core
	@echo "Running Core API Tests..."
	@./test_core

run-rendering: test_rendering
	@echo "Running Headless Rendering Tests..."
	@./test_rendering

run-cli: test_cli
	@echo "Running CLI Interface Tests..."
	@./test_cli

run-cli-execution: test_cli_execution
	@echo "Running CLI Command Execution Tests..."
	@./test_cli_execution

run-formats: test_formats
	@echo "Running File Format Tests..."
	@./test_formats

run-memory: test_memory_perf
	@echo "Running Memory and Performance Tests..."
	@./test_memory_perf

run-integration: test_e2e_integration
	@echo "Running End-to-End Integration Tests..."
	@./test_e2e_integration

# Run all tests sequentially
run-all: $(TESTS)
	@echo "Running ALL Goxel v13 Tests..."
	@echo "=============================="
	@echo ""
	@$(MAKE) run-core
	@echo ""
	@$(MAKE) run-rendering
	@echo ""
	@$(MAKE) run-cli
	@echo ""
	@$(MAKE) run-cli-execution
	@echo ""
	@$(MAKE) run-formats
	@echo ""
	@$(MAKE) run-memory
	@echo ""
	@$(MAKE) run-integration
	@echo ""
	@echo "=============================="
	@echo "All test suites completed!"

# Coverage analysis (requires gcov)
coverage: CFLAGS += --coverage
coverage: $(TESTS)
	@echo "Running tests with coverage analysis..."
	@$(MAKE) run-all
	@gcov $(CORE_SOURCES) $(HEADLESS_SOURCES)
	@echo "Coverage analysis complete. Check *.gcov files."

# Performance benchmarking
benchmark: test_memory_perf
	@echo "Running performance benchmarks..."
	@./test_memory_perf | tee benchmark_results.txt
	@echo "Benchmark results saved to benchmark_results.txt"

# Memory leak detection (requires valgrind)
memcheck: test_core
	@echo "Running memory leak detection..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_core

# Clean all test artifacts
clean:
	rm -f $(TESTS)
	rm -f *.gcov *.gcda *.gcno
	rm -f benchmark_results.txt
	rm -f /tmp/test_*.gox /tmp/test_*.png /tmp/test_*.obj /tmp/test_*.ply

# Development shortcuts
quick: test_core run-core
dev: test_core test_cli run-core run-cli

.PHONY: all run-core run-rendering run-cli run-formats run-memory run-all coverage benchmark memcheck clean quick dev