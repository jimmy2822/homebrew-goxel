#!/bin/bash

# Goxel TDD Test Runner
# 執行所有 TDD 測試並報告結果

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TDD_DIR="$SCRIPT_DIR/tdd"

echo "🧪 Goxel TDD Test Suite"
echo "======================="
echo ""

# 檢查 TDD 目錄是否存在
if [ ! -d "$TDD_DIR" ]; then
    echo "❌ TDD directory not found: $TDD_DIR"
    exit 1
fi

# 進入 TDD 目錄
cd "$TDD_DIR"

# 清理並編譯
echo "🔨 Building TDD tests..."
make clean > /dev/null 2>&1
make all > /dev/null 2>&1

# 執行測試
echo "🏃 Running TDD tests..."
echo ""

if make run_tests; then
    echo ""
    echo "✅ All TDD tests passed successfully!"
    exit 0
else
    echo ""
    echo "❌ Some TDD tests failed!"
    exit 1
fi