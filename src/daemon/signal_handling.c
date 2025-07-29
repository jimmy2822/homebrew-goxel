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

#define _POSIX_C_SOURCE 199309L
#include "daemon_lifecycle.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>

// ============================================================================
// GLOBAL SIGNAL HANDLING STATE
// ============================================================================

// Global daemon context for signal handlers
static daemon_context_t *g_daemon_context = NULL;

// Signal handling state
static struct sigaction old_sigterm_action;
static struct sigaction old_sigint_action;
static struct sigaction old_sighup_action;
static struct sigaction old_sigchld_action;
static struct sigaction old_sigpipe_action;
static bool signals_installed = false;

// Signal-safe flags for communication between signal handlers and main thread
static volatile sig_atomic_t shutdown_signal_received = 0;
static volatile sig_atomic_t reload_signal_received = 0;
static volatile sig_atomic_t pipe_errors_count = 0;

// ============================================================================
// SIGNAL HANDLER IMPLEMENTATIONS
// ============================================================================

/**
 * Signal handler for SIGTERM and SIGINT (graceful shutdown).
 */
static void daemon_signal_shutdown(int signal)
{
    (void)signal; // Unused parameter
    
    // Only use async-signal-safe operations
    // Set signal-safe flag that main thread will check
    shutdown_signal_received = 1;
    
    // Write to a pipe could be used here for immediate notification
    // but for simplicity, we'll rely on main thread polling
}

/**
 * Signal handler for SIGHUP (reload configuration).
 */
static void daemon_signal_reload(int signal)
{
    (void)signal; // Unused parameter
    
    // Only use async-signal-safe operations
    // Set signal-safe flag for main thread to handle config reload
    reload_signal_received = 1;
}

/**
 * Signal handler for SIGCHLD (child process termination).
 */
static void daemon_signal_child(int signal)
{
    (void)signal; // Unused parameter
    
    // Reap any child processes to prevent zombies
    // waitpid is async-signal-safe according to POSIX
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Child process reaped - this is the main purpose of SIGCHLD handler
        // Only use async-signal-safe operations here
    }
}

/**
 * Signal handler for SIGPIPE (broken pipe).
 */
static void daemon_signal_pipe(int signal)
{
    (void)signal; // Unused parameter
    
    // SIGPIPE should generally be ignored - we handle broken pipes in application code
    // Only use async-signal-safe operations - increment atomic counter
    pipe_errors_count++;
}

// ============================================================================
// SIGNAL HANDLER INSTALLATION
// ============================================================================

/**
 * Installs a signal handler with specified options.
 */
static daemon_error_t install_signal_handler(int signal, void (*handler)(int), 
                                           struct sigaction *old_action)
{
    struct sigaction new_action;
    
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = handler;
    sigemptyset(&new_action.sa_mask);
    
    // Add additional signals to block during handler execution
    sigaddset(&new_action.sa_mask, SIGTERM);
    sigaddset(&new_action.sa_mask, SIGINT);
    sigaddset(&new_action.sa_mask, SIGHUP);
    
    // Use SA_RESTART to automatically restart interrupted system calls
    new_action.sa_flags = SA_RESTART;
    
    if (sigaction(signal, &new_action, old_action) == -1) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    return DAEMON_SUCCESS;
}

/**
 * Restores the original signal handler.
 */
static daemon_error_t restore_signal_handler(int signal, struct sigaction *old_action)
{
    if (sigaction(signal, old_action, NULL) == -1) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    return DAEMON_SUCCESS;
}

// ============================================================================
// PUBLIC SIGNAL HANDLING INTERFACE
// ============================================================================

daemon_error_t daemon_setup_signals_impl(daemon_context_t *ctx)
{
    if (!ctx) {
        return DAEMON_ERROR_INVALID_CONTEXT;
    }
    
    if (signals_installed) {
        return DAEMON_ERROR_ALREADY_RUNNING;
    }
    
    // Set global context for signal handlers to access
    g_daemon_context = ctx;
    
    daemon_error_t result;
    
    // Install SIGTERM handler (graceful shutdown)
    result = install_signal_handler(SIGTERM, daemon_signal_shutdown, &old_sigterm_action);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to install SIGTERM handler");
        return result;
    }
    
    // Install SIGINT handler (graceful shutdown)
    result = install_signal_handler(SIGINT, daemon_signal_shutdown, &old_sigint_action);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to install SIGINT handler");
        goto cleanup_sigterm;
    }
    
    // Install SIGHUP handler (configuration reload)
    result = install_signal_handler(SIGHUP, daemon_signal_reload, &old_sighup_action);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to install SIGHUP handler");
        goto cleanup_sigint;
    }
    
    // Install SIGCHLD handler (child process management)
    result = install_signal_handler(SIGCHLD, daemon_signal_child, &old_sigchld_action);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to install SIGCHLD handler");
        goto cleanup_sighup;
    }
    
    // Install SIGPIPE handler (ignore broken pipes)
    result = install_signal_handler(SIGPIPE, daemon_signal_pipe, &old_sigpipe_action);
    if (result != DAEMON_SUCCESS) {
        daemon_set_error(ctx, result, "Failed to install SIGPIPE handler");
        goto cleanup_sigchld;
    }
    
    signals_installed = true;
    return DAEMON_SUCCESS;
    
    // Cleanup on error
