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
docker rmi school-tg-tt-bot:dev 2>/dev/null || echo "  Image not found (already removed)"

echo ""
echo "✓ Cleanup complete"
echo ""
echo "Next steps:"
echo "  1. Rebuild image: docker build -t school-tg-tt-bot:dev -f Dockerfile ."
echo "  2. Redeploy: ./scripts/deploy-dev.sh"

