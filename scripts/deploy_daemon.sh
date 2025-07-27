#!/bin/bash

# Goxel 3D voxels editor
#
# copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
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

# ============================================================================
# GOXEL DAEMON DEPLOYMENT SCRIPT
# ============================================================================

set -e

# Script configuration
SCRIPT_NAME="$(basename "$0")"
SCRIPT_VERSION="14.0.0-daemon"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
DEFAULT_INSTALL_PREFIX="/usr"
DEFAULT_SERVICE_USER="goxel"
DEFAULT_SERVICE_GROUP="goxel"

# Platform detection
PLATFORM="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

# Colors for output
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    BOLD=''
    NC=''
fi

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

log_debug() {
    if [[ "${VERBOSE:-}" == "1" ]]; then
        echo -e "${BOLD}[DEBUG]${NC} $*"
    fi
}

die() {
    log_error "$*"
    exit 1
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

require_root() {
    if [[ $EUID -ne 0 ]]; then
        die "This operation requires root privileges. Please run with sudo."
    fi
}

require_command() {
    if ! command_exists "$1"; then
        die "Required command not found: $1"
    fi
}

# ============================================================================
# PLATFORM-SPECIFIC FUNCTIONS
# ============================================================================

get_platform_defaults() {
    case "$PLATFORM" in
        linux)
            INSTALL_PREFIX="${INSTALL_PREFIX:-/usr}"
            BIN_DIR="$INSTALL_PREFIX/bin"
            LIB_DIR="/var/lib/goxel"
            LOG_DIR="/var/log"
            RUN_DIR="/var/run"
            CONFIG_DIR="/etc/goxel"
            SERVICE_TYPE="systemd"
            ;;
        darwin)
            INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
            BIN_DIR="$INSTALL_PREFIX/bin"
            LIB_DIR="$INSTALL_PREFIX/var/lib/goxel"
            LOG_DIR="$INSTALL_PREFIX/var/log"
            RUN_DIR="$INSTALL_PREFIX/var/run"
            CONFIG_DIR="$INSTALL_PREFIX/etc/goxel"
            SERVICE_TYPE="launchd"
            ;;
        *)
            die "Unsupported platform: $PLATFORM"
            ;;
    esac
}

create_service_user() {
    local user="$1"
    local group="${2:-$user}"
    
    log_info "Creating service user: $user"
    
    case "$PLATFORM" in
        linux)
            if ! id "$user" &>/dev/null; then
                useradd -r -s /bin/false -d "$LIB_DIR" "$user"
                log_success "Created user: $user"
            else
                log_info "User already exists: $user"
            fi
            ;;
        darwin)
            local uid="${USER_ID:-301}"
            local gid="${GROUP_ID:-301}"
            
            if ! dscl . -read "/Users/$user" &>/dev/null; then
                dscl . -create "/Users/$user"
                dscl . -create "/Users/$user" UniqueID "$uid"
                dscl . -create "/Users/$user" PrimaryGroupID "$gid"
                dscl . -create "/Users/$user" UserShell /usr/bin/false
                dscl . -create "/Users/$user" NFSHomeDirectory "$LIB_DIR"
                log_success "Created user: $user"
            else
                log_info "User already exists: $user"
            fi
            ;;
    esac
}

create_directories() {
    log_info "Creating required directories..."
    
    local dirs=("$LIB_DIR" "$LOG_DIR" "$RUN_DIR" "$CONFIG_DIR")
    
    for dir in "${dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            mkdir -p "$dir"
            log_debug "Created directory: $dir"
        fi
    done
    
    # Set ownership for service directories
    chown "$SERVICE_USER:$SERVICE_GROUP" "$LIB_DIR"
    chmod 755 "$LIB_DIR"
    
    log_success "Directories created and configured"
}

install_binary() {
    local binary_path="$1"
    local target_path="$BIN_DIR/goxel-daemon"
    
    log_info "Installing daemon binary..."
    
    if [[ ! -f "$binary_path" ]]; then
        die "Daemon binary not found: $binary_path"
    fi
    
    if [[ ! -x "$binary_path" ]]; then
        die "Daemon binary is not executable: $binary_path"
    fi
    
    # Copy binary to target location
    cp "$binary_path" "$target_path"
    chmod 755 "$target_path"
    chown root:root "$target_path"
    
    log_success "Binary installed: $target_path"
    
    # Verify installation
    if "$target_path" --version >/dev/null; then
        log_success "Binary verification passed"
    else
        die "Binary verification failed"
    fi
}

