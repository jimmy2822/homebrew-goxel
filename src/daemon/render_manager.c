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

#include "render_manager.h"
#include "../log.h"
#include "../../ext_src/uthash/uthash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#ifdef __APPLE__
#include <Security/SecRandom.h>
#elif defined(__linux__)
#include <sys/random.h>
#endif

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * Generates a secure random hex token.
 */
static render_manager_error_t generate_secure_token(char *token_out, size_t token_size)
{
    if (!token_out || token_size < 9) {
        return RENDER_MGR_ERROR_INVALID_PARAMETER;
    }
    
    // Generate 4 random bytes (8 hex chars + null terminator)
    unsigned char random_bytes[4];
    
#ifdef __APPLE__
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof(random_bytes), random_bytes) != 0) {
        LOG_W("SecRandomCopyBytes failed, falling back to pseudo-random");
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 4; i++) {
            random_bytes[i] = (unsigned char)(rand() & 0xFF);
        }
    }
#elif defined(__linux__)
    if (getrandom(random_bytes, sizeof(random_bytes), 0) != sizeof(random_bytes)) {
        LOG_W("getrandom failed, falling back to pseudo-random");
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 4; i++) {
            random_bytes[i] = (unsigned char)(rand() & 0xFF);
        }
    }
#else
    // Fallback for other platforms
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 4; i++) {
        random_bytes[i] = (unsigned char)(rand() & 0xFF);
    }
#endif
    
    // Convert to hex string
    snprintf(token_out, token_size, "%02x%02x%02x%02x", 
             random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3]);
    
    return RENDER_MGR_SUCCESS;
}

/**
 * Creates directory with proper permissions if it doesn't exist.
 */
static render_manager_error_t ensure_directory_exists(const char *dir_path)
{
    if (!dir_path) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return RENDER_MGR_SUCCESS;
        } else {
            LOG_E("Path exists but is not a directory: %s", dir_path);
            return RENDER_MGR_ERROR_PERMISSION_DENIED;
        }
    }
    
    // Create directory with 0755 permissions
    if (mkdir(dir_path, 0755) != 0) {
        if (errno != EEXIST) {
            LOG_E("Failed to create directory %s: %s", dir_path, strerror(errno));
            return RENDER_MGR_ERROR_PERMISSION_DENIED;
        }
    }
    
    LOG_D("Created render directory: %s", dir_path);
    return RENDER_MGR_SUCCESS;
}

/**
 * Gets file size in bytes.
 */
static size_t get_file_size(const char *file_path)
{
    struct stat st;
    if (stat(file_path, &st) == 0) {
        return (size_t)st.st_size;
    }
    return 0;
}

/**
 * Simple checksum calculation (CRC32-like).
 */
static render_manager_error_t calculate_simple_checksum(const char *file_path, 
                                                       char *checksum_out, 
                                                       size_t checksum_size)
{
    if (!file_path || !checksum_out || checksum_size < 9) {
        return RENDER_MGR_ERROR_INVALID_PARAMETER;
    }
    
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        return RENDER_MGR_ERROR_FILE_NOT_FOUND;
    }
    
    uint32_t checksum = 0;
    uint8_t buffer[1024];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            checksum = ((checksum << 1) | (checksum >> 31)) ^ buffer[i];
        }
    }
    
    fclose(file);
    snprintf(checksum_out, checksum_size, "%08x", checksum);
    return RENDER_MGR_SUCCESS;
}

/**
 * Frees a render_info_t structure.
 */
static void free_render_info(render_info_t *info)
{
    if (!info) return;
    
    free(info->file_path);
    free(info->session_id);
    free(info->format);
    free(info->checksum);
    free(info);
}

// ============================================================================
// CORE FUNCTIONS IMPLEMENTATION
// ============================================================================

