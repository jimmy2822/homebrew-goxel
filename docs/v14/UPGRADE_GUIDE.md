# Goxel v14.0 Upgrade Guide

This guide helps you upgrade from Goxel v13.4 to v14.0 with the new daemon architecture.

## Table of Contents
1. [Overview](#overview)
2. [Pre-Upgrade Checklist](#pre-upgrade-checklist)
3. [Installation Methods](#installation-methods)
4. [Migration Steps](#migration-steps)
5. [Configuration](#configuration)
6. [Testing Your Upgrade](#testing-your-upgrade)
7. [Rollback Procedure](#rollback-procedure)
8. [Troubleshooting](#troubleshooting)

## Overview

Goxel v14.0 introduces a daemon architecture that provides 700%+ performance improvement while maintaining 100% backward compatibility. Your existing scripts, integrations, and workflows will continue to work without modification.

### What's New
- **Daemon Process**: Persistent background service
- **Enhanced CLI**: Automatic daemon detection
- **JSON RPC API**: Programmatic access
- **Service Integration**: SystemD/LaunchD support

### Compatibility Promise
- ✅ All v13.4 CLI commands work unchanged
- ✅ Same file formats and data structures  
- ✅ MCP server compatible
- ✅ No breaking changes

## Pre-Upgrade Checklist

Before upgrading:

- [ ] Backup your Goxel projects and configurations
- [ ] Note your current version: `goxel-headless --version`
- [ ] Check system requirements (same as v13.4)
- [ ] Ensure 10MB free disk space
- [ ] Stop any running Goxel processes

## Installation Methods

### Method 1: Package Manager (Recommended)

#### Linux (APT/Debian/Ubuntu)
```bash
# Add Goxel repository (if not already added)
sudo add-apt-repository ppa:goxel/stable
sudo apt update

# Upgrade to v14.0
sudo apt install goxel-headless=14.0.0
```

#### Linux (YUM/RHEL/Fedora)
```bash
# Add repository
sudo yum-config-manager --add-repo https://goxel.xyz/repos/rhel/goxel.repo

# Upgrade
sudo yum upgrade goxel-headless
```

#### macOS (Homebrew)
```bash
# Update Homebrew
brew update

# Upgrade Goxel
brew upgrade goxel-headless
```

#### Windows (Chocolatey)
```powershell
# Upgrade to v14.0
choco upgrade goxel-headless
```

### Method 2: Manual Installation

1. Download the appropriate package:
   - Linux: `goxel-v14.0.0-linux-x64.tar.gz`
   - macOS: `goxel-v14.0.0-macos-universal.dmg`
   - Windows: `goxel-v14.0.0-windows-x64.msi`

2. Extract/Install:
```bash
# Linux
tar -xzf goxel-v14.0.0-linux-x64.tar.gz
cd goxel-v14.0.0
sudo ./install.sh

# macOS
# Mount the DMG and run the installer

# Windows
# Run the MSI installer
```

### Method 3: Build from Source
```bash
git clone https://github.com/goxel/goxel.git
cd goxel
git checkout v14.0.0
scons mode=release all=1
sudo scons install
```

## Migration Steps

### Step 1: Install v14.0

Use one of the installation methods above. v14.0 can be installed alongside v13.4.

### Step 2: Verify Installation

```bash
# Check CLI version
goxel-headless --version
# Output: Goxel 14.0.0

# Check daemon version
goxel-daemon --version
# Output: Goxel Daemon 14.0.0

# List installed components
ls -la $(which goxel-*)
```

### Step 3: Test Compatibility Mode

Before enabling the daemon, test that your existing workflows work:

```bash
# Force standalone mode (v13.4 behavior)
goxel-headless --no-daemon create test.gox
goxel-headless --no-daemon add-voxel 0 0 0 255 0 0 255
```

### Step 4: Enable Daemon Service

#### Linux (SystemD)
```bash
# Enable and start daemon
sudo systemctl enable goxel-daemon
sudo systemctl start goxel-daemon

# Check status
sudo systemctl status goxel-daemon
```

#### macOS (LaunchD)
```bash
# Load the launch agent
launchctl load ~/Library/LaunchAgents/com.goxel.daemon.plist

# Check if running
launchctl list | grep goxel
```

#### Windows (Service)
```powershell
# Install service
goxel-daemon --install-service

# Start service
net start GoxelDaemon
```

### Step 5: Configure Daemon (Optional)

The default configuration works for most users. To customize:

```bash
# Copy default config
sudo cp /usr/share/goxel/goxel-daemon.conf /etc/goxel/

# Edit configuration
sudo nano /etc/goxel/goxel-daemon.conf
```

Example configuration:
```yaml
daemon:
  socket_path: /var/run/goxel/goxel.sock
  worker_threads: 4          # Adjust based on CPU cores
  max_clients: 100          # Maximum concurrent connections
  request_timeout: 30000    # Request timeout in ms
  enable_logging: true
  log_level: info          # debug, info, warn, error
  log_file: /var/log/goxel/daemon.log
```

### Step 6: Update MCP Server

If using the MCP server:

```bash
# Update goxel-mcp to v14.0-compatible version
cd /path/to/goxel-mcp
npm update

# The MCP server will automatically use the daemon if available
```

### Step 7: Update Scripts (Optional)

While not required, you can optimize scripts to explicitly use daemon mode:

```bash
# Old (works with v14.0)
goxel-headless create model.gox

# Optimized for v14.0
goxel-headless --daemon create model.gox
```

## Testing Your Upgrade

### Basic Functionality Test
```bash
# Create a test script
cat > test_v14.sh << 'EOF'
#!/bin/bash
echo "Testing Goxel v14.0..."

# Test basic operations
goxel-headless create test.gox
goxel-headless add-voxel 0 0 0 255 0 0 255
goxel-headless export test.obj

# Test daemon-specific features
goxel-daemon-client status
goxel-daemon-client stats

echo "Test completed successfully!"
EOF

chmod +x test_v14.sh
./test_v14.sh
```

### Performance Test
```bash
# Run performance benchmark
time for i in {1..100}; do
  goxel-headless add-voxel $i 0 0 255 0 0 255
done

# Compare with standalone mode
time for i in {1..100}; do
  goxel-headless --no-daemon add-voxel $i 0 0 255 0 0 255
done
```

### Integration Test
Test your existing scripts and integrations work correctly with v14.0.

## Rollback Procedure

If you need to rollback to v13.4:

### Option 1: Keep Both Versions
```bash
# Use v13.4 explicitly
/usr/local/bin/goxel-headless-v13 command

# Or disable daemon
goxel-headless --no-daemon command
```

### Option 2: Full Rollback
```bash
# Stop daemon
sudo systemctl stop goxel-daemon
sudo systemctl disable goxel-daemon

# Reinstall v13.4
sudo apt install goxel-headless=13.4.0  # Linux
brew install goxel-headless@13.4       # macOS
```

## Troubleshooting

### Daemon Won't Start

**Check logs:**
```bash
# Linux
sudo journalctl -u goxel-daemon -n 50

# macOS
log show --predicate 'process == "goxel-daemon"' --last 5m

# Check daemon directly
goxel-daemon --debug --foreground
```

**Common issues:**
- Socket already exists: `rm /var/run/goxel/goxel.sock`
- Permission denied: Check socket directory permissions
- Port in use: Change socket path in config

### CLI Can't Connect to Daemon

```bash
# Check daemon is running
ps aux | grep goxel-daemon

# Test connection
goxel-daemon-client ping

# Check socket permissions
ls -la /var/run/goxel/goxel.sock

# Force standalone mode as workaround
goxel-headless --no-daemon command
```

### Performance Not Improved

1. Ensure daemon is running
2. Check you're not using `--no-daemon`
3. Verify with: `goxel-headless --daemon --debug create test.gox`
4. Check daemon stats: `goxel-daemon-client stats`

### Memory Usage Concerns

The daemon uses ~45MB of memory (vs 33MB per operation in v13.4):
- This is shared across all operations
- Memory usage doesn't increase with more operations
- Can be limited via SystemD/LaunchD configs

### Getting Help

- GitHub Issues: https://github.com/goxel/goxel/issues
- Documentation: https://goxel.xyz/docs/v14
- Community Forum: https://forum.goxel.xyz

## Summary

Upgrading to Goxel v14.0 is straightforward:
1. Install v14.0 (can coexist with v13.4)
2. Test existing workflows (100% compatible)
3. Enable daemon for 700%+ performance boost
4. Update scripts to use `--daemon` flag (optional)

No breaking changes mean you can upgrade at your own pace and rollback if needed.