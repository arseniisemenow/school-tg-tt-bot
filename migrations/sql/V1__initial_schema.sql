-- Initial database schema for School Telegram Table Tennis Bot
-- Following ADR-002: Database Schema Design

-- Groups table
CREATE TABLE IF NOT EXISTS groups (
    id BIGSERIAL PRIMARY KEY,
    telegram_group_id BIGINT UNIQUE NOT NULL,
    name VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    is_active BOOLEAN DEFAULT TRUE
);

-- Players table
CREATE TABLE IF NOT EXISTS players (
    id BIGSERIAL PRIMARY KEY,
    telegram_user_id BIGINT NOT NULL,
    school_nickname VARCHAR(255),
    is_verified_student BOOLEAN DEFAULT FALSE,
    is_allowed_non_student BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    deleted_at TIMESTAMP WITH TIME ZONE
);

-- Partial unique index for active players (soft delete support)
CREATE UNIQUE INDEX IF NOT EXISTS unique_active_telegram_user 
    ON players(telegram_user_id) 
    WHERE deleted_at IS NULL;

-- Group players junction table
CREATE TABLE IF NOT EXISTS group_players (
    id BIGSERIAL PRIMARY KEY,
    group_id BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    player_id BIGINT NOT NULL REFERENCES players(id) ON DELETE CASCADE,
    current_elo INTEGER DEFAULT 1500,
    matches_played INTEGER DEFAULT 0,
    matches_won INTEGER DEFAULT 0,
    matches_lost INTEGER DEFAULT 0,
    version INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    CONSTRAINT unique_group_player UNIQUE (group_id, player_id)
);

-- Matches table
CREATE TABLE IF NOT EXISTS matches (
    id BIGSERIAL PRIMARY KEY,
    group_id BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    player1_id BIGINT NOT NULL REFERENCES players(id),
    player2_id BIGINT NOT NULL REFERENCES players(id),
    player1_score INTEGER NOT NULL CHECK (player1_score >= 0),
    player2_score INTEGER NOT NULL CHECK (player2_score >= 0),
    player1_elo_before INTEGER NOT NULL,
    player2_elo_before INTEGER NOT NULL,
    player1_elo_after INTEGER NOT NULL,
    player2_elo_after INTEGER NOT NULL,
    idempotency_key VARCHAR(255) UNIQUE NOT NULL,
    created_by_telegram_user_id BIGINT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    is_undone BOOLEAN DEFAULT FALSE,
    undone_at TIMESTAMP WITH TIME ZONE,
    undone_by_telegram_user_id BIGINT,  -- Reference to telegram_user_id, not a FK (soft reference)
    CONSTRAINT check_different_players CHECK (player1_id != player2_id)
);

-- ELO history table (audit trail)
CREATE TABLE IF NOT EXISTS elo_history (
    id BIGSERIAL PRIMARY KEY,
    match_id BIGINT REFERENCES matches(id) ON DELETE SET NULL,
    group_id BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    player_id BIGINT NOT NULL REFERENCES players(id),
    elo_before INTEGER NOT NULL,
    elo_after INTEGER NOT NULL,
    elo_change INTEGER NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    is_undone BOOLEAN DEFAULT FALSE
);

-- Group topics table
CREATE TABLE IF NOT EXISTS group_topics (
    id BIGSERIAL PRIMARY KEY,
    group_id BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    telegram_topic_id INTEGER,
    topic_type VARCHAR(50) NOT NULL,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    CONSTRAINT unique_group_topic_type UNIQUE (group_id, telegram_topic_id, topic_type)
);

-- Player verifications table
CREATE TABLE IF NOT EXISTS player_verifications (
    id BIGSERIAL PRIMARY KEY,
    player_id BIGINT NOT NULL REFERENCES players(id) ON DELETE CASCADE,
    group_id BIGINT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    school_nickname VARCHAR(255) NOT NULL,
    verification_status VARCHAR(50) NOT NULL,
    telegram_message_id BIGINT,
    verified_at TIMESTAMP WITH TIME ZONE,
    expires_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Failed operations table (Dead Letter Queue)
CREATE TABLE IF NOT EXISTS failed_operations (
    id BIGSERIAL PRIMARY KEY,
    operation_type VARCHAR(50) NOT NULL,
    operation_data JSONB,
    error_message TEXT,
    error_code VARCHAR(50),
    retry_count INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_retry_at TIMESTAMP WITH TIME ZONE,
    status VARCHAR(50) DEFAULT 'pending'
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_group_players_group_elo ON group_players(group_id, current_elo DESC);
CREATE INDEX IF NOT EXISTS idx_matches_group_created ON matches(group_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_matches_idempotency ON matches(idempotency_key);
CREATE INDEX IF NOT EXISTS idx_matches_player1_created ON matches(player1_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_matches_player2_created ON matches(player2_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_elo_history_player_created ON elo_history(player_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_elo_history_group_created ON elo_history(group_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_elo_history_match ON elo_history(match_id);
CREATE INDEX IF NOT EXISTS idx_group_topics_group_type ON group_topics(group_id, topic_type, is_active);
CREATE INDEX IF NOT EXISTS idx_player_verifications_player_group ON player_verifications(player_id, group_id, verification_status);
CREATE INDEX IF NOT EXISTS idx_player_verifications_expires ON player_verifications(expires_at);
CREATE INDEX IF NOT EXISTS idx_failed_operations_status ON failed_operations(status);

