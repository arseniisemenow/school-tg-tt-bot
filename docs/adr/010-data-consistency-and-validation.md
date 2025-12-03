# ADR-010: Data Consistency and Validation

## Status
Accepted

## Context
The bot must ensure data consistency and validity:
- ELO ratings must be accurate and consistent
- Matches must be valid (players exist, scores valid)
- Database transactions must maintain ACID properties
- ELO cannot go negative
- All ELO changes must be auditable
- Data integrity must be enforced at database level

Key requirements:
- Database transactions for ELO updates
- ELO validation rules
- Event log for all ELO changes
- Data consistency guarantees
- Validation at multiple levels (application and database)

## Decision
We will implement comprehensive data consistency and validation strategies:

### Database Transactions for ELO Updates

#### Transaction Boundaries
- **Match Registration Transaction**:
  - Begin transaction
  - Validate match (players exist, not duplicate, etc.)
  - Update `group_players` for player1 (with optimistic locking)
  - Update `group_players` for player2 (with optimistic locking)
  - Insert `matches` record
  - Insert `elo_history` records (2 rows: one per player)
  - Commit transaction
- **Undo Operation Transaction**:
  - Begin transaction
  - Validate undo permission
  - Reverse ELO changes (update `group_players`)
  - Mark match as undone
  - Insert reverse `elo_history` entries
  - Commit transaction

#### Transaction Isolation Level
- **Level**: READ COMMITTED (PostgreSQL default)
- **Rationale**:
  - Prevents dirty reads
  - Allows concurrent transactions
  - Good balance between consistency and performance
  - Sufficient for this use case
- **Serializable Isolation**: Not needed (optimistic locking handles conflicts)

#### ACID Compliance
- **Atomicity**: All-or-nothing (transaction rollback on error)
- **Consistency**: Database constraints ensure consistency
- **Isolation**: READ COMMITTED isolation level
- **Durability**: PostgreSQL WAL ensures durability

#### Transaction Error Handling
- **Automatic Rollback**: Any exception triggers rollback
- **Explicit Rollback**: Rollback on validation failures
- **Error Logging**: Log all transaction failures with context
- **User Notification**: Inform user of transaction failures

### ELO Validation Rules

#### ELO Cannot Go Negative
- **Validation Level**: Application and database
- **Application Validation**:
  - After calculating new ELO, check: `new_elo >= 0`
  - If negative, set to 0
  - Log warning if ELO would have gone negative
- **Database Constraint**:
  - `CHECK (current_elo >= 0)` on `group_players.current_elo`
  - `CHECK (elo_after >= 0)` on `elo_history.elo_after`
  - Database enforces constraint (fail transaction if violated)

#### ELO Maximum Limit
- **Maximum ELO**: 10000 (configurable, reasonable upper bound)
- **Validation**:
  - Application: Check `new_elo <= MAX_ELO`
  - Database: `CHECK (current_elo <= 10000)`
- **Rationale**: Prevent unrealistic ELO values

#### ELO Change Validation
- **Reasonable Change**: ELO change should be within expected range
  - Typical change: ±50 points per match
  - Maximum expected: ±200 points (extreme rating difference)
  - Alert if change > 200 points (potential bug)
- **ELO Change Symmetry**: 
  - If player1 gains X points, player2 should lose approximately X points
  - Small differences allowed (rounding, K-factor variations)
  - Verify: `abs((player1_change + player2_change)) < 1` (within rounding)

#### ELO Calculation Validation
- **Formula Verification**: Unit tests verify ELO calculation formula
- **Known Test Cases**: Test with known input/output pairs
- **Edge Cases**: Test extreme rating differences
- **K-Factor Validation**: Verify K-factor is reasonable (typically 32)

### Event Log for ELO Changes

#### Immutable Event Log
- **Table**: `elo_history` (see ADR-002)
- **Immutability**: 
  - Never update or delete `elo_history` records
  - Only insert new records
  - Undo operations create reverse entries (not updates)
- **Completeness**: Every ELO change must have corresponding `elo_history` entry

#### Event Log Contents
- **Required Fields**:
  - `match_id`: Link to match (nullable for manual adjustments)
  - `group_id`: Group context
  - `player_id`: Player whose ELO changed
  - `elo_before`: ELO before change
  - `elo_after`: ELO after change
  - `elo_change`: Computed change (elo_after - elo_before)
  - `created_at`: Timestamp of change
  - `is_undone`: Flag if match was undone
- **Audit Trail**: Complete history of all ELO changes

#### Event Log Validation
- **Consistency Check**: 
  - `elo_history.elo_after` should match `group_players.current_elo`
  - Periodic validation job to check consistency
  - Alert on inconsistencies
- **Completeness Check**:
  - Every match should have 2 `elo_history` entries
  - Every ELO update should have corresponding entry
  - Validation query to detect missing entries

### Data Integrity Enforcement

#### Database Constraints
- **Foreign Keys**: 
  - `matches.player1_id` → `players.id`
  - `matches.player2_id` → `players.id`
  - `matches.group_id` → `groups.id`
  - `elo_history.player_id` → `players.id`
  - `elo_history.group_id` → `groups.id`
  - `group_players.group_id` → `groups.id`
  - `group_players.player_id` → `players.id`
