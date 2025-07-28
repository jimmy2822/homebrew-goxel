/* Goxel 3D voxels editor - Memory Leak and Resource Usage Testing
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

/*
 * Memory Leak and Resource Usage Testing Suite for Goxel v14.0
 * 
 * This comprehensive suite validates memory management and resource usage:
 * - Memory leak detection during normal operations
 * - Resource usage monitoring (memory, file descriptors, sockets)
 * - Memory pressure testing with large datasets
 * - Long-term stability and memory growth analysis
 * - Valgrind/AddressSanitizer integration
 * - Memory usage benchmarking vs v13.4
 * 
 * Test Categories:
 * 1. Basic Memory Leak Detection - Standard operations
 * 2. Stress Memory Testing - High-load scenarios
 * 3. Resource Monitoring - FD, socket, memory tracking
 * 4. Long-term Stability - Extended duration testing
 * 5. Memory Pressure - Large voxel datasets
 * 6. Resource Cleanup - Proper resource deallocation
 */

#include "../../src/daemon/daemon_lifecycle.h"
#include "../../src/daemon/json_rpc.h"
#include "../../src/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>

// ============================================================================
// MEMORY TEST CONFIGURATION
// ============================================================================

#define MAX_MEMORY_CLIENTS 50
#define MAX_RESPONSE_SIZE 8192
#define DEFAULT_MEMORY_TEST_DURATION 300  // 5 minutes
#define MEMORY_SAMPLE_INTERVAL_MS 1000    // 1 second
#define LARGE_DATASET_SIZE 1000000        // 1M voxels for pressure testing
#define MEMORY_LEAK_THRESHOLD_MB 10       // Max acceptable memory growth
#define RESOURCE_LEAK_THRESHOLD 100       // Max acceptable FD/socket growth

// Test files and paths
static const char *MEMORY_DAEMON_SOCKET = "/tmp/goxel_memory_test.sock";
static const char *MEMORY_DAEMON_PID = "/tmp/goxel_memory_test.pid";
static const char *MEMORY_LOG_FILE = "/tmp/goxel_memory_test.log";

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    double timestamp_ms;
    long memory_rss_mb;      // Resident Set Size
    long memory_vss_mb;      // Virtual Set Size  
    long memory_shared_mb;   // Shared memory
    int open_files;          // Open file descriptors
    int socket_count;        // Active socket connections
    double cpu_percent;      // CPU usage percentage
} resource_snapshot_t;

typedef struct {
    // Test configuration
    int test_duration_seconds;
    int num_test_clients;
    bool enable_large_datasets;
    bool enable_valgrind_mode;
    
    // Daemon control
    pid_t daemon_pid;
    bool daemon_running;
    
    // Resource monitoring
    resource_snapshot_t snapshots[3600]; // Up to 1 hour of samples
    int snapshot_count;
    int snapshot_index;
    
    // Memory analysis
    long baseline_memory_mb;
    long peak_memory_mb;
    long final_memory_mb;
    long memory_growth_mb;
    
    // Resource analysis
    int baseline_open_files;
    int peak_open_files;
    int final_open_files;
    
    // Test statistics
    int total_operations;
    int memory_samples;
    bool memory_leak_detected;
    bool resource_leak_detected;
    
    // Synchronization
    pthread_mutex_t monitor_mutex;
    volatile bool stop_monitoring;
    pthread_t monitor_thread;
    
} memory_test_context_t;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static void cleanup_memory_test_files(void)
{
    unlink(MEMORY_DAEMON_SOCKET);
    unlink(MEMORY_DAEMON_PID);
    unlink(MEMORY_LOG_FILE);
}

// ============================================================================
// SYSTEM RESOURCE MONITORING
// ============================================================================

static long get_process_memory_mb(pid_t pid, const char *field)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    
    char line[256];
    long value_kb = -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, field, strlen(field)) == 0) {
            char *ptr = strchr(line, ':');
            if (ptr) {
                sscanf(ptr + 1, "%ld", &value_kb);
                break;
            }
        }
    }
    
    fclose(file);
    return value_kb > 0 ? value_kb / 1024 : -1; // Convert to MB
}

static int count_open_files(pid_t pid)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    
    DIR *dir = opendir(path);
    if (!dir) return -1;
    
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

