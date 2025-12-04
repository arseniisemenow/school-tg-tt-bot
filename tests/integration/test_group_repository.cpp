#include <gtest/gtest.h>
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "database/connection_pool.h"
#include <cstdlib>
#include <pqxx/pqxx>

class GroupRepositoryTest : public ::testing::Test {
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
    
    group_repo_ = std::make_unique<repositories::GroupRepository>(pool_);
    player_repo_ = std::make_unique<repositories::PlayerRepository>(pool_);
    
    // Clean up any existing test data
    cleanupTestData();
    
    // Initialize test counters
    test_group_counter_ = 1;
    test_player_counter_ = 1;
  }
  
  void TearDown() override {
    cleanupTestData();
    group_repo_.reset();
    player_repo_.reset();
    pool_.reset();
  }
  
  void cleanupTestData() {
    if (!pool_) return;
    
    try {
      auto conn = pool_->acquire();
      pqxx::work txn(*conn);
      // Delete test groups (those with telegram_group_id > 1000000)
      txn.exec("DELETE FROM group_topics WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM group_players WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM groups WHERE telegram_group_id > 1000000");
      txn.commit();
      pool_->release(conn);
    } catch (const std::exception&) {
      // Ignore cleanup errors
    }
  }
  
  int64_t getNextTestGroupId() {
    return 1000000 + test_group_counter_++;
  }
  
  int64_t getNextTestPlayerId() {
    return 1000000 + test_player_counter_++;
  }
  
  std::string connection_string_;
  std::shared_ptr<database::ConnectionPool> pool_;
  std::unique_ptr<repositories::GroupRepository> group_repo_;
  std::unique_ptr<repositories::PlayerRepository> player_repo_;
  int test_group_counter_;
  int test_player_counter_;
};

TEST_F(GroupRepositoryTest, CreateOrGetNewGroup) {
  int64_t telegram_id = getNextTestGroupId();
  
  auto group = group_repo_->createOrGet(telegram_id, "Test Group");
  
  EXPECT_GT(group.id, 0);
  EXPECT_EQ(group.telegram_group_id, telegram_id);
  EXPECT_TRUE(group.name.has_value());
  EXPECT_EQ(group.name.value(), "Test Group");
  EXPECT_TRUE(group.is_active);
}

TEST_F(GroupRepositoryTest, CreateOrGetExistingGroup) {
  int64_t telegram_id = getNextTestGroupId();
  
  // Create first time
  auto group1 = group_repo_->createOrGet(telegram_id, "Original Name");
  int64_t group1_id = group1.id;
  
  // Get existing (should update name)
  auto group2 = group_repo_->createOrGet(telegram_id, "Updated Name");
  
  EXPECT_EQ(group1_id, group2.id);
  EXPECT_EQ(group2.telegram_group_id, telegram_id);
  EXPECT_TRUE(group2.name.has_value());
  EXPECT_EQ(group2.name.value(), "Updated Name");
}

TEST_F(GroupRepositoryTest, GetByTelegramIdExisting) {
  int64_t telegram_id = getNextTestGroupId();
  
  // Create group
  auto created = group_repo_->createOrGet(telegram_id, "Test");
  
  // Get by telegram ID
  auto found = group_repo_->getByTelegramId(telegram_id);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->telegram_group_id, telegram_id);
}

TEST_F(GroupRepositoryTest, GetByTelegramIdNonExistent) {
  int64_t telegram_id = getNextTestGroupId();
  
  auto found = group_repo_->getByTelegramId(telegram_id);
  
  EXPECT_FALSE(found.has_value());
}

TEST_F(GroupRepositoryTest, GetByIdExisting) {
  int64_t telegram_id = getNextTestGroupId();
  
  // Create group
  auto created = group_repo_->createOrGet(telegram_id);
  
  // Get by ID
  auto found = group_repo_->getById(created.id);
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, created.id);
  EXPECT_EQ(found->telegram_group_id, telegram_id);
}

TEST_F(GroupRepositoryTest, GetOrCreateGroupPlayer) {
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  // Create group and player
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Get or create group player
  auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  EXPECT_GT(gp.id, 0);
  EXPECT_EQ(gp.group_id, group.id);
  EXPECT_EQ(gp.player_id, player.id);
  EXPECT_EQ(gp.current_elo, 1500);
  EXPECT_EQ(gp.matches_played, 0);
  EXPECT_EQ(gp.matches_won, 0);
  EXPECT_EQ(gp.matches_lost, 0);
  EXPECT_EQ(gp.version, 0);
}

TEST_F(GroupRepositoryTest, GetOrCreateGroupPlayerExisting) {
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  // Create group and player
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Create first time
  auto gp1 = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  int64_t gp1_id = gp1.id;
  
  // Get existing
  auto gp2 = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  
  EXPECT_EQ(gp1_id, gp2.id);
  EXPECT_EQ(gp2.group_id, group.id);
  EXPECT_EQ(gp2.player_id, player.id);
}

TEST_F(GroupRepositoryTest, UpdateGroupPlayerOptimisticLocking) {
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  // Create group and player
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Get or create group player
  auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  int original_version = gp.version;
  
  // Update with correct version (should succeed)
  gp.current_elo = 1600;
  gp.matches_played = 5;
  gp.matches_won = 3;
  gp.matches_lost = 2;
  
  bool updated = group_repo_->updateGroupPlayer(gp);
  EXPECT_TRUE(updated) << "Update should succeed with correct version";
  
  // Verify update
  auto updated_gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  EXPECT_EQ(updated_gp.current_elo, 1600);
  EXPECT_EQ(updated_gp.matches_played, 5);
  EXPECT_EQ(updated_gp.matches_won, 3);
  EXPECT_EQ(updated_gp.matches_lost, 2);
  EXPECT_GT(updated_gp.version, original_version);
}

