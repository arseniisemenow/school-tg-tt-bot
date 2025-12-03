# ADR-014: Development Workflow and Local Setup

## Status
Accepted

## Context
Developers need a consistent, easy-to-setup local development environment that:
- Matches production environment as closely as possible
- Is easy to set up for new developers
- Supports all required services (PostgreSQL, bot, observability)
- Enables database migrations and seeding
- Supports testing workflows

Key requirements:
- Docker Compose for local development
- Local PostgreSQL setup (in docker)
- Bot containerization (in docker)
- Metrics/observability stack
- Database seeding for development
- Migration testing
- Easy onboarding for new developers

## Decision
We will implement a comprehensive local development setup using Docker Compose:

### Docker Compose for Local Development

#### Services
- **bot**: The main application container
- **postgres**: PostgreSQL database
- **otel-collector**: OpenTelemetry Collector (optional, for full observability)
- **Optional Services**:
  - `prometheus`: Metrics storage (if not using OTEL Collector)
  - `grafana`: Metrics visualization (optional)
  - `jaeger`: Distributed tracing (if not using OTEL Collector)

#### Docker Compose File Structure
```yaml
version: '3.8'
services:
  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: school_tg_bot_dev
      POSTGRES_USER: dev_user
      POSTGRES_PASSWORD: dev_password
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./migrations:/docker-entrypoint-initdb.d
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U dev_user"]
      interval: 5s
      timeout: 5s
      retries: 5

  bot:
    build:
      context: .
      dockerfile: Dockerfile.dev
    environment:
      - ENVIRONMENT=development
      - DATABASE_URL=postgresql://dev_user:dev_password@postgres:5432/school_tg_bot_dev
      - TELEGRAM_BOT_TOKEN=${TELEGRAM_BOT_TOKEN}
      - SCHOOL21_API_USERNAME=${SCHOOL21_API_USERNAME}
      - SCHOOL21_API_PASSWORD=${SCHOOL21_API_PASSWORD}
    volumes:
      - .:/app
      - bot_build:/build
    depends_on:
      postgres:
        condition: service_healthy
    command: ./scripts/dev.sh

  otel-collector:
    image: otel/opentelemetry-collector:latest
    volumes:
      - ./otel-collector-config.dev.yaml:/etc/otel-collector-config.yaml
    command: ["--config=/etc/otel-collector-config.yaml"]
    ports:
      - "4317:4317"  # OTLP gRPC
      - "4318:4318"  # OTLP HTTP

volumes:
  postgres_data:
  bot_build:
```

#### Development Dockerfile
- **Base Image**: Use development-friendly base (e.g., `conanio/gcc12`)
- **Hot Reload**: Mount source code as volume for hot reload
- **Dependencies**: Install all build dependencies
- **Development Tools**: Include debugging tools, development libraries

### Local PostgreSQL Setup

#### Database Configuration
- **Database Name**: `school_tg_bot_dev`
- **User**: `dev_user`
- **Password**: `dev_password` (development only)
- **Port**: 5432 (exposed to host for direct access)
- **Data Persistence**: Docker volume for data persistence

#### Migration Execution
- **On Startup**: Run Flyway migrations automatically
- **Manual**: `docker-compose exec bot flyway migrate`
- **Migration Files**: Located in `migrations/` directory
- **Version Control**: Migrations versioned and tracked

#### Database Access
- **From Host**: `psql -h localhost -U dev_user -d school_tg_bot_dev`
- **From Container**: `docker-compose exec postgres psql -U dev_user -d school_tg_bot_dev`
- **GUI Tools**: Connect to `localhost:5432` with GUI tools (DBeaver, pgAdmin)

### Database Seeding

#### Seed Scripts
- **Location**: `scripts/seed/` directory
- **Format**: SQL scripts or Flyway migrations
- **Naming**: `seed_<description>.sql`
- **Execution**: Manual or via script

#### Seed Data
- **Development Seeds**:
  - Sample groups
  - Sample players (with various ELOs)
  - Sample matches
  - Sample ELO history
- **Test Seeds**:
  - Minimal data for testing
  - Edge cases
  - Boundary conditions

#### Seeding Scripts
- **Script**: `scripts/seed_dev.sh`
- **Purpose**: Populate database with development data
- **Usage**: `./scripts/seed_dev.sh`
- **Idempotent**: Can run multiple times safely

#### Seed Data Examples
```sql
-- Sample groups
INSERT INTO groups (telegram_group_id, name) VALUES 
  (123456789, 'Test Group 1'),
  (987654321, 'Test Group 2');

-- Sample players
INSERT INTO players (telegram_user_id, school_nickname, is_verified_student) VALUES
  (111111111, 'test-user-1', true),
  (222222222, 'test-user-2', true);

-- Sample matches
INSERT INTO matches (group_id, player1_id, player2_id, player1_score, player2_score, ...) VALUES
  (1, 1, 2, 3, 1, ...);
```

### Development Scripts

#### Setup Script
- **Script**: `scripts/setup.sh`
- **Purpose**: Initial setup for new developers
- **Actions**:
  1. Check prerequisites (Docker, Docker Compose)
  2. Copy `.env.example` to `.env`
  3. Build Docker images
  4. Start services
  5. Run migrations
  6. Seed development data
- **Usage**: `./scripts/setup.sh`

