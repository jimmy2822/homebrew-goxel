# Goxel v14.0 Integration Tests Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -g -O2
CFLAGS += -I../../src -I../../src/daemon -I../../ext_src
LDFLAGS = -pthread

# Test programs
TESTS = test_e2e_workflow memory_tests test_specific_scenarios

# Default target
all: $(TESTS)

# E2E workflow test
test_e2e_workflow: test_e2e_workflow.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Memory tests
memory_tests: memory_tests.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Specific scenario tests
test_specific_scenarios: test_specific_scenarios.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Run all tests
run: all
	./full_stack_test.sh

# Run specific test suites
run-basic: all
	./full_stack_test.sh --no-stress --no-performance

run-stress: all
	./full_stack_test.sh --no-basic --no-performance

run-performance: all
	./full_stack_test.sh --no-basic --no-stress

# Quick socket test
quick-test:
	./quick_socket_test.sh

# Clean
clean:
	rm -f $(TESTS)
	rm -rf results logs
	rm -f /tmp/goxel_test*.{sock,pid,log}
	rm -f /tmp/quick_client*

.PHONY: all run run-basic run-stress run-performance quick-test clean