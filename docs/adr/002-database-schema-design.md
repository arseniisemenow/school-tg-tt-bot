# ADR-002: Database Schema Design

## Status
Accepted

## Context
We need to design a database schema that supports:
- Multiple Telegram groups using the bot
- Users who can be members of multiple groups
- Match records between players
- ELO rating system with history/audit trail
- Player registration and verification (School21 integration)
- Group-specific configurations
- Undo operations for matches
- Soft/hard delete for GDPR compliance
- Performance requirements (indexes for common queries)
- Data integrity (prevent duplicate matches, invalid states)

Key requirements:
- Normalized schema to avoid data duplication
- Audit trail for all ELO changes
- Support for cross-group player relationships
- Efficient queries for rankings and match history
- Support for undo operations

## Decision
We will use a normalized PostgreSQL schema with the following design:

### Core Tables

#### `groups`
- `id` (BIGSERIAL PRIMARY KEY)
- `telegram_group_id` (BIGINT UNIQUE NOT NULL) - Telegram group chat ID
- `name` (VARCHAR(255)) - Optional group name
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `updated_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `is_active` (BOOLEAN DEFAULT TRUE) - Soft delete flag

#### `players`
- `id` (BIGSERIAL PRIMARY KEY)
- `telegram_user_id` (BIGINT NOT NULL) - Telegram user ID
- `school_nickname` (VARCHAR(255)) - School21 nickname (nullable)
- `is_verified_student` (BOOLEAN DEFAULT FALSE) - Verified via School21 API
- `is_allowed_non_student` (BOOLEAN DEFAULT FALSE) - Admin override for non-students
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `updated_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `deleted_at` (TIMESTAMP WITH TIME ZONE) - Soft delete timestamp (NULL = active)
- UNIQUE constraint on `telegram_user_id` where `deleted_at IS NULL`

