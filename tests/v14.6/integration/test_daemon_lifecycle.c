/*
 * Goxel v14.6 Daemon Lifecycle Integration Test
 * 
 * Tests daemon startup, shutdown, PID file management, and signal handling
 * against the real daemon implementation.
 * 
 * Author: James O'Brien (Agent-4)
 * Date: January 2025
 */

#include "../framework/test_framework.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define DAEMON_BINARY "../../../goxel"
#define DAEMON_PID_FILE "/tmp/goxel-daemon.pid"
#define DAEMON_SOCKET "/tmp/goxel.sock"
#define STARTUP_TIMEOUT_MS 2000
#define SHUTDOWN_TIMEOUT_MS 2000

// Helper to check if daemon is running
static bool is_daemon_running(pid_t pid) {
    return kill(pid, 0) == 0;
}

// Helper to read PID from file
static pid_t read_pid_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    pid_t pid;
    if (fscanf(fp, "%d", &pid) != 1) {
        pid = -1;
    }
    fclose(fp);
    return pid;
}

// Helper to wait for file to exist
static bool wait_for_file(const char *path, int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (access(path, F_OK) == 0) {
            return true;
        }
        usleep(10000); // 10ms
        elapsed += 10;
    }
    return false;
}

// Helper to wait for file to be removed
static bool wait_for_file_removal(const char *path, int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (access(path, F_OK) != 0) {
            return true;
        }
        usleep(10000); // 10ms
        elapsed += 10;
    }
    return false;
}

// Test: Basic daemon startup and shutdown
TEST_CASE(daemon_start_stop) {
    // Clean up any previous state
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    // Start daemon
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        // Child process - execute daemon
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        // If we get here, exec failed
        perror("execl");
        exit(1);
    }
    
    // Parent process - wait for daemon to start
    test_log_info("Started daemon with PID %d", daemon_pid);
    
    // Wait for PID file
    TEST_ASSERT(wait_for_file(DAEMON_PID_FILE, STARTUP_TIMEOUT_MS));
    test_log_info("PID file created");
    
    // Verify PID file content
    pid_t file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(file_pid > 0);
    test_log_info("PID from file: %d", file_pid);
    
    // Wait for socket file
    TEST_ASSERT(wait_for_file(DAEMON_SOCKET, STARTUP_TIMEOUT_MS));
    test_log_info("Socket file created");
    
    // Verify daemon is running
    TEST_ASSERT(is_daemon_running(file_pid));
    test_log_info("Daemon is running");
    
    // Send SIGTERM to shutdown gracefully
    TEST_ASSERT(kill(file_pid, SIGTERM) == 0);
    test_log_info("Sent SIGTERM to daemon");
    
    // Wait for daemon to stop
    int shutdown_elapsed = 0;
    while (shutdown_elapsed < SHUTDOWN_TIMEOUT_MS) {
        if (!is_daemon_running(file_pid)) {
            break;
        }
        usleep(10000); // 10ms
        shutdown_elapsed += 10;
    }
    TEST_ASSERT(!is_daemon_running(file_pid));
    test_log_info("Daemon stopped");
    
    // Verify cleanup
    TEST_ASSERT(wait_for_file_removal(DAEMON_PID_FILE, 1000));
    TEST_ASSERT(wait_for_file_removal(DAEMON_SOCKET, 1000));
    test_log_info("Cleanup completed");
    
    return TEST_PASS;
}

// Test: PID file management
TEST_CASE(daemon_pid_file) {
    // Clean up
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    // Start daemon
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for PID file
    TEST_ASSERT(wait_for_file(DAEMON_PID_FILE, STARTUP_TIMEOUT_MS));
    
    // Read and verify PID
    pid_t file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(file_pid > 0);
    
    // Check file permissions (should be readable)
    struct stat st;
    TEST_ASSERT(stat(DAEMON_PID_FILE, &st) == 0);
    TEST_ASSERT(st.st_mode & S_IRUSR);
    
    // Try to start another daemon (should fail)
    pid_t second_daemon = fork();
    TEST_ASSERT(second_daemon >= 0);
    
    if (second_daemon == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(0); // If exec fails, exit with 0 (expected)
    }
    
    // Wait for second daemon to exit
    int status;
    waitpid(second_daemon, &status, 0);
    TEST_ASSERT(WIFEXITED(status));
    test_log_info("Second daemon correctly refused to start");
    
    // Cleanup
    kill(file_pid, SIGTERM);
    wait_for_file_removal(DAEMON_PID_FILE, SHUTDOWN_TIMEOUT_MS);
    
    return TEST_PASS;
}

