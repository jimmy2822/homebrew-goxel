# Goxel v14.0 Integration Test Suite

Comprehensive integration testing for the v14 daemon architecture.

## Overview

This test suite validates all aspects of the v14 daemon:
- Socket communication and connectivity
- JSON-RPC method implementations
- TypeScript client integration
- Performance benchmarks
- Stress testing and stability
- Memory leak detection

## Quick Start

### 1. Quick Socket Test (Run This First!)

```bash
# Quick test to verify socket communication is working
make quick-test
```

This runs a focused test that:
- Starts the daemon
- Verifies socket creation
- Tests basic client connection
- Sends a JSON-RPC request
- Validates the response

### 2. Run Full Test Suite

```bash
# Build and run all tests
make run

# Run specific test categories
make run-basic      # Connectivity and methods only
make run-stress     # Stress tests only
make run-performance # Performance benchmarks only
```

## Test Categories

### Basic Connectivity Tests
- **Daemon Startup**: Verifies daemon starts and creates socket
- **Single Client**: Tests basic client connection
- **Multiple Clients**: Tests concurrent connections
- **Graceful Shutdown**: Validates clean shutdown

### JSON-RPC Method Tests
- **Basic Methods**: echo, version, status, ping
- **Voxel Operations**: create_project, add_voxel, get_voxel, remove_voxel
- **File Operations**: save_project, load_project, export_model
- **Error Handling**: Invalid methods, malformed requests

### TypeScript Client Tests
- **Connection Management**: Single and pooled connections
- **Request/Response**: Timeout, retry, error handling
- **Health Monitoring**: Connection health checks

### Performance Tests
- **Latency**: Average response time (target: <2.1ms)
- **Throughput**: Operations per second (target: >1000 ops/s)
- **Memory Usage**: Peak memory consumption (target: <50MB)
- **CPU Usage**: Average CPU utilization (target: <80%)

### Stress Tests
- **Concurrent Clients**: 10, 50, 100+ simultaneous connections
- **High Load**: 1000+ sequential operations
- **Burst Traffic**: Sudden load spikes
- **Long Running**: 1-hour stability test
- **Memory Leaks**: Valgrind memory analysis

## Test Scripts

### `full_stack_test.sh`
Main test orchestrator that runs all test suites and generates reports.

Options:
- `--no-cleanup`: Skip cleanup phase
- `--no-build`: Skip build phase
- `--no-basic`: Skip basic tests
- `--no-stress`: Skip stress tests
- `--no-performance`: Skip performance tests
- `--no-client`: Skip TypeScript tests
- `--long-test`: Include 1-hour stability test

### `quick_socket_test.sh`
Focused test for debugging socket communication issues.

### `validate_results.py`
Analyzes test results and determines production readiness.

## Test Programs

### `test_e2e_workflow`
Comprehensive end-to-end test suite covering all scenarios.

### `test_specific_scenarios`
Targeted tests that can be run individually:
```bash
./test_specific_scenarios -t startup
./test_specific_scenarios -t method_echo
./test_specific_scenarios -t stress_50_clients
```

## Performance Targets

| Metric | Target | Critical |
|--------|--------|----------|
| Average Latency | <2.1ms | <5ms |
| Throughput | >1000 ops/s | >500 ops/s |
| Memory Usage | <50MB | <100MB |
| CPU Usage | <80% | <95% |
| Concurrent Clients | 50+ | 20+ |
| Success Rate | >99% | >95% |

## Test Results

Results are saved in:
- `results/`: Test reports and validation summaries
- `logs/`: Detailed test logs
- `performance/`: Performance benchmark data

## Debugging Failed Tests

### Socket Connection Issues
1. Run `make quick-test` to isolate the problem
2. Check daemon logs in `/tmp/goxel_test.log`
3. Verify socket permissions and path
4. Use `strace` or `dtruss` to trace system calls

### Performance Issues
1. Run individual performance tests
2. Use profiling tools (perf, instruments)
3. Check for lock contention
4. Monitor system resources during tests

### Memory Leaks
1. Run with valgrind: `valgrind --leak-check=full ./test_e2e_workflow`
2. Use AddressSanitizer: compile with `-fsanitize=address`
3. Check for unreleased resources

## Production Readiness Criteria

The v14 daemon is considered production-ready when:
1. ✅ All connectivity tests pass
2. ✅ All JSON-RPC methods work correctly
3. ✅ Performance targets are met
4. ✅ No memory leaks detected
5. ✅ Supports 50+ concurrent clients
6. ✅ 99%+ success rate under load
7. ✅ Stable for 1+ hour continuous operation

## Known Issues

### macOS Socket Issue (Under Investigation)
- **Problem**: Socket creation/connection may fail on macOS
- **Symptoms**: "Address already in use" or connection refused
- **Workaround**: Use different socket path or restart
- **Status**: Being debugged by dedicated agent

## Contributing

When adding new tests:
1. Add test scenario to `test_specific_scenarios.c`
2. Update test categories in this README
3. Add validation logic to `validate_results.py`
4. Update performance targets if needed
5. Document any new dependencies or requirements