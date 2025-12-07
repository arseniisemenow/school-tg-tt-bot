#!/bin/bash
# Deploy bot to Docker Swarm (development - single node)
# Run from host machine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

STACK_NAME="school-tg-bot"
STACK_FILE="$PROJECT_ROOT/docker-stack.dev.yml"
DEPLOY_IMAGE_TAG="${DEPLOY_IMAGE_TAG:-image-deploy:1.0.0}"

echo "=== Deploying School TG Bot to Docker Swarm (Dev) ==="
echo ""

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker is not installed."
    exit 1
fi

# Check if Docker Swarm is initialized
if ! docker info | grep -q "Swarm: active"; then
    echo "Docker Swarm is not initialized. Initializing..."
    docker swarm init
    echo "✓ Docker Swarm initialized"
else
    echo "✓ Docker Swarm is already initialized"
fi

echo ""

# Check for required environment variables
echo "Checking environment variables..."
if [ -z "$TELEGRAM_BOT_TOKEN" ] && [ ! -f "$PROJECT_ROOT/.env" ]; then
    echo "WARNING: TELEGRAM_BOT_TOKEN not set and .env file not found"
    echo "  Secrets will need to be created manually"
fi

# Create secrets directory if it doesn't exist
SECRETS_DIR="$PROJECT_ROOT/secrets"
if [ ! -d "$SECRETS_DIR" ]; then
    mkdir -p "$SECRETS_DIR"
    echo "Created secrets directory: $SECRETS_DIR"
fi

# Load environment variables from .env file if it exists
if [ -f "$PROJECT_ROOT/.env" ]; then
    echo "Loading environment variables from .env file..."
    # Use set -a to automatically export all variables
    set -a
    source "$PROJECT_ROOT/.env"
    set +a
    echo "✓ Environment variables loaded from .env"
    
    # Create Docker secrets if they don't exist (for file-based secrets)
    if [ -n "$TELEGRAM_BOT_TOKEN" ]; then
        echo "$TELEGRAM_BOT_TOKEN" > "$SECRETS_DIR/telegram_bot_token.txt"
        echo "✓ Created telegram_bot_token secret file"
    fi
    
    if [ -n "$SCHOOL21_API_USERNAME" ]; then
        echo "$SCHOOL21_API_USERNAME" > "$SECRETS_DIR/school21_api_username.txt"
        echo "✓ Created school21_api_username secret file"
    fi
    
    if [ -n "$SCHOOL21_API_PASSWORD" ]; then
        echo "$SCHOOL21_API_PASSWORD" > "$SECRETS_DIR/school21_api_password.txt"
        echo "✓ Created school21_api_password secret file"
    fi
else
    echo "WARNING: .env file not found at $PROJECT_ROOT/.env"
    echo "  Make sure TELEGRAM_BOT_TOKEN is set as environment variable"
fi

echo ""

