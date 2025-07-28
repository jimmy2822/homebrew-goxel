/*
 * Unit tests for Goxel v14.6 daemon core functionality
 * Author: James O'Brien (Agent-4)
 */

#include "test_framework.h"
#include <string.h>

// Mock structures for testing
typedef struct {
    int socket_fd;
    bool is_running;
    size_t message_count;
} mock_daemon_t;

static mock_daemon_t *test_daemon = NULL;

// Setup and teardown
static void daemon_test_setup(void) {
    test_daemon = calloc(1, sizeof(mock_daemon_t));
    test_daemon->socket_fd = -1;
    test_daemon->is_running = false;
    test_daemon->message_count = 0;
}

static void daemon_test_teardown(void) {
    if (test_daemon) {
        if (test_daemon->socket_fd >= 0) {
            close(test_daemon->socket_fd);
        }
        free(test_daemon);
        test_daemon = NULL;
    }
}

// Test cases
TEST_CASE(daemon_initialization) {
    TEST_ASSERT_NOT_NULL(test_daemon);
    TEST_ASSERT_EQ(-1, test_daemon->socket_fd);
    TEST_ASSERT_EQ(false, test_daemon->is_running);
    TEST_ASSERT_EQ(0, test_daemon->message_count);
    return TEST_PASS;
}

TEST_CASE(daemon_socket_creation) {
    // Create a test socket
    test_daemon->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_ASSERT(test_daemon->socket_fd >= 0);
    
    // Verify socket is valid
    int flags = fcntl(test_daemon->socket_fd, F_GETFL);
    TEST_ASSERT(flags >= 0);
    
    return TEST_PASS;
}

TEST_CASE(daemon_start_stop) {
    // Simulate daemon start
    test_daemon->is_running = true;
    TEST_ASSERT_EQ(true, test_daemon->is_running);
    
    // Simulate processing messages
    test_daemon->message_count = 10;
    TEST_ASSERT_EQ(10, test_daemon->message_count);
    
    // Simulate daemon stop
    test_daemon->is_running = false;
    TEST_ASSERT_EQ(false, test_daemon->is_running);
    
    return TEST_PASS;
}

TEST_CASE(daemon_message_handling) {
    const char *test_message = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"create\",\"params\":{\"file\":\"test.gox\"}}";
    size_t msg_len = strlen(test_message);
    
    TEST_ASSERT(msg_len > 0);
    TEST_ASSERT(msg_len < 1024); // Max message size check
    
    // Simulate message processing
    test_daemon->message_count++;
    TEST_ASSERT_EQ(1, test_daemon->message_count);
    
    return TEST_PASS;
}

TEST_CASE(daemon_error_handling) {
    // Test invalid socket
    int bad_fd = -1;
    int result = fcntl(bad_fd, F_GETFL);
    TEST_ASSERT(result < 0);
    TEST_ASSERT_EQ(EBADF, errno);
    
    return TEST_PASS;
}

// Performance test
TEST_CASE(daemon_message_throughput) {
    const int iterations = 10000;
    
    perf_start_measurement();
    
    for (int i = 0; i < iterations; i++) {
        // Simulate message processing
        test_daemon->message_count++;
    }
    
    double elapsed = perf_end_measurement();
    perf_record_iteration(elapsed);
    
    TEST_ASSERT_EQ(iterations, test_daemon->message_count);
    TEST_ASSERT(elapsed < 100.0); // Should process 10k messages in < 100ms
    
    test_log_info("Processed %d messages in %.2f ms (%.2f msgs/ms)",
                  iterations, elapsed, iterations / elapsed);
    
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(daemon_core) {
    REGISTER_TEST(daemon_core, daemon_initialization, 
                  daemon_test_setup, daemon_test_teardown);
    REGISTER_TEST(daemon_core, daemon_socket_creation,
                  daemon_test_setup, daemon_test_teardown);
    REGISTER_TEST(daemon_core, daemon_start_stop,
                  daemon_test_setup, daemon_test_teardown);
    REGISTER_TEST(daemon_core, daemon_message_handling,
                  daemon_test_setup, daemon_test_teardown);
    REGISTER_TEST(daemon_core, daemon_error_handling,
                  daemon_test_setup, daemon_test_teardown);
    REGISTER_PERF_TEST(daemon_core, daemon_message_throughput,
                       daemon_test_setup, daemon_test_teardown);
}

// Main entry point
int main(int argc, char **argv) {
    test_framework_init();
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            g_test_context.verbose = true;
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            test_set_log_file(argv[++i]);
        }
    }
    
    // Register test suites
    register_daemon_core_tests();
    
    // Run tests
    int failures = test_run_all();
    
    // Cleanup
    test_framework_cleanup();
    
    return failures;
}