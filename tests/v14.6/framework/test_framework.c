/*
 * Goxel v14.6 Unified Test Framework - Implementation
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "test_framework.h"
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <dirent.h>

// Global test context
test_context_t g_test_context = {0};

// ANSI color codes
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

// Performance data storage
static double *perf_samples = NULL;
static size_t perf_sample_count = 0;
static size_t perf_sample_capacity = 0;
static struct timeval perf_start_time;

// Temporary file tracking
static char **temp_files = NULL;
static size_t temp_file_count = 0;

// Framework initialization
void test_framework_init(void) {
    memset(&g_test_context, 0, sizeof(g_test_context));
    g_test_context.verbose = false;
    g_test_context.use_color = isatty(STDOUT_FILENO);
    g_test_context.stop_on_failure = false;
    g_test_context.measure_performance = false;
    
    // Check for CI environment
    const char *ci_env = getenv("CI");
    if (ci_env && strcmp(ci_env, "true") == 0) {
        g_test_context.use_color = false;
    }
}

// Framework cleanup
void test_framework_cleanup(void) {
    // Free test suites
    test_suite_t *suite = g_test_context.suites;
    while (suite) {
        test_suite_t *next_suite = suite->next;
        
        // Free test cases
        test_case_t *test = suite->tests;
        while (test) {
            test_case_t *next_test = test->next;
            free(test);
            test = next_test;
        }
        
        free(suite);
        suite = next_suite;
    }
    
    // Close log file if open
    if (g_test_context.log_file) {
        fclose(g_test_context.log_file);
    }
    
    // Free performance data
    if (perf_samples) {
        free(perf_samples);
        perf_samples = NULL;
    }
    
    // Cleanup temp files
    test_cleanup_temp_files();
}

// Find or create test suite
static test_suite_t* find_or_create_suite(const char *suite_name) {
    test_suite_t *suite = g_test_context.suites;
    
    // Search for existing suite
    while (suite) {
        if (strcmp(suite->name, suite_name) == 0) {
            return suite;
        }
        suite = suite->next;
    }
    
    // Create new suite
    suite = calloc(1, sizeof(test_suite_t));
    strncpy(suite->name, suite_name, TEST_MAX_NAME_LEN - 1);
    
    // Add to list
    suite->next = g_test_context.suites;
    g_test_context.suites = suite;
    g_test_context.total_suites++;
    
    return suite;
}

// Register a test
void test_register(const char *suite_name, const char *test_name,
                  test_result_t (*test_func)(void),
                  void (*setup)(void), void (*teardown)(void),
                  test_type_t type, int timeout_ms) {
    test_suite_t *suite = find_or_create_suite(suite_name);
    
    // Create test case
    test_case_t *test = calloc(1, sizeof(test_case_t));
    snprintf(test->name, TEST_MAX_NAME_LEN, "%s::%s", suite_name, test_name);
    test->type = type;
    test->test_func = test_func;
    test->setup = setup;
    test->teardown = teardown;
    test->timeout_ms = timeout_ms;
    test->enabled = true;
    
    // Add to suite
    test->next = suite->tests;
    suite->tests = test;
    suite->total_tests++;
}

// Run a single test with timeout
static test_result_t run_test_with_timeout(test_case_t *test) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - run the test
        alarm(test->timeout_ms / 1000);
        
        if (test->setup) {
            test->setup();
        }
        
        test_result_t result = test->test_func();
        
        if (test->teardown) {
            test->teardown();
        }
        
        exit(result);
    } else if (pid > 0) {
        // Parent process - wait for child
        int status;
        int timeout_ms = test->timeout_ms;
        
        while (timeout_ms > 0) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                if (WIFEXITED(status)) {
                    return (test_result_t)WEXITSTATUS(status);
                } else {
                    return TEST_ERROR;
                }
            } else if (result < 0) {
                return TEST_ERROR;
            }
            
            usleep(10000); // 10ms
            timeout_ms -= 10;
        }
        
        // Timeout - kill the child
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        return TEST_TIMEOUT;
    } else {
        return TEST_ERROR;
    }
}

// Run a single test
static test_result_t run_single_test(test_case_t *test) {
    test_log_info("Running test: %s", test->name);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    test_result_t result;
    if (test->timeout_ms > 0) {
        result = run_test_with_timeout(test);
    } else {
        if (test->setup) {
            test->setup();
        }
        
        result = test->test_func();
        
        if (test->teardown) {
            test->teardown();
        }
    }
    
    gettimeofday(&end, NULL);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                       (end.tv_usec - start.tv_usec) / 1000.0;
    
    // Log result
    const char *color = COLOR_RESET;
    const char *status = "UNKNOWN";
    
    switch (result) {
        case TEST_PASS:
            color = COLOR_GREEN;
            status = "PASS";
            break;
        case TEST_FAIL:
            color = COLOR_RED;
            status = "FAIL";
            break;
        case TEST_SKIP:
            color = COLOR_YELLOW;
            status = "SKIP";
            break;
        case TEST_ERROR:
            color = COLOR_RED;
            status = "ERROR";
            break;
        case TEST_TIMEOUT:
            color = COLOR_RED;
            status = "TIMEOUT";
            break;
    }
    
    if (g_test_context.use_color) {
        printf("%s[%s]%s %s (%.2f ms)\n", color, status, COLOR_RESET,
               test->name, elapsed_ms);
    } else {
        printf("[%s] %s (%.2f ms)\n", status, test->name, elapsed_ms);
    }
    
    return result;
}

// Run all tests
int test_run_all(void) {
    int total_passed = 0;
    int total_failed = 0;
    int total_skipped = 0;
    
    test_suite_t *suite = g_test_context.suites;
    while (suite) {
        test_log_info("Running suite: %s", suite->name);
        
        if (suite->suite_setup) {
            suite->suite_setup();
        }
        
        test_case_t *test = suite->tests;
        while (test) {
            if (!test->enabled) {
                test = test->next;
                continue;
            }
            
            test_result_t result = run_single_test(test);
            
            switch (result) {
                case TEST_PASS:
                    suite->passed_tests++;
                    total_passed++;
                    break;
                case TEST_SKIP:
                    suite->skipped_tests++;
                    total_skipped++;
                    break;
                default:
                    suite->failed_tests++;
                    total_failed++;
                    if (g_test_context.stop_on_failure) {
                        goto done;
                    }
                    break;
            }
            
            test = test->next;
        }
        
        if (suite->suite_teardown) {
            suite->suite_teardown();
        }
        
        suite = suite->next;
    }
    
done:
    // Print summary
    printf("\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", total_passed + total_failed + total_skipped);
    printf("  Passed:      %d\n", total_passed);
    printf("  Failed:      %d\n", total_failed);
    printf("  Skipped:     %d\n", total_skipped);
    
    return total_failed;
}

// Run tests by suite name
int test_run_suite(const char *suite_name) {
    test_suite_t *suite = g_test_context.suites;
    
    while (suite) {
        if (strcmp(suite->name, suite_name) == 0) {
            int passed = 0;
            int failed = 0;
            
            test_log_info("Running suite: %s", suite->name);
            
            if (suite->suite_setup) {
                suite->suite_setup();
            }
            
            test_case_t *test = suite->tests;
            while (test) {
                if (!test->enabled) {
                    test = test->next;
                    continue;
                }
                
                test_result_t result = run_single_test(test);
                
                if (result == TEST_PASS) {
                    passed++;
                } else if (result != TEST_SKIP) {
                    failed++;
                    if (g_test_context.stop_on_failure) {
                        break;
                    }
                }
                
                test = test->next;
            }
            
            if (suite->suite_teardown) {
                suite->suite_teardown();
            }
            
            return failed;
        }
        suite = suite->next;
    }
    
    test_log_error("Suite not found: %s", suite_name);
    return -1;
}

// Run tests by type
int test_run_by_type(test_type_t type) {
    int total_passed = 0;
    int total_failed = 0;
    
    test_suite_t *suite = g_test_context.suites;
    while (suite) {
        test_case_t *test = suite->tests;
        while (test) {
            if (test->enabled && test->type == type) {
                test_result_t result = run_single_test(test);
                
                if (result == TEST_PASS) {
                    total_passed++;
                } else if (result != TEST_SKIP) {
                    total_failed++;
                    if (g_test_context.stop_on_failure) {
                        return total_failed;
                    }
                }
            }
            test = test->next;
        }
        suite = suite->next;
    }
    
    return total_failed;
}

// Logging functions
static void test_log_v(const char *level, const char *color,
                      const char *fmt, va_list args) {
    char buffer[TEST_MAX_MESSAGE_LEN];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    // Console output
    if (g_test_context.use_color && color) {
        printf("%s[%s]%s %s\n", color, level, COLOR_RESET, buffer);
    } else {
        printf("[%s] %s\n", level, buffer);
    }
    
    // File output
    if (g_test_context.log_file) {
        fprintf(g_test_context.log_file, "[%s] %s\n", level, buffer);
        fflush(g_test_context.log_file);
    }
}

void test_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    test_log_v("INFO", COLOR_CYAN, fmt, args);
    va_end(args);
}

void test_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    test_log_v("ERROR", COLOR_RED, fmt, args);
    va_end(args);
}

void test_log_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    test_log_v("WARNING", COLOR_YELLOW, fmt, args);
    va_end(args);
}

void test_log_debug(const char *fmt, ...) {
    if (g_test_context.verbose) {
        va_list args;
        va_start(args, fmt);
        test_log_v("DEBUG", COLOR_MAGENTA, fmt, args);
        va_end(args);
    }
}

void test_set_log_file(const char *filename) {
    if (g_test_context.log_file) {
        fclose(g_test_context.log_file);
    }
    
    g_test_context.log_file = fopen(filename, "w");
    if (!g_test_context.log_file) {
        test_log_error("Failed to open log file: %s", filename);
    }
}

// Performance measurement
void perf_start_measurement(void) {
    gettimeofday(&perf_start_time, NULL);
}

double perf_end_measurement(void) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    
    double elapsed = (end_time.tv_sec - perf_start_time.tv_sec) * 1000.0 +
                    (end_time.tv_usec - perf_start_time.tv_usec) / 1000.0;
    
    return elapsed;
}

void perf_record_iteration(double time_ms) {
    if (perf_sample_count >= perf_sample_capacity) {
        size_t new_capacity = perf_sample_capacity == 0 ? 1000 : perf_sample_capacity * 2;
        perf_samples = realloc(perf_samples, new_capacity * sizeof(double));
        perf_sample_capacity = new_capacity;
    }
    
    perf_samples[perf_sample_count++] = time_ms;
}

static int compare_doubles(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

perf_metrics_t* perf_calculate_metrics(void) {
    if (perf_sample_count == 0) {
        return NULL;
    }
    
    perf_metrics_t *metrics = calloc(1, sizeof(perf_metrics_t));
    metrics->iterations = perf_sample_count;
    
    // Sort samples
    qsort(perf_samples, perf_sample_count, sizeof(double), compare_doubles);
    
    // Calculate basic stats
    metrics->min_time_ms = perf_samples[0];
    metrics->max_time_ms = perf_samples[perf_sample_count - 1];
    
    double sum = 0;
    for (size_t i = 0; i < perf_sample_count; i++) {
        sum += perf_samples[i];
    }
    metrics->avg_time_ms = sum / perf_sample_count;
    
    // Calculate standard deviation
    double variance = 0;
    for (size_t i = 0; i < perf_sample_count; i++) {
        double diff = perf_samples[i] - metrics->avg_time_ms;
        variance += diff * diff;
    }
    metrics->std_dev_ms = sqrt(variance / perf_sample_count);
    
    // Calculate percentiles
    metrics->percentile_50 = perf_samples[perf_sample_count / 2];
    metrics->percentile_95 = perf_samples[(size_t)(perf_sample_count * 0.95)];
    metrics->percentile_99 = perf_samples[(size_t)(perf_sample_count * 0.99)];
    
    // Reset for next measurement
    perf_sample_count = 0;
    
    return metrics;
}

void perf_print_metrics(const char *test_name, perf_metrics_t *metrics) {
    printf("\nPerformance Metrics for %s:\n", test_name);
    printf("  Iterations:   %zu\n", metrics->iterations);
    printf("  Min time:     %.3f ms\n", metrics->min_time_ms);
    printf("  Max time:     %.3f ms\n", metrics->max_time_ms);
    printf("  Average:      %.3f ms\n", metrics->avg_time_ms);
    printf("  Std dev:      %.3f ms\n", metrics->std_dev_ms);
    printf("  50th %%ile:    %.3f ms\n", metrics->percentile_50);
    printf("  95th %%ile:    %.3f ms\n", metrics->percentile_95);
    printf("  99th %%ile:    %.3f ms\n", metrics->percentile_99);
    
    if (metrics->memory_peak_kb > 0) {
        printf("  Peak memory:  %zu KB\n", metrics->memory_peak_kb);
        printf("  Avg memory:   %zu KB\n", metrics->memory_avg_kb);
    }
}

// Mock server implementation (simplified for brevity)
mock_server_t* mock_server_create_unix(const char *socket_path,
                                      void (*handler)(int client_fd)) {
    mock_server_t *server = calloc(1, sizeof(mock_server_t));
    server->socket_path = socket_path;
    server->handler = handler;
    
    // Create Unix domain socket
    server->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        free(server);
        return NULL;
    }
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    unlink(socket_path); // Remove if exists
    
    if (bind(server->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    
    if (listen(server->socket_fd, 5) < 0) {
        close(server->socket_fd);
        unlink(socket_path);
        free(server);
        return NULL;
    }
    
    return server;
}

// Test utilities
int test_create_temp_file(const char *prefix, char *path_out) {
    char template[256];
    snprintf(template, sizeof(template), "/tmp/%s_XXXXXX", prefix);
    
    int fd = mkstemp(template);
    if (fd >= 0) {
        strcpy(path_out, template);
        
        // Track for cleanup
        temp_files = realloc(temp_files, (temp_file_count + 1) * sizeof(char *));
        temp_files[temp_file_count] = strdup(template);
        temp_file_count++;
    }
    
    return fd;
}

void test_cleanup_temp_files(void) {
    for (size_t i = 0; i < temp_file_count; i++) {
        unlink(temp_files[i]);
        free(temp_files[i]);
    }
    free(temp_files);
    temp_files = NULL;
    temp_file_count = 0;
}

bool test_wait_for_condition(bool (*condition)(void), int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (condition()) {
            return true;
        }
        usleep(10000); // 10ms
        elapsed += 10;
    }
    return false;
}

void test_simulate_latency(int min_ms, int max_ms) {
    int delay_ms = min_ms + (rand() % (max_ms - min_ms + 1));
    usleep(delay_ms * 1000);
}

size_t test_get_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss; // In KB on Linux, bytes on macOS
}

// JSON helpers
char* test_json_create_request(int id, const char *method, const char *params) {
    char *request = malloc(4096);
    if (params) {
        snprintf(request, 4096,
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
                id, method, params);
    } else {
        snprintf(request, 4096,
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\"}",
                id, method);
    }
    return request;
}

// Report generation
void test_generate_json_report(const char *output_file) {
    FILE *fp = fopen(output_file, "w");
    if (!fp) {
        test_log_error("Failed to create report: %s", output_file);
        return;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_run\": {\n");
    fprintf(fp, "    \"timestamp\": \"%ld\",\n", time(NULL));
    fprintf(fp, "    \"total_suites\": %d,\n", g_test_context.total_suites);
    
    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    int total_skipped = 0;
    
    test_suite_t *suite = g_test_context.suites;
    while (suite) {
        total_tests += suite->total_tests;
        total_passed += suite->passed_tests;
        total_failed += suite->failed_tests;
        total_skipped += suite->skipped_tests;
        suite = suite->next;
    }
    
    fprintf(fp, "    \"total_tests\": %d,\n", total_tests);
    fprintf(fp, "    \"passed\": %d,\n", total_passed);
    fprintf(fp, "    \"failed\": %d,\n", total_failed);
    fprintf(fp, "    \"skipped\": %d\n", total_skipped);
    fprintf(fp, "  },\n");
    
    fprintf(fp, "  \"suites\": [\n");
    
    suite = g_test_context.suites;
    while (suite) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", suite->name);
        fprintf(fp, "      \"total\": %d,\n", suite->total_tests);
        fprintf(fp, "      \"passed\": %d,\n", suite->passed_tests);
        fprintf(fp, "      \"failed\": %d,\n", suite->failed_tests);
        fprintf(fp, "      \"skipped\": %d\n", suite->skipped_tests);
        fprintf(fp, "    }%s\n", suite->next ? "," : "");
        suite = suite->next;
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    test_log_info("JSON report generated: %s", output_file);
}

// Additional helper implementations
pid_t read_pid_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    pid_t pid;
    if (fscanf(fp, "%d", &pid) != 1) {
        pid = -1;
    }
    fclose(fp);
    return pid;
}