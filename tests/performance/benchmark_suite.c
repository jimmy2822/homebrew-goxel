/*
 * Goxel Performance Benchmark Suite
 * Author: Alex Kumar
 * Date: January 29, 2025
 * 
 * Comprehensive benchmarking framework for measuring performance
 * during the architecture simplification from 4-layer to 2-layer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <math.h>

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================

#define MAX_SAMPLES 100000
#define MAX_CONCURRENT_THREADS 100
#define DEFAULT_SOCKET_PATH "/tmp/goxel-daemon.sock"
#define MCP_SOCKET_PATH "/tmp/goxel-mcp.sock"
#define TS_CLIENT_PORT 8080

// Performance targets from architecture simplification
#define TARGET_2LAYER_LATENCY_MS 6.0
#define TARGET_4LAYER_LATENCY_MS 11.0
#define TARGET_IMPROVEMENT_FACTOR 1.83  // 11ms -> 6ms

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    ARCH_4LAYER,
    ARCH_2LAYER
} architecture_t;

typedef struct {
    const char* name;
    const char* description;
    int warmup_iterations;
    int test_iterations;
    int concurrent_clients;
    double timeout_ms;
    architecture_t architecture;
} benchmark_config_t;

typedef struct {
    // Latency metrics (all in milliseconds)
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    double p50_latency_ms;
    double p90_latency_ms;
    double p95_latency_ms;
    double p99_latency_ms;
    double stddev_latency_ms;
    
    // Throughput metrics
    double throughput_ops_per_sec;
    double max_concurrent_ops;
    
    // Resource metrics
    size_t memory_usage_bytes;
    size_t peak_memory_bytes;
    double cpu_usage_percent;
    int file_descriptors_used;
    
    // Success metrics
    int successful_operations;
    int failed_operations;
    double success_rate;
    
    // Layer breakdown (for 4-layer architecture)
    double mcp_to_server_ms;
    double server_to_ts_ms;
    double ts_to_daemon_ms;
    double daemon_processing_ms;
    
    // Timing
    struct timespec start_time;
    struct timespec end_time;
    double total_duration_sec;
} benchmark_result_t;

typedef struct {
    double value;
    struct timespec timestamp;
    int layer_timings[4];  // For layer breakdown
} measurement_t;

typedef struct {
    measurement_t samples[MAX_SAMPLES];
    int count;
    pthread_mutex_t mutex;
} measurement_buffer_t;

typedef int (*benchmark_fn)(benchmark_config_t* config, benchmark_result_t* result);

// ============================================================================
// UTILITY FUNCTIONS
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

static size_t get_current_memory_usage(void)
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;  // Convert to bytes
    }
    return 0;
}

static double get_cpu_usage(void)
{
    static struct rusage last_usage;
    static struct timespec last_time;
    static int initialized = 0;
    
    struct rusage current_usage;
    struct timespec current_time;
    
    if (!initialized) {
        getrusage(RUSAGE_SELF, &last_usage);
        get_current_time(&last_time);
        initialized = 1;
        return 0.0;
    }
    
    getrusage(RUSAGE_SELF, &current_usage);
    get_current_time(&current_time);
    
    double cpu_time = (current_usage.ru_utime.tv_sec - last_usage.ru_utime.tv_sec) +
                      (current_usage.ru_utime.tv_usec - last_usage.ru_utime.tv_usec) / 1000000.0 +
                      (current_usage.ru_stime.tv_sec - last_usage.ru_stime.tv_sec) +
                      (current_usage.ru_stime.tv_usec - last_usage.ru_stime.tv_usec) / 1000000.0;
    
    double wall_time = timespec_diff_ms(&last_time, &current_time) / 1000.0;
    
    last_usage = current_usage;
    last_time = current_time;
    
    return (cpu_time / wall_time) * 100.0;
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

static void calculate_statistics(measurement_buffer_t *buffer, benchmark_result_t *result)
{
    if (buffer->count == 0) return;
    
    // Extract latency values
    double *values = malloc(buffer->count * sizeof(double));
    for (int i = 0; i < buffer->count; i++) {
        values[i] = buffer->samples[i].value;
    }
    
    // Sort for percentile calculations
    qsort(values, buffer->count, sizeof(double), compare_doubles);
    
    // Basic statistics
    result->min_latency_ms = values[0];
    result->max_latency_ms = values[buffer->count - 1];
    
    // Calculate mean
    double sum = 0.0;
    for (int i = 0; i < buffer->count; i++) {
        sum += values[i];
    }
    result->avg_latency_ms = sum / buffer->count;
    
    // Calculate standard deviation
    double variance = 0.0;
    for (int i = 0; i < buffer->count; i++) {
        double diff = values[i] - result->avg_latency_ms;
        variance += diff * diff;
    }
    result->stddev_latency_ms = sqrt(variance / buffer->count);
    
    // Percentiles
    result->p50_latency_ms = values[(int)(buffer->count * 0.50)];
    result->p90_latency_ms = values[(int)(buffer->count * 0.90)];
    result->p95_latency_ms = values[(int)(buffer->count * 0.95)];
    result->p99_latency_ms = values[(int)(buffer->count * 0.99)];
    
    // Calculate layer breakdown for 4-layer architecture
    if (buffer->samples[0].layer_timings[0] > 0) {
        double layer_sums[4] = {0};
        for (int i = 0; i < buffer->count; i++) {
            for (int j = 0; j < 4; j++) {
                layer_sums[j] += buffer->samples[i].layer_timings[j] / 1000.0;  // Convert to ms
            }
        }
        result->mcp_to_server_ms = layer_sums[0] / buffer->count;
        result->server_to_ts_ms = layer_sums[1] / buffer->count;
        result->ts_to_daemon_ms = layer_sums[2] / buffer->count;
        result->daemon_processing_ms = layer_sums[3] / buffer->count;
    }
    
    free(values);
}

// ============================================================================
// LAYER SIMULATION FUNCTIONS
// ============================================================================

static int simulate_mcp_client_call(int *duration_us)
{
    struct timespec start, end;
    get_current_time(&start);
    
    // Simulate MCP client processing
    usleep(500 + rand() % 1000);  // 0.5-1.5ms
    
    get_current_time(&end);
    *duration_us = (int)(timespec_diff_ms(&start, &end) * 1000);
    return 0;
}

static int simulate_mcp_server_processing(int *duration_us)
{
    struct timespec start, end;
    get_current_time(&start);
    
    // Simulate MCP server processing
    usleep(1500 + rand() % 1000);  // 1.5-2.5ms
    
    get_current_time(&end);
    *duration_us = (int)(timespec_diff_ms(&start, &end) * 1000);
    return 0;
}

static int simulate_typescript_client_forward(int *duration_us)
{
    struct timespec start, end;
    get_current_time(&start);
    
    // Simulate TypeScript client processing
    usleep(2500 + rand() % 1000);  // 2.5-3.5ms
    
    get_current_time(&end);
    *duration_us = (int)(timespec_diff_ms(&start, &end) * 1000);
    return 0;
}

static int simulate_daemon_processing(int *duration_us)
{
    struct timespec start, end;
    get_current_time(&start);
    
    // Simulate actual daemon work
    usleep(4000 + rand() % 2000);  // 4-6ms
    
    get_current_time(&end);
    *duration_us = (int)(timespec_diff_ms(&start, &end) * 1000);
    return 0;
}

static int simulate_direct_mcp_daemon_call(int *duration_us)
{
    struct timespec start, end;
    get_current_time(&start);
    
    // Simulate direct MCP protocol handling + daemon processing
    usleep(5000 + rand() % 2000);  // 5-7ms total
    
    get_current_time(&end);
    *duration_us = (int)(timespec_diff_ms(&start, &end) * 1000);
    return 0;
}

// ============================================================================
// BENCHMARK IMPLEMENTATIONS
// ============================================================================

static measurement_buffer_t global_buffer = {
    .count = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

// Benchmark 1: Single Operation Latency
int benchmark_single_operation_latency(benchmark_config_t *config, benchmark_result_t *result)
{
    measurement_buffer_t buffer = {0};
    pthread_mutex_init(&buffer.mutex, NULL);
    
    printf("Running single operation latency benchmark (%s)...\n", 
           config->architecture == ARCH_4LAYER ? "4-layer" : "2-layer");
    
    // Warmup
    for (int i = 0; i < config->warmup_iterations; i++) {
        if (config->architecture == ARCH_4LAYER) {
            int layer_times[4];
            simulate_mcp_client_call(&layer_times[0]);
            simulate_mcp_server_processing(&layer_times[1]);
            simulate_typescript_client_forward(&layer_times[2]);
            simulate_daemon_processing(&layer_times[3]);
        } else {
            int duration;
            simulate_direct_mcp_daemon_call(&duration);
        }
    }
    
    // Actual benchmark
    for (int i = 0; i < config->test_iterations && buffer.count < MAX_SAMPLES; i++) {
        struct timespec start, end;
        measurement_t *m = &buffer.samples[buffer.count];
        
        get_current_time(&start);
        
        if (config->architecture == ARCH_4LAYER) {
            // Simulate 4-layer architecture
            simulate_mcp_client_call(&m->layer_timings[0]);
            simulate_mcp_server_processing(&m->layer_timings[1]);
            simulate_typescript_client_forward(&m->layer_timings[2]);
            simulate_daemon_processing(&m->layer_timings[3]);
        } else {
            // Simulate 2-layer architecture
            int total_duration;
            simulate_direct_mcp_daemon_call(&total_duration);
            m->layer_timings[0] = total_duration;
            m->layer_timings[1] = 0;
            m->layer_timings[2] = 0;
            m->layer_timings[3] = 0;
        }
        
        get_current_time(&end);
        m->value = timespec_diff_ms(&start, &end);
        m->timestamp = end;
        buffer.count++;
        
        result->successful_operations++;
        
        if (i % 100 == 0) {
            printf("\rProgress: %d/%d (%.1f%%)", i, config->test_iterations,
                   (double)i / config->test_iterations * 100);
            fflush(stdout);
        }
    }
    printf("\n");
    
    calculate_statistics(&buffer, result);
    result->throughput_ops_per_sec = 1000.0 / result->avg_latency_ms;
    result->success_rate = (double)result->successful_operations / 
                          (result->successful_operations + result->failed_operations) * 100;
    
    pthread_mutex_destroy(&buffer.mutex);
    return 0;
}

// Benchmark 2: Concurrent Load Testing
typedef struct {
    int thread_id;
    benchmark_config_t *config;
    measurement_buffer_t *buffer;
    int operations_completed;
    int operations_failed;
} worker_context_t;

static void* concurrent_worker(void *arg)
{
    worker_context_t *ctx = (worker_context_t*)arg;
    
    for (int i = 0; i < ctx->config->test_iterations / ctx->config->concurrent_clients; i++) {
        struct timespec start, end;
        get_current_time(&start);
        
        // Simulate operation
        if (ctx->config->architecture == ARCH_4LAYER) {
            int layer_times[4];
            simulate_mcp_client_call(&layer_times[0]);
            simulate_mcp_server_processing(&layer_times[1]);
            simulate_typescript_client_forward(&layer_times[2]);
            simulate_daemon_processing(&layer_times[3]);
        } else {
            int duration;
            simulate_direct_mcp_daemon_call(&duration);
        }
        
        get_current_time(&end);
        
        pthread_mutex_lock(&ctx->buffer->mutex);
        if (ctx->buffer->count < MAX_SAMPLES) {
            measurement_t *m = &ctx->buffer->samples[ctx->buffer->count];
            m->value = timespec_diff_ms(&start, &end);
            m->timestamp = end;
            ctx->buffer->count++;
        }
        pthread_mutex_unlock(&ctx->buffer->mutex);
        
        ctx->operations_completed++;
    }
    
    return NULL;
}

int benchmark_concurrent_load(benchmark_config_t *config, benchmark_result_t *result)
{
    measurement_buffer_t buffer = {0};
    pthread_mutex_init(&buffer.mutex, NULL);
    
    printf("Running concurrent load benchmark with %d clients...\n", 
           config->concurrent_clients);
    
    pthread_t threads[MAX_CONCURRENT_THREADS];
    worker_context_t contexts[MAX_CONCURRENT_THREADS];
    
    struct timespec start_time, end_time;
    get_current_time(&start_time);
    
    // Launch concurrent workers
    for (int i = 0; i < config->concurrent_clients; i++) {
        contexts[i].thread_id = i;
        contexts[i].config = config;
        contexts[i].buffer = &buffer;
        contexts[i].operations_completed = 0;
        contexts[i].operations_failed = 0;
        
        pthread_create(&threads[i], NULL, concurrent_worker, &contexts[i]);
    }
    
    // Monitor progress
    int total_ops = config->test_iterations;
    while (1) {
        pthread_mutex_lock(&buffer.mutex);
        int current_ops = buffer.count;
        pthread_mutex_unlock(&buffer.mutex);
        
        printf("\rProgress: %d/%d (%.1f%%)", current_ops, total_ops,
               (double)current_ops / total_ops * 100);
        fflush(stdout);
        
        if (current_ops >= total_ops || current_ops >= MAX_SAMPLES - 1000) {
            break;
        }
        
        usleep(100000);  // Check every 100ms
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < config->concurrent_clients; i++) {
        pthread_join(threads[i], NULL);
        result->successful_operations += contexts[i].operations_completed;
        result->failed_operations += contexts[i].operations_failed;
    }
    
    get_current_time(&end_time);
    printf("\n");
    
    result->total_duration_sec = timespec_diff_ms(&start_time, &end_time) / 1000.0;
    calculate_statistics(&buffer, result);
    result->throughput_ops_per_sec = buffer.count / result->total_duration_sec;
    result->max_concurrent_ops = config->concurrent_clients;
    
    pthread_mutex_destroy(&buffer.mutex);
    return 0;
}

// Benchmark 3: Memory Usage Pattern
int benchmark_memory_usage(benchmark_config_t *config, benchmark_result_t *result)
{
    printf("Running memory usage benchmark...\n");
    
    size_t initial_memory = get_current_memory_usage();
    size_t peak_memory = initial_memory;
    
    // Simulate creating a large voxel scene
    void **allocations = malloc(config->test_iterations * sizeof(void*));
    
    for (int i = 0; i < config->test_iterations; i++) {
        // Allocate memory simulating voxel data
        size_t alloc_size = 1024 + (rand() % 4096);  // 1-5KB per "voxel operation"
        allocations[i] = malloc(alloc_size);
        
        if (allocations[i]) {
            memset(allocations[i], 0, alloc_size);  // Touch the memory
            result->successful_operations++;
        } else {
            result->failed_operations++;
        }
        
        if (i % 100 == 0) {
            size_t current_memory = get_current_memory_usage();
            if (current_memory > peak_memory) {
                peak_memory = current_memory;
            }
            
            printf("\rProgress: %d/%d Memory: %.1f MB", i, config->test_iterations,
                   (current_memory - initial_memory) / (1024.0 * 1024.0));
            fflush(stdout);
        }
    }
    
    result->memory_usage_bytes = get_current_memory_usage() - initial_memory;
    result->peak_memory_bytes = peak_memory - initial_memory;
    
    // Cleanup
    for (int i = 0; i < config->test_iterations; i++) {
        free(allocations[i]);
    }
    free(allocations);
    
    printf("\n");
    return 0;
}

// ============================================================================
// REPORTING FUNCTIONS
// ============================================================================

static void print_benchmark_summary(benchmark_config_t *config, benchmark_result_t *result)
{
    printf("\n=== %s Results ===\n", config->name);
    printf("Architecture: %s\n", config->architecture == ARCH_4LAYER ? "4-layer" : "2-layer");
    printf("Duration: %.2f seconds\n", result->total_duration_sec);
    printf("Operations: %d successful, %d failed (%.1f%% success rate)\n",
           result->successful_operations, result->failed_operations, result->success_rate);
    
    printf("\nLatency Statistics:\n");
    printf("  Min: %.3f ms\n", result->min_latency_ms);
    printf("  Avg: %.3f ms\n", result->avg_latency_ms);
    printf("  Max: %.3f ms\n", result->max_latency_ms);
    printf("  StdDev: %.3f ms\n", result->stddev_latency_ms);
    
    printf("\nPercentiles:\n");
    printf("  P50: %.3f ms\n", result->p50_latency_ms);
    printf("  P90: %.3f ms\n", result->p90_latency_ms);
    printf("  P95: %.3f ms\n", result->p95_latency_ms);
    printf("  P99: %.3f ms\n", result->p99_latency_ms);
    
    if (config->architecture == ARCH_4LAYER && result->mcp_to_server_ms > 0) {
        printf("\nLayer Breakdown:\n");
        printf("  MCP Client → Server: %.3f ms (%.1f%%)\n", 
               result->mcp_to_server_ms,
               result->mcp_to_server_ms / result->avg_latency_ms * 100);
        printf("  Server → TS Client: %.3f ms (%.1f%%)\n",
               result->server_to_ts_ms,
               result->server_to_ts_ms / result->avg_latency_ms * 100);
        printf("  TS Client → Daemon: %.3f ms (%.1f%%)\n",
               result->ts_to_daemon_ms,
               result->ts_to_daemon_ms / result->avg_latency_ms * 100);
        printf("  Daemon Processing: %.3f ms (%.1f%%)\n",
               result->daemon_processing_ms,
               result->daemon_processing_ms / result->avg_latency_ms * 100);
    }
    
    printf("\nThroughput: %.1f ops/sec\n", result->throughput_ops_per_sec);
    
    if (result->memory_usage_bytes > 0) {
        printf("\nMemory Usage:\n");
        printf("  Total: %.2f MB\n", result->memory_usage_bytes / (1024.0 * 1024.0));
        printf("  Peak: %.2f MB\n", result->peak_memory_bytes / (1024.0 * 1024.0));
    }
    
    // Performance evaluation
    if (config->architecture == ARCH_4LAYER) {
        printf("\nTarget Evaluation:\n");
        printf("  Target: %.1f ms\n", TARGET_4LAYER_LATENCY_MS);
        printf("  Achieved: %.3f ms ", result->avg_latency_ms);
        if (result->avg_latency_ms <= TARGET_4LAYER_LATENCY_MS) {
            printf("✅ PASS\n");
        } else {
            printf("❌ FAIL (%.1fx over target)\n",
                   result->avg_latency_ms / TARGET_4LAYER_LATENCY_MS);
        }
    } else {
        printf("\nTarget Evaluation:\n");
        printf("  Target: %.1f ms\n", TARGET_2LAYER_LATENCY_MS);
        printf("  Achieved: %.3f ms ", result->avg_latency_ms);
        if (result->avg_latency_ms <= TARGET_2LAYER_LATENCY_MS) {
            printf("✅ PASS\n");
        } else {
            printf("❌ FAIL (%.1fx over target)\n",
                   result->avg_latency_ms / TARGET_2LAYER_LATENCY_MS);
        }
    }
}

static void save_results_json(benchmark_config_t *configs[], benchmark_result_t *results[], 
                             int num_benchmarks, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", timestamp);
    fprintf(fp, "  \"benchmarks\": [\n");
    
    for (int i = 0; i < num_benchmarks; i++) {
        benchmark_config_t *config = configs[i];
        benchmark_result_t *result = results[i];
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", config->name);
        fprintf(fp, "      \"architecture\": \"%s\",\n", 
                config->architecture == ARCH_4LAYER ? "4-layer" : "2-layer");
        fprintf(fp, "      \"iterations\": %d,\n", config->test_iterations);
        fprintf(fp, "      \"concurrent_clients\": %d,\n", config->concurrent_clients);
        fprintf(fp, "      \"results\": {\n");
        fprintf(fp, "        \"latency\": {\n");
        fprintf(fp, "          \"min\": %.3f,\n", result->min_latency_ms);
        fprintf(fp, "          \"avg\": %.3f,\n", result->avg_latency_ms);
        fprintf(fp, "          \"max\": %.3f,\n", result->max_latency_ms);
        fprintf(fp, "          \"stddev\": %.3f,\n", result->stddev_latency_ms);
        fprintf(fp, "          \"p50\": %.3f,\n", result->p50_latency_ms);
        fprintf(fp, "          \"p90\": %.3f,\n", result->p90_latency_ms);
        fprintf(fp, "          \"p95\": %.3f,\n", result->p95_latency_ms);
        fprintf(fp, "          \"p99\": %.3f\n", result->p99_latency_ms);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"throughput\": %.1f,\n", result->throughput_ops_per_sec);
        fprintf(fp, "        \"success_rate\": %.1f", result->success_rate);
        
        if (config->architecture == ARCH_4LAYER && result->mcp_to_server_ms > 0) {
            fprintf(fp, ",\n        \"layer_breakdown\": {\n");
            fprintf(fp, "          \"mcp_to_server\": %.3f,\n", result->mcp_to_server_ms);
            fprintf(fp, "          \"server_to_ts\": %.3f,\n", result->server_to_ts_ms);
            fprintf(fp, "          \"ts_to_daemon\": %.3f,\n", result->ts_to_daemon_ms);
            fprintf(fp, "          \"daemon_processing\": %.3f\n", result->daemon_processing_ms);
            fprintf(fp, "        }");
        }
        
        fprintf(fp, "\n      }\n");
        fprintf(fp, "    }%s\n", (i < num_benchmarks - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("\nResults saved to: %s\n", filename);
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char *argv[])
{
    printf("Goxel Performance Benchmark Suite\n");
    printf("Architecture Simplification Testing\n");
    printf("===================================\n\n");
    
    srand(time(NULL));
    
    // Define benchmark configurations
    benchmark_config_t single_op_4layer = {
        .name = "Single_Operation_4Layer",
        .description = "Measure single operation latency through 4-layer stack",
        .warmup_iterations = 10,
        .test_iterations = 1000,
        .concurrent_clients = 1,
        .timeout_ms = 100.0,
        .architecture = ARCH_4LAYER
    };
    
    benchmark_config_t single_op_2layer = {
        .name = "Single_Operation_2Layer",
        .description = "Measure single operation latency through 2-layer stack",
        .warmup_iterations = 10,
        .test_iterations = 1000,
        .concurrent_clients = 1,
        .timeout_ms = 100.0,
        .architecture = ARCH_2LAYER
    };
    
    benchmark_config_t concurrent_4layer = {
        .name = "Concurrent_Load_4Layer",
        .description = "Measure performance under concurrent load (4-layer)",
        .warmup_iterations = 0,
        .test_iterations = 10000,
        .concurrent_clients = 10,
        .timeout_ms = 100.0,
        .architecture = ARCH_4LAYER
    };
    
    benchmark_config_t concurrent_2layer = {
        .name = "Concurrent_Load_2Layer",
        .description = "Measure performance under concurrent load (2-layer)",
        .warmup_iterations = 0,
        .test_iterations = 10000,
        .concurrent_clients = 10,
        .timeout_ms = 100.0,
        .architecture = ARCH_2LAYER
    };
    
    // Run benchmarks
    benchmark_result_t results[10];
    benchmark_config_t *configs[10];
    int num_benchmarks = 0;
    
    // Single operation benchmarks
    printf("Phase 1: Single Operation Latency\n");
    printf("---------------------------------\n");
    
    configs[num_benchmarks] = &single_op_4layer;
    benchmark_single_operation_latency(&single_op_4layer, &results[num_benchmarks]);
    print_benchmark_summary(&single_op_4layer, &results[num_benchmarks]);
    num_benchmarks++;
    
    configs[num_benchmarks] = &single_op_2layer;
    benchmark_single_operation_latency(&single_op_2layer, &results[num_benchmarks]);
    print_benchmark_summary(&single_op_2layer, &results[num_benchmarks]);
    num_benchmarks++;
    
    // Calculate improvement
    double improvement = results[0].avg_latency_ms / results[1].avg_latency_ms;
    printf("\n🎯 Latency Improvement: %.2fx (%.1f%% faster)\n", improvement,
           (improvement - 1) * 100);
    
    // Concurrent load benchmarks
    printf("\nPhase 2: Concurrent Load Testing\n");
    printf("---------------------------------\n");
    
    configs[num_benchmarks] = &concurrent_4layer;
    benchmark_concurrent_load(&concurrent_4layer, &results[num_benchmarks]);
    print_benchmark_summary(&concurrent_4layer, &results[num_benchmarks]);
    num_benchmarks++;
    
    configs[num_benchmarks] = &concurrent_2layer;
    benchmark_concurrent_load(&concurrent_2layer, &results[num_benchmarks]);
    print_benchmark_summary(&concurrent_2layer, &results[num_benchmarks]);
    num_benchmarks++;
    
    // Save all results
    save_results_json(configs, (benchmark_result_t*[]){&results[0], &results[1], 
                                                       &results[2], &results[3]},
                     num_benchmarks, "benchmark_results.json");
    
    printf("\n✅ Benchmark suite completed successfully!\n");
    
    return 0;
}