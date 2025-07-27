/*
 * Goxel v14.0 Daemon Architecture - Memory Profiling Tool
 * 
 * This module provides comprehensive memory usage analysis for the daemon
 * including memory leaks detection, peak usage monitoring, and memory
 * efficiency metrics.
 * 
 * Target: <50MB daemon memory usage under normal load
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#ifdef __linux__
#include <sys/types.h>
#include <dirent.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/task_info.h>
#endif

#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define TARGET_MEMORY_MB 50
#define SAMPLE_INTERVAL_MS 100
#define MAX_SAMPLES 10000

typedef struct {
    double timestamp_ms;
    size_t rss_bytes;          // Resident Set Size
    size_t vms_bytes;          // Virtual Memory Size
    size_t heap_bytes;         // Heap memory (if available)
    int active_connections;
    int operations_in_progress;
} memory_sample_t;

typedef struct {
    memory_sample_t samples[MAX_SAMPLES];
    int count;
    size_t peak_rss_bytes;
    size_t peak_vms_bytes;
    size_t baseline_rss_bytes;
    size_t baseline_vms_bytes;
    double total_test_time_ms;
    int memory_leaks_detected;
} memory_profile_t;

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static pid_t find_daemon_pid(void)
{
    // In a real implementation, this would search for the actual daemon process
    // For testing, we'll simulate or use a known PID
    FILE *fp;
    char buffer[256];
    pid_t pid = 0;
    
    // Try to find the daemon process using pgrep
    fp = popen("pgrep -f goxel.*daemon", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            pid = atoi(buffer);
        }
        pclose(fp);
    }
    
    return pid;
}

#ifdef __linux__
static int get_process_memory_linux(pid_t pid, size_t *rss_bytes, size_t *vms_bytes)
{
    char path[256];
    FILE *fp;
    char line[512];
    int found_rss = 0, found_vms = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%zu", rss_bytes);
            *rss_bytes *= 1024; // Convert from KB to bytes
            found_rss = 1;
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%zu", vms_bytes);
            *vms_bytes *= 1024; // Convert from KB to bytes
            found_vms = 1;
        }
        
        if (found_rss && found_vms) break;
    }
    
    fclose(fp);
    return found_rss && found_vms;
}
#endif

#ifdef __APPLE__
static int get_process_memory_macos(pid_t pid, size_t *rss_bytes, size_t *vms_bytes)
{
    mach_port_t task;
    kern_return_t kr;
    task_basic_info_data_t info;
    mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
    
    kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        return 0;
    }
    
    kr = task_info(task, TASK_BASIC_INFO, (task_info_t)&info, &info_count);
    mach_port_deallocate(mach_task_self(), task);
    
    if (kr == KERN_SUCCESS) {
        *rss_bytes = info.resident_size;
        *vms_bytes = info.virtual_size;
        return 1;
    }
    
    return 0;
}
#endif

static int get_process_memory(pid_t pid, size_t *rss_bytes, size_t *vms_bytes)
{
    *rss_bytes = 0;
    *vms_bytes = 0;
    
    if (pid <= 0) {
        return 0;
    }
    
#ifdef __linux__
    return get_process_memory_linux(pid, rss_bytes, vms_bytes);
#elif defined(__APPLE__)
    return get_process_memory_macos(pid, rss_bytes, vms_bytes);
#else
    // Fallback: use ps command
    char cmd[256];
    FILE *fp;
    char buffer[512];
    
    snprintf(cmd, sizeof(cmd), "ps -p %d -o rss,vsz", pid);
    fp = popen(cmd, "r");
    if (!fp) return 0;
    
    // Skip header line
    fgets(buffer, sizeof(buffer), fp);
    
    if (fgets(buffer, sizeof(buffer), fp)) {
        unsigned long rss_kb, vsz_kb;
        if (sscanf(buffer, "%lu %lu", &rss_kb, &vsz_kb) == 2) {
            *rss_bytes = rss_kb * 1024;
            *vms_bytes = vsz_kb * 1024;
            pclose(fp);
            return 1;
        }
    }
    
    pclose(fp);
    return 0;
#endif
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

static void send_test_operation(void)
{
    int sockfd;
    const char *request = "{\"method\":\"add_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0,\"color\":[255,0,0,255]}}";
    char response[1024];
    
    sockfd = connect_to_daemon();
    if (sockfd != -1) {
        send(sockfd, request, strlen(request), 0);
        recv(sockfd, response, sizeof(response) - 1, 0);
        close(sockfd);
    }
}

static void collect_baseline_memory(pid_t daemon_pid, memory_profile_t *profile)
{
    size_t rss, vms;
    
    printf("Collecting baseline memory usage...\n");
    
    // Let daemon settle
    sleep(2);
    
    if (get_process_memory(daemon_pid, &rss, &vms)) {
        profile->baseline_rss_bytes = rss;
        profile->baseline_vms_bytes = vms;
        printf("Baseline Memory - RSS: %.2f MB, VMS: %.2f MB\n",
               rss / (1024.0 * 1024.0), vms / (1024.0 * 1024.0));
    } else {
        printf("Warning: Could not collect baseline memory\n");
        profile->baseline_rss_bytes = 0;
        profile->baseline_vms_bytes = 0;
    }
}

static void memory_monitoring_thread(pid_t daemon_pid, memory_profile_t *profile, 
                                   int duration_sec, volatile int *stop_flag)
{
    double start_time = get_time_ms();
    double last_sample = start_time;
    
    profile->count = 0;
    profile->peak_rss_bytes = 0;
    profile->peak_vms_bytes = 0;
    
    while (!(*stop_flag) && profile->count < MAX_SAMPLES) {
        double current_time = get_time_ms();
        
        if (current_time - last_sample >= SAMPLE_INTERVAL_MS) {
            memory_sample_t *sample = &profile->samples[profile->count];
            
            sample->timestamp_ms = current_time - start_time;
            sample->active_connections = 0; // Would need daemon API to get this
            sample->operations_in_progress = 0; // Would need daemon API to get this
            
            if (get_process_memory(daemon_pid, &sample->rss_bytes, &sample->vms_bytes)) {
                sample->heap_bytes = 0; // Not easily available without instrumentation
                
                if (sample->rss_bytes > profile->peak_rss_bytes) {
                    profile->peak_rss_bytes = sample->rss_bytes;
                }
                if (sample->vms_bytes > profile->peak_vms_bytes) {
                    profile->peak_vms_bytes = sample->vms_bytes;
                }
                
                profile->count++;
            }
            
            last_sample = current_time;
        }
        
        usleep(10000); // 10ms
        
        if (duration_sec > 0 && (current_time - start_time) >= duration_sec * 1000.0) {
            break;
        }
    }
    
    profile->total_test_time_ms = get_time_ms() - start_time;
}

static void generate_memory_load(int duration_sec)
{
    double start_time = get_time_ms();
    int operations = 0;
    
    printf("Generating memory load for %d seconds", duration_sec);
    fflush(stdout);
    
    while ((get_time_ms() - start_time) < duration_sec * 1000.0) {
        send_test_operation();
        operations++;
        
        if (operations % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
        
        usleep(1000); // 1ms between operations
    }
    
    printf(" done (%d operations).\n", operations);
}

static void detect_memory_leaks(memory_profile_t *profile)
{
    if (profile->count < 10) {
        profile->memory_leaks_detected = 0;
        return;
    }
    
    // Check if memory usage consistently increases over time
    size_t early_avg = 0, late_avg = 0;
    int early_count = profile->count / 4;
    int late_count = profile->count / 4;
    int i;
    
    // Average of first quarter
    for (i = 0; i < early_count; i++) {
        early_avg += profile->samples[i].rss_bytes;
    }
    early_avg /= early_count;
    
    // Average of last quarter
    for (i = profile->count - late_count; i < profile->count; i++) {
        late_avg += profile->samples[i].rss_bytes;
    }
    late_avg /= late_count;
    
    // If memory increased by more than 20% without stabilizing, flag as leak
    double increase_ratio = (double)late_avg / early_avg;
    profile->memory_leaks_detected = (increase_ratio > 1.2) ? 1 : 0;
}

static void print_memory_analysis(const memory_profile_t *profile)
{
    if (profile->count == 0) {
        printf("No memory samples collected.\n");
        return;
    }
    
    size_t final_rss = profile->samples[profile->count - 1].rss_bytes;
    size_t final_vms = profile->samples[profile->count - 1].vms_bytes;
    
    double peak_rss_mb = profile->peak_rss_bytes / (1024.0 * 1024.0);
    double peak_vms_mb = profile->peak_vms_bytes / (1024.0 * 1024.0);
    double final_rss_mb = final_rss / (1024.0 * 1024.0);
    double final_vms_mb = final_vms / (1024.0 * 1024.0);
    
    printf("\n=== MEMORY PROFILING RESULTS ===\n");
    printf("Test Duration: %.2f seconds\n", profile->total_test_time_ms / 1000.0);
    printf("Memory Samples: %d\n", profile->count);
    printf("\nMemory Usage:\n");
    printf("  Peak RSS: %.2f MB\n", peak_rss_mb);
    printf("  Peak VMS: %.2f MB\n", peak_vms_mb);
    printf("  Final RSS: %.2f MB\n", final_rss_mb);
    printf("  Final VMS: %.2f MB\n", final_vms_mb);
    
    if (profile->baseline_rss_bytes > 0) {
        double baseline_mb = profile->baseline_rss_bytes / (1024.0 * 1024.0);
        double growth_mb = final_rss_mb - baseline_mb;
        printf("  Baseline RSS: %.2f MB\n", baseline_mb);
        printf("  Memory Growth: %.2f MB\n", growth_mb);
    }
    
    printf("\nPerformance Assessment:\n");
    printf("  Target Memory: <%d MB\n", TARGET_MEMORY_MB);
    printf("  Peak Memory Test: %s\n", 
           (peak_rss_mb < TARGET_MEMORY_MB) ? "PASS" : "FAIL");
    printf("  Memory Leaks: %s\n", 
           profile->memory_leaks_detected ? "DETECTED" : "NONE");
    
    // Memory efficiency grade
    const char *grade;
    if (peak_rss_mb < TARGET_MEMORY_MB * 0.5) {
        grade = "EXCELLENT";
    } else if (peak_rss_mb < TARGET_MEMORY_MB * 0.8) {
        grade = "GOOD";
    } else if (peak_rss_mb < TARGET_MEMORY_MB) {
        grade = "ACCEPTABLE";
    } else {
        grade = "POOR";
    }
    
    printf("  Memory Efficiency: %s\n", grade);
    printf("\n");
}

static void export_memory_data(const memory_profile_t *profile, const char *filename)
{
    FILE *fp;
    int i;
    
    fp = fopen(filename, "w");
    if (!fp) {
        printf("Warning: Could not export memory data to %s\n", filename);
        return;
    }
    
    fprintf(fp, "# Goxel v14.0 Daemon Memory Profile Data\n");
    fprintf(fp, "# Time(ms),RSS(bytes),VMS(bytes)\n");
    
    for (i = 0; i < profile->count; i++) {
        fprintf(fp, "%.2f,%zu,%zu\n",
                profile->samples[i].timestamp_ms,
                profile->samples[i].rss_bytes,
                profile->samples[i].vms_bytes);
    }
    
    fclose(fp);
    printf("Memory data exported to: %s\n", filename);
}

int main(int argc, char *argv[])
{
    int test_duration = 30;
    pid_t daemon_pid;
    memory_profile_t profile = {0};
    volatile int stop_flag = 0;
    
    if (argc > 1) {
        test_duration = atoi(argv[1]);
        if (test_duration <= 0 || test_duration > 300) {
            fprintf(stderr, "Invalid test duration. Using default: 30 seconds\n");
            test_duration = 30;
        }
    }
    
    printf("Goxel v14.0 Daemon Memory Profiling\n");
    printf("===================================\n");
    printf("Target: <%d MB memory usage\n", TARGET_MEMORY_MB);
    printf("Test Duration: %d seconds\n\n", test_duration);
    
    daemon_pid = find_daemon_pid();
    if (daemon_pid <= 0) {
        printf("Warning: Could not find daemon process. Using mock PID for testing.\n");
        daemon_pid = getpid(); // Use our own process as a fallback for testing
    } else {
        printf("Monitoring daemon PID: %d\n", daemon_pid);
    }
    
    collect_baseline_memory(daemon_pid, &profile);
    
    printf("Starting memory monitoring and load generation...\n");
    
    // Start memory monitoring in the background
    if (fork() == 0) {
        // Child process: monitor memory
        memory_monitoring_thread(daemon_pid, &profile, test_duration, &stop_flag);
        exit(0);
    } else {
        // Parent process: generate load
        generate_memory_load(test_duration);
        stop_flag = 1;
        wait(NULL); // Wait for monitoring thread to finish
    }
    
    detect_memory_leaks(&profile);
    print_memory_analysis(&profile);
    export_memory_data(&profile, "daemon_memory_profile.csv");
    
    return (profile.peak_rss_bytes < TARGET_MEMORY_MB * 1024 * 1024) ? 0 : 1;
}