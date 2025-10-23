#!/bin/bash

# MiningSoft Wallet Configuration Script
# This script helps you set up your Monero wallet address

set -e

CONFIG_FILE="config.json"
BACKUP_FILE="config.json.backup"

echo "💰 MiningSoft Wallet Configuration"
echo "=================================="
echo ""

# Check if config file exists
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "❌ Configuration file not found: $CONFIG_FILE"
    exit 1
fi

# Show current wallet address
echo "📋 Current wallet address:"
CURRENT_WALLET=$(grep -o '"pool\.username": "[^"]*"' "$CONFIG_FILE" | cut -d'"' -f4)
echo "   $CURRENT_WALLET"
echo ""

# Get new wallet address
echo "🔑 Enter your Monero wallet address:"
echo "   (Mainnet addresses start with '4' or '8')"
echo "   (Testnet addresses start with '9')"
echo ""
read -p "Wallet address: " NEW_WALLET

# Validate wallet address format
if [[ ! "$NEW_WALLET" =~ ^[49][A-Za-z0-9]{94}$ ]]; then
    echo "❌ Invalid Monero wallet address format!"
    echo "   Mainnet addresses: 95 characters, start with '4' or '8'"
    echo "   Testnet addresses: 95 characters, start with '9'"
    exit 1
fi

echo ""
echo "📝 Updating configuration..."

# Create backup
cp "$CONFIG_FILE" "$BACKUP_FILE"
echo "   ✅ Backup created: $BACKUP_FILE"

# Update primary pool
sed -i '' "s/\"pool\.username\": \"[^\"]*\"/\"pool.username\": \"$NEW_WALLET\"/" "$CONFIG_FILE"

# Update all multi-pool configurations
sed -i '' "s/\"pools\.[^\"]*\.username\": \"[^\"]*\"/&/g" "$CONFIG_FILE"
# This is a simplified approach - in practice, you'd want to update each pool individually

echo "   ✅ Primary pool updated"

# Update multi-pool configurations
for pool in hashvault xmrpool nanopool supportxmr; do
    if grep -q "pools.$pool.username" "$CONFIG_FILE"; then
        sed -i '' "s/\"pools\.$pool\.username\": \"[^\"]*\"/\"pools.$pool.username\": \"$NEW_WALLET\"/" "$CONFIG_FILE"
        echo "   ✅ $pool pool updated"
    fi
done

echo ""
echo "✅ Wallet address updated successfully!"
echo ""
echo "📊 Updated configuration:"
echo "   Primary pool: $(grep -o '"pool\.username": "[^"]*"' "$CONFIG_FILE" | cut -d'"' -f4)"
echo ""
echo "🔄 To apply changes:"
echo "   ./manage_miner.sh restart"
echo ""
echo "📝 Backup saved as: $BACKUP_FILE"
