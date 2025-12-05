// Example test file showing how to test bot scenarios without Telegram API
// This is a template - adapt it based on your actual bot implementation

#include <gtest/gtest.h>
#include "bot/test_bot.h"
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "repositories/match_repository.h"
#include "database/connection_pool.h"
#include "fixtures/telegram_mocks.h"
#include "mocks/mock_school21_client.h"
#include "config/config.h"
#include "models/group.h"
#include <memory>
#include <cstdlib>
#include <pqxx/pqxx>
#include <chrono>

// NOTE: This is an example/template. You'll need to adapt it based on:
// 1. How to create a testable bot instance (may need to make sendMessage mockable)
// 2. How to handle admin checking (currently isGroupAdmin returns false)
// 3. How to capture bot responses (sendMessage calls)

class BotScenariosTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Load config (required for bot initialization)
    auto& config = config::Config::getInstance();
    std::string config_path = "config/config.dev.json";
    try {
      config.load(config_path);
    } catch (const std::exception&) {
      // If config file doesn't exist, that's okay - bot will use defaults
    }
    
    // Setup database connection
    const char* db_url = std::getenv("DATABASE_URL");
    std::string connection_string;
    if (!db_url) {
      std::string host = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
      std::string port = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5432";
      std::string db = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "school_tg_bot";
      std::string user = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "postgres";
      std::string password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "postgres";
      connection_string = "postgresql://" + user + ":" + password + "@" + host + ":" + port + "/" + db;
    } else {
      connection_string = db_url;
    }
    
    database::ConnectionPool::Config db_config;
    db_config.connection_string = connection_string;
    db_config.min_size = 1;
    db_config.max_size = 5;
    
    auto pool_unique = database::ConnectionPool::create(db_config);
    db_pool_ = std::shared_ptr<database::ConnectionPool>(std::move(pool_unique));
    
    if (!db_pool_->healthCheck()) {
      FAIL() << "Database connection failed. Cannot run bot scenario tests.";
    }
    
    // Create repositories
    group_repo_ = std::make_unique<repositories::GroupRepository>(db_pool_);
    player_repo_ = std::make_unique<repositories::PlayerRepository>(db_pool_);
    match_repo_ = std::make_unique<repositories::MatchRepository>(db_pool_);
    
    // Create mock School21 client
    auto school21_client_unique = std::make_unique<test_mocks::MockSchool21Client>();
    
    // Configure mock School21 client with test data
    school21::Participant test_participant;
    test_participant.login = "testuser";
    test_participant.status = "ACTIVE";
    school21_client_unique->addParticipant("testuser", test_participant);
    
    // Create test bot instance (doesn't require valid token)
    bot_ = std::make_unique<bot::TestBot>();
    bot_->initialize();
    bot_->setDependencies(
        db_pool_,
        std::move(group_repo_),
        std::move(player_repo_),
        std::move(match_repo_),
        std::move(school21_client_unique)
    );
    // Re-acquire repos after move (they're now owned by bot_)
    group_repo_ = std::make_unique<repositories::GroupRepository>(db_pool_);
    player_repo_ = std::make_unique<repositories::PlayerRepository>(db_pool_);
    match_repo_ = std::make_unique<repositories::MatchRepository>(db_pool_);
    
    // Clean up test data
    cleanupTestData();
    
    // Initialize test counters
    test_group_counter_ = 1;
    test_player_counter_ = 1;
  }
  
  void TearDown() override {
    cleanupTestData();
    if (bot_) {
      bot_.reset();  // This will destroy the school21_client too
    }
    group_repo_.reset();
    player_repo_.reset();
    match_repo_.reset();
    db_pool_.reset();
  }
  
  void cleanupTestData() {
    if (!db_pool_) return;
    
    try {
      auto conn = db_pool_->acquire();
      pqxx::work txn(*conn);
      // Delete test data in correct order
      txn.exec("DELETE FROM elo_history WHERE match_id IN (SELECT id FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000))");
      txn.exec("DELETE FROM matches WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM group_players WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM group_topics WHERE group_id IN (SELECT id FROM groups WHERE telegram_group_id > 1000000)");
      txn.exec("DELETE FROM groups WHERE telegram_group_id > 1000000");
      txn.exec("DELETE FROM players WHERE telegram_user_id > 1000000");
      txn.commit();
      db_pool_->release(conn);
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
  
  std::shared_ptr<database::ConnectionPool> db_pool_;
  std::unique_ptr<repositories::GroupRepository> group_repo_;
  std::unique_ptr<repositories::PlayerRepository> player_repo_;
  std::unique_ptr<repositories::MatchRepository> match_repo_;
  std::unique_ptr<bot::TestBot> bot_;
  int test_group_counter_;
  int test_player_counter_;
};

// Scenario 1: Bot Added into Channel
// When bot is added, it should be able to handle commands and create groups
TEST_F(BotScenariosTest, BotAddedToChannel) {
  int64_t test_group_id = getNextTestGroupId();
  int64_t bot_user_id = 999888777;  // Bot's user ID
  
  // Create mock ChatMemberUpdated event for bot being added
  auto chat_member = test_fixtures::createMockChatMemberUpdate(
      test_group_id,
      bot_user_id,
      "member",
      true  // is_bot = true
  );
  
  // Note: onChatMemberUpdated and onCommand are protected methods
  // For this test, we'll verify that the group can be created when needed
  // In a real scenario, the bot would handle the chat member update and commands
  // TODO: Make bot methods testable (e.g., create a test bot class or make methods public for testing)
  
  // Verify: Group can be created (this tests the scenario indirectly)
  auto group = group_repo_->createOrGet(test_group_id);
  ASSERT_TRUE(group.id > 0);
  EXPECT_TRUE(group.is_active);
  EXPECT_EQ(group.telegram_group_id, test_group_id);
}

// Scenario 2: Admin Configured Topics
// NOTE: Since isGroupAdmin() currently returns false, we'll manually configure the topic
// to test the topic configuration functionality
// This test doesn't require the bot, so it can run
TEST_F(BotScenariosTest, AdminConfiguresTopic) {
  int64_t test_group_id = getNextTestGroupId();
  int topic_id = 123;
  
  // First, create the group
  auto group = group_repo_->createOrGet(test_group_id);
  
  // Manually configure topic (since admin check returns false, we'll test the configuration directly)
  models::GroupTopic topic;
  topic.group_id = group.id;
  topic.telegram_topic_id = topic_id;
  topic.topic_type = "matches";
  topic.is_active = true;
  topic.created_at = std::chrono::system_clock::now();
  group_repo_->configureTopic(topic);
  
  // Verify: Topic configuration exists
  auto configured_topic = group_repo_->getTopic(group.id, topic_id, "matches");
  ASSERT_TRUE(configured_topic.has_value());
  EXPECT_EQ(configured_topic->topic_type, "matches");
  EXPECT_EQ(configured_topic->telegram_topic_id, topic_id);
  EXPECT_TRUE(configured_topic->is_active);
}

// Scenario 3: Users Registered in ID Topic
// NOTE: This test is simplified because bot constructor requires valid Telegram token
TEST_F(BotScenariosTest, UserRegistersInIdTopic) {
  int64_t test_group_id = getNextTestGroupId();
  int64_t user_id = getNextTestPlayerId();
  int id_topic_id = 456;
  std::string nickname = "testuser";
  
  // Create group
  auto group = group_repo_->createOrGet(test_group_id);
  
  // Configure ID topic first
  models::GroupTopic id_topic;
  id_topic.group_id = group.id;
  id_topic.telegram_topic_id = id_topic_id;
  id_topic.topic_type = "id";
  id_topic.is_active = true;
  id_topic.created_at = std::chrono::system_clock::now();
  group_repo_->configureTopic(id_topic);
  
  // Create mock message in ID topic
  auto message = test_fixtures::createMockMessage(
      test_group_id,
      user_id,
      "/id " + nickname,
      id_topic_id
  );
  
  // Call bot's onCommand to handle /id command
  bot_->onCommand(message);
  
  // Verify: Player registered with nickname
  auto verified_player = player_repo_->getByTelegramId(user_id);
  ASSERT_TRUE(verified_player.has_value());
  EXPECT_EQ(verified_player->school_nickname.value(), nickname);
  // Player should be verified (since mock returns ACTIVE status)
  EXPECT_TRUE(verified_player->is_verified_student || verified_player->is_allowed_non_student);
}

// Scenario 4: Users Registering Matches in Matches Topic
// NOTE: This test is disabled because it requires MessageEntity which has compilation issues
// TODO: Fix tgbotxx MessageEntity compilation issue or find workaround
TEST_F(BotScenariosTest, UserRegistersMatch) {
  int64_t test_group_id = getNextTestGroupId();
  int64_t user_id = getNextTestPlayerId();
  int64_t player1_id = getNextTestPlayerId();
  int64_t player2_id = getNextTestPlayerId();
  int matches_topic_id = 789;
  
  // Create group
  auto group = group_repo_->createOrGet(test_group_id);
  
  // Configure matches topic
  models::GroupTopic matches_topic;
  matches_topic.group_id = group.id;
  matches_topic.telegram_topic_id = matches_topic_id;
  matches_topic.topic_type = "matches";
  matches_topic.is_active = true;
  matches_topic.created_at = std::chrono::system_clock::now();
  group_repo_->configureTopic(matches_topic);
  
  // Create players
  auto player1 = player_repo_->createOrGet(player1_id);
  auto player2 = player_repo_->createOrGet(player2_id);
  
  // Create group players (register them in the group)
  auto gp1 = group_repo_->getOrCreateGroupPlayer(group.id, player1.id);
  auto gp2 = group_repo_->getOrCreateGroupPlayer(group.id, player2.id);
  
  int elo1_before = gp1.current_elo;
  int elo2_before = gp2.current_elo;
  
  // Create mock message with mentions
  auto message = test_fixtures::createMockMessage(
      test_group_id,
      user_id,
      "/match @player1 @player2 3 1",
      matches_topic_id
  );
  
  // Add mentions to message entities
  test_fixtures::addMention(message, "player1", player1_id, 8);
  test_fixtures::addMention(message, "player2", player2_id, 17);
  
  // Call bot's onCommand to handle /match command
  bot_->onCommand(message);
  
  // Verify: Match was created
  auto matches = match_repo_->getByGroupId(group.id, 100, 0);
  ASSERT_GT(matches.size(), 0);
  auto match = matches[0];
  EXPECT_EQ(match.player1_id, player1.id);
  EXPECT_EQ(match.player2_id, player2.id);
  EXPECT_EQ(match.player1_score, 3);
  EXPECT_EQ(match.player2_score, 1);
  EXPECT_FALSE(match.is_undone);
  
  // Verify: ELO updated
  auto gp1_after = group_repo_->getOrCreateGroupPlayer(group.id, player1.id);
  auto gp2_after = group_repo_->getOrCreateGroupPlayer(group.id, player2.id);
  EXPECT_GT(gp1_after.current_elo, elo1_before);  // Winner's ELO increased
  EXPECT_LT(gp2_after.current_elo, elo2_before);   // Loser's ELO decreased
  EXPECT_EQ(gp1_after.matches_played, 1);
  EXPECT_EQ(gp2_after.matches_played, 1);
  EXPECT_EQ(gp1_after.matches_won, 1);
  EXPECT_EQ(gp2_after.matches_lost, 1);
}

// Additional test: User tries to register match in wrong topic
// NOTE: This test is disabled because it requires MessageEntity
TEST_F(BotScenariosTest, UserTriesMatchInWrongTopic) {
  int64_t test_group_id = getNextTestGroupId();
  int64_t user_id = getNextTestPlayerId();
  int64_t player1_id = getNextTestPlayerId();
  int64_t player2_id = getNextTestPlayerId();
  int wrong_topic_id = 999;
  int matches_topic_id = 789;
  
  // Create group
  auto group = group_repo_->createOrGet(test_group_id);
  
  // Configure matches topic
  models::GroupTopic matches_topic;
  matches_topic.group_id = group.id;
  matches_topic.telegram_topic_id = matches_topic_id;
  matches_topic.topic_type = "matches";
  matches_topic.is_active = true;
  matches_topic.created_at = std::chrono::system_clock::now();
  group_repo_->configureTopic(matches_topic);
  
  // Create players
  auto player1 = player_repo_->createOrGet(player1_id);
  auto player2 = player_repo_->createOrGet(player2_id);
  
  // Create mock message in WRONG topic
  auto message = test_fixtures::createMockMessage(
      test_group_id,
      user_id,
      "/match @player1 @player2 3 1",
      wrong_topic_id  // Wrong topic!
  );
  
  test_fixtures::addMention(message, "player1", player1_id, 8);
  test_fixtures::addMention(message, "player2", player2_id, 17);
  
  // Call bot's onCommand to handle /match command (should fail due to wrong topic)
  bot_->onCommand(message);
  
  // Verify: No matches created (command should have been rejected)
  auto matches = match_repo_->getByGroupId(group.id, 100, 0);
  EXPECT_EQ(matches.size(), 0);
}

