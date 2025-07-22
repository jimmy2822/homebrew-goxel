/* Goxel 3D voxels editor - Project Management
 *
 * copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PROJECT_MGMT_H
#define PROJECT_MGMT_H

#include "goxel_core.h"

// Project metadata structure
typedef struct {
    char name[256];
    char author[128];
    char description[512];
    char creation_date[32];
    char last_modified[32];
    int version_major;
    int version_minor;
    int layer_count;
    int voxel_count;
    float bbox[2][3]; // min, max
} project_metadata_t;

// Project management functions
int project_create(goxel_core_t *ctx, const char *name, const char *path);
int project_open(goxel_core_t *ctx, const char *path);
int project_save(goxel_core_t *ctx, const char *path);
int project_save_as(goxel_core_t *ctx, const char *old_path, const char *new_path);
int project_close(goxel_core_t *ctx);

// Project metadata functions
int project_get_metadata(goxel_core_t *ctx, project_metadata_t *metadata);
int project_set_metadata(goxel_core_t *ctx, const project_metadata_t *metadata);
int project_update_stats(goxel_core_t *ctx);

// Session management
int project_auto_save(goxel_core_t *ctx, const char *backup_dir);
int project_load_backup(goxel_core_t *ctx, const char *backup_path);
int project_clean_backups(const char *backup_dir, int max_backups);

// Recent files management
int project_add_recent(goxel_core_t *ctx, const char *path);
int project_get_recent(goxel_core_t *ctx, int index, char *path, size_t path_size);
int project_clear_recent(goxel_core_t *ctx);

// Project validation
int project_validate(const char *path);
int project_check_compatibility(const char *path, int *version_major, int *version_minor);

#endif // PROJECT_MGMT_H