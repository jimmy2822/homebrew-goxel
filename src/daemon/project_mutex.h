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

#ifndef PROJECT_MUTEX_H
#define PROJECT_MUTEX_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    bool has_active_project;
    char project_id[64];
    time_t last_activity;
} project_state_t;

// Global project state
extern project_state_t g_project_state;

// Initialize project mutex system
int project_mutex_init(void);

// Cleanup project mutex system  
void project_mutex_cleanup(void);

// Acquire project lock
int project_lock_acquire(const char *request_id);

// Release project lock
void project_lock_release(void);

// Check if project is idle (for auto-cleanup)
bool project_is_idle(int timeout_seconds);

#endif