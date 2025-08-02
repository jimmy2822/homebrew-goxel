CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = -lm

TESTS = example_voxel_tdd test_daemon_jsonrpc_tdd test_daemon_integration_tdd

all: $(TESTS) run_tests

example_voxel_tdd: example_voxel_tdd.c tdd_framework.h
	$(CC) $(CFLAGS) -o example_voxel_tdd example_voxel_tdd.c $(LDFLAGS)

test_daemon_jsonrpc_tdd: test_daemon_jsonrpc_tdd.c tdd_framework.h
	$(CC) $(CFLAGS) -o test_daemon_jsonrpc_tdd test_daemon_jsonrpc_tdd.c $(LDFLAGS)

test_daemon_integration_tdd: test_daemon_integration_tdd.c tdd_framework.h
	$(CC) $(CFLAGS) -o test_daemon_integration_tdd test_daemon_integration_tdd.c $(LDFLAGS)

run_tests: $(TESTS)
	@echo "Running TDD tests..."
	@for test in $(TESTS); do \
		echo "\n=== Running $$test ==="; \
		./$$test || exit 1; \
	done
	@echo "\n✅ All TDD tests passed!"

clean:
	rm -f $(TESTS) *.o

.PHONY: all run_tests clean