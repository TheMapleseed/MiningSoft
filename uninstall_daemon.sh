#!/bin/bash

# MiningSoft macOS Daemon Uninstallation Script
# This script removes the Monero miner daemon from the system

set -e

echo "🍎 MiningSoft macOS Daemon Uninstaller"
echo "======================================"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ This script must be run as root (use sudo)"
   exit 1
fi

DAEMON_DIR="/Library/LaunchDaemons"
LOG_DIR="/var/log/miningsoft"

echo "🛑 Stopping daemon..."

# Stop and unload the daemon
launchctl stop com.miningsoft.monero-miner 2>/dev/null || true
launchctl unload "$DAEMON_DIR/com.miningsoft.monero-miner.plist" 2>/dev/null || true

echo "🗑️  Removing daemon files..."

# Remove plist file
rm -f "$DAEMON_DIR/com.miningsoft.monero-miner.plist"

echo "📁 Cleaning up logs..."
echo "   Log directory: $LOG_DIR"
echo "   (Logs are preserved for debugging)"

echo "✅ Daemon uninstalled successfully!"
echo ""
echo "📝 Note: Log files are preserved in $LOG_DIR"
echo "   Remove them manually if no longer needed."
