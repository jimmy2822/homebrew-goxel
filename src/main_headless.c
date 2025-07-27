/* Goxel 3D voxels editor - Headless Main Entry Point
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
 * Headless/CLI main entry point for unified build
 * This is called when Goxel runs in headless client mode
 */

#include <stdio.h>

// Forward declaration of the original headless main
int original_headless_main(int argc, char **argv);

/**
 * Headless main entry point for unified build
 * Wraps the original main() from src/headless/main_cli.c
 */
int headless_main(int argc, char **argv)
{
    // TODO: In final integration, rename the original main() in src/headless/main_cli.c
    // to original_headless_main() and call it here.
    // For now, we'll use a placeholder that shows it's working.
    
#ifdef GOXEL_UNIFIED_BUILD
    // When fully integrated, this will call the original headless main
    return original_headless_main(argc, argv);
#else
    // Placeholder for testing
    printf("Headless CLI mode selected\n");
    printf("This will connect to the daemon or run standalone CLI operations\n");
    printf("Use --daemon to run as daemon server instead\n");
    return 0;
#endif
}