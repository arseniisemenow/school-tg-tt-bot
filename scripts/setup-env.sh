#!/bin/bash
# Setup environment script - installs Conan dependencies
# Run inside bot container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Setting up development environment ==="
echo ""

cd "$PROJECT_ROOT"

# Check if Conan is installed
if ! command -v conan &> /dev/null; then
    echo "ERROR: Conan is not installed. Please ensure you're in the bot container."
    exit 1
fi

echo "✓ Conan found: $(conan --version)"
echo ""

# Check if conanfile.txt exists
if [ ! -f conanfile.txt ]; then
    echo "ERROR: conanfile.txt not found in project root"
    exit 1
fi

# Create build directory if it doesn't exist
mkdir -p build

# Detect Conan profile if not exists
if [ ! -f ~/.conan2/profiles/default ]; then
    echo "Detecting Conan profile..."
    conan profile detect --force
    echo "✓ Conan profile detected"
else
    echo "✓ Using existing Conan profile"
fi

echo ""
echo "Installing Conan dependencies..."
echo "This may take a few minutes on first run..."
echo ""

# Install dependencies
conan install . \
    --output-folder=build \
    --build=missing \
    -s compiler.cppstd=20

echo ""
echo "=== Verifying installed libraries ==="
echo ""

# Check if conan_toolchain.cmake exists (indicates successful install)
if [ -f build/conan_toolchain.cmake ]; then
    echo "✓ Conan toolchain file found"
else
    echo "ERROR: Conan toolchain file not found. Installation may have failed."
    exit 1
fi

# Check for key libraries
echo "Checking for installed libraries..."

# Check nlohmann_json
if [ -f build/nlohmann_json-config.cmake ] || [ -f build/nlohmann_json-config-version.cmake ]; then
    echo "✓ nlohmann_json installed"
else
    echo "WARNING: nlohmann_json not found in expected location"
fi

# Check libpqxx
if [ -f build/libpqxx-config.cmake ] || [ -f build/libpqxx-config-version.cmake ]; then
    echo "✓ libpqxx installed"
else
    echo "WARNING: libpqxx not found in expected location"
fi

# Check for CMake dependency files
CMAKE_DEPS_COUNT=$(find build -name "*-config.cmake" -o -name "*-Targets.cmake" 2>/dev/null | wc -l)
if [ "$CMAKE_DEPS_COUNT" -gt 0 ]; then
    echo "✓ Found $CMAKE_DEPS_COUNT CMake dependency files"
else
    echo "WARNING: No CMake dependency files found"
fi

echo ""
echo "=== Patching Conan-generated CMake files ==="
echo ""

# Patch OpenSSL CMake files to fix library declaration issues
if [ -f ./scripts/patch-conan-openssl.sh ]; then
    ./scripts/patch-conan-openssl.sh || echo "Warning: Failed to patch OpenSSL files (may not be needed)"
fi

echo ""
echo "=== Environment setup complete ==="
echo ""
echo "Dependencies are installed and ready to use."
echo "Next step: Run ./scripts/build.sh to build the application"
echo ""