render_manager_t *render_manager_create(const char *output_dir, 
                                       size_t max_cache_size, 
                                       int ttl_seconds)
{
    render_manager_t *rm = calloc(1, sizeof(render_manager_t));
    if (!rm) {
        LOG_E("Failed to allocate render manager");
        return NULL;
    }
    
    // Check environment variables for configuration
    const char *env_dir = getenv(RENDER_MANAGER_ENV_DIR);
    const char *env_ttl = getenv(RENDER_MANAGER_ENV_TTL);
    const char *env_max_size = getenv(RENDER_MANAGER_ENV_MAX_SIZE);
    
    // Set output directory (priority: parameter > env > default)
    if (output_dir) {
        rm->output_dir = strdup(output_dir);
    } else if (env_dir) {
        rm->output_dir = strdup(env_dir);
        LOG_I("Using output directory from environment: %s", env_dir);
    } else {
        rm->output_dir = strdup(RENDER_MANAGER_DEFAULT_DIR);
    }
    
    if (!rm->output_dir) {
        LOG_E("Failed to allocate output directory string");
        free(rm);
        return NULL;
    }
    
    // Set TTL (priority: parameter > env > default)
    if (ttl_seconds > 0) {
        rm->ttl_seconds = ttl_seconds;
    } else if (env_ttl) {
        rm->ttl_seconds = atoi(env_ttl);
        LOG_I("Using TTL from environment: %d seconds", rm->ttl_seconds);
    } else {
        rm->ttl_seconds = RENDER_MANAGER_DEFAULT_TTL_SECONDS;
    }
    
    // Set max cache size (priority: parameter > env > default)
    if (max_cache_size > 0) {
        rm->max_cache_size = max_cache_size;
    } else if (env_max_size) {
        rm->max_cache_size = strtoull(env_max_size, NULL, 10);
        LOG_I("Using max cache size from environment: %zu bytes", rm->max_cache_size);
    } else {
        rm->max_cache_size = RENDER_MANAGER_DEFAULT_MAX_CACHE_SIZE;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&rm->mutex, NULL) != 0) {
        LOG_E("Failed to initialize render manager mutex");
        free(rm->output_dir);
        free(rm);
        return NULL;
    }
    
    // Create output directory
    if (ensure_directory_exists(rm->output_dir) != RENDER_MGR_SUCCESS) {
        LOG_E("Failed to create output directory: %s", rm->output_dir);
        pthread_mutex_destroy(&rm->mutex);
        free(rm->output_dir);
        free(rm);
        return NULL;
    }
    
    // Initialize hash table
    rm->active_renders = NULL;
    
    // Initialize statistics
    rm->total_renders = 0;
    rm->total_cleanups = 0;
    rm->current_cache_size = 0;
    rm->active_count = 0;
    
    LOG_I("Render manager created with dir=%s, max_size=%zu, ttl=%d", 
          rm->output_dir, rm->max_cache_size, rm->ttl_seconds);
    
    return rm;
}

void render_manager_destroy(render_manager_t *rm, bool cleanup_files)
{
    if (!rm) return;
    
    LOG_I("Destroying render manager (cleanup_files=%s)", cleanup_files ? "true" : "false");
    
    pthread_mutex_lock(&rm->mutex);
    
    // Clean up hash table
    render_info_t *info, *tmp;
    HASH_ITER(hh, rm->active_renders, info, tmp) {
        HASH_DEL(rm->active_renders, info);
        
        if (cleanup_files) {
            if (unlink(info->file_path) != 0) {
                LOG_W("Failed to remove file %s: %s", info->file_path, strerror(errno));
            }
        }
        
        free_render_info(info);
    }
    
    pthread_mutex_unlock(&rm->mutex);
    pthread_mutex_destroy(&rm->mutex);
    
    free(rm->output_dir);
    free(rm);
}

render_manager_error_t render_manager_create_path(render_manager_t *rm,
                                                 const char *session_id,
                                                 const char *format,
                                                 char *path_out,
                                                 size_t path_size)
{
    if (!rm || !format || !path_out || path_size == 0) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    // Generate timestamp
    time_t now = time(NULL);
    
    // Generate or use provided session ID
    char session_buffer[32];
    const char *session_to_use;
    
    if (session_id && strlen(session_id) > 0) {
        session_to_use = session_id;
    } else {
        snprintf(session_buffer, sizeof(session_buffer), "auto%ld", (long)now);
        session_to_use = session_buffer;
    }
    
    // Generate random token
    char token[16];
    render_manager_error_t result = generate_secure_token(token, sizeof(token));
    if (result != RENDER_MGR_SUCCESS) {
        LOG_E("Failed to generate secure token");
        return result;
    }
    
    // Construct path: render_[timestamp]_[session_id]_[token].[format]
    int ret = snprintf(path_out, path_size, "%s/render_%ld_%s_%s.%s",
                       rm->output_dir, (long)now, session_to_use, token, format);
    
    if (ret < 0 || (size_t)ret >= path_size) {
        LOG_E("Path buffer too small or formatting error");
        return RENDER_MGR_ERROR_PATH_TOO_LONG;
    }
    
    LOG_D("Generated render path: %s", path_out);
    return RENDER_MGR_SUCCESS;
}

