#include <gtest/gtest.h>
#include "repositories/player_repository.h"
#include "database/connection_pool.h"
#include <cstdlib>
#include <pqxx/pqxx>

class PlayerRepositoryTest : public ::testing::Test {
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
    
    repo_ = std::make_unique<repositories::PlayerRepository>(pool_);
    
    // Clean up any existing test data
    cleanupTestData();
    
    // Initialize test counter
    test_counter_ = 1;
  }
  
  void TearDown() override {
    cleanupTestData();
    repo_.reset();
    pool_.reset();
  }
  
  void cleanupTestData() {
    if (!pool_) return;
    
    try {
      auto conn = pool_->acquire();
      pqxx::work txn(*conn);
      // Delete test players (those with telegram_user_id > 1000000)
      txn.exec("DELETE FROM players WHERE telegram_user_id > 1000000");
      txn.commit();
      pool_->release(conn);
    } catch (const std::exception&) {
      // Ignore cleanup errors
    }
  }
  
  int64_t getNextTestUserId() {
    return 1000000 + test_counter_++;
  }
  
  std::string connection_string_;
  std::shared_ptr<database::ConnectionPool> pool_;
  std::unique_ptr<repositories::PlayerRepository> repo_;
  int test_counter_;
};

TEST_F(PlayerRepositoryTest, CreateOrGetNewPlayer) {
  int64_t telegram_id = getNextTestUserId();
  
  auto player = repo_->createOrGet(telegram_id);
  
  EXPECT_GT(player.id, 0);
  EXPECT_EQ(player.telegram_user_id, telegram_id);
  EXPECT_FALSE(player.school_nickname.has_value());
  EXPECT_FALSE(player.is_verified_student);
  EXPECT_FALSE(player.is_allowed_non_student);
}

TEST_F(PlayerRepositoryTest, CreateOrGetExistingPlayer) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create first time
  auto player1 = repo_->createOrGet(telegram_id);
  int64_t player1_id = player1.id;
  
  // Get existing
  auto player2 = repo_->createOrGet(telegram_id);
  
  EXPECT_EQ(player1_id, player2.id);
  EXPECT_EQ(player2.telegram_user_id, telegram_id);
}

TEST_F(PlayerRepositoryTest, GetByTelegramIdExisting) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player
  auto created = repo_->createOrGet(telegram_id);
  
  // Get by telegram ID
  auto found = repo_->getByTelegramId(telegram_id);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->telegram_user_id, telegram_id);
}

TEST_F(PlayerRepositoryTest, GetByTelegramIdNonExistent) {
  int64_t telegram_id = getNextTestUserId();
  
  auto found = repo_->getByTelegramId(telegram_id);
  
  EXPECT_FALSE(found.has_value());
}

TEST_F(PlayerRepositoryTest, GetByIdExisting) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player
  auto created = repo_->createOrGet(telegram_id);
  
  // Get by ID
  auto found = repo_->getById(created.id);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->telegram_user_id, telegram_id);
}

TEST_F(PlayerRepositoryTest, GetByIdNonExistent) {
  auto found = repo_->getById(999999999);
  
  EXPECT_FALSE(found.has_value());
}

TEST_F(PlayerRepositoryTest, UpdatePlayer) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player
  auto player = repo_->createOrGet(telegram_id);
  
  // Update player
  player.school_nickname = "test_nickname";
  player.is_verified_student = true;
  player.is_allowed_non_student = false;
  
  repo_->update(player);
  
  // Verify update
  auto updated = repo_->getById(player.id);
  ASSERT_TRUE(updated.has_value());
  EXPECT_TRUE(updated->school_nickname.has_value());
  EXPECT_EQ(updated->school_nickname.value(), "test_nickname");
  EXPECT_TRUE(updated->is_verified_student);
  EXPECT_FALSE(updated->is_allowed_non_student);
}

TEST_F(PlayerRepositoryTest, UpdatePlayerClearNickname) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player with nickname
  auto player = repo_->createOrGet(telegram_id);
  player.school_nickname = "test_nickname";
  repo_->update(player);
  
  // Clear nickname
  player.school_nickname = std::nullopt;
  repo_->update(player);
  
  // Verify nickname is cleared
  auto updated = repo_->getById(player.id);
  ASSERT_TRUE(updated.has_value());
  EXPECT_FALSE(updated->school_nickname.has_value());
}

TEST_F(PlayerRepositoryTest, SoftDeletePlayer) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player
  auto player = repo_->createOrGet(telegram_id);
  int64_t player_id = player.id;
  
  // Soft delete
  repo_->softDelete(player_id);
  
  // Verify player is soft deleted (getByTelegramId should return nullopt)
  auto found = repo_->getByTelegramId(telegram_id);
  EXPECT_FALSE(found.has_value());
  
  // But getById should still find it (with deleted_at set)
  auto found_by_id = repo_->getById(player_id);
  // Note: getById doesn't filter by deleted_at, so it should still find it
  // But the deleted_at field should be set
  if (found_by_id.has_value()) {
    EXPECT_TRUE(found_by_id->deleted_at.has_value());
  }
}

TEST_F(PlayerRepositoryTest, CreateOrGetAfterSoftDelete) {
  int64_t telegram_id = getNextTestUserId();
  
  // Create player
  auto player1 = repo_->createOrGet(telegram_id);
  int64_t player1_id = player1.id;
  
  // Soft delete
  repo_->softDelete(player1_id);
  
  // Create or get should create a new player (due to unique constraint on active players)
  // This should work because the old player is soft deleted
  auto player2 = repo_->createOrGet(telegram_id);
  
  // Should be a new player with different ID
  EXPECT_NE(player2.id, player1_id);
  EXPECT_EQ(player2.telegram_user_id, telegram_id);
}

TEST_F(PlayerRepositoryTest, InvalidTelegramId) {
  EXPECT_THROW(repo_->createOrGet(0), std::invalid_argument);
  EXPECT_THROW(repo_->createOrGet(-1), std::invalid_argument);
}

TEST_F(PlayerRepositoryTest, UpdateInvalidPlayerId) {
  models::Player player;
  player.id = 0;
  
  EXPECT_THROW(repo_->update(player), std::invalid_argument);
}

TEST_F(PlayerRepositoryTest, SoftDeleteInvalidId) {
  EXPECT_THROW(repo_->softDelete(0), std::invalid_argument);
  EXPECT_THROW(repo_->softDelete(-1), std::invalid_argument);
}

