#!/bin/bash
# Build both builder and runtime images.
# - Builder image: contains toolchain only (no app build).
# - Deploy image: packages prebuilt binary from ./build.
#
# Usage: ./scripts/build-images.sh [builder-tag] [deploy-tag]
# Defaults: builder-tag=image-build:1.0.0 deploy-tag=image-deploy:1.0.0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILDER_TAG="${1:-image-build:1.0.0}"
DEPLOY_TAG="${2:-image-deploy:1.0.0}"
BINARY_PATH="$PROJECT_ROOT/build/school_tg_tt_bot"

echo "=== Building Docker images ==="
echo "  Builder image: $BUILDER_TAG"
echo "  Deploy image:  $DEPLOY_TAG"
echo ""

echo "Building builder image (toolchain only)..."
docker build -t "$BUILDER_TAG" -f "$PROJECT_ROOT/Dockerfile.dev" "$PROJECT_ROOT"
echo "✓ Builder image built: $BUILDER_TAG"
echo ""

echo "Checking for prebuilt binary at $BINARY_PATH..."
if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: Build artifact not found."
    echo "Please build the project first (e.g., inside bot container):"
    echo "  docker compose exec bot ./scripts/build.sh"
    exit 1
fi

echo "Building deploy/runtime image..."
docker build -t "$DEPLOY_TAG" -f "$PROJECT_ROOT/Dockerfile.deploy" "$PROJECT_ROOT"
echo "✓ Deploy image built: $DEPLOY_TAG"
echo ""

echo "Images ready:"
echo "  Builder: $BUILDER_TAG"
echo "  Deploy:  $DEPLOY_TAG"
echo ""

