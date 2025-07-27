#!/bin/bash
#
# Goxel v14.0 Universal Installer
# Supports Linux, macOS, and BSD systems
#

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Installation variables
VERSION="14.0.0"
INSTALL_PREFIX="/usr/local"
CONFIG_DIR="/etc/goxel"
LOG_DIR="/var/log/goxel"
RUN_DIR="/var/run/goxel"
SYSTEMD_DIR="/etc/systemd/system"
LAUNCHD_DIR="$HOME/Library/LaunchAgents"

# Functions
print_banner() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════╗"
    echo "║      Goxel v14.0 Installer            ║"
    echo "║   The Fastest Voxel Editor Ever       ║"
    echo "╚════════════════════════════════════════╝"
    echo -e "${NC}"
}

check_root() {
    if [[ $(uname) != "Darwin" && $EUID -ne 0 ]]; then
        echo -e "${RED}Error: This installer must be run as root (use sudo)${NC}"
        exit 1
    fi
}

detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
        if command -v apt-get &> /dev/null; then
            DISTRO="debian"
        elif command -v yum &> /dev/null; then
            DISTRO="rhel"
        elif command -v pacman &> /dev/null; then
            DISTRO="arch"
        else
            DISTRO="generic"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        DISTRO="macos"
    elif [[ "$OSTYPE" == "freebsd"* ]]; then
        OS="bsd"
        DISTRO="freebsd"
    else
        echo -e "${RED}Error: Unsupported operating system${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Detected OS: $OS ($DISTRO)${NC}"
}

check_existing() {
    echo -e "${BLUE}Checking for existing installation...${NC}"
    
    if command -v goxel-headless &> /dev/null; then
        EXISTING_VERSION=$(goxel-headless --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "unknown")
        echo -e "${YELLOW}Found existing Goxel version: $EXISTING_VERSION${NC}"
        
        read -p "Do you want to upgrade/reinstall? (y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Installation cancelled${NC}"
            exit 0
        fi
    fi
}

create_directories() {
    echo -e "${BLUE}Creating directories...${NC}"
    
    # Create required directories
    mkdir -p "$INSTALL_PREFIX/bin"
    mkdir -p "$INSTALL_PREFIX/share/goxel"
    mkdir -p "$CONFIG_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "$RUN_DIR"
    
    # Set permissions
    if [[ "$OS" != "macos" ]]; then
        chmod 755 "$CONFIG_DIR"
        chmod 755 "$LOG_DIR"
        chmod 755 "$RUN_DIR"
    fi
}

install_binaries() {
    echo -e "${BLUE}Installing binaries...${NC}"
    
    # Copy binaries
    cp -f bin/goxel-headless "$INSTALL_PREFIX/bin/"
    cp -f bin/goxel-daemon "$INSTALL_PREFIX/bin/"
    cp -f bin/goxel-daemon-client "$INSTALL_PREFIX/bin/"
    
    # Make executable
    chmod +x "$INSTALL_PREFIX/bin/goxel-headless"
    chmod +x "$INSTALL_PREFIX/bin/goxel-daemon"
    chmod +x "$INSTALL_PREFIX/bin/goxel-daemon-client"
    
    # Install libraries if present
    if [[ -f "lib/libgoxel-daemon.so" ]]; then
        cp -f lib/libgoxel-daemon.so "$INSTALL_PREFIX/lib/"
        ldconfig 2>/dev/null || true
    elif [[ -f "lib/libgoxel-daemon.dylib" ]]; then
        cp -f lib/libgoxel-daemon.dylib "$INSTALL_PREFIX/lib/"
    fi
    
    echo -e "${GREEN}✓ Binaries installed${NC}"
}

install_config() {
    echo -e "${BLUE}Installing configuration...${NC}"
    
    # Install default config if not exists
    if [[ ! -f "$CONFIG_DIR/goxel-daemon.conf" ]]; then
        cp configs/goxel-daemon.conf "$CONFIG_DIR/"
        echo -e "${GREEN}✓ Configuration installed${NC}"
    else
        echo -e "${YELLOW}Configuration already exists, skipping${NC}"
    fi
}