render_manager_error_t render_manager_register(render_manager_t *rm,
                                              const char *file_path,
                                              const char *session_id,
                                              const char *format,
                                              int width,
                                              int height)
{
    if (!rm || !file_path || !session_id || !format) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    // Check if file already registered
    render_info_t *existing;
    HASH_FIND_STR(rm->active_renders, file_path, existing);
    if (existing) {
        pthread_mutex_unlock(&rm->mutex);
        LOG_W("File already registered: %s", file_path);
        return RENDER_MGR_ERROR_FILE_EXISTS;
    }
    
    // Create new render info
    render_info_t *info = calloc(1, sizeof(render_info_t));
    if (!info) {
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_ERROR_OUT_OF_MEMORY;
    }
    
    info->file_path = strdup(file_path);
    info->session_id = strdup(session_id);
    info->format = strdup(format);
    
    if (!info->file_path || !info->session_id || !info->format) {
        free_render_info(info);
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_ERROR_OUT_OF_MEMORY;
    }
    
    info->file_size = get_file_size(file_path);
    info->created_at = time(NULL);
    info->expires_at = info->created_at + rm->ttl_seconds;
    info->width = width;
    info->height = height;
    
    // Calculate checksum
    char checksum_buffer[16];
    if (calculate_simple_checksum(file_path, checksum_buffer, sizeof(checksum_buffer)) == RENDER_MGR_SUCCESS) {
        info->checksum = strdup(checksum_buffer);
    }
    
    // Set up hash key
    strncpy(info->path_key, file_path, sizeof(info->path_key) - 1);
    info->path_key[sizeof(info->path_key) - 1] = '\0';
    
    // Add to hash table
    HASH_ADD_STR(rm->active_renders, path_key, info);
    
    // Update statistics
    rm->total_renders++;
    rm->current_cache_size += info->file_size;
    rm->active_count++;
    
    pthread_mutex_unlock(&rm->mutex);
    
    LOG_D("Registered render: %s (%zu bytes, %dx%d)", 
          file_path, info->file_size, width, height);
    
    return RENDER_MGR_SUCCESS;
}

render_manager_error_t render_manager_cleanup_expired(render_manager_t *rm,
                                                     int *removed_count,
                                                     size_t *freed_bytes)
{
    if (!rm) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    time_t now = time(NULL);
    int removed = 0;
    size_t freed = 0;
    
    render_info_t *info, *tmp;
    HASH_ITER(hh, rm->active_renders, info, tmp) {
        if (now >= info->expires_at) {
            // Remove file
            if (unlink(info->file_path) == 0) {
                freed += info->file_size;
                removed++;
                LOG_D("Removed expired render: %s", info->file_path);
            } else {
                LOG_W("Failed to remove expired file %s: %s", 
                      info->file_path, strerror(errno));
            }
            
            // Remove from hash table
            HASH_DEL(rm->active_renders, info);
            rm->active_count--;
            free_render_info(info);
        }
    }
    
    rm->current_cache_size -= freed;
    rm->total_cleanups++;
    
    pthread_mutex_unlock(&rm->mutex);
    
    if (removed > 0) {
        LOG_I("Cleanup removed %d expired renders, freed %zu bytes", removed, freed);
    }
    
    if (removed_count) *removed_count = removed;
    if (freed_bytes) *freed_bytes = freed;
    
    return RENDER_MGR_SUCCESS;
}

render_manager_error_t render_manager_enforce_cache_limit(render_manager_t *rm,
                                                        int *removed_count,
                                                        size_t *freed_bytes)
{
    if (!rm) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    if (rm->current_cache_size <= rm->max_cache_size) {
        pthread_mutex_unlock(&rm->mutex);
        if (removed_count) *removed_count = 0;
        if (freed_bytes) *freed_bytes = 0;
        return RENDER_MGR_SUCCESS;
    }
    
    int removed = 0;
    size_t freed = 0;
    
    // Create array of renders sorted by creation time (oldest first)
    int count = HASH_COUNT(rm->active_renders);
    if (count == 0) {
        pthread_mutex_unlock(&rm->mutex);
        if (removed_count) *removed_count = 0;
        if (freed_bytes) *freed_bytes = 0;
        return RENDER_MGR_SUCCESS;
    }
    
    render_info_t **renders = malloc(count * sizeof(render_info_t*));
    if (!renders) {
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_ERROR_OUT_OF_MEMORY;
    }
    
    // Fill array
    int i = 0;
    render_info_t *info, *tmp;
    HASH_ITER(hh, rm->active_renders, info, tmp) {
        renders[i++] = info;
    }
    
    // Sort by creation time (simple bubble sort for small arrays)
    for (i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (renders[j]->created_at > renders[j + 1]->created_at) {
                render_info_t *temp = renders[j];
                renders[j] = renders[j + 1];
                renders[j + 1] = temp;
            }
        }
    }
    
    // Remove oldest files until under limit
    for (i = 0; i < count && rm->current_cache_size > rm->max_cache_size; i++) {
        info = renders[i];
        
        if (unlink(info->file_path) == 0) {
            freed += info->file_size;
            removed++;
            rm->current_cache_size -= info->file_size;
            LOG_D("Removed for cache limit: %s", info->file_path);
        }
        
        HASH_DEL(rm->active_renders, info);
        rm->active_count--;
        free_render_info(info);
    }
    
    free(renders);
    pthread_mutex_unlock(&rm->mutex);
    
    if (removed > 0) {
        LOG_I("Cache limit enforcement removed %d renders, freed %zu bytes", removed, freed);
    }
    
    if (removed_count) *removed_count = removed;
    if (freed_bytes) *freed_bytes = freed;
    
    return RENDER_MGR_SUCCESS;
}

