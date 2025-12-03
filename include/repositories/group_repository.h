#ifndef REPOSITORIES_GROUP_REPOSITORY_H
#define REPOSITORIES_GROUP_REPOSITORY_H

#include <memory>
#include <optional>
#include <vector>
#include "models/group.h"
#include "models/player.h"

namespace database {
class ConnectionPool;
}

namespace repositories {

class GroupRepository {
 public:
  explicit GroupRepository(std::shared_ptr<database::ConnectionPool> pool);
  
  // Create or get group by Telegram group ID
  models::Group createOrGet(int64_t telegram_group_id, 
                           const std::string& name = "");
  
  // Get group by Telegram group ID
  std::optional<models::Group> getByTelegramId(int64_t telegram_group_id);
  
  // Get group by ID
  std::optional<models::Group> getById(int64_t id);
  
  // Get or create group player
  models::GroupPlayer getOrCreateGroupPlayer(int64_t group_id, 
                                             int64_t player_id);
  
  // Update group player (with optimistic locking)
  bool updateGroupPlayer(const models::GroupPlayer& group_player);
  
  // Get rankings for a group
  std::vector<models::GroupPlayer> getRankings(int64_t group_id, 
                                               int limit = 10);
  
  // Configure topic
  void configureTopic(const models::GroupTopic& topic);
  
  // Get topic configuration
  std::optional<models::GroupTopic> getTopic(int64_t group_id,
                                             int telegram_topic_id,
                                             const std::string& topic_type);

 private:
  std::shared_ptr<database::ConnectionPool> pool_;
};

}  // namespace repositories

#endif  // REPOSITORIES_GROUP_REPOSITORY_H