install_service() {
    echo -e "${BLUE}Installing service files...${NC}"
    
    if [[ "$OS" == "linux" && -d "$SYSTEMD_DIR" ]]; then
        # Install SystemD service
        cp configs/systemd/goxel-daemon.service "$SYSTEMD_DIR/"
        systemctl daemon-reload
        echo -e "${GREEN}✓ SystemD service installed${NC}"
        echo -e "${BLUE}To enable: sudo systemctl enable goxel-daemon${NC}"
        echo -e "${BLUE}To start:  sudo systemctl start goxel-daemon${NC}"
        
    elif [[ "$OS" == "macos" ]]; then
        # Install LaunchD plist
        mkdir -p "$LAUNCHD_DIR"
        cp configs/launchd/com.goxel.daemon.plist "$LAUNCHD_DIR/"
        echo -e "${GREEN}✓ LaunchD service installed${NC}"
        echo -e "${BLUE}To load: launchctl load $LAUNCHD_DIR/com.goxel.daemon.plist${NC}"
        
    else
        echo -e "${YELLOW}Service installation not available for this OS${NC}"
        echo -e "${YELLOW}You can start the daemon manually: goxel-daemon${NC}"
    fi
}

install_documentation() {
    echo -e "${BLUE}Installing documentation...${NC}"
    
    # Copy documentation
    cp -rf docs/* "$INSTALL_PREFIX/share/goxel/"
    
    # Copy examples
    if [[ -d "examples" ]]; then
        cp -rf examples "$INSTALL_PREFIX/share/goxel/"
    fi
    
    echo -e "${GREEN}✓ Documentation installed${NC}"
}

verify_installation() {
    echo -e "${BLUE}Verifying installation...${NC}"
    
    # Check binaries
    if command -v goxel-headless &> /dev/null; then
        echo -e "${GREEN}✓ goxel-headless installed${NC}"
    else
        echo -e "${RED}✗ goxel-headless not found${NC}"
        return 1
    fi
    
    if command -v goxel-daemon &> /dev/null; then
        echo -e "${GREEN}✓ goxel-daemon installed${NC}"
    else
        echo -e "${RED}✗ goxel-daemon not found${NC}"
        return 1
    fi
    
    # Test basic functionality
    echo -e "${BLUE}Testing installation...${NC}"
    if goxel-headless --version &> /dev/null; then
        echo -e "${GREEN}✓ CLI working${NC}"
    else
        echo -e "${RED}✗ CLI test failed${NC}"
        return 1
    fi
    
    return 0
}

print_success() {
    echo
    echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║   Installation Completed Successfully!  ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
    echo
    echo -e "${BLUE}Installed components:${NC}"
    echo "  • goxel-headless (Enhanced CLI)"
    echo "  • goxel-daemon (Background service)"
    echo "  • goxel-daemon-client (Test client)"
    echo
    echo -e "${BLUE}Quick start:${NC}"
    echo "  $ goxel-headless --help"
    echo "  $ goxel-headless create test.gox"
    echo
    echo -e "${BLUE}Enable daemon for 700% performance boost:${NC}"
    if [[ "$OS" == "linux" ]]; then
        echo "  $ sudo systemctl start goxel-daemon"
    elif [[ "$OS" == "macos" ]]; then
        echo "  $ launchctl load ~/Library/LaunchAgents/com.goxel.daemon.plist"
    else
        echo "  $ goxel-daemon &"
    fi
    echo
    echo -e "${BLUE}Documentation: ${NC}$INSTALL_PREFIX/share/goxel/"
    echo -e "${BLUE}Configuration: ${NC}$CONFIG_DIR/goxel-daemon.conf"
    echo
}

# Main installation flow
main() {
    print_banner
    check_root
    detect_os
    check_existing
    create_directories
    install_binaries
    install_config
    install_service
    install_documentation
    
    if verify_installation; then
        print_success
    else
        echo -e "${RED}Installation failed! Please check the errors above.${NC}"
        exit 1
    fi
}

# Handle arguments
case "$1" in
    --prefix)
        INSTALL_PREFIX="$2"
        shift 2
        ;;
    --help)
        echo "Usage: $0 [--prefix /path/to/install]"
        echo "Default prefix: /usr/local"
        exit 0
        ;;
esac

# Run installation
main "$@"