# Check webhook configuration
if [ -z "$WEBHOOK_DOMAIN" ] && [ -z "$WEBHOOK_URL" ]; then
    echo "WARNING: WEBHOOK_DOMAIN or WEBHOOK_URL not set"
    echo ""
    echo "Options:"
    echo "1. Set up tunnel automatically (cloudflared - recommended)"
    echo "2. Set up tunnel manually (ngrok/cloudflared/localtunnel)"
    echo "3. Continue without webhook (will use polling mode)"
    echo ""
    read -p "Choose option (1/2/3): " -n 1 -r
    echo
    
    if [[ $REPLY == "1" ]]; then
        # Try to use cloudflared if available
        if command -v cloudflared &> /dev/null; then
            echo "Setting up cloudflared tunnel..."
            # Start cloudflared in background
            cloudflared tunnel --url http://localhost:8443 > /tmp/cloudflared.log 2>&1 &
            CLOUDFLARED_PID=$!
            sleep 3
            
            # Extract URL from cloudflared output
            if [ -f /tmp/cloudflared.log ]; then
                TUNNEL_URL=$(grep -oP 'https://[a-zA-Z0-9-]+\.trycloudflare\.com' /tmp/cloudflared.log | head -1)
                if [ -n "$TUNNEL_URL" ]; then
                    # Extract domain (remove https://)
                    WEBHOOK_DOMAIN="${TUNNEL_URL#https://}"
                    export WEBHOOK_DOMAIN
                    echo "✓ Tunnel established: $WEBHOOK_DOMAIN"
                    echo "  Cloudflared PID: $CLOUDFLARED_PID"
                    echo "  To stop tunnel: kill $CLOUDFLARED_PID"
                else
                    echo "Failed to extract tunnel URL. Check /tmp/cloudflared.log"
                    kill $CLOUDFLARED_PID 2>/dev/null || true
                    exit 1
                fi
            else
                echo "Failed to start cloudflared tunnel"
                exit 1
            fi
        else
            echo "cloudflared not found. Install it:"
            echo "  brew install cloudflare/cloudflare/cloudflared  # macOS"
            echo "  or download from https://github.com/cloudflare/cloudflared/releases"
            echo ""
            echo "Falling back to manual setup..."
            REPLY="2"
        fi
    fi
    
    if [[ $REPLY == "2" ]]; then
        echo ""
        echo "Manual tunnel setup:"
        echo "  Option A - ngrok:"
        echo "    1. Install ngrok: https://ngrok.com/download"
        echo "    2. Run: ngrok http 8443"
        echo "    3. Copy the HTTPS URL (e.g., abc123.ngrok.io)"
        echo "    4. Set: export WEBHOOK_DOMAIN=abc123.ngrok.io"
        echo ""
        echo "  Option B - cloudflared:"
        echo "    1. Install cloudflared"
        echo "    2. Run: cloudflared tunnel --url http://localhost:8443"
        echo "    3. Copy the HTTPS URL (e.g., abc123.trycloudflare.com)"
        echo "    4. Set: export WEBHOOK_DOMAIN=abc123.trycloudflare.com"
        echo ""
        echo "  Option C - localtunnel:"
        echo "    1. Install: npm install -g localtunnel"
        echo "    2. Run: lt --port 8443"
        echo "    3. Copy the HTTPS URL"
        echo "    4. Set: export WEBHOOK_DOMAIN=<tunnel-domain>"
        echo ""
        read -p "Enter WEBHOOK_DOMAIN (or press Enter to skip): " WEBHOOK_DOMAIN_INPUT
        if [ -n "$WEBHOOK_DOMAIN_INPUT" ]; then
            export WEBHOOK_DOMAIN="$WEBHOOK_DOMAIN_INPUT"
            echo "✓ WEBHOOK_DOMAIN set to: $WEBHOOK_DOMAIN"
        else
            echo "Continuing without webhook (will use polling mode)"
        fi
    fi
    
    if [[ $REPLY == "3" ]]; then
        echo "Continuing without webhook (will use polling mode)"
    fi
    
    if [[ ! $REPLY =~ ^[123]$ ]]; then
        echo "Invalid option. Continuing without webhook..."
    fi
fi

# Generate webhook secret token if not set
if [ -z "$WEBHOOK_SECRET_TOKEN" ]; then
    echo ""
    echo "WEBHOOK_SECRET_TOKEN not set - generating random token..."
    WEBHOOK_SECRET_TOKEN=$(openssl rand -hex 32 2>/dev/null || head -c 32 /dev/urandom | base64 | tr -d '\n')
    export WEBHOOK_SECRET_TOKEN
    echo "✓ Generated WEBHOOK_SECRET_TOKEN"
    echo "  (Save this for reference: $WEBHOOK_SECRET_TOKEN)"
fi

echo ""

# Check if build artifacts exist
BUILD_BINARY="$PROJECT_ROOT/build/school_tg_tt_bot"
if [ ! -f "$BUILD_BINARY" ]; then
    echo "ERROR: Build artifacts not found at $BUILD_BINARY"
    echo ""
    echo "Please build the project first (e.g., inside bot container):"
    echo "  docker compose exec bot ./scripts/build.sh"
    echo ""
    echo "Or build via builder image:"
    echo "  ./scripts/build-docker.sh"
    echo ""
    echo "Ensure the binary exists at:"
    echo "  $BUILD_BINARY"
    exit 1
