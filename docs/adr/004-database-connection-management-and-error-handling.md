# ADR-004: Database Connection Management and Error Handling

## Status
Accepted

## Context
The bot requires reliable database connectivity to PostgreSQL for all operations. We need to handle:
- Concurrent requests requiring database access
- Connection failures and transient network issues
- Connection pool management
- Health monitoring
- Graceful degradation on failures
- Telegram API rate limits
- Transaction rollback on errors
- Retry logic for transient failures
- Dead letter queue for failed operations
- Graceful shutdown

Key requirements:
- High availability and reliability
- Efficient connection reuse
- Automatic recovery from failures
- Proper error handling and logging
- No data loss on errors

## Decision
We will implement comprehensive database connection management and error handling using the following strategies:

### Connection Pooling
- **Library**: libpqxx connection pool
- **Pool Size**: 
  - Minimum: 2 connections
  - Maximum: 10 connections (configurable via config file)
  - Formula: `max(2, min(10, num_threads + 2))`
- **Connection Lifecycle**:
  - Lazy initialization: Create connections on first use
  - Connection reuse: Reuse connections from pool
  - Idle timeout: Close idle connections after 5 minutes
  - Max lifetime: Recycle connections after 1 hour of use
- **Thread Safety**: libpqxx pool is thread-safe, multiple threads can access concurrently

### Connection Retry Strategy
- **Exponential Backoff**: 
  - Initial delay: 100ms
  - Max delay: 30 seconds
  - Multiplier: 2x per retry
  - Max retries: 5 attempts
- **Retryable Errors**:
  - Connection failures (network issues)
  - Temporary database unavailability
  - Connection timeout
  - Transient PostgreSQL errors (class 08, 40, 57)
- **Non-Retryable Errors**:
  - Authentication failures
  - Syntax errors
  - Constraint violations
  - Permission denied errors
- **Circuit Breaker Pattern**:
  - Track consecutive failures
  - After 5 consecutive failures: open circuit (stop retrying)
  - After 30 seconds: half-open (try one request)
  - On success: close circuit (normal operation)
  - On failure: reopen circuit

### Connection Health Checks
- **Health Check Frequency**: Every 30 seconds
- **Health Check Query**: `SELECT 1` (lightweight)
- **Implementation**:
  - Background thread performs health checks
  - Mark connections as unhealthy on failure
  - Remove unhealthy connections from pool
  - Create new connections to replace unhealthy ones
- **Pool Refresh**: If >50% of connections unhealthy, refresh entire pool
- **Monitoring**: Log health check failures, expose metrics

### Transaction Management
- **Transaction Boundaries**: 
  - Each match registration: one transaction
  - Each ELO update: one transaction
  - Each undo operation: one transaction
- **Rollback Strategy**:
  - Automatic rollback on any exception
  - Explicit rollback on validation failures
  - Log all rollbacks with context
- **Isolation Level**: READ COMMITTED (PostgreSQL default)
- **Error Handling**:
  - Catch all database exceptions
  - Log error details (query, parameters, error code)
  - Rollback transaction
  - Return appropriate error to user

### Retry Logic for Transient Failures
- **Retryable Operations**:
  - Database queries (SELECT, INSERT, UPDATE)
  - Connection establishment
  - Transaction commits
- **Non-Retryable Operations**:
  - Transaction rollbacks (already failed)
  - Validation errors (user input issues)
  - Constraint violations (data integrity issues)
- **Retry Strategy**:
  - Exponential backoff: 100ms, 200ms, 400ms, 800ms, 1600ms
  - Max 3 retries for application-level operations
  - Max 5 retries for connection-level operations
  - After max retries: escalate to dead letter queue
- **Idempotency**: All retried operations must be idempotent (use idempotency keys)

