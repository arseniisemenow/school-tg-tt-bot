#!/bin/bash
# Build Docker images that contain toolchains/deps (no app compilation).
# Usage: ./scripts/compile-images.sh
# Re-run this only when Dockerfiles change.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILDER_IMAGE="image-build:1.0.0"

echo "=== Building toolchain images (no app build) ==="
echo ""

echo "Building builder image: $BUILDER_IMAGE"
docker build -t "$BUILDER_IMAGE" -f "$PROJECT_ROOT/Dockerfile.dev" "$PROJECT_ROOT"
echo "✓ Builder image built"

echo ""
echo "Images ready. Use ./scripts/build-docker.sh to build artifacts with the prebuilt builder image."
echo "To build both builder and deploy images in one go, run ./scripts/build-images.sh."

