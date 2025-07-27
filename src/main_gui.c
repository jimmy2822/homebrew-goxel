/* Goxel 3D voxels editor - GUI Main Entry Point
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

/**
 * GUI-specific main entry point for unified build
 * This is called when Goxel runs in GUI mode
 */

#include <stdio.h>

// Forward declaration of the original GUI main
int original_gui_main(int argc, char **argv);

/**
 * GUI main entry point for unified build
 * Wraps the original main() from src/main.c
 */
int gui_main(int argc, char **argv)
{
    // TODO: In final integration, rename the original main() in src/main.c
    // to original_gui_main() and call it here.
    // For now, we'll use a placeholder.
    
#ifdef GOXEL_UNIFIED_BUILD
    // When fully integrated, this will call the original GUI main
    return original_gui_main(argc, argv);
#else
    // Placeholder for testing
    printf("GUI mode not yet fully integrated in unified build\n");
    printf("Run with --headless for CLI mode or --daemon for daemon mode\n");
    return 1;
#endif
}