### Dead Letter Queue (DLQ)
- **Storage**: PostgreSQL table `failed_operations`
- **Schema**:
  - `id` (BIGSERIAL PRIMARY KEY)
  - `operation_type` (VARCHAR(50)) - 'match_registration', 'elo_update', etc.
  - `operation_data` (JSONB) - Serialized operation data
  - `error_message` (TEXT)
  - `error_code` (VARCHAR(50))
  - `retry_count` (INTEGER DEFAULT 0)
  - `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
  - `last_retry_at` (TIMESTAMP WITH TIME ZONE)
  - `status` (VARCHAR(50)) - 'pending', 'retrying', 'failed', 'resolved'
- **Processing**:
  - Background job retries DLQ entries every 5 minutes
  - Max 3 retry attempts per entry
  - After max retries: mark as 'failed', alert admin
  - Manual intervention: Admin can manually retry or delete entries
- **Monitoring**: Alert on DLQ size > 100 entries

### Graceful Shutdown
- **Shutdown Sequence**:
  1. Stop accepting new requests (set shutdown flag)
  2. Wait for in-flight requests to complete (max 30 seconds)
  3. Close all database connections gracefully
  4. Flush any pending logs/metrics
  5. Exit process
- **Signal Handling**: Handle SIGTERM and SIGINT
- **In-Flight Requests**: 
  - Track active requests
  - Allow completion of current transaction
  - Reject new requests during shutdown
- **Connection Cleanup**: 
  - Wait for active transactions to complete
  - Close idle connections
  - Force close if timeout exceeded (10 seconds)

### Telegram API Rate Limit Handling
- **Rate Limit Detection**:
  - Monitor HTTP 429 responses
  - Parse `Retry-After` header
  - Track rate limit errors in metrics
- **Backoff Strategy**:
  - Use `Retry-After` header value if provided
  - Otherwise: exponential backoff (1s, 2s, 4s, 8s, 16s)
  - Max backoff: 60 seconds
- **Request Queuing**:
  - Queue rate-limited requests
  - Process queue when rate limit expires
  - Max queue size: 1000 requests
  - Drop requests if queue full (log warning)
- **Error Handling**:
  - Log rate limit violations
  - Alert if rate limit hit frequently (>10 times/hour)
  - Return user-friendly error message

### Error Classification
- **Transient Errors** (retryable):
  - Network timeouts
  - Connection failures
  - Temporary database unavailability
  - Rate limit errors (429)
  - Deadlock errors (retry transaction)
- **Permanent Errors** (non-retryable):
  - Authentication failures
  - Permission denied
  - Constraint violations
  - Invalid input data
  - Syntax errors
- **Error Response**:
  - Transient: Retry automatically, log for monitoring
  - Permanent: Return error to user immediately, log for debugging

### Monitoring and Alerting
- **Metrics to Track**:
  - Connection pool size (current, min, max)
  - Connection failures (count, rate)
  - Transaction failures (count, rate)
  - Retry attempts (count, success rate)
  - DLQ size
  - Average query latency
  - Rate limit violations
- **Alerts**:
  - Connection pool exhausted (>90% utilization)
  - High failure rate (>10% of requests)
  - DLQ size > 100 entries
  - Circuit breaker open
  - Rate limit violations > 10/hour

## Consequences

### Positive
- **Reliability**: Connection pooling and retry logic improve reliability
- **Performance**: Connection reuse reduces overhead
- **Resilience**: Automatic recovery from transient failures
- **Observability**: Comprehensive metrics and logging
- **Data Integrity**: Proper transaction management prevents data corruption
- **User Experience**: Transparent retries, users don't see transient failures

### Negative
- **Complexity**: Multiple layers of error handling add complexity
- **Latency**: Retries add latency to failed operations
- **Resource Usage**: Connection pool uses database connections
- **DLQ Overhead**: Additional table and background processing

### Neutral
- **Configuration**: Need to tune pool size and retry parameters
- **Monitoring**: Need to monitor metrics and alerts

## Alternatives Considered

### Single Connection (No Pooling)
- **Rejected**: Poor performance under concurrent load, connection exhaustion

### Larger Connection Pool
- **Considered**: 20+ connections
- **Rejected**: Wastes database resources, not needed for bot workload

### Synchronous Retries Only
- **Rejected**: Blocks request handling, poor user experience

### No Dead Letter Queue
- **Rejected**: Failed operations would be lost, no way to recover

### Pessimistic Retry Strategy
- **Approach**: Always retry, no circuit breaker
- **Rejected**: Could overwhelm database during outages

### External Circuit Breaker Library
- **Considered**: Use dedicated circuit breaker library
- **Rejected**: Simple implementation sufficient, avoid extra dependency

### Message Queue for Async Processing
- **Rejected**: Adds complexity, not needed for bot's synchronous nature

### Connection Pool per Thread
- **Rejected**: Wastes connections, shared pool more efficient






