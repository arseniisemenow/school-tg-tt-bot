#!/bin/bash
# Test script - runs unit and integration tests
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Running build script first ==="
echo ""

./scripts/build.sh

echo "=== Running tests ==="
echo ""

cd "$PROJECT_ROOT"

# Check if test binary exists
TEST_BINARY_PATH="build/school_tg_tt_bot_tests"
if [ ! -f "$TEST_BINARY_PATH" ]; then
    echo "ERROR: Test binary not found."
    echo "Please run ./scripts/build.sh first to build tests"
    exit 1
fi

echo "✓ Test binary found: $TEST_BINARY_PATH"
echo ""

# Check if test binary is executable
if [ ! -x "$TEST_BINARY_PATH" ]; then
    echo "ERROR: Test binary is not executable"
    exit 1
fi

# Check if database is accessible
echo "Checking database connection..."
if [ -z "$DATABASE_URL" ] && [ -z "$POSTGRES_PASSWORD" ]; then
    echo "WARNING: Database environment variables not set."
    echo "Some tests may be skipped."
    echo ""
fi

# Run the tests
echo "Running test suite..."
echo ""

"$TEST_BINARY_PATH" --gtest_color=yes

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "=== All tests passed ==="
    echo ""
else
    echo "=== Some tests failed ==="
    echo ""
    exit $EXIT_CODE
fi

