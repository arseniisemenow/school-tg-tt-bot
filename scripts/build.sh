#!/bin/bash
# Build script - builds the C++ application
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Building application ==="
echo ""

cd "$PROJECT_ROOT"

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed. Please ensure you're in the bot container."
    exit 1
fi

echo "✓ CMake found: $(cmake --version | head -n 1)"
echo ""

# Configure CMake
echo "Configuring CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

echo "✓ CMake configuration successful"
echo ""

# Build the application
echo "Building application..."
echo "This may take a few minutes..."
echo "Using $(nproc) parallel jobs for faster builds"
echo ""

cmake --build build --parallel $(nproc)

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo ""
echo "=== Build complete ==="
echo ""

# Check if binary was created
BINARY_PATH="build/school_tg_tt_bot"
if [ -f "$BINARY_PATH" ]; then
    echo "✓ Binary created: $BINARY_PATH"
    ls -lh "$BINARY_PATH"
    echo ""
    echo "Next steps:"
    echo "  - Run tests: ./scripts/test.sh"
    echo "  - Run application: ./scripts/run.sh"
else
    echo "WARNING: Binary not found at expected location: $BINARY_PATH"
    echo "Build may have completed but binary location is different"
    exit 1
fi

echo ""
