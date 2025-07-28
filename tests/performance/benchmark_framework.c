/*
 * Goxel v14.0 Performance Validation Framework
 * 
 * This framework provides the infrastructure to validate performance claims
 * once RPC methods are implemented. It establishes baseline measurements
 * and comparison methodologies.
 * 
 * Design principles:
 * - Works even when RPC methods return errors (measures overhead)
 * - Provides CLI baseline measurements for comparison
 * - Tracks all key performance metrics
 * - Generates detailed reports for validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <errno.h>
#include <math.h>

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

// ============================================================================
// CONFIGURATION AND CONSTANTS
// ============================================================================

#define MAX_SAMPLES 10000
#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define CLI_BINARY "../../goxel-headless"

// Performance targets from v14 specifications
#define TARGET_LATENCY_MS 2.1
#define TARGET_THROUGHPUT_OPS 1000
#define TARGET_MEMORY_MB 50
#define TARGET_IMPROVEMENT_FACTOR 7.0

// Test configuration
#define DEFAULT_WARMUP_ITERATIONS 10
#define DEFAULT_TEST_ITERATIONS 100
#define PROGRESS_UPDATE_INTERVAL 10

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    METRIC_LATENCY,
    METRIC_THROUGHPUT,
    METRIC_MEMORY,
    METRIC_CPU,
    METRIC_STARTUP_TIME
} metric_type_t;

typedef struct {
    double value;
    struct timespec timestamp;
    int success;
    char error_msg[256];
} measurement_t;

typedef struct {
    measurement_t samples[MAX_SAMPLES];
    int count;
    char test_name[128];
    metric_type_t metric_type;
    
    // Statistical results
    double min;
    double max;
    double mean;
    double median;
    double stddev;
    double p50;
    double p90;
    double p95;
    double p99;
    
    // Additional metadata
    int successes;
    int failures;
    double success_rate;
    struct timespec start_time;
    struct timespec end_time;
    double total_duration_sec;
} benchmark_result_t;

typedef struct {
    const char *name;
    const char *description;
    int (*setup_fn)(void *context);
    int (*benchmark_fn)(void *context, measurement_t *measurement);
    int (*teardown_fn)(void *context);
    void *context;
    int iterations;
    int warmup_iterations;
} benchmark_test_t;

// ============================================================================
// TIMING AND MEASUREMENT UTILITIES
// ============================================================================

static double timespec_diff_ms(struct timespec *start, struct timespec *end)
{
    double diff = (end->tv_sec - start->tv_sec) * 1000.0;
    diff += (end->tv_nsec - start->tv_nsec) / 1000000.0;
    return diff;
}

static void get_current_time(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC_RAW, ts);
}

static double get_time_ms(void)
{
    struct timespec ts;
    get_current_time(&ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// ============================================================================
// STATISTICS CALCULATIONS
// ============================================================================

static int compare_doubles(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static void calculate_statistics(benchmark_result_t *result)
{
    if (result->count == 0) return;
    
    // Extract successful measurements
    double values[MAX_SAMPLES];
    int valid_count = 0;
    
    for (int i = 0; i < result->count; i++) {
        if (result->samples[i].success) {
            values[valid_count++] = result->samples[i].value;
        }
    }
    
    if (valid_count == 0) {
        result->success_rate = 0.0;
        return;
    }
    
    // Sort for percentile calculations
    qsort(values, valid_count, sizeof(double), compare_doubles);
    
    // Basic statistics
    result->min = values[0];
    result->max = values[valid_count - 1];
    result->median = values[valid_count / 2];
    result->successes = valid_count;
    result->failures = result->count - valid_count;
    result->success_rate = (double)valid_count / result->count * 100.0;
    
    // Mean
    double sum = 0.0;
    for (int i = 0; i < valid_count; i++) {
        sum += values[i];
    }
    result->mean = sum / valid_count;
    
    // Standard deviation
    double variance = 0.0;
    for (int i = 0; i < valid_count; i++) {
        double diff = values[i] - result->mean;
        variance += diff * diff;
    }
    result->stddev = sqrt(variance / valid_count);
    
    // Percentiles
    result->p50 = values[(int)(valid_count * 0.50)];
    result->p90 = values[(int)(valid_count * 0.90)];
    result->p95 = values[(int)(valid_count * 0.95)];
    result->p99 = values[(int)(valid_count * 0.99)];
}

// ============================================================================
// RESOURCE MONITORING
// ============================================================================

typedef struct {
    double cpu_user_sec;
    double cpu_system_sec;
    double memory_rss_mb;
    double memory_vms_mb;
} resource_usage_t;

static void get_resource_usage(resource_usage_t *usage)
{
    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == 0) {
        usage->cpu_user_sec = rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec / 1000000.0;
        usage->cpu_system_sec = rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec / 1000000.0;
        usage->memory_rss_mb = rusage.ru_maxrss / 1024.0; // Convert KB to MB
    } else {
        memset(usage, 0, sizeof(resource_usage_t));
    }
    
    // For more detailed memory info, would need to parse /proc/self/status
    // This is a simplified version using rusage
    usage->memory_vms_mb = usage->memory_rss_mb * 1.2; // Approximate
}

// ============================================================================
// BENCHMARK EXECUTION ENGINE
// ============================================================================

static void print_progress(const char *test_name, int current, int total)
{
    if (current % PROGRESS_UPDATE_INTERVAL == 0 || current == total) {
        printf("\r  %s: %d/%d (%.1f%%)", test_name, current, total, 
               (double)current / total * 100.0);
        fflush(stdout);
    }
}

static int run_benchmark(benchmark_test_t *test, benchmark_result_t *result)
{
    printf("Running benchmark: %s\n", test->name);
    printf("  Description: %s\n", test->description);
    
    // Initialize result structure
    memset(result, 0, sizeof(benchmark_result_t));
    strncpy(result->test_name, test->name, sizeof(result->test_name) - 1);
    get_current_time(&result->start_time);
    
    // Setup phase
    if (test->setup_fn) {
        printf("  Setting up...\n");
        if (test->setup_fn(test->context) != 0) {
            fprintf(stderr, "  Setup failed!\n");
            return -1;
        }
    }
    
    // Warmup phase
    if (test->warmup_iterations > 0) {
        printf("  Warming up (%d iterations)...\n", test->warmup_iterations);
        for (int i = 0; i < test->warmup_iterations; i++) {
            measurement_t warmup_measurement;
            test->benchmark_fn(test->context, &warmup_measurement);
            print_progress("Warmup", i + 1, test->warmup_iterations);
        }
        printf("\n");
    }
    
    // Benchmark phase
    printf("  Running benchmark (%d iterations)...\n", test->iterations);
    for (int i = 0; i < test->iterations && i < MAX_SAMPLES; i++) {
        measurement_t *measurement = &result->samples[result->count];
        
        int ret = test->benchmark_fn(test->context, measurement);
        if (ret == 0) {
            measurement->success = 1;
        } else {
            measurement->success = 0;
            if (strlen(measurement->error_msg) == 0) {
                snprintf(measurement->error_msg, sizeof(measurement->error_msg),
                        "Benchmark function returned %d", ret);
            }
        }
        
        result->count++;
        print_progress("Benchmark", i + 1, test->iterations);
    }
    printf("\n");
    
    // Teardown phase
    if (test->teardown_fn) {
        printf("  Cleaning up...\n");
        test->teardown_fn(test->context);
    }
    
    // Calculate statistics
    get_current_time(&result->end_time);
    result->total_duration_sec = timespec_diff_ms(&result->start_time, &result->end_time) / 1000.0;
    calculate_statistics(result);
    
    return 0;
}

// ============================================================================
// REPORT GENERATION
// ============================================================================

static void print_benchmark_summary(benchmark_result_t *result)
{
    printf("\n=== %s Results ===\n", result->test_name);
    printf("Duration: %.2f seconds\n", result->total_duration_sec);
    printf("Samples: %d (Success: %d, Failed: %d)\n", 
           result->count, result->successes, result->failures);
    printf("Success Rate: %.1f%%\n", result->success_rate);
    
    if (result->successes > 0) {
        printf("\nStatistics:\n");
        printf("  Min: %.3f ms\n", result->min);
        printf("  Max: %.3f ms\n", result->max);
        printf("  Mean: %.3f ms\n", result->mean);
        printf("  Median: %.3f ms\n", result->median);
        printf("  StdDev: %.3f ms\n", result->stddev);
        printf("\nPercentiles:\n");
        printf("  P50: %.3f ms\n", result->p50);
        printf("  P90: %.3f ms\n", result->p90);
        printf("  P95: %.3f ms\n", result->p95);
        printf("  P99: %.3f ms\n", result->p99);
        
        // Performance target evaluation
        if (result->metric_type == METRIC_LATENCY) {
            printf("\nTarget Evaluation:\n");
            printf("  Target: <%.1f ms\n", TARGET_LATENCY_MS);
            printf("  Achieved: %.3f ms ", result->mean);
            if (result->mean <= TARGET_LATENCY_MS) {
                printf("✅ PASS\n");
            } else {
                printf("❌ FAIL (%.1fx over target)\n", 
                       result->mean / TARGET_LATENCY_MS);
            }
        }
    }
    
    printf("\n");
}

static void save_results_json(benchmark_result_t *results, int num_results, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": %.0f,\n", (double)time(NULL));
    fprintf(fp, "  \"results\": [\n");
    
    for (int i = 0; i < num_results; i++) {
        benchmark_result_t *r = &results[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"test_name\": \"%s\",\n", r->test_name);
        fprintf(fp, "      \"samples\": %d,\n", r->count);
        fprintf(fp, "      \"success_rate\": %.2f,\n", r->success_rate);
        fprintf(fp, "      \"duration_sec\": %.2f,\n", r->total_duration_sec);
        
        if (r->successes > 0) {
            fprintf(fp, "      \"statistics\": {\n");
            fprintf(fp, "        \"min\": %.3f,\n", r->min);
            fprintf(fp, "        \"max\": %.3f,\n", r->max);
            fprintf(fp, "        \"mean\": %.3f,\n", r->mean);
            fprintf(fp, "        \"median\": %.3f,\n", r->median);
            fprintf(fp, "        \"stddev\": %.3f,\n", r->stddev);
            fprintf(fp, "        \"p50\": %.3f,\n", r->p50);
            fprintf(fp, "        \"p90\": %.3f,\n", r->p90);
            fprintf(fp, "        \"p95\": %.3f,\n", r->p95);
            fprintf(fp, "        \"p99\": %.3f\n", r->p99);
            fprintf(fp, "      }\n");
        }
        
        fprintf(fp, "    }%s\n", (i < num_results - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("Results saved to: %s\n", filename);
}

// ============================================================================
// EXAMPLE BENCHMARK: SOCKET CONNECTION OVERHEAD
// ============================================================================

typedef struct {
    const char *socket_path;
    int sockfd;
} socket_benchmark_context_t;

static int socket_connect_benchmark(void *context, measurement_t *measurement)
{
    socket_benchmark_context_t *ctx = (socket_benchmark_context_t *)context;
    struct sockaddr_un addr;
    struct timespec start, end;
    
    // Create socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        snprintf(measurement->error_msg, sizeof(measurement->error_msg),
                "socket() failed: %s", strerror(errno));
        return -1;
    }
    
    // Prepare address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctx->socket_path, sizeof(addr.sun_path) - 1);
    
    // Measure connection time
    get_current_time(&start);
    int ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    get_current_time(&end);
    
    if (ret == -1) {
        snprintf(measurement->error_msg, sizeof(measurement->error_msg),
                "connect() failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    // Record measurement
    measurement->value = timespec_diff_ms(&start, &end);
    get_current_time(&measurement->timestamp);
    
    close(sockfd);
    return 0;
}

// ============================================================================
// MAIN FUNCTION (EXAMPLE USAGE)
// ============================================================================

int main(int argc, char *argv[])
{
    printf("Goxel v14.0 Performance Validation Framework\n");
    printf("============================================\n\n");
    
    // Example: Socket connection overhead benchmark
    socket_benchmark_context_t socket_ctx = {
        .socket_path = SOCKET_PATH,
        .sockfd = -1
    };
    
    benchmark_test_t socket_test = {
        .name = "Socket_Connection_Overhead",
        .description = "Measures Unix domain socket connection latency",
        .setup_fn = NULL,
        .benchmark_fn = socket_connect_benchmark,
        .teardown_fn = NULL,
        .context = &socket_ctx,
        .iterations = 100,
        .warmup_iterations = 10
    };
    
    benchmark_result_t results[10];
    int num_results = 0;
    
    // Run the benchmark
    if (run_benchmark(&socket_test, &results[num_results]) == 0) {
        print_benchmark_summary(&results[num_results]);
        num_results++;
    }
    
    // Save results
    if (num_results > 0) {
        save_results_json(results, num_results, "benchmark_results.json");
    }
    
    printf("\nBenchmark framework validation complete.\n");
    printf("Note: This framework will be used to validate v14.0 performance\n");
    printf("      once JSON-RPC methods are implemented.\n");
    
    return 0;
}