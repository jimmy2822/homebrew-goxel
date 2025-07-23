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

#include "cli_interface.h"
#include "cli_commands.h"
#include "../core/goxel_core.h"
#include "render_headless.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // For getopt on POSIX systems

static cli_context_t *g_cli_context = NULL;
static goxel_core_context_t *g_goxel_context = NULL;

static int initialize_goxel_context(void)
{
    g_goxel_context = goxel_core_create_context();
    if (!g_goxel_context) {
        fprintf(stderr, "Error: Failed to create Goxel context\n");
        return -1;
    }
    
    if (goxel_core_init(g_goxel_context) != 0) {
        fprintf(stderr, "Error: Failed to initialize Goxel core\n");
        goxel_core_destroy_context(g_goxel_context);
        g_goxel_context = NULL;
        return -1;
    }
    
    if (headless_render_init(1920, 1080) != 0) {
        fprintf(stderr, "Error: Failed to initialize headless rendering\n");
        goxel_core_shutdown(g_goxel_context);
        goxel_core_destroy_context(g_goxel_context);
        g_goxel_context = NULL;
        return -1;
    }
    
    return 0;
}

static void cleanup_goxel_context(void)
{
    if (g_goxel_context) {
        headless_render_shutdown();
        goxel_core_shutdown(g_goxel_context);
        goxel_core_destroy_context(g_goxel_context);
        g_goxel_context = NULL;
    }
}

static int register_all_commands(cli_context_t *ctx)
{
    cli_result_t result;
    
    result = register_project_commands(ctx);
    if (result != CLI_SUCCESS) {
        fprintf(stderr, "Error registering project commands: %s\n", cli_error_string(result));
        return -1;
    }
    
    result = register_voxel_commands(ctx);
    if (result != CLI_SUCCESS) {
        fprintf(stderr, "Error registering voxel commands: %s\n", cli_error_string(result));
        return -1;
    }
    
    return 0;
}

static void print_startup_info(void)
{
    printf("Goxel Headless CLI - 3D Voxel Editor Command Line Interface\n");
    printf("Version: 13.0.0-alpha\n");
    printf("Headless rendering: %s\n", headless_render_is_initialized() ? "Enabled" : "Disabled");
    
    int width, height, bpp;
    void *buffer = headless_render_get_buffer(&width, &height, &bpp);
    if (buffer) {
        printf("Render buffer: %dx%d (%d bpp)\n", width, height, bpp);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    int exit_code = 0;
    
    g_cli_context = cli_create_context(argv[0]);
    if (!g_cli_context) {
        fprintf(stderr, "Error: Failed to create CLI context\n");
        return 1;
    }
    
    if (initialize_goxel_context() != 0) {
        cli_destroy_context(g_cli_context);
        return 1;
    }
    
    cli_set_goxel_context(g_cli_context, g_goxel_context);
    
    if (register_all_commands(g_cli_context) != 0) {
        cleanup_goxel_context();
        cli_destroy_context(g_cli_context);
        return 1;
    }
    
    bool verbose = false;
    bool quiet = false;
    const char *config_file = NULL;
    
    // Process only global options before command name
    int command_start = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_help(g_cli_context);
            cleanup_goxel_context();
            cli_destroy_context(g_cli_context);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            cli_print_version();
            cleanup_goxel_context();
            cli_destroy_context(g_cli_context);
            return 0;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_file = argv[i + 1];
            i++; // Skip the config file argument
        } else if (argv[i][0] != '-') {
            // This is the command name
            command_start = i;
            break;
        }
    }
    
    // Also handle global options that come after the command
    if (command_start != -1) {
        for (int i = command_start + 1; i < argc; i++) {
            if (strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
            } else if (strcmp(argv[i], "--quiet") == 0) {
                quiet = true;
            }
        }
    }
    
    cli_set_global_options(g_cli_context, verbose, quiet, config_file);
    
    printf("DEBUG: Set global options, argc=%d\n", argc);
    fflush(stdout);
    
    if (!quiet && argc == 1) {
        print_startup_info();
        cli_print_help(g_cli_context);
        cleanup_goxel_context();
        cli_destroy_context(g_cli_context);
        return 0;
    }
    
    printf("DEBUG: About to call cli_run\n");
    fflush(stdout);
    
    cli_result_t result = cli_run(g_cli_context, argc, argv);
    
    printf("DEBUG: cli_run returned %d\n", result);
    fflush(stdout);
    if (result != CLI_SUCCESS) {
        if (!quiet) {
            fprintf(stderr, "Command failed: %s\n", cli_error_string(result));
        }
        exit_code = (int)result;
    }
    
    cleanup_goxel_context();
    cli_destroy_context(g_cli_context);
    
    return exit_code;
}