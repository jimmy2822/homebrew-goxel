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

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include "cli_interface.h"

cli_result_t cmd_create(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_open(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_save(cli_context_t *ctx, cli_args_t *args);

cli_result_t cmd_voxel_add(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_voxel_remove(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_voxel_paint(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_voxel_batch_add(cli_context_t *ctx, cli_args_t *args);

cli_result_t cmd_layer_create(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_layer_delete(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_layer_merge(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_layer_visibility(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_layer_rename(cli_context_t *ctx, cli_args_t *args);

cli_result_t cmd_render(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_export(cli_context_t *ctx, cli_args_t *args);
cli_result_t cmd_convert(cli_context_t *ctx, cli_args_t *args);

cli_result_t cmd_script(cli_context_t *ctx, cli_args_t *args);

cli_result_t register_project_commands(cli_context_t *ctx);
cli_result_t register_voxel_commands(cli_context_t *ctx);
cli_result_t register_layer_commands(cli_context_t *ctx);
cli_result_t register_render_commands(cli_context_t *ctx);
cli_result_t register_export_commands(cli_context_t *ctx);
cli_result_t register_script_commands(cli_context_t *ctx);

#endif // CLI_COMMANDS_H