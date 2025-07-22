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

#ifndef CAMERA_HEADLESS_H
#define CAMERA_HEADLESS_H

#include <stdbool.h>

// Forward declaration
typedef struct camera camera_t;

/*
 * Function: headless_camera_set_preset
 * Set camera to a predefined preset view.
 *
 * Parameters:
 *   camera - Camera to modify
 *   preset_name - Name of preset ("front", "back", "left", "right", 
 *                 "top", "bottom", "isometric", "default")
 *
 * Return:
 *   0 on success, -1 on error or if preset not found
 */
int headless_camera_set_preset(camera_t *camera, const char *preset_name);

/*
 * Function: headless_camera_set_preset_angles
 * Set camera to custom rotation angles.
 *
 * Parameters:
 *   camera - Camera to modify
 *   rz_degrees - Z-axis rotation in degrees
 *   rx_degrees - X-axis rotation in degrees
 *
 * Return:
 *   0 on success, -1 on error
 */
int headless_camera_set_preset_angles(camera_t *camera, float rz_degrees, float rx_degrees);

/*
 * Function: headless_camera_set_position
 * Set camera position and target (look-at point).
 *
 * Parameters:
 *   camera - Camera to modify
 *   position - Camera position [x, y, z]
 *   target - Target position to look at [x, y, z]
 *
 * Return:
 *   0 on success, -1 on error
 */
int headless_camera_set_position(camera_t *camera, const float position[3], 
                                const float target[3]);

/*
 * Function: headless_camera_set_orthographic
 * Set camera projection mode.
 *
 * Parameters:
 *   camera - Camera to modify
 *   ortho - true for orthographic, false for perspective
 *
 * Return:
 *   0 on success, -1 on error
 */
int headless_camera_set_orthographic(camera_t *camera, bool ortho);

/*
 * Function: headless_camera_set_distance
 * Set camera distance from target.
 *
 * Parameters:
 *   camera - Camera to modify
 *   distance - Distance from target (must be positive)
 *
 * Return:
 *   0 on success, -1 on error
 */
int headless_camera_set_distance(camera_t *camera, float distance);

/*
 * Function: headless_camera_get_preset_names
 * Get list of available camera presets.
 *
 * Parameters:
 *   count - Optional pointer to receive count of presets
 *
 * Return:
 *   Array of preset name strings
 */
const char **headless_camera_get_preset_names(int *count);

/*
 * Function: headless_camera_fit_box
 * Adjust camera to fit a bounding box in view.
 *
 * Parameters:
 *   camera - Camera to modify
 *   box - Bounding box to fit [4][4] matrix
 *
 * Return:
 *   0 on success, -1 on error
 */
int headless_camera_fit_box(camera_t *camera, const float box[4][4]);

#endif // CAMERA_HEADLESS_H