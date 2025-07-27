/* Goxel 3D voxels editor - Unified Entry Point
 *
 * copyright (c) 2015-2025 Guillaume Chereau <guillaume@noctua-software.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>

// Forward declarations for mode-specific main functions
int gui_main(int argc, char **argv);
int headless_main(int argc, char **argv);
int daemon_main(int argc, char **argv);  // From src/daemon/daemon_main.c

// Configuration for unified execution
typedef struct {
    bool headless_mode;
    bool daemon_mode;
    bool gui_mode;
} unified_config_t;

/**
 * Detect execution mode from command line and environment
 * Priority order:
 * 1. --daemon flag -> daemon server mode
 * 2. --headless flag -> CLI client mode
 * 3. 'goxel-headless' symlink -> CLI client mode
 * 4. Default -> GUI mode
 */
static void detect_execution_mode(int argc, char **argv, unified_config_t *config)
{
    // Initialize all modes to false
    config->headless_mode = false;
    config->daemon_mode = false;
    config->gui_mode = false;
    
    // Check for daemon mode first (highest priority)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--daemon") == 0 || 
            strcmp(argv[i], "--daemonize") == 0 ||
            strcmp(argv[i], "-D") == 0) {
            config->daemon_mode = true;
            return;
        }
    }
    
    // Check for headless/CLI mode
    bool is_headless = false;
    
    // Check command line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0 || 
            strcmp(argv[i], "-H") == 0) {
            is_headless = true;
            break;
        }
    }
    
    // Check if invoked as goxel-headless (symlink compatibility)
    if (!is_headless) {
        const char *progname = basename(argv[0]);
        if (strstr(progname, "headless") != NULL) {
            is_headless = true;
        }
    }
    
    // Check environment variable
    if (!is_headless) {
        const char *env_headless = getenv("GOXEL_HEADLESS");
        if (env_headless && strcmp(env_headless, "1") == 0) {
            is_headless = true;
        }
    }
    
    if (is_headless) {
        config->headless_mode = true;
    } else {
        config->gui_mode = true;
    }
}

/**
 * Filter out mode selection arguments before passing to mode-specific main
 * This prevents the mode-specific code from seeing mode selection flags
 * Note: --daemon/--daemonize are kept for daemon_main to process
 */
static void filter_mode_args(int *argc, char **argv, bool keep_daemon_flags)
{
    int new_argc = 1; // Keep program name
    
    for (int i = 1; i < *argc; i++) {
        // Always filter out --headless/-H
        if (strcmp(argv[i], "--headless") == 0 || 
            strcmp(argv[i], "-H") == 0) {
            continue;
        }
        
        // Filter daemon flags only if not keeping them
        if (!keep_daemon_flags && 
            (strcmp(argv[i], "--daemon") == 0 || 
             strcmp(argv[i], "--daemonize") == 0 ||
             strcmp(argv[i], "-D") == 0)) {
            continue;
        }
        
        argv[new_argc++] = argv[i];
    }
    
    *argc = new_argc;
}

/**
 * Unified main entry point
 * Detects execution mode and delegates to appropriate subsystem
 */
int main(int argc, char **argv)
{
    unified_config_t config = {0};
    
    // Detect which mode to run in
    detect_execution_mode(argc, argv, &config);
    
    if (config.daemon_mode) {
        // Run as daemon server - keep daemon flags for daemon_main to process
        filter_mode_args(&argc, argv, true);
        return daemon_main(argc, argv);
    } else if (config.headless_mode) {
        // Run in headless/CLI client mode
        filter_mode_args(&argc, argv, false);
        return headless_main(argc, argv);
    } else {
        // Run in GUI mode
        filter_mode_args(&argc, argv, false);
        return gui_main(argc, argv);
    }
}

// Temporary stubs - these will be replaced by the actual implementations
// when we refactor the existing main.c and main_cli.c files

#ifdef UNIFIED_BUILD_STUB
int gui_main(int argc, char **argv)
{
    printf("GUI mode not yet integrated\n");
    return 1;
}

int headless_main(int argc, char **argv)
{
    printf("Headless mode not yet integrated\n");
    return 1;
}
#endif