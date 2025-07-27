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
# GOXEL DAEMON CONTROL SCRIPT
# ============================================================================

set -e

# Script configuration
SCRIPT_NAME="$(basename "$0")"
SCRIPT_VERSION="14.0.0-daemon"

# Default paths (can be overridden by environment variables)
DAEMON_BINARY="${GOXEL_DAEMON_BINARY:-./goxel-daemon}"
PID_FILE="${GOXEL_PID_FILE:-/tmp/goxel-daemon.pid}"
SOCKET_PATH="${GOXEL_SOCKET_PATH:-/tmp/goxel-daemon.sock}"
LOG_FILE="${GOXEL_LOG_FILE:-/tmp/goxel-daemon.log}"
CONFIG_FILE="${GOXEL_CONFIG_FILE:-}"

# Daemon configuration
DAEMON_USER="${GOXEL_DAEMON_USER:-}"
DAEMON_GROUP="${GOXEL_DAEMON_GROUP:-}"
WORKING_DIR="${GOXEL_WORKING_DIR:-/}"
START_TIMEOUT="${GOXEL_START_TIMEOUT:-30}"
STOP_TIMEOUT="${GOXEL_STOP_TIMEOUT:-30}"

# Colors for output
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m' # No Color
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

# Check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if daemon binary exists and is executable
check_daemon_binary() {
    if [[ ! -f "$DAEMON_BINARY" ]]; then
        die "Daemon binary not found: $DAEMON_BINARY"
    fi
    
    if [[ ! -x "$DAEMON_BINARY" ]]; then
        die "Daemon binary is not executable: $DAEMON_BINARY"
    fi
}

# Get daemon PID from PID file
get_daemon_pid() {
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(cat "$PID_FILE" 2>/dev/null) || return 1
        
        # Validate PID format
        if [[ "$pid" =~ ^[0-9]+$ ]]; then
            echo "$pid"
            return 0
        fi
    fi
    
    return 1
}

# Check if daemon process is running
is_daemon_running() {
    local pid
    pid=$(get_daemon_pid) || return 1
    
    if kill -0 "$pid" 2>/dev/null; then
        return 0
    else
        log_warning "Removing stale PID file: $PID_FILE"
        rm -f "$PID_FILE"
        return 1
    fi
}

# Wait for daemon to start
wait_for_start() {
    local timeout=$1
    local count=0
    
    log_info "Waiting for daemon to start (timeout: ${timeout}s)..."
    
    while [[ $count -lt $timeout ]]; do
        if is_daemon_running; then
            return 0
        fi
        
        sleep 1
        ((count++))
        
        if [[ $((count % 5)) -eq 0 ]]; then
            log_debug "Still waiting... (${count}s elapsed)"
        fi
    done
    
    return 1
}

# Wait for daemon to stop
wait_for_stop() {
    local timeout=$1
    local count=0
    
    log_info "Waiting for daemon to stop (timeout: ${timeout}s)..."
    
    while [[ $count -lt $timeout ]]; do
        if ! is_daemon_running; then
            return 0
        fi
        
        sleep 1
        ((count++))
        
        if [[ $((count % 5)) -eq 0 ]]; then
            log_debug "Still waiting... (${count}s elapsed)"
        fi
    done
    
    return 1
}

# Create directory for file if it doesn't exist
ensure_directory() {
    local file_path="$1"
    local dir_path
    dir_path="$(dirname "$file_path")"
    
    if [[ ! -d "$dir_path" ]]; then
        log_debug "Creating directory: $dir_path"
        mkdir -p "$dir_path" || die "Failed to create directory: $dir_path"
    fi
}

# ============================================================================
# DAEMON CONTROL FUNCTIONS
# ============================================================================

