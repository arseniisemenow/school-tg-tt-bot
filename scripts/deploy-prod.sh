#!/bin/bash
# Deploy bot to Docker Swarm (production - multi-node)
# Run from Swarm manager node

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

STACK_NAME="school-tg-bot"
STACK_FILE="$PROJECT_ROOT/docker-stack.prod.yml"

echo "=== Deploying School TG Bot to Docker Swarm (Production) ==="
echo ""

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker is not installed."
    exit 1
fi

# Check if running on Swarm manager
if ! docker info | grep -q "Swarm: active"; then
    echo "ERROR: Docker Swarm is not active on this node"
    echo "  This script must be run on a Swarm manager node"
    exit 1
fi

if ! docker info | grep -q "Is Manager: true"; then
    echo "ERROR: This node is not a Swarm manager"
    echo "  This script must be run on a Swarm manager node"
    exit 1
fi

echo "✓ Running on Swarm manager node"

# Check for required environment variables
echo ""
echo "Checking required environment variables..."

REQUIRED_VARS=("WEBHOOK_DOMAIN")
MISSING_VARS=()

for var in "${REQUIRED_VARS[@]}"; do
    if [ -z "${!var}" ]; then
        MISSING_VARS+=("$var")
    fi
done

if [ ${#MISSING_VARS[@]} -ne 0 ]; then
    echo "ERROR: Missing required environment variables:"
    for var in "${MISSING_VARS[@]}"; do
        echo "  - $var"
    done
    exit 1
fi

echo "✓ Required environment variables set"

# Check for Docker secrets
echo ""
echo "Checking Docker secrets..."

REQUIRED_SECRETS=(
    "telegram_bot_token"
    "school21_api_username"
    "school21_api_password"
    "postgres_db"
    "postgres_user"
    "postgres_password"
    "webhook_secret_token"
)

MISSING_SECRETS=()

for secret in "${REQUIRED_SECRETS[@]}"; do
    if ! docker secret ls | grep -q "$secret"; then
        MISSING_SECRETS+=("$secret")
    fi
done

if [ ${#MISSING_SECRETS[@]} -ne 0 ]; then
    echo "WARNING: Missing Docker secrets:"
    for secret in "${MISSING_SECRETS[@]}"; do
        echo "  - $secret"
    done
    echo ""
    echo "Create secrets using:"
    echo "  echo 'value' | docker secret create secret_name -"
    echo ""
    echo "Or create from file:"
    echo "  docker secret create secret_name /path/to/file"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "✓ All required secrets exist"
fi

# Check webhook configuration
echo ""
if [ -z "$WEBHOOK_SECRET_TOKEN" ]; then
    echo "WARNING: WEBHOOK_SECRET_TOKEN environment variable not set"
    echo "  Webhook requests will not be validated"
    echo "  Make sure the secret 'webhook_secret_token' is created"
fi

# Check node configuration for webhook registrar
echo ""
echo "Checking node configuration..."
NODE_COUNT=$(docker node ls -q | wc -l)
echo "  Swarm nodes: $NODE_COUNT"

# Check if any node is labeled as webhook registrar
REGISTRAR_NODES=$(docker node ls --filter "node.labels.webhook.registrar=true" -q | wc -l)
if [ "$REGISTRAR_NODES" -eq 0 ]; then
    echo "  WARNING: No nodes labeled as webhook registrar"
    echo "  To label a node: docker node update --label-add webhook.registrar=true <node-i  d>"
    echo "  Or set WEBHOOK_REGISTRAR=true in environment for one instance"
fi

echo ""

# Show current stack status if it exists
if docker stack ls | grep -q "$STACK_NAME"; then
    echo "Stack $STACK_NAME already exists"
    echo "Current service status:"
    docker stack services "$STACK_NAME"
    echo ""
    read -p "Update existing stack? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Deployment cancelled"
        exit 0
    fi
fi

# Deploy the stack
echo ""
echo "Deploying stack: $STACK_NAME"
echo "  Stack file: $STACK_FILE"
echo "  Webhook domain: $WEBHOOK_DOMAIN"

docker stack deploy -c "$STACK_FILE" "$STACK_NAME"

echo ""
echo "Waiting for services to start..."
sleep 10

# Show service status
echo ""
echo "=== Service Status ==="
docker stack services "$STACK_NAME"

echo ""
echo "=== Service Tasks ==="
docker stack ps "$STACK_NAME"

echo ""
echo "=== Bot Service Logs (last 30 lines) ==="
docker service logs --tail 30 "$STACK_NAME"_bot 2>&1 || echo "Bot service not ready yet"

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Next steps:"
echo "1. Monitor services: docker stack services $STACK_NAME"
echo "2. View logs: docker service logs -f $STACK_NAME_bot"
echo "3. Check webhook registration:"
echo "   - Look for 'Webhook registered with Telegram' in bot logs"
echo "   - Only one instance should show this message (the registrar)"
echo "4. Verify webhook is receiving updates"
echo "5. Scale bot replicas: docker service scale ${STACK_NAME}_bot=N"
echo ""
echo "To remove the stack: docker stack rm $STACK_NAME"
echo "To update the stack: Run this script again"

