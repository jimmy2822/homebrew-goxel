/*
 * Goxel v14.0 Concurrent Client Performance Test
 * 
 * This test validates the daemon's ability to handle multiple simultaneous
 * clients efficiently, measuring throughput, latency, and resource usage
 * under concurrent load.
 * 
 * Target: Support for 10+ concurrent clients without degradation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

// macOS doesn't have pthread_barrier, so we implement a simple version
#ifdef __APPLE__
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripcount;
} pthread_barrier_t;

static int pthread_barrier_init(pthread_barrier_t *barrier, void *attr, unsigned int count) {
    barrier->count = 0;
    barrier->tripcount = count;
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    return 0;
}

static int pthread_barrier_wait(pthread_barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count >= barrier->tripcount) {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    } else {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

static int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================

#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define MAX_CLIENTS 100
#define MAX_OPERATIONS_PER_CLIENT 1000
#define DEFAULT_NUM_CLIENTS 10
#define DEFAULT_OPERATIONS 100
#define DEFAULT_DURATION_SEC 30

// Test operation types
typedef enum {
    OP_PING,
    OP_CREATE_PROJECT,
    OP_ADD_VOXEL,
    OP_GET_VOXEL,
    OP_REMOVE_VOXEL,
    OP_EXPORT_MESH,
    OP_GET_STATUS,
    OP_TYPE_COUNT
} operation_type_t;

// Operation templates
static const char *operation_templates[] = {
    "{\"method\":\"ping\"}",
    "{\"method\":\"create_project\",\"params\":{\"name\":\"test_%d\"}}",
    "{\"method\":\"add_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d,\"color\":[255,0,0,255]}}",
    "{\"method\":\"get_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
    "{\"method\":\"remove_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
    "{\"method\":\"export_mesh\",\"params\":{\"format\":\"obj\"}}",
    "{\"method\":\"get_status\"}"
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    int client_id;
    double latency_ms;
    int success;
    operation_type_t op_type;
    struct timespec timestamp;
} operation_result_t;

typedef struct {
    int client_id;
    int num_operations;
    int socket_fd;
    pthread_t thread_id;
    
    // Results
    operation_result_t *results;
    int result_count;
    int success_count;
    int failure_count;
    
    // Timing
    double total_time_ms;
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    
    // Synchronization
    pthread_mutex_t *results_mutex;
    pthread_barrier_t *start_barrier;
    int *running;
} client_context_t;

typedef struct {
    int num_clients;
    int operations_per_client;
    int duration_sec;
    
    // Global results
    int total_operations;
    int total_successes;
    int total_failures;
    double test_duration_ms;
    
    // Per-client results
    client_context_t *clients;
    
    // Aggregated metrics
    double avg_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    double throughput_ops_sec;
    double success_rate;
    
    // Latency distribution
    double p50_latency_ms;
    double p90_latency_ms;
    double p95_latency_ms;
    double p99_latency_ms;
} concurrency_test_result_t;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
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

static int send_operation(int sockfd, operation_type_t op_type, int client_id,
                         double *latency_ms)
{
    char request[512];
    char response[4096];
    double start_time, end_time;
    ssize_t bytes_sent, bytes_received;
    
    // Generate request based on operation type
    switch (op_type) {
        case OP_CREATE_PROJECT:
            snprintf(request, sizeof(request), operation_templates[op_type], client_id);
            break;
        case OP_ADD_VOXEL:
        case OP_GET_VOXEL:
        case OP_REMOVE_VOXEL:
            // Generate random coordinates
            snprintf(request, sizeof(request), operation_templates[op_type],
                    rand() % 100, rand() % 100, rand() % 100);
            break;
        default:
            strcpy(request, operation_templates[op_type]);
            break;
    }
    
    start_time = get_time_ms();
    
    bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent != (ssize_t)strlen(request)) {
        *latency_ms = -1;
        return 0;
    }
    
    bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    end_time = get_time_ms();
    
    *latency_ms = end_time - start_time;
    
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        // Simple success check - look for error in response
        if (strstr(response, "\"error\"") == NULL) {
            return 1;
        }
    }
    
    return 0;
}

// ============================================================================
// CLIENT WORKER THREAD
// ============================================================================

static void* client_worker(void *arg)
{
    client_context_t *ctx = (client_context_t*)arg;
    int operations_completed = 0;
    double start_time, current_time;
    
    // Allocate results buffer
    ctx->results = calloc(ctx->num_operations, sizeof(operation_result_t));
    if (!ctx->results) {
        fprintf(stderr, "Client %d: Failed to allocate results buffer\n", 
                ctx->client_id);
        return NULL;
    }
    
    // Connect to daemon
    ctx->socket_fd = connect_to_daemon();
    if (ctx->socket_fd == -1) {
        fprintf(stderr, "Client %d: Failed to connect to daemon\n", ctx->client_id);
        free(ctx->results);
        return NULL;
    }
    
    // Wait for all clients to be ready
    pthread_barrier_wait(ctx->start_barrier);
    
    start_time = get_time_ms();
    ctx->min_latency_ms = 999999.0;
    ctx->max_latency_ms = 0.0;
    
    // Execute operations
    while (*ctx->running && operations_completed < ctx->num_operations) {
        operation_result_t *result = &ctx->results[ctx->result_count];
        
        // Select random operation type
        result->op_type = rand() % OP_TYPE_COUNT;
        result->client_id = ctx->client_id;
        clock_gettime(CLOCK_MONOTONIC_RAW, &result->timestamp);
        
        // Execute operation
        result->success = send_operation(ctx->socket_fd, result->op_type, 
                                       ctx->client_id, &result->latency_ms);
        
        if (result->success) {
            ctx->success_count++;
            
            // Update latency stats
            if (result->latency_ms < ctx->min_latency_ms) {
                ctx->min_latency_ms = result->latency_ms;
            }
            if (result->latency_ms > ctx->max_latency_ms) {
                ctx->max_latency_ms = result->latency_ms;
            }
        } else {
            ctx->failure_count++;
        }
        
        ctx->result_count++;
        operations_completed++;
        
        // Small delay between operations to simulate realistic load
        usleep(rand() % 10000); // 0-10ms random delay
    }
    
    current_time = get_time_ms();
    ctx->total_time_ms = current_time - start_time;
    
    // Calculate average latency
    if (ctx->success_count > 0) {
        double sum = 0.0;
        for (int i = 0; i < ctx->result_count; i++) {
            if (ctx->results[i].success) {
                sum += ctx->results[i].latency_ms;
            }
        }
        ctx->avg_latency_ms = sum / ctx->success_count;
    }
    
    // Close connection
    close(ctx->socket_fd);
    
    return NULL;
}

// ============================================================================
// TEST EXECUTION
// ============================================================================

static int run_concurrency_test(int num_clients, int operations_per_client,
                               int duration_sec, concurrency_test_result_t *result)
{
    pthread_t *threads;
    pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_barrier_t start_barrier;
    int running = 1;
    double test_start, test_end;
    
    printf("Starting concurrency test:\n");
    printf("  Clients: %d\n", num_clients);
    printf("  Operations per client: %d\n", operations_per_client);
    printf("  Duration: %d seconds\n", duration_sec);
    
    // Initialize result structure
    memset(result, 0, sizeof(concurrency_test_result_t));
    result->num_clients = num_clients;
    result->operations_per_client = operations_per_client;
    result->duration_sec = duration_sec;
    
    // Allocate client contexts
    result->clients = calloc(num_clients, sizeof(client_context_t));
    threads = calloc(num_clients, sizeof(pthread_t));
    
    if (!result->clients || !threads) {
        fprintf(stderr, "Failed to allocate memory for test\n");
        return -1;
    }
    
    // Initialize barrier for synchronized start
    pthread_barrier_init(&start_barrier, NULL, num_clients + 1);
    
    // Initialize and start client threads
    for (int i = 0; i < num_clients; i++) {
        client_context_t *ctx = &result->clients[i];
        ctx->client_id = i;
        ctx->num_operations = operations_per_client;
        ctx->results_mutex = &results_mutex;
        ctx->start_barrier = &start_barrier;
        ctx->running = &running;
        
        if (pthread_create(&threads[i], NULL, client_worker, ctx) != 0) {
            fprintf(stderr, "Failed to create thread for client %d\n", i);
            // Clean up already created threads
            running = 0;
            pthread_barrier_wait(&start_barrier);
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(result->clients);
            free(threads);
            return -1;
        }
    }
    
    // Wait for all clients to be ready
    pthread_barrier_wait(&start_barrier);
    
    printf("\nAll clients connected. Starting test...\n");
    test_start = get_time_ms();
    
    // Run for specified duration
    if (duration_sec > 0) {
        sleep(duration_sec);
        running = 0;
    }
    
    // Wait for all clients to complete
    printf("Waiting for clients to complete...\n");
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    test_end = get_time_ms();
    result->test_duration_ms = test_end - test_start;
    
    // Cleanup
    pthread_barrier_destroy(&start_barrier);
    pthread_mutex_destroy(&results_mutex);
    free(threads);
    
    printf("Test completed.\n\n");
    
    return 0;
}

// ============================================================================
// RESULT ANALYSIS
// ============================================================================

static int compare_latencies(const void *a, const void *b)
{
    double la = *(const double *)a;
    double lb = *(const double *)b;
    return (la > lb) - (la < lb);
}

static void analyze_results(concurrency_test_result_t *result)
{
    double *all_latencies;
    int total_latency_count = 0;
    double sum_latency = 0.0;
    
    // Aggregate results from all clients
    result->total_operations = 0;
    result->total_successes = 0;
    result->total_failures = 0;
    result->min_latency_ms = 999999.0;
    result->max_latency_ms = 0.0;
    
    // Count total successful operations for latency array
    for (int i = 0; i < result->num_clients; i++) {
        client_context_t *client = &result->clients[i];
        for (int j = 0; j < client->result_count; j++) {
            if (client->results[j].success) {
                total_latency_count++;
            }
        }
    }
    
    // Allocate array for all latencies
    all_latencies = malloc(total_latency_count * sizeof(double));
    if (!all_latencies) {
        fprintf(stderr, "Failed to allocate latency array\n");
        return;
    }
    
    // Collect all data
    int latency_index = 0;
    for (int i = 0; i < result->num_clients; i++) {
        client_context_t *client = &result->clients[i];
        
        result->total_operations += client->result_count;
        result->total_successes += client->success_count;
        result->total_failures += client->failure_count;
        
        // Collect latencies
        for (int j = 0; j < client->result_count; j++) {
            if (client->results[j].success) {
                double lat = client->results[j].latency_ms;
                all_latencies[latency_index++] = lat;
                sum_latency += lat;
                
                if (lat < result->min_latency_ms) {
                    result->min_latency_ms = lat;
                }
                if (lat > result->max_latency_ms) {
                    result->max_latency_ms = lat;
                }
            }
        }
    }
    
    // Calculate metrics
    if (result->total_successes > 0) {
        result->avg_latency_ms = sum_latency / result->total_successes;
        result->success_rate = (double)result->total_successes / 
                              result->total_operations * 100.0;
        
        // Sort latencies for percentiles
        qsort(all_latencies, total_latency_count, sizeof(double), compare_latencies);
        
        // Calculate percentiles
        result->p50_latency_ms = all_latencies[total_latency_count / 2];
        result->p90_latency_ms = all_latencies[(int)(total_latency_count * 0.90)];
        result->p95_latency_ms = all_latencies[(int)(total_latency_count * 0.95)];
        result->p99_latency_ms = all_latencies[(int)(total_latency_count * 0.99)];
    }
    
    // Calculate throughput
    result->throughput_ops_sec = result->total_operations / 
                                (result->test_duration_ms / 1000.0);
    
    free(all_latencies);
}

// ============================================================================
// REPORT GENERATION
// ============================================================================

static void print_results(const concurrency_test_result_t *result)
{
    printf("=== Concurrency Test Results ===\n");
    printf("Test Configuration:\n");
    printf("  Concurrent Clients: %d\n", result->num_clients);
    printf("  Total Operations: %d\n", result->total_operations);
    printf("  Test Duration: %.2f seconds\n", result->test_duration_ms / 1000.0);
    
    printf("\nPerformance Metrics:\n");
    printf("  Throughput: %.1f ops/sec\n", result->throughput_ops_sec);
    printf("  Success Rate: %.1f%% (%d/%d)\n", 
           result->success_rate, result->total_successes, result->total_operations);
    
    printf("\nLatency Statistics:\n");
    printf("  Min: %.3f ms\n", result->min_latency_ms);
    printf("  Max: %.3f ms\n", result->max_latency_ms);
    printf("  Avg: %.3f ms\n", result->avg_latency_ms);
    printf("  P50: %.3f ms\n", result->p50_latency_ms);
    printf("  P90: %.3f ms\n", result->p90_latency_ms);
    printf("  P95: %.3f ms\n", result->p95_latency_ms);
    printf("  P99: %.3f ms\n", result->p99_latency_ms);
    
    printf("\nPer-Client Summary:\n");
    for (int i = 0; i < result->num_clients && i < 10; i++) {
        client_context_t *client = &result->clients[i];
        printf("  Client %d: %d ops, %.1f%% success, avg %.2f ms\n",
               client->client_id, client->result_count,
               (double)client->success_count / client->result_count * 100.0,
               client->avg_latency_ms);
    }
    if (result->num_clients > 10) {
        printf("  ... and %d more clients\n", result->num_clients - 10);
    }
    
    // Target evaluation
    printf("\nTarget Evaluation:\n");
    printf("  Latency Target (<2.1ms avg): ");
    if (result->avg_latency_ms <= 2.1) {
        printf("✅ PASS (%.3f ms)\n", result->avg_latency_ms);
    } else {
        printf("❌ FAIL (%.3f ms)\n", result->avg_latency_ms);
    }
    
    printf("  Throughput Target (>1000 ops/sec): ");
    if (result->throughput_ops_sec >= 1000) {
        printf("✅ PASS (%.1f ops/sec)\n", result->throughput_ops_sec);
    } else {
        printf("❌ FAIL (%.1f ops/sec)\n", result->throughput_ops_sec);
    }
    
    printf("  Concurrent Clients Target (>10): ");
    if (result->num_clients >= 10 && result->success_rate > 95) {
        printf("✅ PASS (%d clients, %.1f%% success)\n", 
               result->num_clients, result->success_rate);
    } else {
        printf("❌ FAIL\n");
    }
}

static void save_results_csv(const concurrency_test_result_t *result,
                            const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    // Write header
    fprintf(fp, "client_id,operation_id,op_type,success,latency_ms,timestamp\n");
    
    // Write all operation results
    for (int i = 0; i < result->num_clients; i++) {
        client_context_t *client = &result->clients[i];
        for (int j = 0; j < client->result_count; j++) {
            operation_result_t *op = &client->results[j];
            fprintf(fp, "%d,%d,%d,%d,%.3f,%.6f\n",
                   op->client_id, j, op->op_type, op->success,
                   op->latency_ms, 
                   op->timestamp.tv_sec + op->timestamp.tv_nsec / 1e9);
        }
    }
    
    fclose(fp);
    printf("\nDetailed results saved to: %s\n", filename);
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char *argv[])
{
    int num_clients = DEFAULT_NUM_CLIENTS;
    int operations = DEFAULT_OPERATIONS;
    int duration = 0;
    concurrency_test_result_t result;
    
    // Parse command line arguments
    if (argc > 1) {
        num_clients = atoi(argv[1]);
        if (num_clients < 1 || num_clients > MAX_CLIENTS) {
            fprintf(stderr, "Number of clients must be between 1 and %d\n", 
                    MAX_CLIENTS);
            return 1;
        }
    }
    
    if (argc > 2) {
        operations = atoi(argv[2]);
        if (operations < 1 || operations > MAX_OPERATIONS_PER_CLIENT) {
            fprintf(stderr, "Operations per client must be between 1 and %d\n",
                    MAX_OPERATIONS_PER_CLIENT);
            return 1;
        }
    }
    
    if (argc > 3) {
        duration = atoi(argv[3]);
    }
    
    printf("Goxel v14.0 Concurrent Client Performance Test\n");
    printf("=============================================\n\n");
    
    // Check if daemon is running
    int test_sock = connect_to_daemon();
    if (test_sock == -1) {
        fprintf(stderr, "Error: Cannot connect to daemon at %s\n", SOCKET_PATH);
        fprintf(stderr, "Please start the daemon first.\n");
        return 1;
    }
    close(test_sock);
    
    // Run the test
    if (run_concurrency_test(num_clients, operations, duration, &result) != 0) {
        fprintf(stderr, "Test execution failed\n");
        return 1;
    }
    
    // Analyze and print results
    analyze_results(&result);
    print_results(&result);
    
    // Save detailed results
    save_results_csv(&result, "concurrency_test_results.csv");
    
    // Cleanup
    for (int i = 0; i < result.num_clients; i++) {
        if (result.clients[i].results) {
            free(result.clients[i].results);
        }
    }
    free(result.clients);
    
    return 0;
}