#### `group_players` (Junction Table)
- `id` (BIGSERIAL PRIMARY KEY)
- `group_id` (BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE)
- `player_id` (BIGINT NOT NULL REFERENCES players(id) ON DELETE CASCADE)
- `current_elo` (INTEGER DEFAULT 1500) - Current ELO in this group
- `matches_played` (INTEGER DEFAULT 0)
- `matches_won` (INTEGER DEFAULT 0)
- `matches_lost` (INTEGER DEFAULT 0)
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `updated_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- UNIQUE constraint on `(group_id, player_id)`
- Index on `(group_id, current_elo DESC)` for ranking queries

#### `matches`
- `id` (BIGSERIAL PRIMARY KEY)
- `group_id` (BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE)
- `player1_id` (BIGINT NOT NULL REFERENCES players(id))
- `player2_id` (BIGINT NOT NULL REFERENCES players(id))
- `player1_score` (INTEGER NOT NULL CHECK (player1_score >= 0))
- `player2_score` (INTEGER NOT NULL CHECK (player2_score >= 0))
- `player1_elo_before` (INTEGER NOT NULL)
- `player2_elo_before` (INTEGER NOT NULL)
- `player1_elo_after` (INTEGER NOT NULL)
- `player2_elo_after` (INTEGER NOT NULL)
- `idempotency_key` (VARCHAR(255) UNIQUE NOT NULL) - message_id or command hash
- `created_by_telegram_user_id` (BIGINT NOT NULL) - Who created the match
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `is_undone` (BOOLEAN DEFAULT FALSE) - Undo flag
- `undone_at` (TIMESTAMP WITH TIME ZONE)
- `undone_by_telegram_user_id` (BIGINT REFERENCES players(telegram_user_id))
- CHECK constraint: `player1_id != player2_id`
- Index on `(group_id, created_at DESC)` for match history
- Index on `(idempotency_key)` for duplicate prevention
- Index on `(player1_id, created_at DESC)` and `(player2_id, created_at DESC)` for player history

#### `elo_history` (Audit Trail)
- `id` (BIGSERIAL PRIMARY KEY)
- `match_id` (BIGINT REFERENCES matches(id) ON DELETE SET NULL)
- `group_id` (BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE)
- `player_id` (BIGINT NOT NULL REFERENCES players(id))
- `elo_before` (INTEGER NOT NULL)
- `elo_after` (INTEGER NOT NULL)
- `elo_change` (INTEGER NOT NULL) - Computed: elo_after - elo_before
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- `is_undone` (BOOLEAN DEFAULT FALSE) - If the match was undone
- Index on `(player_id, created_at DESC)` for player ELO history
- Index on `(group_id, created_at DESC)` for group ELO history
- Index on `(match_id)` for match-related ELO changes

#### `group_topics` (Topic Configuration)
- `id` (BIGSERIAL PRIMARY KEY)
- `group_id` (BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE)
- `telegram_topic_id` (INTEGER) - Telegram topic ID (NULL for non-forum groups)
- `topic_type` (VARCHAR(50) NOT NULL) - 'id', 'ranking', 'matches'
- `is_active` (BOOLEAN DEFAULT TRUE)
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- UNIQUE constraint on `(group_id, telegram_topic_id, topic_type)`
- Index on `(group_id, topic_type, is_active)`

#### `player_verifications` (School21 Verification)
- `id` (BIGSERIAL PRIMARY KEY)
- `player_id` (BIGINT NOT NULL REFERENCES players(id) ON DELETE CASCADE)
- `group_id` (BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE)
- `school_nickname` (VARCHAR(255) NOT NULL)
- `verification_status` (VARCHAR(50) NOT NULL) - 'pending', 'verified', 'failed', 'expired'
- `telegram_message_id` (BIGINT) - Message ID in ID topic
- `verified_at` (TIMESTAMP WITH TIME ZONE)
- `expires_at` (TIMESTAMP WITH TIME ZONE) - 5 minutes after message creation
- `created_at` (TIMESTAMP WITH TIME ZONE DEFAULT NOW())
- Index on `(player_id, group_id, verification_status)`
- Index on `(expires_at)` for cleanup jobs

### Design Decisions

#### Normalization
- Separate `players` and `group_players` tables to support multi-group membership
- ELO is stored per-group in `group_players.current_elo`
- Match records reference players directly, not group_players (simpler)

#### ELO History Table
- Immutable audit trail: all ELO changes recorded
- One row per player per match (two rows per match total)
- Includes `is_undone` flag to track undo operations
- Allows point-in-time ELO reconstruction

#### Soft Delete Strategy
- `players.deleted_at` for GDPR compliance (hard delete after retention period)
- `groups.is_active` for soft delete (preserve history)
- `matches.is_undone` instead of delete (preserve audit trail)

#### Cross-Group Isolation
- ELO is per-group (stored in `group_players`)
- Matches are per-group (foreign key to `group_id`)
- Players can exist in multiple groups with different ELOs
- No cross-group contamination possible

#### Indexing Strategy
- Primary indexes on foreign keys
- Composite indexes for common queries:
  - Ranking: `(group_id, current_elo DESC)`
  - Match history: `(group_id, created_at DESC)`
  - Player history: `(player_id, created_at DESC)`
  - Idempotency: `(idempotency_key)` unique index

#### Constraints
- Foreign keys with CASCADE for data integrity
- CHECK constraints for score validation and self-match prevention
- UNIQUE constraints for idempotency and group-player relationships
- NOT NULL constraints on critical fields

## Consequences

### Positive
- **Data Integrity**: Foreign keys and constraints prevent invalid states
- **Performance**: Strategic indexes support fast ranking and history queries
- **Audit Trail**: Complete ELO history enables undo and debugging
- **Multi-Group Support**: Clean separation allows players in multiple groups
- **Normalization**: Reduces data duplication and update anomalies
- **Undo Support**: `is_undone` flag preserves history while allowing reversals

### Negative
- **Complexity**: More tables and relationships than denormalized approach
- **Join Overhead**: Some queries require joins (mitigated by indexes)
- **Storage**: ELO history table will grow over time (need cleanup strategy)

### Neutral
- **Migration Complexity**: More tables to migrate, but Flyway handles this
- **Query Complexity**: Some queries need joins, but PostgreSQL handles efficiently

## Alternatives Considered

### Denormalized Schema
- **Rejected**: Would duplicate player data per group, harder to maintain consistency

### Single ELO Table
- **Rejected**: Would require complex queries to get current ELO, harder to maintain history

### Event Sourcing
- **Considered**: Full event sourcing for ELO changes
- **Rejected**: Overkill for this use case, adds complexity without clear benefit

### Separate ELO Table (No History)
- **Rejected**: Need audit trail for undo operations and debugging

### Global ELO (Not Per-Group)
- **Rejected**: Requirement is per-group ELO to prevent cross-group contamination

### Hard Delete Only
- **Rejected**: Need soft delete for GDPR compliance and user rejoin scenarios

