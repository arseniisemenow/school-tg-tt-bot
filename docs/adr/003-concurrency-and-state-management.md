# ADR-003: Concurrency and State Management

## Status
Accepted

## Context
The Telegram bot must handle:
- Concurrent requests from multiple groups and users
- Bot restarts without losing state or messages
- Race conditions when multiple matches are processed simultaneously
- Duplicate command processing (idempotency)
- ELO updates that must be atomic and consistent
- Telegram API rate limits and transient failures

Key requirements:
- No in-memory state that could be lost on restart
- Thread-safe operations
- Prevent duplicate ELO updates from the same command
- Handle concurrent match registrations correctly
- Recover gracefully from bot restarts

## Decision
We will implement a stateless bot design with the following concurrency and state management strategies:

### Stateless Bot Design
- **No in-memory state**: All state is stored in PostgreSQL database
- **State recovery on restart**: Bot queries database on startup to rebuild any necessary caches
- **Database as source of truth**: All operations read from and write to database
- **No command queuing**: Process commands synchronously (Telegram handles queuing)

### Thread Safety Strategy
- **Connection pooling**: Use libpqxx connection pool (thread-safe)
- **Database transactions**: All ELO updates wrapped in transactions
- **Transaction isolation**: Use PostgreSQL's default isolation level (READ COMMITTED)
- **No application-level locking**: Rely on database transactions and optimistic locking

### Idempotency Implementation
- **Idempotency key**: Use Telegram `message_id` or hash of command + parameters
- **Storage**: Store `idempotency_key` in `matches` table with UNIQUE constraint
- **Validation**: Before processing match, check if `idempotency_key` exists
- **Early return**: If duplicate found, return success without processing (idempotent response)

### Optimistic Locking for Race Conditions
- **Version field**: Add `version` column to `group_players` table
- **Update pattern**:
  1. Read current ELO and version
  2. Calculate new ELO
  3. Update with `WHERE version = <read_version>`
  4. Check affected rows - if 0, retry (optimistic lock failed)
- **Retry strategy**: Exponential backoff with max 3 retries for optimistic lock conflicts
- **Transaction boundaries**: Each ELO update in separate transaction

### Transaction Management
- **Transaction boundaries**: Each match registration is one transaction
- **ACID compliance**: Use PostgreSQL transactions for all ELO updates
- **Rollback on error**: Any error in transaction triggers automatic rollback
- **Isolation level**: READ COMMITTED (PostgreSQL default)
  - Prevents dirty reads
- **Transaction scope**: 
  - Begin transaction
  - Validate match (players exist, not duplicate, etc.)
  - Update `group_players` for both players (with optimistic locking)
  - Insert `matches` record
  - Insert `elo_history` records (2 rows)
  - Commit transaction

### State Recovery on Restart
- **No persistent state needed**: Bot is stateless
- **Configuration reload**: Read config file and environment variables on startup
- **Database connection**: Establish connection pool on startup
- **Health check**: Verify database connectivity before accepting requests
- **No message replay**: Telegram webhook/polling handles message delivery

### Concurrent Request Handling
- **Thread pool**: Use thread pool for handling Telegram API callbacks
- **Per-request isolation**: Each request processed independently
- **No shared mutable state**: All state in database
- **Connection pool**: Thread-safe connection pool handles concurrent database access

### Idempotency Key Generation
- **Primary**: Use Telegram `message_id` (unique per message)
- **Fallback**: If message_id not available, hash(command + player1_id + player2_id + scores + timestamp)
- **Storage**: Store in `matches.idempotency_key` with UNIQUE constraint
- **Validation**: Database constraint prevents duplicates at DB level

### ELO Update Flow (With Concurrency Protection)
```
1. Receive match command
2. Generate/retrieve idempotency_key
3. BEGIN TRANSACTION
4. Check if idempotency_key exists in matches (SELECT)
   - If exists: ROLLBACK, return success (idempotent)
5. Validate players exist and are in group
6. Read current ELO and version for player1 (SELECT ... FOR UPDATE)
7. Read current ELO and version for player2 (SELECT ... FOR UPDATE)
8. Calculate new ELO values
9. Update player1 ELO (UPDATE ... WHERE version = <read_version>)
   - Check affected rows, if 0: ROLLBACK, retry from step 6
10. Update player2 ELO (UPDATE ... WHERE version = <read_version>)
    - Check affected rows, if 0: ROLLBACK, retry from step 6
11. Insert match record
12. Insert elo_history records (2 rows)
13. COMMIT TRANSACTION
```

### Retry Strategy for Optimistic Lock Conflicts
- **Max retries**: 3 attempts
- **Backoff**: Exponential (100ms, 200ms, 400ms)
- **After max retries**: Return error to user
- **Logging**: Log all retry attempts for observability

### Error Handling
- **Transaction rollback**: Automatic on any exception
- **Error logging**: Log all errors with context
- **User notification**: Inform user of errors via Telegram message
- **No partial updates**: Transaction ensures all-or-nothing

## Consequences

### Positive
- **Stateless**: Bot can restart without losing state
- **Scalability**: Can run multiple bot instances (with proper idempotency)
- **Data Integrity**: Transactions ensure ACID compliance
- **No Lost Updates**: Optimistic locking prevents race conditions
- **Idempotency**: Duplicate commands are safely ignored
- **Recovery**: Simple restart recovery (just reconnect to database)

### Negative
- **Database Load**: Every operation hits database (mitigated by connection pooling)
- **Retry Overhead**: Optimistic lock conflicts require retries
- **Transaction Overhead**: Each match requires transaction (acceptable for this use case)
- **Complexity**: Optimistic locking adds complexity to update logic

### Neutral
- **Latency**: Database round-trips add latency (acceptable for bot use case)
- **Connection Pooling**: Need to size pool appropriately

## Alternatives Considered

### Pessimistic Locking
- **Approach**: Use SELECT FOR UPDATE to lock rows
- **Rejected**: Higher contention, potential deadlocks, worse performance

### In-Memory State with Persistence
- **Approach**: Keep state in memory, periodically persist
- **Rejected**: Risk of data loss on crash, more complex recovery

### Command Queuing
- **Approach**: Queue commands, process sequentially
- **Rejected**: Not needed - Telegram handles message delivery, adds unnecessary complexity

### Event Sourcing
- **Approach**: Store events, derive state
- **Rejected**: Overkill for this use case, adds significant complexity

### Distributed Locking
- **Approach**: Use Redis or similar for distributed locks
- **Rejected**: Adds dependency, database transactions sufficient

### No Idempotency
- **Approach**: Process all commands, handle duplicates in application
- **Rejected**: Risk of duplicate ELO updates, harder to detect

### No Optimistic Locking
- **Approach**: Just update ELO without version checking
- **Rejected**: Risk of lost updates in concurrent scenarios

### Higher Isolation Level (SERIALIZABLE)
- **Approach**: Use SERIALIZABLE isolation level
- **Rejected**: Higher contention, more rollbacks, worse performance than optimistic locking