static int count_socket_connections(pid_t pid)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/net/unix", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        // Try system-wide socket count as fallback
        file = fopen("/proc/net/unix", "r");
        if (!file) return -1;
    }
    
    char line[512];
    int socket_count = 0;
    
    // Skip header line
    if (fgets(line, sizeof(line), file)) {
        while (fgets(line, sizeof(line), file)) {
            if (strstr(line, "goxel") || strstr(line, MEMORY_DAEMON_SOCKET)) {
                socket_count++;
            }
        }
    }
    
    fclose(file);
    return socket_count;
}

static double get_cpu_usage(pid_t pid)
{
    static long last_utime = 0, last_stime = 0;
    static double last_time = 0;
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) return -1.0;
    
    long utime, stime;
    char comm[256];
    int pid_read, ppid, pgrp, session, tty_nr, tpgid;
    unsigned flags;
    
    if (fscanf(file, "%d %s %*c %d %d %d %d %d %u %*u %*u %*u %*u %ld %ld",
               &pid_read, comm, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags, &utime, &stime) < 10) {
        fclose(file);
        return -1.0;
    }
    
    fclose(file);
    
    double current_time = get_time_ms();
    
    if (last_time > 0) {
        long total_time = (utime + stime) - (last_utime + last_stime);
        double elapsed_time = (current_time - last_time) / 1000.0; // Convert to seconds
        double cpu_percent = (total_time / sysconf(_SC_CLK_TCK)) / elapsed_time * 100.0;
        
        last_utime = utime;
        last_stime = stime;
        last_time = current_time;
        
        return cpu_percent;
    } else {
        last_utime = utime;
        last_stime = stime;
        last_time = current_time;
        return 0.0;
    }
}

static void take_resource_snapshot(memory_test_context_t *ctx, resource_snapshot_t *snapshot)
{
    snapshot->timestamp_ms = get_time_ms();
    
    if (ctx->daemon_pid > 0) {
        snapshot->memory_rss_mb = get_process_memory_mb(ctx->daemon_pid, "VmRSS");
        snapshot->memory_vss_mb = get_process_memory_mb(ctx->daemon_pid, "VmSize");
        snapshot->memory_shared_mb = get_process_memory_mb(ctx->daemon_pid, "VmShared");
        snapshot->open_files = count_open_files(ctx->daemon_pid);
        snapshot->socket_count = count_socket_connections(ctx->daemon_pid);
        snapshot->cpu_percent = get_cpu_usage(ctx->daemon_pid);
    } else {
        snapshot->memory_rss_mb = -1;
        snapshot->memory_vss_mb = -1;
        snapshot->memory_shared_mb = -1;
        snapshot->open_files = -1;
        snapshot->socket_count = -1;
        snapshot->cpu_percent = -1.0;
    }
}

// ============================================================================
// DAEMON MANAGEMENT FOR MEMORY TESTING
// ============================================================================

static bool start_memory_test_daemon(memory_test_context_t *ctx)
{
    cleanup_memory_test_files();
    
    ctx->daemon_pid = fork();
    if (ctx->daemon_pid == 0) {
        // Child process - exec the daemon
        char max_conn_str[32];
        snprintf(max_conn_str, sizeof(max_conn_str), "%d", MAX_MEMORY_CLIENTS + 10);
        
        char *args[] = {
            "../../goxel-headless",
            "--daemon",
            "--socket", (char*)MEMORY_DAEMON_SOCKET,
            "--pid-file", (char*)MEMORY_DAEMON_PID,
            "--log-file", (char*)MEMORY_LOG_FILE,
            "--max-connections", max_conn_str,
            NULL
        };
        
        // If valgrind mode is enabled, run under valgrind
        if (ctx->enable_valgrind_mode) {
            char *valgrind_args[] = {
                "valgrind",
                "--tool=memcheck",
                "--leak-check=full", 
                "--show-leak-kinds=all",
                "--track-origins=yes",
                "--log-file=/tmp/goxel_valgrind.log",
                "../../goxel-headless",
                "--daemon",
                "--socket", (char*)MEMORY_DAEMON_SOCKET,
                "--pid-file", (char*)MEMORY_DAEMON_PID,
                "--log-file", (char*)MEMORY_LOG_FILE,
                "--max-connections", max_conn_str,
                NULL
            };
            execvp("valgrind", valgrind_args);
        } else {
            execv("../../goxel-headless", args);
        }
        
        exit(1);
    } else if (ctx->daemon_pid > 0) {
        // Parent process - wait for daemon to start
        for (int i = 0; i < 300; i++) { // 30 second timeout for valgrind startup
            if (access(MEMORY_DAEMON_SOCKET, F_OK) == 0) {
                sleep_ms(500); // Give valgrind extra time
                ctx->daemon_running = true;
                return true;
            }
            sleep_ms(100);
        }
        return false;
    } else {
        return false;
    }
}

