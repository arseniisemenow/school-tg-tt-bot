# Development Workflow

## Overview

The development setup uses separate containers for building and running:

1. **Build container** (`bot-builder`): For building the project with all build tools
2. **Runtime containers**: PostgreSQL and the bot application

## Quick Start

### Step 1: Install Dependencies (Only when needed)

The dependency installation script (`scripts/install-deps.sh`) is smart - it only runs `conan install` when:
- Dependencies are not installed yet
- `conanfile.txt` has been modified

```bash
# Start build container
docker compose --profile build up -d bot-builder

# Install dependencies (only runs if needed)
docker compose --profile build exec bot-builder /app/scripts/install-deps.sh
```

**Note**: The script will automatically skip installation if dependencies are already up to date!

### Step 2: Build the Project

After dependencies are installed, build the project:

```bash
# Build the project
docker compose --profile build exec bot-builder /app/scripts/build.sh
```

### Step 3: Run the Application

After building, start the runtime containers:

```bash
# Start postgres and bot (runtime)
docker compose up -d postgres bot

# Check logs
docker compose logs -f bot

# Check status
docker compose ps
```

### Step 3.5: Seed the Database (development data)

With postgres running, load sample dev data:

```bash
docker compose up -d postgres
docker compose exec postgres psql -U dev_user -d school_tg_bot_dev -f /docker-entrypoint-initdb.d/seed.sql
```

The seed script truncates tables and repopulates them, so you can rerun it anytime to reset local data.

### Step 4: Rebuild After Changes

When you make code changes:

```bash
# Rebuild in the builder container (dependencies won't be reinstalled)
docker compose --profile build exec bot-builder /app/scripts/build.sh

# Restart the bot container to run the new binary
docker compose restart bot
```

## Scripts

### `scripts/install-deps.sh`
- **Purpose**: Install Conan dependencies
- **Smart**: Only runs when `conanfile.txt` changes or dependencies are missing
- **Usage**: `docker compose --profile build exec bot-builder /app/scripts/install-deps.sh`

### `scripts/build.sh`
- **Purpose**: Build the C++ application
- **Requires**: Dependencies must be installed first
- **Usage**: `docker compose --profile build exec bot-builder /app/scripts/build.sh`

## Container Details

### Build Container (`bot-builder`)
- **Purpose**: Build the C++ application
- **Volumes**: 
  - Source code: `.:/app`
  - Build artifacts: `bot_build:/app/build`
  - Conan cache: `conan_cache:/root/.conan2`
- **Profile**: `build` (only starts with `--profile build`)

### Runtime Container (`bot`)
- **Purpose**: Run the built application
- **Volumes**: 
  - Source code: `.:/app` (for config files)
  - Build artifacts: `bot_build:/app/build` (shared with builder)
- **Command**: Runs `/app/build/school_tg_tt_bot`

### PostgreSQL Container (`postgres`)
- **Purpose**: Database for the application
- **Port**: `5433:5432` (host:container)
- **Database**: `school_tg_bot_dev`

## Environment Variables

Create a `.env` file in the project root:

```bash
TELEGRAM_BOT_TOKEN=your_bot_token_here
SCHOOL21_API_USERNAME=your_username
SCHOOL21_API_PASSWORD=your_password
```

## Docker Swarm Deployment (Development)

For local development with Docker Swarm (single-node):

```bash
# Initialize Swarm (if not already initialized)
docker swarm init

# Set environment variables
export WEBHOOK_DOMAIN=your-ngrok-domain.ngrok.io  # or use ngrok/tunnel
export WEBHOOK_SECRET_TOKEN=your-secret-token
export TELEGRAM_BOT_TOKEN=your-bot-token
export SCHOOL21_API_USERNAME=your-username
export SCHOOL21_API_PASSWORD=your-password

# Deploy stack
./scripts/deploy-dev.sh
```

**For local testing with HTTPS tunnel:**
- Use ngrok: `ngrok http 8443`
- Use cloudflared: `cloudflared tunnel --url http://localhost:8443`
- Set `WEBHOOK_DOMAIN` to the tunnel domain

**View logs:**
```bash
docker service logs -f school-tg-bot_bot
```

**Remove stack:**
```bash
docker stack rm school-tg-bot
```

## Useful Commands

```bash
# Install dependencies (only if needed)
docker compose --profile build exec bot-builder /app/scripts/install-deps.sh

# Build the project
docker compose --profile build exec bot-builder /app/scripts/build.sh

# View bot logs
docker compose logs -f bot

# View postgres logs
docker compose logs -f postgres

# Stop everything
docker compose down

# Stop and remove volumes (clean slate)
docker compose down -v

# Rebuild containers
docker compose build

# Enter build container shell
docker compose --profile build exec bot-builder bash

# Enter runtime container shell
docker compose exec bot bash
```

## Troubleshooting

### Build fails with "OpenSSL not found"
The build container should have OpenSSL installed. If not:
```bash
docker compose exec bot-builder apt-get update && apt-get install -y libssl-dev
```

### Binary not found when starting bot
Make sure you've built the project first:
```bash
docker compose --profile build exec bot-builder /app/scripts/build.sh
```

### Database connection errors
Check that postgres is healthy:
```bash
docker compose ps postgres
docker compose logs postgres
```

### Dependencies keep reinstalling
The `install-deps.sh` script checks if `conanfile.txt` has changed. If you're seeing repeated installs, check:
- The `build/conan.lock` file exists
- The `build/conan_toolchain.cmake` file exists
- `conanfile.txt` hasn't been modified
