# ADR-009: Testing Strategy

## Status
Accepted

## Context
The bot requires comprehensive testing to ensure:
- Correct ELO calculations
- Database operations work correctly
- Integration with Telegram API
- Backup/restore functionality
- Concurrent operations
- Error handling
- **Abuse prevention and security** (critical)

Key requirements:
- Unit tests for ELO calculations
- Integration tests with test database
- Mock Telegram API for testing
- Backup/restore testing
- Load testing for concurrent operations (optional)
- **Abuse pattern testing** (very important)

## Decision
We will implement a comprehensive testing strategy with multiple test levels:

### Unit Testing Strategy

#### Test Framework
- **Framework**: Google Test (gtest) or Catch2
- **Rationale**: 
  - Industry standard for C++
  - Good CMake integration
  - Rich assertion macros
  - Test fixtures support
- **Build Integration**: Tests built as separate executables

#### ELO Calculation Unit Tests
- **Test Cases**:
  - Basic ELO calculation (win/loss)
  - ELO change with different K-factors
  - ELO change with different rating differences
  - Edge cases: very high/low ELO differences
  - ELO cannot go negative
  - ELO change symmetry (if player1 wins, player2 loses equivalent)
- **Test Data**: 
  - Known input/output pairs
  - Boundary values
  - Invalid inputs (negative scores, etc.)
- **Coverage Target**: 100% coverage for ELO calculation logic

#### Other Unit Tests
- **Command Parsing**:
  - Valid command formats
  - Invalid command formats
  - Edge cases (extra spaces, missing parameters)
- **Input Validation**:
  - User ID validation
  - Score validation
  - String sanitization
- **Utility Functions**:
  - Date/time formatting
  - String utilities
  - Configuration parsing

#### Test Organization
- **Structure**: Mirror source code structure
- **Naming**: `test_<module>_<functionality>.cpp`
- **Fixtures**: Use test fixtures for common setup
- **Mocking**: Mock external dependencies (database, Telegram API)

### Integration Testing Strategy

#### Test Database Setup
- **Database**: Separate PostgreSQL database for testing
- **Setup**:
  - Docker Compose with test database
  - Flyway migrations run on test database
  - Test data seeding
- **Isolation**: Each test gets clean database state
- **Teardown**: Clean up after each test

#### Integration Test Scope
- **Database Operations**:
  - Player registration
  - Match creation
  - ELO updates
  - Undo operations
  - Transaction rollback
- **End-to-End Flows**:
  - Complete match registration flow
  - Player registration → match → ELO update
  - Undo match flow
- **Concurrent Operations**:
  - Multiple matches simultaneously
  - Race condition testing
  - Optimistic locking verification

#### Test Data Management
- **Fixtures**: Predefined test data sets
- **Factories**: Test data factories for dynamic data
- **Cleanup**: Automatic cleanup after tests
- **Seeding**: Seed scripts for common scenarios

### Telegram API Mocking

#### Mock Strategy
- **Approach**: Mock Telegram Bot API responses
- **Implementation Options**:
  1. **Mock Server**: HTTP server that mimics Telegram API
  2. **Library Mock**: Mock tgbotxx library calls
  3. **Interface Abstraction**: Abstract Telegram API behind interface, mock interface

#### Decision: Mock Server
- **Tool**: Custom mock server or use existing tool
- **Implementation**:
  - Simple HTTP server that responds to Telegram API endpoints
  - Configurable responses for different scenarios
  - Record requests for verification
- **Benefits**:
  - Tests real HTTP communication
  - Can test error scenarios
  - No dependency on real Telegram API

#### Mock Scenarios
- **Success Cases**:
  - Message sent successfully
  - User information retrieved
  - Group information retrieved
- **Error Cases**:
  - Rate limit (429)
  - Invalid token (401)
  - Network errors
  - Timeout errors

#### Test Isolation
- **Per-Test Mock**: Each test gets fresh mock server
- **State Reset**: Reset mock state between tests
- **Request Verification**: Verify correct API calls made

### Backup/Restore Testing

#### Test Strategy
- **Frequency**: Weekly (automated)
- **Process**:
  1. Create test database with known data
  2. Run backup
  3. Modify/delete test database
  4. Restore from backup
  5. Verify data matches original
- **Verification**:
  - Row counts match
  - Data integrity (foreign keys)
  - ELO values correct
  - Match history intact

#### Test Scenarios
- **Full Backup/Restore**: Complete database restore
- **Point-in-Time Recovery**: Restore to specific timestamp
- **WAL Archiving**: Verify WAL files archived correctly
- **Backup Corruption**: Test handling of corrupted backups
- **Restore Failure**: Test error handling on restore failure

#### Automated Testing
- **CI/CD Integration**: Run backup/restore tests in CI
- **Test Environment**: Use separate test database
- **Cleanup**: Clean up test backups after verification

