# ADR-006: Security and Abuse Prevention

## Status
Accepted

## Context
The bot operates in public Telegram groups and must protect against:
- SQL injection attacks
- Command injection
- Input validation bypass
- Spam and command flooding
- Impersonation (forwarded messages)
- Match manipulation (fake matches, collusion)
- Unauthorized ELO modifications
- Rate limit abuse

Key requirements:
- Input validation and sanitization
- SQL injection prevention
- Rate limiting per user and per group
- Permission checks for sensitive operations
- Abuse pattern detection and prevention
- Privilege rules enforcement

## Decision
We will implement a multi-layered security and abuse prevention strategy:

### Input Validation
- **Command Parsing**:
  - Strict command format validation
  - Regex-based parsing with whitelist approach
  - Reject malformed commands immediately
- **Player Mentions**:
  - Validate Telegram user IDs (numeric, within valid range)
  - Verify mentioned users exist in database
  - Reject invalid or non-existent users
- **Score Validation**:
  - Integer validation (non-negative)
  - Range checks (reasonable limits, e.g., 0-100)
  - Reject negative scores
- **String Inputs**:
  - Length limits (prevent buffer overflow)
  - Character whitelist (alphanumeric, underscores, hyphens)
  - Trim whitespace
  - Reject control characters
- **School Nickname Validation**:
  - Format validation (alphanumeric, specific pattern)
  - Length limits
  - Character restrictions

### SQL Injection Prevention
- **Parameterized Queries Only**:
  - Use libpqxx prepared statements
  - Never concatenate user input into SQL strings
  - All user data passed as parameters
- **Query Builder**:
  - Use libpqxx query builder for dynamic queries
  - Validate all identifiers (table names, column names)
  - Whitelist approach for dynamic table/column names
- **Input Sanitization**:
  - Sanitize before parameter binding
  - Type checking (ensure integers are integers)
  - Escape special characters (though parameters handle this)
- **Code Review**:
  - Prohibit string concatenation for SQL
  - Static analysis tools to detect SQL injection risks
  - Code review checklist item

### Rate Limiting
- **Per-User Rate Limiting**:
  - Limit: 10 commands per minute per user
  - Storage: In-memory cache (Redis-like) or database table
  - Key: `rate_limit:user:<telegram_user_id>`
  - Sliding window algorithm
- **Per-Group Rate Limiting**:
  - Limit: 100 commands per minute per group
  - Key: `rate_limit:group:<group_id>`
  - Prevents group-wide flooding
- **Rate Limit Storage**:
  - Option 1: PostgreSQL table `rate_limits`
    - `key` (VARCHAR PRIMARY KEY)
    - `count` (INTEGER)
    - `window_start` (TIMESTAMP)
    - Cleanup old entries periodically
  - Option 2: In-memory with periodic persistence
- **Rate Limit Response**:
  - Return error message: "Rate limit exceeded. Please wait."
  - Log violation (for monitoring)
  - Don't process command
  - HTTP 429 equivalent (Telegram message)

### Permission and Authorization Model
- **Permission Storage**: Database table `permissions`
  - `id` (BIGSERIAL PRIMARY KEY)
  - `group_id` (BIGINT REFERENCES groups(id))
  - `telegram_user_id` (BIGINT)
  - `permission_type` (VARCHAR) - 'admin', 'player', 'viewer'
  - `granted_at` (TIMESTAMP)
  - `granted_by` (BIGINT)
- **Permission Checks**:
  - **Register Players**: Self-registration only (each user registers themselves)
  - **Create Matches**: Any user (if command author is one of the players, no approval needed)
  - **Undo Matches**: Only match players and group admins
  - **Modify ELO**: Only through match registration (no direct ELO modification)
  - **View Rankings**: Anyone in group
  - **Configure Topics**: Group admins only
- **Admin Detection**:
  - Check Telegram group admin status via Bot API
  - Cache admin list (refresh every 5 minutes)
  - Fallback: Database `permissions` table for manual admin assignment
- **Permission Enforcement**:
  - Check permissions before processing command
  - Return clear error message on permission denied
  - Log permission denials (for security monitoring)

### Abuse Pattern Prevention

#### Spam Command Prevention
- **Detection**:
  - Track command frequency per user
  - Detect rapid-fire commands (same command within 1 second)
  - Pattern: >5 identical commands in 10 seconds
