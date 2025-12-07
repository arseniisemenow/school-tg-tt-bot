# Telegram Webhook Setup Guide

## Overview

The bot supports **automatic webhook registration** with Telegram. When you deploy with `WEBHOOK_DOMAIN` or `WEBHOOK_URL` set, the bot will automatically register the webhook on startup (no manual API calls needed).

## Quick Start

### Option 1: Automatic Setup (Recommended)

The deployment script can automatically set up a tunnel:

```bash
./scripts/deploy-dev.sh
# Choose option 1 when prompted for tunnel setup
```

### Option 2: Manual Setup

1. **Set up a tunnel** (choose one):

   **Using ngrok:**
   ```bash
   # Install ngrok: https://ngrok.com/download
   ngrok http 8443
   # Copy the HTTPS URL (e.g., abc123.ngrok.io)
   ```

   **Using cloudflared:**
   ```bash
   # Install cloudflared
   cloudflared tunnel --url http://localhost:8443
   # Copy the HTTPS URL (e.g., abc123.trycloudflare.com)
   ```

   **Using localtunnel:**
   ```bash
   # Install: npm install -g localtunnel
   lt --port 8443
   # Copy the HTTPS URL
   ```

2. **Set environment variables:**
   ```bash
   export WEBHOOK_DOMAIN=your-tunnel-domain.com
   export WEBHOOK_SECRET_TOKEN=$(openssl rand -hex 32)  # Optional but recommended
   ```

3. **Deploy:**
   ```bash
   ./scripts/deploy-dev.sh
   ```

## How It Works

1. **Bot starts** → Reads `WEBHOOK_DOMAIN` or `WEBHOOK_URL` from environment
2. **Constructs webhook URL** → `https://${WEBHOOK_DOMAIN}/webhook`
3. **Starts webhook server** → Listens on port 8443
4. **Registers with Telegram** → Automatically calls `setWebhook()` API
5. **Receives updates** → Telegram sends updates to your webhook URL

## Configuration

### Environment Variables

- `WEBHOOK_DOMAIN` - Domain name (e.g., `abc123.ngrok.io`)
- `WEBHOOK_URL` - Full URL (e.g., `https://abc123.ngrok.io/webhook`)
- `WEBHOOK_SECRET_TOKEN` - Secret token for request validation (optional but recommended)
- `WEBHOOK_REGISTRAR` - Set to `true` to register webhook (default: `true`)

### Config File

In `config/config.dev.json`:
```json
{
  "telegram": {
    "webhook": {
      "enabled": true,
      "port": 8443,
      "path": "/webhook"
    }
  }
}
```

## Verification

### Check Webhook Registration

```bash
# View bot logs
docker service logs school-tg-bot_bot | grep -i webhook

# Look for:
# "Webhook registered with Telegram: https://..."
```

### Test Webhook

Send a message to your bot in Telegram. Check logs:
```bash
docker service logs -f school-tg-bot_bot
```

You should see incoming updates being processed.

### Manual Verification

You can also check webhook status via Telegram API:
```bash
curl "https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getWebhookInfo"
```

## Troubleshooting

### Webhook Not Registering

1. **Check environment variables:**
   ```bash
   docker service inspect school-tg-bot_bot | grep -A 20 "Env"
   ```

2. **Check bot logs:**
   ```bash
   docker service logs school-tg-bot_bot | grep -i error
   ```

3. **Verify tunnel is working:**
   ```bash
   curl https://your-tunnel-domain.com/webhook
   # Should return 404 or 405 (not 502/503)
   ```

4. **Check port 8443 is exposed:**
   ```bash
   docker stack services school-tg-bot
   # Should show port mapping: 8443:8443
   ```

### Webhook Receives Updates But Bot Doesn't Respond

1. Check bot logs for processing errors
2. Verify `WEBHOOK_SECRET_TOKEN` matches (if using)
3. Check database connection

### Tunnel Disconnects

- **ngrok**: Free tier has session limits. Use paid plan or restart tunnel
- **cloudflared**: More stable, recommended for development
- **localtunnel**: May disconnect after inactivity

## Production Setup

For production, use a proper domain with HTTPS:

1. Set up reverse proxy (nginx/traefik) with SSL
2. Point domain to your server
3. Set `WEBHOOK_DOMAIN=your-domain.com`
4. Deploy with production config

See `docs/DEPLOYMENT-DEV.md` for more details.

