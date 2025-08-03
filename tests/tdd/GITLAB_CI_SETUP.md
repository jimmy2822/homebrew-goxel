# GitLab CI Setup for TDD Tests

## Overview

This GitLab CI configuration automatically runs all TDD (Test-Driven Development) tests whenever code is pushed to the repository. The CI pipeline ensures code quality by verifying that all tests pass before allowing merges.

## Features

- **Automated TDD Test Execution**: Runs all TDD tests on every push
- **JUnit Test Reports**: Generates test reports visible in GitLab UI
- **Parallel Test Jobs**: Runs different test categories in parallel for faster feedback
- **Memory Leak Detection**: Optional Valgrind checks for memory issues
- **Code Coverage Reports**: Optional coverage analysis (manual trigger)
- **Quick Tests for MRs**: Fast sanity checks on merge requests

## Pipeline Structure

### Stages
1. **Build Stage**: Compiles the daemon with debug symbols
2. **Test Stage**: Runs various test suites

### Test Jobs

#### `test:tdd` (Main TDD Suite)
- Runs all TDD tests using the test runner
- Generates JUnit XML reports for GitLab integration
- Artifacts: test logs and results

#### Individual Test Jobs
- `test:daemon_jsonrpc`: JSON-RPC protocol tests
- `test:daemon_integration`: Integration tests
- `test:connection_lifecycle`: Connection handling tests

#### Optional Jobs (Manual Trigger)
- `test:valgrind`: Memory leak detection
- `test:coverage`: Code coverage analysis

## Test Reports

The pipeline generates several types of reports:

1. **JUnit Reports**: Viewable in GitLab's test report UI
2. **Console Logs**: Full test output for debugging
3. **Coverage Reports**: Code coverage metrics (when enabled)

## Local Testing

Before pushing, you can run tests locally:

```bash
# Run all TDD tests
cd tests
./run_tdd_tests.sh

# Generate JUnit report locally
cd tests/tdd
make all
./generate_junit_report.sh
```

## Viewing Results in GitLab

1. **Pipeline View**: Check the pipeline status on the commits page
2. **Test Reports**: Click on the pipeline → Tests tab to see detailed results
3. **Artifacts**: Download test logs from the job artifacts

## Troubleshooting

### Test Failures
- Check the job logs for detailed error messages
- Download artifacts for complete test output
- Run tests locally to reproduce issues

### Build Issues
- Ensure all dependencies are listed in the CI config
- Check that the build commands match local development

### Report Generation
- JUnit reports require the `bc` command for timing calculations
- Ensure test output follows the expected format

## Customization

To add new tests:
1. Add test executable to `TESTS` variable in `tests/tdd/Makefile`
2. Ensure test uses the TDD framework macros
3. The CI will automatically pick up new tests

## Best Practices

1. **Write Tests First**: Follow TDD principles
2. **Keep Tests Fast**: CI runs on every push
3. **Fix Failures Immediately**: Don't push with failing tests
4. **Use Test Categories**: Group related tests together
5. **Monitor Coverage**: Aim for high test coverage

## Dependencies

The CI environment includes:
- Ubuntu 22.04
- GCC/G++ compiler
- Make and SCons
- Required libraries (GLFW, GTK3, PNG, etc.)
- Testing tools (Valgrind, gcovr)

## Contact

For CI/CD issues or improvements, please open an issue in the GitLab project.