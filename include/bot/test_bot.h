#ifndef BOT_TEST_BOT_H
#define BOT_TEST_BOT_H

#include "bot/bot_base.h"
#include "bot/test_bot_api.h"

namespace bot {

// Test-friendly bot implementation that doesn't require a valid Telegram token
// Uses CRTP with BotBase and TestBotApi
class TestBot : public BotBase<TestBot>, public TestBotApi {
 public:
  TestBot();
  ~TestBot() override = default;
  
  // Provide BotApi implementation for CRTP
  BotApi* getApiImpl() { return this; }
  
  // Initialize bot (set up handlers, dependencies, etc.)
  void initialize();
  
  // Set dependencies
  void setDependencies(
      std::shared_ptr<database::ConnectionPool> db_pool,
      std::unique_ptr<repositories::GroupRepository> group_repo,
      std::unique_ptr<repositories::PlayerRepository> player_repo,
      std::unique_ptr<repositories::MatchRepository> match_repo,
      std::unique_ptr<school21::ApiClient> school21_client);
};

}  // namespace bot

#endif  // BOT_TEST_BOT_H

