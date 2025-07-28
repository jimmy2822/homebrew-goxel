/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../src/daemon/daemon_lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

// ============================================================================
// TEST FRAMEWORK
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf(ANSI_COLOR_GREEN "  ✓ " ANSI_COLOR_RESET "%s\n", message); \
        tests_passed++; \
    } else { \
        printf(ANSI_COLOR_RED "  ✗ " ANSI_COLOR_RESET "%s\n", message); \
        tests_failed++; \
    } \
} while(0)

#define TEST_SECTION(name) do { \
    printf(ANSI_COLOR_BLUE "\n=== " name " ===" ANSI_COLOR_RESET "\n"); \
} while(0)

#define TEST_SUBSECTION(name) do { \
    printf(ANSI_COLOR_YELLOW "\n--- " name " ---" ANSI_COLOR_RESET "\n"); \
} while(0)

// ============================================================================
// TEST UTILITIES
// ============================================================================

static const char *TEST_PID_FILE = "/tmp/test-goxel-daemon.pid";
static const char *TEST_SOCKET_PATH = "/tmp/test-goxel-daemon.sock";
static const char *TEST_LOG_FILE = "/tmp/test-goxel-daemon.log";

static void cleanup_test_files(void)
{
    unlink(TEST_PID_FILE);
    unlink(TEST_SOCKET_PATH);
    unlink(TEST_LOG_FILE);
}

static daemon_config_t create_test_config(void)
{
    daemon_config_t config = daemon_default_config();
    config.pid_file_path = TEST_PID_FILE;
    config.socket_path = TEST_SOCKET_PATH;
    config.log_file_path = TEST_LOG_FILE;
    config.daemonize = false; // Don't fork for testing
    config.create_pid_file = true;
    config.startup_timeout_ms = 5000;  // 5 seconds for testing
    config.shutdown_timeout_ms = 5000; // 5 seconds for testing
    return config;
}

static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

static void test_daemon_config(void)
{
    TEST_SECTION("Daemon Configuration Tests");
    
    // Test default configuration
    TEST_SUBSECTION("Default Configuration");
    daemon_config_t default_config = daemon_default_config();
    TEST_ASSERT(default_config.pid_file_path != NULL, "Default PID file path is set");
    TEST_ASSERT(default_config.socket_path != NULL, "Default socket path is set");
    TEST_ASSERT(default_config.max_connections > 0, "Default max connections is positive");
    TEST_ASSERT(default_config.startup_timeout_ms > 0, "Default startup timeout is positive");
    TEST_ASSERT(default_config.shutdown_timeout_ms > 0, "Default shutdown timeout is positive");
    
    // Test configuration validation
    TEST_SUBSECTION("Configuration Validation");
    daemon_config_t valid_config = create_test_config();
    TEST_ASSERT(daemon_validate_config(&valid_config) == DAEMON_SUCCESS, "Valid configuration passes validation");
    
    daemon_config_t invalid_config = valid_config;
    invalid_config.pid_file_path = NULL;
    TEST_ASSERT(daemon_validate_config(&invalid_config) != DAEMON_SUCCESS, "Invalid configuration fails validation");
    
    invalid_config = valid_config;
    invalid_config.max_connections = 0;
    TEST_ASSERT(daemon_validate_config(&invalid_config) != DAEMON_SUCCESS, "Zero max connections fails validation");
    
    // Test configuration loading
    TEST_SUBSECTION("Configuration Loading");
    daemon_config_t loaded_config;
    TEST_ASSERT(daemon_load_config(NULL, &loaded_config) == DAEMON_SUCCESS, "Loading with NULL path uses defaults");
    
    // Test directory creation
    TEST_SUBSECTION("Directory Creation");
    TEST_ASSERT(daemon_create_directories(&valid_config) == DAEMON_SUCCESS, "Directory creation succeeds");
}

// ============================================================================
// MOCK INTERFACE TESTS
// ============================================================================