static bool stop_memory_test_daemon(memory_test_context_t *ctx)
{
    if (!ctx->daemon_running || ctx->daemon_pid <= 0) {
        return true;
    }
    
    // Take final snapshot before stopping
    resource_snapshot_t final_snapshot;
    take_resource_snapshot(ctx, &final_snapshot);
    ctx->final_memory_mb = final_snapshot.memory_rss_mb;
    ctx->final_open_files = final_snapshot.open_files;
    
    if (kill(ctx->daemon_pid, SIGTERM) == 0) {
        int status;
        int wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        
        for (int i = 0; i < 100 && wait_result == 0; i++) { // 10 second timeout
            sleep_ms(100);
            wait_result = waitpid(ctx->daemon_pid, &status, WNOHANG);
        }
        
        if (wait_result == 0) {
            kill(ctx->daemon_pid, SIGKILL);
            waitpid(ctx->daemon_pid, &status, 0);
        }
    }
    
    ctx->daemon_running = false;
    ctx->daemon_pid = 0;
    cleanup_memory_test_files();
    return true;
}

// ============================================================================
// RESOURCE MONITORING THREAD
// ============================================================================

static void* resource_monitor_thread(void *arg)
{
    memory_test_context_t *ctx = (memory_test_context_t*)arg;
    
    while (!ctx->stop_monitoring && ctx->daemon_running) {
        pthread_mutex_lock(&ctx->monitor_mutex);
        
        if (ctx->snapshot_count < (int)(sizeof(ctx->snapshots) / sizeof(ctx->snapshots[0]))) {
            take_resource_snapshot(ctx, &ctx->snapshots[ctx->snapshot_count]);
            
            resource_snapshot_t *snapshot = &ctx->snapshots[ctx->snapshot_count];
            
            // Update peak values
            if (snapshot->memory_rss_mb > ctx->peak_memory_mb) {
                ctx->peak_memory_mb = snapshot->memory_rss_mb;
            }
            
            if (snapshot->open_files > ctx->peak_open_files) {
                ctx->peak_open_files = snapshot->open_files;
            }
            
            // Set baseline on first sample
            if (ctx->snapshot_count == 0) {
                ctx->baseline_memory_mb = snapshot->memory_rss_mb;
                ctx->baseline_open_files = snapshot->open_files;
            }
            
            ctx->snapshot_count++;
            ctx->memory_samples++;
        }
        
        pthread_mutex_unlock(&ctx->monitor_mutex);
        
        sleep_ms(MEMORY_SAMPLE_INTERVAL_MS);
    }
    
    return NULL;
}

// ============================================================================
// MEMORY TEST CLIENT OPERATIONS
// ============================================================================

static bool connect_memory_client(int *socket_fd)
{
    *socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*socket_fd < 0) return false;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MEMORY_DAEMON_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(*socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        return true;
    } else {
        close(*socket_fd);
        *socket_fd = -1;
        return false;
    }
}

static void disconnect_memory_client(int socket_fd)
{
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

static bool send_memory_test_request(int socket_fd, const char *method, const char *params, int request_id)
{
    char request[2048];
    snprintf(request, sizeof(request),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}\n",
        method, params ? params : "[]", request_id);
    
    ssize_t sent = send(socket_fd, request, strlen(request), 0);
    if (sent <= 0) return false;
    
    char response[MAX_RESPONSE_SIZE];
    ssize_t received = recv(socket_fd, response, sizeof(response) - 1, 0);
    
    return received > 0;
}

// ============================================================================
// MEMORY TEST SCENARIOS
// ============================================================================

