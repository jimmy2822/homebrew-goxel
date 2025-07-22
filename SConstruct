# Goxel 3D voxels editor
#
# copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
#
# Goxel is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# goxel.  If not, see <http://www.gnu.org/licenses/>.

import glob
import os
import sys

vars = Variables('settings.py')
vars.AddVariables(
    EnumVariable('mode', 'Build mode', 'debug',
        allowed_values=('debug', 'release', 'profile', 'analyze')),
    BoolVariable('werror', 'Warnings as error', True),
    BoolVariable('sound', 'Enable sound', False),
    BoolVariable('yocto', 'Enable yocto renderer', True),
    BoolVariable('headless', 'Build headless version (no GUI)', False),
    BoolVariable('gui', 'Build with GUI support', True),
    BoolVariable('cli_tools', 'Build CLI tools', False),
    BoolVariable('c_api', 'Build C API shared library', False),
    PathVariable('config_file', 'Config file to use', 'src/config.h'),
)

target_os = str(Platform())

if target_os == 'posix':
    vars.AddVariables(
        EnumVariable('nfd_backend', 'Native file dialog backend', default='gtk',
                     allowed_values=('gtk', 'portal')),
    )


env = Environment(variables=vars, ENV=os.environ)
conf = env.Configure()

if env['mode'] == 'analyze':
    # Make sure clang static analyzer has a chance to override de compiler
    # and set CCC settings
    env["CC"] = os.getenv("CC") or env["CC"]
    env["CXX"] = os.getenv("CXX") or env["CXX"]
    env["ENV"].update(x for x in os.environ.items() if x[0].startswith("CCC_"))


if os.environ.get('CC') == 'clang':
    env.Replace(CC='clang', CXX='clang++')

# Asan & Ubsan (need to come first).
if env['mode'] == 'debug' and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LINKFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])


# Global compilation flags.
# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++
env.Append(
    CFLAGS=['-std=gnu99', '-Wall',
            '-Wno-unknow-pragma', '-Wno-unknown-warning-option'],
    CXXFLAGS=['-std=gnu++17', '-Wall', '-Wno-narrowing']
)

if env['werror']:
    env.Append(CCFLAGS='-Werror')

if env['mode'] not in ['debug', 'analyze']:
    env.Append(CPPDEFINES='NDEBUG', CCFLAGS='-O3')

if env['mode'] == 'debug':
    env.Append(CCFLAGS=['-O0'])

if env['mode'] in ('profile', 'debug'):
    env.Append(CCFLAGS='-g')

env.Append(CPPPATH=['src', '.'])
env.Append(CCFLAGS=['-include', '$config_file'])

# Add build flags for different modes
if env['headless']:
    env.Append(CPPDEFINES=['GOXEL_HEADLESS=1'])
if env['gui']:
    env.Append(CPPDEFINES=['GOXEL_GUI=1'])
if env['cli_tools']:
    env.Append(CPPDEFINES=['GOXEL_CLI_TOOLS=1'])
if env['c_api']:
    env.Append(CPPDEFINES=['GOXEL_C_API=1'])

# Get all the c and c++ files in src, recursively.
sources = []
gui_sources = []
core_sources = []

# Always include core functionality
for root, dirnames, filenames in os.walk('src/core'):
    for filename in filenames:
        if filename.endswith('.c') or filename.endswith('.cpp'):
            core_sources.append(os.path.join(root, filename))

# Include GUI sources only if not headless
if not env['headless']:
    for root, dirnames, filenames in os.walk('src/gui'):
        for filename in filenames:
            if filename.endswith('.c') or filename.endswith('.cpp'):
                gui_sources.append(os.path.join(root, filename))

# Include headless sources if building headless
headless_sources = []
if env['headless'] or env['cli_tools']:
    for root, dirnames, filenames in os.walk('src/headless'):
        for filename in filenames:
            if filename.endswith('.c') or filename.endswith('.cpp'):
                headless_sources.append(os.path.join(root, filename))