static void test_mock_interfaces(void)
{
    TEST_SECTION("Mock Interface Tests");
    
    // Test mock server
    TEST_SUBSECTION("Mock Server");
    mock_server_t *server = mock_server_create(TEST_SOCKET_PATH);
    TEST_ASSERT(server != NULL, "Mock server creation succeeds");
    TEST_ASSERT(server->socket_path != NULL, "Mock server has socket path");
    TEST_ASSERT(strcmp(server->socket_path, TEST_SOCKET_PATH) == 0, "Mock server socket path is correct");
    TEST_ASSERT(!server->is_running, "Mock server is initially not running");
    
    TEST_ASSERT(mock_server_start(server) == DAEMON_SUCCESS, "Mock server start succeeds");
    TEST_ASSERT(server->is_running, "Mock server is running after start");
    
    TEST_ASSERT(mock_server_stop(server) == DAEMON_SUCCESS, "Mock server stop succeeds");
    TEST_ASSERT(!server->is_running, "Mock server is not running after stop");
    
    mock_server_destroy(server);
    
    // Test mock Goxel instance
    TEST_SUBSECTION("Mock Goxel Instance");
    mock_goxel_instance_t *instance = mock_goxel_create("test.cfg");
    TEST_ASSERT(instance != NULL, "Mock Goxel instance creation succeeds");
    TEST_ASSERT(instance->config_file != NULL, "Mock Goxel instance has config file");
    TEST_ASSERT(strcmp(instance->config_file, "test.cfg") == 0, "Mock Goxel instance config file is correct");
    TEST_ASSERT(!instance->is_initialized, "Mock Goxel instance is initially not initialized");
    
    TEST_ASSERT(mock_goxel_initialize(instance) == DAEMON_SUCCESS, "Mock Goxel instance initialization succeeds");
    TEST_ASSERT(instance->is_initialized, "Mock Goxel instance is initialized");
    
    TEST_ASSERT(mock_goxel_shutdown(instance) == DAEMON_SUCCESS, "Mock Goxel instance shutdown succeeds");
    TEST_ASSERT(!instance->is_initialized, "Mock Goxel instance is not initialized after shutdown");
    
    mock_goxel_destroy(instance);
}

// ============================================================================
// DAEMON CONTEXT TESTS
// ============================================================================

static void test_daemon_context(void)
{
    TEST_SECTION("Daemon Context Tests");
    
    cleanup_test_files();
    
    // Test context creation
    TEST_SUBSECTION("Context Creation");
    daemon_config_t config = create_test_config();
    daemon_context_t *ctx = daemon_context_create(&config);
    TEST_ASSERT(ctx != NULL, "Daemon context creation succeeds");
    TEST_ASSERT(ctx->state == DAEMON_STATE_STOPPED, "Initial state is STOPPED");
    TEST_ASSERT(ctx->server != NULL, "Mock server is created");
    TEST_ASSERT(ctx->goxel_instance != NULL, "Mock Goxel instance is created");
    TEST_ASSERT(!ctx->shutdown_requested, "Shutdown is not initially requested");
    
    // Test state management
    TEST_SUBSECTION("State Management");
    TEST_ASSERT(daemon_get_state(ctx) == DAEMON_STATE_STOPPED, "Get initial state");
    TEST_ASSERT(daemon_set_state(ctx, DAEMON_STATE_STARTING) == DAEMON_SUCCESS, "Set state to STARTING");
    TEST_ASSERT(daemon_get_state(ctx) == DAEMON_STATE_STARTING, "State is now STARTING");
    TEST_ASSERT(!daemon_is_running(ctx), "Daemon is not running in STARTING state");
    
    TEST_ASSERT(daemon_set_state(ctx, DAEMON_STATE_RUNNING) == DAEMON_SUCCESS, "Set state to RUNNING");
    TEST_ASSERT(daemon_is_running(ctx), "Daemon is running in RUNNING state");
    
    TEST_ASSERT(!daemon_shutdown_requested(ctx), "Shutdown not requested initially");
    daemon_request_shutdown(ctx);
    TEST_ASSERT(daemon_shutdown_requested(ctx), "Shutdown requested after request");
    
    // Test error handling
    TEST_SUBSECTION("Error Handling");
    daemon_set_error(ctx, DAEMON_ERROR_CONFIG_INVALID, "Test error message");
    TEST_ASSERT(daemon_get_last_error(ctx) == DAEMON_ERROR_CONFIG_INVALID, "Last error code is correct");
    
    const char *error_msg = daemon_get_last_error_message(ctx);
    TEST_ASSERT(error_msg != NULL, "Error message is not NULL");
    TEST_ASSERT(strcmp(error_msg, "Test error message") == 0, "Error message is correct");
    
    // Test statistics
    TEST_SUBSECTION("Statistics");
    daemon_stats_t stats;
    TEST_ASSERT(daemon_get_stats(ctx, &stats) == DAEMON_SUCCESS, "Get statistics succeeds");
    TEST_ASSERT(stats.state == DAEMON_STATE_RUNNING, "Statistics state is correct");
    
    daemon_increment_requests(ctx);
    daemon_increment_errors(ctx);
    daemon_update_activity(ctx);
    
    TEST_ASSERT(daemon_get_stats(ctx, &stats) == DAEMON_SUCCESS, "Get updated statistics succeeds");
    TEST_ASSERT(stats.total_requests == 1, "Request count is correct");
    TEST_ASSERT(stats.total_errors == 1, "Error count is correct");
    
    daemon_context_destroy(ctx);
    cleanup_test_files();
}

