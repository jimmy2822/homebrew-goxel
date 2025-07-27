#!/bin/bash
#
# Goxel v14.0 Performance Regression Detection Script
#
# This script provides automated performance regression detection for use in
# CI/CD pipelines and development workflows. It compares current performance
# metrics against established baselines and alerts on significant degradations.
#
# Usage: ./performance_regression_check.sh [options]
#   --baseline FILE      Baseline performance data file (JSON)
#   --current FILE       Current performance data file (JSON)
#   --threshold PERCENT  Regression threshold percentage (default: 10)
#   --output FILE        Output file for regression report
#   --ci                 CI mode - machine readable output
#   --strict             Strict mode - fail on any regression
#   --help               Show help message

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEFAULT_THRESHOLD=10
STRICT_MODE=false
CI_MODE=false

# Colors for output (disabled in CI mode)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Global variables
BASELINE_FILE=""
CURRENT_FILE=""
OUTPUT_FILE=""
THRESHOLD=$DEFAULT_THRESHOLD
REGRESSIONS_FOUND=0
TOTAL_COMPARISONS=0

# Logging functions
log_info() {
    if [ "$CI_MODE" = true ]; then
        echo "INFO: $1"
    else
        echo -e "${BLUE}[INFO]${NC} $1"
    fi
}

log_success() {
    if [ "$CI_MODE" = true ]; then
        echo "SUCCESS: $1"
    else
        echo -e "${GREEN}[SUCCESS]${NC} $1"
    fi
}

log_warning() {
    if [ "$CI_MODE" = true ]; then
        echo "WARNING: $1"
    else
        echo -e "${YELLOW}[WARNING]${NC} $1"
    fi
}

log_error() {
    if [ "$CI_MODE" = true ]; then
        echo "ERROR: $1" >&2
    else
        echo -e "${RED}[ERROR]${NC} $1" >&2
    fi
}

log_regression() {
    if [ "$CI_MODE" = true ]; then
        echo "REGRESSION: $1"
    else
        echo -e "${RED}[REGRESSION]${NC} $1"
    fi
}

# Help function
show_help() {
    cat << EOF
Goxel v14.0 Performance Regression Detection

USAGE:
    $0 --baseline BASELINE_FILE --current CURRENT_FILE [OPTIONS]

REQUIRED ARGUMENTS:
    --baseline FILE          Baseline performance data (JSON format)
    --current FILE           Current performance data (JSON format)

OPTIONS:
    --threshold PERCENT      Regression threshold percentage (default: 10)
    --output FILE            Output regression report file
    --ci                     CI mode - machine readable output
    --strict                 Strict mode - fail on any regression > 0%
    --help                   Show this help message

EXAMPLES:
    # Basic regression check
    $0 --baseline baseline.json --current current.json
    
    # Strict mode with 5% threshold
    $0 --baseline baseline.json --current current.json --threshold 5 --strict
    
    # CI mode with report output
    $0 --baseline baseline.json --current current.json --ci --output regression_report.json

THRESHOLD INTERPRETATION:
    The threshold represents the acceptable performance degradation percentage.
    For metrics where lower is better (latency, memory):
      - Regression if current > baseline * (1 + threshold/100)
    For metrics where higher is better (throughput, improvement ratio):
      - Regression if current < baseline * (1 - threshold/100)

EXIT CODES:
    0    No regressions detected
    1    Regressions detected
    2    Error in execution

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --baseline)
                BASELINE_FILE="$2"
                shift 2
                ;;
            --current)
                CURRENT_FILE="$2"
                shift 2
                ;;
            --threshold)
                THRESHOLD="$2"
                shift 2
                ;;
            --output)
                OUTPUT_FILE="$2"
                shift 2
                ;;
            --ci)
                CI_MODE=true
                shift
                ;;
            --strict)
                STRICT_MODE=true
                THRESHOLD=0
                shift
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 2
                ;;
        esac
    done
    
    # Validate required arguments
    if [ -z "$BASELINE_FILE" ] || [ -z "$CURRENT_FILE" ]; then
        log_error "Both --baseline and --current arguments are required"
        show_help
        exit 2
    fi
    
    # Validate files exist
    if [ ! -f "$BASELINE_FILE" ]; then
        log_error "Baseline file not found: $BASELINE_FILE"
        exit 2
    fi
    
    if [ ! -f "$CURRENT_FILE" ]; then
        log_error "Current file not found: $CURRENT_FILE"
        exit 2
    fi
    
    # Validate threshold
    if ! [[ "$THRESHOLD" =~ ^[0-9]+\.?[0-9]*$ ]]; then
        log_error "Invalid threshold value: $THRESHOLD"
        exit 2
    fi
}