install_configuration() {
    log_info "Installing configuration files..."
    
    local config_source="$PROJECT_ROOT/config/daemon_config.json"
    local config_target="$CONFIG_DIR/daemon_config.json"
    
    if [[ -f "$config_source" ]]; then
        cp "$config_source" "$config_target"
        chmod 640 "$config_target"
        chown root:"$SERVICE_GROUP" "$config_target"
        log_success "Configuration installed: $config_target"
    else
        log_warning "Configuration file not found: $config_source"
    fi
}

install_service() {
    log_info "Installing system service..."
    
    case "$SERVICE_TYPE" in
        systemd)
            install_systemd_service
            ;;
        launchd)
            install_launchd_service
            ;;
        *)
            die "Unsupported service type: $SERVICE_TYPE"
            ;;
    esac
}

install_systemd_service() {
    require_command systemctl
    
    local service_source="$PROJECT_ROOT/config/goxel-daemon.service"
    local service_target="/etc/systemd/system/goxel-daemon.service"
    
    if [[ ! -f "$service_source" ]]; then
        die "Systemd service file not found: $service_source"
    fi
    
    # Customize service file with actual paths
    sed \
        -e "s|/usr/local/bin/goxel-daemon|$BIN_DIR/goxel-daemon|g" \
        -e "s|/var/run/goxel-daemon|$RUN_DIR/goxel-daemon|g" \
        -e "s|/var/log/goxel-daemon|$LOG_DIR/goxel-daemon|g" \
        -e "s|/var/lib/goxel|$LIB_DIR|g" \
        -e "s|User=goxel|User=$SERVICE_USER|g" \
        -e "s|Group=goxel|Group=$SERVICE_GROUP|g" \
        "$service_source" > "$service_target"
    
    chmod 644 "$service_target"
    
    # Reload systemd and enable service
    systemctl daemon-reload
    systemctl enable goxel-daemon
    
    log_success "Systemd service installed and enabled"
}

install_launchd_service() {
    require_command launchctl
    
    local plist_source="$PROJECT_ROOT/config/com.goxel.daemon.plist"
    local plist_target="/Library/LaunchDaemons/com.goxel.daemon.plist"
    
    if [[ ! -f "$plist_source" ]]; then
        die "LaunchDaemon plist not found: $plist_source"
    fi
    
    # Customize plist with actual paths
    sed \
        -e "s|/usr/local/bin/goxel-daemon|$BIN_DIR/goxel-daemon|g" \
        -e "s|/usr/local/var/run/goxel-daemon|$RUN_DIR/goxel-daemon|g" \
        -e "s|/usr/local/var/log/goxel-daemon|$LOG_DIR/goxel-daemon|g" \
        -e "s|/usr/local/var/lib/goxel|$LIB_DIR|g" \
        -e "s|<string>_goxel</string>|<string>$SERVICE_USER</string>|g" \
        "$plist_source" > "$plist_target"
    
    chmod 644 "$plist_target"
    chown root:wheel "$plist_target"
    
    # Load the service
    launchctl load "$plist_target"
    
    log_success "LaunchDaemon installed and loaded"
}

# ============================================================================
# DEPLOYMENT FUNCTIONS
# ============================================================================

deploy_daemon() {
    local binary_path="$1"
    
    log_info "Starting Goxel daemon deployment..."
    log_info "Platform: $PLATFORM ($ARCH)"
    log_info "Install prefix: $INSTALL_PREFIX"
    log_info "Service user: $SERVICE_USER:$SERVICE_GROUP"
    
    # Get platform-specific defaults
    get_platform_defaults
    
    # Check prerequisites
    require_root
    
    # Create service user
    create_service_user "$SERVICE_USER" "$SERVICE_GROUP"
    
    # Create required directories
    create_directories
    
    # Install daemon binary
    install_binary "$binary_path"
    
    # Install configuration
    install_configuration
    
    # Install system service
    install_service
    
    log_success "Daemon deployment completed successfully"
    
    # Show next steps
    show_next_steps
}

