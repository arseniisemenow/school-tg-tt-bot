# Local Development Docker Swarm Setup

This guide explains how to run the bot in Docker Swarm mode for local development.

## Prerequisites

- Docker installed and running
- `.env` file with required variables (or environment variables set)
- Optional: ngrok/cloudflared for HTTPS tunnel (if testing webhooks)

## Quick Start

### Option 1: Using the Deployment Script (Recommended)

```bash
# 1. Ensure you have a .env file with required variables
#    TELEGRAM_BOT_TOKEN=your_token
#    SCHOOL21_API_USERNAME=your_username
#    SCHOOL21_API_PASSWORD=your_password

# 2. Build the Docker image first
docker build -t school-tg-tt-bot:dev -f Dockerfile .

# 3. Set webhook configuration (optional for webhook mode)
export WEBHOOK_DOMAIN=your-ngrok-domain.ngrok.io  # If using ngrok
export WEBHOOK_SECRET_TOKEN=your-secret-token      # Optional but recommended

# 4. Run the deployment script
./scripts/deploy-dev.sh
```

The script will:
- Initialize Docker Swarm if needed
- Create secrets from your `.env` file
- Deploy the stack
- Show service status and logs

### Option 2: Manual Deployment

```bash
# 1. Initialize Docker Swarm (if not already initialized)
docker swarm init

# 2. Build the Docker image
docker build -t school-tg-tt-bot:dev -f Dockerfile .

# 3. Create secrets directory and files
mkdir -p secrets
echo "your-telegram-bot-token" > secrets/telegram_bot_token.txt
echo "your-username" > secrets/school21_api_username.txt
echo "your-password" > secrets/school21_api_password.txt

# 4. Set environment variables (optional)
export WEBHOOK_DOMAIN=your-domain.com
export WEBHOOK_SECRET_TOKEN=your-secret-token

# 5. Deploy the stack
docker stack deploy -c docker-stack.dev.yml school-tg-bot
```

## Configuration

### Environment Variables

The bot reads configuration from:
1. Environment variables (set before deployment)
2. `.env` file (if using the deployment script)
3. Config file: `config/config.dev.json`

**Required for webhook mode:**
- `WEBHOOK_DOMAIN` or `WEBHOOK_URL` - The public domain where Telegram will send updates
- `WEBHOOK_SECRET_TOKEN` (optional) - Secret token for validating webhook requests

**Required for bot operation:**
- `TELEGRAM_BOT_TOKEN` - Your Telegram bot token
- `SCHOOL21_API_USERNAME` - School21 API username (optional)
- `SCHOOL21_API_PASSWORD` - School21 API password (optional)

### Webhook Setup for Local Testing

Since Telegram requires HTTPS, you need a tunnel for local development:

**Using ngrok:**
```bash
# Install ngrok: https://ngrok.com/download
ngrok http 8443

# Copy the HTTPS URL (e.g., https://abc123.ngrok.io)
# Set it as WEBHOOK_DOMAIN
export WEBHOOK_DOMAIN=abc123.ngrok.io
```

**Using cloudflared:**
```bash
# Install cloudflared: https://developers.cloudflare.com/cloudflare-one/connections/connect-apps/install-and-setup/installation/
cloudflared tunnel --url http://localhost:8443

# Copy the HTTPS URL and set as WEBHOOK_DOMAIN
export WEBHOOK_DOMAIN=your-tunnel-url.trycloudflare.com
```

**Using localtunnel:**
```bash
# Install: npm install -g localtunnel
lt --port 8443

# Copy the HTTPS URL and set as WEBHOOK_DOMAIN
export WEBHOOK_DOMAIN=your-tunnel-url.loca.lt
```

## Managing the Stack

### View Service Status
```bash
docker stack services school-tg-bot
```

### View Service Logs
```bash
# All services
docker service logs -f school-tg-bot_bot

# Last 50 lines
docker service logs --tail 50 school-tg-bot_bot
```

### View Service Tasks
```bash
docker stack ps school-tg-bot
```

### Scale Services
```bash
# Scale bot to 2 replicas (for testing multi-instance)
docker service scale school-tg-bot_bot=2
```

### Update Stack
```bash
# After making changes to docker-stack.dev.yml
docker stack deploy -c docker-stack.dev.yml school-tg-bot
```

### Remove Stack
```bash
docker stack rm school-tg-bot
```

### Leave Swarm (if needed)
```bash
docker swarm leave --force
```

## Troubleshooting

### Services Not Starting

Check service status:
```bash
docker stack services school-tg-bot
docker stack ps school-tg-bot
```

Check logs:
```bash
docker service logs school-tg-bot_bot
docker service logs school-tg-bot_postgres
```

### Image Not Found

Make sure you've built the image:
```bash
docker build -t school-tg-tt-bot:dev -f Dockerfile .
```

### Secrets Not Found

The stack file expects secrets in `./secrets/` directory:
```bash
mkdir -p secrets
echo "your-token" > secrets/telegram_bot_token.txt
echo "your-username" > secrets/school21_api_username.txt
echo "your-password" > secrets/school21_api_password.txt
```

### Webhook Not Registering

1. Check that `WEBHOOK_DOMAIN` or `WEBHOOK_URL` is set
2. Verify the domain is accessible (test with curl)
3. Check bot logs for webhook registration errors:
   ```bash
   docker service logs school-tg-bot_bot | grep -i webhook
   ```

### Database Connection Issues

Check PostgreSQL is running:
```bash
docker service ps school-tg-bot_postgres
docker service logs school-tg-bot_postgres
```

Test connection:
```bash
docker exec -it $(docker ps -q -f name=postgres) psql -U dev_user -d school_tg_bot_dev
```

## Development Workflow

1. **Make code changes**
2. **Rebuild the image:**
   ```bash
   docker build -t school-tg-tt-bot:dev -f Dockerfile .
   ```
3. **Update the stack:**
   ```bash
   docker stack deploy -c docker-stack.dev.yml school-tg-bot
   ```
4. **Check logs to verify:**
   ```bash
   docker service logs -f school-tg-bot_bot
   ```

## Differences from Docker Compose

- **Swarm mode**: Uses Docker Swarm instead of Docker Compose
- **Overlay network**: Services communicate via overlay network
- **Secrets**: Uses Docker secrets (file-based for dev)
- **No build in stack**: Must build image before deployment
- **Service discovery**: Uses service names for DNS resolution

## Next Steps

- For production deployment, see `docs/DEPLOYMENT-PROD.md` (if exists) or `README.md`
- For development with Docker Compose, see `README-DEV.md`

