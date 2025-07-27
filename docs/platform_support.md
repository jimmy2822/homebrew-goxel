# Goxel v14.0 Daemon - Platform Support Matrix

## Overview

The Goxel v14.0 daemon architecture has been designed and tested across multiple platforms to ensure broad compatibility and consistent performance. This document details the support level, requirements, and known issues for each platform.

## Platform Compatibility Matrix

| Platform | Architecture | Support Level | Min Version | Status |
|----------|-------------|---------------|-------------|---------|
| **Linux** | | | | |
| Ubuntu | x86_64, ARM64 | Full | 20.04 LTS | ✅ Production |
| Debian | x86_64, ARM64 | Full | 10 (Buster) | ✅ Production |
| RHEL/CentOS | x86_64 | Full | 8 | ✅ Production |
| Alpine | x86_64, ARM64 | Full | 3.14 | ✅ Production |
| Arch | x86_64 | Community | Rolling | ✅ Tested |
| **macOS** | | | | |
| macOS | x86_64 | Full | 10.15 (Catalina) | ✅ Production |
| macOS | ARM64 (M1/M2) | Full | 11.0 (Big Sur) | ✅ Production |
| **Windows** | | | | |
| Windows | x86_64 | WSL2 | Windows 10 2004 | ⚠️ Beta |
| Windows | x86_64 | Native | Windows 10 | 🚧 Planned |
| **BSD** | | | | |
| FreeBSD | x86_64 | Community | 12.0 | ⚠️ Beta |
| OpenBSD | x86_64 | Community | 6.8 | 🚧 Planned |

### Support Levels
- **Full**: Officially supported, tested in CI, production-ready
- **Community**: Community-maintained, may work but not officially tested
- **WSL2**: Supported through Windows Subsystem for Linux 2
- **Beta**: In testing, may have known issues
- **Planned**: Support planned for future release

## Feature Availability by Platform

| Feature | Linux | macOS | Windows (WSL2) | Windows (Native) |
|---------|-------|-------|----------------|------------------|
| Unix Domain Sockets | ✅ | ✅ | ✅ | ❌ |
| Abstract Sockets | ✅ | ❌ | ✅ | ❌ |
| Named Pipes | ❌ | ❌ | ❌ | ✅ (planned) |
| Process Daemonization | ✅ | ✅ | ✅ | ⚠️ |
| Signal Handling | ✅ | ✅ | ✅ | ⚠️ |
| systemd Integration | ✅ | N/A | ✅ | N/A |
| launchd Integration | N/A | ✅ | N/A | N/A |
| Service Integration | N/A | N/A | N/A | ✅ (planned) |
| File Locking | ✅ | ✅ | ✅ | ✅ |
| Memory Mapping | ✅ | ✅ | ✅ | ✅ |
| Performance Monitoring | ✅ | ✅ | ✅ | ⚠️ |

## Performance Characteristics

### Linux
- **Startup Time**: < 80ms
- **Request Latency**: < 2.0ms
- **Memory Usage**: ~8MB baseline
- **Socket Creation**: < 5ms
- **Notes**: Best performance, full feature set

### macOS
- **Startup Time**: < 90ms
- **Request Latency**: < 2.2ms
- **Memory Usage**: ~10MB baseline
- **Socket Creation**: < 8ms
- **Notes**: Excellent performance, no abstract sockets

### Windows (WSL2)
- **Startup Time**: < 120ms
- **Request Latency**: < 3.0ms
- **Memory Usage**: ~12MB baseline
- **Socket Creation**: < 10ms
- **Notes**: Good performance, requires WSL2

## Platform-Specific Installation

### Linux

#### Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential libglfw3-dev libglew-dev libpng-dev

# Build and install
make -C tests -f Makefile.daemon
sudo make -C tests -f Makefile.daemon install-all

# Start with systemd
sudo systemctl start goxel-daemon
sudo systemctl enable goxel-daemon
```

#### Alpine Linux
```bash
# Install dependencies
apk add build-base glfw-dev glew-dev libpng-dev

# Build and install
make -C tests -f Makefile.daemon
make -C tests -f Makefile.daemon install-all

# Start with OpenRC
rc-service goxel-daemon start
rc-update add goxel-daemon
```

### macOS

```bash
# Install dependencies
brew install glfw glew libpng

# Build
make -C tests -f Makefile.daemon

# Install with launchd (user-level)
cp com.goxel.daemon.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.goxel.daemon.plist

# Or system-level (requires root)
sudo cp com.goxel.daemon.plist /Library/LaunchDaemons/
sudo launchctl load /Library/LaunchDaemons/com.goxel.daemon.plist
```

### Windows (WSL2)

```powershell
# Ensure WSL2 is installed
wsl --install

# Inside WSL2 Ubuntu
sudo apt-get update
sudo apt-get install -y build-essential libglfw3-dev libglew-dev libpng-dev

# Build and run
make -C tests -f Makefile.daemon
./tests/goxel-daemon --foreground
```

## Known Issues and Workarounds

### Linux
- **Issue**: SELinux may block socket creation
  - **Workaround**: Add SELinux policy or run in permissive mode
- **Issue**: AppArmor restrictions on Ubuntu
  - **Workaround**: Create AppArmor profile for goxel-daemon

### macOS
- **Issue**: Gatekeeper may block unsigned binary
  - **Workaround**: `xattr -d com.apple.quarantine goxel-daemon`
- **Issue**: Socket path length limited to 104 bytes
  - **Workaround**: Use shorter paths or /tmp directory

### Windows
- **Issue**: Native Windows doesn't support Unix sockets
  - **Workaround**: Use WSL2 or wait for named pipe support
- **Issue**: WSL2 socket files not visible from Windows
  - **Workaround**: Use network sockets for Windows interop

## Security Considerations

### File Permissions
- Socket files: 0660 (user and group read/write)
- PID files: 0644 (user write, others read)
- Log files: 0640 (user write, group read)

### Process Isolation
- Linux: Can use namespaces and cgroups
- macOS: Sandboxing not recommended (breaks socket access)
- Windows: Limited isolation in WSL2

### Network Security
- Unix sockets provide local-only access
- No network exposure by default
- Authentication via file permissions and peer credentials

## Testing Your Platform

Run the platform validation tool:

```bash
# Test current platform
python3 tools/cross_platform_validator.py --run-local

# Generate compatibility report
python3 tools/cross_platform_validator.py --generate-report --output report.html
```

## Contributing Platform Support

To add support for a new platform:

1. Create platform-specific tests in `tests/platforms/<platform>/`
2. Add platform detection to `tools/cross_platform_validator.py`
3. Update CI configuration in `.github/workflows/cross-platform.yml`
4. Document platform-specific requirements here
5. Submit PR with test results

## Future Platform Support

### Planned for v14.1
- Windows native support with named pipes
- FreeBSD full support
- NetBSD/OpenBSD community support

### Under Consideration
- Haiku OS
- Plan 9
- iOS/Android (for development tools)

## Support Policy

- **Full Support**: Security updates for 2 years, features for 1 year
- **Community Support**: Best-effort basis, community PRs welcome
- **Beta Support**: May have breaking changes between releases

For platform-specific issues, please file a GitHub issue with:
- Platform details (OS, version, architecture)
- Test results from validation tool
- Relevant logs and error messages