cleanup_sigchld:
    restore_signal_handler(SIGCHLD, &old_sigchld_action);
cleanup_sighup:
    restore_signal_handler(SIGHUP, &old_sighup_action);
cleanup_sigint:
    restore_signal_handler(SIGINT, &old_sigint_action);
cleanup_sigterm:
    restore_signal_handler(SIGTERM, &old_sigterm_action);
    
    // Clear global context on error
    g_daemon_context = NULL;
    
    return result;
}

daemon_error_t daemon_cleanup_signals_impl(void)
{
    if (!signals_installed) {
        return DAEMON_SUCCESS;
    }
    
    daemon_error_t result = DAEMON_SUCCESS;
    daemon_error_t temp_result;
    
    // Clear global context before cleanup
    g_daemon_context = NULL;
    
    // Reset signal flags
    daemon_reset_signal_flags();
    
    // Restore all signal handlers
    temp_result = restore_signal_handler(SIGTERM, &old_sigterm_action);
    if (temp_result != DAEMON_SUCCESS && result == DAEMON_SUCCESS) {
        result = temp_result;
    }
    
    temp_result = restore_signal_handler(SIGINT, &old_sigint_action);
    if (temp_result != DAEMON_SUCCESS && result == DAEMON_SUCCESS) {
        result = temp_result;
    }
    
    temp_result = restore_signal_handler(SIGHUP, &old_sighup_action);
    if (temp_result != DAEMON_SUCCESS && result == DAEMON_SUCCESS) {
        result = temp_result;
    }
    
    temp_result = restore_signal_handler(SIGCHLD, &old_sigchld_action);
    if (temp_result != DAEMON_SUCCESS && result == DAEMON_SUCCESS) {
        result = temp_result;
    }
    
    temp_result = restore_signal_handler(SIGPIPE, &old_sigpipe_action);
    if (temp_result != DAEMON_SUCCESS && result == DAEMON_SUCCESS) {
        result = temp_result;
    }
    
    signals_installed = false;
    
    return result;
}

// ============================================================================
// SIGNAL UTILITIES
// ============================================================================

/**
 * Sends a signal to a daemon process by PID.
 */
daemon_error_t daemon_send_signal(pid_t pid, int signal)
{
    if (pid <= 0) {
        return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    if (kill(pid, signal) == -1) {
        switch (errno) {
            case ESRCH:
                return DAEMON_ERROR_NOT_RUNNING;
            case EPERM:
                return DAEMON_ERROR_PERMISSION_DENIED;
            default:
                return DAEMON_ERROR_UNKNOWN;
        }
    }
    
    return DAEMON_SUCCESS;
}

/**
 * Sends SIGTERM to a daemon process.
 */
daemon_error_t daemon_send_shutdown_signal(pid_t pid)
{
    return daemon_send_signal(pid, SIGTERM);
}

/**
 * Sends SIGHUP to a daemon process.
 */
daemon_error_t daemon_send_reload_signal(pid_t pid)
{
    return daemon_send_signal(pid, SIGHUP);
}

/**
 * Sends SIGKILL to a daemon process (force termination).
 */
daemon_error_t daemon_send_kill_signal(pid_t pid)
{
    return daemon_send_signal(pid, SIGKILL);
}

/**
 * Blocks signals during critical sections.
 */
daemon_error_t daemon_block_signals(sigset_t *old_mask)
{
    sigset_t mask;
    
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGCHLD);
    
    if (pthread_sigmask(SIG_BLOCK, &mask, old_mask) != 0) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    return DAEMON_SUCCESS;
}

/**
 * Unblocks signals after critical sections.
 */
daemon_error_t daemon_unblock_signals(const sigset_t *old_mask)
{
    if (pthread_sigmask(SIG_SETMASK, old_mask, NULL) != 0) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    return DAEMON_SUCCESS;
}

/**
 * Waits for a specific signal with timeout.
 * Note: This is a simplified implementation for compatibility.
 */