# Include remaining src files (excluding core/, gui/, headless/ subdirs)
other_sources = []

# Files with GUI dependencies that should be excluded for CLI builds
gui_dependent_files = [
    'gui.cpp', 'imgui.cpp', 'main.c',  # Original exclusions
    'goxel.c',  # Contains GUI-specific code
    'tools.c',  # Contains GUI tool interfaces
    'filters.c',  # Contains GUI filter interfaces
    # Additional files with duplicates in core/
    'image.c',  # Duplicate of core/image.c
    'layer.c',  # Duplicate of core/layer.c
    'material.c',  # Duplicate of core/material.c
    'palette.c',  # Duplicate of core/palette.c
    'shape.c',  # Duplicate of core/shape.c
    'file_format.c',  # Duplicate of core/file_format.c
    'volume.c',  # Duplicate of core/volume_ops.c
    'volume_utils.c',  # Duplicate of core/volume_utils.c
    # Utils duplicates
    'utils/b64.c', 'utils/box.c', 'utils/cache.c', 'utils/color.c',
    'utils/geometry.c', 'utils/gl.c', 'utils/img.c', 'utils/ini.c',
    'utils/json.c', 'utils/mo_reader.c', 'utils/mustache.c', 'utils/path.c',
    'utils/sound.c', 'utils/texture.c', 'utils/vec.c',
]

# Directories with GUI dependencies for CLI builds
gui_dependent_dirs = ['tools', 'filters', 'formats', 'utils']

for root, dirnames, filenames in os.walk('src'):
    # Skip subdirectories we handle separately
    if 'core' in root or 'gui' in root or 'headless' in root:
        continue
    
    for filename in filenames:
        if filename.endswith('.c') or filename.endswith('.cpp'):
            # For CLI/headless builds, exclude GUI-dependent files and directories
            if env['headless'] or env['cli_tools']:
                if filename in gui_dependent_files:
                    continue
                # Check if file is in GUI-dependent directory
                skip_file = False
                for gui_dir in gui_dependent_dirs:
                    if gui_dir in root:
                        skip_file = True
                        break
                if skip_file:
                    continue
            other_sources.append(os.path.join(root, filename))

sources = core_sources + other_sources
# Include GUI sources only if NOT building headless OR CLI tools
if not env['headless'] and not env['cli_tools']:
    sources += gui_sources
if env['headless'] or env['cli_tools']:
    sources += headless_sources

# Use CLI main for CLI tools build
if env['cli_tools'] and not env['headless']:
    # For CLI tools, use the CLI main entry point
    cli_main = 'src/headless/main_cli.c'
    if cli_main in sources:
        sources.remove(cli_main)
    sources.append(cli_main)

# Check for libpng.
if conf.CheckLibWithHeader('libpng', 'png.h', 'c'):
    env.Append(CPPDEFINES='HAVE_LIBPNG=1')

# Linux compilation support.
if target_os == 'posix':
    if env['headless']:
        # Headless mode uses OSMesa for offscreen rendering
        env.Append(LIBS=['OSMesa', 'm', 'dl', 'pthread'])
        env.Append(CPPDEFINES=['OSMESA_RENDERING=1'])
    else:
        # GUI mode uses regular OpenGL
        env.Append(LIBS=['GL', 'm', 'dl', 'pthread'])
        # Note: add '--static' to link with all the libs needed by glfw3.
        env.ParseConfig('pkg-config --libs glfw3')

    # File dialogs only needed for GUI mode
    if not env['headless']:
        # Pick the proper native file dialog backend.
        if env['nfd_backend'] == 'portal':
            env.ParseConfig('pkg-config --cflags --libs dbus-1')
            sources.append('ext_src/nfd/nfd_portal.cpp')

        if env['nfd_backend'] == 'gtk':
            env.ParseConfig('pkg-config --cflags --libs gtk+-3.0')
            sources.append('ext_src/nfd/nfd_gtk.cpp')


