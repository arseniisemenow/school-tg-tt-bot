# WEBHOOK_DOMAIN vs WEBHOOK_URL Format Guide

## Overview

You can use **either** `WEBHOOK_DOMAIN` **or** `WEBHOOK_URL` (not both). The bot constructs the final webhook URL differently depending on which one you use.

## Format Examples

### WEBHOOK_DOMAIN (Domain Only)

**Format:** Just the domain name, **without** `https://` or path

**Examples:**
```bash
# Development tunnels
export WEBHOOK_DOMAIN=abc123.ngrok.io
export WEBHOOK_DOMAIN=xyz789.trycloudflare.com
export WEBHOOK_DOMAIN=random-name.loca.lt

# Production domains
export WEBHOOK_DOMAIN=bot.example.com
export WEBHOOK_DOMAIN=api.mycompany.com
```

**How it works:**
- Bot constructs: `https://${WEBHOOK_DOMAIN}${path}`
- Uses `telegram.webhook.path` from config (default: `/webhook`)
- Final URL: `https://abc123.ngrok.io/webhook`

**When to use:**
- ✅ Using default path (`/webhook`)
- ✅ Simpler, less configuration
- ✅ Recommended for most cases

### WEBHOOK_URL (Full URL)

**Format:** Complete HTTPS URL including path

**Examples:**
```bash
# With default path
export WEBHOOK_URL=https://abc123.ngrok.io/webhook

# With custom path
export WEBHOOK_URL=https://bot.example.com/api/telegram/webhook

# With query parameters (if needed)
export WEBHOOK_URL=https://bot.example.com/webhook?token=secret
```

**How it works:**
- Used **as-is**, no construction
- Takes precedence over `WEBHOOK_DOMAIN`
- Ignores `telegram.webhook.path` config

**When to use:**
- ✅ Custom path (not `/webhook`)
- ✅ Need query parameters
- ✅ Full control over URL format

## What It Depends On

### 1. **Tunnel Service** (Development)

The domain format depends on which tunnel service you use:

| Service | Domain Format | Example |
|---------|--------------|---------|
| **ngrok** | `*.ngrok.io` or `*.ngrok-free.app` | `abc123.ngrok.io` |
| **cloudflared** | `*.trycloudflare.com` | `xyz789.trycloudflare.com` |
| **localtunnel** | `*.loca.lt` | `random-name.loca.lt` |
| **serveo** | `*.serveo.net` | `custom-name.serveo.net` |

**How to get it:**
```bash
# ngrok
ngrok http 8443
# Output: Forwarding  https://abc123.ngrok.io -> http://localhost:8443

# cloudflared
cloudflared tunnel --url http://localhost:8443
# Output: https://xyz789.trycloudflare.com

# localtunnel
lt --port 8443
# Output: your url is: https://random-name.loca.lt
```

### 2. **Production Domain** (Production)

For production, use your own domain:

```bash
export WEBHOOK_DOMAIN=bot.yourcompany.com
# or
export WEBHOOK_URL=https://bot.yourcompany.com/webhook
```

**Requirements:**
- ✅ Domain must have valid SSL certificate (HTTPS required)
- ✅ Domain must be publicly accessible
- ✅ Port 443 (HTTPS) must be open
- ✅ Reverse proxy (nginx/traefik) configured

### 3. **Path Configuration**

The path comes from `config/config.dev.json`:

```json
{
  "telegram": {
    "webhook": {
      "path": "/webhook"  // Default path
    }
  }
}
```

**With WEBHOOK_DOMAIN:**
- Uses path from config: `/webhook`
- Final: `https://domain.com/webhook`

**With WEBHOOK_URL:**
- Ignores config path
- Uses path from URL: `https://domain.com/custom/path`

## Complete Examples

### Example 1: Development with ngrok (WEBHOOK_DOMAIN)

```bash
# 1. Start tunnel
ngrok http 8443
# Output: Forwarding  https://abc123.ngrok.io -> http://localhost:8443

# 2. Set domain (just the domain, no https:// or path)
export WEBHOOK_DOMAIN=abc123.ngrok.io

# 3. Deploy
./scripts/deploy-dev.sh

# Bot will register: https://abc123.ngrok.io/webhook
```

