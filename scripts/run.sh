#!/bin/bash
# Run script - starts the application
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Starting application ==="
echo ""

cd "$PROJECT_ROOT"

# Check if binary exists
BINARY_PATH="build/school_tg_tt_bot"
if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: Application binary not found."
    echo "Please run ./scripts/build.sh first"
    exit 1
fi

if [ ! -x "$BINARY_PATH" ]; then
    echo "ERROR: Binary is not executable"
    exit 1
fi

echo "✓ Binary found: $BINARY_PATH"
echo ""

# Check database connectivity
echo "Checking database connectivity..."
if command -v pg_isready &> /dev/null; then
    DB_HOST="${POSTGRES_HOST:-postgres}"
    DB_PORT="${POSTGRES_PORT:-5432}"
    
    if pg_isready -h "$DB_HOST" -p "$DB_PORT" -U "${POSTGRES_USER:-dev_user}" > /dev/null 2>&1; then
        echo "✓ Database is ready"
    else
        echo "⚠ WARNING: Database may not be ready"
        echo "  Host: $DB_HOST"
        echo "  Port: $DB_PORT"
        echo "  This may cause the application to fail on startup"
    fi
else
    echo "⚠ WARNING: pg_isready not available, skipping database check"
fi

echo ""

# Check required environment variables
echo "Checking environment variables..."
MISSING_VARS=0

if [ -z "$TELEGRAM_BOT_TOKEN" ]; then
    echo "⚠ WARNING: TELEGRAM_BOT_TOKEN is not set"
    MISSING_VARS=$((MISSING_VARS + 1))
else
    echo "✓ TELEGRAM_BOT_TOKEN is set"
fi

if [ -z "$SCHOOL21_API_USERNAME" ]; then
    echo "⚠ WARNING: SCHOOL21_API_USERNAME is not set"
    MISSING_VARS=$((MISSING_VARS + 1))
else
    echo "✓ SCHOOL21_API_USERNAME is set"
fi

if [ -z "$SCHOOL21_API_PASSWORD" ]; then
    echo "⚠ WARNING: SCHOOL21_API_PASSWORD is not set"
    MISSING_VARS=$((MISSING_VARS + 1))
else
    echo "✓ SCHOOL21_API_PASSWORD is set"
fi

echo ""

if [ $MISSING_VARS -gt 0 ]; then
    echo "⚠ WARNING: Some environment variables are missing"
    echo "  The application may not function correctly"
    echo "  Please check your .env file or environment variables"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
    echo ""
fi

# Check if process is already running
echo "Checking if application is already running..."
if pgrep -f "$BINARY_PATH" > /dev/null; then
    echo "⚠ WARNING: Application appears to be already running"
    echo "  PID: $(pgrep -f "$BINARY_PATH" | head -1)"
    echo ""
    read -p "Continue and start another instance? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
    echo ""
fi

# Start the application
echo "Starting application..."
echo "  Binary: $BINARY_PATH"
echo "  Working directory: $PROJECT_ROOT"
echo ""
echo "Press Ctrl+C to stop the application"
echo ""

# Run the binary
"$BINARY_PATH"

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "Application exited successfully"
else
    echo "Application exited with code: $EXIT_CODE"
fi

# Verify process is running (if we get here, it means the process exited)
echo ""
echo "Checking process status..."
if pgrep -f "$BINARY_PATH" > /dev/null; then
    echo "✓ Application is still running"
    echo "  PID: $(pgrep -f "$BINARY_PATH" | head -1)"
    echo ""
    echo "To check process status, run: ps aux | grep school_tg_tt_bot"
else
    echo "Application is not running"
    echo "  Exit code: $EXIT_CODE"
fi

echo ""

