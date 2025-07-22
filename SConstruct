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
for root, dirnames, filenames in os.walk('src'):
    # Skip subdirectories we handle separately
    if 'core' in root or 'gui' in root or 'headless' in root:
        continue
    for filename in filenames:
        if filename.endswith('.c') or filename.endswith('.cpp'):
            # Skip GUI-specific files for headless build
            if env['headless'] or env['cli_tools']:
                if filename in ['gui.cpp', 'imgui.cpp', 'main.c']:
                    continue
            other_sources.append(os.path.join(root, filename))

sources = core_sources + other_sources
if not env['headless']:
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
if env['headless']:
    target_name = 'goxel-headless'
elif env['cli_tools']:
    target_name = 'goxel-cli'
else:
    target_name = 'goxel'

env.Program(target=target_name, source=sorted(sources))
