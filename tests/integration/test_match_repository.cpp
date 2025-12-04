#include <gtest/gtest.h>
#include "repositories/match_repository.h"
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "database/connection_pool.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <pqxx/pqxx>

class MatchRepositoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get database connection string from environment
    const char* db_url = std::getenv("DATABASE_URL");
    if (!db_url) {
      std::string host = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
      std::string port = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5432";
      std::string db = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "school_tg_bot";
      std::string user = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "postgres";
      std::string password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "postgres";
      
      connection_string_ = "postgresql://" + user + ":" + password + "@" + host + ":" + port + "/" + db;
    } else {
      connection_string_ = db_url;
    }
    
    database::ConnectionPool::Config config;
    config.connection_string = connection_string_;
    config.min_size = 1;
    config.max_size = 5;
    
    auto pool_unique = database::ConnectionPool::create(config);
    pool_ = std::shared_ptr<database::ConnectionPool>(std::move(pool_unique));
    
    if (!pool_->healthCheck()) {
      FAIL() << "Database connection failed. Cannot run repository tests.";
    }
    
    match_repo_ = std::make_unique<repositories::MatchRepository>(pool_);
    group_repo_ = std::make_unique<repositories::GroupRepository>(pool_);
    player_repo_ = std::make_unique<repositories::PlayerRepository>(pool_);
    
    // Clean up any existing test data
    cleanupTestData();
    
    // Initialize test counters
    test_match_counter_ = 1;
    test_group_counter_ = 1;
    test_player_counter_ = 1;
  }
  
  void TearDown() override {
    cleanupTestData();
    match_repo_.reset();
    group_repo_.reset();
    player_repo_.reset();
    pool_.reset();
  }
  
  void cleanupTestData() {
    if (!pool_) return;
    
    try {
      auto conn = pool_->acquire();
      pqxx::work txn(*conn);
      // Delete test data in correct order (foreign key constraints)
      txn.exec("DELETE FROM elo_history WHERE match_id IN (SELECT id FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000))");
      txn.exec("DELETE FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM group_players WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM groups WHERE telegram_group_id > 1000000");
      txn.exec("DELETE FROM players WHERE telegram_user_id > 1000000");
      txn.commit();
      pool_->release(conn);
    } catch (const std::exception&) {
      // Ignore cleanup errors
    }
  }
  
  std::string getNextIdempotencyKey() {
    return "test_key_" + std::to_string(test_match_counter_++);
  }
  
  int64_t getNextTestGroupId() {
    return 1000000 + test_group_counter_++;
  }
  
  int64_t getNextTestPlayerId() {
    return 1000000 + test_player_counter_++;
  }
  
  models::Match createTestMatch(int64_t group_id, int64_t player1_id, int64_t player2_id) {
    models::Match match;
    match.group_id = group_id;
    match.player1_id = player1_id;
    match.player2_id = player2_id;
    match.player1_score = 3;
    match.player2_score = 1;
    match.player1_elo_before = 1500;
    match.player2_elo_before = 1500;
    match.player1_elo_after = 1520;
    match.player2_elo_after = 1480;
    match.idempotency_key = getNextIdempotencyKey();
    match.created_by_telegram_user_id = 123456;
    match.is_undone = false;
    return match;
  }
  
  std::string connection_string_;
  std::shared_ptr<database::ConnectionPool> pool_;
  std::unique_ptr<repositories::MatchRepository> match_repo_;
  std::unique_ptr<repositories::GroupRepository> group_repo_;
  std::unique_ptr<repositories::PlayerRepository> player_repo_;
  int test_match_counter_;
  int test_group_counter_;
  int test_player_counter_;
};

TEST_F(MatchRepositoryTest, CreateMatch) {
  // Create group and players
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  // Create match
  auto match = createTestMatch(group.id, player1.id, player2.id);
  auto created = match_repo_->create(match);
  
  EXPECT_GT(created.id, 0);
  EXPECT_EQ(created.group_id, group.id);
  EXPECT_EQ(created.player1_id, player1.id);
  EXPECT_EQ(created.player2_id, player2.id);
  EXPECT_EQ(created.player1_score, 3);
  EXPECT_EQ(created.player2_score, 1);
  EXPECT_EQ(created.idempotency_key, match.idempotency_key);
  EXPECT_FALSE(created.is_undone);
}

TEST_F(MatchRepositoryTest, GetByIdExisting) {
  // Create group, players, and match
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  auto match = createTestMatch(group.id, player1.id, player2.id);
  auto created = match_repo_->create(match);
  
  // Get by ID
  auto found = match_repo_->getById(created.id);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->group_id, group.id);
  EXPECT_EQ(found->player1_id, player1.id);
  EXPECT_EQ(found->player2_id, player2.id);
}

TEST_F(MatchRepositoryTest, GetByIdNonExistent) {
  auto found = match_repo_->getById(999999999);
  
  EXPECT_FALSE(found.has_value());
}

TEST_F(MatchRepositoryTest, GetByIdempotencyKeyExisting) {
  // Create group, players, and match
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  auto match = createTestMatch(group.id, player1.id, player2.id);
  std::string idempotency_key = match.idempotency_key;
  auto created = match_repo_->create(match);
  
  // Get by idempotency key
  auto found = match_repo_->getByIdempotencyKey(idempotency_key);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->idempotency_key, idempotency_key);
}

TEST_F(MatchRepositoryTest, GetByIdempotencyKeyNonExistent) {
  auto found = match_repo_->getByIdempotencyKey("non_existent_key");
  
  EXPECT_FALSE(found.has_value());
}