// ============================================================================
// PID FILE TESTS
// ============================================================================

static void test_pid_file_management(void)
{
    TEST_SECTION("PID File Management Tests");
    
    cleanup_test_files();
    
    // Test PID file creation
    TEST_SUBSECTION("PID File Creation");
    TEST_ASSERT(daemon_create_pid_file(TEST_PID_FILE) == DAEMON_SUCCESS, "PID file creation succeeds");
    TEST_ASSERT(access(TEST_PID_FILE, F_OK) == 0, "PID file exists");
    
    // Test reading PID file
    TEST_SUBSECTION("PID File Reading");
    pid_t current_pid = getpid();
    pid_t read_pid;
    TEST_ASSERT(daemon_read_pid_file(TEST_PID_FILE, &read_pid) == DAEMON_SUCCESS, "PID file reading succeeds");
    TEST_ASSERT(read_pid == current_pid, "Read PID matches current PID");
    
    // Test process running check
    TEST_SUBSECTION("Process Running Check");
    TEST_ASSERT(daemon_is_process_running(current_pid), "Current process is running");
    TEST_ASSERT(!daemon_is_process_running(99999), "Non-existent process is not running");
    
    // Test duplicate PID file creation
    TEST_SUBSECTION("Duplicate PID File");
    TEST_ASSERT(daemon_create_pid_file(TEST_PID_FILE) == DAEMON_ERROR_ALREADY_RUNNING, 
                "Duplicate PID file creation fails appropriately");
    
    // Test PID file removal
    TEST_SUBSECTION("PID File Removal");
    TEST_ASSERT(daemon_remove_pid_file(TEST_PID_FILE) == DAEMON_SUCCESS, "PID file removal succeeds");
    TEST_ASSERT(access(TEST_PID_FILE, F_OK) != 0, "PID file no longer exists");
    
    cleanup_test_files();
}

// ============================================================================
// SIGNAL HANDLING TESTS
// ============================================================================

