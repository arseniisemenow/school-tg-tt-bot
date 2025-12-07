#include <gtest/gtest.h>
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "repositories/match_repository.h"
#include "database/connection_pool.h"
#include "utils/validation.h"
#include <cstdlib>
#include <pqxx/pqxx>

class RepositoryValidationTest : public ::testing::Test {
 protected:
  void SetUp() override {
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
      FAIL() << "Database connection failed.";
    }
    
    group_repo_ = std::make_unique<repositories::GroupRepository>(pool_);
    player_repo_ = std::make_unique<repositories::PlayerRepository>(pool_);
    match_repo_ = std::make_unique<repositories::MatchRepository>(pool_);
    
    cleanupTestData();
    
    test_counter_ = 1;
  }
  
  void TearDown() override {
    cleanupTestData();
  }
  
  void cleanupTestData() {
    if (!pool_) return;
    try {
      auto conn = pool_->acquire();
      pqxx::work txn(*conn);
      txn.exec("DELETE FROM elo_history WHERE match_id IN (SELECT id FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 2000000))");
      txn.exec("DELETE FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 2000000)");
      txn.exec("DELETE FROM group_players WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 2000000)");
      txn.exec("DELETE FROM groups WHERE telegram_group_id > 2000000");
      txn.exec("DELETE FROM players WHERE telegram_user_id > 2000000");
      txn.commit();
      pool_->release(conn);
    } catch (const std::exception&) {}
  }
  
  int64_t getNextId() { return 2000000 + test_counter_++; }
  
  std::string connection_string_;
  std::shared_ptr<database::ConnectionPool> pool_;
  std::unique_ptr<repositories::GroupRepository> group_repo_;
  std::unique_ptr<repositories::PlayerRepository> player_repo_;
  std::unique_ptr<repositories::MatchRepository> match_repo_;
  int test_counter_;
};

