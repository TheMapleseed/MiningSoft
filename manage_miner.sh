#!/bin/bash

# MiningSoft macOS Miner Management Script
# This script provides easy management of the Monero miner daemon

set -e

DAEMON_NAME="com.miningsoft.monero-miner"
LOG_DIR="/var/log/miningsoft"

show_help() {
    echo "🍎 MiningSoft Miner Management"
    echo "=============================="
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  start     - Start the miner daemon"
    echo "  stop      - Stop the miner daemon"
    echo "  restart   - Restart the miner daemon"
    echo "  status    - Show daemon status"
    echo "  logs      - Show live logs"
    echo "  errors    - Show error logs"
    echo "  install   - Install as system daemon (requires sudo)"
    echo "  uninstall - Remove system daemon (requires sudo)"
    echo "  help      - Show this help"
    echo ""
}

start_miner() {
    echo "🚀 Starting miner daemon..."
    sudo launchctl start "$DAEMON_NAME"
    echo "✅ Miner started"
}

stop_miner() {
    echo "🛑 Stopping miner daemon..."
    sudo launchctl stop "$DAEMON_NAME"
    echo "✅ Miner stopped"
}

restart_miner() {
    echo "🔄 Restarting miner daemon..."
    stop_miner
    sleep 2
    start_miner
}

show_status() {
    echo "📊 Miner Status:"
    echo "================"
    sudo launchctl list | grep "$DAEMON_NAME" || echo "❌ Daemon not found"
    echo ""
    echo "📁 Log files:"
    echo "  Main:   $LOG_DIR/miner.log"
    echo "  Errors: $LOG_DIR/miner_error.log"
}

show_logs() {
    echo "📋 Live Miner Logs (Ctrl+C to exit):"
    echo "====================================="
    sudo tail -f "$LOG_DIR/miner.log"
}

show_errors() {
    echo "❌ Error Logs:"
    echo "=============="
    sudo tail -20 "$LOG_DIR/miner_error.log"
}

install_daemon() {
    if [[ $EUID -ne 0 ]]; then
        echo "❌ Installation requires root privileges"
        echo "   Run: sudo $0 install"
        exit 1
    fi
    
    echo "Installing daemon..."
    # Call the install script
    "$(dirname "$0")/install_daemon.sh"
}

uninstall_daemon() {
    if [[ $EUID -ne 0 ]]; then
        echo "❌ Uninstallation requires root privileges"
        echo "   Run: sudo $0 uninstall"
        exit 1
    fi
    
    echo "Uninstalling daemon..."
    # Call the uninstall script
    "$(dirname "$0")/uninstall_daemon.sh"
}

# Main command handling
case "${1:-help}" in
    start)
        start_miner
        ;;
    stop)
        stop_miner
        ;;
    restart)
        restart_miner
        ;;
    status)
        show_status
        ;;
    logs)
        show_logs
        ;;
    errors)
        show_errors
        ;;
    install)
        install_daemon
        ;;
    uninstall)
        uninstall_daemon
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        echo "❌ Unknown command: $1"
        echo ""
        show_help
        exit 1
        ;;
esac