// ============================================================================
// QUERY AND MANAGEMENT FUNCTIONS
// ============================================================================

render_manager_error_t render_manager_get_render_info(render_manager_t *rm,
                                                     const char *file_path,
                                                     render_info_t **info_out)
{
    if (!rm || !file_path) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    render_info_t *info;
    HASH_FIND_STR(rm->active_renders, file_path, info);
    
    if (info_out) {
        *info_out = info;
    }
    
    pthread_mutex_unlock(&rm->mutex);
    
    return info ? RENDER_MGR_SUCCESS : RENDER_MGR_ERROR_FILE_NOT_FOUND;
}

render_manager_error_t render_manager_remove_render(render_manager_t *rm,
                                                   const char *file_path)
{
    if (!rm || !file_path) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    render_info_t *info;
    HASH_FIND_STR(rm->active_renders, file_path, info);
    
    if (!info) {
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_ERROR_FILE_NOT_FOUND;
    }
    
    // Remove file
    if (unlink(file_path) != 0) {
        LOG_W("Failed to remove file %s: %s", file_path, strerror(errno));
    }
    
    // Remove from tracking
    HASH_DEL(rm->active_renders, info);
    rm->current_cache_size -= info->file_size;
    rm->active_count--;
    
    free_render_info(info);
    
    pthread_mutex_unlock(&rm->mutex);
    
    LOG_D("Removed render: %s", file_path);
    return RENDER_MGR_SUCCESS;
}

render_manager_error_t render_manager_list_renders(render_manager_t *rm,
                                                  render_info_t ***renders_out,
                                                  int *count_out)
{
    if (!rm || !renders_out || !count_out) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    *renders_out = NULL;
    *count_out = 0;
    
    pthread_mutex_lock(&rm->mutex);
    
    int count = rm->active_count;
    if (count == 0) {
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_SUCCESS;
    }
    
    // Allocate array for render info pointers
    render_info_t **renders = malloc(count * sizeof(render_info_t*));
    if (!renders) {
        pthread_mutex_unlock(&rm->mutex);
        return RENDER_MGR_ERROR_OUT_OF_MEMORY;
    }
    
    // Fill array with render info pointers
    int i = 0;
    render_info_t *info, *tmp;
    HASH_ITER(hh, rm->active_renders, info, tmp) {
        if (i < count) {
            renders[i++] = info;
        }
    }
    
    pthread_mutex_unlock(&rm->mutex);
    
    *renders_out = renders;
    *count_out = i;
    
    return RENDER_MGR_SUCCESS;
}

