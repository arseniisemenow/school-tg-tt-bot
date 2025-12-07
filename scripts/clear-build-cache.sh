#!/bin/bash
# Clear Docker images and rebuild
# Use this if you want a clean rebuild

set -e

STACK_NAME="school-tg-bot"

echo "=== Clearing Docker Images ==="
echo ""

# Remove stack if running
if docker stack ls | grep -q "$STACK_NAME"; then
    echo "Removing stack..."
    docker stack rm "$STACK_NAME"
    echo "Waiting for stack removal..."
    sleep 5
fi

# Remove old images
echo "Removing old images..."
docker rmi image-deploy:1.0.0 2>/dev/null || echo "  Deploy image not found (already removed)"
docker rmi image-deploy-base:1.0.0 2>/dev/null || echo "  Deploy base image not found (already removed)"
docker rmi image-build:1.0.0 2>/dev/null || echo "  Builder image not found (already removed)"

echo ""
echo "✓ Cleanup complete"
echo ""
echo "Next steps:"
echo "  1. Rebuild images (builder + deploy base): ./scripts/build-images.sh"
echo "  2. Rebuild artifacts: ./scripts/build-docker.sh"
echo "  3. Package runtime image: ./scripts/package-runtime.sh"
echo "  4. Redeploy: ./scripts/deploy-dev.sh"

