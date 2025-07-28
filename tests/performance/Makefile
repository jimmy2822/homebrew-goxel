# Goxel v14.0 Performance Testing Framework Makefile
# Builds all performance testing tools

CC = gcc
CFLAGS = -Wall -O2 -std=c99 -pthread -g
LDFLAGS = -pthread -lm

# Performance test executables
TESTS = latency_benchmark throughput_test memory_profiling stress_test comparison_framework cli_baseline concurrency_test

# Default target
all: $(TESTS)

# Individual test targets
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

cli_baseline: cli_baseline.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

concurrency_test: concurrency_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Development targets
debug: CFLAGS += -DDEBUG -O0
debug: $(TESTS)

profile: CFLAGS += -pg
profile: $(TESTS)

# Testing targets
test: $(TESTS)
	@echo "Running basic functionality tests..."
	@./latency_benchmark 5 > /dev/null && echo "✓ Latency benchmark works" || echo "✗ Latency benchmark failed"
	@./throughput_test 2 > /dev/null && echo "✓ Throughput test works" || echo "✗ Throughput test failed" 
	@./memory_profiling 2 > /dev/null && echo "✓ Memory profiling works" || echo "✗ Memory profiling failed"
	@./stress_test 2 > /dev/null && echo "✓ Stress test works" || echo "✗ Stress test failed"
	@./comparison_framework > /dev/null && echo "✓ Comparison framework works" || echo "✗ Comparison framework failed"
	@./cli_baseline --version > /dev/null 2>&1 && echo "✓ CLI baseline works" || echo "✗ CLI baseline failed"
	@./concurrency_test 2 10 > /dev/null && echo "✓ Concurrency test works" || echo "✗ Concurrency test failed"

# Cleanup
clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -f core
	rm -f daemon_memory_profile.csv

# Installation (copies to system-wide location)
install: $(TESTS)
	mkdir -p $(DESTDIR)/usr/local/bin/goxel-performance
	cp $(TESTS) $(DESTDIR)/usr/local/bin/goxel-performance/

# Help target
help:
	@echo "Goxel v14.0 Performance Testing Framework"
	@echo "========================================"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build all performance tests"
	@echo "  debug            Build with debug symbols"
	@echo "  profile          Build with profiling enabled"
	@echo "  test             Run basic functionality tests"
	@echo "  clean            Remove built files"
	@echo "  install          Install to system location"
	@echo "  help             Show this help"
	@echo ""
	@echo "Individual tests:"
	@echo "  latency_benchmark    Build latency testing tool"
	@echo "  throughput_test      Build throughput testing tool"
	@echo "  memory_profiling     Build memory analysis tool"
	@echo "  stress_test          Build stress testing tool"
	@echo "  comparison_framework Build performance comparison tool"
	@echo "  cli_baseline         Build CLI baseline measurement tool"
	@echo "  concurrency_test     Build concurrent client testing tool"

.PHONY: all debug profile test clean install help