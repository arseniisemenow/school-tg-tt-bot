#ifndef REPOSITORIES_PLAYER_REPOSITORY_H
#define REPOSITORIES_PLAYER_REPOSITORY_H

#include <memory>
#include <optional>
#include <vector>
#include "models/player.h"

namespace database {
class ConnectionPool;
}

namespace pqxx {
class row;
}

namespace repositories {

class PlayerRepository {
 public:
  explicit PlayerRepository(std::shared_ptr<database::ConnectionPool> pool);
  
  // Create or get player by Telegram user ID
  models::Player createOrGet(int64_t telegram_user_id);
  
  // Get player by Telegram user ID
  std::optional<models::Player> getByTelegramId(int64_t telegram_user_id);
  
  // Get player by ID
  std::optional<models::Player> getById(int64_t id);
  
  // Update player
  void update(const models::Player& player);
  
  // Soft delete player
  void softDelete(int64_t player_id);

 private:
  std::shared_ptr<database::ConnectionPool> pool_;
  
  // Helper to convert database row to Player model
  models::Player rowToPlayer(const pqxx::row& row);
};

}  // namespace repositories

#endif  // REPOSITORIES_PLAYER_REPOSITORY_H

