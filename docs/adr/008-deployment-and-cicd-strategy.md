# ADR-008: Deployment and CI/CD Strategy

## Status
Accepted

## Context
The bot needs a reliable deployment and CI/CD pipeline that:
- Ensures code quality before deployment
- Automates testing and validation
- Packages the application for deployment
- Supports both development and production environments
- Handles version management
- Enables rollback capabilities
- Works with Docker Compose for production deployment

Key requirements:
- GitHub Actions for CI/CD
- Pipeline stages: lint → build → unit tests → migrations → integration tests → package → push
- Docker image building
- Docker Compose stack for production
- Webhook vs polling decision
- Process management
- Graceful restarts
- Version management
- Rollback strategy

## Decision
We will implement a comprehensive deployment and CI/CD strategy:

### CI/CD Pipeline (GitHub Actions)

#### Pipeline Stages
1. **Lint** (can fail)
   - Tool: `clang-tidy` or `cppcheck`
   - Check code style and potential issues
   - Fail pipeline on linting errors
   - Run on: All pull requests and pushes

2. **Build**
   - Build with CMake
   - Use Conan to fetch dependencies
   - Build for target platform (Linux)
   - Cache Conan dependencies for faster builds
   - Fail on build errors

3. **Unit Tests**
   - Run all unit tests
   - Generate coverage reports
   - Fail if tests fail
   - Coverage threshold: 70% (configurable)

4. **Run Migrations on Ephemeral DB**
   - Start PostgreSQL container
   - Run Flyway migrations
   - Verify migrations succeed
   - Clean up container after test

5. **Integration Tests**
   - Start full stack (bot + PostgreSQL + OTEL Collector)
   - Run integration test suite
   - Test with real database
   - Clean up after tests

6. **Package Image**
   - Build Docker image
   - Tag with: `latest`, `v<version>`, `<commit-sha>`
   - Multi-stage build for optimization
   - Scan image for vulnerabilities (optional)

7. **Push to Registry**
   - Push to GitHub Container Registry (ghcr.io)
   - Authenticate with GITHUB_TOKEN
   - Push tags: `latest`, version tag, commit SHA

#### GitHub Actions Workflow
- **Triggers**:
  - Push to `main` branch
  - Pull requests to `main`
  - Manual workflow dispatch
- **Matrix Strategy**: Build for multiple platforms (no needed for now)
- **Secrets**: Database credentials for integration tests
- **Artifacts**: Test reports, coverage reports, Docker images

### Docker Image Build Strategy

#### Multi-Stage Build
```dockerfile
# Stage 1: Build
FROM conanio/gcc12:latest AS builder
WORKDIR /build
COPY . .
RUN conan install . --output-folder=build --build=missing
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target school_tg_tt_bot

# Stage 2: Runtime
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libpqxx-6.2 \
    postgresql-client \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/build/school_tg_tt_bot /usr/local/bin/
COPY --from=builder /build/flyway /usr/local/bin/
ENTRYPOINT ["/usr/local/bin/school_tg_tt_bot"]
```

#### Image Optimization
- **Base Image**: Ubuntu 22.04 (minimal)
- **Layer Caching**: Order Dockerfile commands for optimal caching
- **Size Optimization**: Remove build dependencies in final image
- **Security**: Use non-root user in container
- **Health Check**: Include HEALTHCHECK instruction

#### Image Tagging
- `latest`: Latest successful build from main branch
- `v<major>.<minor>.<patch>`: Semantic version tags
- `<commit-sha>`: Specific commit SHA
- `pr-<number>`: Pull request builds (for testing)

### Telegram Connection: Webhook vs Polling

#### Decision: Webhook (Primary), Polling (Fallback)
- **Webhook Mode** (Production):
  - More efficient (no constant polling)
  - Lower latency
  - Better for production scale
  - Requires HTTPS endpoint
  - Requires process management for reliability

- **Polling Mode** (Development/Fallback):
  - Simpler setup (no web server needed)
  - Good for development
  - Fallback if webhook fails
  - Higher latency
  - More API calls

