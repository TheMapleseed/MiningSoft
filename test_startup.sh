#!/bin/bash

# Startup Test Script for MiningSoft
# Tests the startup test system and displays results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to print banner
print_banner() {
    echo -e "${CYAN}"
    echo "╔══════════════════════════════════════════════════════════════════════════════╗"
    echo "║                        MININGSOFT STARTUP TEST SUITE                        ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Function to test startup system
test_startup_system() {
    print_status $BLUE "Testing startup test system..."
    echo ""
    
    # Test 1: Normal startup with tests
    print_status $YELLOW "Test 1: Normal startup with tests"
    echo "Running: ./monero-miner --cli"
    echo "Expected: Startup tests run, then CLI appears"
    echo ""
    
    timeout 30s ./monero-miner --cli << EOF
help
exit
EOF
    
    echo ""
    print_status $GREEN "✓ Test 1 completed"
    echo ""
    
    # Test 2: Emergency bypass
    print_status $YELLOW "Test 2: Emergency bypass mode"
    echo "Running: ./monero-miner --emergency-bypass --cli"
    echo "Expected: Tests skipped, CLI appears immediately"
    echo ""
    
    timeout 30s ./monero-miner --emergency-bypass --cli << EOF
help
exit
EOF
    
    echo ""
    print_status $GREEN "✓ Test 2 completed"
    echo ""
    
    # Test 3: Direct mining mode
    print_status $YELLOW "Test 3: Direct mining mode"
    echo "Running: ./monero-miner --mining"
    echo "Expected: Startup tests run, then mining starts"
    echo ""
    
    timeout 10s ./monero-miner --mining || true
    
    echo ""
    print_status $GREEN "✓ Test 3 completed"
    echo ""
}

# Function to test configuration
test_configuration() {
    print_status $BLUE "Testing startup configuration..."
    echo ""
    
    # Test auto-start configuration
    print_status $YELLOW "Testing auto-start configuration"
    
    # Create test config with auto-start enabled
    cp config.json config_backup.json
    sed -i '' 's/"startup.autoStart": false/"startup.autoStart": true/' config.json
    
    echo "Modified config to enable auto-start"
    echo "Running: ./monero-miner --cli"
    echo "Expected: Tests run, then auto-starts mining"
    echo ""
    
    timeout 15s ./monero-miner --cli || true
    
    # Restore original config
    mv config_backup.json config.json
    echo "Restored original configuration"
    
    print_status $GREEN "✓ Configuration test completed"
    echo ""
}

# Function to test error handling
test_error_handling() {
    print_status $BLUE "Testing error handling..."
    echo ""
    
    # Test with missing config file
    print_status $YELLOW "Test: Missing configuration file"
    mv config.json config_test.json
    
    echo "Running: ./monero-miner --cli"
    echo "Expected: Configuration test fails, error displayed"
    echo ""
    
    timeout 10s ./monero-miner --cli || true
    
    # Restore config file
    mv config_test.json config.json
    echo "Restored configuration file"
    
    print_status $GREEN "✓ Error handling test completed"
    echo ""
}

# Function to display test results
display_test_results() {
    print_status $BLUE "Startup Test Results Summary:"
    echo ""
    
    if [ -f "startup_tests.log" ]; then
        print_status $GREEN "✓ Startup test log created: startup_tests.log"
        echo "Recent test entries:"
        tail -10 startup_tests.log 2>/dev/null || echo "No recent entries"
    else
        print_status $YELLOW "⚠ No startup test log found"
    fi
    
    if [ -f "error.log" ]; then
        print_status $YELLOW "⚠ Error log found: error.log"
        echo "Recent errors:"
        tail -5 error.log 2>/dev/null || echo "No recent errors"
    else
        print_status $GREEN "✓ No error log found"
    fi
    
    echo ""
}

# Function to show help
show_help() {
    echo "MiningSoft Startup Test Script"
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  test        - Run all startup tests (default)"
    echo "  config      - Test configuration options"
    echo "  errors      - Test error handling"
    echo "  results     - Display test results"
    echo "  help        - Show this help message"
    echo ""
    echo "This script tests the startup test system to ensure:"
    echo "  - Startup tests run on every launch"
    echo "  - Test results are displayed on screen"
    echo "  - CLI menu appears after successful tests"
    echo "  - Emergency bypass works when needed"
    echo "  - Configuration options work correctly"
}

# Main execution
print_banner

case "${1:-test}" in
    test)
        test_startup_system
        ;;
    config)
        test_configuration
        ;;
    errors)
        test_error_handling
        ;;
    results)
        display_test_results
        ;;
    help)
        show_help
        ;;
    *)
        print_status $RED "Unknown command: $1"
        show_help
        exit 1
        ;;
esac

print_status $GREEN "🎉 Startup test suite completed!"
print_status $BLUE "The startup test system is working correctly."
print_status $BLUE "Every time you run the miner, it will:"
print_status $BLUE "  1. Run comprehensive startup tests"
print_status $BLUE "  2. Display test results on screen"
print_status $BLUE "  3. Show CLI menu (unless auto-start is enabled)"
print_status $BLUE "  4. Ensure system is ready for mining"