#### Development Script
- **Script**: `scripts/dev.sh`
- **Purpose**: Start development server
- **Actions**:
  1. Check database connectivity
  2. Run migrations (if needed)
  3. Start bot in development mode
  4. Enable hot reload (if supported)
- **Usage**: `docker-compose up bot` or `./scripts/dev.sh`

#### Test Script
- **Script**: `scripts/test.sh`
- **Purpose**: Run test suite
- **Actions**:
  1. Start test database
  2. Run migrations
  3. Run unit tests
  4. Run integration tests
  5. Generate coverage reports
- **Usage**: `./scripts/test.sh`

#### Cleanup Script
- **Script**: `scripts/cleanup.sh`
- **Purpose**: Clean up development environment
- **Actions**:
  1. Stop containers
  2. Remove volumes (optional flag)
  3. Clean build artifacts
- **Usage**: `./scripts/cleanup.sh`

### Environment Setup

#### `.env` File
- **Template**: `.env.example` (committed to git)
- **Actual**: `.env` (not committed, in `.gitignore`)
- **Contents**:
  ```
  TELEGRAM_BOT_TOKEN=your_bot_token_here
  SCHOOL21_API_USERNAME=your_username
  SCHOOL21_API_PASSWORD=your_password
  ENVIRONMENT=development
  ```

#### Prerequisites
- **Docker**: Docker 20.10+
- **Docker Compose**: Docker Compose 2.0+ (prob built-in in docker)
- **Git**: For version control

### Migration Versioning

#### Flyway Versioning Strategy
- **Format**: `V<version>__<description>.sql`
- **Examples**:
  - `V1__initial_schema.sql`
  - `V2__add_elo_history.sql`
  - `V3__add_group_topics.sql`
- **Versioning**: Sequential integers (1, 2, 3, ...)
- **Description**: Brief description of migration

#### Migration Naming Conventions
- **Descriptive**: Clear description of what migration does
- **Consistent**: Use consistent naming style
- **No Spaces**: Use underscores instead of spaces

#### Migration Testing
- **Test Migrations**: Test migrations in development first
- **Rollback Testing**: Test rollback procedures
- **Integration**: Migrations tested in CI/CD pipeline

### Development Workflow

#### Typical Workflow
1. **Clone Repository**: `git clone <repo>`
2. **Setup Environment**: `./scripts/setup.sh`
3. **Start Services**: `docker-compose up -d`
4. **Develop**: Make code changes
5. **Test**: `./scripts/test.sh`
6. **Run Migrations**: `docker-compose exec bot flyway migrate`
7. **Seed Data**: `./scripts/seed_dev.sh` (if needed)
8. **Commit**: Commit changes with proper messages

#### Hot Reload (If Supported)
- **File Watching**: Watch for file changes
- **Auto Rebuild**: Rebuild on changes
- **Auto Restart**: Restart application on changes
- **Limitations**: May not work for all changes (config, migrations)

### Observability in Development

#### Development Observability
- **Console Output**: Logs to console (pretty-printed)
- **Metrics**: Console output or local file
- **Tracing**: Console output or local file
- **No OTEL Collector**: Simpler setup (optional)

#### Production-Like Observability (Optional)
- **OTEL Collector**: Enable OTEL Collector in docker-compose
- **Prometheus**: Optional Prometheus for metrics
- **Grafana**: Optional Grafana for visualization
- **Jaeger**: Optional Jaeger for tracing

### Testing in Development

#### Test Database
- **Separate Database**: `school_tg_bot_test`
- **Isolation**: Tests don't affect development database
- **Setup**: Created automatically by test scripts
- **Cleanup**: Cleaned after tests

#### Running Tests
- **Unit Tests**: `./scripts/test.sh --unit`
- **Integration Tests**: `./scripts/test.sh --integration`
- **All Tests**: `./scripts/test.sh`
- **Watch Mode**: Optional watch mode for TDD

### Documentation

#### README
- **Setup Instructions**: Step-by-step setup guide
- **Prerequisites**: List of required tools
- **Common Tasks**: Common development tasks
- **Troubleshooting**: Common issues and solutions

#### Development Guide
- **Architecture**: Overview of architecture
- **Code Structure**: Code organization
- **Adding Features**: Guide for adding new features
- **Testing Guide**: How to write and run tests

## Consequences

### Positive
- **Consistency**: Same environment for all developers
- **Easy Setup**: New developers can start quickly
- **Production-Like**: Matches production environment
- **Isolation**: Development doesn't affect production
- **Reproducibility**: Docker ensures reproducible environments

### Negative
- **Docker Dependency**: Requires Docker (maybe issue for some)
- **Resource Usage**: Docker uses system resources
- **Complexity**: Docker Compose adds some complexity
- **Maintenance**: Need to maintain Docker files and scripts

### Neutral
- **Learning Curve**: Developers need to learn Docker basics
- **Performance**: Slight performance overhead from containerization

## Alternatives Considered

### No Docker (Bare Metal Setup)
- **Rejected**: Harder to set up, environment differences

### Vagrant
- **Rejected**: Docker is more modern and widely used

### Kubernetes (Kind, Minikube)
- **Rejected**: Overkill for local development

### Separate Services (No Docker Compose)
- **Rejected**: Harder to manage, more setup steps

### No Seeding Scripts
- **Rejected**: Developers need test data to work effectively

### Manual Migration Execution
- **Rejected**: Automated migrations reduce errors