fi

echo "✓ Build artifacts found: $BUILD_BINARY"
echo ""

# Package runtime image (no compilation)
echo "Packaging runtime image: $DEPLOY_IMAGE_TAG"
"$PROJECT_ROOT/scripts/package-runtime.sh" "$DEPLOY_IMAGE_TAG"
echo ""

# Load environment variables from .env file if it exists
if [ -f "$PROJECT_ROOT/.env" ]; then
    echo "Loading environment variables from .env file..."
    # Source .env file to load variables
    set -a  # Automatically export all variables
    source "$PROJECT_ROOT/.env"
    set +a  # Stop automatically exporting
    echo "✓ Environment variables loaded from .env"
fi

# Deploy the stack with webhook environment variables
echo "Deploying stack: $STACK_NAME"
if [ -n "$WEBHOOK_DOMAIN" ]; then
    echo "  Using WEBHOOK_DOMAIN: $WEBHOOK_DOMAIN"
fi
if [ -n "$WEBHOOK_URL" ]; then
    echo "  Using WEBHOOK_URL: $WEBHOOK_URL"
fi
if [ -n "$WEBHOOK_SECRET_TOKEN" ]; then
    echo "  Using WEBHOOK_SECRET_TOKEN: [hidden]"
fi
if [ -n "$TELEGRAM_BOT_TOKEN" ]; then
    echo "  Using TELEGRAM_BOT_TOKEN: [hidden]"
else
    echo "  ERROR: TELEGRAM_BOT_TOKEN not set!"
    echo "  Make sure .env file exists and contains TELEGRAM_BOT_TOKEN"
    exit 1
fi
if [ -n "$SCHOOL21_API_USERNAME" ]; then
    echo "  Using SCHOOL21_API_USERNAME: [hidden]"
fi
if [ -n "$SCHOOL21_API_PASSWORD" ]; then
    echo "  Using SCHOOL21_API_PASSWORD: [hidden]"
fi

# Note: Environment variables are already exported via 'set -a' when loading .env
# Docker Swarm reads these from the shell environment when deploying
docker stack deploy -c "$STACK_FILE" "$STACK_NAME"

echo ""
echo "Waiting for services to start..."
sleep 5

# Show service status
echo ""
echo "=== Service Status ==="
docker stack services "$STACK_NAME"

echo ""
echo "=== Service Logs (last 20 lines) ==="
docker service logs --tail 20 "$STACK_NAME"_bot 2>&1 || echo "Bot service not ready yet"

echo ""
echo "=== Deployment Complete ==="
echo ""

if [ -n "$WEBHOOK_DOMAIN" ] || [ -n "$WEBHOOK_URL" ]; then
    echo "✓ Webhook mode enabled"
    echo "  The bot will automatically register the webhook with Telegram on startup"
    echo "  (WEBHOOK_REGISTRAR=true is set in docker-stack.dev.yml)"
    echo ""
    if [ -n "$CLOUDFLARED_PID" ]; then
        echo "⚠ Cloudflared tunnel is running (PID: $CLOUDFLARED_PID)"
        echo "  Keep this terminal open or the tunnel will close"
        echo "  To stop tunnel: kill $CLOUDFLARED_PID"
        echo ""
    fi
else
    echo "⚠ Webhook not configured - bot will use polling mode"
    echo "  To enable webhook:"
    echo "    1. Set up a tunnel (ngrok/cloudflared)"
    echo "    2. Set WEBHOOK_DOMAIN environment variable"
    echo "    3. Redeploy: ./scripts/deploy-dev.sh"
    echo ""
fi

echo "Next steps:"
echo "1. Check service status: docker stack services $STACK_NAME"
echo "2. View logs: docker service logs -f ${STACK_NAME}_bot"
echo "3. Verify webhook registration (if enabled):"
echo "   docker service logs ${STACK_NAME}_bot | grep -i webhook"
echo ""
echo "To remove the stack: docker stack rm $STACK_NAME"

