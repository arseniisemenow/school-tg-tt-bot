#!/bin/bash
# Test script to verify tunnel and webhook are working

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Testing Tunnel and Webhook Setup ==="
echo ""

# 1. Check if tunnel is running
echo "1. Checking if tunnel is running..."
TUNNEL_PID=$(pgrep -f "cloudflared tunnel" | head -1)
if [ -n "$TUNNEL_PID" ]; then
    echo "   ✓ Tunnel is running (PID: $TUNNEL_PID)"
else
    echo "   ✗ Tunnel is NOT running!"
    echo "   Run: ./scripts/setup-tunnel.sh"
    exit 1
fi

# 2. Extract tunnel URL from logs
echo ""
echo "2. Extracting tunnel URL..."
TUNNEL_URL=$(grep -oE 'https://[a-zA-Z0-9-]+\.trycloudflare\.com' /tmp/tunnel.log 2>/dev/null | tail -1)
if [ -z "$TUNNEL_URL" ]; then
    echo "   ✗ Could not find tunnel URL in logs"
    echo "   Check: tail -f /tmp/tunnel.log"
    exit 1
fi
TUNNEL_DOMAIN="${TUNNEL_URL#https://}"
echo "   ✓ Tunnel URL: $TUNNEL_URL"
echo "   ✓ Tunnel Domain: $TUNNEL_DOMAIN"

# 3. Test if tunnel URL is reachable
echo ""
echo "3. Testing tunnel URL reachability..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "$TUNNEL_URL/webhook" -X POST -H "Content-Type: application/json" -d '{"test":true}' 2>&1 || echo "000")
if [ "$HTTP_CODE" == "200" ] || [ "$HTTP_CODE" == "404" ] || [ "$HTTP_CODE" == "400" ]; then
    echo "   ✓ Tunnel URL is reachable (HTTP $HTTP_CODE)"
else
    echo "   ✗ Tunnel URL is NOT reachable (HTTP $HTTP_CODE)"
    echo "   This means Telegram cannot reach your bot!"
    exit 1
fi

# 4. Check if bot service is running
echo ""
echo "4. Checking bot service..."
if docker service ls | grep -q "school-tg-bot_bot"; then
    echo "   ✓ Bot service is running"
    REPLICAS=$(docker service ls | grep school-tg-bot_bot | awk '{print $4}')
    echo "   ✓ Replicas: $REPLICAS"
else
    echo "   ✗ Bot service is NOT running!"
    exit 1
fi

# 5. Check webhook domain in bot environment
echo ""
echo "5. Checking webhook configuration in bot..."
BOT_WEBHOOK=$(docker service inspect school-tg-bot_bot --format '{{range .Spec.TaskTemplate.ContainerSpec.Env}}{{if eq (index (split . "=") 0) "WEBHOOK_DOMAIN"}}{{println .}}{{end}}{{end}}' 2>/dev/null | cut -d'=' -f2)
if [ -z "$BOT_WEBHOOK" ]; then
    echo "   ✗ WEBHOOK_DOMAIN not set in bot service!"
    exit 1
fi
echo "   ✓ Bot WEBHOOK_DOMAIN: $BOT_WEBHOOK"

if [ "$BOT_WEBHOOK" != "$TUNNEL_DOMAIN" ]; then
    echo "   ⚠ WARNING: Bot webhook domain ($BOT_WEBHOOK) doesn't match tunnel domain ($TUNNEL_DOMAIN)"
    echo "   You need to update .env and redeploy!"
    echo ""
    echo "   Run:"
    echo "   1. Update .env: WEBHOOK_DOMAIN=$TUNNEL_DOMAIN"
    echo "   2. Redeploy: ./scripts/deploy-dev.sh"
    exit 1
fi

# 6. Check if webhook is registered with Telegram
echo ""
echo "6. Checking webhook registration..."
WEBHOOK_REGISTERED=$(docker service logs school-tg-bot_bot --tail 100 2>&1 | grep -E "Webhook registered.*$TUNNEL_DOMAIN" | tail -1)
if [ -n "$WEBHOOK_REGISTERED" ]; then
    echo "   ✓ Webhook registered with Telegram"
    echo "   $WEBHOOK_REGISTERED"
else
    echo "   ⚠ WARNING: Could not find webhook registration log"
    echo "   Check logs: docker service logs school-tg-bot_bot | grep webhook"
fi

# 7. Test sending a request through tunnel to bot
echo ""
echo "7. Testing request through tunnel to bot..."
TEST_RESPONSE=$(curl -s --max-time 5 "$TUNNEL_URL/webhook" \
    -X POST \
    -H "Content-Type: application/json" \
    -H "X-Telegram-Bot-Api-Secret-Token: test" \
    -d '{"update_id":12345,"message":{"message_id":1,"from":{"id":123456,"is_bot":false,"first_name":"Test"},"chat":{"id":123456,"type":"private"},"date":1234567890,"text":"/start"}}' 2>&1)

if echo "$TEST_RESPONSE" | grep -q "OK\|200\|400\|404"; then
    echo "   ✓ Bot received request through tunnel"
    echo "   Response: $(echo "$TEST_RESPONSE" | head -1)"
else
    echo "   ⚠ Bot response unclear: $TEST_RESPONSE"
fi

# 8. Check bot logs for incoming requests
echo ""
echo "8. Checking bot logs for recent activity..."
RECENT_LOGS=$(docker service logs school-tg-bot_bot --tail 50 --since 2m 2>&1 | grep -E "(Incoming|Processing|webhook|update)" | wc -l)
if [ "$RECENT_LOGS" -gt 0 ]; then
    echo "   ✓ Found $RECENT_LOGS recent log entries"
else
    echo "   ⚠ No recent activity in logs"
fi

echo ""
echo "=== Test Summary ==="
echo ""
echo "Tunnel URL: $TUNNEL_URL"
echo "Bot Webhook Domain: $BOT_WEBHOOK"
echo ""
echo "To monitor incoming requests:"
echo "  docker service logs -f school-tg-bot_bot"
echo ""
echo "To test manually, send a message to your bot in Telegram"
echo ""