daemon_start() {
    log_info "Starting Goxel daemon..."
    
    # Check if daemon is already running
    if is_daemon_running; then
        local pid
        pid=$(get_daemon_pid)
        log_warning "Daemon is already running (PID: $pid)"
        return 0
    fi
    
    # Check daemon binary
    check_daemon_binary
    
    # Ensure directories exist
    ensure_directory "$PID_FILE"
    ensure_directory "$LOG_FILE"
    
    # Build daemon command
    local daemon_cmd=("$DAEMON_BINARY" "--daemonize")
    
    # Add configuration options
    daemon_cmd+=("--pid-file" "$PID_FILE")
    daemon_cmd+=("--socket" "$SOCKET_PATH")
    daemon_cmd+=("--log-file" "$LOG_FILE")
    daemon_cmd+=("--working-dir" "$WORKING_DIR")
    
    if [[ -n "$CONFIG_FILE" ]]; then
        daemon_cmd+=("--config" "$CONFIG_FILE")
    fi
    
    if [[ -n "$DAEMON_USER" ]]; then
        daemon_cmd+=("--user" "$DAEMON_USER")
    fi
    
    if [[ -n "$DAEMON_GROUP" ]]; then
        daemon_cmd+=("--group" "$DAEMON_GROUP")
    fi
    
    if [[ "${VERBOSE:-}" == "1" ]]; then
        daemon_cmd+=("--verbose")
    fi
    
    log_debug "Executing: ${daemon_cmd[*]}"
    
    # Start the daemon
    "${daemon_cmd[@]}" || die "Failed to start daemon"
    
    # Wait for daemon to start
    if wait_for_start "$START_TIMEOUT"; then
        local pid
        pid=$(get_daemon_pid)
        log_success "Daemon started successfully (PID: $pid)"
        
        # Display status information
        daemon_status
    else
        die "Daemon failed to start within ${START_TIMEOUT} seconds"
    fi
}

daemon_stop() {
    log_info "Stopping Goxel daemon..."
    
    # Check if daemon is running
    if ! is_daemon_running; then
        log_warning "Daemon is not running"
        return 0
    fi
    
    local pid
    pid=$(get_daemon_pid)
    log_info "Sending SIGTERM to daemon (PID: $pid)..."
    
    # Send SIGTERM for graceful shutdown
    if ! kill -TERM "$pid" 2>/dev/null; then
        log_warning "Failed to send SIGTERM, daemon may have already stopped"
        return 0
    fi
    
    # Wait for graceful shutdown
    if wait_for_stop "$STOP_TIMEOUT"; then
        log_success "Daemon stopped gracefully"
    else
        log_warning "Daemon did not stop gracefully, sending SIGKILL..."
        
        # Force kill if still running
        if is_daemon_running; then
            kill -KILL "$pid" 2>/dev/null || true
            sleep 2
            
            if is_daemon_running; then
                die "Failed to stop daemon"
            else
                log_success "Daemon force-stopped"
            fi
        fi
    fi
    
    # Clean up PID file
    rm -f "$PID_FILE"
}

daemon_restart() {
    log_info "Restarting Goxel daemon..."
    daemon_stop
    sleep 2
    daemon_start
}

daemon_status() {
    if is_daemon_running; then
        local pid
        pid=$(get_daemon_pid)
        log_success "Daemon is running (PID: $pid)"
        
        # Show additional information
        echo "  PID file: $PID_FILE"
        echo "  Socket: $SOCKET_PATH"
        echo "  Log file: $LOG_FILE"
        
        # Show process information if ps is available
        if command_exists ps; then
            echo "  Process info:"
            ps -p "$pid" -o pid,ppid,user,time,cmd 2>/dev/null | sed 's/^/    /' || true
        fi
        
        # Show socket file information
        if [[ -S "$SOCKET_PATH" ]]; then
            echo "  Socket file exists: $SOCKET_PATH"
        else
            log_warning "Socket file not found: $SOCKET_PATH"
        fi
        
        return 0
    else
        log_error "Daemon is not running"
        return 1
    fi
}

daemon_reload() {
    log_info "Reloading Goxel daemon configuration..."
    
    # Check if daemon is running
    if ! is_daemon_running; then
        die "Daemon is not running"
    fi
    
    local pid
    pid=$(get_daemon_pid)
    log_info "Sending SIGHUP to daemon (PID: $pid)..."
    
    # Send SIGHUP for configuration reload
    if kill -HUP "$pid" 2>/dev/null; then
        log_success "Configuration reload signal sent"
    else
        die "Failed to send reload signal to daemon"
    fi
}

