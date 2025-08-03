#!/bin/bash

# Script to run TDD tests and generate JUnit XML report
# This enables GitLab CI to display test results in the UI

OUTPUT_FILE="test-results.xml"
TEMP_LOG="test_output.tmp"

# Initialize XML report
cat > "$OUTPUT_FILE" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
EOF

# Run each test and capture output
TOTAL_TESTS=0
TOTAL_FAILURES=0
TOTAL_TIME=0

# Test list from Makefile
TESTS="example_voxel_tdd test_daemon_jsonrpc_tdd test_daemon_integration_tdd test_connection_lifecycle"

for test in $TESTS; do
    echo "Running $test..."
    
    # Run test and capture output
    START_TIME=$(date +%s.%N)
    if ./$test > "$TEMP_LOG" 2>&1; then
        TEST_RESULT="pass"
        FAILURES=0
    else
        TEST_RESULT="fail"
        FAILURES=1
        TOTAL_FAILURES=$((TOTAL_FAILURES + 1))
    fi
    END_TIME=$(date +%s.%N)
    
    # Calculate duration
    DURATION=$(echo "$END_TIME - $START_TIME" | bc)
    
    # Count assertions from output
    ASSERTIONS=$(grep -c "PASS\|FAIL" "$TEMP_LOG" || echo "0")
    TOTAL_TESTS=$((TOTAL_TESTS + ASSERTIONS))
    
    # Add test suite to XML
    cat >> "$OUTPUT_FILE" << EOF
  <testsuite name="$test" tests="$ASSERTIONS" failures="$FAILURES" time="$DURATION">
EOF
    
    # Parse individual test results
    TEST_NUM=1
    while IFS= read -r line; do
        if [[ $line == *"Running:"* ]]; then
            TEST_NAME=$(echo "$line" | sed 's/Running: //')
        elif [[ $line == *"✓ PASS"* ]]; then
            cat >> "$OUTPUT_FILE" << EOF
    <testcase classname="$test" name="$TEST_NAME" time="0.001"/>
EOF
            TEST_NUM=$((TEST_NUM + 1))
        elif [[ $line == *"✗ FAIL"* ]]; then
            ERROR_MSG=$(echo "$line" | sed 's/.*FAIL //')
            cat >> "$OUTPUT_FILE" << EOF
    <testcase classname="$test" name="$TEST_NAME" time="0.001">
      <failure message="$ERROR_MSG">$line</failure>
    </testcase>
EOF
            TEST_NUM=$((TEST_NUM + 1))
        fi
    done < "$TEMP_LOG"
    
    # Close test suite
    echo "  </testsuite>" >> "$OUTPUT_FILE"
    
    # Print test output to console
    cat "$TEMP_LOG"
    echo ""
done

# Close XML report
echo "</testsuites>" >> "$OUTPUT_FILE"

# Clean up
rm -f "$TEMP_LOG"

# Summary
echo "================================"
echo "Test Report Generated: $OUTPUT_FILE"
echo "Total Tests: $TOTAL_TESTS"
echo "Total Failures: $TOTAL_FAILURES"
echo "================================"

# Exit with appropriate code
if [ $TOTAL_FAILURES -eq 0 ]; then
    exit 0
else
    exit 1
fi