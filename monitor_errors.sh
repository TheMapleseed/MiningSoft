#!/bin/bash

# Error monitoring script for MiningSoft
# Monitors logs for errors and provides real-time alerts

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
LOG_FILE="miner.log"
ERROR_LOG_FILE="error.log"
MONITOR_INTERVAL=5  # seconds
MAX_ERRORS=10
ERROR_WINDOW=300    # seconds

# Error counters
declare -A error_counts
declare -A error_timestamps

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to get current timestamp
get_timestamp() {
    date '+%Y-%m-%d %H:%M:%S'
}

# Function to check if log file exists
check_log_file() {
    if [ ! -f "$LOG_FILE" ]; then
        print_status $YELLOW "Warning: Log file $LOG_FILE not found"
        return 1
    fi
    return 0
}

# Function to monitor errors in real-time
monitor_errors() {
    print_status $BLUE "Starting error monitoring..."
    print_status $BLUE "Monitoring: $LOG_FILE"
    print_status $BLUE "Error log: $ERROR_LOG_FILE"
    print_status $BLUE "Max errors: $MAX_ERRORS per $ERROR_WINDOW seconds"
    echo ""
    
    # Create error log file if it doesn't exist
    touch "$ERROR_LOG_FILE"
    
    # Start monitoring
    while true; do
        if check_log_file; then
            # Check for errors in the last interval
            local current_time=$(date +%s)
            local window_start=$((current_time - ERROR_WINDOW))
            
            # Count errors in the time window
            local error_count=0
            while IFS= read -r line; do
                if [[ "$line" =~ (ERROR|CRITICAL|FATAL) ]]; then
                    error_count=$((error_count + 1))
                    echo "[$(get_timestamp)] $line" >> "$ERROR_LOG_FILE"
                fi
            done < <(tail -n 100 "$LOG_FILE" 2>/dev/null)
            
            # Check if error threshold is exceeded
            if [ "$error_count" -gt "$MAX_ERRORS" ]; then
                print_status $RED "ALERT: Error threshold exceeded!"
                print_status $RED "Errors in last $ERROR_WINDOW seconds: $error_count"
                print_status $RED "Threshold: $MAX_ERRORS"
                echo ""
            elif [ "$error_count" -gt 0 ]; then
                print_status $YELLOW "Warning: $error_count errors detected in last $ERROR_WINDOW seconds"
            else
                print_status $GREEN "✓ No errors detected"
            fi
        fi
        
        sleep "$MONITOR_INTERVAL"
    done
}

# Function to analyze error patterns
analyze_errors() {
    print_status $BLUE "Analyzing error patterns..."
    echo ""
    
    if [ ! -f "$ERROR_LOG_FILE" ]; then
        print_status $YELLOW "No error log file found"
        return
    fi
    
    # Count error types
    print_status $BLUE "Error Type Analysis:"
    grep -o "\[ERROR\]\|\[CRITICAL\]\|\[FATAL\]" "$ERROR_LOG_FILE" 2>/dev/null | sort | uniq -c | sort -nr || true
    echo ""
    
    # Count error categories
    print_status $BLUE "Error Category Analysis:"
    grep -o "\[[A-Z]*\]" "$ERROR_LOG_FILE" 2>/dev/null | sort | uniq -c | sort -nr || true
    echo ""
    
    # Show recent errors
    print_status $BLUE "Recent Errors:"
    tail -10 "$ERROR_LOG_FILE" 2>/dev/null || print_status $YELLOW "No recent errors found"
    echo ""
}

# Function to show error statistics
show_stats() {
    print_status $BLUE "Error Statistics:"
    echo ""
    
    if [ ! -f "$ERROR_LOG_FILE" ]; then
        print_status $YELLOW "No error log file found"
        return
    fi
    
    local total_errors=$(wc -l < "$ERROR_LOG_FILE" 2>/dev/null || echo "0")
    local error_errors=$(grep -c "\[ERROR\]" "$ERROR_LOG_FILE" 2>/dev/null || echo "0")
    local critical_errors=$(grep -c "\[CRITICAL\]" "$ERROR_LOG_FILE" 2>/dev/null || echo "0")
    local fatal_errors=$(grep -c "\[FATAL\]" "$ERROR_LOG_FILE" 2>/dev/null || echo "0")
    
    echo "  Total Errors: $total_errors"
    echo "  ERROR level: $error_errors"
    echo "  CRITICAL level: $critical_errors"
    echo "  FATAL level: $fatal_errors"
    echo ""
    
    if [ "$total_errors" -gt 0 ]; then
        local error_rate=$(echo "scale=2; $total_errors * 3600 / $(($(date +%s) - $(stat -f %m "$ERROR_LOG_FILE" 2>/dev/null || echo $(date +%s))))" | bc 2>/dev/null || echo "0")
        echo "  Error Rate: $error_rate errors/hour"
    fi
    echo ""
}

# Function to clear error logs
clear_logs() {
    print_status $YELLOW "Clearing error logs..."
    > "$ERROR_LOG_FILE"
    print_status $GREEN "✓ Error logs cleared"
}

# Function to show help
show_help() {
    echo "MiningSoft Error Monitor"
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  monitor    - Start real-time error monitoring (default)"
    echo "  analyze    - Analyze error patterns"
    echo "  stats      - Show error statistics"
    echo "  clear      - Clear error logs"
    echo "  help       - Show this help message"
    echo ""
    echo "Configuration:"
    echo "  LOG_FILE: $LOG_FILE"
    echo "  ERROR_LOG_FILE: $ERROR_LOG_FILE"
    echo "  MONITOR_INTERVAL: $MONITOR_INTERVAL seconds"
    echo "  MAX_ERRORS: $MAX_ERRORS per $ERROR_WINDOW seconds"
}

# Main execution
case "${1:-monitor}" in
    monitor)
        monitor_errors
        ;;
    analyze)
        analyze_errors
        ;;
    stats)
        show_stats
        ;;
    clear)
        clear_logs
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
