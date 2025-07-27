/*
 * Goxel v14.0 Daemon Architecture - Throughput Test Framework
 * 
 * This module measures throughput performance for various voxel operations
 * in operations per second (ops/sec). Tests concurrent operations and
 * sustained performance under load.
 * 
 * Target: >1000 voxel operations/second
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define MAX_THREADS 16
#define MAX_OPERATIONS 100000
#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define TARGET_THROUGHPUT_OPS 1000
#define TEST_DURATION_SEC 10

typedef struct {
    int operations_completed;
    int operations_failed;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
} thread_stats_t;

typedef struct {
    int thread_id;
    int target_operations;
    int test_duration_sec;
    const char *operation_type;
    const char *request_template;
    thread_stats_t *stats;
    volatile int *stop_flag;
} thread_context_t;

typedef struct {
    const char *name;
    const char *request_template;
    int expected_throughput;
    int concurrent_threads;
} throughput_scenario_t;

static const throughput_scenario_t scenarios[] = {
    {
        "add_voxel",
        "{\"method\":\"add_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d,\"color\":[%d,%d,%d,255]}}",
        1500,
        4
    },
    {
        "get_voxel", 
        "{\"method\":\"get_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
        2000,
        6
    },
    {
        "remove_voxel",
        "{\"method\":\"remove_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
        1800,
        4
    },
    {
        "batch_add_voxels",
        "{\"method\":\"batch_add_voxels\",\"params\":{\"voxels\":[{\"x\":%d,\"y\":%d,\"z\":%d,\"color\":[255,0,0,255]}]}}",
        800,
        2
    },
    {
        "get_project_info",
        "{\"method\":\"get_project_info\"}",
        5000,
        8
    },
    {NULL, NULL, 0, 0}
};

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static int connect_to_daemon(void)
{
    int sockfd;
    struct sockaddr_un addr;
    
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

static int execute_operation(const char *request)
{
    int sockfd;
    char response[4096];
    ssize_t bytes_sent, bytes_received;
    
    sockfd = connect_to_daemon();
    if (sockfd == -1) {
        return 0;
    }
    
    bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent <= 0) {
        close(sockfd);
        return 0;
    }
    
    bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    close(sockfd);
    
    return (bytes_received > 0) ? 1 : 0;
}

static void *throughput_worker(void *arg)
{
    thread_context_t *ctx = (thread_context_t*)arg;
    char request[512];
    double start_time, end_time, op_time;
    int x, y, z, r, g, b;
    
    ctx->stats->operations_completed = 0;
    ctx->stats->operations_failed = 0;
    ctx->stats->total_time_ms = 0.0;
    ctx->stats->min_time_ms = 999999.0;
    ctx->stats->max_time_ms = 0.0;
    
    double test_start = get_time_ms();
    
    while (!(*ctx->stop_flag) && 
           (ctx->test_duration_sec == 0 || 
            (get_time_ms() - test_start) < ctx->test_duration_sec * 1000.0)) {
        
        // Generate random parameters for the operation
        x = rand() % 100 - 50;
        y = rand() % 100 - 50;  
        z = rand() % 100 - 50;
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
        
        // Format the request based on operation type
        if (strstr(ctx->operation_type, "add_voxel") || 
            strstr(ctx->operation_type, "get_voxel") ||
            strstr(ctx->operation_type, "remove_voxel")) {
            snprintf(request, sizeof(request), ctx->request_template, x, y, z, r, g, b);
        } else if (strstr(ctx->operation_type, "batch_add_voxels")) {
            snprintf(request, sizeof(request), ctx->request_template, x, y, z);
        } else {
            strcpy(request, ctx->request_template);
        }
        
        start_time = get_time_ms();
        
        if (execute_operation(request)) {
            end_time = get_time_ms();
            op_time = end_time - start_time;
            
            ctx->stats->operations_completed++;
            ctx->stats->total_time_ms += op_time;
            
            if (op_time < ctx->stats->min_time_ms) {
                ctx->stats->min_time_ms = op_time;
            }
            if (op_time > ctx->stats->max_time_ms) {
                ctx->stats->max_time_ms = op_time;
            }
        } else {
            ctx->stats->operations_failed++;
        }
        
        // Small delay to prevent overwhelming the daemon
        usleep(100); // 0.1ms
    }
    
    return NULL;
}

static int run_throughput_test(const throughput_scenario_t *scenario, int duration_sec)
{
    pthread_t threads[MAX_THREADS];
    thread_context_t contexts[MAX_THREADS];
    thread_stats_t stats[MAX_THREADS];
    volatile int stop_flag = 0;
    double start_time, end_time, test_duration;
    int i, total_ops = 0, total_failed = 0;
    double total_throughput;
    
    printf("Testing %s throughput (target: %d ops/sec, %d threads)...\n",
           scenario->name, scenario->expected_throughput, scenario->concurrent_threads);
    
    start_time = get_time_ms();
    
    // Create worker threads
    for (i = 0; i < scenario->concurrent_threads; i++) {
        contexts[i].thread_id = i;
        contexts[i].target_operations = 0; // Unlimited
        contexts[i].test_duration_sec = duration_sec;
        contexts[i].operation_type = scenario->name;
        contexts[i].request_template = scenario->request_template;
        contexts[i].stats = &stats[i];
        contexts[i].stop_flag = &stop_flag;
        
        if (pthread_create(&threads[i], NULL, throughput_worker, &contexts[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            stop_flag = 1;
            return 0;
        }
    }
    
    // Let the test run for the specified duration
    sleep(duration_sec);
    stop_flag = 1;
    
    // Wait for all threads to complete
    for (i = 0; i < scenario->concurrent_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    end_time = get_time_ms();
    test_duration = (end_time - start_time) / 1000.0;
    
    // Aggregate results
    for (i = 0; i < scenario->concurrent_threads; i++) {
        total_ops += stats[i].operations_completed;
        total_failed += stats[i].operations_failed;
    }
    
    total_throughput = total_ops / test_duration;
    
    printf("Results for %s:\n", scenario->name);
    printf("  Test Duration: %.2f seconds\n", test_duration);
    printf("  Total Operations: %d\n", total_ops);
    printf("  Failed Operations: %d\n", total_failed);
    printf("  Success Rate: %.1f%%\n", 
           (total_ops + total_failed > 0) ? 
           (100.0 * total_ops / (total_ops + total_failed)) : 0.0);
    printf("  Throughput: %.1f ops/sec\n", total_throughput);
    printf("  Target: %d ops/sec - %s\n", 
           scenario->expected_throughput,
           (total_throughput >= scenario->expected_throughput) ? "PASS" : "FAIL");
    
    // Per-thread breakdown
    printf("  Per-thread stats:\n");
    for (i = 0; i < scenario->concurrent_threads; i++) {
        if (stats[i].operations_completed > 0) {
            double avg_time = stats[i].total_time_ms / stats[i].operations_completed;
            printf("    Thread %d: %d ops, avg %.2fms (%.2f-%.2fms)\n",
                   i, stats[i].operations_completed, avg_time,
                   stats[i].min_time_ms, stats[i].max_time_ms);
        }
    }
    printf("\n");
    
    return (total_throughput >= scenario->expected_throughput) ? 1 : 0;
}

static void run_sustained_throughput_test(int duration_sec)
{
    printf("=== Sustained Throughput Test (%d seconds) ===\n", duration_sec);
    
    const char *request = "{\"method\":\"add_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0,\"color\":[255,0,0,255]}}";
    double start_time = get_time_ms();
    int operations = 0;
    double last_report = start_time;
    
    printf("Running sustained operations");
    fflush(stdout);
    
    while ((get_time_ms() - start_time) < duration_sec * 1000.0) {
        if (execute_operation(request)) {
            operations++;
        }
        
        // Report progress every 2 seconds
        if ((get_time_ms() - last_report) >= 2000.0) {
            printf(".");
            fflush(stdout);
            last_report = get_time_ms();
        }
    }
    
    double end_time = get_time_ms();
    double actual_duration = (end_time - start_time) / 1000.0;
    double sustained_throughput = operations / actual_duration;
    
    printf(" done.\n");
    printf("Sustained Throughput Results:\n");
    printf("  Duration: %.2f seconds\n", actual_duration);
    printf("  Operations: %d\n", operations);
    printf("  Sustained Throughput: %.1f ops/sec\n", sustained_throughput);
    printf("  Target: %d ops/sec - %s\n\n",
           TARGET_THROUGHPUT_OPS,
           (sustained_throughput >= TARGET_THROUGHPUT_OPS) ? "PASS" : "FAIL");
}

int main(int argc, char *argv[])
{
    int test_duration = TEST_DURATION_SEC;
    int i, num_tests, tests_passed = 0;
    
    if (argc > 1) {
        test_duration = atoi(argv[1]);
        if (test_duration <= 0 || test_duration > 60) {
            fprintf(stderr, "Invalid test duration. Using default: %d seconds\n", 
                   TEST_DURATION_SEC);
            test_duration = TEST_DURATION_SEC;
        }
    }
    
    printf("Goxel v14.0 Daemon Throughput Benchmark\n");
    printf("=======================================\n");
    printf("Target: >%d operations/second\n", TARGET_THROUGHPUT_OPS);
    printf("Test Duration: %d seconds per test\n\n", test_duration);
    
    // Initialize random seed
    srand(time(NULL));
    
    // Count available test scenarios
    for (num_tests = 0; scenarios[num_tests].name; num_tests++);
    
    // Run individual throughput tests
    for (i = 0; i < num_tests; i++) {
        if (run_throughput_test(&scenarios[i], test_duration)) {
            tests_passed++;
        }
    }
    
    // Run sustained throughput test
    run_sustained_throughput_test(test_duration * 2);
    
    // Summary
    printf("=== THROUGHPUT BENCHMARK SUMMARY ===\n");
    printf("Tests Passed: %d/%d\n", tests_passed, num_tests);
    printf("Overall Grade: %s\n",
           (tests_passed >= num_tests * 0.8) ? "EXCELLENT" : "NEEDS_IMPROVEMENT");
    
    return (tests_passed >= num_tests * 0.8) ? 0 : 1;
}