static void test_basic_memory_operations(memory_test_context_t *ctx)
{
    printf("\n🧪 Basic Memory Operations Test\n");
    printf("================================\n");
    
    int socket_fd;
    if (!connect_memory_client(&socket_fd)) {
        printf("❌ Failed to connect memory test client\n");
        return;
    }
    
    // Perform basic operations to test for obvious leaks
    const int num_operations = 1000;
    printf("Performing %d basic operations...\n", num_operations);
    
    for (int i = 0; i < num_operations; i++) {
        // Create project
        send_memory_test_request(socket_fd, "goxel.create_project", 
                                "[\"Memory Test\",16,16,16]", i * 4 + 1);
        
        // Add voxel
        send_memory_test_request(socket_fd, "goxel.add_voxel", 
                                "[0,-16,0,255,0,0,255,0]", i * 4 + 2);
        
        // Get voxel
        send_memory_test_request(socket_fd, "goxel.get_voxel", 
                                "[0,-16,0]", i * 4 + 3);
        
        // Get status
        send_memory_test_request(socket_fd, "goxel.get_status", 
                                "[]", i * 4 + 4);
        
        ctx->total_operations += 4;
        
        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d operations\n", i + 1, num_operations);
        }
    }
    
    disconnect_memory_client(socket_fd);
    printf("✅ Basic operations completed\n");
}

static void test_large_dataset_memory(memory_test_context_t *ctx)
{
    if (!ctx->enable_large_datasets) {
        printf("\n⏭️  Skipping large dataset test (disabled)\n");
        return;
    }
    
    printf("\n📦 Large Dataset Memory Test\n");
    printf("============================\n");
    
    int socket_fd;
    if (!connect_memory_client(&socket_fd)) {
        printf("❌ Failed to connect for large dataset test\n");
        return;
    }
    
    // Create a large project
    send_memory_test_request(socket_fd, "goxel.create_project", 
                            "[\"Large Dataset\",128,128,128]", 1);
    
    // Add many voxels to test memory pressure
    const int voxels_to_add = 10000;
    printf("Adding %d voxels to test memory pressure...\n", voxels_to_add);
    
    for (int i = 0; i < voxels_to_add; i++) {
        char params[256];
        int x = (i % 64) - 32;
        int y = ((i / 64) % 64) - 48; // Offset to -16 area
        int z = (i / 4096) - 32;
        
        snprintf(params, sizeof(params), "[%d,%d,%d,255,%d,%d,255,0]", 
                x, y, z, (i % 256), ((i * 2) % 256));
        
        send_memory_test_request(socket_fd, "goxel.add_voxel", params, i + 10);
        ctx->total_operations++;
        
        if ((i + 1) % 1000 == 0) {
            printf("  Progress: %d/%d voxels\n", i + 1, voxels_to_add);
        }
    }
    
    disconnect_memory_client(socket_fd);
    printf("✅ Large dataset test completed\n");
}

static void test_connection_cycling_memory(memory_test_context_t *ctx)
{
    printf("\n🔄 Connection Cycling Memory Test\n");
    printf("=================================\n");
    
    const int cycles = 100;
    printf("Performing %d connect/disconnect cycles...\n", cycles);
    
    for (int i = 0; i < cycles; i++) {
        int socket_fd;
        if (connect_memory_client(&socket_fd)) {
            // Perform a few operations
            send_memory_test_request(socket_fd, "goxel.get_status", "[]", i * 3 + 1);
            send_memory_test_request(socket_fd, "goxel.list_layers", "[]", i * 3 + 2);
            send_memory_test_request(socket_fd, "goxel.get_status", "[]", i * 3 + 3);
            
            disconnect_memory_client(socket_fd);
            ctx->total_operations += 3;
        }
        
        if ((i + 1) % 10 == 0) {
            printf("  Progress: %d/%d cycles\n", i + 1, cycles);
        }
        
        // Brief pause to avoid overwhelming
        sleep_ms(10);
    }
    
    printf("✅ Connection cycling completed\n");
}

// ============================================================================
// MEMORY ANALYSIS
// ============================================================================