### Load Testing Strategy

#### Purpose
- Verify system handles concurrent operations
- Identify performance bottlenecks
- Test rate limiting
- Verify connection pooling

#### Load Testing Approach
- **Tool**: Custom load testing script or tool (e.g., k6, Apache Bench)
- **Scenarios**:
  - Concurrent match registrations (10, 50, 100 simultaneous)
  - Concurrent ELO updates
  - Database connection pool exhaustion
  - Rate limit handling under load
- **Metrics**:
  - Request latency (p50, p95, p99)
  - Throughput (requests per second)
  - Error rate
  - Database connection pool usage
  - CPU and memory usage

#### Load Testing Frequency
- **Before Major Releases**: Run load tests
- **Periodic**: Monthly load tests
- **After Performance Changes**: Run load tests

#### Load Testing Environment
- **Separate Environment**: Use dedicated test environment
- **Realistic Data**: Use realistic data volumes
- **Monitoring**: Monitor system during load tests

### Abuse Pattern Testing Strategy

#### Purpose
- Verify abuse prevention mechanisms work correctly
- Test security measures against various attack vectors
- Ensure rate limiting and validation prevent abuse
- Validate permission checks and authorization
- Test edge cases that could be exploited

#### Test Categories

##### Spam Command Testing
- **Test Cases**:
  - Rapid-fire identical commands (>5 in 10 seconds)
  - Same command repeated 100+ times
  - Commands with minimal delay between them
- **Expected Behavior**:
  - Commands after threshold are ignored
  - Rate limit increased for offending user
  - Spam detection logged
  - User receives rate limit message after threshold
- **Test Scenarios**:
  - Single user spamming `/match` command
  - Single user spamming `/ranking` command
  - Multiple users spamming simultaneously
  - Spam recovery (user stops spamming, rate limit resets)

##### Command Flooding Testing
- **Test Cases**:
  - 50+ commands per second in single group
  - 100+ commands per second across all groups
  - Mixed command types flooding
- **Expected Behavior**:
  - Group-level throttling activates (>10 commands/second)
  - Commands queued if queue not full
  - Commands dropped if queue >1000 (with warning)
  - System remains responsive
- **Test Scenarios**:
  - Coordinated flooding from multiple users
  - Uncoordinated flooding (natural high traffic)
  - Flooding during normal operations
  - Recovery after flooding stops

##### Impersonation Testing
- **Test Cases**:
  - Forwarded message with command
  - Message with spoofed user information
  - Command from forwarded context
- **Expected Behavior**:
  - Forwarded commands are rejected
  - Error message: "Commands cannot be forwarded"
  - Impersonation attempt logged
  - Original user ID always used (from `message.from.id`)
- **Test Scenarios**:
  - Forward `/match` command
  - Forward `/ranking` command
  - Forward `/id` command
  - Multiple forwarded messages
  - Verify `message.from.id` is always used (not user-provided)

##### Match Spamming Testing
- **Test Cases**:
  - Same two players, 10+ matches in 1 hour
  - Same two players, 5+ matches in 1 day
  - Rapid matches between same players (<1 minute apart)
  - Matches with invalid scores (both 0, negative, etc.)
- **Expected Behavior**:
  - Matches exceeding threshold are rejected
  - Alert sent to admin if threshold exceeded
  - Invalid matches rejected with clear error
  - Collusion detection logged
- **Test Scenarios**:
  - Legitimate rapid matches (tournament scenario)
  - Collusion attempt (same players, many matches)
  - Fake matches (invalid scores)
  - Self-match attempts (player1 == player2)
  - Matches with non-existent players

##### Rate Limiting Testing
- **Test Cases**:
  - User exceeds 10 commands/minute limit
  - Group exceeds 100 commands/minute limit
  - Rate limit reset after time window
  - Rate limit persistence across restarts
- **Expected Behavior**:
  - Rate limit violations return error message
  - Rate limit violations logged
  - Rate limit resets after time window
  - Different limits for different operations (if applicable)
- **Test Scenarios**:
  - Per-user rate limit enforcement
  - Per-group rate limit enforcement
  - Rate limit sliding window behavior
  - Rate limit with concurrent requests
  - Rate limit recovery (user waits, then can proceed)

##### Permission Bypass Testing
- **Test Cases**:
  - Non-admin trying to undo match (not a player)
  - Non-player trying to create match for others
  - Regular user trying to configure topics
  - User trying to modify ELO directly
- **Expected Behavior**:
  - Permission checks prevent unauthorized operations
  - Clear error messages on permission denial
  - Permission denials logged
  - Admin operations require admin status