daemon_error_t daemon_wait_for_signal(int signal, int timeout_ms)
{
    sigset_t mask, old_mask;
    
    sigemptyset(&mask);
    sigaddset(&mask, signal);
    
    // Block the signal first
    if (pthread_sigmask(SIG_BLOCK, &mask, &old_mask) != 0) {
        return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
    }
    
    // Simple timeout implementation using sleep
    // In a full implementation, this would use sigtimedwait or signalfd
    int elapsed_ms = 0;
    const int poll_interval_ms = 10;
    
    while (elapsed_ms < timeout_ms) {
        // Check if signal is pending (simplified approach)
        sigset_t pending;
        if (sigpending(&pending) == 0) {
            if (sigismember(&pending, signal)) {
                // Signal is pending, consume it
                int sig;
                if (sigwait(&mask, &sig) == 0 && sig == signal) {
                    pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
                    return DAEMON_SUCCESS;
                }
            }
        }
        
        // Sleep for a short interval
        struct timespec ts = {0, poll_interval_ms * 1000000};
        nanosleep(&ts, NULL);
        elapsed_ms += poll_interval_ms;
    }
    
    // Restore signal mask
    pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
    
    return DAEMON_ERROR_TIMEOUT;
}

// ============================================================================
// SIGNAL STATUS CHECKING (for main thread)
// ============================================================================

/**
 * Checks if a shutdown signal was received and processes it.
 * This should be called periodically by the main daemon loop.
 */
daemon_error_t daemon_process_signals(daemon_context_t *ctx)
{
    if (!ctx) {
        return DAEMON_ERROR_INVALID_CONTEXT;
    }
    
    daemon_error_t result = DAEMON_SUCCESS;
    
    // Check for shutdown signal
    if (shutdown_signal_received) {
        shutdown_signal_received = 0; // Reset flag
        daemon_request_shutdown(ctx);
        daemon_set_error(ctx, DAEMON_SUCCESS, "Received shutdown signal, shutting down gracefully");
        result = DAEMON_SUCCESS; // Shutdown is not an error
    }
    
    // Check for reload signal
    if (reload_signal_received) {
        reload_signal_received = 0; // Reset flag
        daemon_set_error(ctx, DAEMON_SUCCESS, "Received SIGHUP, configuration reload requested");
        daemon_update_activity(ctx);
        // In a full implementation, we would reload configuration here
    }
    
    // Process pipe errors
    if (pipe_errors_count > 0) {
        // Add accumulated pipe errors to daemon stats
        for (sig_atomic_t i = 0; i < pipe_errors_count; i++) {
            daemon_increment_errors(ctx);
        }
        pipe_errors_count = 0; // Reset counter
    }
    
    return result;
}

/**
 * Checks if any signals are pending without processing them.
 */
bool daemon_has_pending_signals(void)
{
    return shutdown_signal_received || reload_signal_received || pipe_errors_count > 0;
}

/**
 * Resets all signal flags (mainly for testing).
 */
void daemon_reset_signal_flags(void)
{
    shutdown_signal_received = 0;
    reload_signal_received = 0;
    pipe_errors_count = 0;
}

// ============================================================================
// SIGNAL TESTING UTILITIES
// ============================================================================

/**
 * Checks if signal handlers are properly installed.
 */
bool daemon_signals_installed(void)
{
    return signals_installed;
}

/**
 * Tests signal handling by sending a signal to self.
 */
daemon_error_t daemon_test_signal_handling(daemon_context_t *ctx, int signal)
{
    if (!ctx) {
        return DAEMON_ERROR_INVALID_CONTEXT;
    }
    
    if (!signals_installed) {
        return DAEMON_ERROR_NOT_RUNNING;
    }
    
    // Save current state
    bool old_shutdown_requested = daemon_shutdown_requested(ctx);
    
    // Send signal to self
    daemon_error_t result = daemon_send_signal(getpid(), signal);
    if (result != DAEMON_SUCCESS) {
        return result;
    }
    
    // Give the signal handler time to execute
    daemon_sleep_ms(100);
    
    // Check if signal was handled appropriately
    switch (signal) {
        case SIGTERM:
        case SIGINT:
            if (!daemon_shutdown_requested(ctx)) {
                return DAEMON_ERROR_SIGNAL_SETUP_FAILED;
            }
            // Restore original state for testing
            pthread_mutex_lock(&ctx->state_mutex);
            ctx->shutdown_requested = old_shutdown_requested;
            pthread_mutex_unlock(&ctx->state_mutex);
            break;
            
        case SIGHUP:
            // For SIGHUP, just check that the daemon is still responsive
            daemon_update_activity(ctx);
            break;
            
        default:
            return DAEMON_ERROR_INVALID_PARAMETER;
    }
    
    return DAEMON_SUCCESS;
}

/**
 * Gets a human-readable name for a signal.
 */
const char *daemon_signal_name(int signal)
{
    switch (signal) {
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
        case SIGCHLD: return "SIGCHLD";
        case SIGPIPE: return "SIGPIPE";
        case SIGKILL: return "SIGKILL";
        case SIGUSR1: return "SIGUSR1";
        case SIGUSR2: return "SIGUSR2";
        default:
            return "UNKNOWN";
    }
}