#### Webhook Setup
- **Endpoint**: `/webhook` (HTTPS required)
- **Port**: 8443 (configurable)
- **SSL/TLS**: Required (use Let's Encrypt or similar)
- **Webhook URL**: `https://<domain>:8443/webhook`
- **Secret Token**: Use secret token for webhook verification
- **Certificate Management**: Auto-renewal with certbot

#### Fallback Strategy
- If webhook fails to set: Fall back to polling
- If webhook receives errors: Switch to polling temporarily
- Monitor webhook health and alert on failures

### Process Management

#### Decision: Docker Compose (Primary), systemd (Alternative)
- **Docker Compose** (Recommended):
  - Container orchestration
  - Easy service dependencies
  - Resource limits
  - Health checks
  - Restart policies
  - Log management

- **systemd** (Alternative for bare metal):
  - Native Linux service management
  - Good for VPS without Docker
  - Service file configuration
  - Auto-restart on failure

#### Docker Compose Configuration
```yaml
version: '3.8'
services:
  bot:
    image: ghcr.io/<org>/school-tg-tt-bot:latest
    restart: unless-stopped
    environment:
      - DATABASE_URL=${DATABASE_URL}
      - TELEGRAM_BOT_TOKEN=${TELEGRAM_BOT_TOKEN}
    depends_on:
      postgres:
        condition: service_healthy
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    resources:
      limits:
        cpus: '1'
        memory: 512M

  postgres:
    image: postgres:15
    restart: unless-stopped
    environment:
      - POSTGRES_DB=${POSTGRES_DB}
      - POSTGRES_USER=${POSTGRES_USER}
      - POSTGRES_PASSWORD=${POSTGRES_PASSWORD}
    volumes:
      - postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER}"]
      interval: 10s
      timeout: 5s
      retries: 5

  otel-collector:
    image: otel/opentelemetry-collector:latest
    restart: unless-stopped
    volumes:
      - ./otel-collector-config.yaml:/etc/otel-collector-config.yaml
    command: ["--config=/etc/otel-collector-config.yaml"]
```

### Graceful Restart Without Message Loss

#### Webhook Mode
- **Message Acknowledgment**: Acknowledge webhook immediately (200 OK)
- **Async Processing**: Process message asynchronously after acknowledgment
- **In-Flight Tracking**: Track processing requests
- **Shutdown Sequence**:
  1. Stop accepting new webhooks (return 503)
  2. Wait for in-flight requests (max 30 seconds)
  3. Complete current transactions
  4. Shutdown gracefully

#### Polling Mode
- **Message Offset**: Track last processed message ID
- **Shutdown**: Save offset before shutdown
- **Startup**: Resume from saved offset

#### State Preservation
- All state in database (stateless design)
- No in-memory state to preserve
- Just need to complete in-flight transactions

### Version Management

#### Application Versioning
- **Semantic Versioning**: `MAJOR.MINOR.PATCH`
- **Version Source**: 
  - Git tags for releases
  - `VERSION` file in repository
  - Build-time injection via CMake
- **Version Display**: `--version` flag shows version
- **Version in Metrics**: Include version in OpenTelemetry resource attributes

#### Database Schema Versioning
- **Flyway Versioning**: Flyway manages schema versions
- **Version Format**: `V<version>__<description>.sql`
- **Migration Tracking**: Flyway tracks applied migrations in `flyway_schema_history`
- **Version Compatibility**: Application checks database version on startup

#### Version Compatibility Matrix
- Application version X.Y.Z compatible with schema version A.B
- Check compatibility on startup
- Fail fast if incompatible

### Rollback Strategy

#### Application Rollback
- **Docker Image Rollback**:
  - Keep previous image tags
  - Update Docker Compose to use previous tag
  - `docker-compose pull && docker-compose up -d`
  - Rollback time: < 5 minutes

- **Database Migration Rollback**:
  - Flyway supports undo migrations (optional)
  - Create `U<version>__<description>.sql` files
  - Run undo migration: `flyway undo`
  - Test rollback in staging first

#### Rollback Testing
- **Staging Environment**: Test rollbacks in staging
- **Rollback Procedures**: Documented runbook
- **Automated Rollback**: Optional automated rollback on health check failure

#### Rollback Triggers
- Health check failures after deployment
- Error rate spike after deployment
- Manual trigger by operator

### Production Deployment

#### Deployment Process
1. **Pre-Deployment**:
   - Run full CI/CD pipeline
   - Verify all tests pass
   - Review changes

2. **Deployment**:
   - Tag release in Git
   - CI/CD builds and pushes image
   - Update Docker Compose on VPS
   - Pull new image
   - Run database migrations (if any)
   - Restart services

3. **Post-Deployment**:
   - Verify health checks pass
   - Monitor error rates
   - Verify functionality
   - Rollback if issues detected

#### Deployment Automation
- **Manual Deployment**: Operator triggers deployment
- **Automated Deployment**: Auto-deploy on merge to main (optional)
- **Blue-Green Deployment**: Not needed for this scale (can add later)

### Resource Management

#### Resource Limits
- **CPU**: 1 core (configurable)
- **Memory**: 512MB (configurable)
- **Disk**: Minimal (application only)
- **Network**: Standard

#### Scaling Strategy
- **Horizontal Scaling**: Can run multiple instances (with proper idempotency)
- **Vertical Scaling**: Increase resources if needed
- **Auto-Scaling**: Not needed initially (can add later)

### Monitoring Deployment

#### Deployment Metrics
- Deployment frequency
- Deployment success rate
- Time to deploy
- Rollback frequency

#### Post-Deployment Monitoring
- Error rate monitoring (first 10 minutes)
- Performance metrics
- Health check status
- User-reported issues

## Consequences

### Positive
- **Automation**: CI/CD automates testing and deployment
- **Quality Assurance**: Multiple test stages ensure quality
- **Reproducibility**: Docker ensures consistent environments
- **Fast Rollback**: Quick rollback capability
- **Version Control**: Clear versioning strategy
- **Scalability**: Docker Compose enables easy scaling

### Negative
- **Complexity**: CI/CD pipeline adds complexity
- **Build Time**: Full pipeline takes time (mitigated by caching)
- **Docker Dependency**: Requires Docker on deployment target
- **Maintenance**: Need to maintain CI/CD pipeline and Docker images

### Neutral
- **Cost**: GitHub Actions minutes (free tier available)
- **Storage**: Docker image storage costs
- **Configuration**: Need to configure CI/CD and Docker Compose

## Alternatives Considered

### External CI/CD (Jenkins, GitLab CI)
- **Rejected**: GitHub Actions is simpler and integrated

### Single-Stage Docker Build
- **Rejected**: Multi-stage builds produce smaller images

### No Docker (Bare Metal)
- **Considered**: Deploy directly to VPS
- **Rejected**: Docker provides better isolation and portability

### Kubernetes
- **Rejected**: Overkill for this scale, Docker Compose sufficient

### Automated Deployments Only
- **Rejected**: Manual deployment provides safety check

### No Rollback Strategy
- **Rejected**: Need ability to quickly revert problematic deployments

### Polling Only (No Webhook)
- **Rejected**: Webhook is more efficient for production

### No Health Checks
- **Rejected**: Health checks essential for orchestration