daemon_health() {
    log_info "Checking Goxel daemon health..."
    
    # Check if daemon is running
    if ! is_daemon_running; then
        log_error "Daemon is not running"
        return 1
    fi
    
    local pid
    pid=$(get_daemon_pid)
    
    # Check socket connectivity
    log_info "Testing socket connectivity..."
    if command_exists nc; then
        if timeout 5 nc -U "$SOCKET_PATH" < /dev/null; then
            log_success "Socket is responding"
        else
            log_error "Socket is not responding"
            return 1
        fi
    elif command_exists socat; then
        if timeout 5 socat - "UNIX-CONNECT:$SOCKET_PATH" < /dev/null; then
            log_success "Socket is responding"
        else
            log_error "Socket is not responding"
            return 1
        fi
    else
        log_warning "Cannot test socket (nc or socat not available)"
    fi
    
    # Check process health
    if [[ -f "/proc/$pid/status" ]]; then
        local status
        status=$(grep "State:" "/proc/$pid/status" 2>/dev/null || echo "State: unknown")
        log_info "Process status: $status"
    fi
    
    # Check log file for recent errors
    if [[ -f "$LOG_FILE" && -r "$LOG_FILE" ]]; then
        local recent_errors
        recent_errors=$(tail -100 "$LOG_FILE" | grep -i "error\|fatal\|critical" | wc -l)
        if [[ $recent_errors -gt 0 ]]; then
            log_warning "Found $recent_errors recent error(s) in log file"
            log_info "Recent errors:"
            tail -100 "$LOG_FILE" | grep -i "error\|fatal\|critical" | tail -5 | sed 's/^/  /'
        else
            log_success "No recent errors in log file"
        fi
    fi
    
    log_success "Daemon health check completed"
    return 0
}

daemon_test() {
    log_info "Testing Goxel daemon functionality..."
    
    check_daemon_binary
    
    # Run daemon lifecycle tests
    log_info "Running lifecycle tests..."
    if "$DAEMON_BINARY" --test-lifecycle; then
        log_success "Lifecycle tests passed"
    else
        die "Lifecycle tests failed"
    fi
    
    # Run signal handling tests
    log_info "Running signal handling tests..."
    if "$DAEMON_BINARY" --test-signals; then
        log_success "Signal handling tests passed"
    else
        die "Signal handling tests failed"
    fi
    
    # Run health check if daemon is running
    if is_daemon_running; then
        log_info "Running health check on running daemon..."
        daemon_health
    fi
    
    log_success "All tests passed"
}

# ============================================================================
# SYSTEM INTEGRATION
# ============================================================================

install_systemd_service() {
    log_info "Installing systemd service..."
    
    local service_file="/etc/systemd/system/goxel-daemon.service"
    
    # Check if we have permission to write to systemd directory
    if [[ ! -w "$(dirname "$service_file")" ]]; then
        die "Permission denied: Cannot write to $(dirname "$service_file"). Run as root or use sudo."
    fi
    
    # Create systemd service file
    cat > "$service_file" << EOF
[Unit]
Description=Goxel 3D Voxel Editor Daemon
Documentation=https://goxel.xyz
After=network.target

[Service]
Type=forking
User=${DAEMON_USER:-$(whoami)}
Group=${DAEMON_GROUP:-$(id -gn)}
WorkingDirectory=$WORKING_DIR
ExecStart=$DAEMON_BINARY --daemonize --pid-file $PID_FILE --socket $SOCKET_PATH --log-file $LOG_FILE
ExecReload=/bin/kill -HUP \$MAINPID
ExecStop=/bin/kill -TERM \$MAINPID
PIDFile=$PID_FILE
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
    
    log_success "Systemd service installed: $service_file"
    
    # Reload systemd
    systemctl daemon-reload
    log_info "Systemd configuration reloaded"
    
    log_info "To enable the service at boot:"
    log_info "  sudo systemctl enable goxel-daemon"
    log_info "To start the service:"
    log_info "  sudo systemctl start goxel-daemon"
}

uninstall_systemd_service() {
    log_info "Uninstalling systemd service..."
    
    local service_file="/etc/systemd/system/goxel-daemon.service"
    
    # Stop and disable service if it exists
    if [[ -f "$service_file" ]]; then
        systemctl stop goxel-daemon 2>/dev/null || true
        systemctl disable goxel-daemon 2>/dev/null || true
        rm -f "$service_file"
        systemctl daemon-reload
        log_success "Systemd service uninstalled"
    else
        log_warning "Systemd service not found"
    fi
}

# ============================================================================
# HELP AND VERSION
# ============================================================================

print_version() {
    echo "$SCRIPT_NAME version $SCRIPT_VERSION"
    echo "Goxel v14.0 Daemon Architecture - Control Script"
    echo "Copyright (c) 2025 Guillaume Chereau"
}

