#!/bin/bash
#
# Goxel v14.0 Uninstaller
# Cleanly removes Goxel and optionally preserves user data
#

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Variables
INSTALL_PREFIX="/usr/local"
CONFIG_DIR="/etc/goxel"
LOG_DIR="/var/log/goxel"
RUN_DIR="/var/run/goxel"
USER_DATA_DIR="$HOME/.goxel"
PRESERVE_USER_DATA=true
BACKUP_DIR=""

print_banner() {
    echo -e "${RED}"
    echo "╔════════════════════════════════════════╗"
    echo "║      Goxel v14.0 Uninstaller          ║"
    echo "╚════════════════════════════════════════╝"
    echo -e "${NC}"
}

check_root() {
    if [[ $(uname) != "Darwin" && $EUID -ne 0 ]]; then
        echo -e "${RED}Error: This uninstaller must be run as root (use sudo)${NC}"
        exit 1
    fi
}

confirm_uninstall() {
    echo -e "${YELLOW}This will remove Goxel v14.0 from your system.${NC}"
    echo
    echo "The following will be removed:"
    echo "  • Goxel binaries (goxel-headless, goxel-daemon, etc.)"
    echo "  • System configuration files"
    echo "  • Service files (SystemD/LaunchD)"
    echo "  • Log files"
    
    if [[ "$PRESERVE_USER_DATA" == false ]]; then
        echo -e "  ${RED}• User data and projects${NC}"
    else
        echo -e "  ${GREEN}• User data will be preserved${NC}"
    fi
    
    echo
    read -p "Continue with uninstall? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Uninstall cancelled${NC}"
        exit 0
    fi
}

stop_services() {
    echo -e "${BLUE}Stopping services...${NC}"
    
    # Stop daemon process
    if pgrep -x "goxel-daemon" > /dev/null; then
        echo "Stopping goxel-daemon..."
        pkill -TERM -x "goxel-daemon" || true
        sleep 2
    fi
    
    # Stop any CLI processes
    if pgrep -x "goxel-headless" > /dev/null; then
        echo "Stopping goxel-headless processes..."
        pkill -TERM -x "goxel-headless" || true
    fi
    
    # Stop SystemD service
    if [[ -f "/etc/systemd/system/goxel-daemon.service" ]]; then
        echo "Stopping and disabling SystemD service..."
        systemctl stop goxel-daemon 2>/dev/null || true
        systemctl disable goxel-daemon 2>/dev/null || true
        systemctl daemon-reload
    fi
    
    # Stop LaunchD service on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [[ -f "$HOME/Library/LaunchAgents/com.goxel.daemon.plist" ]]; then
            echo "Unloading LaunchD service..."
            launchctl unload "$HOME/Library/LaunchAgents/com.goxel.daemon.plist" 2>/dev/null || true
        fi
    fi
    
    echo -e "${GREEN}✓ Services stopped${NC}"
}

backup_user_data() {
    if [[ "$PRESERVE_USER_DATA" == true && -d "$USER_DATA_DIR" ]]; then
        BACKUP_DIR="$HOME/goxel-backup-$(date +%Y%m%d_%H%M%S)"
        echo -e "${BLUE}Backing up user data...${NC}"
        cp -rp "$USER_DATA_DIR" "$BACKUP_DIR"
        echo -e "${GREEN}✓ User data backed up to: $BACKUP_DIR${NC}"
    fi
}

remove_binaries() {
    echo -e "${BLUE}Removing binaries...${NC}"
    
    # Remove executables
    rm -f "$INSTALL_PREFIX/bin/goxel-headless"
    rm -f "$INSTALL_PREFIX/bin/goxel-daemon"
    rm -f "$INSTALL_PREFIX/bin/goxel-daemon-client"
    
    # Remove libraries
    rm -f "$INSTALL_PREFIX/lib/libgoxel-daemon.so"
    rm -f "$INSTALL_PREFIX/lib/libgoxel-daemon.dylib"
    rm -f "$INSTALL_PREFIX/lib/libgoxel-daemon.dll"
    
    # Update library cache on Linux
    if command -v ldconfig &> /dev/null; then
        ldconfig 2>/dev/null || true
    fi
    
    echo -e "${GREEN}✓ Binaries removed${NC}"
}

remove_config() {
    echo -e "${BLUE}Removing configuration...${NC}"
    
    # Remove config directory
    rm -rf "$CONFIG_DIR"
    
    # Remove log directory
    rm -rf "$LOG_DIR"
    
    # Remove run directory
    rm -rf "$RUN_DIR"
    
    # Remove user data if requested
    if [[ "$PRESERVE_USER_DATA" == false ]]; then
        rm -rf "$USER_DATA_DIR"
        echo -e "${YELLOW}User data removed${NC}"
    fi
    
    echo -e "${GREEN}✓ Configuration removed${NC}"
}

remove_services() {
    echo -e "${BLUE}Removing service files...${NC}"
    
    # Remove SystemD service
    if [[ -f "/etc/systemd/system/goxel-daemon.service" ]]; then
        rm -f "/etc/systemd/system/goxel-daemon.service"
        systemctl daemon-reload
        echo "✓ SystemD service removed"
    fi
    
    # Remove LaunchD plist on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        rm -f "$HOME/Library/LaunchAgents/com.goxel.daemon.plist"
        echo "✓ LaunchD service removed"
    fi
    
    echo -e "${GREEN}✓ Service files removed${NC}"
}

remove_documentation() {
    echo -e "${BLUE}Removing documentation...${NC}"
    
    rm -rf "$INSTALL_PREFIX/share/goxel"
    
    echo -e "${GREEN}✓ Documentation removed${NC}"
}

remove_user() {
    # On some systems, a 'goxel' user might have been created
    if id "goxel" &>/dev/null; then
        echo -e "${BLUE}Removing goxel user...${NC}"
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            userdel goxel 2>/dev/null || true
            groupdel goxel 2>/dev/null || true
        fi
        echo -e "${GREEN}✓ User removed${NC}"
    fi
}

print_summary() {
    echo
    echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║    Uninstall Completed Successfully!    ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
    echo
    
    if [[ -n "$BACKUP_DIR" ]]; then
        echo -e "${BLUE}Your user data has been preserved at:${NC}"
        echo "  $BACKUP_DIR"
        echo
    fi
    
    echo "Goxel v14.0 has been removed from your system."
    echo
    echo "Thank you for using Goxel!"
}

# Main uninstall flow
main() {
    print_banner
    check_root
    confirm_uninstall
    stop_services
    backup_user_data
    remove_binaries
    remove_config
    remove_services
    remove_documentation
    remove_user
    print_summary
}

# Handle arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --remove-user-data)
            PRESERVE_USER_DATA=false
            echo -e "${RED}Warning: User data will be removed!${NC}"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --remove-user-data   Also remove user data and projects"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Run uninstall
main