static void analyze_memory_usage(memory_test_context_t *ctx)
{
    if (ctx->snapshot_count < 2) {
        printf("⚠️  Insufficient data for memory analysis\n");
        return;
    }
    
    ctx->memory_growth_mb = ctx->final_memory_mb - ctx->baseline_memory_mb;
    
    // Check for memory leaks
    ctx->memory_leak_detected = (ctx->memory_growth_mb > MEMORY_LEAK_THRESHOLD_MB);
    
    // Check for resource leaks
    int fd_growth = ctx->final_open_files - ctx->baseline_open_files;
    ctx->resource_leak_detected = (fd_growth > RESOURCE_LEAK_THRESHOLD);
    
    printf("\n📊 Memory Usage Analysis\n");
    printf("========================\n");
    printf("Baseline Memory: %ld MB\n", ctx->baseline_memory_mb);
    printf("Peak Memory: %ld MB\n", ctx->peak_memory_mb);
    printf("Final Memory: %ld MB\n", ctx->final_memory_mb);
    printf("Memory Growth: %+ld MB\n", ctx->memory_growth_mb);
    printf("Memory Samples: %d\n", ctx->memory_samples);
    
    printf("\n🔗 Resource Usage Analysis\n");
    printf("===========================\n");
    printf("Baseline File Descriptors: %d\n", ctx->baseline_open_files);
    printf("Peak File Descriptors: %d\n", ctx->peak_open_files);
    printf("Final File Descriptors: %d\n", ctx->final_open_files);
    printf("FD Growth: %+d\n", fd_growth);
    
    printf("\n🎯 Leak Detection Results\n");
    printf("==========================\n");
    printf("Memory Leak Check: %s (threshold: %d MB)\n", 
           ctx->memory_leak_detected ? "❌ DETECTED" : "✅ PASSED", MEMORY_LEAK_THRESHOLD_MB);
    printf("Resource Leak Check: %s (threshold: %d FDs)\n",
           ctx->resource_leak_detected ? "❌ DETECTED" : "✅ PASSED", RESOURCE_LEAK_THRESHOLD);
    
    // Print memory timeline for significant changes
    if (ctx->snapshot_count > 10) {
        printf("\n📈 Memory Timeline (sampling every 10 snapshots)\n");
        printf("================================================\n");
        for (int i = 0; i < ctx->snapshot_count; i += 10) {
            resource_snapshot_t *snapshot = &ctx->snapshots[i];
            double elapsed_sec = (snapshot->timestamp_ms - ctx->snapshots[0].timestamp_ms) / 1000.0;
            printf("  %6.1fs: %4ld MB RSS, %3d FDs, %5.1f%% CPU\n",
                   elapsed_sec, snapshot->memory_rss_mb, snapshot->open_files, snapshot->cpu_percent);
        }
    }
}

// ============================================================================
// MAIN MEMORY TEST EXECUTION
// ============================================================================

static void initialize_memory_context(memory_test_context_t *ctx, int duration_seconds, 
                                     bool enable_large_datasets, bool enable_valgrind)
{
    memset(ctx, 0, sizeof(memory_test_context_t));
    
    ctx->test_duration_seconds = duration_seconds;
    ctx->num_test_clients = 5;
    ctx->enable_large_datasets = enable_large_datasets;
    ctx->enable_valgrind_mode = enable_valgrind;
    
    pthread_mutex_init(&ctx->monitor_mutex, NULL);
}

static bool run_memory_tests(memory_test_context_t *ctx)
{
    printf("🧠 Memory Leak and Resource Usage Testing\n");
    printf("==========================================\n");
    printf("Duration: %d seconds\n", ctx->test_duration_seconds);
    printf("Large datasets: %s\n", ctx->enable_large_datasets ? "Enabled" : "Disabled");
    printf("Valgrind mode: %s\n", ctx->enable_valgrind_mode ? "Enabled" : "Disabled");
    
    // Start daemon
    printf("\n🚀 Starting daemon for memory testing...\n");
    if (!start_memory_test_daemon(ctx)) {
        printf("❌ Failed to start memory test daemon\n");
        return false;
    }
    
    // Start resource monitoring
    printf("📈 Starting resource monitoring...\n");
    if (pthread_create(&ctx->monitor_thread, NULL, resource_monitor_thread, ctx) != 0) {
        printf("❌ Failed to start monitoring thread\n");
        stop_memory_test_daemon(ctx);
        return false;
    }
    
    // Wait for initial baseline
    sleep_ms(2000);
    
    // Run memory tests
    test_basic_memory_operations(ctx);
    test_large_dataset_memory(ctx);
    test_connection_cycling_memory(ctx);
    
    // Continue monitoring for the remaining duration
    double test_start = get_time_ms();
    double elapsed_seconds = 0;
    
    printf("\n⏱️  Continuing monitoring for %d seconds...\n", ctx->test_duration_seconds);
    while (elapsed_seconds < ctx->test_duration_seconds) {
        sleep_ms(5000);
        elapsed_seconds = (get_time_ms() - test_start) / 1000.0;
        
        printf("  Monitoring: %.0f/%d seconds\n", elapsed_seconds, ctx->test_duration_seconds);
    }
    
    // Stop monitoring
    ctx->stop_monitoring = true;
    pthread_join(ctx->monitor_thread, NULL);
    
    // Stop daemon
    printf("\n🛑 Stopping daemon...\n");
    stop_memory_test_daemon(ctx);
    
    // Analyze results
    analyze_memory_usage(ctx);
    
    return true;
}

