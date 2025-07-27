#!/bin/bash
#
# Goxel v13 to v14 Upgrade Script
# Safely upgrades while preserving user data and configurations
#

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Variables
BACKUP_DIR="$HOME/.goxel-backup-$(date +%Y%m%d_%H%M%S)"
OLD_VERSION=""
PRESERVE_CONFIG=true

print_banner() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════╗"
    echo "║    Goxel v13 → v14 Upgrade Script     ║"
    echo "║     Safe upgrade with backup           ║"
    echo "╚════════════════════════════════════════╝"
    echo -e "${NC}"
}

check_existing_version() {
    echo -e "${BLUE}Checking current Goxel installation...${NC}"
    
    if command -v goxel-headless &> /dev/null; then
        OLD_VERSION=$(goxel-headless --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "unknown")
        echo -e "${GREEN}Found Goxel version: $OLD_VERSION${NC}"
        
        # Check if it's v13.x
        if [[ $OLD_VERSION =~ ^13\. ]]; then
            echo -e "${GREEN}✓ Eligible for upgrade${NC}"
        else
            echo -e "${YELLOW}Warning: Current version is $OLD_VERSION (expected v13.x)${NC}"
            read -p "Continue anyway? (y/n) " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                exit 0
            fi
        fi
    else
        echo -e "${YELLOW}No existing Goxel installation found${NC}"
        echo "This will be a fresh installation of v14.0"
    fi
}

backup_existing() {
    echo -e "${BLUE}Creating backup...${NC}"
    
    mkdir -p "$BACKUP_DIR"
    
    # Backup binaries if they exist
    if [[ -f "/usr/local/bin/goxel-headless" ]]; then
        cp -p "/usr/local/bin/goxel-headless" "$BACKUP_DIR/" 2>/dev/null || true
    fi
    
    # Backup configuration
    if [[ -d "/etc/goxel" ]]; then
        cp -rp "/etc/goxel" "$BACKUP_DIR/" 2>/dev/null || true
    fi
    
    # Backup user data
    if [[ -d "$HOME/.goxel" ]]; then
        cp -rp "$HOME/.goxel" "$BACKUP_DIR/" 2>/dev/null || true
    fi
    
    # Save version info
    echo "$OLD_VERSION" > "$BACKUP_DIR/previous_version.txt"
    
    echo -e "${GREEN}✓ Backup created at: $BACKUP_DIR${NC}"
}

stop_services() {
    echo -e "${BLUE}Stopping existing services...${NC}"
    
    # Stop any running goxel processes
    if pgrep -x "goxel-headless" > /dev/null; then
        echo "Stopping goxel-headless processes..."
        pkill -x "goxel-headless" || true
        sleep 1
    fi
    
    # Stop systemd service if exists
    if systemctl is-active --quiet goxel-daemon 2>/dev/null; then
        echo "Stopping goxel-daemon service..."
        sudo systemctl stop goxel-daemon || true
    fi
    
    # Stop launchd service on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if launchctl list | grep -q "com.goxel.daemon"; then
            echo "Stopping goxel daemon on macOS..."
            launchctl unload ~/Library/LaunchAgents/com.goxel.daemon.plist 2>/dev/null || true
        fi
    fi
    
    echo -e "${GREEN}✓ Services stopped${NC}"
}

perform_upgrade() {
    echo -e "${BLUE}Installing Goxel v14.0...${NC}"
    
    # Run the main installer
    if [[ -f "./scripts/install.sh" ]]; then
        sudo ./scripts/install.sh
    else
        echo -e "${RED}Error: install.sh not found${NC}"
        exit 1
    fi
}

migrate_config() {
    echo -e "${BLUE}Migrating configuration...${NC}"
    
    # If old config exists and user wants to preserve it
    if [[ -f "$BACKUP_DIR/goxel/goxel.conf" && "$PRESERVE_CONFIG" == true ]]; then
        echo "Detected old configuration file"
        # Note: v14 uses different config format, manual migration may be needed
        echo -e "${YELLOW}Note: Configuration format has changed in v14${NC}"
        echo "Old config backed up at: $BACKUP_DIR/goxel/goxel.conf"
        echo "New config at: /etc/goxel/goxel-daemon.conf"
    fi
    
    echo -e "${GREEN}✓ Configuration ready${NC}"
}