- **Unique Constraints**:
  - `matches.idempotency_key` UNIQUE (prevent duplicates)
  - `(group_id, player_id)` UNIQUE on `group_players`
  - `(group_id, telegram_topic_id, topic_type)` UNIQUE on `group_topics`
- **Check Constraints**:
  - `group_players.current_elo >= 0 AND current_elo <= 10000`
  - `elo_history.elo_after >= 0 AND elo_after <= 10000`
  - `matches.player1_id != matches.player2_id` (no self-matches)
  - `matches.player1_score >= 0 AND player2_score >= 0`
- **NOT NULL Constraints**: Critical fields marked NOT NULL

#### Application-Level Validation
- **Input Validation**: Validate all user inputs (see ADR-006)
- **Business Rule Validation**:
  - Players must exist and be active
  - Players must be in the group
  - Match must not be duplicate (idempotency check)
  - Scores must be valid (non-negative, reasonable)
- **State Validation**:
  - Verify players exist before match creation
  - Verify group exists and is active
  - Verify permissions before operations

### Consistency Checks

#### Periodic Consistency Validation
- **Frequency**: Daily (during low-traffic period)
- **Checks**:
  1. **ELO Consistency**:
     - `group_players.current_elo` should match latest `elo_history.elo_after`
     - Query: Find mismatches
  2. **Match Count Consistency**:
     - `group_players.matches_played` should match count of matches
     - Query: Compare counts
  3. **Win/Loss Count Consistency**:
     - `group_players.matches_won` + `matches_lost` should equal `matches_played`
     - Query: Verify arithmetic
  4. **ELO History Completeness**:
     - Every match should have 2 `elo_history` entries
     - Query: Find matches with missing history
- **Alerting**: Alert on any inconsistencies found
- **Auto-Repair**: Attempt to repair inconsistencies (with logging)

#### Real-Time Consistency Checks
- **On ELO Update**: Verify ELO consistency after update
- **On Match Creation**: Verify match data integrity
- **On Undo**: Verify undo operation consistency

### Validation at Multiple Levels

#### Level 1: Input Validation (User Input)
- **Command Parsing**: Validate command format
- **Parameter Validation**: Validate user IDs, scores, etc.
- **Type Checking**: Ensure correct data types

#### Level 2: Business Logic Validation
- **Business Rules**: Validate business rules (players exist, permissions, etc.)
- **State Validation**: Verify system state allows operation
- **ELO Calculation**: Validate ELO calculation inputs

#### Level 3: Database Constraints
- **Database-Level**: Database enforces constraints
- **Last Line of Defense**: Catches any validation misses
- **Fail Fast**: Database rejects invalid data immediately

### ELO Calculation Consistency

#### Calculation Formula
- **Standard ELO Formula**: Use standard ELO rating system
- **K-Factor**: Configurable (default: 32)
- **Expected Score**: `E = 1 / (1 + 10^((R2 - R1) / 400))`
- **New Rating**: `R' = R + K * (S - E)`
  - `R`: Current rating
  - `K`: K-factor
  - `S`: Actual score (1 for win, 0 for loss)
  - `E`: Expected score

#### Calculation Verification
- **Unit Tests**: Comprehensive unit tests for ELO calculation
- **Known Test Cases**: Test with known rating changes
- **Symmetry Check**: Verify win/loss symmetry
- **Edge Cases**: Test extreme rating differences

### Data Validation Rules Summary

#### Match Validation
- Players must exist and be active
- Players must be in the group
- Players must be different (no self-matches)
- Scores must be non-negative integers
- At least one score must be > 0 (someone must win)
- Match must not be duplicate (idempotency key check)

#### ELO Validation
- ELO cannot be negative (enforced at app and DB level)
- ELO cannot exceed maximum (10000)
- ELO change must be reasonable (±200 points max, alert if exceeded)
- ELO change symmetry (player1 gain ≈ player2 loss)

#### Player Validation
- Telegram user ID must be valid (positive integer)
- School nickname must match format (if provided)
- Player must be verified or allowed (if verification required)

#### Group Validation
- Group must exist and be active
- Group must have valid Telegram group ID

## Consequences

### Positive
- **Data Integrity**: Multiple validation layers ensure data integrity
- **Consistency**: Transactions and constraints maintain consistency
- **Auditability**: Complete event log enables audit trail
- **Error Prevention**: Validation catches errors early
- **Confidence**: Strong validation provides confidence in data

### Negative
- **Performance**: Validation adds overhead (minimal)
- **Complexity**: Multiple validation layers add complexity
- **Maintenance**: Need to maintain validation rules

### Neutral
- **Database Load**: Constraints add minimal database load
- **Development Time**: Writing validation takes time

## Alternatives Considered

### No Database Constraints (Application Only)
- **Rejected**: Database constraints are last line of defense

### Serializable Isolation Level
- **Rejected**: Too restrictive, optimistic locking sufficient

### No ELO Maximum Limit
- **Rejected**: Need upper bound to prevent unrealistic values

### Update ELO History (Not Immutable)
- **Rejected**: Immutability enables complete audit trail

### No Consistency Checks
- **Rejected**: Need to detect and fix inconsistencies

### Stricter Validation (Reject More)
- **Considered**: Reject more edge cases
- **Decision**: Current validation balanced between safety and usability

### No Event Log (Just Current ELO)
- **Rejected**: Need audit trail for debugging and undo operations



