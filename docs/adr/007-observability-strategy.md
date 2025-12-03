# ADR-007: Observability Strategy

## Status
Accepted

## Context
The bot requires comprehensive observability to:
- Monitor system health and performance
- Debug issues and errors
- Track business metrics (ELO updates, matches, users)
- Understand request flows
- Detect anomalies and alert on issues
- Support both development and production environments

Key requirements:
- Structured logging with context
- Metrics collection (ELO updates, match counts, errors, response times, user counts)
- Distributed tracing for request flows
- Health check endpoint (if webhook mode)
- Alerting thresholds
- Different setup for dev (simpler) vs prod (OTEL Collector)

## Decision
We will implement a comprehensive observability strategy using OpenTelemetry C++:

### Structured Logging
- **Library**: OpenTelemetry C++ Logging API
- **Log Format**: JSON (structured, machine-readable)
- **Log Levels**:
  - `TRACE`: Detailed debugging information
  - `DEBUG`: Debug information for development
  - `INFO`: General informational messages
  - `WARN`: Warning messages (non-critical issues)
  - `ERROR`: Error messages (recoverable errors)
  - `FATAL`: Critical errors (application may terminate)
- **Log Context**:
  - Request ID (correlation ID for tracing)
  - User ID (telegram_user_id)
  - Group ID (group_id)
  - Operation type (match_registration, elo_update, etc.)
  - Timestamp (ISO 8601)
  - Thread ID
  - Source file and line number
- **Log Fields** (structured):
  ```json
  {
    "timestamp": "2024-01-15T10:30:00Z",
    "level": "INFO",
    "message": "Match registered successfully",
    "request_id": "abc123",
    "user_id": 123456789,
    "group_id": 987654321,
    "operation": "match_registration",
    "match_id": 42,
    "player1_id": 111,
    "player2_id": 222
  }
  ```
- **Log Aggregation**:
  - Development: Console output (pretty-printed JSON)
  - Production: Send to OTEL Collector (then to backend of choice)
- **Sensitive Data**: Never log passwords, API keys, or full message content

### Metrics Collection
- **Library**: OpenTelemetry C++ Metrics API
- **Metrics to Track**:

#### Business Metrics
- `matches.registered` (Counter): Total matches registered
  - Labels: `group_id`, `result` (player1_won, player2_won)
- `elo.updates` (Counter): Total ELO updates
  - Labels: `group_id`, `direction` (increase, decrease)
- `elo.change` (Histogram): ELO change amount
  - Labels: `group_id`
  - Buckets: [-100, -50, -25, -10, 0, 10, 25, 50, 100]
- `players.registered` (Counter): Total players registered
  - Labels: `group_id`
- `players.active` (Gauge): Current active players per group
  - Labels: `group_id`
- `users.total` (Gauge): Total unique users across all groups

#### System Metrics
- `requests.total` (Counter): Total requests processed
  - Labels: `method`, `status_code`, `endpoint`
- `requests.duration` (Histogram): Request processing time
  - Labels: `method`, `endpoint`
  - Buckets: [10ms, 50ms, 100ms, 500ms, 1s, 5s]
- `errors.total` (Counter): Total errors
  - Labels: `error_type`, `error_code`
- `database.queries` (Counter): Database queries
  - Labels: `query_type`, `status`
- `database.query.duration` (Histogram): Database query duration
  - Labels: `query_type`
- `database.connections.active` (Gauge): Active database connections
- `database.connections.pool.size` (Gauge): Connection pool size
- `rate_limit.violations` (Counter): Rate limit violations
  - Labels: `user_id`, `group_id`, `limit_type`

#### Performance Metrics
- `cache.hits` (Counter): Cache hits
- `cache.misses` (Counter): Cache misses
- `cache.size` (Gauge): Cache size
- `telegram.api.calls` (Counter): Telegram API calls
  - Labels: `method`, `status_code`
- `telegram.api.duration` (Histogram): Telegram API call duration
- `telegram.rate_limit.hits` (Counter): Telegram rate limit hits

- **Metric Naming Conventions**:
  - Use dots for hierarchy: `namespace.metric_name`
  - Use lowercase with underscores: `matches_registered`
  - Be descriptive: `database_query_duration_seconds`
  - Include units in name: `response_time_seconds`
- **Metric Export Frequency**: 
  - Push metrics every 10 seconds (configurable)
  - Batch metrics for efficiency
- **Metric Storage**: 
  - Development: Console output or local file
  - Production: OTEL Collector → Prometheus/backend of choice

### Distributed Tracing
- **Library**: OpenTelemetry C++ Tracing API
- **Trace Context Propagation**:
  - Extract trace context from incoming requests
  - Propagate trace context to downstream services (database, Telegram API)
  - Use W3C Trace Context standard
- **Spans to Create**:
  - Root span: Each Telegram message/command
  - Child spans:
    - Command parsing
    - Permission check
    - Database query (each query)
    - ELO calculation
    - Match registration
    - Telegram API call
- **Span Attributes**:
  - `telegram.user_id`
  - `telegram.group_id`
  - `telegram.message_id`
  - `command.type`
  - `match.id`
  - `player1.id`, `player2.id`
- **Sampling Strategy**:
  - Development: 100% sampling (all traces)
  - Production: 10% sampling (1 in 10 traces)
  - Always trace errors (100% error sampling)
- **Trace Export**:
  - Development: Console output or local file
  - Production: OTEL Collector → Jaeger/Tempo/backend of choice

### Health Check Endpoint
- **Endpoint**: `/health` (HTTP endpoint for webhook mode)
- **Health Check Types**:
  - **Liveness**: Is the application running?
    - Response: `200 OK` if process alive
  - **Readiness**: Is the application ready to serve requests?
    - Checks: Database connectivity, connection pool health
    - Response: `200 OK` if ready, `503 Service Unavailable` if not