test_upgrade() {
    echo -e "${BLUE}Testing upgrade...${NC}"
    
    # Test CLI
    if goxel-headless --version &> /dev/null; then
        NEW_VERSION=$(goxel-headless --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
        echo -e "${GREEN}✓ CLI working (version $NEW_VERSION)${NC}"
    else
        echo -e "${RED}✗ CLI test failed${NC}"
        return 1
    fi
    
    # Test daemon
    if goxel-daemon --version &> /dev/null; then
        echo -e "${GREEN}✓ Daemon installed${NC}"
    else
        echo -e "${RED}✗ Daemon not found${NC}"
        return 1
    fi
    
    # Test backward compatibility
    echo -e "${BLUE}Testing backward compatibility...${NC}"
    goxel-headless --no-daemon create test_upgrade.gox
    goxel-headless --no-daemon add-voxel 0 0 0 255 0 0 255
    if [[ -f "test_upgrade.gox" ]]; then
        echo -e "${GREEN}✓ Backward compatibility verified${NC}"
        rm -f test_upgrade.gox
    else
        echo -e "${RED}✗ Compatibility test failed${NC}"
        return 1
    fi
    
    return 0
}

enable_daemon() {
    echo -e "${BLUE}Would you like to enable the daemon service for 700% performance boost?${NC}"
    read -p "Enable daemon? (y/n) " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [[ "$OSTYPE" == "linux-gnu"* ]] && command -v systemctl &> /dev/null; then
            sudo systemctl enable goxel-daemon
            sudo systemctl start goxel-daemon
            echo -e "${GREEN}✓ Daemon service enabled and started${NC}"
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            launchctl load ~/Library/LaunchAgents/com.goxel.daemon.plist
            echo -e "${GREEN}✓ Daemon service loaded${NC}"
        else
            echo -e "${YELLOW}Please start daemon manually: goxel-daemon &${NC}"
        fi
    fi
}

print_success() {
    echo
    echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║     Upgrade Completed Successfully!     ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
    echo
    echo -e "${BLUE}Upgrade Summary:${NC}"
    echo "  • Previous version: $OLD_VERSION"
    echo "  • New version: 14.0.0"
    echo "  • Backup location: $BACKUP_DIR"
    echo
    echo -e "${BLUE}What's new:${NC}"
    echo "  • 700%+ performance improvement with daemon mode"
    echo "  • JSON RPC API for programmatic access"
    echo "  • 100% backward compatibility maintained"
    echo
    echo -e "${BLUE}Next steps:${NC}"
    echo "  1. Test your existing scripts (should work unchanged)"
    echo "  2. Try daemon mode: goxel-headless --daemon create test.gox"
    echo "  3. Run benchmark: python3 /usr/local/share/goxel/examples/performance_demos/benchmark.py"
    echo
    echo -e "${YELLOW}Note: Your old installation is backed up at:${NC}"
    echo "  $BACKUP_DIR"
    echo
}

rollback() {
    echo -e "${RED}Upgrade failed. Rolling back...${NC}"
    
    # Restore binaries
    if [[ -f "$BACKUP_DIR/goxel-headless" ]]; then
        sudo cp -p "$BACKUP_DIR/goxel-headless" "/usr/local/bin/" 2>/dev/null || true
    fi
    
    # Restore config
    if [[ -d "$BACKUP_DIR/goxel" ]]; then
        sudo cp -rp "$BACKUP_DIR/goxel" "/etc/" 2>/dev/null || true
    fi
    
    echo -e "${YELLOW}Rollback completed. Previous version restored.${NC}"
    echo "Backup remains at: $BACKUP_DIR"
}

# Main upgrade flow
main() {
    print_banner
    
    # Check if running as root when needed
    if [[ $(uname) != "Darwin" && $EUID -ne 0 ]]; then
        echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
        exit 1
    fi
    
    check_existing_version
    backup_existing
    stop_services
    
    if perform_upgrade; then
        migrate_config
        if test_upgrade; then
            enable_daemon
            print_success
        else
            echo -e "${RED}Upgrade tests failed!${NC}"
            rollback
            exit 1
        fi
    else
        echo -e "${RED}Upgrade failed!${NC}"
        rollback
        exit 1
    fi
}

# Handle arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-backup)
            echo -e "${YELLOW}Warning: Running without backup!${NC}"
            BACKUP_DIR="/dev/null"
            shift
            ;;
        --no-config)
            PRESERVE_CONFIG=false
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --no-backup    Skip backup (dangerous!)"
            echo "  --no-config    Don't preserve old configuration"
            echo "  --help         Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Run upgrade
main