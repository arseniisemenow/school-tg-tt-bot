#!/bin/bash
# Helper script to set up a tunnel for webhook development
# Run this on the HOST machine (not in container)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="$PROJECT_ROOT/.env"

echo "=== Setting up Webhook Tunnel ==="
echo ""
echo "This script will help you set up a tunnel so Telegram can reach your bot."
echo "The bot is running on port 8443 in Docker."
echo ""

# Check if tunnel tools are available
TUNNEL_TOOL=""
TUNNEL_CMD=""

if command -v ngrok &> /dev/null; then
    TUNNEL_TOOL="ngrok"
    TUNNEL_CMD="ngrok http 8443"
    echo "✓ Found ngrok"
elif command -v cloudflared &> /dev/null || [ -f ~/bin/cloudflared ]; then
    TUNNEL_TOOL="cloudflared"
    if [ -f ~/bin/cloudflared ]; then
        TUNNEL_CMD="$HOME/bin/cloudflared tunnel --url http://localhost:8443"
    else
        TUNNEL_CMD="cloudflared tunnel --url http://localhost:8443"
    fi
    echo "✓ Found cloudflared"
else
    echo "No tunnel tool found. Installing cloudflared (free, no registration needed)..."
    echo ""
    
    # Try to download cloudflared
    mkdir -p ~/bin
    cd ~/bin
    
    if wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -O cloudflared 2>/dev/null; then
        chmod +x cloudflared
        TUNNEL_TOOL="cloudflared"
        TUNNEL_CMD="$HOME/bin/cloudflared tunnel --url http://localhost:8443"
        echo "✓ Downloaded cloudflared to ~/bin/cloudflared"
        
        # Add to PATH if not already there
        if [[ ":$PATH:" != *":$HOME/bin:"* ]]; then
            export PATH="$HOME/bin:$PATH"
            echo "export PATH=\"\$HOME/bin:\$PATH\"" >> ~/.bashrc 2>/dev/null || true
            echo "✓ Added ~/bin to PATH"
        fi
    else
        echo "ERROR: Failed to download cloudflared"
        echo ""
        echo "Please install manually:"
        echo "  1. Download from: https://github.com/cloudflare/cloudflared/releases"
        echo "  2. Extract and add to PATH"
        echo "  3. Run this script again"
        echo ""
        exit 1
    fi
fi

echo ""
echo "Starting tunnel in background..."
echo "Command: $TUNNEL_CMD"
echo ""

# Start tunnel in background
$TUNNEL_CMD > /tmp/tunnel.log 2>&1 &
TUNNEL_PID=$!

echo "Tunnel started (PID: $TUNNEL_PID)"
echo "Waiting for tunnel to establish..."
sleep 5

# Extract tunnel URL from logs
TUNNEL_URL=""
TUNNEL_DOMAIN=""

if [ "$TUNNEL_TOOL" == "ngrok" ]; then
    # Try to get URL from ngrok API
    TUNNEL_URL=$(curl -s http://localhost:4040/api/tunnels 2>/dev/null | grep -oP '"public_url":"https://[^"]+' | head -1 | cut -d'"' -f4)
    if [ -n "$TUNNEL_URL" ]; then
        TUNNEL_DOMAIN="${TUNNEL_URL#https://}"
    fi
elif [ "$TUNNEL_TOOL" == "cloudflared" ]; then
    # Extract from cloudflared logs - cloudflared outputs URL in a specific format
    sleep 3  # Give cloudflared more time to output URL
    # Look for the line with "Visit it at" which contains the URL
    TUNNEL_URL=$(grep -i "visit it at" /tmp/tunnel.log 2>/dev/null | grep -oE 'https://[a-zA-Z0-9-]+\.trycloudflare\.com' | head -1)
    if [ -z "$TUNNEL_URL" ]; then
        # Try simpler pattern - just find any https://...trycloudflare.com
        TUNNEL_URL=$(grep -oE 'https://[a-zA-Z0-9-]+\.trycloudflare\.com' /tmp/tunnel.log 2>/dev/null | head -1)
    fi
    if [ -z "$TUNNEL_URL" ]; then
        # Try matching the full line format
        TUNNEL_URL=$(grep -E 'trycloudflare\.com' /tmp/tunnel.log 2>/dev/null | grep -oE 'https://[^[:space:]]+\.trycloudflare\.com' | head -1)
    fi
    if [ -n "$TUNNEL_URL" ]; then
        TUNNEL_DOMAIN="${TUNNEL_URL#https://}"
        # Remove trailing slash if present
        TUNNEL_DOMAIN="${TUNNEL_DOMAIN%/}"
    fi
fi

if [ -z "$TUNNEL_DOMAIN" ]; then
    echo "WARNING: Could not automatically extract tunnel URL"
    echo "Check the tunnel output:"
    echo "  - ngrok: http://localhost:4040"
    echo "  - cloudflared: Check /tmp/tunnel.log"
    echo ""
    read -p "Enter the tunnel domain (e.g., abc123.ngrok.io or abc123.trycloudflare.com): " TUNNEL_DOMAIN_INPUT
    if [ -n "$TUNNEL_DOMAIN_INPUT" ]; then
        TUNNEL_DOMAIN="$TUNNEL_DOMAIN_INPUT"
        TUNNEL_URL="https://$TUNNEL_DOMAIN"
    else
        echo "No domain provided. Exiting."
        kill $TUNNEL_PID 2>/dev/null || true
        exit 1
    fi
fi

echo ""
echo "✓ Tunnel established!"
echo "  URL: $TUNNEL_URL"
echo "  Domain: $TUNNEL_DOMAIN"
echo "  PID: $TUNNEL_PID"
echo ""

# Update .env file
if [ -f "$ENV_FILE" ]; then
    # Backup .env
    cp "$ENV_FILE" "$ENV_FILE.bak"
    
    # Update WEBHOOK_DOMAIN
    if grep -q "^WEBHOOK_DOMAIN=" "$ENV_FILE"; then
        sed -i "s|^WEBHOOK_DOMAIN=.*|WEBHOOK_DOMAIN=$TUNNEL_DOMAIN|" "$ENV_FILE"
    else
        echo "WEBHOOK_DOMAIN=$TUNNEL_DOMAIN" >> "$ENV_FILE"
    fi
    
    echo "✓ Updated .env file with WEBHOOK_DOMAIN=$TUNNEL_DOMAIN"
else
    echo "WARNING: .env file not found at $ENV_FILE"
    echo "Please manually set: WEBHOOK_DOMAIN=$TUNNEL_DOMAIN"
fi

echo ""
echo "=== Next Steps ==="
echo ""
echo "1. The tunnel is running in the background (PID: $TUNNEL_PID)"
echo "2. Keep this terminal open or the tunnel will close"
echo "3. Redeploy the bot: ./scripts/deploy-dev.sh"
echo "4. The bot will register the webhook with Telegram automatically"
echo ""
echo "To stop the tunnel: kill $TUNNEL_PID"
echo "To view tunnel logs: tail -f /tmp/tunnel.log"
echo ""

