#include "bot/test_bot.h"
#include "config/config.h"
#include "observability/logger.h"
#include "utils/elo_calculator.h"

namespace bot {

TestBot::TestBot()
    : BotBase<TestBot>(),
      TestBotApi() {
  // Initialize logger - BotBase will use it
  // Note: BotBase needs logger_ to be set, but it's private
  // We'll need to make it accessible or initialize it differently
}

void TestBot::initialize() {
  // This will be handled by BotBase::initialize()
  // But we need to call it
  BotBase<TestBot>::initialize();
}

void TestBot::setDependencies(
    std::shared_ptr<database::ConnectionPool> db_pool,
    std::unique_ptr<repositories::GroupRepository> group_repo,
    std::unique_ptr<repositories::PlayerRepository> player_repo,
    std::unique_ptr<repositories::MatchRepository> match_repo,
    std::unique_ptr<school21::ApiClient> school21_client) {
  BotBase<TestBot>::setDependencies(
      db_pool,
      std::move(group_repo),
      std::move(player_repo),
      std::move(match_repo),
      std::move(school21_client)
  );
}

}  // namespace bot

