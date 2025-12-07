#!/bin/bash
# Build script - builds the C++ application in a Docker container
# Build artifacts are output to ./build/ directory on the host
# Usage: ./scripts/build-docker.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILDER_IMAGE="image-build:1.0.0"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building Application in Docker Container ==="
echo ""

# Ensure build directory exists
mkdir -p "$BUILD_DIR"

# Require prebuilt builder image (compile once via scripts/compile-images.sh or build-images.sh)
if ! docker image inspect "$BUILDER_IMAGE" &> /dev/null; then
    echo "ERROR: Builder image not found: $BUILDER_IMAGE"
    echo "Please build images first:"
    echo "  ./scripts/build-images.sh    # builds builder and deploy images"
    echo "    or"
    echo "  ./scripts/compile-images.sh  # builds builder image only"
    exit 1
fi

echo "Building application..."
echo "  Source: $PROJECT_ROOT"
echo "  Output: $BUILD_DIR"
echo ""

# Run build in container (interactive for logs)
docker run --rm -i \
    -v "$PROJECT_ROOT:/src" \
    -v "$BUILD_DIR:/src/build" \
    -w /src \
    "$BUILDER_IMAGE" \
    bash -c "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel \$(nproc)"

echo ""
echo "=== Build Complete ==="
echo ""

# Check if binary was created
BINARY_PATH="$BUILD_DIR/school_tg_tt_bot"
if [ -f "$BINARY_PATH" ]; then
    echo "✓ Binary created: $BINARY_PATH"
    ls -lh "$BINARY_PATH"
    echo ""
    echo "Build artifacts are ready for packaging."
echo "Next: ./scripts/package-runtime.sh [tag]"
    echo "Then deploy: ./scripts/deploy-dev.sh"
else
    echo "WARNING: Binary not found at expected location: $BINARY_PATH"
    echo "Build may have completed but binary location is different"
    exit 1
fi

echo ""

