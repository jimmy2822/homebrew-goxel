/*
 * Goxel v14.6 Unified Test Framework
 * 
 * Comprehensive testing infrastructure for both GUI and headless modes.
 * Provides test fixtures, mocking capabilities, and performance measurement.
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#ifndef GOXEL_TEST_FRAMEWORK_H
#define GOXEL_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

// Test configuration
#define TEST_DEFAULT_TIMEOUT 5000  // 5 seconds
#define TEST_MAX_NAME_LEN 256
#define TEST_MAX_MESSAGE_LEN 1024
#define TEST_PERF_ITERATIONS 1000
#define TEST_SOCKET_PATH "/tmp/goxel_test.sock"
#define TEST_TCP_PORT 9999

// Test result codes
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2,
    TEST_ERROR = 3,
    TEST_TIMEOUT = 4
} test_result_t;

// Test types
typedef enum {
    TEST_TYPE_UNIT,
    TEST_TYPE_INTEGRATION,
    TEST_TYPE_PERFORMANCE,
    TEST_TYPE_STRESS,
    TEST_TYPE_SECURITY
} test_type_t;

// Performance metrics
typedef struct {
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
    double std_dev_ms;
    double percentile_50;
    double percentile_95;
    double percentile_99;
    size_t iterations;
    size_t memory_peak_kb;
    size_t memory_avg_kb;
} perf_metrics_t;

// Test case structure
typedef struct test_case {
    char name[TEST_MAX_NAME_LEN];
    test_type_t type;
    test_result_t (*test_func)(void);
    void (*setup)(void);
    void (*teardown)(void);
    int timeout_ms;
    bool enabled;
    struct test_case *next;
} test_case_t;

// Test suite structure
typedef struct test_suite {
    char name[TEST_MAX_NAME_LEN];
    test_case_t *tests;
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    void (*suite_setup)(void);
    void (*suite_teardown)(void);
    struct test_suite *next;
} test_suite_t;

// Mock server structure
typedef struct {
    int socket_fd;
    pthread_t thread;
    bool running;
    int port;
    const char *socket_path;
    void (*handler)(int client_fd);
} mock_server_t;

// Global test context
typedef struct {
    test_suite_t *suites;
    int total_suites;
    FILE *log_file;
    bool verbose;
    bool stop_on_failure;
    bool measure_performance;
    bool use_color;
    perf_metrics_t *perf_data;
} test_context_t;

// Global test context instance
extern test_context_t g_test_context;

// Test macros
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            test_log_error("Assertion failed: %s at %s:%d", \
                          #condition, __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            test_log_error("Assert equal failed: expected %d, got %d at %s:%d", \
                          (int)(expected), (int)(actual), __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            test_log_error("Assert string equal failed: expected '%s', got '%s' at %s:%d", \
                          (expected), (actual), __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            test_log_error("Assert null failed: pointer is not NULL at %s:%d", \
                          __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            test_log_error("Assert not null failed: pointer is NULL at %s:%d", \
                          __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

// Test registration macros
#define TEST_CASE(name) \
    static test_result_t test_##name(void)

#define TEST_SUITE(name) \
    void register_##name##_tests(void)

#define REGISTER_TEST(suite, test, setup_func, teardown_func) \
    test_register(#suite, #test, test_##test, setup_func, teardown_func, \
                 TEST_TYPE_UNIT, TEST_DEFAULT_TIMEOUT)

#define REGISTER_PERF_TEST(suite, test, setup_func, teardown_func) \
    test_register(#suite, #test, test_##test, setup_func, teardown_func, \
                 TEST_TYPE_PERFORMANCE, TEST_DEFAULT_TIMEOUT)

// Core framework functions
void test_framework_init(void);
void test_framework_cleanup(void);
void test_register(const char *suite_name, const char *test_name,
                  test_result_t (*test_func)(void),
                  void (*setup)(void), void (*teardown)(void),
                  test_type_t type, int timeout_ms);
int test_run_all(void);
int test_run_suite(const char *suite_name);
int test_run_by_type(test_type_t type);

// Logging functions
void test_log_info(const char *fmt, ...);
void test_log_error(const char *fmt, ...);
void test_log_warning(const char *fmt, ...);
void test_log_debug(const char *fmt, ...);
void test_set_log_file(const char *filename);

// Performance measurement
void perf_start_measurement(void);
double perf_end_measurement(void);
void perf_record_iteration(double time_ms);
perf_metrics_t* perf_calculate_metrics(void);
void perf_print_metrics(const char *test_name, perf_metrics_t *metrics);

// Mock server functions
mock_server_t* mock_server_create_unix(const char *socket_path,
                                      void (*handler)(int client_fd));
mock_server_t* mock_server_create_tcp(int port,
                                     void (*handler)(int client_fd));
void mock_server_start(mock_server_t *server);
void mock_server_stop(mock_server_t *server);
void mock_server_destroy(mock_server_t *server);

// Test utilities
int test_create_temp_file(const char *prefix, char *path_out);
void test_cleanup_temp_files(void);
bool test_wait_for_condition(bool (*condition)(void), int timeout_ms);
void test_simulate_latency(int min_ms, int max_ms);
size_t test_get_memory_usage(void);

// Fixture helpers
void* test_fixture_create(size_t size);
void test_fixture_destroy(void *fixture);
void test_fixture_reset(void *fixture, size_t size);

// JSON test helpers
char* test_json_create_request(int id, const char *method, const char *params);
bool test_json_validate_response(const char *response, int expected_id);
char* test_json_extract_result(const char *response);
char* test_json_extract_error(const char *response);

// Socket test helpers
int test_connect_unix_socket(const char *socket_path);
int test_connect_tcp_socket(const char *host, int port);
bool test_send_request(int fd, const char *request);
char* test_receive_response(int fd, int timeout_ms);

// Process test helpers
int test_start_daemon(const char *daemon_path, const char **args);
bool test_daemon_is_running(int pid);
void test_stop_daemon(int pid);
int test_wait_for_daemon(int pid, int timeout_ms);

// Memory leak detection
void test_memory_check_start(void);
bool test_memory_check_end(size_t *leaked_bytes);
void test_memory_report(void);

// Test data generators
void test_generate_random_voxels(uint8_t *data, size_t count);
char* test_generate_gox_file(int size_class); // small, medium, large
char* test_generate_obj_file(int complexity);
char* test_generate_ply_file(int vertex_count);

// Benchmark utilities
typedef struct {
    const char *name;
    void (*benchmark_func)(int iterations);
    int iterations;
    perf_metrics_t metrics;
} benchmark_t;

void benchmark_register(const char *name, void (*func)(int), int iterations);
void benchmark_run_all(void);
void benchmark_compare(const char *baseline_file, const char *current_file);

// Report generation
void test_generate_report(const char *output_file, const char *format);
void test_generate_html_report(const char *output_file);
void test_generate_json_report(const char *output_file);
void test_generate_junit_xml(const char *output_file);

// CI/CD integration
bool test_check_ci_environment(void);
void test_set_ci_mode(bool enabled);
void test_upload_artifacts(const char *directory);

// Additional helpers for urgent tests
pid_t read_pid_file(const char *path);
test_suite_t* find_or_create_suite(const char *suite_name);

#endif // GOXEL_TEST_FRAMEWORK_H