undeploy_daemon() {
    log_info "Undeploying Goxel daemon..."
    
    require_root
    get_platform_defaults
    
    # Stop service
    case "$SERVICE_TYPE" in
        systemd)
            if systemctl is-enabled goxel-daemon &>/dev/null; then
                systemctl stop goxel-daemon || true
                systemctl disable goxel-daemon || true
                rm -f /etc/systemd/system/goxel-daemon.service
                systemctl daemon-reload
                log_success "Systemd service removed"
            fi
            ;;
        launchd)
            if [[ -f /Library/LaunchDaemons/com.goxel.daemon.plist ]]; then
                launchctl unload /Library/LaunchDaemons/com.goxel.daemon.plist || true
                rm -f /Library/LaunchDaemons/com.goxel.daemon.plist
                log_success "LaunchDaemon removed"
            fi
            ;;
    esac
    
    # Remove binary
    if [[ -f "$BIN_DIR/goxel-daemon" ]]; then
        rm -f "$BIN_DIR/goxel-daemon"
        log_success "Binary removed"
    fi
    
    # Remove configuration
    if [[ -f "$CONFIG_DIR/daemon_config.json" ]]; then
        rm -f "$CONFIG_DIR/daemon_config.json"
        rmdir "$CONFIG_DIR" 2>/dev/null || true
        log_success "Configuration removed"
    fi
    
    # Remove service user (optional)
    if [[ "${REMOVE_USER:-}" == "1" ]]; then
        case "$PLATFORM" in
            linux)
                if id "$SERVICE_USER" &>/dev/null; then
                    userdel "$SERVICE_USER" || true
                    log_success "Service user removed"
                fi
                ;;
            darwin)
                if dscl . -read "/Users/$SERVICE_USER" &>/dev/null; then
                    dscl . -delete "/Users/$SERVICE_USER" || true
                    log_success "Service user removed"
                fi
                ;;
        esac
    fi
    
    log_success "Daemon undeployed successfully"
}

show_next_steps() {
    echo
    log_info "Next steps:"
    
    case "$SERVICE_TYPE" in
        systemd)
            echo "  1. Start the service: sudo systemctl start goxel-daemon"
            echo "  2. Check status: sudo systemctl status goxel-daemon"
            echo "  3. View logs: sudo journalctl -u goxel-daemon -f"
            echo "  4. Test health: $SCRIPT_DIR/daemon_control.sh health"
            ;;
        launchd)
            echo "  1. Check status: sudo launchctl list | grep goxel"
            echo "  2. View logs: tail -f $LOG_DIR/goxel-daemon.log"
            echo "  3. Test health: $SCRIPT_DIR/daemon_control.sh health"
            ;;
    esac
    
    echo
    log_info "Configuration:"
    echo "  Binary: $BIN_DIR/goxel-daemon"
    echo "  Socket: $RUN_DIR/goxel-daemon.sock"
    echo "  Logs: $LOG_DIR/goxel-daemon.log"
    echo "  Data: $LIB_DIR"
    echo
}

verify_deployment() {
    log_info "Verifying deployment..."
    
    get_platform_defaults
    
    # Check binary
    if [[ -x "$BIN_DIR/goxel-daemon" ]]; then
        log_success "Binary found and executable"
    else
        log_error "Binary not found or not executable: $BIN_DIR/goxel-daemon"
        return 1
    fi
    
    # Check service
    case "$SERVICE_TYPE" in
        systemd)
            if systemctl is-enabled goxel-daemon &>/dev/null; then
                log_success "Systemd service is enabled"
            else
                log_error "Systemd service is not enabled"
                return 1
            fi
            ;;
        launchd)
            if [[ -f /Library/LaunchDaemons/com.goxel.daemon.plist ]]; then
                log_success "LaunchDaemon is installed"
            else
                log_error "LaunchDaemon is not installed"
                return 1
            fi
            ;;
    esac
    
    # Check directories
    for dir in "$LIB_DIR" "$LOG_DIR" "$RUN_DIR" "$CONFIG_DIR"; do
        if [[ -d "$dir" ]]; then
            log_debug "Directory exists: $dir"
        else
            log_error "Directory missing: $dir"
            return 1
        fi
    done
    
    # Test daemon functionality
    if "$BIN_DIR/goxel-daemon" --version >/dev/null 2>&1; then
        log_success "Daemon binary test passed"
    else
        log_error "Daemon binary test failed"
        return 1
    fi
    
    log_success "Deployment verification completed"
}

