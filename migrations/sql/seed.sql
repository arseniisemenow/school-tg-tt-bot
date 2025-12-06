-- Seed data for development environment
BEGIN;

-- Clear existing data and reset identities
TRUNCATE TABLE
    elo_history,
    matches,
    group_topics,
    player_verifications,
    group_players,
    players,
    groups,
    failed_operations
    RESTART IDENTITY CASCADE;

-- Groups
INSERT INTO groups (telegram_group_id, name, is_active, created_at, updated_at)
VALUES
    (1000000001, 'Campus TT Club', TRUE, NOW(), NOW()),
    (1000000002, 'Weekend Ping Pong', TRUE, NOW(), NOW());

-- Players
INSERT INTO players (telegram_user_id, school_nickname, is_verified_student, is_allowed_non_student, created_at, updated_at)
VALUES
    (500001, 'ace.alex', TRUE, FALSE, NOW(), NOW()),
    (500002, 'spin.sara', TRUE, FALSE, NOW(), NOW()),
    (500003, 'smash.sam', FALSE, TRUE, NOW(), NOW());

-- Group membership with starting ELO
INSERT INTO group_players (group_id, player_id, current_elo, matches_played, matches_won, matches_lost, created_at, updated_at)
VALUES
    (1, 1, 1520, 0, 0, 0, NOW(), NOW()),
    (1, 2, 1500, 0, 0, 0, NOW(), NOW()),
    (1, 3, 1480, 0, 0, 0, NOW(), NOW()),
    (2, 2, 1510, 0, 0, 0, NOW(), NOW()),
    (2, 3, 1490, 0, 0, 0, NOW(), NOW());

-- Matches (IDs will start at 1 after TRUNCATE RESTART IDENTITY)
INSERT INTO matches (
    group_id,
    player1_id,
    player2_id,
    player1_score,
    player2_score,
    player1_elo_before,
    player2_elo_before,
    player1_elo_after,
    player2_elo_after,
    idempotency_key,
    created_by_telegram_user_id,
    created_at,
    is_undone
) VALUES
    (1, 1, 2, 11, 8, 1520, 1500, 1535, 1485, 'match-1-group-1', 500001, NOW() - INTERVAL '2 days', FALSE),
    (1, 2, 3, 11, 9, 1485, 1480, 1495, 1470, 'match-2-group-1', 500002, NOW() - INTERVAL '1 day', FALSE),
    (2, 2, 3, 11, 6, 1510, 1490, 1525, 1475, 'match-1-group-2', 500002, NOW() - INTERVAL '3 days', FALSE);

-- ELO history for matches above
INSERT INTO elo_history (match_id, group_id, player_id, elo_before, elo_after, elo_change, created_at, is_undone)
VALUES
    (1, 1, 1, 1520, 1535, 15, NOW() - INTERVAL '2 days', FALSE),
    (1, 1, 2, 1500, 1485, -15, NOW() - INTERVAL '2 days', FALSE),
    (2, 1, 2, 1485, 1495, 10, NOW() - INTERVAL '1 day', FALSE),
    (2, 1, 3, 1480, 1470, -10, NOW() - INTERVAL '1 day', FALSE),
    (3, 2, 2, 1510, 1525, 15, NOW() - INTERVAL '3 days', FALSE),
    (3, 2, 3, 1490, 1475, -15, NOW() - INTERVAL '3 days', FALSE);

-- Group topics
INSERT INTO group_topics (group_id, telegram_topic_id, topic_type, is_active, created_at)
VALUES
    (1, 101, 'general', TRUE, NOW()),
    (1, 102, 'matches', TRUE, NOW()),
    (2, 201, 'general', TRUE, NOW());

-- Player verifications
INSERT INTO player_verifications (player_id, group_id, school_nickname, verification_status, telegram_message_id, verified_at, expires_at, created_at)
VALUES
    (1, 1, 'ace.alex', 'approved', 900001, NOW() - INTERVAL '2 days', NOW() + INTERVAL '28 days', NOW() - INTERVAL '2 days'),
    (2, 1, 'spin.sara', 'approved', 900002, NOW() - INTERVAL '1 day', NOW() + INTERVAL '29 days', NOW() - INTERVAL '1 day'),
    (3, 2, 'smash.sam', 'pending', 900003, NULL, NOW() + INTERVAL '7 days', NOW() - INTERVAL '12 hours');

-- Failed operations (DLQ) sample
INSERT INTO failed_operations (operation_type, operation_data, error_message, error_code, retry_count, created_at, last_retry_at, status)
VALUES
    ('send_notification', '{"group_id":1,"reason":"network"}', 'Timeout while calling Telegram API', 'TG_TIMEOUT', 2, NOW() - INTERVAL '6 hours', NOW() - INTERVAL '3 hours', 'pending'),
    ('recalc_elo', '{"match_id":3}', 'ELO service unavailable', 'ELO_DOWN', 1, NOW() - INTERVAL '1 day', NOW() - INTERVAL '20 hours', 'failed');

COMMIT;