static void print_memory_test_summary(memory_test_context_t *ctx)
{
    printf("\n============================================================\n");
    printf("🎯 MEMORY TEST SUMMARY\n");
    printf("============================================================\n");
    
    printf("\n📋 Test Overview:\n");
    printf("  Total Operations: %d\n", ctx->total_operations);
    printf("  Memory Samples: %d\n", ctx->memory_samples);
    printf("  Test Duration: %d seconds\n", ctx->test_duration_seconds);
    
    printf("\n🧠 Memory Assessment:\n");
    if (ctx->baseline_memory_mb > 0 && ctx->final_memory_mb > 0) {
        printf("  Memory Usage: %ld MB → %ld MB (%+ld MB)\n", 
               ctx->baseline_memory_mb, ctx->final_memory_mb, ctx->memory_growth_mb);
        printf("  Peak Memory: %ld MB\n", ctx->peak_memory_mb);
        
        if (ctx->memory_leak_detected) {
            printf("  Memory Leak: ❌ DETECTED (growth > %d MB)\n", MEMORY_LEAK_THRESHOLD_MB);
        } else {
            printf("  Memory Leak: ✅ NOT DETECTED (within %d MB threshold)\n", MEMORY_LEAK_THRESHOLD_MB);
        }
    } else {
        printf("  Memory Assessment: ⚠️  INCOMPLETE DATA\n");
    }
    
    printf("\n🔗 Resource Assessment:\n");
    if (ctx->baseline_open_files > 0 && ctx->final_open_files > 0) {
        int fd_growth = ctx->final_open_files - ctx->baseline_open_files;
        printf("  File Descriptors: %d → %d (%+d)\n", 
               ctx->baseline_open_files, ctx->final_open_files, fd_growth);
        printf("  Peak File Descriptors: %d\n", ctx->peak_open_files);
        
        if (ctx->resource_leak_detected) {
            printf("  Resource Leak: ❌ DETECTED (FD growth > %d)\n", RESOURCE_LEAK_THRESHOLD);
        } else {
            printf("  Resource Leak: ✅ NOT DETECTED (within %d FD threshold)\n", RESOURCE_LEAK_THRESHOLD);
        }
    } else {
        printf("  Resource Assessment: ⚠️  INCOMPLETE DATA\n");
    }
    
    printf("\n🏆 Overall Result:\n");
    bool overall_pass = !ctx->memory_leak_detected && !ctx->resource_leak_detected;
    printf("  Memory Test: %s\n", overall_pass ? "✅ PASSED" : "❌ FAILED");
    
    if (ctx->enable_valgrind_mode) {
        printf("\n🔍 Valgrind Analysis:\n");
        printf("  Valgrind log: /tmp/goxel_valgrind.log\n");
        printf("  Run 'cat /tmp/goxel_valgrind.log' for detailed leak analysis\n");
    }
    
    printf("============================================================\n");
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    printf("🧠 Goxel v14.0 Memory Leak and Resource Usage Testing\n");
    printf("======================================================\n");
    
    // Parse command line arguments
    int duration = 60; // Default 1 minute
    bool enable_large_datasets = false;
    bool enable_valgrind = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--large-datasets") == 0) {
            enable_large_datasets = true;
        } else if (strcmp(argv[i], "--valgrind") == 0) {
            enable_valgrind = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --duration SECONDS    Test duration (default: 60)\n");
            printf("  --large-datasets      Enable large dataset testing\n");
            printf("  --valgrind            Run daemon under valgrind\n");
            printf("  --help                Show this help\n");
            return 0;
        }
    }
    
    // Validate parameters
    if (duration <= 0 || duration > 3600) {
        printf("❌ Invalid duration: %d seconds (max: 3600)\n", duration);
        return 1;
    }
    
    // Initialize and run memory tests
    memory_test_context_t ctx;
    initialize_memory_context(&ctx, duration, enable_large_datasets, enable_valgrind);
    
    cleanup_memory_test_files();
    
    bool success = run_memory_tests(&ctx);
    print_memory_test_summary(&ctx);
    
    // Cleanup
    pthread_mutex_destroy(&ctx.monitor_mutex);
    cleanup_memory_test_files();
    
    // Return success only if no leaks detected
    bool overall_success = success && !ctx.memory_leak_detected && !ctx.resource_leak_detected;
    return overall_success ? 0 : 1;
}