### Example 2: Development with cloudflared (WEBHOOK_DOMAIN)

```bash
# 1. Start tunnel
cloudflared tunnel --url http://localhost:8443
# Output: https://xyz789.trycloudflare.com

# 2. Set domain
export WEBHOOK_DOMAIN=xyz789.trycloudflare.com

# 3. Deploy
./scripts/deploy-dev.sh

# Bot will register: https://xyz789.trycloudflare.com/webhook
```

### Example 3: Custom path (WEBHOOK_URL)

```bash
# Use full URL with custom path
export WEBHOOK_URL=https://bot.example.com/api/v1/telegram/webhook

# Deploy
./scripts/deploy-dev.sh

# Bot will register: https://bot.example.com/api/v1/telegram/webhook
```

### Example 4: Production (WEBHOOK_DOMAIN)

```bash
# Production domain
export WEBHOOK_DOMAIN=bot.yourcompany.com

# Deploy
./scripts/deploy-prod.sh

# Bot will register: https://bot.yourcompany.com/webhook
```

## Important Notes

### ✅ DO:

- Use **domain only** for `WEBHOOK_DOMAIN`: `abc123.ngrok.io`
- Use **full URL** for `WEBHOOK_URL`: `https://abc123.ngrok.io/webhook`
- Ensure domain is **publicly accessible**
- Use **HTTPS** (required by Telegram)
- Test domain accessibility: `curl https://your-domain.com/webhook`

### ❌ DON'T:

- Don't include `https://` in `WEBHOOK_DOMAIN`: ❌ `https://abc123.ngrok.io`
- Don't include path in `WEBHOOK_DOMAIN`: ❌ `abc123.ngrok.io/webhook`
- Don't use `http://` (Telegram requires HTTPS): ❌ `http://abc123.ngrok.io`
- Don't use localhost: ❌ `localhost:8443` (Telegram can't reach it)

## Verification

### Check what URL will be used:

```bash
# If using WEBHOOK_DOMAIN
echo "https://${WEBHOOK_DOMAIN}/webhook"

# If using WEBHOOK_URL
echo "${WEBHOOK_URL}"
```

### Test domain accessibility:

```bash
# Test if domain is reachable
curl -I https://your-domain.com/webhook

# Should return HTTP status (200, 404, 405 are OK - means domain works)
# 502/503 means domain not reachable
```

### Check bot logs:

```bash
docker service logs school-tg-bot_bot | grep -i webhook

# Look for:
# "Webhook registered with Telegram: https://..."
```

## Troubleshooting

### "Domain not accessible"

**Problem:** Telegram can't reach your webhook URL

**Solutions:**
1. Verify tunnel is running: `ps aux | grep ngrok` (or cloudflared)
2. Test domain: `curl https://your-domain.com/webhook`
3. Check port mapping: `docker stack services school-tg-bot`
4. Ensure port 8443 is exposed: Should show `8443:8443`

### "Invalid webhook URL"

**Problem:** Wrong format in environment variable

**Check:**
```bash
# Wrong
export WEBHOOK_DOMAIN=https://abc123.ngrok.io  # ❌ Has https://
export WEBHOOK_DOMAIN=abc123.ngrok.io/webhook  # ❌ Has path

# Correct
export WEBHOOK_DOMAIN=abc123.ngrok.io  # ✅ Domain only
```

### "Webhook path mismatch"

**Problem:** Path in URL doesn't match server path

**Solution:**
- Use `WEBHOOK_URL` with exact path, or
- Ensure `WEBHOOK_DOMAIN` + config path matches

## Summary

| Variable | Format | Example | Final URL |
|----------|--------|---------|-----------|
| `WEBHOOK_DOMAIN` | Domain only | `abc123.ngrok.io` | `https://abc123.ngrok.io/webhook` |
| `WEBHOOK_URL` | Full URL | `https://abc123.ngrok.io/webhook` | `https://abc123.ngrok.io/webhook` |

**Recommendation:** Use `WEBHOOK_DOMAIN` for simplicity (unless you need a custom path).

