/*
 * Goxel v14.0 MCP Stress Test
 * 
 * Multi-threaded stress testing for Sarah's MCP handler implementation.
 * Tests concurrent access, memory pressure, and sustained load.
 * 
 * Author: Alex Kumar - Testing & Performance Validation Expert
 * Week 2, Day 2 (February 4, 2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <atomic>
#include <sys/resource.h>
#include <errno.h>

#include "../src/daemon/mcp_handler.h"

// ============================================================================
// STRESS TEST CONFIGURATION
// ============================================================================

#define MAX_THREADS 32
#define STRESS_DURATION_SEC 60
#define REQUESTS_PER_THREAD 10000
#define MEMORY_PRESSURE_MB 100

// Thread-safe counters
static _Atomic size_t total_requests = 0;
static _Atomic size_t successful_requests = 0;
static _Atomic size_t failed_requests = 0;
static _Atomic size_t total_time_us = 0;

// Test control
static volatile bool stop_test = false;

// ============================================================================
// THREAD DATA AND FUNCTIONS
// ============================================================================

typedef struct {
    int thread_id;
    int num_requests;
    pthread_t pthread;
    bool finished;
    double avg_latency_us;
    size_t requests_completed;
    size_t requests_failed;
} stress_thread_data_t;

/**
 * Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * Create a test MCP request
 */
static mcp_tool_request_t *create_stress_request(int variant)
{
    const char *tools[] = {"ping", "version", "list_methods", "goxel_create_project"};
    const char *args[] = {NULL, NULL, NULL, "{\"name\": \"stress_test\"}"};
    
    int tool_idx = variant % 4;
    
    mcp_tool_request_t *request = calloc(1, sizeof(mcp_tool_request_t));
    if (!request) return NULL;
    
    request->tool = strdup(tools[tool_idx]);
    if (!request->tool) {
        free(request);
        return NULL;
    }
    
    if (args[tool_idx]) {
        request->arguments = json_parse(args[tool_idx], strlen(args[tool_idx]));
        if (!request->arguments) {
            free(request->tool);
            free(request);
            return NULL;
        }
    }
    
    return request;
}

/**
 * Worker thread function
 */
static void *stress_worker_thread(void *arg)
{
    stress_thread_data_t *data = (stress_thread_data_t *)arg;
    
    double total_latency = 0.0;
    
    for (int i = 0; i < data->num_requests && !stop_test; i++) {
        mcp_tool_request_t *request = create_stress_request(i);
        if (!request) {
            data->requests_failed++;
            atomic_fetch_add(&failed_requests, 1);
            continue;
        }
        
        uint64_t start_time = get_time_us();
        
        json_rpc_request_t *jsonrpc_request = NULL;
        mcp_error_code_t result = mcp_translate_request(request, &jsonrpc_request);
        
        uint64_t end_time = get_time_us();
        uint64_t latency = end_time - start_time;
        
        if (result == MCP_SUCCESS) {
            data->requests_completed++;
            atomic_fetch_add(&successful_requests, 1);
            total_latency += (double)latency;
            
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
        } else {
            data->requests_failed++;
            atomic_fetch_add(&failed_requests, 1);
        }
        
        atomic_fetch_add(&total_requests, 1);
        atomic_fetch_add(&total_time_us, latency);
        
        mcp_free_request(request);
        
        // Brief pause to avoid overwhelming
        if (i % 100 == 0) {
            usleep(100); // 0.1ms pause every 100 requests
        }
    }
    
    if (data->requests_completed > 0) {
        data->avg_latency_us = total_latency / data->requests_completed;
    }
    
    data->finished = true;
    return NULL;
}

/**
 * Monitor thread function - prints progress
 */
static void *monitor_thread(void *arg)
{
    (void)arg; // Unused
    
    while (!stop_test) {
        sleep(5);
        
        size_t total = atomic_load(&total_requests);
        size_t success = atomic_load(&successful_requests);
        size_t failed = atomic_load(&failed_requests);
        double avg_latency = 0.0;
        
        if (success > 0) {
            avg_latency = (double)atomic_load(&total_time_us) / success;
        }
        
        printf("[MONITOR] Total: %zu, Success: %zu, Failed: %zu, Avg: %.3f μs\n",
               total, success, failed, avg_latency);
    }
    
    return NULL;
}

/**
 * Get current memory usage in KB
 */
static size_t get_memory_usage_kb(void)
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss; // macOS returns bytes, Linux returns KB
    }
    return 0;
}

// ============================================================================
// MAIN STRESS TESTS
// ============================================================================

/**
 * Multi-threaded concurrent access test
 */
