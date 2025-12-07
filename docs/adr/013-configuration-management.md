# ADR-013: Configuration Management

## Status
Accepted

## Context
The bot requires configuration for:
- Sensitive data (API keys, passwords, tokens) - must be secure
- Non-sensitive data (URLs, timeouts, limits) - can be in config file
- Environment-specific settings (dev, staging, production)
- Runtime configuration (some settings may need reloading)

Key requirements:
- Environment variables for sensitive data
- Config file for non-sensitive data
- Secret management
- Environment-specific configurations
- Config reloading strategy (if needed)

## Decision
We will implement a two-tier configuration management strategy:

### Environment Variables for Sensitive Data

#### Sensitive Data Categories
- **API Credentials**:
  - `TELEGRAM_BOT_TOKEN`: Telegram bot token
  - `SCHOOL21_API_USERNAME`: School21 API username
  - `SCHOOL21_API_PASSWORD`: School21 API password
- **Database Credentials**:
  - `DATABASE_URL`: PostgreSQL connection string (or separate vars)
  - `POSTGRES_HOST`: Database host
  - `POSTGRES_PORT`: Database port
  - `POSTGRES_DB`: Database name
  - `POSTGRES_USER`: Database user
  - `POSTGRES_PASSWORD`: Database password
- **Backup Storage**:
  - `AWS_ACCESS_KEY_ID`: AWS S3 access key (if using S3)
  - `AWS_SECRET_ACCESS_KEY`: AWS secret key
  - `AWS_REGION`: AWS region
  - `S3_BACKUP_BUCKET`: S3 bucket name
- **Observability**:
  - `OTEL_EXPORTER_OTLP_ENDPOINT`: OTEL Collector endpoint
  - `OTEL_SERVICE_NAME`: Service name for observability

#### Environment Variable Naming
- **Convention**: UPPER_SNAKE_CASE
- **Prefix**: Use prefixes for grouping (e.g., `SCHOOL21_`, `POSTGRES_`, `AWS_`)
- **Validation**: Validate required environment variables on startup
- **Documentation**: Document all required environment variables

#### Secret Management
- **Development**: Use `.env` file (not committed to git)
- **Production**: 
  - Use container orchestration secrets (Docker secrets, Kubernetes secrets)
  - Or environment variables set by deployment system
  - Never commit secrets to repository
- **Rotation**: Support secret rotation without code changes

### Config File for Non-Sensitive Data

#### Config File Format
- **Format**: JSON (chosen for simplicity and native C++ support)
- **Alternative Considered**: YAML, TOML
- **Decision**: JSON is simple, widely supported, easy to parse in C++
- **Location**: `config/config.json` (configurable via environment variable)

#### Config File Structure
```json
{
  "database": {
    "connection_pool": {
      "min_size": 2,
      "max_size": 10,
      "idle_timeout_seconds": 300,
      "max_lifetime_seconds": 3600
    },
    "query_timeout_seconds": 30
  },
  "telegram": {
    "webhook": {
      "enabled": true,
      "port": 8443,
      "path": "/webhook"
    },
    "polling": {
      "enabled": false,
      "timeout_seconds": 30
    },
    "rate_limit": {
      "per_user_per_minute": 10,
      "per_group_per_minute": 100
    }
  },
  "school21": {
    "api": {
      "base_url": "https://platform.21-school.ru/services/21-school/api/v1",
      "timeout_seconds": 10,
      "max_retries": 3
    },
    "verification": {
      "cache_ttl_success_hours": 24,
      "cache_ttl_failure_hours": 1,
      "auto_delete_delay_minutes": 5
    }
  },
  "backup": {
    "schedule": "0 2 * * *",
    "local_path": "/var/backups/school-tg-bot",
    "retention_days": 14,
    "remote": {
      "provider": "s3",
      "bucket": "school-tg-bot-backups"
    }
  },
  "observability": {
    "log_level": "INFO",
    "metrics_export_interval_seconds": 10,
    "trace_sampling_rate": 0.1
  },
  "abuse_prevention": {
    "spam_threshold": 5,
    "spam_window_seconds": 10,
    "match_spam_limit_per_hour": 10,
    "match_spam_limit_per_day": 5
  },
  "elo": {
    "k_factor": 32,
    "initial_elo": 1500,
    "max_elo": 10000
  }
}
```

#### Config File Location
- **Default**: `config/config.json` (relative to executable)
- **Override**: `CONFIG_FILE` environment variable
- **Search Paths**: 
  1. Path from `CONFIG_FILE` env var
  2. `./config/config.json`
  3. `/etc/school-tg-bot/config.json`
  4. `~/.config/school-tg-bot/config.json`

