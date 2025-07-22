/* Goxel 3D voxels editor - Project Management Implementation
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

#include "project_mgmt.h"
#include "utils/path.h"
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

static void get_current_timestamp(char *buffer, size_t buffer_size)
{
    time_t rawtime;
    struct tm *timeinfo;
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

int project_create(goxel_core_t *ctx, const char *name, const char *path)
{
    if (!ctx || !name) return -1;
    
    // Create new image
    if (ctx->image) {
        image_delete(ctx->image);
    }
    
    ctx->image = image_new();
    if (!ctx->image) return -1;
    
    // Set project metadata
    if (path) {
        snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", path);
    }
    
    // Create default layer
    layer_t *layer = image_add_layer(ctx->image, NULL);
    if (layer) {
        snprintf(layer->name, sizeof(layer->name), "Main");
        ctx->image->active_layer = layer;
    }
    
    return 0;
}

int project_open(goxel_core_t *ctx, const char *path)
{
    if (!ctx || !path) return -1;
    
    // Check if file exists and is readable
    struct stat st;
    if (stat(path, &st) != 0) {
        return -errno;
    }
    
    // Validate project file
    int validation_result = project_validate(path);
    if (validation_result != 0) {
        return validation_result;
    }
    
    // Load the project
    image_t *img = image_new();
    if (!img) return -1;
    
    // Use file format system to load
    const char *format = NULL;
    const char *ext = path_get_ext(path);
    if (ext) {
        if (strcmp(ext, "gox") == 0) format = "gox";
        else if (strcmp(ext, "vox") == 0) format = "vox";
        else if (strcmp(ext, "obj") == 0) format = "wavefront";
        else if (strcmp(ext, "ply") == 0) format = "ply";
    }
    
    int ret = goxel_import_file(path, format);
    if (ret != 0) {
        image_delete(img);
        return ret;
    }
    
    // Replace current image
    if (ctx->image) {
        image_delete(ctx->image);
    }
    
    ctx->image = img;
    snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", path);
    
    // Add to recent files
    project_add_recent(ctx, path);
    
    return 0;
}

int project_save(goxel_core_t *ctx, const char *path)
{
    if (!ctx || !ctx->image) return -1;
    
    const char *save_path = path ? path : ctx->image->path;
    if (!save_path || strlen(save_path) == 0) return -1;
    
    // Auto-detect format from extension
    const char *format = NULL;
    const char *ext = path_get_ext(save_path);
    if (ext) {
        if (strcmp(ext, "gox") == 0) format = "gox";
        else if (strcmp(ext, "vox") == 0) format = "vox";
        else if (strcmp(ext, "obj") == 0) format = "wavefront";
        else if (strcmp(ext, "ply") == 0) format = "ply";
    }
    
    int ret = goxel_export_to_file(save_path, format);
    if (ret == 0) {
        snprintf(ctx->image->path, sizeof(ctx->image->path), "%s", save_path);
        project_add_recent(ctx, save_path);
    }
    
    return ret;
}

int project_save_as(goxel_core_t *ctx, const char *old_path, const char *new_path)
{
    if (!ctx || !new_path) return -1;
    
    return project_save(ctx, new_path);
}

int project_close(goxel_core_t *ctx)
{
    if (!ctx) return -1;
    
    if (ctx->image) {
        image_delete(ctx->image);
        ctx->image = NULL;
    }
    
    return 0;
}

int project_get_metadata(goxel_core_t *ctx, project_metadata_t *metadata)
{
    if (!ctx || !metadata || !ctx->image) return -1;
    
    memset(metadata, 0, sizeof(*metadata));
    
    // Get basic info from image
    const char *path = ctx->image->path;
    if (path && strlen(path) > 0) {
        const char *basename = path_get_basename(path);
        snprintf(metadata->name, sizeof(metadata->name), "%s", basename);
    } else {
        snprintf(metadata->name, sizeof(metadata->name), "Untitled");
    }
    
    // Get layer count
    metadata->layer_count = 0;
    layer_t *layer;
    DL_FOREACH(ctx->image->layers, layer) {
        metadata->layer_count++;
    }
    
    // Update project statistics
    project_update_stats(ctx);
    
    // Get bounding box
    if (ctx->image->active_layer && ctx->image->active_layer->volume) {
        float bbox[2][3];
        volume_get_bbox(ctx->image->active_layer->volume, bbox, false);
        memcpy(metadata->bbox, bbox, sizeof(bbox));
    }
    
    // Version info
    metadata->version_major = 0;
    metadata->version_minor = 15;
    
    get_current_timestamp(metadata->last_modified, sizeof(metadata->last_modified));
    
    return 0;
}

int project_set_metadata(goxel_core_t *ctx, const project_metadata_t *metadata)
{
    if (!ctx || !metadata || !ctx->image) return -1;
    
    // Currently metadata is mostly read-only from the image structure
    // In the future, we could store additional metadata in the image
    
    return 0;
}

int project_update_stats(goxel_core_t *ctx)
{
    if (!ctx || !ctx->image) return -1;
    
    // This would update voxel counts and other statistics
    // Implementation depends on performance requirements
    
    return 0;
}

int project_auto_save(goxel_core_t *ctx, const char *backup_dir)
{
    if (!ctx || !ctx->image || !backup_dir) return -1;
    
    // Create backup directory if it doesn't exist
    struct stat st;
    if (stat(backup_dir, &st) != 0) {
        if (mkdir(backup_dir, 0755) != 0) {
            return -errno;
        }
    }
    
    // Generate backup filename with timestamp
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    
    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s/autosave_%04d%02d%02d_%02d%02d%02d.gox",
             backup_dir, 
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    return project_save(ctx, backup_path);
}

int project_load_backup(goxel_core_t *ctx, const char *backup_path)
{
    return project_open(ctx, backup_path);
}

int project_clean_backups(const char *backup_dir, int max_backups)
{
    // This would implement backup file cleanup
    // For now, just return success
    return 0;
}

int project_add_recent(goxel_core_t *ctx, const char *path)
{
    if (!ctx || !path) return -1;
    
    // Shift existing entries down
    for (int i = 7; i > 0; i--) {
        strcpy(ctx->recent_files[i], ctx->recent_files[i-1]);
    }
    
    // Add new entry at the top
    snprintf(ctx->recent_files[0], sizeof(ctx->recent_files[0]), "%s", path);
    
    return 0;
}

int project_get_recent(goxel_core_t *ctx, int index, char *path, size_t path_size)
{
    if (!ctx || !path || index < 0 || index >= 8) return -1;
    
    if (strlen(ctx->recent_files[index]) == 0) return -1;
    
    snprintf(path, path_size, "%s", ctx->recent_files[index]);
    return 0;
}

int project_clear_recent(goxel_core_t *ctx)
{
    if (!ctx) return -1;
    
    for (int i = 0; i < 8; i++) {
        ctx->recent_files[i][0] = '\0';
    }
    
    return 0;
}

int project_validate(const char *path)
{
    if (!path) return -1;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return -errno;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return -1; // Not a regular file
    }
    
    // Additional validation could be added here
    // For now, just check if file exists and is readable
    
    return 0;
}

int project_check_compatibility(const char *path, int *version_major, int *version_minor)
{
    if (!path || !version_major || !version_minor) return -1;
    
    *version_major = 0;
    *version_minor = 15;
    
    // This would parse file headers to check version compatibility
    // For now, assume all files are compatible
    
    return 0;
}