static void test_signal_handling(void)
{
    TEST_SECTION("Signal Handling Tests");
    
    cleanup_test_files();
    
    daemon_config_t config = create_test_config();
    daemon_context_t *ctx = daemon_context_create(&config);
    TEST_ASSERT(ctx != NULL, "Context creation for signal tests");
    
    // Test signal setup
    TEST_SUBSECTION("Signal Setup");
    TEST_ASSERT(daemon_setup_signals(ctx) == DAEMON_SUCCESS, "Signal setup succeeds");
    TEST_ASSERT(daemon_signals_installed(), "Signals are installed");
    
    // Initialize daemon for signal testing
    TEST_ASSERT(daemon_initialize(ctx, NULL) == DAEMON_SUCCESS, "Daemon initialization for signal tests");
    daemon_set_state(ctx, DAEMON_STATE_RUNNING);
    
    // Test SIGHUP handling
    TEST_SUBSECTION("SIGHUP Handling");
    TEST_ASSERT(daemon_test_signal_handling(ctx, SIGHUP) == DAEMON_SUCCESS, "SIGHUP handling works");
    
    // Test SIGTERM handling
    TEST_SUBSECTION("SIGTERM Handling");
    TEST_ASSERT(daemon_test_signal_handling(ctx, SIGTERM) == DAEMON_SUCCESS, "SIGTERM handling works");
    
    // Test SIGINT handling
    TEST_SUBSECTION("SIGINT Handling");
    TEST_ASSERT(daemon_test_signal_handling(ctx, SIGINT) == DAEMON_SUCCESS, "SIGINT handling works");
    
    // Test signal cleanup
    TEST_SUBSECTION("Signal Cleanup");
    TEST_ASSERT(daemon_cleanup_signals() == DAEMON_SUCCESS, "Signal cleanup succeeds");
    TEST_ASSERT(!daemon_signals_installed(), "Signals are not installed after cleanup");
    
    daemon_context_destroy(ctx);
    cleanup_test_files();
}

// ============================================================================
// DAEMON LIFECYCLE TESTS
// ============================================================================

static void test_daemon_lifecycle(void)
{
    TEST_SECTION("Daemon Lifecycle Tests");
    
    cleanup_test_files();
    
    daemon_config_t config = create_test_config();
    daemon_context_t *ctx = daemon_context_create(&config);
    TEST_ASSERT(ctx != NULL, "Context creation for lifecycle tests");
    
    // Test initialization
    TEST_SUBSECTION("Daemon Initialization");
    TEST_ASSERT(daemon_initialize(ctx, NULL) == DAEMON_SUCCESS, "Daemon initialization succeeds");
    TEST_ASSERT(daemon_get_state(ctx) == DAEMON_STATE_STARTING, "State is STARTING after initialization");
    
    // Test start
    TEST_SUBSECTION("Daemon Start");
    TEST_ASSERT(daemon_start(ctx) == DAEMON_SUCCESS, "Daemon start succeeds");
    TEST_ASSERT(daemon_get_state(ctx) == DAEMON_STATE_RUNNING, "State is RUNNING after start");
    TEST_ASSERT(access(TEST_PID_FILE, F_OK) == 0, "PID file exists after start");
    
    // Test running check
    TEST_SUBSECTION("Running Check");
    TEST_ASSERT(daemon_is_running(ctx), "Daemon is running after start");
    TEST_ASSERT(ctx->server->is_running, "Mock server is running");
    TEST_ASSERT(ctx->goxel_instance->is_initialized, "Mock Goxel instance is initialized");
    
    // Test shutdown
    TEST_SUBSECTION("Daemon Shutdown");
    TEST_ASSERT(daemon_shutdown(ctx) == DAEMON_SUCCESS, "Daemon shutdown succeeds");
    TEST_ASSERT(daemon_get_state(ctx) == DAEMON_STATE_STOPPED, "State is STOPPED after shutdown");
    TEST_ASSERT(!daemon_is_running(ctx), "Daemon is not running after shutdown");
    TEST_ASSERT(!ctx->server->is_running, "Mock server is not running after shutdown");
    TEST_ASSERT(!ctx->goxel_instance->is_initialized, "Mock Goxel instance is not initialized after shutdown");
    
    daemon_context_destroy(ctx);
    cleanup_test_files();
}

// ============================================================================
// CONCURRENT DAEMON TEST
// ============================================================================