# ============================================================================
# HELP AND VERSION
# ============================================================================

print_version() {
    echo "$SCRIPT_NAME version $SCRIPT_VERSION"
    echo "Goxel v14.0 Daemon Architecture - Deployment Script"
    echo "Copyright (c) 2025 Guillaume Chereau"
}

print_help() {
    print_version
    echo
    echo "Usage: $SCRIPT_NAME [OPTIONS] COMMAND [BINARY_PATH]"
    echo
    echo "Deploys the Goxel daemon for production use with system service integration."
    echo
    echo "Commands:"
    echo "  deploy BINARY_PATH      Deploy daemon with system service"
    echo "  undeploy               Remove daemon and system service"
    echo "  verify                 Verify deployment integrity"
    echo
    echo "Options:"
    echo "  -h, --help             Show this help message"
    echo "  -v, --version          Show version information"
    echo "  -V, --verbose          Enable verbose output"
    echo "  --prefix PREFIX        Installation prefix (default: $DEFAULT_INSTALL_PREFIX)"
    echo "  --user USER            Service user (default: $DEFAULT_SERVICE_USER)"
    echo "  --group GROUP          Service group (default: $DEFAULT_SERVICE_GROUP)"
    echo "  --remove-user          Remove service user during undeploy"
    echo
    echo "Environment Variables:"
    echo "  INSTALL_PREFIX         Override installation prefix"
    echo "  SERVICE_USER           Override service user"
    echo "  SERVICE_GROUP          Override service group"
    echo "  USER_ID                Service user ID (macOS only)"
    echo "  GROUP_ID               Service group ID (macOS only)"
    echo
    echo "Examples:"
    echo "  $SCRIPT_NAME deploy ./goxel-daemon"
    echo "  $SCRIPT_NAME --prefix /opt/goxel deploy /path/to/goxel-daemon"
    echo "  $SCRIPT_NAME --user myuser --group mygroup deploy ./goxel-daemon"
    echo "  $SCRIPT_NAME verify"
    echo "  $SCRIPT_NAME --remove-user undeploy"
    echo
}

# ============================================================================
# COMMAND LINE PARSING
# ============================================================================

parse_args() {
    INSTALL_PREFIX="$DEFAULT_INSTALL_PREFIX"
    SERVICE_USER="$DEFAULT_SERVICE_USER"
    SERVICE_GROUP="$DEFAULT_SERVICE_GROUP"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_help
                exit 0
                ;;
            -v|--version)
                print_version
                exit 0
                ;;
            -V|--verbose)
                export VERBOSE=1
                ;;
            --prefix)
                INSTALL_PREFIX="$2"
                shift
                ;;
            --user)
                SERVICE_USER="$2"
                shift
                ;;
            --group)
                SERVICE_GROUP="$2"
                shift
                ;;
            --remove-user)
                export REMOVE_USER=1
                ;;
            deploy|undeploy|verify)
                COMMAND="$1"
                ;;
            -*)
                die "Unknown option: $1"
                ;;
            *)
                if [[ "$COMMAND" == "deploy" && -z "${BINARY_PATH:-}" ]]; then
                    BINARY_PATH="$1"
                else
                    die "Unknown argument: $1"
                fi
                ;;
        esac
        shift
    done
}

# ============================================================================
# MAIN FUNCTION
# ============================================================================

main() {
    parse_args "$@"
    
    if [[ -z "${COMMAND:-}" ]]; then
        log_error "No command specified"
        echo
        print_help
        exit 1
    fi
    
    case "$COMMAND" in
        deploy)
            if [[ -z "${BINARY_PATH:-}" ]]; then
                die "Binary path required for deploy command"
            fi
            deploy_daemon "$BINARY_PATH"
            ;;
        undeploy)
            undeploy_daemon
            ;;
        verify)
            verify_deployment
            ;;
        *)
            die "Unknown command: $COMMAND"
            ;;
    esac
}

# Run main function with all arguments
main "$@"