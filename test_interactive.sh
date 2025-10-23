#!/bin/bash

echo "Testing MiningSoft CLI Interactive Mode"
echo "======================================="
echo ""
echo "This will start the CLI in interactive mode."
echo "You should see a prompt 'MiningSoft>' and be able to type commands."
echo "Try commands like: help, wallet, status, exit"
echo ""
echo "Starting CLI in 3 seconds..."
sleep 3

# Start the CLI interactively
./monero-miner --cli