# Windows compilation support.
if target_os == 'msys':
    env.Append(CXXFLAGS=['-Wno-attributes', '-Wno-unused-variable',
                         '-Wno-unused-function'])
    env.Append(CCFLAGS=['-Wno-error=address']) # To remove if possible.
    env.Append(LIBS=['glfw3', 'opengl32', 'z', 'tre', 'gdi32', 'Comdlg32',
                     'ole32', 'uuid', 'shell32'],
               LINKFLAGS='--static')
    sources += glob.glob('ext_src/glew/glew.c')
    sources.append('ext_src/nfd/nfd_win.cpp')
    env.Append(CPPPATH=['ext_src/glew'])
    env.Append(CPPDEFINES=['GLEW_STATIC', 'FREE_WINDOWS'])

# OSX Compilation support.
if target_os == 'darwin':
    sources += glob.glob('src/*.m')
    sources.append('ext_src/nfd/nfd_cocoa.m')
    env.Append(FRAMEWORKS=[
        'OpenGL', 'Cocoa', 'AppKit', 'UniformTypeIdentifiers'])
    env.Append(LIBS=['m', 'objc'])
    # Fix warning in noc_file_dialog (the code should be fixed instead).
    env.Append(CCFLAGS=['-Wno-deprecated-declarations'])
    env.ParseConfig('pkg-config --cflags --libs glfw3')
    env['sound'] = False


# Add external libs.
env.Append(CPPPATH=['ext_src'])
env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])
env.Append(CPPPATH=['ext_src/nfd'])
env.Append(CPPPATH=['ext_src/noc'])
env.Append(CPPPATH=['ext_src/xxhash'])
env.Append(CPPPATH=['ext_src/meshoptimizer'])

if env['sound']:
    env.Append(LIBS='openal')
    env.Append(CPPDEFINES='SOUND=1')

if not env['yocto']:
    env.Append(CPPDEFINES='YOCTO=0')

# Append external environment flags
env.Append(
    CFLAGS=os.environ.get("CFLAGS", "").split(),
    CXXFLAGS=os.environ.get("CXXFLAGS", "").split(),
    LINKFLAGS=os.environ.get("LDFLAGS", "").split()
)

# Build compile_commands.json.
try:
    env.Tool('compilation_db')
    env.CompilationDatabase()
except:
    pass