render_manager_error_t render_manager_get_stats(render_manager_t *rm,
                                               render_manager_stats_t *stats_out)
{
    if (!rm || !stats_out) {
        return RENDER_MGR_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&rm->mutex);
    
    stats_out->total_renders = rm->total_renders;
    stats_out->total_cleanups = rm->total_cleanups;
    stats_out->current_cache_size = rm->current_cache_size;
    stats_out->active_count = rm->active_count;
    stats_out->max_cache_size = rm->max_cache_size;
    stats_out->ttl_seconds = rm->ttl_seconds;
    stats_out->output_dir = rm->output_dir; // Note: shares pointer, do not free
    
    pthread_mutex_unlock(&rm->mutex);
    
    return RENDER_MGR_SUCCESS;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

render_manager_error_t render_manager_create_directory(const char *dir_path)
{
    return ensure_directory_exists(dir_path);
}

render_manager_error_t render_manager_generate_token(char *token_out, 
                                                    size_t token_size)
{
    return generate_secure_token(token_out, token_size);
}

render_manager_error_t render_manager_calculate_checksum(const char *file_path,
                                                        char *checksum_out,
                                                        size_t checksum_size)
{
    return calculate_simple_checksum(file_path, checksum_out, checksum_size);
}

bool render_manager_validate_path(const char *file_path, const char *base_dir)
{
    if (!file_path || !base_dir) {
        return false;
    }
    
    // Simple validation: check if path starts with base_dir
    size_t base_len = strlen(base_dir);
    if (strncmp(file_path, base_dir, base_len) != 0) {
        return false;
    }
    
    // Check for directory traversal patterns
    if (strstr(file_path, "..") != NULL) {
        return false;
    }
    
    return true;
}

const char *render_manager_error_string(render_manager_error_t error)
{
    switch (error) {
        case RENDER_MGR_SUCCESS:
            return "Success";
        case RENDER_MGR_ERROR_NULL_POINTER:
            return "NULL pointer";
        case RENDER_MGR_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case RENDER_MGR_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case RENDER_MGR_ERROR_FILE_EXISTS:
            return "File already exists";
        case RENDER_MGR_ERROR_FILE_NOT_FOUND:
            return "File not found";
        case RENDER_MGR_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case RENDER_MGR_ERROR_DISK_FULL:
            return "Disk full";
        case RENDER_MGR_ERROR_IO_ERROR:
            return "I/O error";
        case RENDER_MGR_ERROR_MUTEX_ERROR:
            return "Mutex error";
        case RENDER_MGR_ERROR_PATH_TOO_LONG:
            return "Path too long";
        case RENDER_MGR_ERROR_CACHE_FULL:
            return "Cache full";
        case RENDER_MGR_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

// ============================================================================
// BACKGROUND CLEANUP THREAD SUPPORT
// ============================================================================

static void *cleanup_thread_main(void *arg)
{
    render_cleanup_thread_t *cleanup_thread = (render_cleanup_thread_t*)arg;
    
    LOG_I("Render cleanup thread started (interval=%d seconds)", 
          cleanup_thread->cleanup_interval_seconds);
    
    while (true) {
        pthread_mutex_lock(&cleanup_thread->stop_mutex);
        bool should_stop = cleanup_thread->stop_requested;
        pthread_mutex_unlock(&cleanup_thread->stop_mutex);
        
        if (should_stop) {
            break;
        }
        
        // Perform cleanup
        int removed_count = 0;
        size_t freed_bytes = 0;
        
        render_manager_cleanup_expired(cleanup_thread->rm, &removed_count, &freed_bytes);
        render_manager_enforce_cache_limit(cleanup_thread->rm, NULL, NULL);
        
        // Sleep until next cleanup
        sleep(cleanup_thread->cleanup_interval_seconds);
    }
    
    LOG_I("Render cleanup thread stopping");
    return NULL;
}

render_cleanup_thread_t *render_manager_start_cleanup_thread(render_manager_t *rm,
                                                           int cleanup_interval_seconds)
{
    if (!rm) {
        return NULL;
    }
    
    render_cleanup_thread_t *cleanup_thread = calloc(1, sizeof(render_cleanup_thread_t));
    if (!cleanup_thread) {
        LOG_E("Failed to allocate cleanup thread structure");
        return NULL;
    }
    
    cleanup_thread->rm = rm;
    cleanup_thread->cleanup_interval_seconds = cleanup_interval_seconds > 0 ? 
        cleanup_interval_seconds : RENDER_MANAGER_DEFAULT_CLEANUP_INTERVAL;
    cleanup_thread->stop_requested = false;
    
    if (pthread_mutex_init(&cleanup_thread->stop_mutex, NULL) != 0) {
        LOG_E("Failed to initialize cleanup thread mutex");
        free(cleanup_thread);
        return NULL;
    }
    
    if (pthread_create(&cleanup_thread->thread_id, NULL, cleanup_thread_main, cleanup_thread) != 0) {
        LOG_E("Failed to create cleanup thread");
        pthread_mutex_destroy(&cleanup_thread->stop_mutex);
        free(cleanup_thread);
        return NULL;
    }
    
    return cleanup_thread;
}

void render_manager_stop_cleanup_thread(render_cleanup_thread_t *cleanup_thread)
{
    if (!cleanup_thread) {
        return;
    }
    
    pthread_mutex_lock(&cleanup_thread->stop_mutex);
    cleanup_thread->stop_requested = true;
    pthread_mutex_unlock(&cleanup_thread->stop_mutex);
    
    pthread_join(cleanup_thread->thread_id, NULL);
    pthread_mutex_destroy(&cleanup_thread->stop_mutex);
    
    free(cleanup_thread);
}