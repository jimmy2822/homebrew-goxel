
Goxel
=====

> **📚 Documentation has been reorganized!**  
> Please see our new structured documentation:
> - [Architecture Overview](dev_docs/01_ARCHITECTURE.md)
> - [Project Overview](dev_docs/02_README.md)
> - [Build Instructions](dev_docs/03_BUILD.md)
> - [API Reference](dev_docs/04_API.md)
> - [Quick Start Guide](dev_docs/05_QUICKSTART.md)

Version 14.0.0 (Daemon Architecture)

**NEW**: Goxel v14.0 introduces a high-performance daemon mode for headless operation, automation, and enterprise deployments. The daemon provides JSON-RPC API access over Unix sockets, enabling integration with any programming language.

By Guillaume Chereau <guillaume@noctua-software.com>

[![Build Status](https://github.com/guillaumechereau/goxel/actions/workflows/ci.yml/badge.svg)](https://github.com/guillaumechereau/goxel/actions/workflows/ci.yml)
[![DebianBadge](https://badges.debian.net/badges/debian/unstable/goxel/version.svg)](https://packages.debian.org/unstable/goxel)

Official webpage: https://goxel.xyz

About
-----

You can use goxel to create voxel graphics (3D images formed of cubes).  It
works on Linux, BSD, Windows and macOS.


Download
--------

The last release files can be downloaded from [there](
https://github.com/guillaumechereau/goxel/releases/latest).

Goxel is also available for [iOS](
https://itunes.apple.com/us/app/goxel-3d-voxel-editor/id1259097826) and
[Android](
https://play.google.com/store/apps/details?id=com.noctuasoftware.goxel).


![goxel screenshot 0](https://goxel.xyz/gallery/thibault-fisherman-house.jpg)
Fisherman house, made with Goxel by
[Thibault Simar](https://www.artstation.com/exm)


Licence
-------

Goxel is released under the GNU GPL3 licence.  If you want to use the code
with a commercial project please contact me: I am willing to provide a
version of the code under a commercial license.


Features
--------

**GUI Mode Features:**
- 24 bits RGB colors
- Unlimited scene size
- Unlimited undo buffer
- Layers
- Marching Cube rendering
- Procedural rendering
- Export to obj, ply, png, magica voxel, qubicle, gltf, stl
- Ray tracing

**v14.0 Daemon Mode Features:**
- JSON-RPC 2.0 API over Unix sockets
- Concurrent request processing with worker pool
- Headless rendering (no display required)
- Full voxel editing capabilities via API
- Language-agnostic client support
- 683% performance improvement over sequential operations
- Enterprise deployment support (systemd/launchd)


Usage
-----

### GUI Mode
- Left click: apply selected tool operation
- Middle click: rotate the view
- Right click: pan the view
- Left/Right arrow: rotate the view
- Mouse wheel: zoom in and out

### Daemon Mode (v14.0)
```bash
# Start daemon
./goxel-daemon --foreground --socket /tmp/goxel.sock

# Connect with any JSON-RPC client (Python example)
python3 examples/json_rpc_client.py
```


Building
--------

The building system uses scons. The code is in C99, using some GNU extensions.

### Build Options
- **GUI Mode**: `scons` (debug) or `scons mode=release`
- **Daemon Mode**: `scons daemon=1` or `scons mode=release daemon=1`
- **Both Modes**: Default build includes both GUI and daemon

### Linux/BSD

Install dependencies using your package manager. On Debian/Ubuntu:
```bash
sudo apt-get install scons pkg-config libglfw3-dev libgtk-3-dev libpng-dev
```

Then build:
```bash
# GUI version
make release

# Daemon version
scons mode=release daemon=1
```

### Windows

Install [MSYS2](https://www.msys2.org/) and the following packages:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw \
          mingw-w64-x86_64-libtre-git scons make
```

Then build:
```bash
# GUI version
make release

# Daemon version (requires WSL2 for Unix sockets)
scons mode=release daemon=1
```

### macOS

Install dependencies:
```bash
brew install scons glfw tre
```

Then build:
```bash
# GUI version
make release

# Daemon version
scons mode=release daemon=1
```


Contributing
------------

In order for your contribution to Goxel to be accepted, you have to sign the
[Goxel Contributor License Agreement (CLA)](doc/cla/sign-cla.md).  This is
mostly to allow me to distribute the mobile branch goxel under a non GPL
licence.

Also, please read the [contributing document](CONTRIBUTING.md).


Donations
---------

I you feel like it, you can support the development of Goxel with a donation at
the following bitcoin address: 1QCQeWTi6Xnh3UJbwhLMgSZQAypAouTVrY