print_help() {
    print_version
    echo
    echo "Usage: $SCRIPT_NAME [OPTIONS] COMMAND"
    echo
    echo "Controls the Goxel daemon for headless 3D voxel editing operations."
    echo
    echo "Commands:"
    echo "  start                   Start the daemon"
    echo "  stop                    Stop the daemon"
    echo "  restart                 Restart the daemon"
    echo "  status                  Show daemon status"
    echo "  health                  Check daemon health"
    echo "  reload                  Reload daemon configuration"
    echo "  test                    Run daemon functionality tests"
    echo
    echo "System Integration:"
    echo "  install-systemd         Install systemd service (requires root)"
    echo "  uninstall-systemd       Uninstall systemd service (requires root)"
    echo
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -v, --version           Show version information"
    echo "  -V, --verbose           Enable verbose output"
    echo "  -b, --binary PATH       Daemon binary path (default: $DAEMON_BINARY)"
    echo "  -p, --pid-file PATH     PID file path (default: $PID_FILE)"
    echo "  -s, --socket PATH       Socket path (default: $SOCKET_PATH)"
    echo "  -l, --log-file PATH     Log file path (default: $LOG_FILE)"
    echo "  -c, --config FILE       Configuration file path"
    echo "  -u, --user USER         Run daemon as user"
    echo "  -g, --group GROUP       Run daemon as group"
    echo "  -w, --working-dir DIR   Working directory (default: $WORKING_DIR)"
    echo
    echo "Environment Variables:"
    echo "  GOXEL_DAEMON_BINARY     Override daemon binary path"
    echo "  GOXEL_PID_FILE          Override PID file path"
    echo "  GOXEL_SOCKET_PATH       Override socket path"
    echo "  GOXEL_LOG_FILE          Override log file path"
    echo "  GOXEL_CONFIG_FILE       Override config file path"
    echo "  GOXEL_DAEMON_USER       Override daemon user"
    echo "  GOXEL_DAEMON_GROUP      Override daemon group"
    echo "  GOXEL_WORKING_DIR       Override working directory"
    echo "  GOXEL_START_TIMEOUT     Override start timeout (default: $START_TIMEOUT)"
    echo "  GOXEL_STOP_TIMEOUT      Override stop timeout (default: $STOP_TIMEOUT)"
    echo
    echo "Examples:"
    echo "  $SCRIPT_NAME start                          # Start daemon with defaults"
    echo "  $SCRIPT_NAME --verbose status               # Show verbose status"
    echo "  $SCRIPT_NAME --binary ./my-daemon start     # Use custom daemon binary"
    echo "  sudo $SCRIPT_NAME install-systemd           # Install systemd service"
    echo
}

# ============================================================================
# COMMAND LINE PARSING
# ============================================================================

parse_args() {
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
            -b|--binary)
                DAEMON_BINARY="$2"
                shift
                ;;
            -p|--pid-file)
                PID_FILE="$2"
                shift
                ;;
            -s|--socket)
                SOCKET_PATH="$2"
                shift
                ;;
            -l|--log-file)
                LOG_FILE="$2"
                shift
                ;;
            -c|--config)
                CONFIG_FILE="$2"
                shift
                ;;
            -u|--user)
                DAEMON_USER="$2"
                shift
                ;;
            -g|--group)
                DAEMON_GROUP="$2"
                shift
                ;;
            -w|--working-dir)
                WORKING_DIR="$2"
                shift
                ;;
            start|stop|restart|status|health|reload|test|install-systemd|uninstall-systemd)
                COMMAND="$1"
                ;;
            -*)
                die "Unknown option: $1"
                ;;
            *)
                if [[ -z "${COMMAND:-}" ]]; then
                    COMMAND="$1"
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
    # Parse command line arguments
    parse_args "$@"
    
    # Check if command was provided
    if [[ -z "${COMMAND:-}" ]]; then
        log_error "No command specified"
        echo
        print_help
        exit 1
    fi
    
    log_debug "Daemon binary: $DAEMON_BINARY"
    log_debug "PID file: $PID_FILE"
    log_debug "Socket path: $SOCKET_PATH"
    log_debug "Log file: $LOG_FILE"
    
    # Execute command
    case "$COMMAND" in
        start)
            daemon_start
            ;;
        stop)
            daemon_stop
            ;;
        restart)
            daemon_restart
            ;;
        status)
            daemon_status
            ;;
        health)
            daemon_health
            ;;
        reload)
            daemon_reload
            ;;
        test)
            daemon_test
            ;;
        install-systemd)
            install_systemd_service
            ;;
        uninstall-systemd)
            uninstall_systemd_service
            ;;
        *)
            die "Unknown command: $COMMAND"
            ;;
    esac
}

# Run main function with all arguments
main "$@"