# Extract metric value from JSON data
extract_metric() {
    local json_file="$1"
    local test_name="$2"
    local metric_name="$3"
    
    python3 << EOF
import json
import sys

try:
    with open('$json_file', 'r') as f:
        data = json.load(f)
    
    for result in data.get('results', []):
        if result.get('name') == '$test_name':
            metrics = result.get('metrics', {})
            if '$metric_name' in metrics:
                print(metrics['$metric_name']['value'])
                sys.exit(0)
    
    # Metric not found
    sys.exit(1)
    
except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(1)
EOF
}

# Check if metric shows regression
check_metric_regression() {
    local test_name="$1"
    local metric_name="$2"
    local baseline_value="$3"
    local current_value="$4"
    local is_lower_better="$5"  # true/false
    
    local regression_detected=false
    local change_percent
    local threshold_value
    
    # Calculate percentage change
    change_percent=$(python3 -c "print(abs(($current_value - $baseline_value) / $baseline_value * 100))")
    
    # Calculate threshold value based on metric type
    if [ "$is_lower_better" = "true" ]; then
        # Lower is better (latency, memory) - regression if current > baseline * (1 + threshold/100)
        threshold_value=$(python3 -c "print($baseline_value * (1 + $THRESHOLD / 100))")
        if (( $(python3 -c "print($current_value > $threshold_value)") )); then
            regression_detected=true
        fi
    else
        # Higher is better (throughput, improvement) - regression if current < baseline * (1 - threshold/100)
        threshold_value=$(python3 -c "print($baseline_value * (1 - $THRESHOLD / 100))")
        if (( $(python3 -c "print($current_value < $threshold_value)") )); then
            regression_detected=true
        fi
    fi
    
    TOTAL_COMPARISONS=$((TOTAL_COMPARISONS + 1))
    
    if [ "$regression_detected" = true ]; then
        REGRESSIONS_FOUND=$((REGRESSIONS_FOUND + 1))
        log_regression "$test_name.$metric_name: $baseline_value → $current_value (${change_percent}% change)"
        
        # Add to output file if specified
        if [ -n "$OUTPUT_FILE" ]; then
            cat >> "$OUTPUT_FILE" << EOF
{
  "test": "$test_name",
  "metric": "$metric_name",
  "baseline_value": $baseline_value,
  "current_value": $current_value,
  "change_percent": $change_percent,
  "threshold_percent": $THRESHOLD,
  "regression_detected": true,
  "lower_is_better": $is_lower_better
},
EOF
        fi
        
        return 0  # Regression found
    else
        log_info "$test_name.$metric_name: $baseline_value → $current_value (${change_percent}% change) - OK"
        
        # Add to output file if specified
        if [ -n "$OUTPUT_FILE" ]; then
            cat >> "$OUTPUT_FILE" << EOF
{
  "test": "$test_name",
  "metric": "$metric_name",
  "baseline_value": $baseline_value,
  "current_value": $current_value,
  "change_percent": $change_percent,
  "threshold_percent": $THRESHOLD,
  "regression_detected": false,
  "lower_is_better": $is_lower_better
},
EOF
        fi
        
        return 1  # No regression
    fi
}

# Initialize output file
initialize_output_file() {
    if [ -n "$OUTPUT_FILE" ]; then
        cat > "$OUTPUT_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "baseline_file": "$BASELINE_FILE",
  "current_file": "$CURRENT_FILE",
  "threshold_percent": $THRESHOLD,
  "strict_mode": $STRICT_MODE,
  "comparisons": [
EOF
    fi
}

# Finalize output file
finalize_output_file() {
    if [ -n "$OUTPUT_FILE" ]; then
        # Remove trailing comma from last entry
        sed -i '$ s/,$//' "$OUTPUT_FILE"
        
        cat >> "$OUTPUT_FILE" << EOF
  ],
  "summary": {
    "total_comparisons": $TOTAL_COMPARISONS,
    "regressions_found": $REGRESSIONS_FOUND,
    "regression_rate_percent": $(python3 -c "print($REGRESSIONS_FOUND / max($TOTAL_COMPARISONS, 1) * 100)")
  }
}
EOF
        log_info "Regression report written to: $OUTPUT_FILE"
    fi
}

