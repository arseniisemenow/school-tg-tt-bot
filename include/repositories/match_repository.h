#ifndef REPOSITORIES_MATCH_REPOSITORY_H
#define REPOSITORIES_MATCH_REPOSITORY_H

#include <memory>
#include <optional>
#include <vector>
#include "models/match.h"

namespace database {
class ConnectionPool;
}

namespace repositories {

class MatchRepository {
 public:
  explicit MatchRepository(std::shared_ptr<database::ConnectionPool> pool);
  
  // Create match
  models::Match create(const models::Match& match);
  
  // Get match by ID
  std::optional<models::Match> getById(int64_t id);
  
  // Get match by idempotency key
  std::optional<models::Match> getByIdempotencyKey(
      const std::string& idempotency_key);
  
  // Get matches for a group
  std::vector<models::Match> getByGroupId(int64_t group_id, 
                                         int limit = 50, 
                                         int offset = 0);
  
  // Undo match
  void undoMatch(int64_t match_id, int64_t undone_by_user_id);
  
  // Create ELO history entry
  void createEloHistory(const models::EloHistory& history);

 private:
  std::shared_ptr<database::ConnectionPool> pool_;
};

}  // namespace repositories

#endif  // REPOSITORIES_MATCH_REPOSITORY_H

