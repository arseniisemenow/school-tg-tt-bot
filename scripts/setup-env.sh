#!/bin/bash
# Setup environment script - verifies system packages are installed
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Verifying development environment ==="
echo ""

cd "$PROJECT_ROOT"

# Check if required packages are installed
echo "Checking for required packages..."

MISSING_PACKAGES=()

if ! dpkg -l | grep -q libpqxx-dev; then
    MISSING_PACKAGES+=("libpqxx-dev")
fi

if ! dpkg -l | grep -q libcurl4-openssl-dev; then
    MISSING_PACKAGES+=("libcurl4-openssl-dev")
fi

if ! dpkg -l | grep -q nlohmann-json3-dev; then
    MISSING_PACKAGES+=("nlohmann-json3-dev")
fi

if ! dpkg -l | grep -q libssl-dev; then
    MISSING_PACKAGES+=("libssl-dev")
fi

if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
    echo "ERROR: Missing required packages: ${MISSING_PACKAGES[*]}"
    echo "Please install them: sudo apt-get install ${MISSING_PACKAGES[*]}"
    exit 1
fi

echo "✓ All required packages are installed"
echo ""

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed."
    echo "Please install: sudo apt-get install cmake"
    exit 1
fi

echo "✓ CMake found: $(cmake --version | head -n 1)"
echo ""

# Create build directory if it doesn't exist
mkdir -p build

echo "=== Environment setup complete ==="
echo ""
echo "Dependencies are ready to use."
echo "Next step: Run ./scripts/build.sh to build the application"
echo ""
