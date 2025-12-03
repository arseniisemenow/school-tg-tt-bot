#include "repositories/player_repository.h"
#include "database/connection_pool.h"

namespace repositories {

PlayerRepository::PlayerRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
}

models::Player PlayerRepository::createOrGet(int64_t telegram_user_id) {
  // TODO: Implement
  models::Player player;
  player.telegram_user_id = telegram_user_id;
  return player;
}

std::optional<models::Player> PlayerRepository::getByTelegramId(
    int64_t telegram_user_id) {
  // TODO: Implement
  return std::nullopt;
}

std::optional<models::Player> PlayerRepository::getById(int64_t id) {
  // TODO: Implement
  return std::nullopt;
}

void PlayerRepository::update(const models::Player& player) {
  // TODO: Implement
}

void PlayerRepository::softDelete(int64_t player_id) {
  // TODO: Implement
}

}  // namespace repositories