// PlayerRepository validation tests
TEST_F(RepositoryValidationTest, PlayerRepositoryCreateOrGetInvalidId) {
  EXPECT_THROW({
    player_repo_->createOrGet(0);
  }, std::invalid_argument);
  
  EXPECT_THROW({
    player_repo_->createOrGet(-1);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, PlayerRepositoryUpdateInvalidId) {
  models::Player player;
  player.id = 0;
  
  EXPECT_THROW({
    player_repo_->update(player);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, PlayerRepositoryUpdateStringTooLong) {
  int64_t telegram_id = getNextId();
  auto player = player_repo_->createOrGet(telegram_id);
  
  std::string long_nickname(utils::MAX_STRING_LENGTH + 1, 'a');
  player.school_nickname = long_nickname;
  
  EXPECT_THROW({
    player_repo_->update(player);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, PlayerRepositorySoftDeleteInvalidId) {
  EXPECT_THROW({
    player_repo_->softDelete(0);
  }, std::invalid_argument);
  
  EXPECT_THROW({
    player_repo_->softDelete(-1);
  }, std::invalid_argument);
}

// GroupRepository validation tests
TEST_F(RepositoryValidationTest, GroupRepositoryUpdateGroupPlayerInvalidElo) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test negative ELO
  gp.current_elo = -1;
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
  
  // Test ELO exceeding maximum
  gp.current_elo = utils::MAX_ELO + 1;
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, GroupRepositoryUpdateGroupPlayerInvalidStats) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test negative matches_played
  gp.matches_played = -1;
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
  
  // Reset
  gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test negative matches_won
  gp.matches_won = -1;
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
  
  // Reset
  gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test negative matches_lost
  gp.matches_lost = -1;
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
  
  // Reset
  gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test wins + losses > played
  gp.matches_played = 5;
  gp.matches_won = 3;
  gp.matches_lost = 3;  // 3 + 3 = 6 > 5
  EXPECT_THROW({
    group_repo_->updateGroupPlayer(gp);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, GroupRepositoryUpdateGroupPlayerValidEloBounds) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  // Test minimum ELO (0)
  gp.current_elo = 0;
  bool updated = group_repo_->updateGroupPlayer(gp);
  EXPECT_TRUE(updated);
  
  // Test maximum ELO (10000)
  gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  gp.current_elo = utils::MAX_ELO;
  updated = group_repo_->updateGroupPlayer(gp);
  EXPECT_TRUE(updated);
}

TEST_F(RepositoryValidationTest, GroupRepositoryConfigureTopicInvalidInput) {
  models::GroupTopic topic;
  topic.group_id = 0;
  topic.topic_type = "matches";
  
  EXPECT_THROW({
    group_repo_->configureTopic(topic);
  }, std::invalid_argument);
  
  topic.group_id = getNextId();
  topic.topic_type = "";  // Empty topic_type
  
  EXPECT_THROW({
    group_repo_->configureTopic(topic);
  }, std::invalid_argument);
  
  topic.topic_type = std::string(utils::MAX_TOPIC_TYPE_LENGTH + 1, 'a');
  
  EXPECT_THROW({
    group_repo_->configureTopic(topic);
  }, std::invalid_argument);
}

// MatchRepository validation tests
TEST_F(RepositoryValidationTest, MatchRepositoryCreateInvalidIds) {
  models::Match match;
  match.group_id = 0;
  match.player1_id = getNextId();
  match.player2_id = getNextId();
  match.idempotency_key = "test_key";
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.group_id = getNextId();
  match.player1_id = 0;
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.player1_id = getNextId();
  match.player2_id = 0;
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateSelfMatch) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  models::Match match;
  match.group_id = group.id;
  match.player1_id = player.id;
  match.player2_id = player.id;  // Same player
  match.idempotency_key = "test_key";
  match.player1_score = 3;
  match.player2_score = 1;
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateInvalidIdempotencyKey) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player1_id = getNextId();
  int64_t telegram_player2_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  models::Match match;
  match.group_id = group.id;
  match.player1_id = player1.id;
  match.player2_id = player2.id;
  match.idempotency_key = "";  // Empty
  match.player1_score = 3;
  match.player2_score = 1;
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.idempotency_key = std::string(utils::MAX_IDEMPOTENCY_KEY_LENGTH + 1, 'a');
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateInvalidScores) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player1_id = getNextId();
  int64_t telegram_player2_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  models::Match match;
  match.group_id = group.id;
  match.player1_id = player1.id;
  match.player2_id = player2.id;
  match.idempotency_key = "test_key";
  match.player1_score = -1;  // Negative
  match.player2_score = 1;
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.player1_score = 3;
  match.player2_score = -1;  // Negative
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.player1_score = 0;
  match.player2_score = 0;  // Both zero
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateInvalidElo) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player1_id = getNextId();
  int64_t telegram_player2_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player1 = player_repo_->createOrGet(telegram_player1_id);
  auto player2 = player_repo_->createOrGet(telegram_player2_id);
  
  models::Match match;
  match.group_id = group.id;
  match.player1_id = player1.id;
  match.player2_id = player2.id;
  match.idempotency_key = "test_key";
  match.player1_score = 3;
  match.player2_score = 1;
  match.player1_elo_before = -1;  // Negative ELO
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
  
  match.player1_elo_before = 1500;
  match.player1_elo_after = utils::MAX_ELO + 1;  // Exceeds maximum
  
  EXPECT_THROW({
    match_repo_->create(match);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateEloHistoryInvalidElo) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  models::EloHistory history;
  history.group_id = group.id;
  history.player_id = player.id;
  history.elo_before = -1;  // Negative
  history.elo_after = 1500;
  history.elo_change = 1501;
  
  EXPECT_THROW({
    match_repo_->createEloHistory(history);
  }, std::invalid_argument);
  
  history.elo_before = 1500;
  history.elo_after = utils::MAX_ELO + 1;  // Exceeds maximum
  
  EXPECT_THROW({
    match_repo_->createEloHistory(history);
  }, std::invalid_argument);
}

TEST_F(RepositoryValidationTest, MatchRepositoryCreateEloHistoryValidBounds) {
  int64_t telegram_group_id = getNextId();
  int64_t telegram_player_id = getNextId();
  
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  models::EloHistory history;
  history.group_id = group.id;
  history.player_id = player.id;
  history.elo_before = 0;  // Minimum
  history.elo_after = 1500;
  history.elo_change = 1500;
  
  // Should not throw
  EXPECT_NO_THROW({
    match_repo_->createEloHistory(history);
  });
  
  history.elo_before = 1500;
  history.elo_after = utils::MAX_ELO;  // Maximum
  history.elo_change = utils::MAX_ELO - 1500;
  
  // Should not throw
  EXPECT_NO_THROW({
    match_repo_->createEloHistory(history);
  });
}

TEST_F(RepositoryValidationTest, MatchRepositoryUndoMatchInvalidId) {
  EXPECT_THROW({
    match_repo_->undoMatch(0, 123);
  }, std::invalid_argument);
  
  EXPECT_THROW({
    match_repo_->undoMatch(-1, 123);
  }, std::invalid_argument);
}





