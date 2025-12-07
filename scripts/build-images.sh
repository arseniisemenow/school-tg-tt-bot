#!/bin/bash
# Build builder image and deploy runtime image (no app artifacts baked in).
# - Builder image: contains toolchain only (no app build).
# - Deploy image: runtime deps only (binary/config/migrations supplied at runtime).
#
# Usage: ./scripts/build-images.sh [builder-tag] [deploy-tag]
# Defaults: builder-tag=image-build:1.0.0 deploy-tag=image-deploy:1.0.0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILDER_TAG="${1:-image-build:1.0.0}"
DEPLOY_TAG="${2:-image-deploy:1.0.0}"

echo "=== Building Docker images ==="
echo "  Builder image: $BUILDER_TAG"
echo "  Deploy image:  $DEPLOY_TAG (no artifacts baked in)"
echo ""

echo "Building builder image (toolchain only)..."
docker build -t "$BUILDER_TAG" -f "$PROJECT_ROOT/Dockerfile.dev" "$PROJECT_ROOT"
echo "✓ Builder image built: $BUILDER_TAG"
echo ""

echo "Building deploy image (runtime only, no artifacts)..."
docker build -t "$DEPLOY_TAG" -f "$PROJECT_ROOT/Dockerfile.deploy" "$PROJECT_ROOT"
echo "✓ Deploy image built: $DEPLOY_TAG"
echo ""

echo "Images ready:"
echo "  Builder: $BUILDER_TAG"
echo "  Deploy:  $DEPLOY_TAG (expects binary/config/migrations mounted at runtime)"
echo ""