static int test_concurrent_access(int num_threads, int requests_per_thread)
{
    printf("Testing concurrent access (%d threads, %d requests each)...\n",
           num_threads, requests_per_thread);
    
    stress_thread_data_t *threads = calloc(num_threads, sizeof(stress_thread_data_t));
    if (!threads) {
        printf("FAIL: Memory allocation failed\n");
        return 1;
    }
    
    // Reset counters
    atomic_store(&total_requests, 0);
    atomic_store(&successful_requests, 0);
    atomic_store(&failed_requests, 0);
    atomic_store(&total_time_us, 0);
    stop_test = false;
    
    size_t start_memory = get_memory_usage_kb();
    uint64_t start_time = get_time_us();
    
    // Start monitor thread
    pthread_t monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    
    // Start worker threads
    for (int i = 0; i < num_threads; i++) {
        threads[i].thread_id = i;
        threads[i].num_requests = requests_per_thread;
        threads[i].finished = false;
        threads[i].requests_completed = 0;
        threads[i].requests_failed = 0;
        threads[i].avg_latency_us = 0.0;
        
        int result = pthread_create(&threads[i].pthread, NULL, 
                                   stress_worker_thread, &threads[i]);
        if (result != 0) {
            printf("FAIL: Could not create thread %d: %s\n", i, strerror(result));
            stop_test = true;
            break;
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        if (threads[i].pthread) {
            pthread_join(threads[i].pthread, NULL);
        }
    }
    
    stop_test = true;
    pthread_join(monitor, NULL);
    
    uint64_t end_time = get_time_us();
    size_t end_memory = get_memory_usage_kb();
    
    // Calculate results
    size_t total = atomic_load(&total_requests);
    size_t success = atomic_load(&successful_requests);
    size_t failed = atomic_load(&failed_requests);
    
    double total_time_sec = (double)(end_time - start_time) / 1000000.0;
    double avg_latency_us = success > 0 ? (double)atomic_load(&total_time_us) / success : 0.0;
    double throughput_ops_sec = total_time_sec > 0 ? total / total_time_sec : 0.0;
    
    // Print results
    printf("\nConcurrent Access Test Results:\n");
    printf("  Threads:           %d\n", num_threads);
    printf("  Total requests:    %zu\n", total);
    printf("  Successful:        %zu (%.1f%%)\n", success, 100.0 * success / total);
    printf("  Failed:            %zu (%.1f%%)\n", failed, 100.0 * failed / total);
    printf("  Test duration:     %.2f seconds\n", total_time_sec);
    printf("  Average latency:   %.3f μs\n", avg_latency_us);
    printf("  Throughput:        %.0f ops/sec\n", throughput_ops_sec);
    printf("  Memory delta:      %zd KB\n", end_memory - start_memory);
    
    // Per-thread statistics
    printf("\nPer-thread performance:\n");
    for (int i = 0; i < num_threads; i++) {
        printf("  Thread %2d: %zu success, %zu failed, %.3f μs avg\n",
               i, threads[i].requests_completed, threads[i].requests_failed,
               threads[i].avg_latency_us);
    }
    
    // Validate results
    bool passed = true;
    
    // Check success rate
    double success_rate = 100.0 * success / total;
    if (success_rate < 95.0) {
        printf("FAIL: Success rate too low (%.1f%% < 95%%)\n", success_rate);
        passed = false;
    }
    
    // Check average latency
    if (avg_latency_us > 2.0) { // Allow some overhead for contention
        printf("FAIL: Average latency too high (%.3f μs > 2.0 μs)\n", avg_latency_us);
        passed = false;
    }
    
    // Check throughput
    if (throughput_ops_sec < 10000) { // Reasonable minimum for concurrent access
        printf("FAIL: Throughput too low (%.0f ops/sec < 10000)\n", throughput_ops_sec);
        passed = false;
    }
    
    printf("\n%s: Concurrent access test\n", passed ? "PASS" : "FAIL");
    
    free(threads);
    return passed ? 0 : 1;
}

/**
 * Sustained load test
 */
static int test_sustained_load(int duration_sec)
{
    printf("Testing sustained load (%d seconds)...\n", duration_sec);
    
    // Reset counters
    atomic_store(&total_requests, 0);
    atomic_store(&successful_requests, 0);
    atomic_store(&failed_requests, 0);
    atomic_store(&total_time_us, 0);
    stop_test = false;
    
    size_t start_memory = get_memory_usage_kb();
    uint64_t start_time = get_time_us();
    uint64_t end_test_time = start_time + (uint64_t)duration_sec * 1000000;
    
    // Start monitor thread
    pthread_t monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    
    // Run sustained load
    int request_count = 0;
    while (get_time_us() < end_test_time && !stop_test) {
        mcp_tool_request_t *request = create_stress_request(request_count++);
        if (!request) {
            atomic_fetch_add(&failed_requests, 1);
            continue;
        }
        
        uint64_t req_start = get_time_us();
        
        json_rpc_request_t *jsonrpc_request = NULL;
        mcp_error_code_t result = mcp_translate_request(request, &jsonrpc_request);
        
        uint64_t req_end = get_time_us();
        uint64_t latency = req_end - req_start;
        
        if (result == MCP_SUCCESS) {
            atomic_fetch_add(&successful_requests, 1);
            if (jsonrpc_request) json_rpc_free_request(jsonrpc_request);
        } else {
            atomic_fetch_add(&failed_requests, 1);
        }
        
        atomic_fetch_add(&total_requests, 1);
        atomic_fetch_add(&total_time_us, latency);
        
        mcp_free_request(request);
        
        // Brief pause to prevent overwhelming
        usleep(100); // 0.1ms between requests
    }
    
    stop_test = true;
    pthread_join(monitor, NULL);
    
    uint64_t actual_end_time = get_time_us();
    size_t end_memory = get_memory_usage_kb();
    
    // Calculate results
    size_t total = atomic_load(&total_requests);
    size_t success = atomic_load(&successful_requests);
    size_t failed = atomic_load(&failed_requests);
    
    double actual_duration = (double)(actual_end_time - start_time) / 1000000.0;
    double avg_latency_us = success > 0 ? (double)atomic_load(&total_time_us) / success : 0.0;
    double throughput_ops_sec = actual_duration > 0 ? total / actual_duration : 0.0;
    
    printf("\nSustained Load Test Results:\n");
    printf("  Duration:          %.2f seconds\n", actual_duration);
    printf("  Total requests:    %zu\n", total);
    printf("  Successful:        %zu (%.1f%%)\n", success, 100.0 * success / total);
    printf("  Failed:            %zu (%.1f%%)\n", failed, 100.0 * failed / total);
    printf("  Average latency:   %.3f μs\n", avg_latency_us);
    printf("  Throughput:        %.0f ops/sec\n", throughput_ops_sec);
    printf("  Memory delta:      %zd KB\n", end_memory - start_memory);
    printf("  Memory per request: %.3f bytes\n", 
           total > 0 ? (double)(end_memory - start_memory) * 1024 / total : 0.0);
    
    // Validate sustained performance
    bool passed = true;
    
    double success_rate = 100.0 * success / total;
    if (success_rate < 98.0) {
        printf("FAIL: Success rate degraded (%.1f%% < 98%%)\n", success_rate);
        passed = false;
    }
    
    if (avg_latency_us > 1.0) {
        printf("FAIL: Latency degraded (%.3f μs > 1.0 μs)\n", avg_latency_us);
        passed = false;
    }
    
    // Check for memory leaks (should be minimal growth)
    double memory_per_req = total > 0 ? (double)(end_memory - start_memory) * 1024 / total : 0.0;
    if (memory_per_req > 1024) { // More than 1KB per request suggests leaks
        printf("FAIL: Possible memory leak (%.1f bytes per request)\n", memory_per_req);
        passed = false;
    }
    
    printf("\n%s: Sustained load test\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char *argv[])
{
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                      Goxel v14.0 MCP Stress Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Testing Sarah's MCP Handler under stress conditions\n");
    printf("Author: Alex Kumar - Testing & Performance Validation Expert\n");
    printf("Date: February 4, 2025 (Week 2, Day 2)\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    // Initialize MCP handler
    mcp_error_code_t result = mcp_handler_init();
    if (result != MCP_SUCCESS) {
        printf("FATAL: Failed to initialize MCP handler: %s\n", mcp_error_string(result));
        return 1;
    }
    
    int failures = 0;
    
    // Test 1: Moderate concurrent access
    printf("Test 1: Moderate Concurrent Access\n");
    printf("──────────────────────────────────\n");
    failures += test_concurrent_access(4, 1000);
    printf("\n");
    
    // Test 2: High concurrent access
    printf("Test 2: High Concurrent Access\n");
    printf("──────────────────────────────\n");
    failures += test_concurrent_access(16, 500);
    printf("\n");
    
    // Test 3: Sustained load
    printf("Test 3: Sustained Load\n");
    printf("─────────────────────\n");
    failures += test_sustained_load(30); // 30 second test
    printf("\n");
    
    // Get final MCP statistics
    mcp_handler_stats_t stats;
    mcp_get_handler_stats(&stats);
    
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                               FINAL RESULTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Failed tests: %d\n", failures);
    printf("\nMCP Handler Final Statistics:\n");
    printf("  Total translations:   %zu\n", stats.requests_translated);
    printf("  Translation errors:   %zu\n", stats.translation_errors);
    printf("  Direct translations:  %zu\n", stats.direct_translations);
    printf("  Mapped translations:  %zu\n", stats.mapped_translations);
    printf("  Avg translation time: %.3f μs\n", stats.avg_translation_time_us);
    printf("  Batch requests:       %zu\n", stats.batch_requests);
    
    if (failures == 0) {
        printf("\n🎉 SUCCESS: Sarah's MCP handler passes all stress tests!\n");
        printf("   Implementation is robust under concurrent load.\n");
    } else {
        printf("\n❌ FAILURE: %d stress tests failed\n", failures);
        printf("   Implementation may have concurrency or memory issues.\n");
    }
    
    mcp_handler_cleanup();
    return failures > 0 ? 1 : 0;
}