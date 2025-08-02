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

#include "project_mutex.h"
#include "../log.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Global project state
project_state_t g_project_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .has_active_project = false,
    .project_id = "",
    .last_activity = 0
};

int project_mutex_init(void)
{
    int ret = pthread_mutex_init(&g_project_state.mutex, NULL);
    if (ret != 0) {
        LOG_E("Failed to initialize project mutex: %s", strerror(ret));
        return -1;
    }
    
    g_project_state.has_active_project = false;
    memset(g_project_state.project_id, 0, sizeof(g_project_state.project_id));
    g_project_state.last_activity = 0;
    
    LOG_I("Project mutex system initialized");
    return 0;
}

void project_mutex_cleanup(void)
{
    pthread_mutex_destroy(&g_project_state.mutex);
    LOG_I("Project mutex system cleaned up");
}

int project_lock_acquire(const char *request_id)
{
    if (!request_id) {
        LOG_E("Invalid request_id for project lock");
        return -1;
    }
    
    LOG_D("Attempting to acquire project lock for request: %s", request_id);
    
    // macOS doesn't have pthread_mutex_timedlock, use trylock with retry
    int attempts = 50; // 5 seconds with 100ms sleeps
    int ret;
    
    while (attempts > 0) {
        ret = pthread_mutex_trylock(&g_project_state.mutex);
        if (ret == 0) {
            // Success
            break;
        } else if (ret == EBUSY) {
            // Mutex is locked, wait and retry
            usleep(100000); // 100ms
            attempts--;
        } else {
            // Other error
            LOG_E("Failed to acquire project lock: %s", strerror(ret));
            return -1;
        }
    }
    
    if (attempts == 0) {
        LOG_W("Project lock acquisition timed out for request: %s", request_id);
        return -1;
    }
    
    LOG_I("Project lock acquired for request: %s", request_id);
    g_project_state.last_activity = time(NULL);
    return 0;
}

void project_lock_release(void)
{
    pthread_mutex_unlock(&g_project_state.mutex);
    LOG_D("Project lock released");
}

bool project_is_idle(int timeout_seconds)
{
    if (!g_project_state.has_active_project) {
        return false;
    }
    
    time_t now = time(NULL);
    time_t idle_time = now - g_project_state.last_activity;
    
    return (idle_time >= timeout_seconds);
}