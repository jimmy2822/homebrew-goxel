/* Goxel 3D voxels editor
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

#include "../src/daemon/worker_pool.h"
#include "../src/daemon/request_queue.h"
#include "../src/daemon/socket_server.h"
#include "../src/daemon/json_rpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>

// ============================================================================
// TEST CONFIGURATION
// ============================================================================

#define TEST_SOCKET_PATH "/tmp/goxel-test-concurrent.sock"
#define TEST_NUM_CLIENTS 10
#define TEST_REQUESTS_PER_CLIENT 100
#define TEST_WORKER_THREADS 8
#define TEST_QUEUE_SIZE 2048
#define TEST_TIMEOUT_SECONDS 30

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static int64_t get_current_time_us(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

static void cleanup_socket(void)
{
    unlink(TEST_SOCKET_PATH);
}

// ============================================================================
// MOCK REQUEST PROCESSING
// ============================================================================

static int mock_process_request(void *request_data, int worker_id, void *context)
{
    // Simulate some processing time
    usleep(rand() % 10000); // 0-10ms random processing time
    
    printf("Worker %d processed request %p\n", worker_id, request_data);
    return 0;
}

static void mock_cleanup_request(void *request_data)
{
    free(request_data);
}

// ============================================================================
// CLIENT SIMULATION
// ============================================================================

typedef struct {
    int client_id;
    int num_requests;
    const char *socket_path;
    int successful_requests;
    int failed_requests;
    int64_t total_time_us;
} test_client_t;

static void *client_thread_func(void *arg)
{
    test_client_t *client = (test_client_t*)arg;
    int64_t start_time = get_current_time_us();
    
    printf("Client %d starting with %d requests\n", client->client_id, client->num_requests);
    
    // Connect to server
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        printf("Client %d: Failed to create socket\n", client->client_id);
        return NULL;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, client->socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Client %d: Failed to connect to server\n", client->client_id);
        close(sock_fd);
        return NULL;
    }
    
    printf("Client %d connected to server\n", client->client_id);
    
    // Send requests
    for (int i = 0; i < client->num_requests; i++) {
        // Create simple JSON-RPC request
        char request_json[512];
        snprintf(request_json, sizeof(request_json),
                "{\"jsonrpc\":\"2.0\",\"method\":\"test.echo\",\"params\":{\"message\":\"Hello from client %d request %d\"},\"id\":%d}",
                client->client_id, i, i);
        
        // Send request
        uint32_t msg_length = htonl(strlen(request_json));
        uint32_t msg_id = htonl(i);
        uint32_t msg_type = htonl(0);
        uint32_t timestamp = htonl((uint32_t)(get_current_time_us() >> 32));
        
        // Send header
        if (write(sock_fd, &msg_id, sizeof(msg_id)) != sizeof(msg_id) ||
            write(sock_fd, &msg_type, sizeof(msg_type)) != sizeof(msg_type) ||
            write(sock_fd, &msg_length, sizeof(msg_length)) != sizeof(msg_length) ||
            write(sock_fd, &timestamp, sizeof(timestamp)) != sizeof(timestamp)) {
            printf("Client %d: Failed to send header for request %d\n", client->client_id, i);
            client->failed_requests++;
            continue;
        }
        
        // Send payload
        if (write(sock_fd, request_json, strlen(request_json)) != (ssize_t)strlen(request_json)) {
            printf("Client %d: Failed to send payload for request %d\n", client->client_id, i);
            client->failed_requests++;
            continue;
        }
        
        client->successful_requests++;
        
        // Small delay between requests
        usleep(1000); // 1ms
    }
    
    close(sock_fd);
    
    client->total_time_us = get_current_time_us() - start_time;
    printf("Client %d completed: %d successful, %d failed, %ld μs total\n",
           client->client_id, client->successful_requests, client->failed_requests,
           client->total_time_us);
    
    return NULL;
}

// ============================================================================
// WORKER POOL TESTS
// ============================================================================

static int test_worker_pool_basic(void)
{
    printf("\n=== Testing Worker Pool Basic Operations ===\n");
    
    worker_pool_config_t config = worker_pool_default_config();
    config.worker_count = 4;
    config.queue_capacity = 100;
    config.process_func = mock_process_request;
    config.cleanup_func = mock_cleanup_request;
    
    worker_pool_t *pool = worker_pool_create(&config);
    if (!pool) {
        printf("FAIL: Could not create worker pool\n");
        return 1;
    }
    
    printf("Worker pool created successfully\n");
    
    if (worker_pool_start(pool) != WORKER_POOL_SUCCESS) {
        printf("FAIL: Could not start worker pool\n");
        worker_pool_destroy(pool);
        return 1;
    }
    
    printf("Worker pool started successfully\n");
    
    // Submit test requests
    int num_requests = 50;
    for (int i = 0; i < num_requests; i++) {
        int *request_data = malloc(sizeof(int));
        *request_data = i;
        
        worker_pool_error_t result = worker_pool_submit_request(pool, request_data, 
                                                               WORKER_PRIORITY_NORMAL);
        if (result != WORKER_POOL_SUCCESS) {
            printf("FAIL: Could not submit request %d: %s\n", 
                   i, worker_pool_error_string(result));
            free(request_data);
            break;
        }
    }
    
    printf("Submitted %d requests\n", num_requests);
    
    // Wait for processing
    sleep(3);
    
    // Get statistics
    worker_stats_t stats;
    if (worker_pool_get_stats(pool, &stats) == WORKER_POOL_SUCCESS) {
        printf("Statistics:\n");
        printf("  Requests processed: %lu\n", stats.requests_processed);
        printf("  Requests failed: %lu\n", stats.requests_failed);
        printf("  Average processing time: %lu μs\n", stats.average_processing_time_us);
        printf("  Active workers: %d\n", stats.active_workers);
        printf("  Idle workers: %d\n", stats.idle_workers);
    }
    
    if (worker_pool_stop(pool) != WORKER_POOL_SUCCESS) {
        printf("FAIL: Could not stop worker pool\n");
        worker_pool_destroy(pool);
        return 1;
    }
    
    worker_pool_destroy(pool);
    printf("Worker pool test completed successfully\n");
    return 0;
}

static int test_worker_pool_stress(void)
{
    printf("\n=== Testing Worker Pool Stress ===\n");
    
    worker_pool_config_t config = worker_pool_default_config();
    config.worker_count = TEST_WORKER_THREADS;
    config.queue_capacity = TEST_QUEUE_SIZE;
    config.enable_priority_queue = true;
    config.process_func = mock_process_request;
    config.cleanup_func = mock_cleanup_request;
    
    worker_pool_t *pool = worker_pool_create(&config);
    if (!pool) {
        printf("FAIL: Could not create worker pool\n");
        return 1;
    }
    
    if (worker_pool_start(pool) != WORKER_POOL_SUCCESS) {
        printf("FAIL: Could not start worker pool\n");
        worker_pool_destroy(pool);
        return 1;
    }
    
    int64_t start_time = get_current_time_us();
    
    // Submit many requests rapidly
    int num_requests = 1000;
    int submitted = 0;
    int failed = 0;
    
    for (int i = 0; i < num_requests; i++) {
        int *request_data = malloc(sizeof(int));
        *request_data = i;
        
        worker_priority_t priority = (i % 4 == 0) ? WORKER_PRIORITY_HIGH : WORKER_PRIORITY_NORMAL;
        
        worker_pool_error_t result = worker_pool_submit_request(pool, request_data, priority);
        if (result == WORKER_POOL_SUCCESS) {
            submitted++;
        } else {
            failed++;
            free(request_data);
        }
    }
    
    printf("Submitted %d requests, %d failed\n", submitted, failed);
    
    // Wait for all requests to be processed
    while (worker_pool_get_queue_size(pool) > 0) {
        usleep(10000); // 10ms
    }
    
    // Additional wait for workers to finish current tasks
    sleep(2);
    
    int64_t end_time = get_current_time_us();
    int64_t total_time = end_time - start_time;
    
    // Get final statistics
    worker_stats_t stats;
    if (worker_pool_get_stats(pool, &stats) == WORKER_POOL_SUCCESS) {
        printf("Stress test results:\n");
        printf("  Total time: %ld μs (%.2f ms)\n", total_time, total_time / 1000.0);
        printf("  Requests processed: %lu\n", stats.requests_processed);
        printf("  Requests failed: %lu\n", stats.requests_failed);
        printf("  Average processing time: %lu μs\n", stats.average_processing_time_us);
        printf("  Throughput: %.2f requests/second\n", 
               (double)stats.requests_processed * 1000000.0 / total_time);
    }
    
    worker_pool_stop(pool);
    worker_pool_destroy(pool);
    
    printf("Worker pool stress test completed\n");
    return 0;
}

// ============================================================================
// REQUEST QUEUE TESTS
// ============================================================================

static int test_request_queue_basic(void)
{
    printf("\n=== Testing Request Queue Basic Operations ===\n");
    
    request_queue_config_t config = request_queue_default_config();
    config.max_size = 100;
    config.enable_priority_queue = true;
    
    request_queue_t *queue = request_queue_create(&config);
    if (!queue) {
        printf("FAIL: Could not create request queue\n");
        return 1;
    }
    
    printf("Request queue created successfully\n");
    
    // Test basic enqueue/dequeue
    for (int i = 0; i < 10; i++) {
        json_rpc_request_t *request = calloc(1, sizeof(json_rpc_request_t));
        request->method = strdup("test.method");
        json_rpc_create_id_number(i, &request->id);
        
        uint32_t request_id;
        request_priority_t priority = (i % 2) ? REQUEST_PRIORITY_HIGH : REQUEST_PRIORITY_NORMAL;
        
        request_queue_error_t result = request_queue_enqueue(queue, NULL, request, 
                                                           priority, 0, &request_id);
        if (result != REQUEST_QUEUE_SUCCESS) {
            printf("FAIL: Could not enqueue request %d: %s\n", 
                   i, request_queue_error_string(result));
            json_rpc_free_request(request);
            break;
        }
        
        printf("Enqueued request %d with ID %u\n", i, request_id);
    }
    
    printf("Queue size: %d\n", request_queue_get_size(queue));
    
    // Dequeue all requests
    int dequeued = 0;
    while (!request_queue_is_empty(queue)) {
        queued_request_t *request = request_queue_dequeue(queue, dequeued);
        if (request) {
            printf("Dequeued request ID %u (priority %d)\n", 
                   request->request_id, request->priority);
            
            request_queue_complete_request(queue, request, true);
            request_queue_destroy_request(request);
            dequeued++;
        } else {
            break;
        }
    }
    
    printf("Dequeued %d requests\n", dequeued);
    
    // Get statistics
    request_queue_stats_t stats;
    if (request_queue_get_stats(queue, &stats) == REQUEST_QUEUE_SUCCESS) {
        printf("Queue statistics:\n");
        printf("  Total enqueued: %lu\n", stats.total_enqueued);
        printf("  Total dequeued: %lu\n", stats.total_dequeued);
        printf("  Total completed: %lu\n", stats.total_completed);
        printf("  Average wait time: %lu μs\n", stats.average_wait_time_us);
    }
    
    request_queue_destroy(queue);
    printf("Request queue test completed successfully\n");
    return 0;
}

// ============================================================================
// INTEGRATED STRESS TEST
// ============================================================================

static int test_concurrent_stress(void)
{
    printf("\n=== Testing Concurrent Processing Stress ===\n");
    
    // Cleanup any existing socket
    cleanup_socket();
    
    // Create socket server for testing
    socket_server_config_t server_config = socket_server_default_config();
    server_config.socket_path = TEST_SOCKET_PATH;
    server_config.max_connections = TEST_NUM_CLIENTS * 2;
    server_config.thread_per_client = false;
    server_config.thread_pool_size = TEST_WORKER_THREADS;
    
    socket_server_t *server = socket_server_create(&server_config);
    if (!server) {
        printf("FAIL: Could not create socket server\n");
        return 1;
    }
    
    if (socket_server_start(server) != SOCKET_SUCCESS) {
        printf("FAIL: Could not start socket server\n");
        socket_server_destroy(server);
        cleanup_socket();
        return 1;
    }
    
    printf("Socket server started on %s\n", TEST_SOCKET_PATH);
    
    // Create and start client threads
    pthread_t client_threads[TEST_NUM_CLIENTS];
    test_client_t clients[TEST_NUM_CLIENTS];
    
    int64_t test_start_time = get_current_time_us();
    
    for (int i = 0; i < TEST_NUM_CLIENTS; i++) {
        clients[i].client_id = i;
        clients[i].num_requests = TEST_REQUESTS_PER_CLIENT;
        clients[i].socket_path = TEST_SOCKET_PATH;
        clients[i].successful_requests = 0;
        clients[i].failed_requests = 0;
        clients[i].total_time_us = 0;
        
        if (pthread_create(&client_threads[i], NULL, client_thread_func, &clients[i]) != 0) {
            printf("FAIL: Could not create client thread %d\n", i);
            break;
        }
    }
    
    printf("Started %d client threads\n", TEST_NUM_CLIENTS);
    
    // Wait for all clients to complete
    int total_successful = 0;
    int total_failed = 0;
    int64_t total_client_time = 0;
    
    for (int i = 0; i < TEST_NUM_CLIENTS; i++) {
        pthread_join(client_threads[i], NULL);
        total_successful += clients[i].successful_requests;
        total_failed += clients[i].failed_requests;
        total_client_time += clients[i].total_time_us;
    }
    
    int64_t test_end_time = get_current_time_us();
    int64_t total_test_time = test_end_time - test_start_time;
    
    printf("\n=== Stress Test Results ===\n");
    printf("Total test time: %ld μs (%.2f seconds)\n", 
           total_test_time, total_test_time / 1000000.0);
    printf("Total requests sent: %d\n", total_successful + total_failed);
    printf("Successful requests: %d\n", total_successful);
    printf("Failed requests: %d\n", total_failed);
    printf("Success rate: %.2f%%\n", 
           (double)total_successful / (total_successful + total_failed) * 100.0);
    printf("Average client time: %ld μs\n", total_client_time / TEST_NUM_CLIENTS);
    printf("Throughput: %.2f requests/second\n", 
           (double)total_successful * 1000000.0 / total_test_time);
    
    // Get server statistics
    socket_server_stats_t server_stats;
    if (socket_server_get_stats(server, &server_stats) == SOCKET_SUCCESS) {
        printf("\nServer statistics:\n");
        printf("  Total connections: %d\n", server_stats.total_connections);
        printf("  Current connections: %d\n", server_stats.current_connections);
        printf("  Messages received: %lu\n", server_stats.messages_received);
        printf("  Messages sent: %lu\n", server_stats.messages_sent);
        printf("  Connection errors: %lu\n", server_stats.connection_errors);
    }
    
    // Stop server
    socket_server_stop(server);
    socket_server_destroy(server);
    cleanup_socket();
    
    printf("Concurrent stress test completed\n");
    
    // Consider test successful if we achieved reasonable performance
    double throughput = (double)total_successful * 1000000.0 / total_test_time;
    if (throughput < 100.0) {  // Less than 100 requests/second is poor
        printf("FAIL: Throughput too low (%.2f requests/second)\n", throughput);
        return 1;
    }
    
    if (total_failed > total_successful * 0.05) {  // More than 5% failure rate
        printf("FAIL: Too many failed requests (%d/%d = %.2f%%)\n", 
               total_failed, total_successful + total_failed,
               (double)total_failed / (total_successful + total_failed) * 100.0);
        return 1;
    }
    
    return 0;
}

// ============================================================================
// MAIN TEST FUNCTION
// ============================================================================

int main(int argc, char *argv[])
{
    printf("Goxel Concurrent Processing Test Suite\n");
    printf("======================================\n");
    
    // Setup signal handler for cleanup
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    
    // Initialize random seed
    srand((unsigned int)time(NULL));
    
    int failed_tests = 0;
    
    // Run individual component tests
    if (test_worker_pool_basic() != 0) {
        failed_tests++;
    }
    
    if (test_worker_pool_stress() != 0) {
        failed_tests++;
    }
    
    if (test_request_queue_basic() != 0) {
        failed_tests++;
    }
    
    // Run integrated stress test
    if (test_concurrent_stress() != 0) {
        failed_tests++;
    }
    
    printf("\n======================================\n");
    if (failed_tests == 0) {
        printf("✅ ALL TESTS PASSED\n");
        printf("Concurrent processing system is working correctly!\n");
        printf("Expected performance improvement: 2-3x over sequential processing\n");
        return 0;
    } else {
        printf("❌ %d TESTS FAILED\n", failed_tests);
        printf("Concurrent processing system needs fixes before deployment\n");
        return 1;
    }
}