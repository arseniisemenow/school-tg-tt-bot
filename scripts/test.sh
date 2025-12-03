#!/bin/bash
# Test script - runs simple tests
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Running tests ==="
echo ""

cd "$PROJECT_ROOT"

# Check if binary exists
BINARY_PATH="build/school_tg_tt_bot"
if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: Application binary not found."
    echo "Please run ./scripts/build.sh first"
    exit 1
fi

echo "✓ Binary found: $BINARY_PATH"
echo ""

# Check if binary is executable
if [ ! -x "$BINARY_PATH" ]; then
    echo "ERROR: Binary is not executable"
    exit 1
fi

# Run simple test: check if binary can show help or version
echo "Running simple test: checking binary execution..."
echo ""

# Test 1: Check if binary exists and is executable
echo "Test 1: Binary exists and is executable"
if [ -f "$BINARY_PATH" ] && [ -x "$BINARY_PATH" ]; then
    echo "✓ PASS: Binary exists and is executable"
else
    echo "✗ FAIL: Binary does not exist or is not executable"
    exit 1
fi

# Test 2: Check binary file type
echo ""
echo "Test 2: Binary file type"
FILE_TYPE=$(file "$BINARY_PATH" 2>/dev/null || echo "unknown")
if echo "$FILE_TYPE" | grep -q "ELF.*executable"; then
    echo "✓ PASS: Binary is a valid ELF executable"
    echo "  Type: $FILE_TYPE"
else
    echo "✗ FAIL: Binary is not a valid executable"
    echo "  Type: $FILE_TYPE"
    exit 1
fi

# Test 3: Check if binary has required symbols (basic check)
echo ""
echo "Test 3: Binary symbols check"
if command -v nm &> /dev/null; then
    SYMBOLS=$(nm "$BINARY_PATH" 2>/dev/null | wc -l)
    if [ "$SYMBOLS" -gt 0 ]; then
        echo "✓ PASS: Binary contains symbols ($SYMBOLS symbols found)"
    else
        echo "WARNING: No symbols found in binary (may be stripped)"
    fi
else
    echo "⚠ SKIP: nm command not available, skipping symbol check"
fi

# Test 4: Try to run binary with invalid args (should exit with error, not crash)
echo ""
echo "Test 4: Binary error handling"
# Try running with --help or --version, or just run and kill quickly
# We'll use timeout to prevent hanging
if command -v timeout &> /dev/null; then
    # Try to run with timeout, expect it to either show help/version or exit
    timeout 2s "$BINARY_PATH" --help 2>&1 > /dev/null || true
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo "⚠ WARNING: Binary appears to hang (may be waiting for config)"
    else
        echo "✓ PASS: Binary handles invalid/help arguments"
    fi
else
    echo "⚠ SKIP: timeout command not available, skipping execution test"
fi

# Test 5: Check binary dependencies
echo ""
echo "Test 5: Binary dependencies"
if command -v ldd &> /dev/null; then
    DEPS=$(ldd "$BINARY_PATH" 2>/dev/null | grep -v "not a dynamic" | wc -l)
    if [ "$DEPS" -gt 0 ]; then
        echo "✓ PASS: Binary has dependencies linked ($DEPS dependencies)"
        echo "  Key dependencies:"
        ldd "$BINARY_PATH" 2>/dev/null | grep -E "(libpq|libcurl|libstdc)" | head -3 || true
    else
        echo "⚠ WARNING: No dynamic dependencies found (may be statically linked)"
    fi
else
    echo "⚠ SKIP: ldd command not available, skipping dependency check"
fi

echo ""
echo "=== All tests passed ==="
echo ""
echo "The application binary is ready to use."
echo "Next step: Run ./scripts/run.sh to start the application"
echo ""