static void test_concurrent_daemon(void)
{
    TEST_SECTION("Concurrent Daemon Tests");
    
    cleanup_test_files();
    
    // Test that we can't start two daemons with the same PID file
    TEST_SUBSECTION("PID File Locking");
    
    daemon_config_t config1 = create_test_config();
    daemon_context_t *ctx1 = daemon_context_create(&config1);
    TEST_ASSERT(ctx1 != NULL, "First context creation succeeds");
    
    TEST_ASSERT(daemon_initialize(ctx1, NULL) == DAEMON_SUCCESS, "First daemon initialization succeeds");
    TEST_ASSERT(daemon_start(ctx1) == DAEMON_SUCCESS, "First daemon start succeeds");
    
    // Try to start second daemon with same PID file
    daemon_config_t config2 = create_test_config();
    daemon_context_t *ctx2 = daemon_context_create(&config2);
    TEST_ASSERT(ctx2 != NULL, "Second context creation succeeds");
    
    TEST_ASSERT(daemon_initialize(ctx2, NULL) == DAEMON_ERROR_ALREADY_RUNNING, 
                "Second daemon initialization fails (already running)");
    
    // Clean up
    daemon_shutdown(ctx1);
    daemon_context_destroy(ctx1);
    daemon_context_destroy(ctx2);
    cleanup_test_files();
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

static void test_error_handling(void)
{
    TEST_SECTION("Error Handling Tests");
    
    // Test error string function
    TEST_SUBSECTION("Error Strings");
    TEST_ASSERT(daemon_error_string(DAEMON_SUCCESS) != NULL, "Success error string is not NULL");
    TEST_ASSERT(daemon_error_string(DAEMON_ERROR_INVALID_CONTEXT) != NULL, "Error string for invalid context");
    TEST_ASSERT(daemon_error_string(DAEMON_ERROR_UNKNOWN) != NULL, "Error string for unknown error");
    
    // Test invalid parameter handling
    TEST_SUBSECTION("Invalid Parameters");
    TEST_ASSERT(daemon_context_create(NULL) == NULL, "Context creation with NULL config fails");
    TEST_ASSERT(daemon_initialize(NULL, NULL) == DAEMON_ERROR_INVALID_CONTEXT, "Initialize with NULL context fails");
    TEST_ASSERT(daemon_start(NULL) == DAEMON_ERROR_INVALID_CONTEXT, "Start with NULL context fails");
    TEST_ASSERT(daemon_shutdown(NULL) == DAEMON_ERROR_INVALID_CONTEXT, "Shutdown with NULL context fails");
    TEST_ASSERT(daemon_get_state(NULL) == DAEMON_STATE_ERROR, "Get state with NULL context returns error");
    
    // Test PID file error handling
    TEST_SUBSECTION("PID File Errors");
    TEST_ASSERT(daemon_create_pid_file(NULL) == DAEMON_ERROR_INVALID_PARAMETER, 
                "Create PID file with NULL path fails");
    TEST_ASSERT(daemon_read_pid_file("/nonexistent/path", NULL) == DAEMON_ERROR_INVALID_PARAMETER,
                "Read PID file with NULL PID pointer fails");
    
    pid_t dummy_pid;
    TEST_ASSERT(daemon_read_pid_file("/nonexistent/file.pid", &dummy_pid) == DAEMON_ERROR_CONFIG_NOT_FOUND,
                "Read nonexistent PID file fails");
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

static void test_utility_functions(void)
{
    TEST_SECTION("Utility Function Tests");
    
    // Test timestamp function
    TEST_SUBSECTION("Timestamp Function");
    int64_t timestamp1 = daemon_get_timestamp();
    sleep_ms(10);
    int64_t timestamp2 = daemon_get_timestamp();
    TEST_ASSERT(timestamp2 > timestamp1, "Timestamp increases over time");
    TEST_ASSERT((timestamp2 - timestamp1) >= 10000, "Timestamp difference is at least 10ms in microseconds");
    
    // Test sleep function
    TEST_SUBSECTION("Sleep Function");
    int64_t start_time = daemon_get_timestamp();
    daemon_sleep_ms(50);
    int64_t end_time = daemon_get_timestamp();
    int64_t elapsed_us = end_time - start_time;
    int64_t elapsed_ms = elapsed_us / 1000;
    TEST_ASSERT(elapsed_ms >= 45 && elapsed_ms <= 100, "Sleep duration is approximately correct");
    
    // Test signal utilities
    TEST_SUBSECTION("Signal Utilities");
    TEST_ASSERT(strcmp(daemon_signal_name(SIGTERM), "SIGTERM") == 0, "SIGTERM signal name is correct");
    TEST_ASSERT(strcmp(daemon_signal_name(SIGINT), "SIGINT") == 0, "SIGINT signal name is correct");
    TEST_ASSERT(strcmp(daemon_signal_name(SIGHUP), "SIGHUP") == 0, "SIGHUP signal name is correct");
    TEST_ASSERT(strcmp(daemon_signal_name(99999), "UNKNOWN") == 0, "Unknown signal name is correct");
}

// ============================================================================
// STRESS TESTS
// ============================================================================

static void test_stress_scenarios(void)
{
    TEST_SECTION("Stress Test Scenarios");
    
    // Test rapid start/stop cycles
    TEST_SUBSECTION("Rapid Start/Stop Cycles");
    
    for (int i = 0; i < 5; i++) {
        cleanup_test_files();
        
        daemon_config_t config = create_test_config();
        daemon_context_t *ctx = daemon_context_create(&config);
        TEST_ASSERT(ctx != NULL, "Context creation in rapid cycle");
        
        TEST_ASSERT(daemon_initialize(ctx, NULL) == DAEMON_SUCCESS, "Initialization in rapid cycle");
        TEST_ASSERT(daemon_start(ctx) == DAEMON_SUCCESS, "Start in rapid cycle");
        TEST_ASSERT(daemon_shutdown(ctx) == DAEMON_SUCCESS, "Shutdown in rapid cycle");
        
        daemon_context_destroy(ctx);
    }
    
    // Test multiple signal sends
    TEST_SUBSECTION("Multiple Signal Operations");
    cleanup_test_files();
    
    daemon_config_t config = create_test_config();
    daemon_context_t *ctx = daemon_context_create(&config);
    TEST_ASSERT(ctx != NULL, "Context creation for signal stress test");
    
    TEST_ASSERT(daemon_initialize(ctx, NULL) == DAEMON_SUCCESS, "Initialization for signal stress test");
    daemon_set_state(ctx, DAEMON_STATE_RUNNING);
    
    // Send multiple SIGHUP signals rapidly
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(daemon_test_signal_handling(ctx, SIGHUP) == DAEMON_SUCCESS, "Rapid SIGHUP handling");
    }
    
    daemon_context_destroy(ctx);
    cleanup_test_files();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char *argv[])
{
    printf(ANSI_COLOR_BLUE "Goxel v14.0 Daemon Lifecycle Management Tests\n" ANSI_COLOR_RESET);
    printf("===============================================\n");
    
    // Clean up any existing test files
    cleanup_test_files();
    
    // Run test suites
    test_daemon_config();
    test_mock_interfaces();
    test_daemon_context();
    test_pid_file_management();
    test_signal_handling();
    test_daemon_lifecycle();
    test_concurrent_daemon();
    test_error_handling();
    test_utility_functions();
    test_stress_scenarios();
    
    // Final cleanup
    cleanup_test_files();
    
    // Print test summary
    printf("\n" ANSI_COLOR_BLUE "===============================================\n" ANSI_COLOR_RESET);
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  " ANSI_COLOR_GREEN "Passed: %d" ANSI_COLOR_RESET "\n", tests_passed);
    
    if (tests_failed > 0) {
        printf("  " ANSI_COLOR_RED "Failed: %d" ANSI_COLOR_RESET "\n", tests_failed);
        printf("\n" ANSI_COLOR_RED "TESTS FAILED" ANSI_COLOR_RESET "\n");
        return 1;
    } else {
        printf("  Failed: 0\n");
        printf("\n" ANSI_COLOR_GREEN "ALL TESTS PASSED" ANSI_COLOR_RESET "\n");
        return 0;
    }
}