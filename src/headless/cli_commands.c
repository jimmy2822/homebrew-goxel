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
    
    printf("DEBUG: cmd_open - got input_file: %s\n", input_file);
    fflush(stdout);
    
    if (!ctx->quiet) {
        printf("Opening project: %s", input_file);
        if (read_only) printf(" (read-only mode)");
        printf("\n");
    }
    
    printf("DEBUG: cmd_open - getting goxel context\n");
    fflush(stdout);
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    printf("DEBUG: cmd_open - calling goxel_core_load_project\n");
    fflush(stdout);
    
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
    printf("DEBUG: ENTERED cmd_voxel_add function\n");
    fflush(stdout);
    
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    printf("DEBUG: Context validation passed in cmd_voxel_add\n");
    fflush(stdout);
    
    printf("DEBUG: Parsing arguments...\n");
    fflush(stdout);
    
    const char *pos_str = cli_get_option_string(args, "pos", NULL);
    const char *color_str = cli_get_option_string(args, "color", "255,255,255,255");
    int layer_id = cli_get_option_int(args, "layer", -1);
    
    printf("DEBUG: Arguments parsed successfully\n");
    fflush(stdout);
    
    // Get project file from positional arguments
    const char *project_file = NULL;
    if (cli_get_positional_count(args) > 0) {
        project_file = cli_get_positional_arg(args, 0);
    }
    
    printf("DEBUG: Project file: %s\n", project_file ? project_file : "NULL");
    fflush(stdout);
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
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
        printf(" to project: %s\n", project_file);
    }
    
    printf("DEBUG: Getting Goxel context...\n");
    fflush(stdout);
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    printf("DEBUG: Goxel context retrieved successfully\n");
    fflush(stdout);
    
    // Load project file or create new project if it doesn't exist
    printf("DEBUG: Checking if project file exists...\n");
    fflush(stdout);
    
    FILE *test_file = fopen(project_file, "r");
    
    printf("DEBUG: File check completed\n");
    fflush(stdout);
    if (test_file) {
        fclose(test_file);
        printf("DEBUG: File exists, loading project...\n");
        fflush(stdout);
        
        // File exists, try to load it
        if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
            fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
            return CLI_ERROR_PROJECT_LOAD_FAILED;
        }
        
        printf("DEBUG: Project loaded successfully\n");
        fflush(stdout);
    } else {
        printf("DEBUG: File doesn't exist, creating new project...\n");
        fflush(stdout);
        
        // File doesn't exist, create a new project
        if (!ctx->quiet) {
            printf("Creating new project: %s\n", project_file);
        }
        if (goxel_core_create_project(goxel_ctx, project_file, 64, 64, 64) != 0) {
            fprintf(stderr, "Error: Failed to create new project\n");
            return CLI_ERROR_PROJECT_LOAD_FAILED;
        }
        
        printf("DEBUG: New project created successfully\n");
        fflush(stdout);
    }
    
    uint8_t color[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    printf("DEBUG: About to call goxel_core_add_voxel()...\n");
    fflush(stdout);
    if (goxel_core_add_voxel(goxel_ctx, x, y, z, color, layer_id) != 0) {
        fprintf(stderr, "Error: Failed to add voxel\n");
        return CLI_ERROR_VOXEL_OPERATION_FAILED;
    }
    
    // Save project back
    if (goxel_core_save_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", project_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
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
    
    // Get project file from positional arguments
    const char *project_file = NULL;
    if (cli_get_positional_count(args) > 0) {
        project_file = cli_get_positional_arg(args, 0);
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
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
    
    // Save project back
    if (goxel_core_save_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", project_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
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
    
    // Get project file from positional arguments
    const char *project_file = NULL;
    if (cli_get_positional_count(args) > 0) {
        project_file = cli_get_positional_arg(args, 0);
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
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
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    uint8_t color[4] = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
    
    if (goxel_core_paint_voxel(goxel_ctx, x, y, z, color, layer_id) != 0) {
        fprintf(stderr, "Error: Failed to paint voxel\n");
        return CLI_ERROR_VOXEL_OPERATION_FAILED;
    }
    
    // Save project back
    if (goxel_core_save_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", project_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
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
    cli_add_option_with_default(ctx, "create", "y", "height", "Project height in voxels",
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
                                 "[OPTIONS] <project-file>",
                                 cmd_voxel_remove);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "voxel-remove", "p", "pos", "Voxel position (x,y,z)", CLI_OPT_STRING, false);
    cli_add_option(ctx, "voxel-remove", "b", "box", "Box area (x1,y1,z1,x2,y2,z2)", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "voxel-remove", "l", "layer", "Layer ID (-1 for active layer)",
                               CLI_OPT_INT, false, NULL, -1, 0.0f, false);
    
    result = cli_register_command(ctx, "voxel-paint",
                                 "Paint a voxel at the specified position",
                                 "[OPTIONS] <project-file>",
                                 cmd_voxel_paint);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "voxel-paint", "p", "pos", "Voxel position (x,y,z)", CLI_OPT_STRING, true);
    cli_add_option(ctx, "voxel-paint", "c", "color", "Voxel color (r,g,b,a)", CLI_OPT_STRING, true);
    cli_add_option_with_default(ctx, "voxel-paint", "l", "layer", "Layer ID (-1 for active layer)",
                               CLI_OPT_INT, false, NULL, -1, 0.0f, false);
    
    // Layer commands
    result = cli_register_command(ctx, "layer-create",
                                 "Create a new layer",
                                 "[OPTIONS] <project-file>",
                                 cmd_layer_create);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option_with_default(ctx, "layer-create", "n", "name", "Layer name",
                               CLI_OPT_STRING, false, "New Layer", 0, 0.0f, false);
    cli_add_option(ctx, "layer-create", "c", "color", "Layer color (r,g,b,a)", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "layer-create", "v", "visible", "Layer visibility (1=visible, 0=hidden)",
                               CLI_OPT_INT, false, NULL, 1, 0.0f, false);

    result = cli_register_command(ctx, "layer-delete",
                                 "Delete a layer",
                                 "[OPTIONS]",
                                 cmd_layer_delete);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "layer-delete", "i", "id", "Layer ID", CLI_OPT_INT, false);
    cli_add_option(ctx, "layer-delete", "n", "name", "Layer name", CLI_OPT_STRING, false);

    result = cli_register_command(ctx, "layer-merge",
                                 "Merge two layers",
                                 "[OPTIONS]",
                                 cmd_layer_merge);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "layer-merge", "s", "source", "Source layer ID", CLI_OPT_INT, false);
    cli_add_option(ctx, "layer-merge", "t", "target", "Target layer ID", CLI_OPT_INT, false);
    cli_add_option(ctx, "layer-merge", NULL, "source-name", "Source layer name", CLI_OPT_STRING, false);
    cli_add_option(ctx, "layer-merge", NULL, "target-name", "Target layer name", CLI_OPT_STRING, false);

    result = cli_register_command(ctx, "layer-visibility",
                                 "Set layer visibility",
                                 "[OPTIONS] <project-file>",
                                 cmd_layer_visibility);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "layer-visibility", "i", "id", "Layer ID", CLI_OPT_INT, false);
    cli_add_option(ctx, "layer-visibility", "n", "name", "Layer name", CLI_OPT_STRING, false);
    cli_add_option(ctx, "layer-visibility", "v", "visible", "Visibility (1=visible, 0=hidden)", CLI_OPT_INT, true);

    result = cli_register_command(ctx, "layer-rename",
                                 "Rename a layer",
                                 "[OPTIONS]",
                                 cmd_layer_rename);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "layer-rename", "i", "id", "Layer ID", CLI_OPT_INT, false);
    cli_add_option(ctx, "layer-rename", "n", "name", "Current layer name", CLI_OPT_STRING, false);
    cli_add_option(ctx, "layer-rename", NULL, "new-name", "New layer name", CLI_OPT_STRING, true);
    
    // Rendering commands
    result = cli_register_command(ctx, "render",
                                 "Render the scene to an image file",
                                 "[OPTIONS] <project-file> <output-file>",
                                 cmd_render);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "render", "o", "output", "Output image file", CLI_OPT_STRING, false);
    cli_add_option_with_default(ctx, "render", "w", "width", "Image width",
                               CLI_OPT_INT, false, NULL, 800, 0.0f, false);
    cli_add_option_with_default(ctx, "render", "h", "height", "Image height",
                               CLI_OPT_INT, false, NULL, 600, 0.0f, false);
    cli_add_option_with_default(ctx, "render", "f", "format", "Image format (png, jpg)",
                               CLI_OPT_STRING, false, "png", 0, 0.0f, false);
    cli_add_option_with_default(ctx, "render", "q", "quality", "Image quality (1-100)",
                               CLI_OPT_INT, false, NULL, 90, 0.0f, false);
    cli_add_option_with_default(ctx, "render", "c", "camera", "Camera preset",
                               CLI_OPT_STRING, false, "default", 0, 0.0f, false);

    // Export commands
    result = cli_register_command(ctx, "export",
                                 "Export project to various formats",
                                 "[OPTIONS] <project-file> <output-file>",
                                 cmd_export);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "export", "o", "output", "Output file", CLI_OPT_STRING, false);
    cli_add_option(ctx, "export", "f", "format", "Export format (auto-detect from extension if not specified)", CLI_OPT_STRING, false);

    result = cli_register_command(ctx, "convert",
                                 "Convert between different voxel formats",
                                 "[OPTIONS] INPUT_FILE OUTPUT_FILE",
                                 cmd_convert);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "convert", "i", "input", "Input file", CLI_OPT_STRING, false);
    cli_add_option(ctx, "convert", "o", "output", "Output file", CLI_OPT_STRING, false);
    cli_add_option(ctx, "convert", "f", "format", "Output format", CLI_OPT_STRING, false);

    // Scripting commands
    result = cli_register_command(ctx, "script",
                                 "Execute JavaScript scripts",
                                 "[OPTIONS] [SCRIPT_FILE]",
                                 cmd_script);
    if (result != CLI_SUCCESS) return result;
    
    cli_add_option(ctx, "script", "f", "file", "Script file to execute", CLI_OPT_STRING, false);
    cli_add_option(ctx, "script", "c", "code", "Inline script code to execute", CLI_OPT_STRING, false);
    
    return CLI_SUCCESS;
}