- **Health Check Response**:
  ```json
  {
    "status": "healthy",
    "timestamp": "2024-01-15T10:30:00Z",
    "checks": {
      "database": "healthy",
      "connection_pool": "healthy",
      "telegram_api": "healthy"
    }
  }
  ```
- **Implementation**:
  - Lightweight endpoint (no heavy operations)
  - Cache health status (update every 5 seconds)
  - Return quickly (<100ms)

### Alerting Thresholds
- **Metrics to Alert On**:

#### Error Rate
- **Metric**: `errors.total` rate
- **Threshold**: >10 errors per minute
- **Severity**: Warning
- **Action**: Log alert, notify on-call

#### High Error Rate
- **Metric**: `errors.total` rate
- **Threshold**: >50 errors per minute
- **Severity**: Critical
- **Action**: Immediate notification, page on-call

#### Database Connection Pool Exhausted
- **Metric**: `database.connections.pool.size` / `database.connections.active`
- **Threshold**: >90% pool utilization for >1 minute
- **Severity**: Warning
- **Action**: Alert, investigate connection leaks

#### High Response Time
- **Metric**: `requests.duration` (p95)
- **Threshold**: >1 second for >5 minutes
- **Severity**: Warning
- **Action**: Investigate performance degradation

#### Telegram API Rate Limit
- **Metric**: `telegram.rate_limit.hits`
- **Threshold**: >5 hits per hour
- **Severity**: Warning
- **Action**: Review rate limit handling

#### Backup Failure
- **Metric**: Custom backup status
- **Threshold**: Backup failure
- **Severity**: Critical
- **Action**: Immediate notification

#### Health Check Failure
- **Metric**: Health check endpoint
- **Threshold**: Health check returns non-200
- **Severity**: Critical
- **Action**: Immediate notification, restart service

### Development vs Production Setup

#### Development Setup
- **Logging**: Console output (pretty-printed JSON)
- **Metrics**: Console output or local file
- **Tracing**: Console output or local file
- **No OTEL Collector**: Direct export to console/file
- **100% Sampling**: All traces for debugging
- **Verbose Logging**: DEBUG level enabled

#### Production Setup
- **OTEL Collector**: Deploy OTEL Collector as sidecar or separate service
- **Logging**: OTEL Collector → Loki/Elasticsearch/backend
- **Metrics**: OTEL Collector → Prometheus/backend
- **Tracing**: OTEL Collector → Jaeger/Tempo/backend
- **10% Sampling**: Reduce trace volume
- **INFO Level**: Production logging level
- **Resource Attributes**: 
  - `service.name`: `school-tg-bot`
  - `service.version`: Application version
  - `deployment.environment`: `production`
  - `host.name`: Hostname
  - `host.id`: Host ID

### OpenTelemetry Configuration
- **OTEL Exporter**:
  - Development: `OTLP` exporter to console or file
  - Production: `OTLP` exporter to OTEL Collector endpoint
- **OTEL Resource**:
  - Service name, version, environment
  - Host information
  - Deployment information
- **OTEL SDK Configuration**:
  - Batch processor (batch spans/metrics before export)
  - Batch size: 512 spans, 512 metrics
  - Export timeout: 30 seconds
  - Export interval: 10 seconds

### Log Aggregation Backends
- **Options** (via OTEL Collector):
  - Loki (Grafana stack)
  - Elasticsearch
  - Cloud logging (GCP, AWS CloudWatch)
- **Choice**: Configurable via OTEL Collector configuration

### Metrics Backends
- **Options** (via OTEL Collector):
  - Prometheus
  - Cloud monitoring (GCP, AWS CloudWatch)
  - Datadog, New Relic
- **Choice**: Configurable via OTEL Collector configuration

### Tracing Backends
- **Options** (via OTEL Collector):
  - Jaeger
  - Tempo (Grafana stack)
  - Cloud tracing (GCP, AWS X-Ray)
- **Choice**: Configurable via OTEL Collector configuration

## Consequences

### Positive
- **Comprehensive Observability**: Logs, metrics, and traces provide full visibility
- **Standardized**: OpenTelemetry is industry standard
- **Vendor Agnostic**: Can switch backends without code changes
- **Rich Context**: Structured logging and traces provide rich context
- **Debugging**: Distributed tracing makes debugging easier
- **Performance Monitoring**: Metrics enable performance optimization
- **Alerting**: Enables proactive issue detection

### Negative
- **Complexity**: OpenTelemetry C++ setup can be complex
- **Performance Overhead**: Instrumentation adds overhead (mitigated by sampling)
- **Storage Costs**: Logs, metrics, and traces require storage
- **Learning Curve**: Team needs to learn OpenTelemetry APIs

### Neutral
- **Configuration**: Need to configure OTEL Collector and backends
- **Maintenance**: Need to maintain observability infrastructure

## Alternatives Considered

### Separate Tools (Prometheus + Grafana + Jaeger)
- **Approach**: Use separate observability tools
- **Rejected**: More complex setup, OpenTelemetry provides unified approach

### Custom Logging/Metrics
- **Approach**: Build custom observability solution
- **Rejected**: Reinventing the wheel, OpenTelemetry is standard

### No Distributed Tracing
- **Rejected**: Tracing is essential for debugging distributed systems

### Higher Sampling Rate (50%)
- **Considered**: 50% sampling in production
- **Decision**: 10% sufficient, reduces overhead and storage

### No Health Check Endpoint
- **Rejected**: Needed for orchestration (Kubernetes, Docker, etc.)

### Text Logging (Not JSON)
- **Rejected**: JSON enables better parsing and aggregation

### Lower Log Level in Production (WARN)
- **Rejected**: INFO level needed for operational visibility



