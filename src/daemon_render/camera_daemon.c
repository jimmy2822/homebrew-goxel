/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"
#include "camera_daemon.h"

typedef enum {
    CAMERA_PRESET_FRONT,
    CAMERA_PRESET_BACK,
    CAMERA_PRESET_LEFT,
    CAMERA_PRESET_RIGHT,
    CAMERA_PRESET_TOP,
    CAMERA_PRESET_BOTTOM,
    CAMERA_PRESET_ISOMETRIC,
    CAMERA_PRESET_DEFAULT
} camera_preset_t;

typedef struct {
    const char *name;
    float rz;  // Z rotation in radians
    float rx;  // X rotation in radians
} preset_info_t;

static const preset_info_t CAMERA_PRESETS[] = {
    {"front", 0.0f, M_PI / 2.0f},        // Front: {0, 90}
    {"back", M_PI, M_PI / 2.0f},         // Back: opposite of front
    {"left", M_PI / 2.0f, M_PI / 2.0f},  // Left: {90, 90}
    {"right", -M_PI / 2.0f, M_PI / 2.0f}, // Right: {-90, 90}
    {"top", 0.0f, 0.0f},                 // Top: {0, 0}
    {"bottom", 0.0f, M_PI},              // Bottom: opposite of top
    {"isometric", M_PI / 4.0f, M_PI / 4.0f}, // Isometric: {45, 45}
    {"default", M_PI / 4.0f, M_PI / 4.0f}    // Default: same as isometric
};

static void headless_camera_apply_preset(camera_t *camera, const preset_info_t *preset)
{
    if (!camera || !preset) return;
    
    // Reset camera matrix
    mat4_set_identity(camera->mat);
    mat4_itranslate(camera->mat, 0, 0, camera->dist);
    
    // Apply rotation angles
    camera_turntable(camera, preset->rz, preset->rx);
}

int headless_camera_set_preset(camera_t *camera, const char *preset_name)
{
    if (!camera || !preset_name) return -1;
    
    size_t num_presets = sizeof(CAMERA_PRESETS) / sizeof(CAMERA_PRESETS[0]);
    
    for (size_t i = 0; i < num_presets; i++) {
        if (strcmp(CAMERA_PRESETS[i].name, preset_name) == 0) {
            headless_camera_apply_preset(camera, &CAMERA_PRESETS[i]);
            return 0;
        }
    }
    
    return -1; // Preset not found
}

int headless_camera_set_preset_angles(camera_t *camera, float rz_degrees, float rx_degrees)
{
    if (!camera) return -1;
    
    preset_info_t custom_preset = {
        "custom",
        rz_degrees * M_PI / 180.0f,
        rx_degrees * M_PI / 180.0f
    };
    
    headless_camera_apply_preset(camera, &custom_preset);
    return 0;
}

int headless_camera_set_position(camera_t *camera, const float position[3], 
                                const float target[3])
{
    if (!camera || !position || !target) return -1;
    
    float dir[3], up[3] = {0, 0, 1};
    
    // Calculate direction vector
    vec3_sub(target, position, dir);
    float dist = vec3_norm(dir);
    vec3_normalize(dir, dir);
    
    // Set camera distance
    camera->dist = dist;
    
    // Create camera matrix from position and direction
    mat4_set_identity(camera->mat);
    mat4_itranslate(camera->mat, position[0], position[1], position[2]);
    
    // Orient camera to look at target
    float right[3], camera_up[3];
    vec3_cross(dir, up, right);
    vec3_normalize(right, right);
    vec3_cross(right, dir, camera_up);
    vec3_normalize(camera_up, camera_up);
    
    // Build rotation matrix
    camera->mat[0][0] = right[0];    camera->mat[0][1] = right[1];    camera->mat[0][2] = right[2];
    camera->mat[1][0] = camera_up[0]; camera->mat[1][1] = camera_up[1]; camera->mat[1][2] = camera_up[2];
    camera->mat[2][0] = -dir[0];     camera->mat[2][1] = -dir[1];     camera->mat[2][2] = -dir[2];
    
    return 0;
}

int headless_camera_set_orthographic(camera_t *camera, bool ortho)
{
    if (!camera) return -1;
    
    camera->ortho = ortho;
    return 0;
}

int headless_camera_set_distance(camera_t *camera, float distance)
{
    if (!camera || distance <= 0) return -1;
    
    camera->dist = distance;
    
    // Update camera position to maintain current orientation
    float center[3];
    mat4_mul_vec3(camera->mat, VEC(0, 0, -distance), center);
    
    // Rebuild matrix with new distance
    mat4_itranslate(camera->mat, 0, 0, distance);
    
    return 0;
}

const char **headless_camera_get_preset_names(int *count)
{
    static const char *preset_names[8];
    static bool initialized = false;
    
    if (!initialized) {
        size_t num_presets = sizeof(CAMERA_PRESETS) / sizeof(CAMERA_PRESETS[0]);
        for (size_t i = 0; i < num_presets; i++) {
            preset_names[i] = CAMERA_PRESETS[i].name;
        }
        initialized = true;
    }
    
    if (count) {
        *count = sizeof(CAMERA_PRESETS) / sizeof(CAMERA_PRESETS[0]);
    }
    
    return preset_names;
}

int headless_camera_fit_box(camera_t *camera, const float box[4][4])
{
    if (!camera) return -1;
    
    camera_fit_box(camera, box);
    return 0;
}