TEST_F(GroupRepositoryTest, UpdateGroupPlayerOptimisticLockFailure) {
  int64_t telegram_group_id = getNextTestGroupId();
  int64_t telegram_player_id = getNextTestPlayerId();
  
  // Create group and player
  auto group = group_repo_->createOrGet(telegram_group_id);
  auto player = player_repo_->createOrGet(telegram_player_id);
  
  // Get or create group player
  auto gp1 = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  int original_version = gp1.version;
  
  // Simulate concurrent update: another transaction updates the version
  {
    auto conn = pool_->acquire();
    pqxx::work txn(*conn);
    txn.exec_params(
      "UPDATE group_players SET version = version + 1, updated_at = NOW() WHERE id = $1",
      gp1.id
    );
    txn.commit();
    pool_->release(conn);
  }
  
  // Try to update with old version (should fail)
  gp1.current_elo = 1700;
  bool updated = group_repo_->updateGroupPlayer(gp1);
  EXPECT_FALSE(updated) << "Update should fail with stale version (optimistic lock)";
  
  // Verify ELO was not updated
  auto gp2 = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
  EXPECT_NE(gp2.current_elo, 1700);
  EXPECT_GT(gp2.version, original_version);
}

TEST_F(GroupRepositoryTest, GetRankings) {
  int64_t telegram_group_id = getNextTestGroupId();
  
  // Create group
  auto group = group_repo_->createOrGet(telegram_group_id);
  
  // Create players and group players with different ELOs
  std::vector<int64_t> player_ids;
  for (int i = 0; i < 5; ++i) {
    int64_t telegram_player_id = getNextTestPlayerId();
    auto player = player_repo_->createOrGet(telegram_player_id);
    player_ids.push_back(player.id);
    
    auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
    gp.current_elo = 1500 + (i * 100);  // Different ELOs: 1500, 1600, 1700, 1800, 1900
    group_repo_->updateGroupPlayer(gp);
  }
  
  // Get rankings
  auto rankings = group_repo_->getRankings(group.id, 10);
  
  EXPECT_EQ(rankings.size(), 5);
  
  // Verify rankings are in descending order
  for (size_t i = 0; i < rankings.size() - 1; ++i) {
    EXPECT_GE(rankings[i].current_elo, rankings[i + 1].current_elo)
        << "Rankings should be in descending ELO order";
  }
  
  // Highest ELO should be first
  EXPECT_EQ(rankings[0].current_elo, 1900);
  EXPECT_EQ(rankings[4].current_elo, 1500);
}

TEST_F(GroupRepositoryTest, GetRankingsWithLimit) {
  int64_t telegram_group_id = getNextTestGroupId();
  
  // Create group
  auto group = group_repo_->createOrGet(telegram_group_id);
  
  // Create 10 players
  for (int i = 0; i < 10; ++i) {
    int64_t telegram_player_id = getNextTestPlayerId();
    auto player = player_repo_->createOrGet(telegram_player_id);
    auto gp = group_repo_->getOrCreateGroupPlayer(group.id, player.id);
    gp.current_elo = 1500 + (i * 50);
    group_repo_->updateGroupPlayer(gp);
  }
  
  // Get top 3 rankings
  auto rankings = group_repo_->getRankings(group.id, 3);
  
  EXPECT_EQ(rankings.size(), 3);
  EXPECT_EQ(rankings[0].current_elo, 1950);  // Highest
  EXPECT_EQ(rankings[1].current_elo, 1900);
  EXPECT_EQ(rankings[2].current_elo, 1850);
}

TEST_F(GroupRepositoryTest, ConfigureTopic) {
  int64_t telegram_group_id = getNextTestGroupId();
  
  // Create group
  auto group = group_repo_->createOrGet(telegram_group_id);
  
  // Configure topic
  models::GroupTopic topic;
  topic.group_id = group.id;
  topic.telegram_topic_id = 123;
  topic.topic_type = "matches";
  topic.is_active = true;
  
  group_repo_->configureTopic(topic);
  
  // Get topic
  auto found = group_repo_->getTopic(group.id, 123, "matches");
  
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->group_id, group.id);
  EXPECT_TRUE(found->telegram_topic_id.has_value());
  EXPECT_EQ(found->telegram_topic_id.value(), 123);
  EXPECT_EQ(found->topic_type, "matches");
  EXPECT_TRUE(found->is_active);
}

TEST_F(GroupRepositoryTest, ConfigureTopicUpdate) {
  int64_t telegram_group_id = getNextTestGroupId();
  
  // Create group
  auto group = group_repo_->createOrGet(telegram_group_id);
  
  // Configure topic first time
  models::GroupTopic topic;
  topic.group_id = group.id;
  topic.telegram_topic_id = 456;
  topic.topic_type = "id";
  topic.is_active = true;
  
  group_repo_->configureTopic(topic);
  
  // Update topic (deactivate)
  topic.is_active = false;
  group_repo_->configureTopic(topic);
  
  // Verify update
  auto found = group_repo_->getTopic(group.id, 456, "id");
  ASSERT_TRUE(found.has_value());
  EXPECT_FALSE(found->is_active);
}

TEST_F(GroupRepositoryTest, GetTopicNonExistent) {
  int64_t telegram_group_id = getNextTestGroupId();
  
  // Create group
  auto group = group_repo_->createOrGet(telegram_group_id);
  
  // Try to get non-existent topic
  auto found = group_repo_->getTopic(group.id, 999, "ranking");
  
  EXPECT_FALSE(found.has_value());
}