- **Test Scenarios**:
  - Undo permission: only players and admins
  - Match creation: any user (if author is player, no approval)
  - Topic configuration: admins only
  - Player registration: self-registration only
  - Admin detection: verify Telegram admin status checked

##### SQL Injection Testing
- **Test Cases**:
  - Commands with SQL injection attempts in parameters
  - User IDs with SQL injection payloads
  - Scores with SQL injection payloads
  - School nicknames with SQL injection payloads
- **Expected Behavior**:
  - All inputs sanitized and parameterized
  - SQL injection attempts fail safely
  - No database errors exposed to user
  - Injection attempts logged for security monitoring
- **Test Scenarios**:
  - `'; DROP TABLE matches; --` in various fields
  - `' OR '1'='1` in user ID
  - Union-based injection attempts
  - Time-based blind SQL injection attempts
  - Verify parameterized queries used (no string concatenation)

##### Input Validation Bypass Testing
- **Test Cases**:
  - Negative scores
  - Non-numeric scores
  - Invalid user IDs (negative, zero, very large)
  - Invalid command formats
  - Extremely long inputs
  - Special characters in inputs
  - Unicode and emoji in inputs
- **Expected Behavior**:
  - Invalid inputs rejected with clear errors
  - Input validation at multiple levels
  - No crashes or errors from invalid input
  - Validation errors logged
- **Test Scenarios**:
  - `/match @player1 @player2 -1 3` (negative score)
  - `/match @player1 @player2 abc 3` (non-numeric)
  - `/match @player1 @player2 999999 0` (extreme values)
  - `/match @player1 @player2 3 1 extra` (extra parameters)
  - Very long usernames or nicknames
  - Control characters in inputs

##### Idempotency Testing
- **Test Cases**:
  - Duplicate match with same message_id
  - Duplicate match with same command hash
  - Retry of same command
  - Concurrent duplicate commands
- **Expected Behavior**:
  - Duplicate matches rejected (idempotent)
  - No ELO double-update
  - Success response (idempotent operation)
  - Duplicate detection logged
- **Test Scenarios**:
  - Same message processed twice
  - Same command sent multiple times
  - Concurrent duplicate commands
  - Idempotency key collision handling

##### ELO Manipulation Testing
- **Test Cases**:
  - Attempts to create matches that would make ELO negative
  - Attempts to create matches with extreme ELO changes
  - Attempts to bypass ELO validation
  - Attempts to directly modify ELO (if such endpoint exists)
- **Expected Behavior**:
  - ELO cannot go negative (enforced)
  - ELO changes within reasonable bounds
  - ELO validation at application and database level
  - Direct ELO modification not allowed
- **Test Scenarios**:
  - Match that would result in negative ELO
  - Match with extreme rating difference (>2000 points)
  - Verify ELO constraints in database
  - Verify ELO calculation cannot be bypassed

##### Cross-Group Contamination Testing
- **Test Cases**:
  - User in Group A trying to create match in Group B
  - User trying to view rankings from different group
  - ELO from one group affecting another
- **Expected Behavior**:
  - Group isolation enforced
  - Users can only interact with their group
  - ELO per-group (no cross-contamination)
  - Group context always validated
- **Test Scenarios**:
  - Match creation with players from different groups
  - Ranking query from wrong group
  - Verify group_id always checked
  - Verify ELO stored per-group

#### Abuse Test Implementation

##### Test Framework
- **Integration with Existing Tests**: Abuse tests as integration tests
- **Test Fixtures**: Create abuse scenario fixtures
- **Mock Setup**: Configure mocks for abuse scenarios
- **Assertions**: Verify abuse prevention worked

##### Test Data for Abuse Testing
- **Malicious Inputs**: Library of SQL injection, XSS, command injection payloads
- **Rate Limit Scenarios**: Scripts to simulate rate limit violations
- **Permission Scenarios**: Test users with different permission levels
- **Edge Cases**: Boundary values, extreme inputs

##### Automated Abuse Testing
- **CI/CD Integration**: Run abuse tests in CI pipeline
- **Frequency**: Run on every pull request
- **Test Suite**: Dedicated abuse test suite
- **Reporting**: Detailed reports on abuse test results

##### Manual Penetration Testing
- **Frequency**: Quarterly or before major releases
- **Scope**: Full security audit
- **Tools**: Use security testing tools
- **External Audit**: Consider external security audit

#### Abuse Test Metrics
- **Coverage**: All abuse patterns from ADR-006 covered
- **Pass Rate**: 100% pass rate required (no false positives)
- **Performance**: Abuse tests should not significantly slow CI
- **Maintenance**: Update tests as new abuse patterns emerge

### Test Organization