TEST_F(MatchRepositoryTest, GetByGroupId) {
  // Create group and players
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  int64_t telegram_player3_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  auto player3 = player_repo_->createOrGet(telegram_player3_id);
  
  // Create multiple matches
  auto match1 = createTestMatch(group.id, player1.id, player2.id);
  auto match2 = createTestMatch(group.id, player2.id, player3.id);
  auto match3 = createTestMatch(group.id, player1.id, player3.id);
  
  auto created1 = match_repo_->create(match1);
  auto created2 = match_repo_->create(match2);
  auto created3 = match_repo_->create(match3);
  
  // Get matches for group
  auto matches = match_repo_->getByGroupId(group.id, 10, 0);
  
  EXPECT_GE(matches.size(), 3);
  
  // Verify matches are in descending order (newest first)
  for (size_t i = 0; i < matches.size() - 1; ++i) {
    EXPECT_GE(matches[i].created_at, matches[i + 1].created_at)
        << "Matches should be in descending order by created_at";
  }
}

TEST_F(MatchRepositoryTest, GetByGroupIdWithLimit) {
  // Create group and players
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  // Create 5 matches
  for (int i = 0; i < 5; ++i) {
    auto match = createTestMatch(group.id, player1.id, player2.id);
    match_repo_->create(match);
    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  // Get only 3 matches
  auto matches = match_repo_->getByGroupId(group.id, 3, 0);
  
  EXPECT_EQ(matches.size(), 3);
}

TEST_F(MatchRepositoryTest, GetByGroupIdWithOffset) {
  // Create group and players
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  // Create 5 matches
  std::vector<int64_t> match_ids;
  for (int i = 0; i < 5; ++i) {
    auto match = createTestMatch(group.id, player1.id, player2.id);
    auto created = match_repo_->create(match);
    match_ids.push_back(created.id);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  // Get matches with offset
  auto matches = match_repo_->getByGroupId(group.id, 2, 1);
  
  EXPECT_EQ(matches.size(), 2);
  // Should skip the first (newest) match
  EXPECT_NE(matches[0].id, match_ids[4]);
}

TEST_F(MatchRepositoryTest, UndoMatch) {
  // Create group, players, and match
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  auto match = createTestMatch(group.id, player1.id, player2.id);
  auto created = match_repo_->create(match);
  
  // Undo match
  int64_t undone_by_user = 789012;
  match_repo_->undoMatch(created.id, undone_by_user);
  
  // Verify match is undone
  auto found = match_repo_->getById(created.id);
  ASSERT_TRUE(found.has_value());
  EXPECT_TRUE(found->is_undone);
  EXPECT_TRUE(found->undone_at.has_value());
  EXPECT_TRUE(found->undone_by_telegram_user_id.has_value());
  EXPECT_EQ(found->undone_by_telegram_user_id.value(), undone_by_user);
}

TEST_F(MatchRepositoryTest, CreateEloHistory) {
  // Create group and player
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Create match first
  int64_t telegram_player2_id = getNextTestPlayerId();
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  auto match = createTestMatch(group.id, player.id, player2.id);
  auto created_match = match_repo_->create(match);
  
  // Create ELO history
  models::EloHistory history;
  history.match_id = created_match.id;
  history.group_id = group.id;
  history.player_id = player.id;
  history.elo_before = 1500;
  history.elo_after = 1520;
  history.elo_change = 20;
  history.is_undone = false;
  
  match_repo_->createEloHistory(history);
  
  // Verify history was created by querying database
  auto conn = pool_->acquire();
  pqxx::work txn(*conn);
  auto result = txn.exec_params(
    "SELECT COUNT(*) as cnt FROM elo_history WHERE match_id = $1 AND player_id = $2",
    created_match.id, player.id
  );
  txn.commit();
  pool_->release(conn);
  
  EXPECT_FALSE(result.empty());
  EXPECT_GT(result[0]["cnt"].as<int>(), 0);
}

TEST_F(MatchRepositoryTest, CreateEloHistoryWithoutMatchId) {
  // Create group and player
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Create ELO history without match_id (manual adjustment)
  models::EloHistory history;
  history.match_id = std::nullopt;
  history.group_id = group.id;
  history.player_id = player.id;
  history.elo_before = 1500;
  history.elo_after = 1600;
  history.elo_change = 100;
  history.is_undone = false;
  
  match_repo_->createEloHistory(history);
  
  // Verify history was created
  auto conn = pool_->acquire();
  pqxx::work txn(*conn);
  auto result = txn.exec_params(
    "SELECT COUNT(*) as cnt FROM elo_history WHERE match_id IS NULL AND player_id = $1",
    player.id
  );
  txn.commit();
  pool_->release(conn);
  
  EXPECT_FALSE(result.empty());
  EXPECT_GT(result[0]["cnt"].as<int>(), 0);
}

TEST_F(MatchRepositoryTest, CreateMatchInvalidInput) {
  models::Match match;
  match.group_id = 0;  // Invalid
  
  EXPECT_THROW(match_repo_->create(match), std::invalid_argument);
}

TEST_F(MatchRepositoryTest, CreateMatchEmptyIdempotencyKey) {
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player1_id = getNextTestPlayerId();
  int64_t telegram_player2_id = getNextTestPlayerId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  auto match = createTestMatch(group.id, player1.id, player2.id);
  match.idempotency_key = "";  // Empty
  
  EXPECT_THROW(match_repo_->create(match), std::invalid_argument);
}

TEST_F(MatchRepositoryTest, UndoMatchInvalidId) {
  EXPECT_THROW(match_repo_->undoMatch(0, 123), std::invalid_argument);
}

TEST_F(MatchRepositoryTest, CreateEloHistoryInvalidInput) {
  models::EloHistory history;
  history.group_id = 0;  // Invalid
  
  EXPECT_THROW(match_repo_->createEloHistory(history), std::invalid_argument);
}