# Build different targets based on mode
if env['c_api']:
    print("Building C API shared library (simplified version for Phase 4)")
    
    # Create a minimal stub API implementation that compiles
    # This is a placeholder until full dependencies are resolved
    minimal_api_source = '''
#include "../../include/goxel_headless.h"
#include <stdlib.h>
#include <string.h>

struct goxel_context { int placeholder; };

goxel_context_t *goxel_create_context(void) {
    return calloc(1, sizeof(struct goxel_context));
}

goxel_error_t goxel_init_context(goxel_context_t *ctx) {
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

void goxel_destroy_context(goxel_context_t *ctx) {
    if (ctx) free(ctx);
}

goxel_error_t goxel_create_project(goxel_context_t *ctx, const char *name, 
                                   int width, int height, int depth) {
    (void)name; (void)width; (void)height; (void)depth;
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

goxel_error_t goxel_add_voxel(goxel_context_t *ctx, int x, int y, int z, 
                              const goxel_color_t *color) {
    (void)x; (void)y; (void)z; (void)color;
    return ctx ? GOXEL_SUCCESS : GOXEL_ERROR_INVALID_CONTEXT;
}

const char *goxel_get_error_string(goxel_error_t error) {
    switch(error) {
        case GOXEL_SUCCESS: return "Success";
        case GOXEL_ERROR_INVALID_CONTEXT: return "Invalid context";
        default: return "Unknown error";
    }
}

const char *goxel_get_version(int *major, int *minor, int *patch) {
    if (major) *major = 0;
    if (minor) *minor = 15;
    if (patch) *patch = 2;
    return "0.15.2";
}

bool goxel_has_feature(const char *feature) {
    (void)feature;
    return false;
}

// Stub implementations for all other API functions
goxel_error_t goxel_load_project(goxel_context_t *ctx, const char *path) { (void)ctx; (void)path; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_save_project(goxel_context_t *ctx, const char *path) { (void)ctx; (void)path; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_save_project_format(goxel_context_t *ctx, const char *path, const char *format) { (void)ctx; (void)path; (void)format; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_close_project(goxel_context_t *ctx) { (void)ctx; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_project_bounds(goxel_context_t *ctx, int *width, int *height, int *depth) { (void)ctx; (void)width; (void)height; (void)depth; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_remove_voxel(goxel_context_t *ctx, int x, int y, int z) { (void)ctx; (void)x; (void)y; (void)z; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_voxel(goxel_context_t *ctx, int x, int y, int z, goxel_color_t *color) { (void)ctx; (void)x; (void)y; (void)z; (void)color; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_add_voxel_batch(goxel_context_t *ctx, const goxel_pos_t *positions, const goxel_color_t *colors, size_t count) { (void)ctx; (void)positions; (void)colors; (void)count; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_remove_voxels_in_box(goxel_context_t *ctx, const goxel_box_t *box) { (void)ctx; (void)box; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_paint_voxel(goxel_context_t *ctx, int x, int y, int z, const goxel_color_t *color) { (void)ctx; (void)x; (void)y; (void)z; (void)color; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_create_layer(goxel_context_t *ctx, const char *name, const goxel_color_t *color, bool visible, goxel_layer_id_t *layer_id) { (void)ctx; (void)name; (void)color; (void)visible; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_delete_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_set_active_layer(goxel_context_t *ctx, goxel_layer_id_t layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_active_layer(goxel_context_t *ctx, goxel_layer_id_t *layer_id) { (void)ctx; (void)layer_id; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_set_layer_visibility(goxel_context_t *ctx, goxel_layer_id_t layer_id, bool visible) { (void)ctx; (void)layer_id; (void)visible; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_get_layer_count(goxel_context_t *ctx, int *count) { (void)ctx; (void)count; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_render_to_file(goxel_context_t *ctx, const char *output_path, const goxel_render_options_t *options) { (void)ctx; (void)output_path; (void)options; return GOXEL_ERROR_INVALID_OPERATION; }
goxel_error_t goxel_render_to_buffer(goxel_context_t *ctx, uint8_t **buffer, size_t *buffer_size, const goxel_render_options_t *options) { (void)ctx; (void)buffer; (void)buffer_size; (void)options; return GOXEL_ERROR_INVALID_OPERATION; }
const char *goxel_get_last_error(goxel_context_t *ctx) { (void)ctx; return NULL; }
goxel_error_t goxel_get_memory_usage(goxel_context_t *ctx, size_t *bytes_used, size_t *bytes_allocated) { (void)ctx; (void)bytes_used; (void)bytes_allocated; return GOXEL_ERROR_INVALID_OPERATION; }
'''

    # Write minimal stub to a temporary file
    stub_file = 'src/headless/goxel_headless_api_stub.c'
    with open(stub_file, 'w') as f:
        f.write(minimal_api_source)
    
    # For shared library, we need position-independent code
    api_env = env.Clone()
    api_env.Append(CCFLAGS=['-fPIC'])
    api_env.Append(CPPDEFINES=['GOXEL_HEADLESS=1'])
    
    # Build the shared library with minimal stub
    libgoxel = api_env.SharedLibrary(target='libgoxel-headless', source=[stub_file])
    
    # Install headers and library
    api_env.Install('build/lib', libgoxel)
    api_env.Install('build/include', 'include/goxel_headless.h')
    
    # Create alias for convenience
    api_env.Alias('c_api', ['build/lib', 'build/include'])
    
    print("Note: This is a stub implementation for Phase 4 demonstration.")

# Build executable targets
if env['headless']:
    target_name = 'goxel-headless'
elif env['cli_tools']:
    target_name = 'goxel-cli'
else:
    target_name = 'goxel'

if not env['c_api']:
    env.Program(target=target_name, source=sorted(sources))
