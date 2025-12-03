#include "repositories/group_repository.h"
#include "database/connection_pool.h"

namespace repositories {

GroupRepository::GroupRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
}

models::Group GroupRepository::createOrGet(int64_t telegram_group_id,
                                          const std::string& name) {
  // TODO: Implement
  models::Group group;
  group.telegram_group_id = telegram_group_id;
  group.name = name.empty() ? std::nullopt : std::make_optional(name);
  return group;
}

std::optional<models::Group> GroupRepository::getByTelegramId(
    int64_t telegram_group_id) {
  // TODO: Implement
  return std::nullopt;
}

std::optional<models::Group> GroupRepository::getById(int64_t id) {
  // TODO: Implement
  return std::nullopt;
}

models::GroupPlayer GroupRepository::getOrCreateGroupPlayer(
    int64_t group_id, int64_t player_id) {
  // TODO: Implement
  models::GroupPlayer gp;
  gp.group_id = group_id;
  gp.player_id = player_id;
  return gp;
}

bool GroupRepository::updateGroupPlayer(
    const models::GroupPlayer& group_player) {
  // TODO: Implement with optimistic locking
  return false;
}

std::vector<models::GroupPlayer> GroupRepository::getRankings(
    int64_t group_id, int limit) {
  // TODO: Implement
  return {};
}

void GroupRepository::configureTopic(const models::GroupTopic& topic) {
  // TODO: Implement
}

std::optional<models::GroupTopic> GroupRepository::getTopic(
    int64_t group_id, int telegram_topic_id, const std::string& topic_type) {
  // TODO: Implement
  return std::nullopt;
}

}  // namespace repositories

