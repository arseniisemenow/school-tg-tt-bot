#!/bin/bash
# Patch Conan-generated OpenSSL CMake files to add 'ssl' and 'crypto' as system libraries

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Run setup-env.sh first."
    exit 1
fi

# Find OpenSSL CMake files
OPENSSL_TARGET_FILE=$(find "$BUILD_DIR" -name "OpenSSL-Target-release.cmake" -o -name "OpenSSL-Target-debug.cmake" | head -1)

if [ -z "$OPENSSL_TARGET_FILE" ]; then
    echo "OpenSSL CMake files not found. Run setup-env.sh first."
    exit 1
fi

# Patch the file to add 'ssl' and 'crypto' to system_libs
# This fixes the issue where Conan's OpenSSL package doesn't declare these libraries
for file in "$BUILD_DIR"/OpenSSL-Target-*.cmake; do
    if [ -f "$file" ]; then
        # Check if already patched
        if grep -q "SYSTEM_LIBS.*ssl" "$file"; then
            echo "File $file already patched"
            continue
        fi
        
        # Add ssl and crypto to system_libs if they're missing
        # Look for the line that sets system libs and add ssl/crypto if not present
        sed -i 's/set(OpenSSL_SYSTEM_LIBS_RELEASE "\(.*\)")/set(OpenSSL_SYSTEM_LIBS_RELEASE "\1 ssl crypto")/g' "$file" 2>/dev/null || true
        sed -i 's/set(OpenSSL_SYSTEM_LIBS_DEBUG "\(.*\)")/set(OpenSSL_SYSTEM_LIBS_DEBUG "\1 ssl crypto")/g' "$file" 2>/dev/null || true
        
        # If the variable doesn't exist, add it
        if ! grep -q "OpenSSL_SYSTEM_LIBS" "$file"; then
            # Find a good place to insert (after variable declarations)
            sed -i '/^set(OpenSSL_LIBS/a set(OpenSSL_SYSTEM_LIBS_RELEASE "ssl crypto")\nset(OpenSSL_SYSTEM_LIBS_DEBUG "ssl crypto")' "$file" 2>/dev/null || true
        fi
        
        echo "Patched $file"
    fi
done

echo "OpenSSL CMake files patched successfully"

