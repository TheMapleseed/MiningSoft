#!/bin/bash

# MiningSoft macOS Daemon Installation Script
# This script installs the Monero miner as a system daemon

set -e

echo "🍎 MiningSoft macOS Daemon Installer"
echo "====================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ This script must be run as root (use sudo)"
   exit 1
fi

# Get the current directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLIST_FILE="$SCRIPT_DIR/com.miningsoft.monero-miner.plist"
DAEMON_DIR="/Library/LaunchDaemons"
LOG_DIR="/var/log/miningsoft"

echo "📁 Setting up directories..."

# Create log directory
mkdir -p "$LOG_DIR"
chown root:wheel "$LOG_DIR"
chmod 755 "$LOG_DIR"

echo "📋 Installing LaunchDaemon..."

# Copy plist file to system directory
cp "$PLIST_FILE" "$DAEMON_DIR/"
chown root:wheel "$DAEMON_DIR/com.miningsoft.monero-miner.plist"
chmod 644 "$DAEMON_DIR/com.miningsoft.monero-miner.plist"

echo "🔧 Loading daemon..."

# Unload if already loaded
launchctl unload "$DAEMON_DIR/com.miningsoft.monero-miner.plist" 2>/dev/null || true

# Load the daemon
launchctl load "$DAEMON_DIR/com.miningsoft.monero-miner.plist"

echo "✅ Daemon installed successfully!"
echo ""
echo "📊 Management commands:"
echo "  Start:   sudo launchctl start com.miningsoft.monero-miner"
echo "  Stop:    sudo launchctl stop com.miningsoft.monero-miner"
echo "  Status:  sudo launchctl list | grep miningsoft"
echo "  Logs:    tail -f /var/log/miningsoft/miner.log"
echo "  Errors:  tail -f /var/log/miningsoft/miner_error.log"
echo ""
echo "🔄 The miner will now start automatically at boot and stop gracefully at shutdown."
echo "📝 Check the logs to verify it's running properly."
