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

#include "cli_commands.h"
#include "../core/goxel_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cli_result_t cmd_create(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *project_name = cli_get_option_string(args, "name", NULL);
    const char *output_file = cli_get_option_string(args, "output", NULL);
    int width = cli_get_option_int(args, "width", 64);
    int height = cli_get_option_int(args, "height", 64);
    int depth = cli_get_option_int(args, "depth", 64);
    
    if (!output_file) {
        if (cli_get_positional_count(args) > 0) {
            output_file = cli_get_positional_arg(args, 0);
        } else {
            fprintf(stderr, "Error: Output file not specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Creating new project");
        if (project_name) printf(" '%s'", project_name);
        printf(" with dimensions %dx%dx%d\n", width, height, depth);
        printf("Output file: %s\n", output_file);
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_create_project(goxel_ctx, project_name, width, height, depth) != 0) {
        fprintf(stderr, "Error: Failed to create project\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_save_project(goxel_ctx, output_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", output_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Project created successfully: %s\n", output_file);
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_open(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *input_file = cli_get_option_string(args, "input", NULL);
    bool read_only = cli_get_option_bool(args, "read-only", false);
    
    if (!input_file) {
        if (cli_get_positional_count(args) > 0) {
            input_file = cli_get_positional_arg(args, 0);
        } else {
            fprintf(stderr, "Error: Input file not specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Opening project: %s", input_file);
        if (read_only) printf(" (read-only mode)");
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_load_project(goxel_ctx, input_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", input_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    if (read_only) {
        goxel_core_set_read_only(goxel_ctx, true);
    }
    
    if (!ctx->quiet) {
        printf("Project opened successfully\n");
        
        int layer_count = goxel_core_get_layer_count(goxel_ctx);
        printf("Layers: %d\n", layer_count);
        
        int width, height, depth;
        if (goxel_core_get_project_bounds(goxel_ctx, &width, &height, &depth) == 0) {
            printf("Dimensions: %dx%dx%d\n", width, height, depth);
        }
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_save(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *output_file = cli_get_option_string(args, "output", NULL);
    bool create_backup = cli_get_option_bool(args, "backup", true);
    const char *format = cli_get_option_string(args, "format", NULL);
    
    if (!output_file) {
        if (cli_get_positional_count(args) > 0) {
            output_file = cli_get_positional_arg(args, 0);
        } else {
            fprintf(stderr, "Error: Output file not specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Saving project to: %s", output_file);
        if (format) printf(" (format: %s)", format);
        if (create_backup) printf(" (with backup)");
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (create_backup && goxel_core_create_backup(goxel_ctx, output_file) != 0) {
        fprintf(stderr, "Warning: Failed to create backup file\n");
    }
    
    int result;
    if (format) {
        result = goxel_core_save_project_format(goxel_ctx, output_file, format);
    } else {
        result = goxel_core_save_project(goxel_ctx, output_file);
    }
    
    if (result != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", output_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Project saved successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_voxel_add(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *pos_str = cli_get_option_string(args, "pos", NULL);
    const char *color_str = cli_get_option_string(args, "color", "255,255,255,255");
    int layer_id = cli_get_option_int(args, "layer", -1);
    
    if (!pos_str) {
        fprintf(stderr, "Error: Position not specified (use --pos x,y,z)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    int x, y, z;
    if (sscanf(pos_str, "%d,%d,%d", &x, &y, &z) != 3) {
        fprintf(stderr, "Error: Invalid position format '%s' (expected: x,y,z)\n", pos_str);
        return CLI_ERROR_INVALID_ARGS;
    }
    
    int r, g, b, a = 255;
    if (sscanf(color_str, "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
        if (sscanf(color_str, "%d,%d,%d", &r, &g, &b) != 3) {
            fprintf(stderr, "Error: Invalid color format '%s' (expected: r,g,b or r,g,b,a)\n", color_str);
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Adding voxel at (%d,%d,%d) with color (%d,%d,%d,%d)", x, y, z, r, g, b, a);
        if (layer_id >= 0) printf(" on layer %d", layer_id);
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    uint8_t color[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    if (goxel_core_add_voxel(goxel_ctx, x, y, z, color, layer_id) != 0) {
        fprintf(stderr, "Error: Failed to add voxel\n");
        return CLI_ERROR_VOXEL_OPERATION_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Voxel added successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_voxel_remove(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *pos_str = cli_get_option_string(args, "pos", NULL);
    const char *box_str = cli_get_option_string(args, "box", NULL);
    int layer_id = cli_get_option_int(args, "layer", -1);
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (box_str) {
        int x1, y1, z1, x2, y2, z2;
        if (sscanf(box_str, "%d,%d,%d,%d,%d,%d", &x1, &y1, &z1, &x2, &y2, &z2) != 6) {
            fprintf(stderr, "Error: Invalid box format '%s' (expected: x1,y1,z1,x2,y2,z2)\n", box_str);
            return CLI_ERROR_INVALID_ARGS;
        }
        
        if (!ctx->quiet) {
            printf("Removing voxels in box (%d,%d,%d) to (%d,%d,%d)", x1, y1, z1, x2, y2, z2);
            if (layer_id >= 0) printf(" on layer %d", layer_id);
            printf("\n");
        }
        
        if (goxel_core_remove_voxels_in_box(goxel_ctx, x1, y1, z1, x2, y2, z2, layer_id) != 0) {
            fprintf(stderr, "Error: Failed to remove voxels in box\n");
            return CLI_ERROR_VOXEL_OPERATION_FAILED;
        }
        
    } else if (pos_str) {
        int x, y, z;
        if (sscanf(pos_str, "%d,%d,%d", &x, &y, &z) != 3) {
            fprintf(stderr, "Error: Invalid position format '%s' (expected: x,y,z)\n", pos_str);
            return CLI_ERROR_INVALID_ARGS;
        }
        
        if (!ctx->quiet) {
            printf("Removing voxel at (%d,%d,%d)", x, y, z);
            if (layer_id >= 0) printf(" on layer %d", layer_id);
            printf("\n");
        }
        
        if (goxel_core_remove_voxel(goxel_ctx, x, y, z, layer_id) != 0) {
            fprintf(stderr, "Error: Failed to remove voxel\n");
            return CLI_ERROR_VOXEL_OPERATION_FAILED;
        }
        
    } else {
        fprintf(stderr, "Error: Position or box not specified (use --pos x,y,z or --box x1,y1,z1,x2,y2,z2)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        printf("Voxel(s) removed successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_voxel_paint(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *pos_str = cli_get_option_string(args, "pos", NULL);
    const char *color_str = cli_get_option_string(args, "color", NULL);
    int layer_id = cli_get_option_int(args, "layer", -1);
    
    if (!pos_str) {
        fprintf(stderr, "Error: Position not specified (use --pos x,y,z)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!color_str) {
        fprintf(stderr, "Error: Color not specified (use --color r,g,b,a)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    int x, y, z;
    if (sscanf(pos_str, "%d,%d,%d", &x, &y, &z) != 3) {
        fprintf(stderr, "Error: Invalid position format '%s' (expected: x,y,z)\n", pos_str);
        return CLI_ERROR_INVALID_ARGS;
    }
    
    int r, g, b, a = 255;
    if (sscanf(color_str, "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
        if (sscanf(color_str, "%d,%d,%d", &r, &g, &b) != 3) {
            fprintf(stderr, "Error: Invalid color format '%s' (expected: r,g,b or r,g,b,a)\n", color_str);
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Painting voxel at (%d,%d,%d) with color (%d,%d,%d,%d)", x, y, z, r, g, b, a);
        if (layer_id >= 0) printf(" on layer %d", layer_id);
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    uint8_t color[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    if (goxel_core_paint_voxel(goxel_ctx, x, y, z, color, layer_id) != 0) {
        fprintf(stderr, "Error: Failed to paint voxel\n");
        return CLI_ERROR_VOXEL_OPERATION_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Voxel painted successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t register_project_commands(cli_context_t *ctx)
{
    if (!ctx) return CLI_ERROR_INVALID_ARGS;
    
    cli_result_t result;
    
    result = cli_register_command(ctx, "create", 
                                 "Create a new voxel project",
                                 "[OPTIONS] <output-file>",
                                 cmd_create);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "create", "n", "name", "Project name", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "create", "w", "width", "Project width in voxels", 
                               CLI_OPT_INT, false, NULL, 64, 0.0f, false);
    cli_add_option_with_default(ctx, "create", "h", "height", "Project height in voxels",
                               CLI_OPT_INT, false, NULL, 64, 0.0f, false);
    cli_add_option_with_default(ctx, "create", "d", "depth", "Project depth in voxels",
                               CLI_OPT_INT, false, NULL, 64, 0.0f, false);
    cli_add_option(ctx, "create", "o", "output", "Output file path", CLI_OPT_STRING, false);
    
    result = cli_register_command(ctx, "open",
                                 "Open an existing voxel project",
                                 "[OPTIONS] <input-file>",
                                 cmd_open);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "open", "i", "input", "Input file path", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "open", "r", "read-only", "Open in read-only mode",
                               CLI_OPT_BOOL, false, NULL, 0, 0.0f, false);
    
    result = cli_register_command(ctx, "save",
                                 "Save the current project",
                                 "[OPTIONS] <output-file>",
                                 cmd_save);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "save", "o", "output", "Output file path", CLI_OPT_STRING, false);
    cli_add_option(ctx, "save", "f", "format", "Output format", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "save", "b", "backup", "Create backup file",
                               CLI_OPT_BOOL, false, NULL, 0, 0.0f, true);
    
    return CLI_SUCCESS;
}

cli_result_t register_voxel_commands(cli_context_t *ctx)
{
    if (!ctx) return CLI_ERROR_INVALID_ARGS;
    
    cli_result_t result;
    
    result = cli_register_command(ctx, "voxel-add",
                                 "Add a voxel at the specified position",
                                 "[OPTIONS]",
                                 cmd_voxel_add);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "voxel-add", "p", "pos", "Voxel position (x,y,z)", CLI_OPT_STRING, true);
    cli_add_option_with_default(ctx, "voxel-add", "c", "color", "Voxel color (r,g,b,a)",
                               CLI_OPT_STRING, false, "255,255,255,255", 0, 0.0f, false);
    cli_add_option_with_default(ctx, "voxel-add", "l", "layer", "Layer ID (-1 for active layer)",
                               CLI_OPT_INT, false, NULL, -1, 0.0f, false);
    
    result = cli_register_command(ctx, "voxel-remove",
                                 "Remove voxel(s) at the specified position or area",
                                 "[OPTIONS]",
                                 cmd_voxel_remove);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "voxel-remove", "p", "pos", "Voxel position (x,y,z)", CLI_OPT_STRING, false);
    cli_add_option(ctx, "voxel-remove", "b", "box", "Box area (x1,y1,z1,x2,y2,z2)", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "voxel-remove", "l", "layer", "Layer ID (-1 for active layer)",
                               CLI_OPT_INT, false, NULL, -1, 0.0f, false);
    
    result = cli_register_command(ctx, "voxel-paint",
                                 "Paint a voxel at the specified position",
                                 "[OPTIONS]",
                                 cmd_voxel_paint);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "voxel-paint", "p", "pos", "Voxel position (x,y,z)", CLI_OPT_STRING, true);
    cli_add_option(ctx, "voxel-paint", "c", "color", "Voxel color (r,g,b,a)", CLI_OPT_STRING, true);
    cli_add_option_with_default(ctx, "voxel-paint", "l", "layer", "Layer ID (-1 for active layer)",
                               CLI_OPT_INT, false, NULL, -1, 0.0f, false);
    
    return CLI_SUCCESS;
}