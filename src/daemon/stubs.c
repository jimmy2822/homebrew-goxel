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

/**
 * Stub functions for daemon mode.
 * These functions are required by various parts of the code but don't
 * have meaningful implementations in daemon mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>

// Forward declarations to avoid including full headers
typedef struct action action_t;
typedef struct volume volume_t;
typedef struct material material_t;
typedef struct renderer renderer_t;

// System functions
void sys_log(const char *format, ...)
{
    // Daemon logs to stderr
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void sys_on_saved(const char *path)
{
    // No-op in daemon mode
    (void)path;
}

int sys_open_file_dialog(const char *filters, const char *title, 
                        const char *default_path, char *path, int path_size)
{
    // File dialogs not supported in daemon mode
    (void)filters;
    (void)title;
    (void)default_path;
    (void)path;
    (void)path_size;
    return 0;
}

// Translation function
const char *tr(const char *key)
{
    // No translation in daemon mode, return key as-is
    return key;
}

// Texture stub is already defined in headless/goxel_headless.c

// Additional system functions
int sys_get_save_path(const char *filters, const char *title, 
                     const char *default_path, char *path, int path_size)
{
    // Save dialogs not supported in daemon mode
    (void)filters;
    (void)title;
    (void)default_path;
    (void)path;
    (void)path_size;
    return 0;
}

double sys_get_time(void)
{
    // Return current time in seconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

const char *sys_get_user_dir(void)
{
    // Return home directory
    static char *home = NULL;
    if (!home) {
        home = getenv("HOME");
        if (!home) home = ".";
    }
    return home;
}

int sys_iter_paths(const char *path, int flags, 
                  void (*callback)(const char *path, void *user), void *user)
{
    // Path iteration not implemented in daemon mode
    (void)path;
    (void)flags;
    (void)callback;
    (void)user;
    return 0;
}

typedef struct { char d_name[256]; } sys_dir_entry_t;
int sys_list_dir(const char *path, sys_dir_entry_t **entries)
{
    // Directory listing not implemented in daemon mode
    (void)path;
    *entries = NULL;
    return 0;
}

// Render functions
void render_get_light_dir(float out[3])
{
    // Default light direction
    out[0] = 0.577f;  // Normalized (1, 1, 1)
    out[1] = 0.577f;
    out[2] = 0.577f;
}

// Action system stub
void action_register(const action_t *action, int idx)
{
    // Actions not supported in daemon mode
    (void)action;
    (void)idx;
}

// Render functions
void render_submit(renderer_t *rend, const float viewport[4],
                   const uint8_t clear_color[4])
{
    // Rendering not supported in daemon mode
    (void)rend;
    (void)viewport;
    (void)clear_color;
}

void render_volume(renderer_t *rend, const volume_t *volume,
                   const material_t *material, int effects)
{
    // Volume rendering not supported in daemon mode
    (void)rend;
    (void)volume;
    (void)material;
    (void)effects;
}