/*
 * Goxel v14.6 Urgent Test Runner
 * 
 * Quick test runner for immediate daemon validation
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "framework/test_framework.h"
#include <stdio.h>
#include <stdlib.h>

// External test suite registrations
extern void register_daemon_lifecycle_tests(void);
extern void register_socket_connection_tests(void);
extern void register_json_rpc_echo_tests(void);
extern void register_daemon_baseline_tests(void);

int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf("Goxel v14.6 Urgent Daemon Validation Tests\n");
    printf("===========================================\n\n");
    
    // Initialize test framework
    test_framework_init();
    
    // Create results directory
    system("mkdir -p results");
    
    // Register test suites
    printf("Registering test suites...\n");
    register_daemon_lifecycle_tests();
    register_socket_connection_tests();
    register_json_rpc_echo_tests();
    register_daemon_baseline_tests();
    
    // Set options
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        g_test_context.verbose = true;
    }
    
    // Run specific suite or all
    int failed = 0;
    if (argc > 1 && strcmp(argv[1], "--suite") == 0 && argc > 2) {
        printf("\nRunning suite: %s\n", argv[2]);
        failed = test_run_suite(argv[2]);
    } else if (argc > 1 && strcmp(argv[1], "--performance") == 0) {
        printf("\nRunning performance tests only...\n");
        failed = test_run_by_type(TEST_TYPE_PERFORMANCE);
    } else {
        printf("\nRunning all urgent tests...\n");
        failed = test_run_all();
    }
    
    // Generate report
    test_generate_json_report("results/urgent_test_results.json");
    
    // Cleanup
    test_framework_cleanup();
    
    // Final status
    printf("\n===========================================\n");
    if (failed == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d tests FAILED!\n", failed);
    }
    printf("===========================================\n");
    
    return failed;
}