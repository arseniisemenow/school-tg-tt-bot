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

All scripts are located in the `scripts/` directory and are designed to be run inside the bot container (unless otherwise specified).

#### Environment Setup Script
- **Script**: `scripts/setup-env.sh`
- **Purpose**: Install and configure development environment dependencies
- **Location**: Inside bot container at `/app/scripts/setup-env.sh`
- **Actions**:
  1. Detect Conan profile (if not already configured)
  2. Run `conan install . --output-folder=build --build=missing -s compiler.cppstd=20`
  3. Verify dependencies are installed correctly
  4. Create build directory structure if needed
- **Usage**: `./scripts/setup-env.sh` (inside bot container)
- **When to Run**:
  - First time setup
  - After modifying `conanfile.txt`
  - When dependencies need to be refreshed
- **Idempotent**: Yes, safe to run multiple times

#### Build Script
- **Script**: `scripts/build.sh`
- **Purpose**: Build the C++ application
- **Location**: Inside bot container at `/app/scripts/build.sh`
- **Actions**:
  1. Check if Conan dependencies are installed
  2. Configure CMake with Conan toolchain: `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug`
  3. Build the application: `cmake --build build`
  4. Verify binary was created
- **Usage**: `./scripts/build.sh` (inside bot container)
- **Output**: Binary at `build/school_tg_tt_bot`
- **Build Type**: Debug (for development)

#### Test Script
- **Script**: `scripts/test.sh`
- **Purpose**: Run test suite
- **Location**: Inside bot container at `/app/scripts/test.sh`
- **Actions**:
  1. Check if application is built
  2. Run unit tests (if configured)
  3. Run integration tests (if configured)
  4. Generate coverage reports (if configured)
  5. Exit with appropriate exit code
- **Usage**: `./scripts/test.sh` (inside bot container)
- **Options**:
  - `--unit`: Run only unit tests
  - `--integration`: Run only integration tests
  - `--coverage`: Generate coverage reports

#### Run Script
- **Script**: `scripts/run.sh`
- **Purpose**: Start the application for manual testing
- **Location**: Inside bot container at `/app/scripts/run.sh`
- **Actions**:
  1. Check if application is built
  2. Verify database connectivity
  3. Check required environment variables
  4. Start the application: `./build/school_tg_tt_bot`
- **Usage**: `./scripts/run.sh` (inside bot container)
- **Alternative**: Run binary directly: `./build/school_tg_tt_bot`

#### Migration Script
- **Script**: `scripts/migrate.sh`
- **Purpose**: Run database migrations
- **Location**: Inside bot container at `/app/scripts/migrate.sh`
- **Actions**:
  1. Check database connectivity
  2. Run Flyway migrations
  3. Report migration status
- **Usage**: `./scripts/migrate.sh` (inside bot container)

#### Seed Development Data Script
- **Script**: `scripts/seed-dev.sh`
- **Purpose**: Populate database with development test data
- **Location**: Inside bot container at `/app/scripts/seed-dev.sh`
- **Actions**:
  1. Check database connectivity
  2. Run seed SQL scripts
  3. Verify seed data was inserted
- **Usage**: `./scripts/seed-dev.sh` (inside bot container)
- **Idempotent**: Yes, safe to run multiple times

#### Initial Setup Script (Host Machine)
- **Script**: `scripts/setup.sh` (optional, for convenience)
- **Purpose**: Initial setup for new developers (run from host)
- **Location**: On host machine at `scripts/setup.sh`
- **Actions**:
  1. Check prerequisites (Docker, Docker Compose)
  2. Copy `.env.example` to `.env` (if not exists)
  3. Build Docker images: `docker compose build`
  4. Start services: `docker compose up -d`
  5. Provide instructions for next steps
- **Usage**: `./scripts/setup.sh` (from host machine)
- **Note**: This is a convenience script; developers can also follow manual steps

#### Cleanup Script (Host Machine)
- **Script**: `scripts/cleanup.sh` (optional)
- **Purpose**: Clean up development environment
- **Location**: On host machine at `scripts/cleanup.sh`
- **Actions**:
  1. Stop containers: `docker compose down`
  2. Remove volumes (if `--volumes` flag provided)
  3. Clean build artifacts (optional)
- **Usage**: 
  - `./scripts/cleanup.sh` - Stop containers
  - `./scripts/cleanup.sh --volumes` - Stop containers and remove volumes

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

#### Initial Setup (First Time)
1. **Clone Repository**: `git clone <repo>`
2. **Create Environment File**: Copy `.env.example` to `.env` and fill in required values
3. **Start Services**: `docker compose up -d` (starts PostgreSQL and optionally OTEL Collector)
4. **Enter Bot Container**: `docker compose exec bot bash`
5. **Setup Environment**: Inside container, run `./scripts/setup-env.sh` (installs Conan dependencies)
6. **Build Application**: Inside container, run `./scripts/build.sh`
7. **Run Migrations**: `./scripts/migrate.sh` (if needed)
8. **Seed Data**: `./scripts/seed-dev.sh` (optional, for test data)

#### Daily Development Workflow

The development process is designed to be fast and efficient, with all services managed through Docker Compose:

##### Step 1: Start All Services
Start PostgreSQL and optionally OTEL Collector using Docker Compose:

```bash
# Start all required services (postgres, bot container)
docker compose up -d

# Optionally start OTEL Collector for observability
docker compose --profile observability up -d otel-collector
```

This will:
- Start PostgreSQL database container
- Start bot container (interactive shell, ready for development)
- Optionally start OTEL Collector if observability profile is enabled
- All services are networked together and ready to use

##### Step 2: Enter Bot Container
Enter the bot container to perform development tasks:

```bash
docker compose exec bot bash
```

The container has:
- All build tools (GCC 12, CMake, Conan 2.x)
- Source code mounted at `/app` (changes on host are reflected in container)
- Build directory persisted in Docker volume
- Access to PostgreSQL via `postgres:5432` hostname
- Access to OTEL Collector via `otel-collector:4317/4318` if enabled

##### Step 3: Setup Environment (First Time or After Dependency Changes)
Inside the bot container, run the environment setup script:

```bash
./scripts/setup-env.sh
```

This script:
- Runs `conan install` to install all C++ dependencies
- Configures Conan profile if needed
- Sets up build directory structure
- Can be run multiple times safely (idempotent)
- Only reinstalls dependencies if `conanfile.txt` has changed

**Note**: This step is only needed:
- On first setup
- When `conanfile.txt` is modified
- When dependencies need to be refreshed

##### Step 4: Build the Application
Build the C++ application:

```bash
./scripts/build.sh
```

This script:
- Configures CMake with Conan toolchain
- Builds the application in Debug mode (for development)
- Places the binary in `build/school_tg_tt_bot`
- Shows build progress and errors

**Note**: After code changes, simply run this script again to rebuild.

##### Step 5: Run Tests or Start Application

**Option A: Run Tests**
```bash
./scripts/test.sh
```

This will:
- Run unit tests
- Run integration tests (if configured)
- Generate test coverage reports
- Exit with appropriate exit code

**Option B: Start Application for Manual Testing**
```bash
./scripts/run.sh
```

Or run directly:
```bash
./build/school_tg_tt_bot
```

The application will:
- Connect to PostgreSQL database
- Connect to Telegram Bot API (using token from environment)
- Start processing Telegram messages
- Log to console (development mode)

##### Iterative Development Cycle

After making code changes:

1. **Rebuild**: `./scripts/build.sh` (inside container)
2. **Test**: `./scripts/test.sh` (inside container) OR
3. **Run**: `./scripts/run.sh` (inside container) to test manually via Telegram
4. **Exit Container**: `exit` when done
5. **Stop Services**: `docker compose down` (optional, containers can stay running)

##### Quick Reference Commands

**From Host Machine:**
```bash
# Start all services
docker compose up -d

# Start with observability
docker compose --profile observability up -d

# Enter bot container
docker compose exec bot bash

# View logs
docker compose logs -f bot

# Stop all services
docker compose down

# Stop and remove volumes (clean slate)
docker compose down -v
```

**Inside Bot Container:**
```bash
# Setup environment (conan install)
./scripts/setup-env.sh

# Build application
./scripts/build.sh

# Run tests
./scripts/test.sh

# Run application
./scripts/run.sh

# Run migrations
./scripts/migrate.sh

# Seed development data
./scripts/seed-dev.sh
```

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

#### Testing Workflow

Testing is integrated into the daily development workflow:

1. **Enter Bot Container**: `docker compose exec bot bash`
2. **Build Application**: `./scripts/build.sh` (if not already built)
3. **Run Tests**: `./scripts/test.sh`

#### Test Database
- **Separate Database**: `school_tg_bot_test` (optional, can use dev database for integration tests)
- **Isolation**: Unit tests don't require database; integration tests use test database
- **Setup**: Created automatically by test scripts if needed
- **Cleanup**: Test database is cleaned/reset between test runs

#### Running Tests

**Inside Bot Container:**

```bash
# Run all tests
./scripts/test.sh

# Run only unit tests
./scripts/test.sh --unit

# Run only integration tests
./scripts/test.sh --integration

# Run tests with coverage
./scripts/test.sh --coverage
```

#### Manual Testing via Telegram

For manual testing of bot functionality:

1. **Start Application**: `./scripts/run.sh` (inside bot container)
2. **Interact with Bot**: Send messages to the bot via Telegram
3. **Monitor Logs**: Application logs are printed to console
4. **Check Database**: Query database to verify state changes
5. **Stop Application**: Press `Ctrl+C` to stop

#### Test Types

- **Unit Tests**: Test individual components in isolation
  - No database required
  - Fast execution
  - Run frequently during development

- **Integration Tests**: Test components working together
  - May require test database
  - Test database interactions
  - Test Telegram API interactions (mocked or test bot)

- **End-to-End Tests**: Full system testing
  - Requires all services running
  - Uses test Telegram bot
  - Tests complete user workflows

#### Test Data Management

- **Unit Tests**: Use mocks and stubs, no persistent data
- **Integration Tests**: Use test database with seed data
- **Manual Testing**: Use development database with seed data from `scripts/seed-dev.sh`

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



