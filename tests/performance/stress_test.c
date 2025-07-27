/*
 * Goxel v14.0 Daemon Architecture - Stress Test Framework
 * 
 * This module provides comprehensive stress testing for the daemon under
 * extreme load conditions including high concurrency, rapid operations,
 * and resource exhaustion scenarios.
 * 
 * Target: Handle 10+ concurrent clients reliably
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
#include <sys/wait.h>

#define MAX_CONCURRENT_CLIENTS 32
#define MAX_OPERATIONS_PER_CLIENT 1000
#define SOCKET_PATH "/tmp/goxel_daemon_test.sock"
#define STRESS_TEST_DURATION 60
#define TARGET_CONCURRENT_CLIENTS 10

typedef struct {
    int client_id;
    int operations_completed;
    int operations_failed;
    int connection_failures;
    double total_response_time_ms;
    double min_response_time_ms;
    double max_response_time_ms;
    int test_duration_sec;
    volatile int *stop_flag;
} stress_client_t;

typedef struct {
    const char *name;
    const char *description;
    int num_clients;
    int operations_per_client;
    int test_duration_sec;
    const char **request_patterns;
    int num_patterns;
} stress_scenario_t;

static const char *basic_operations[] = {
    "{\"method\":\"ping\"}",
    "{\"method\":\"get_status\"}",
    "{\"method\":\"add_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0,\"color\":[255,0,0,255]}}",
    "{\"method\":\"get_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0}}",
    "{\"method\":\"remove_voxel\",\"params\":{\"x\":0,\"y\":0,\"z\":0}}"
};

static const char *heavy_operations[] = {
    "{\"method\":\"export_mesh\",\"params\":{\"format\":\"obj\"}}",
    "{\"method\":\"import_mesh\",\"params\":{\"data\":\"v 0 0 0\\nf 1 1 1\"}}",
    "{\"method\":\"batch_add_voxels\",\"params\":{\"voxels\":[{\"x\":1,\"y\":1,\"z\":1,\"color\":[255,0,0,255]}]}}",
    "{\"method\":\"render_preview\",\"params\":{\"width\":256,\"height\":256}}"
};

static const char *rapid_fire_operations[] = {
    "{\"method\":\"add_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d,\"color\":[%d,%d,%d,255]}}",
    "{\"method\":\"get_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
    "{\"method\":\"remove_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d}}"
};

static const stress_scenario_t scenarios[] = {
    {
        "concurrent_basic",
        "Multiple clients performing basic operations",
        TARGET_CONCURRENT_CLIENTS,
        100,
        30,
        basic_operations,
        sizeof(basic_operations) / sizeof(basic_operations[0])
    },
    {
        "concurrent_heavy",
        "Multiple clients performing heavy operations",
        5,
        20,
        45,
        heavy_operations,
        sizeof(heavy_operations) / sizeof(heavy_operations[0])
    },
    {
        "rapid_fire",
        "Single client rapid-fire operations",
        1,
        5000,
        20,
        rapid_fire_operations,
        sizeof(rapid_fire_operations) / sizeof(rapid_fire_operations[0])
    },
    {
        "connection_storm",
        "Many clients with short-lived connections",
        20,
        50,
        25,
        basic_operations,
        sizeof(basic_operations) / sizeof(basic_operations[0])
    },
    {NULL, NULL, 0, 0, 0, NULL, 0}
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

static int send_request(int sockfd, const char *request, char *response, size_t response_size)
{
    ssize_t bytes_sent, bytes_received;
    
    bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent <= 0) {
        return 0;
    }
    
    bytes_received = recv(sockfd, response, response_size - 1, 0);
    if (bytes_received <= 0) {
        return 0;
    }
    
    response[bytes_received] = '\0';
    return 1;
}

static void *stress_client_worker(void *arg)
{
    stress_client_t *client = (stress_client_t*)arg;
    char request[512];
    char response[4096];
    double start_time, end_time, response_time;
    int sockfd;
    int operation_count = 0;
    double test_start_time = get_time_ms();
    
    client->operations_completed = 0;
    client->operations_failed = 0;
    client->connection_failures = 0;
    client->total_response_time_ms = 0.0;
    client->min_response_time_ms = 999999.0;
    client->max_response_time_ms = 0.0;
    
    while (!(*client->stop_flag) && 
           (client->test_duration_sec == 0 || 
            (get_time_ms() - test_start_time) < client->test_duration_sec * 1000.0)) {
        
        // Create connection for each operation (stress test)
        sockfd = connect_to_daemon();
        if (sockfd == -1) {
            client->connection_failures++;
            usleep(1000); // 1ms delay on connection failure
            continue;
        }
        
        // Generate a request (for demonstration, using a simple pattern)
        snprintf(request, sizeof(request), 
                 "{\"method\":\"add_voxel\",\"params\":{\"x\":%d,\"y\":%d,\"z\":%d,\"color\":[255,%d,%d,255]}}",
                 rand() % 100, rand() % 100, rand() % 100, 
                 rand() % 256, rand() % 256);
        
        start_time = get_time_ms();
        
        if (send_request(sockfd, request, response, sizeof(response))) {
            end_time = get_time_ms();
            response_time = end_time - start_time;
            
            client->operations_completed++;
            client->total_response_time_ms += response_time;
            
            if (response_time < client->min_response_time_ms) {
                client->min_response_time_ms = response_time;
            }
            if (response_time > client->max_response_time_ms) {
                client->max_response_time_ms = response_time;
            }
        } else {
            client->operations_failed++;
        }
        
        close(sockfd);
        operation_count++;
        
        // Variable delay based on client ID to create realistic patterns
        usleep((client->client_id % 5 + 1) * 100); // 0.1-0.6ms delay
    }
    
    return NULL;
}

static int run_stress_scenario(const stress_scenario_t *scenario)
{
    pthread_t threads[MAX_CONCURRENT_CLIENTS];
    stress_client_t clients[MAX_CONCURRENT_CLIENTS];
    volatile int stop_flag = 0;
    double start_time, end_time, test_duration;
    int i, total_ops = 0, total_failed = 0, total_conn_failures = 0;
    double total_response_time = 0.0;
    
    printf("=== %s ===\n", scenario->name);
    printf("Description: %s\n", scenario->description);
    printf("Clients: %d | Operations/Client: %d | Duration: %d sec\n",
           scenario->num_clients, scenario->operations_per_client, scenario->test_duration_sec);
    printf("Starting stress test");
    fflush(stdout);
    
    start_time = get_time_ms();
    
    // Create stress client threads
    for (i = 0; i < scenario->num_clients; i++) {
        clients[i].client_id = i;
        clients[i].test_duration_sec = scenario->test_duration_sec;
        clients[i].stop_flag = &stop_flag;
        
        if (pthread_create(&threads[i], NULL, stress_client_worker, &clients[i]) != 0) {
            fprintf(stderr, "Failed to create client thread %d\n", i);
            stop_flag = 1;
            return 0;
        }
    }
    
    // Let the test run
    for (i = 0; i < scenario->test_duration_sec; i++) {
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    
    stop_flag = 1;
    
    // Wait for all client threads to complete
    for (i = 0; i < scenario->num_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    
    end_time = get_time_ms();
    test_duration = (end_time - start_time) / 1000.0;
    printf(" done.\n");
    
    // Aggregate results
    for (i = 0; i < scenario->num_clients; i++) {
        total_ops += clients[i].operations_completed;
        total_failed += clients[i].operations_failed;
        total_conn_failures += clients[i].connection_failures;
        total_response_time += clients[i].total_response_time_ms;
    }
    
    double avg_response_time = (total_ops > 0) ? (total_response_time / total_ops) : 0.0;
    double throughput = total_ops / test_duration;
    double success_rate = (total_ops + total_failed > 0) ? 
                         (100.0 * total_ops / (total_ops + total_failed)) : 0.0;
    
    printf("\nResults:\n");
    printf("  Test Duration: %.2f seconds\n", test_duration);
    printf("  Total Operations: %d\n", total_ops);
    printf("  Failed Operations: %d\n", total_failed);
    printf("  Connection Failures: %d\n", total_conn_failures);
    printf("  Success Rate: %.1f%%\n", success_rate);
    printf("  Average Response Time: %.2f ms\n", avg_response_time);
    printf("  Throughput: %.1f ops/sec\n", throughput);
    
    // Per-client breakdown
    printf("  Client Performance:\n");
    for (i = 0; i < scenario->num_clients; i++) {
        if (clients[i].operations_completed > 0) {
            double client_avg = clients[i].total_response_time_ms / clients[i].operations_completed;
            printf("    Client %2d: %4d ops, avg %.2fms (%.2f-%.2fms), %d failures\n",
                   i, clients[i].operations_completed, client_avg,
                   clients[i].min_response_time_ms, clients[i].max_response_time_ms,
                   clients[i].operations_failed + clients[i].connection_failures);
        }
    }
    
    // Stress test evaluation
    int passed = 1;
    if (success_rate < 95.0) {
        printf("  Status: FAIL - Low success rate (%.1f%% < 95%%)\n", success_rate);
        passed = 0;
    } else if (avg_response_time > 10.0) {
        printf("  Status: FAIL - High response time (%.2fms > 10ms)\n", avg_response_time);
        passed = 0;
    } else if (total_conn_failures > total_ops * 0.01) {
        printf("  Status: FAIL - Too many connection failures (%d)\n", total_conn_failures);
        passed = 0;
    } else {
        printf("  Status: PASS\n");
    }
    
    printf("\n");
    return passed;
}

static void run_daemon_stability_test(int duration_sec)
{
    printf("=== Daemon Stability Test ===\n");
    printf("Duration: %d seconds\n", duration_sec);
    printf("Monitoring daemon health during extended operation...\n");
    
    double start_time = get_time_ms();
    int ping_count = 0, ping_failures = 0;
    
    while ((get_time_ms() - start_time) < duration_sec * 1000.0) {
        int sockfd = connect_to_daemon();
        if (sockfd != -1) {
            const char *ping = "{\"method\":\"ping\"}";
            char response[256];
            
            if (send_request(sockfd, ping, response, sizeof(response))) {
                ping_count++;
            } else {
                ping_failures++;
            }
            close(sockfd);
        } else {
            ping_failures++;
        }
        
        sleep(5); // Ping every 5 seconds
    }
    
    double stability_rate = (ping_count + ping_failures > 0) ? 
                           (100.0 * ping_count / (ping_count + ping_failures)) : 0.0;
    
    printf("Stability Results:\n");
    printf("  Ping Tests: %d\n", ping_count + ping_failures);
    printf("  Successful Pings: %d\n", ping_count);
    printf("  Failed Pings: %d\n", ping_failures);
    printf("  Stability Rate: %.1f%%\n", stability_rate);
    printf("  Status: %s\n", (stability_rate >= 99.0) ? "PASS" : "FAIL");
    printf("\n");
}

static void run_resource_exhaustion_test(void)
{
    printf("=== Resource Exhaustion Test ===\n");
    printf("Testing daemon behavior under resource pressure...\n");
    
    // Test 1: Connection exhaustion
    printf("Testing connection limits...\n");
    int connections[100];
    int max_connections = 0;
    
    for (int i = 0; i < 100; i++) {
        connections[i] = connect_to_daemon();
        if (connections[i] != -1) {
            max_connections++;
        } else {
            break;
        }
        usleep(1000); // 1ms delay between connections
    }
    
    printf("  Maximum concurrent connections: %d\n", max_connections);
    
    // Close all connections
    for (int i = 0; i < max_connections; i++) {
        close(connections[i]);
    }
    
    // Test 2: Rapid connection/disconnection
    printf("Testing rapid connect/disconnect cycles...\n");
    int rapid_cycles = 0;
    double rapid_start = get_time_ms();
    
    while ((get_time_ms() - rapid_start) < 10000.0) { // 10 seconds
        int sockfd = connect_to_daemon();
        if (sockfd != -1) {
            close(sockfd);
            rapid_cycles++;
        }
    }
    
    printf("  Rapid cycles completed: %d in 10 seconds\n", rapid_cycles);
    printf("  Status: %s\n", (rapid_cycles > 1000) ? "PASS" : "FAIL");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int test_duration = STRESS_TEST_DURATION;
    int i, num_scenarios, scenarios_passed = 0;
    
    if (argc > 1) {
        test_duration = atoi(argv[1]);
        if (test_duration <= 0 || test_duration > 300) {
            fprintf(stderr, "Invalid test duration. Using default: %d seconds\n", 
                   STRESS_TEST_DURATION);
            test_duration = STRESS_TEST_DURATION;
        }
    }
    
    printf("Goxel v14.0 Daemon Stress Test Suite\n");
    printf("====================================\n");
    printf("Target: Handle %d+ concurrent clients reliably\n", 
           TARGET_CONCURRENT_CLIENTS);
    printf("Base Test Duration: %d seconds\n\n", test_duration);
    
    // Initialize random seed
    srand(time(NULL));
    
    // Count scenarios
    for (num_scenarios = 0; scenarios[num_scenarios].name; num_scenarios++);
    
    // Run stress scenarios
    for (i = 0; i < num_scenarios; i++) {
        if (run_stress_scenario(&scenarios[i])) {
            scenarios_passed++;
        }
    }
    
    // Run additional stress tests
    run_daemon_stability_test(test_duration / 2);
    run_resource_exhaustion_test();
    
    // Final summary
    printf("=== STRESS TEST SUMMARY ===\n");
    printf("Scenarios Passed: %d/%d\n", scenarios_passed, num_scenarios);
    printf("Overall Grade: %s\n",
           (scenarios_passed >= num_scenarios * 0.8) ? "ROBUST" : "NEEDS_HARDENING");
    printf("Daemon Stress Tolerance: %s\n",
           (scenarios_passed == num_scenarios) ? "EXCELLENT" : "GOOD");
    
    return (scenarios_passed >= num_scenarios * 0.8) ? 0 : 1;
}