- **Response**:
  - Ignore spam commands (don't process)
  - Log spam detection
  - Temporary rate limit increase (1 command per 10 seconds for 5 minutes)
- **Escalation**:
  - After 3 spam incidents: 1 command per minute for 1 hour
  - After 10 spam incidents: Report to admin, consider ban

#### Command Flooding Prevention
- **Detection**:
  - Monitor commands per second per group
  - Alert if >50 commands/second in single group
  - Track command queue depth
- **Response**:
  - Throttle group commands (process max 10/second)
  - Queue excess commands
  - Drop commands if queue >1000 (log warning)
- **Protection**: Rate limiting already handles this, but add group-level throttling

#### Impersonation Detection
- **Forwarded Message Detection**:
  - Check `message.forward_from` or `message.forward_from_chat`
  - Reject commands from forwarded messages
  - Return error: "Commands cannot be forwarded. Please send directly."
- **User Identity Verification**:
  - Always use `message.from.id` (cannot be spoofed by user)
  - Never trust user-provided user IDs in command text
  - Verify user exists in Telegram (via Bot API if needed)

#### Match Spamming Prevention
- **Duplicate Match Detection**:
  - Idempotency key prevents exact duplicates
  - Additional check: Same players, same scores within 1 minute = reject
- **Collusion Detection**:
  - Pattern: Same two players, many matches in short time
  - Rule: Max 10 matches between same two players per hour
  - Rule: Max 5 matches between same two players per day
  - Alert admin if threshold exceeded
- **Fake Match Prevention**:
  - Verify both players are in the group
  - Verify both players are verified (if verification required)
  - Reject matches where one player is the bot itself
  - Reject matches with invalid scores (e.g., both scores 0)
- **Match Validation Rules**:
  - At least one score must be > 0 (someone must win)
  - Scores must be reasonable (e.g., max 100 per game)
  - Players must be different
  - Players must exist and be active

### Security Logging
- **Security Events to Log**:
  - Permission denials
  - Rate limit violations
  - Spam detection
  - Impersonation attempts
  - Invalid input attempts
  - SQL injection attempts (if detected)
  - Match validation failures
- **Log Format**:
  - Structured logging (JSON)
  - Include: timestamp, user_id, group_id, event_type, details
  - Security log level (separate from application logs)
- **Log Retention**: 90 days for security logs

### Input Sanitization Details
- **Command Format Validation**:
  - `/match @player1 @player2 3 1` - strict format
  - Regex: `^/match @(\d+) @(\d+) (\d+) (\d+)$`
  - Extract and validate each component
- **Telegram User ID Validation**:
  - Must be positive integer
  - Range: 1 to 2^63-1 (Telegram's valid range)
  - Verify user exists (database lookup)
- **Score Validation**:
  - Must be non-negative integer
  - Range: 0 to 1000 (reasonable upper limit)
  - Reject negative or non-numeric

### Error Messages
- **Security-Conscious Error Messages**:
  - Don't reveal internal details (database errors, stack traces)
  - Generic error: "An error occurred. Please try again."
  - Log detailed errors internally
  - User-friendly messages: "Invalid command format", "Player not found", etc.

### Regular Security Audits
- **Code Review**: Security-focused code review for all changes
- **Dependency Scanning**: Regular updates for security vulnerabilities
- **Penetration Testing**: Periodic security testing
- **Log Analysis**: Regular review of security logs for patterns

## Consequences

### Positive
- **Security**: Multi-layered approach protects against various attack vectors
- **Abuse Prevention**: Comprehensive rules prevent common abuse patterns
- **User Trust**: Secure bot builds user confidence
- **Compliance**: Meets security best practices
- **Observability**: Security logging enables threat detection

### Negative
- **Complexity**: Multiple security layers add complexity
- **Performance**: Rate limiting and validation add overhead
- **False Positives**: Rate limiting may affect legitimate heavy users
- **Maintenance**: Need to update rules as abuse patterns evolve

### Neutral
- **Configuration**: Need to tune rate limits and thresholds
- **Monitoring**: Need to monitor security events and adjust rules

## Alternatives Considered

### No Rate Limiting
- **Rejected**: Vulnerable to abuse and DoS attacks

### Stricter Rate Limits
- **Considered**: 5 commands per minute
- **Decision**: 10 commands per minute balances security and usability

### Database-Only Rate Limiting
- **Approach**: Store all rate limits in database
- **Rejected**: Higher latency, more database load

### External Rate Limiting Service
- **Approach**: Use Redis or dedicated service
- **Rejected**: Adds dependency, database sufficient for this scale

### Whitelist-Only Approach
- **Approach**: Only allow known users
- **Rejected**: Bot needs to be open to group members

### No Impersonation Detection
- **Rejected**: Forwarded messages could be used to spoof commands

### Manual Abuse Detection
- **Rejected**: Doesn't scale, automated detection needed

### Harder Match Validation
- **Considered**: Require both players to confirm match
- **Decision**: Current rule (if author is player, no approval) balances security and usability

### Ban System
- **Considered**: Implement user banning
- **Decision**: Rate limiting sufficient for now, can add later if needed