#### Test Structure
```
tests/
  unit/
    test_elo_calculation.cpp
    test_command_parsing.cpp
    test_input_validation.cpp
  integration/
    test_database_operations.cpp
    test_match_flow.cpp
    test_concurrent_operations.cpp
  abuse/
    test_spam_prevention.cpp
    test_command_flooding.cpp
    test_impersonation.cpp
    test_match_spamming.cpp
    test_rate_limiting.cpp
    test_permission_bypass.cpp
    test_sql_injection.cpp
    test_input_validation_bypass.cpp
    test_idempotency.cpp
    test_elo_manipulation.cpp
    test_cross_group_contamination.cpp
  e2e/
    test_full_workflow.cpp
  mocks/
    telegram_api_mock.cpp
  fixtures/
    test_data.cpp
    abuse_scenarios.cpp
```

#### Test Execution
- **Unit Tests**: Fast, run on every build
- **Integration Tests**: Slower, run in CI/CD
- **Abuse Tests**: Critical, run in CI/CD on every PR
- **Load Tests**: Slowest, run periodically
- **Test Categories**: Use test tags/categories for selective execution

### Test Coverage

#### Coverage Targets
- **ELO Calculation**: 100% coverage
- **Core Business Logic**: >90% coverage
- **Security/Abuse Prevention**: 100% coverage (critical)
- **Overall**: >70% coverage
- **Critical Paths**: 100% coverage

#### Coverage Tools
- **Tool**: gcov + lcov or similar
- **Reporting**: Generate HTML coverage reports
- **CI Integration**: Fail build if coverage below threshold

### Test Data Management

#### Test Fixtures
- **Predefined Data**: Common test scenarios
- **Factories**: Generate test data dynamically
- **Builders**: Fluent API for building test objects

#### Test Database Seeding
- **Seed Scripts**: SQL scripts for test data
- **Flyway Integration**: Use Flyway for test data migrations
- **Cleanup**: Automatic cleanup after tests

### Continuous Testing

#### CI/CD Integration
- **Unit Tests**: Run on every commit
- **Integration Tests**: Run on pull requests
- **Abuse Tests**: Run on every pull request (critical, must pass)
- **Full Test Suite**: Run before merge to main
- **Test Reports**: Publish test results and coverage
- **Security Reports**: Publish abuse test results separately

#### Test Failure Handling
- **Fail Fast**: Stop on first test failure (configurable)
- **Test Reports**: Detailed failure reports
- **Retry Flaky Tests**: Retry transient failures

### Performance Testing

#### Performance Benchmarks
- **ELO Calculation**: <1ms per calculation
- **Database Query**: <10ms for simple queries
- **Match Registration**: <100ms end-to-end
- **Response Time**: <500ms for user-facing operations

#### Benchmark Tests
- **Tool**: Google Benchmark or similar
- **CI Integration**: Track benchmark results over time
- **Regression Detection**: Alert on performance regressions

## Consequences

### Positive
- **Quality Assurance**: Comprehensive testing ensures quality
- **Confidence**: Tests provide confidence in changes
- **Documentation**: Tests serve as documentation
- **Regression Prevention**: Tests catch regressions early
- **Refactoring Safety**: Tests enable safe refactoring
- **Security**: Abuse testing ensures security measures work correctly
- **Abuse Prevention**: Comprehensive abuse testing prevents exploitation

### Negative
- **Development Time**: Writing tests takes time
- **Maintenance**: Tests need maintenance as code changes
- **Test Infrastructure**: Need to maintain test infrastructure
- **Flaky Tests**: Some tests may be flaky (need fixing)

### Neutral
- **CI/CD Time**: Tests add time to CI/CD pipeline
- **Test Data**: Need to manage test data

## Alternatives Considered

### No Unit Tests
- **Rejected**: Unit tests essential for ELO calculation correctness

### Manual Testing Only
- **Rejected**: Doesn't scale, error-prone, not repeatable

### Integration Tests Only (No Unit Tests)
- **Rejected**: Unit tests are faster and catch issues earlier

### Real Telegram API for Testing
- **Rejected**: Unreliable, rate limits, requires real bot token

### No Load Testing
- **Considered**: Skip load testing initially
- **Decision**: Include load testing for critical concurrent operations

### Higher Coverage Target (90%+)
- **Considered**: 90% overall coverage
- **Decision**: 70% overall with 100% for critical paths is more practical

### No Backup/Restore Testing
- **Rejected**: Backup/restore is critical, must be tested

### External Test Framework
- **Considered**: Use external testing service
- **Rejected**: Google Test/Catch2 sufficient, no need for external service

### No Abuse Testing
- **Rejected**: Abuse testing is critical for security, must be comprehensive

### Manual Abuse Testing Only
- **Rejected**: Automated abuse tests ensure consistent coverage and catch regressions

### Limited Abuse Testing
- **Considered**: Test only common abuse patterns
- **Decision**: Comprehensive abuse testing covers all patterns from ADR-006

