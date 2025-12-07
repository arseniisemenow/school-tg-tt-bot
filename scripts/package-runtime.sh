#!/bin/bash
# Package runtime image from prebuilt artifacts in ./build
# Usage:
#   ./scripts/package-runtime.sh [image-tag] [base-image-tag]
# Default tag: image-deploy:1.0.0 (from image-deploy-base:1.0.0)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE_TAG="${1:-image-deploy:1.0.0}"
BASE_IMAGE_TAG="${2:-image-deploy-base:1.0.0}"
BINARY_PATH="$PROJECT_ROOT/build/school_tg_tt_bot"

echo "=== Packaging runtime image ==="
echo "  Tag:        $IMAGE_TAG"
echo "  Base image: $BASE_IMAGE_TAG"
echo "  Source:     $PROJECT_ROOT/build"
echo ""

if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: Build artifact not found at $BINARY_PATH"
    echo "Please build first:"
    echo "  ./scripts/build-docker.sh"
    exit 1
fi

docker build \
  -t "$IMAGE_TAG" \
  -f "$PROJECT_ROOT/Dockerfile.deploy" \
  --build-arg BASE_IMAGE="$BASE_IMAGE_TAG" \
  "$PROJECT_ROOT"

echo "✓ Runtime image built: $IMAGE_TAG"

