#include "repositories/match_repository.h"
#include "database/connection_pool.h"

namespace repositories {

MatchRepository::MatchRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
}

models::Match MatchRepository::create(const models::Match& match) {
  // TODO: Implement
  return match;
}

std::optional<models::Match> MatchRepository::getById(int64_t id) {
  // TODO: Implement
  return std::nullopt;
}

std::optional<models::Match> MatchRepository::getByIdempotencyKey(
    const std::string& idempotency_key) {
  // TODO: Implement
  return std::nullopt;
}

std::vector<models::Match> MatchRepository::getByGroupId(int64_t group_id,
                                                         int limit,
                                                         int offset) {
  // TODO: Implement
  return {};
}

void MatchRepository::undoMatch(int64_t match_id, 
                               int64_t undone_by_user_id) {
  // TODO: Implement
}

void MatchRepository::createEloHistory(const models::EloHistory& history) {
  // TODO: Implement
}

}  // namespace repositories

