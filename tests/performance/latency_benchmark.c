/*
 * Goxel v14.0 Daemon Architecture - Latency Benchmark Framework
 * 
 * This module provides comprehensive latency measurement capabilities for
 * daemon-based operations including socket communication, request processing,
 * and response generation.
 * 
 * Target: <2.1ms average request latency
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_SAMPLES 10000
#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define TARGET_LATENCY_MS 2.1
#define WARMUP_REQUESTS 10

typedef struct {
    double latency_ms;
    int success;
    size_t response_size;
} latency_sample_t;

typedef struct {
    latency_sample_t samples[MAX_SAMPLES];
    int count;
    double min_ms;
    double max_ms;
    double avg_ms;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    int success_rate;
} latency_stats_t;

typedef struct {
    const char *name;
    const char *request;
    size_t request_size;
    double target_ms;
} test_scenario_t;

static const test_scenario_t scenarios[] = {
    {"ping", "{\"method\":\"ping\"}", 16, 0.5},
    {"get_status", "{\"method\":\"get_status\"}", 22, 1.0},
    {"create_project", "{\"method\":\"create_project\",\"params\":{\"name\":\"test\"}}", 58, 2.0},
    {"add_voxel", "{\"method\":\"add_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0,\"color\":[255,0,0,255]}}", 88, 1.5},
    {"get_voxel", "{\"method\":\"get_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0}}", 55, 1.0},
    {"export_mesh", "{\"method\":\"export_mesh\",\"params\":{\"format\":\"obj\"}}", 58, 5.0},
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
        perror("socket");
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

static int send_request_and_measure(const char *request, size_t request_size, 
                                   latency_sample_t *sample)
{
    int sockfd;
    char response[4096];
    double start_time, end_time;
    ssize_t bytes_sent, bytes_received;
    
    sample->success = 0;
    sample->response_size = 0;
    
    sockfd = connect_to_daemon();
    if (sockfd == -1) {
        sample->latency_ms = -1.0;
        return 0;
    }
    
    start_time = get_time_ms();
    
    bytes_sent = send(sockfd, request, request_size, 0);
    if (bytes_sent != (ssize_t)request_size) {
        close(sockfd);
        sample->latency_ms = -1.0;
        return 0;
    }
    
    bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    end_time = get_time_ms();
    
    close(sockfd);
    
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        sample->success = 1;
        sample->response_size = bytes_received;
        sample->latency_ms = end_time - start_time;
        return 1;
    }
    
    sample->latency_ms = end_time - start_time;
    return 0;
}

static int compare_samples(const void *a, const void *b)
{
    const latency_sample_t *sa = (const latency_sample_t*)a;
    const latency_sample_t *sb = (const latency_sample_t*)b;
    
    if (sa->latency_ms < sb->latency_ms) return -1;
    if (sa->latency_ms > sb->latency_ms) return 1;
    return 0;
}

static void calculate_stats(latency_stats_t *stats)
{
    int i, success_count = 0;
    double sum = 0.0;
    latency_sample_t valid_samples[MAX_SAMPLES];
    int valid_count = 0;
    
    if (stats->count == 0) return;
    
    // Extract valid samples and calculate basic stats
    for (i = 0; i < stats->count; i++) {
        if (stats->samples[i].success && stats->samples[i].latency_ms >= 0) {
            valid_samples[valid_count++] = stats->samples[i];
            sum += stats->samples[i].latency_ms;
            success_count++;
        }
    }
    
    if (valid_count == 0) {
        stats->success_rate = 0;
        return;
    }
    
    stats->success_rate = (success_count * 100) / stats->count;
    stats->avg_ms = sum / valid_count;
    
    // Sort for percentile calculations
    qsort(valid_samples, valid_count, sizeof(latency_sample_t), compare_samples);
    
    stats->min_ms = valid_samples[0].latency_ms;
    stats->max_ms = valid_samples[valid_count - 1].latency_ms;
    stats->p50_ms = valid_samples[valid_count / 2].latency_ms;
    stats->p95_ms = valid_samples[(int)(valid_count * 0.95)].latency_ms;
    stats->p99_ms = valid_samples[(int)(valid_count * 0.99)].latency_ms;
}

static void warmup_daemon(void)
{
    int i;
    latency_sample_t sample;
    
    printf("Warming up daemon with %d requests...\n", WARMUP_REQUESTS);
    
    for (i = 0; i < WARMUP_REQUESTS; i++) {
        send_request_and_measure(scenarios[0].request, scenarios[0].request_size, &sample);
        usleep(10000); // 10ms between warmup requests
    }
    
    printf("Warmup complete.\n\n");
}

static int run_latency_test(const test_scenario_t *scenario, int num_samples,
                           latency_stats_t *stats)
{
    int i;
    
    printf("Testing %s latency (target: %.1fms)...\n", scenario->name, scenario->target_ms);
    printf("Sending %d requests", num_samples);
    fflush(stdout);
    
    stats->count = 0;
    
    for (i = 0; i < num_samples && i < MAX_SAMPLES; i++) {
        if (i % (num_samples / 10) == 0) {
            printf(".");
            fflush(stdout);
        }
        
        send_request_and_measure(scenario->request, scenario->request_size, 
                               &stats->samples[stats->count]);
        stats->count++;
        
        // Small delay to avoid overwhelming the daemon
        usleep(1000); // 1ms
    }
    
    printf(" done.\n");
    
    calculate_stats(stats);
    return stats->success_rate > 0;
}

static void print_latency_report(const test_scenario_t *scenario, 
                                const latency_stats_t *stats)
{
    const char *status = (stats->avg_ms <= scenario->target_ms) ? "PASS" : "FAIL";
    
    printf("\n=== %s Latency Report ===\n", scenario->name);
    printf("Target: %.1fms | Status: %s\n", scenario->target_ms, status);
    printf("Success Rate: %d%%\n", stats->success_rate);
    printf("Samples: %d\n", stats->count);
    
    if (stats->success_rate > 0) {
        printf("Average: %.2fms\n", stats->avg_ms);
        printf("Min: %.2fms | Max: %.2fms\n", stats->min_ms, stats->max_ms);
        printf("P50: %.2fms | P95: %.2fms | P99: %.2fms\n", 
               stats->p50_ms, stats->p95_ms, stats->p99_ms);
    }
    printf("\n");
}

static void print_summary_report(latency_stats_t results[], int num_tests)
{
    int i, tests_passed = 0;
    double overall_avg = 0.0;
    int total_samples = 0;
    
    printf("=== LATENCY BENCHMARK SUMMARY ===\n");
    
    for (i = 0; i < num_tests; i++) {
        if (results[i].success_rate > 0) {
            overall_avg += results[i].avg_ms * results[i].count;
            total_samples += results[i].count;
            
            if (results[i].avg_ms <= scenarios[i].target_ms) {
                tests_passed++;
            }
        }
    }
    
    if (total_samples > 0) {
        overall_avg /= total_samples;
    }
    
    printf("Tests Passed: %d/%d\n", tests_passed, num_tests);
    printf("Overall Average Latency: %.2fms\n", overall_avg);
    printf("Target Achievement: %s\n", 
           (overall_avg <= TARGET_LATENCY_MS) ? "ACHIEVED" : "FAILED");
    printf("Daemon Performance Grade: %s\n",
           (tests_passed >= num_tests * 0.8) ? "EXCELLENT" : "NEEDS_IMPROVEMENT");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int num_samples = 100;
    int i, num_tests;
    latency_stats_t results[10];
    
    if (argc > 1) {
        num_samples = atoi(argv[1]);
        if (num_samples <= 0 || num_samples > MAX_SAMPLES) {
            fprintf(stderr, "Invalid sample count. Using default: 100\n");
            num_samples = 100;
        }
    }
    
    printf("Goxel v14.0 Daemon Latency Benchmark\n");
    printf("====================================\n");
    printf("Target: <%.1fms average latency\n", TARGET_LATENCY_MS);
    printf("Samples per test: %d\n\n", num_samples);
    
    // Count available test scenarios
    for (num_tests = 0; scenarios[num_tests].name; num_tests++);
    
    warmup_daemon();
    
    // Run all latency tests
    for (i = 0; i < num_tests; i++) {
        if (!run_latency_test(&scenarios[i], num_samples, &results[i])) {
            fprintf(stderr, "Warning: Test %s had no successful samples\n", 
                   scenarios[i].name);
        }
        print_latency_report(&scenarios[i], &results[i]);
    }
    
    print_summary_report(results, num_tests);
    
    return 0;
}