#### Config File Validation
- **Schema Validation**: Validate config file against schema
- **Required Fields**: Check all required fields present
- **Type Validation**: Validate field types (int, string, bool, etc.)
- **Range Validation**: Validate numeric ranges (e.g., port 1-65535)
- **Startup Failure**: Fail fast on invalid config

### Environment-Specific Configurations

#### Development
- **Config File**: `config/config.dev.json`
- **Environment Variables**: `.env` file (not committed)
- **Settings**:
  - Lower log levels (DEBUG)
  - Test database
  - Mock School21 API (optional)
  - Simpler observability (console output)

#### Staging
- **Config File**: `config/config.staging.json`
- **Environment Variables**: Set by deployment
- **Settings**:
  - Production-like but with test data
  - Real School21 API
  - Full observability

#### Production
- **Config File**: `config/config.prod.json`
- **Environment Variables**: Set by deployment (secrets)
- **Settings**:
  - Production database
  - Real School21 API
  - Full observability (OTEL Collector)
  - Optimized performance settings

#### Environment Detection
- **Method**: `ENVIRONMENT` environment variable
- **Values**: `development`, `staging`, `production`
- **Default**: `development` if not set
- **Config Selection**: Load `config/config.{environment}.json`

### Config Reloading Strategy

#### Reloadable Settings
- **Non-Reloadable** (require restart):
  - Database connection settings
  - Port bindings
  - File paths
- **Reloadable** (can change at runtime):
  - Log levels
  - Rate limits
  - Cache TTLs
  - Feature flags

#### Reload Mechanism
- **Signal-Based**: SIGHUP triggers config reload
- **API Endpoint**: `/admin/reload-config` (admin only, if webhook mode)
- **Validation**: Validate new config before applying
- **Rollback**: Keep old config if new config invalid
- **Logging**: Log config reload events

#### Implementation
- **Config Watcher**: Optional file watcher for config file changes
- **Atomic Updates**: Update config atomically (no partial updates)
- **Thread Safety**: Ensure thread-safe config access

### Configuration Access

#### Configuration API
- **Singleton Pattern**: Single config instance
- **Thread-Safe**: Thread-safe access (mutex-protected)
- **Type-Safe**: Type-safe getters (getInt, getString, getBool)
- **Default Values**: Provide defaults for optional settings
- **Nested Access**: Support nested config access (e.g., `database.connection_pool.max_size`)

#### Example Usage
```cpp
Config& config = Config::getInstance();
int maxPoolSize = config.getInt("database.connection_pool.max_size", 10);
std::string botToken = std::getenv("TELEGRAM_BOT_TOKEN");
```

### Configuration Documentation

#### Config Schema
- **Schema File**: `config/config.schema.json` (JSON Schema)
- **Purpose**: Validate config files, document structure
- **Tooling**: Use JSON Schema validator

#### Documentation
- **README**: Document all configuration options
- **Examples**: Provide example config files for each environment
- **Defaults**: Document default values
- **Environment Variables**: Document all required environment variables

### Security Considerations

#### Secret Handling
- **Never Log**: Never log sensitive values (passwords, tokens)
- **Masking**: Mask secrets in logs (show only first/last few characters)
- **Memory**: Clear sensitive data from memory when possible
- **File Permissions**: Restrict config file permissions (600)

#### Config File Security
- **Permissions**: Config file readable only by application user
- **Validation**: Validate config to prevent injection attacks
- **No Secrets**: Never put secrets in config file (use env vars)

## Consequences

### Positive
- **Security**: Sensitive data in environment variables (not in files)
- **Flexibility**: Config file allows easy changes without code changes
- **Environment Support**: Easy to support multiple environments
- **Validation**: Config validation catches errors early
- **Documentation**: Config file serves as documentation

### Negative
- **Complexity**: Two-tier system adds complexity
- **Maintenance**: Need to maintain config files and schema
- **Parsing**: Need JSON parsing library
- **Validation**: Need config validation logic

### Neutral
- **Performance**: Config loading is one-time (negligible overhead)
- **File Format**: JSON is simple but less human-friendly than YAML

## Alternatives Considered

### All in Environment Variables
- **Rejected**: Too many environment variables, harder to manage

### All in Config File
- **Rejected**: Secrets in files are security risk

### YAML Config File
- **Considered**: YAML is more human-friendly
- **Rejected**: Requires YAML library, JSON is simpler

### TOML Config File
- **Considered**: TOML is good for configs
- **Rejected**: Less common, JSON is more standard

### No Config Reloading
- **Considered**: Simpler, require restart for changes
- **Decision**: Support reloading for operational flexibility

### External Config Service (etcd, Consul)
- **Rejected**: Overkill for this scale, adds dependency

### Config in Database
- **Rejected**: Circular dependency (need config to connect to database)






