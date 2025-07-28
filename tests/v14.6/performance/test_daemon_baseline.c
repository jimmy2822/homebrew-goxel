/*
 * Goxel v14.6 Daemon Performance Baseline Test
 * 
 * Measures key performance metrics: daemon startup time, socket connection
 * latency, JSON-RPC round-trip time, and memory usage baseline.
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "../framework/test_framework.h"
#include <sys/un.h>
#include <sys/resource.h>

#define DAEMON_BINARY "../../../goxel"
#define DAEMON_SOCKET "/tmp/goxel.sock"
#define DAEMON_PID_FILE "/tmp/goxel-daemon.pid"
#define WARMUP_ITERATIONS 10
#define MEASURE_ITERATIONS 100

// Helper to get process memory usage
static size_t get_process_memory(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    
    char line[256];
    size_t vmrss = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "VmRSS: %zu kB", &vmrss) == 1) {
            break;
        }
    }
    
    fclose(fp);
    return vmrss;
}

// Test: Daemon startup time
TEST_CASE(daemon_startup_time) {
    test_log_info("Measuring daemon startup time...");
    
    // Measure multiple startups
    for (int i = 0; i < MEASURE_ITERATIONS; i++) {
        // Clean up
        unlink(DAEMON_PID_FILE);
        unlink(DAEMON_SOCKET);
        
        // Start timing
        perf_start_measurement();
        
        // Fork and exec daemon
        pid_t daemon_pid = fork();
        TEST_ASSERT(daemon_pid >= 0);
        
        if (daemon_pid == 0) {
            execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
            exit(1);
        }
        
        // Wait for socket to be ready (indicates daemon is fully started)
        bool started = false;
        int wait_time = 0;
        while (wait_time < 5000) {
            if (access(DAEMON_SOCKET, F_OK) == 0) {
                started = true;
                break;
            }
            usleep(10000); // 10ms
            wait_time += 10;
        }
        
        double elapsed = perf_end_measurement();
        
        if (started) {
            perf_record_iteration(elapsed);
            
            if (i == 0) {
                test_log_info("First startup: %.2f ms", elapsed);
            }
        } else {
            test_log_warning("Daemon startup timeout on iteration %d", i);
        }
        
        // Stop daemon
        kill(daemon_pid, SIGTERM);
        waitpid(daemon_pid, NULL, 0);
        
        // Brief pause between iterations
        usleep(100000); // 100ms
    }
    
    // Calculate and report metrics
    perf_metrics_t *metrics = perf_calculate_metrics();
    TEST_ASSERT_NOT_NULL(metrics);
    
    perf_print_metrics("Daemon Startup Time", metrics);
    
    // Save results for comparison
    FILE *fp = fopen("results/daemon_startup_baseline.json", "w");
    if (fp) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"test\": \"daemon_startup_time\",\n");
        fprintf(fp, "  \"iterations\": %zu,\n", metrics->iterations);
        fprintf(fp, "  \"min_ms\": %.3f,\n", metrics->min_time_ms);
        fprintf(fp, "  \"avg_ms\": %.3f,\n", metrics->avg_time_ms);
        fprintf(fp, "  \"max_ms\": %.3f,\n", metrics->max_time_ms);
        fprintf(fp, "  \"p50_ms\": %.3f,\n", metrics->percentile_50);
        fprintf(fp, "  \"p95_ms\": %.3f,\n", metrics->percentile_95);
        fprintf(fp, "  \"p99_ms\": %.3f\n", metrics->percentile_99);
        fprintf(fp, "}\n");
        fclose(fp);
    }
    
    free(metrics);
    
    return TEST_PASS;
}

// Test: Socket connection latency
TEST_CASE(socket_connection_latency) {
    test_log_info("Measuring socket connection latency...");
    
    // Start daemon once
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for daemon
    int wait_time = 0;
    bool daemon_ready = false;
    while (wait_time < 5000) {
        if (access(DAEMON_SOCKET, F_OK) == 0) {
            daemon_ready = true;
            break;
        }
        usleep(10000);
        wait_time += 10;
    }
    TEST_ASSERT(daemon_ready);
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        int fd = test_connect_unix_socket(DAEMON_SOCKET);
        if (fd >= 0) close(fd);
    }
    
    // Measure connection times
    for (int i = 0; i < MEASURE_ITERATIONS; i++) {
        perf_start_measurement();
        
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd >= 0) {
            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
            
            int result = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            double elapsed = perf_end_measurement();
            
            if (result == 0) {
                perf_record_iteration(elapsed);
            }
            
            close(fd);
        }
    }
    
    // Calculate metrics
    perf_metrics_t *metrics = perf_calculate_metrics();
    TEST_ASSERT_NOT_NULL(metrics);
    
    perf_print_metrics("Socket Connection Latency", metrics);
    free(metrics);
    
    // Cleanup
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    
    return TEST_PASS;
}

// Test: JSON-RPC round-trip time
TEST_CASE(json_rpc_round_trip) {
    test_log_info("Measuring JSON-RPC round-trip time...");
    
    // Start daemon
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for daemon
    int wait_time = 0;
    bool daemon_ready = false;
    while (wait_time < 5000) {
        if (access(DAEMON_SOCKET, F_OK) == 0) {
            daemon_ready = true;
            break;
        }
        usleep(10000);
        wait_time += 10;
    }
    TEST_ASSERT(daemon_ready);
    
    // Connect
    int fd = test_connect_unix_socket(DAEMON_SOCKET);
    TEST_ASSERT(fd >= 0);
    
    // Prepare echo request
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\","
                         "\"params\":{\"message\":\"benchmark\"}}\n";
    size_t request_len = strlen(request);
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        send(fd, request, request_len, 0);
        char buffer[1024];
        recv(fd, buffer, sizeof(buffer), 0);
    }
    
    // Measure round-trip times
    for (int i = 0; i < MEASURE_ITERATIONS; i++) {
        perf_start_measurement();
        
        // Send request
        ssize_t sent = send(fd, request, request_len, 0);
        if (sent == request_len) {
            // Receive response
            char buffer[1024];
            ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
            
            double elapsed = perf_end_measurement();
            
            if (received > 0) {
                perf_record_iteration(elapsed);
            }
        }
    }
    
    // Calculate metrics
    perf_metrics_t *metrics = perf_calculate_metrics();
    TEST_ASSERT_NOT_NULL(metrics);
    
    perf_print_metrics("JSON-RPC Round-Trip Time", metrics);
    
    // Save results
    FILE *fp = fopen("results/json_rpc_baseline.json", "w");
    if (fp) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"test\": \"json_rpc_round_trip\",\n");
        fprintf(fp, "  \"iterations\": %zu,\n", metrics->iterations);
        fprintf(fp, "  \"min_ms\": %.3f,\n", metrics->min_time_ms);
        fprintf(fp, "  \"avg_ms\": %.3f,\n", metrics->avg_time_ms);
        fprintf(fp, "  \"max_ms\": %.3f,\n", metrics->max_time_ms);
        fprintf(fp, "  \"p50_ms\": %.3f,\n", metrics->percentile_50);
        fprintf(fp, "  \"p95_ms\": %.3f,\n", metrics->percentile_95);
        fprintf(fp, "  \"p99_ms\": %.3f\n", metrics->percentile_99);
        fprintf(fp, "}\n");
        fclose(fp);
    }
    
    free(metrics);
    close(fd);
    
    // Cleanup
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    
    return TEST_PASS;
}

// Test: Memory usage baseline
TEST_CASE(memory_usage_baseline) {
    test_log_info("Measuring memory usage baseline...");
    
    // Start daemon
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for daemon
    int wait_time = 0;
    bool daemon_ready = false;
    while (wait_time < 5000) {
        if (access(DAEMON_SOCKET, F_OK) == 0) {
            daemon_ready = true;
            break;
        }
        usleep(10000);
        wait_time += 10;
    }
    TEST_ASSERT(daemon_ready);
    
    // Get PID from file
    pid_t file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(file_pid > 0);
    
    // Measure initial memory
    usleep(500000); // 500ms settle time
    size_t initial_memory = get_process_memory(file_pid);
    test_log_info("Initial memory: %zu KB", initial_memory);
    
    // Connect some clients
    int clients[10];
    for (int i = 0; i < 10; i++) {
        clients[i] = test_connect_unix_socket(DAEMON_SOCKET);
        TEST_ASSERT(clients[i] >= 0);
    }
    
    // Measure with connections
    usleep(100000); // 100ms
    size_t connected_memory = get_process_memory(file_pid);
    test_log_info("Memory with 10 connections: %zu KB", connected_memory);
    
    // Send some requests
    for (int i = 0; i < 10; i++) {
        char request[256];
        snprintf(request, sizeof(request),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"echo\","
                "\"params\":{\"data\":\"test\"}}\n", i);
        send(clients[i], request, strlen(request), 0);
    }
    
    // Wait and measure
    usleep(500000); // 500ms
    size_t active_memory = get_process_memory(file_pid);
    test_log_info("Memory after activity: %zu KB", active_memory);
    
    // Disconnect clients
    for (int i = 0; i < 10; i++) {
        close(clients[i]);
    }
    
    // Final measurement
    usleep(1000000); // 1 second
    size_t final_memory = get_process_memory(file_pid);
    test_log_info("Final memory: %zu KB", final_memory);
    
    // Save results
    FILE *fp = fopen("results/memory_baseline.json", "w");
    if (fp) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"test\": \"memory_usage_baseline\",\n");
        fprintf(fp, "  \"initial_kb\": %zu,\n", initial_memory);
        fprintf(fp, "  \"connected_kb\": %zu,\n", connected_memory);
        fprintf(fp, "  \"active_kb\": %zu,\n", active_memory);
        fprintf(fp, "  \"final_kb\": %zu,\n", final_memory);
        fprintf(fp, "  \"connection_overhead_kb\": %zu,\n", connected_memory - initial_memory);
        fprintf(fp, "  \"activity_overhead_kb\": %zu\n", active_memory - connected_memory);
        fprintf(fp, "}\n");
        fclose(fp);
    }
    
    // Cleanup
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(daemon_baseline) {
    REGISTER_PERF_TEST(daemon_baseline, daemon_startup_time, NULL, NULL);
    REGISTER_PERF_TEST(daemon_baseline, socket_connection_latency, NULL, NULL);
    REGISTER_PERF_TEST(daemon_baseline, json_rpc_round_trip, NULL, NULL);
    REGISTER_PERF_TEST(daemon_baseline, memory_usage_baseline, NULL, NULL);
}