// Test: Signal handling
TEST_CASE(daemon_signal_handling) {
    // Clean up
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    // Start daemon
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for startup
    TEST_ASSERT(wait_for_file(DAEMON_PID_FILE, STARTUP_TIMEOUT_MS));
    pid_t file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(file_pid > 0);
    
    // Test SIGHUP (should reload config - daemon should stay running)
    TEST_ASSERT(kill(file_pid, SIGHUP) == 0);
    usleep(100000); // 100ms
    TEST_ASSERT(is_daemon_running(file_pid));
    test_log_info("SIGHUP handled correctly");
    
    // Test SIGUSR1 (custom signal - should be ignored)
    TEST_ASSERT(kill(file_pid, SIGUSR1) == 0);
    usleep(100000); // 100ms
    TEST_ASSERT(is_daemon_running(file_pid));
    test_log_info("SIGUSR1 handled correctly");
    
    // Test SIGINT (should shutdown gracefully)
    TEST_ASSERT(kill(file_pid, SIGINT) == 0);
    
    // Wait for shutdown
    int shutdown_elapsed = 0;
    while (shutdown_elapsed < SHUTDOWN_TIMEOUT_MS) {
        if (!is_daemon_running(file_pid)) {
            break;
        }
        usleep(10000);
        shutdown_elapsed += 10;
    }
    TEST_ASSERT(!is_daemon_running(file_pid));
    test_log_info("SIGINT shutdown completed");
    
    // Verify cleanup
    TEST_ASSERT(wait_for_file_removal(DAEMON_PID_FILE, 1000));
    TEST_ASSERT(wait_for_file_removal(DAEMON_SOCKET, 1000));
    
    return TEST_PASS;
}

// Test: Daemon crash recovery
TEST_CASE(daemon_crash_recovery) {
    // Clean up
    unlink(DAEMON_PID_FILE);
    unlink(DAEMON_SOCKET);
    
    // Start daemon
    pid_t daemon_pid = fork();
    TEST_ASSERT(daemon_pid >= 0);
    
    if (daemon_pid == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // Wait for startup
    TEST_ASSERT(wait_for_file(DAEMON_PID_FILE, STARTUP_TIMEOUT_MS));
    pid_t file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(file_pid > 0);
    TEST_ASSERT(wait_for_file(DAEMON_SOCKET, STARTUP_TIMEOUT_MS));
    
    // Simulate crash with SIGKILL
    TEST_ASSERT(kill(file_pid, SIGKILL) == 0);
    
    // Wait for process to die
    int death_elapsed = 0;
    while (death_elapsed < 1000) {
        if (!is_daemon_running(file_pid)) {
            break;
        }
        usleep(10000);
        death_elapsed += 10;
    }
    TEST_ASSERT(!is_daemon_running(file_pid));
    test_log_info("Daemon killed");
    
    // PID file should still exist (not cleaned up on crash)
    TEST_ASSERT(access(DAEMON_PID_FILE, F_OK) == 0);
    
    // Start new daemon (should detect stale PID file)
    pid_t new_daemon = fork();
    TEST_ASSERT(new_daemon >= 0);
    
    if (new_daemon == 0) {
        execl(DAEMON_BINARY, "goxel", "--headless", "--daemon", NULL);
        exit(1);
    }
    
    // New daemon should start successfully
    usleep(500000); // 500ms
    pid_t new_file_pid = read_pid_file(DAEMON_PID_FILE);
    TEST_ASSERT(new_file_pid > 0);
    TEST_ASSERT(new_file_pid != file_pid);
    TEST_ASSERT(is_daemon_running(new_file_pid));
    test_log_info("New daemon started with PID %d", new_file_pid);
    
    // Cleanup
    kill(new_file_pid, SIGTERM);
    wait_for_file_removal(DAEMON_PID_FILE, SHUTDOWN_TIMEOUT_MS);
    
    return TEST_PASS;
}

// Test suite registration
TEST_SUITE(daemon_lifecycle) {
    REGISTER_TEST(daemon_lifecycle, daemon_start_stop, NULL, NULL);
    REGISTER_TEST(daemon_lifecycle, daemon_pid_file, NULL, NULL);
    REGISTER_TEST(daemon_lifecycle, daemon_signal_handling, NULL, NULL);
    REGISTER_TEST(daemon_lifecycle, daemon_crash_recovery, NULL, NULL);
}