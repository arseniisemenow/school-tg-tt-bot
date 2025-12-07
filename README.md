# School Telegram Table Tennis Bot

A Telegram bot for managing ELO-based rankings and match tracking for School21 students.

## Features

- **Match Registration**: Register table tennis matches with `/match @player1 @player2 score1 score2`
- **ELO Ranking System**: Automatic ELO calculation and ranking display
- **School21 Verification**: Verify students via School21 API
- **Topic-Based Organization**: Support for forum groups with topic-based command routing
- **Undo Operations**: Undo recent matches with permission checks
- **Multi-Group Support**: Support multiple Telegram groups with isolated ELO ratings

## Architecture

This project follows the Architecture Decision Records (ADRs) in `docs/adr/`. Key decisions:

- **Technology Stack**: C++20, tgbotxx, libpqxx, OpenTelemetry, PostgreSQL
- **Database**: PostgreSQL with Flyway migrations
- **Configuration**: JSON config files + environment variables
- **Observability**: OpenTelemetry for logging, metrics, and tracing

## Project Structure

```
.
├── src/                    # Source files
│   ├── bot/               # Telegram bot implementation
│   ├── config/            # Configuration management
│   ├── database/          # Database connection pool
│   ├── models/            # Data models
│   ├── repositories/      # Database repositories (TODO)
│   ├── commands/          # Command handlers (TODO)
│   ├── school21/          # School21 API integration
│   ├── observability/     # Logging, metrics, tracing
│   └── utils/             # Utility functions
├── include/               # Header files (mirrors src structure)
├── config/                # Configuration files
├── migrations/            # Database migrations (Flyway)
│   └── sql/
├── docs/                  # Documentation
│   └── adr/              # Architecture Decision Records
└── tests/                 # Tests (TODO)
```

## Setup

### Prerequisites

- C++20 compiler (GCC 10+, Clang 12+)
- CMake 4.0+
- Conan 2.x
- PostgreSQL 12+
- libcurl (for School21 API)

### Build

1. Install dependencies via Conan:
```bash
conan install . --output-folder=build --build=missing
```

2. Configure and build:
```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

### Configuration

1. Copy `.env.example` to `.env` and fill in values:
```bash
cp .env.example .env
# Edit .env with your values
```

2. Configure database:
```bash
# Create database
createdb school_tg_bot

# Run migrations (using Flyway)
flyway -url=jdbc:postgresql://localhost:5432/school_tg_bot \
       -user=postgres \
       -password=your_password \
       -locations=filesystem:migrations/sql \
       migrate
```

3. Run the bot:
```bash
./build/school_tg_tt_bot
```

## Deployment

The bot supports two deployment modes:

### Webhook Mode (Production)

The bot uses webhooks to receive updates from Telegram. This is the recommended mode for production deployments.

**Requirements:**
- HTTPS endpoint (Telegram requirement)
- Public domain or tunnel (ngrok, cloudflared, etc.)
- Secret token for webhook validation (optional but recommended)

**Docker Swarm Deployment:**

1. **Development (single-node):**
   ```bash
   # Initialize Swarm (if not already initialized)
   docker swarm init
   
   # Set environment variables
   export WEBHOOK_DOMAIN=your-domain.com  # or use WEBHOOK_URL
   export WEBHOOK_SECRET_TOKEN=your-secret-token
   
   # Deploy stack
   ./scripts/deploy-dev.sh
   ```

2. **Production (multi-node):**
   ```bash
   # On manager node: Initialize Swarm
   docker swarm init
   
   # On worker nodes: Join Swarm
   docker swarm join --token <token> <manager-ip>
   
   # Create Docker secrets
   echo "your-token" | docker secret create telegram_bot_token -
   echo "your-secret" | docker secret create webhook_secret_token -
   # ... create other secrets
   
   # Set environment variables
   export WEBHOOK_DOMAIN=your-domain.com
   
   # Deploy stack
   ./scripts/deploy-prod.sh
   ```

**Webhook Configuration:**
- Set `telegram.webhook.enabled: true` in config file
- Set `telegram.webhook.port` (default: 8443)
- Set `telegram.webhook.path` (default: "/webhook")
- Provide `WEBHOOK_DOMAIN` or `WEBHOOK_URL` environment variable
- Provide `WEBHOOK_SECRET_TOKEN` for request validation

**Multi-Instance Setup:**
- Only one instance should register webhook (`WEBHOOK_REGISTRAR=true`)
- All instances can receive webhook requests via Swarm routing mesh
- Use placement constraints or node labels to designate registrar instance

See `docker-stack.dev.yml` and `docker-stack.prod.yml` for stack configurations.

### Polling Mode (Development/Fallback)

For local development or as a fallback, the bot can use polling:

```json
{
  "telegram": {
    "webhook": { "enabled": false },
    "polling": { "enabled": true }
  }
}
```

## Development

### Current Status

The project structure is initialized with:
- ✅ Configuration management
- ✅ Database connection pool
- ✅ Basic bot structure with command handlers (stubs)
- ✅ School21 API client (basic implementation)
- ✅ Observability (logging)
- ✅ ELO calculator
- ✅ Database schema migrations
- ✅ Webhook support with Docker Swarm deployment
- ⚠️ Command handlers need full implementation
- ⚠️ Repositories need implementation
- ⚠️ Full School21 integration needs completion
- ⚠️ OpenTelemetry integration needs completion

### Next Steps

1. Implement command handlers (match, ranking, id, undo, etc.)
2. Implement database repositories
3. Complete School21 API integration
4. Add OpenTelemetry metrics and tracing
5. Add tests

## Configuration

See `config/config.dev.json` and `config/config.prod.json` for configuration options.

Environment variables are used for sensitive data (see `.env.example`).

## License

[Add your license here]