cli_result_t cmd_layer_create(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    // Get project file from positional arguments
    const char *project_file = NULL;
    if (cli_get_positional_count(args) > 0) {
        project_file = cli_get_positional_arg(args, 0);
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    const char *name = cli_get_option_string(args, "name", "New Layer");
    const char *color_str = cli_get_option_string(args, "color", NULL);
    int visible = cli_get_option_int(args, "visible", 1);
    
    if (!ctx->quiet) {
        printf("Creating layer '%s'", name);
        if (color_str) printf(" with color %s", color_str);
        printf(" (visibility: %s) in project: %s\n", visible ? "visible" : "hidden", project_file);
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    // Parse color if provided
    uint8_t color[4] = {255, 255, 255, 255};
    if (color_str) {
        int r, g, b, a = 255;
        if (sscanf(color_str, "%d,%d,%d,%d", &r, &g, &b, &a) >= 3) {
            color[0] = (uint8_t)r;
            color[1] = (uint8_t)g;
            color[2] = (uint8_t)b;
            color[3] = (uint8_t)a;
        }
    }
    
    int layer_id = goxel_core_create_layer(goxel_ctx, name, color, visible);
    if (layer_id < 0) {
        fprintf(stderr, "Error: Failed to create layer\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Save project back
    if (goxel_core_save_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", project_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Layer created successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_layer_delete(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    int layer_id = cli_get_option_int(args, "id", -1);
    const char *name = cli_get_option_string(args, "name", NULL);
    
    if (layer_id == -1 && !name) {
        fprintf(stderr, "Error: Either layer ID (--id) or layer name (--name) must be specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        if (layer_id >= 0) {
            printf("Deleting layer with ID %d\n", layer_id);
        } else {
            printf("Deleting layer named '%s'\n", name);
        }
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_delete_layer(goxel_ctx, layer_id, name) != 0) {
        fprintf(stderr, "Error: Failed to delete layer\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Layer deleted successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_layer_merge(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    int source_id = cli_get_option_int(args, "source", -1);
    int target_id = cli_get_option_int(args, "target", -1);
    const char *source_name = cli_get_option_string(args, "source-name", NULL);
    const char *target_name = cli_get_option_string(args, "target-name", NULL);
    
    if (source_id == -1 && !source_name) {
        fprintf(stderr, "Error: Source layer must be specified (--source or --source-name)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (target_id == -1 && !target_name) {
        fprintf(stderr, "Error: Target layer must be specified (--target or --target-name)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        printf("Merging layers: ");
        if (source_id >= 0) printf("ID %d", source_id);
        else printf("'%s'", source_name);
        printf(" -> ");
        if (target_id >= 0) printf("ID %d", target_id);
        else printf("'%s'", target_name);
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_merge_layers(goxel_ctx, source_id, target_id, source_name, target_name) != 0) {
        fprintf(stderr, "Error: Failed to merge layers\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Layers merged successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_layer_visibility(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    // Get project file from positional arguments
    const char *project_file = NULL;
    if (cli_get_positional_count(args) > 0) {
        project_file = cli_get_positional_arg(args, 0);
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    int layer_id = cli_get_option_int(args, "id", -1);
    const char *name = cli_get_option_string(args, "name", NULL);
    int visible = cli_get_option_int(args, "visible", -1);
    
    if (layer_id == -1 && !name) {
        fprintf(stderr, "Error: Either layer ID (--id) or layer name (--name) must be specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (visible == -1) {
        fprintf(stderr, "Error: Visibility must be specified (--visible 1 or --visible 0)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        printf("Setting layer ");
        if (layer_id >= 0) printf("ID %d", layer_id);
        else printf("'%s'", name);
        printf(" visibility to %s in project: %s\n", visible ? "visible" : "hidden", project_file);
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    if (goxel_core_set_layer_visibility(goxel_ctx, layer_id, name, visible) != 0) {
        fprintf(stderr, "Error: Failed to set layer visibility\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Save project back
    if (goxel_core_save_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to save project to '%s'\n", project_file);
        return CLI_ERROR_PROJECT_SAVE_FAILED;
    }
    
    if (!ctx->quiet) {
        printf("Layer visibility updated successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_layer_rename(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    int layer_id = cli_get_option_int(args, "id", -1);
    const char *old_name = cli_get_option_string(args, "name", NULL);
    const char *new_name = cli_get_option_string(args, "new-name", NULL);
    
    if (layer_id == -1 && !old_name) {
        fprintf(stderr, "Error: Either layer ID (--id) or current layer name (--name) must be specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!new_name) {
        fprintf(stderr, "Error: New layer name must be specified (--new-name)\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        printf("Renaming layer ");
        if (layer_id >= 0) printf("ID %d", layer_id);
        else printf("'%s'", old_name);
        printf(" to '%s'\n", new_name);
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (goxel_core_rename_layer(goxel_ctx, layer_id, old_name, new_name) != 0) {
        fprintf(stderr, "Error: Failed to rename layer\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Layer renamed successfully\n");
    }
    
    return CLI_SUCCESS;
}

// Rendering commands
cli_result_t cmd_render(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    // Get project file and output file from positional arguments
    const char *project_file = NULL;
    const char *output_file = cli_get_option_string(args, "output", NULL);
    
    if (cli_get_positional_count(args) >= 2) {
        project_file = cli_get_positional_arg(args, 0);
        if (!output_file) {
            output_file = cli_get_positional_arg(args, 1);
        }
    } else if (cli_get_positional_count(args) == 1) {
        if (!output_file) {
            // Single argument could be either project or output - check file extension
            const char *arg = cli_get_positional_arg(args, 0);
            if (strstr(arg, ".gox") || strstr(arg, ".vox") || strstr(arg, ".qb")) {
                project_file = arg;
            } else {
                output_file = arg;
            }
        } else {
            project_file = cli_get_positional_arg(args, 0);
        }
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!output_file) {
        fprintf(stderr, "Error: Output file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    const char *camera_preset = cli_get_option_string(args, "camera", "default");
    int width = cli_get_option_int(args, "width", 800);
    int height = cli_get_option_int(args, "height", 600);
    const char *format = cli_get_option_string(args, "format", "png");
    int quality = cli_get_option_int(args, "quality", 90);
    
    if (!ctx->quiet) {
        printf("Rendering project %s to %s (%dx%d, format: %s, quality: %d, camera: %s)\n",
               project_file, output_file, width, height, format, quality, camera_preset);
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    if (goxel_core_render_to_file(goxel_ctx, output_file, width, height, format, quality, camera_preset) != 0) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Scene rendered successfully\n");
    }
    
    return CLI_SUCCESS;
}

// Export/import commands
cli_result_t cmd_export(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    // Get project file and output file from positional arguments  
    const char *project_file = NULL;
    const char *output_file = cli_get_option_string(args, "output", NULL);
    const char *format = cli_get_option_string(args, "format", NULL);
    
    if (cli_get_positional_count(args) >= 2) {
        project_file = cli_get_positional_arg(args, 0);
        if (!output_file) {
            output_file = cli_get_positional_arg(args, 1);
        }
    } else if (cli_get_positional_count(args) == 1) {
        if (!output_file) {
            output_file = cli_get_positional_arg(args, 0);
        } else {
            project_file = cli_get_positional_arg(args, 0);
        }
    }
    
    if (!project_file) {
        fprintf(stderr, "Error: Project file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!output_file) {
        fprintf(stderr, "Error: Output file not specified\n");
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (!ctx->quiet) {
        printf("Exporting project %s to %s", project_file, output_file);
        if (format) printf(" (format: %s)", format);
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load project file
    if (goxel_core_load_project(goxel_ctx, project_file) != 0) {
        fprintf(stderr, "Error: Failed to load project from '%s'\n", project_file);
        return CLI_ERROR_PROJECT_LOAD_FAILED;
    }
    
    if (goxel_core_export_project(goxel_ctx, output_file, format) != 0) {
        fprintf(stderr, "Error: Failed to export project\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Project exported successfully\n");
    }
    
    return CLI_SUCCESS;
}

cli_result_t cmd_convert(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *input_file = cli_get_option_string(args, "input", NULL);
    const char *output_file = cli_get_option_string(args, "output", NULL);
    const char *format = cli_get_option_string(args, "format", NULL);
    
    if (!input_file) {
        if (cli_get_positional_count(args) > 0) {
            input_file = cli_get_positional_arg(args, 0);
        } else {
            fprintf(stderr, "Error: Input file not specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!output_file) {
        if (cli_get_positional_count(args) > 1) {
            output_file = cli_get_positional_arg(args, 1);
        } else {
            fprintf(stderr, "Error: Output file not specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        printf("Converting %s to %s", input_file, output_file);
        if (format) printf(" (format: %s)", format);
        printf("\n");
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    // Load input file
    if (goxel_core_load_project(goxel_ctx, input_file) != 0) {
        fprintf(stderr, "Error: Failed to load input file %s\n", input_file);
        return CLI_ERROR_GENERIC;
    }
    
    // Export to output file
    if (goxel_core_export_project(goxel_ctx, output_file, format) != 0) {
        fprintf(stderr, "Error: Failed to export to %s\n", output_file);
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Conversion completed successfully\n");
    }
    
    return CLI_SUCCESS;
}

// Scripting commands
cli_result_t cmd_script(cli_context_t *ctx, cli_args_t *args)
{
    if (!ctx || !args) return CLI_ERROR_INVALID_ARGS;
    
    const char *script_file = cli_get_option_string(args, "file", NULL);
    const char *script_code = cli_get_option_string(args, "code", NULL);
    
    if (!script_file && !script_code) {
        if (cli_get_positional_count(args) > 0) {
            script_file = cli_get_positional_arg(args, 0);
        } else {
            fprintf(stderr, "Error: Either script file (--file) or inline code (--code) must be specified\n");
            return CLI_ERROR_INVALID_ARGS;
        }
    }
    
    if (!ctx->quiet) {
        if (script_file) {
            printf("Executing script file: %s\n", script_file);
        } else {
            printf("Executing inline script code\n");
        }
    }
    
    goxel_core_context_t *goxel_ctx = (goxel_core_context_t*)ctx->goxel_context;
    if (!goxel_ctx) {
        fprintf(stderr, "Error: Goxel context not initialized\n");
        return CLI_ERROR_GENERIC;
    }
    
    int result;
    if (script_file) {
        result = goxel_core_execute_script_file(goxel_ctx, script_file);
    } else {
        result = goxel_core_execute_script(goxel_ctx, script_code);
    }
    
    if (result != 0) {
        fprintf(stderr, "Error: Script execution failed\n");
        return CLI_ERROR_GENERIC;
    }
    
    if (!ctx->quiet) {
        printf("Script executed successfully\n");
    }
    
    return CLI_SUCCESS;
}