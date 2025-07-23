/* Goxel 3D voxels editor
 *
 * copyright (c) 2023-present Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef SCRIPT_H
#define SCRIPT_H

/*
 * Function: script_run_from_file
 * Run a JavaScript script from a file.
 */
int script_run_from_file(const char *filename, int argc, const char **argv);

/*
 * Function: script_run_from_string
 * Run JavaScript code from a string.
 */
int script_run_from_string(const char *script_code, const char *source_name);

void script_init(void);

/*
 * List all the registered scripts to show in the script menu.
 */
void script_iter_all(void *user, void (*f)(void *user, const char *name));

/*
 * Execute a registered script.
 */
int script_execute(const char *name);


#endif // SCRIPT_H