# Main regression checking function
check_performance_regressions() {
    log_info "Checking for performance regressions..."
    log_info "Baseline: $BASELINE_FILE"
    log_info "Current: $CURRENT_FILE"
    log_info "Threshold: ${THRESHOLD}%"
    
    initialize_output_file
    
    # Define metrics to check (test_name:metric_name:lower_is_better)
    local metrics_to_check=(
        "latency_benchmark:avg_latency:true"
        "throughput_benchmark:avg_throughput:false"
        "throughput_benchmark:max_throughput:false"
        "memory_profiling:peak_memory:true"
        "memory_profiling:final_memory:true"
        "performance_comparison:improvement_ratio:false"
        "performance_comparison:best_improvement:false"
    )
    
    # Check each metric
    for metric_def in "${metrics_to_check[@]}"; do
        IFS=':' read -r test_name metric_name lower_is_better <<< "$metric_def"
        
        # Extract baseline value
        baseline_value=$(extract_metric "$BASELINE_FILE" "$test_name" "$metric_name" 2>/dev/null)
        if [ $? -ne 0 ]; then
            log_warning "Baseline metric not found: $test_name.$metric_name"
            continue
        fi
        
        # Extract current value
        current_value=$(extract_metric "$CURRENT_FILE" "$test_name" "$metric_name" 2>/dev/null)
        if [ $? -ne 0 ]; then
            log_warning "Current metric not found: $test_name.$metric_name"
            continue
        fi
        
        # Check for regression
        check_metric_regression "$test_name" "$metric_name" "$baseline_value" "$current_value" "$lower_is_better"
    done
    
    finalize_output_file
}

# Generate summary report
generate_summary() {
    local regression_rate=0
    if [ $TOTAL_COMPARISONS -gt 0 ]; then
        regression_rate=$(python3 -c "print(int($REGRESSIONS_FOUND / $TOTAL_COMPARISONS * 100))")
    fi
    
    if [ "$CI_MODE" = true ]; then
        echo "REGRESSION_CHECK_COMPLETE"
        echo "TOTAL_COMPARISONS: $TOTAL_COMPARISONS"
        echo "REGRESSIONS_FOUND: $REGRESSIONS_FOUND"
        echo "REGRESSION_RATE: ${regression_rate}%"
        echo "THRESHOLD: ${THRESHOLD}%"
        echo "STRICT_MODE: $STRICT_MODE"
    else
        echo ""
        echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${BLUE}║                  REGRESSION CHECK SUMMARY                    ║${NC}"
        echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
        echo ""
        echo -e "  Total Comparisons:    ${BLUE}$TOTAL_COMPARISONS${NC}"
        echo -e "  Regressions Found:    $(if [ $REGRESSIONS_FOUND -gt 0 ]; then echo -e "${RED}$REGRESSIONS_FOUND${NC}"; else echo -e "${GREEN}$REGRESSIONS_FOUND${NC}"; fi)"
        echo -e "  Regression Rate:      $(if [ $regression_rate -gt 0 ]; then echo -e "${RED}${regression_rate}%${NC}"; else echo -e "${GREEN}${regression_rate}%${NC}"; fi)"
        echo -e "  Threshold:            ${BLUE}${THRESHOLD}%${NC}"
        echo -e "  Mode:                 $(if [ "$STRICT_MODE" = true ]; then echo -e "${YELLOW}STRICT${NC}"; else echo -e "${BLUE}NORMAL${NC}"; fi)"
        echo ""
        
        if [ $REGRESSIONS_FOUND -eq 0 ]; then
            echo -e "  ${GREEN}✅ No performance regressions detected!${NC}"
        else
            echo -e "  ${RED}⚠️  Performance regressions detected. Review the output above.${NC}"
        fi
        echo ""
    fi
}

# Main execution
main() {
    # Parse arguments
    parse_args "$@"
    
    # Print header
    if [ "$CI_MODE" = false ]; then
        echo -e "${BLUE}"
        echo "╔══════════════════════════════════════════════════════════════╗"
        echo "║            Goxel v14.0 Performance Regression Check         ║"
        echo "╚══════════════════════════════════════════════════════════════╝"
        echo -e "${NC}"
    else
        echo "REGRESSION_CHECK_START: $(date -Iseconds)"
    fi
    
    # Check for regressions
    check_performance_regressions
    
    # Generate summary
    generate_summary
    
    # Determine exit code
    if [ $REGRESSIONS_FOUND -gt 0 ]; then
        if [ "$STRICT_MODE" = true ]; then
            log_error "Strict mode: Any regression is considered a failure"
            exit 1
        elif [ $REGRESSIONS_FOUND -gt $(( TOTAL_COMPARISONS / 4 )) ]; then
            log_error "Too many regressions detected (> 25% of metrics)"
            exit 1
        else
            log_warning "Some regressions detected but within acceptable limits"
            exit 0
        fi
    else
        log_success "No performance regressions detected"
        exit 0
    fi
}

# Run main function with all arguments
main "$@"