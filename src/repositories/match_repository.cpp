#include "repositories/match_repository.h"
#include "database/connection_pool.h"
#include "database/transaction.h"
#include "observability/logger.h"
#include "utils/validation.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>

namespace repositories {

MatchRepository::MatchRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
  if (!pool_) {
    throw std::runtime_error("ConnectionPool is null");
  }
}

models::Match MatchRepository::create(const models::Match& match) {
  auto logger = observability::Logger::getInstance();
  logger->debug("MatchRepository::create called with group_id=" + std::to_string(match.group_id) + 
                " player1_id=" + std::to_string(match.player1_id) + " player2_id=" + std::to_string(match.player2_id));
  
  // Input validation
  try {
    utils::validateId(match.group_id, "match.group_id");
    utils::validateId(match.player1_id, "match.player1_id");
    utils::validateId(match.player2_id, "match.player2_id");
    if (match.player1_id == match.player2_id) {
      throw std::invalid_argument("player1_id and player2_id must be different (no self-matches)");
    }
    utils::validateIdempotencyKey(match.idempotency_key);
    utils::validateScore(match.player1_score, "player1_score");
    utils::validateScore(match.player2_score, "player2_score");
    if (match.player1_score == 0 && match.player2_score == 0) {
      throw std::invalid_argument("At least one score must be greater than 0");
    }
    utils::validateElo(match.player1_elo_before, "player1_elo_before");
    utils::validateElo(match.player2_elo_before, "player2_elo_before");
    utils::validateElo(match.player1_elo_after, "player1_elo_after");
    utils::validateElo(match.player2_elo_after, "player2_elo_after");
  } catch (const std::invalid_argument& e) {
    logger->error("MatchRepository::create - Invalid input: " + std::string(e.what()) + 
                  " group_id=" + std::to_string(match.group_id));
    throw;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    logger->error("MatchRepository::create - Failed to acquire database connection");
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    // Insert match and get the ID back
    auto result = txn.exec_params(
      "INSERT INTO matches (group_id, player1_id, player2_id, player1_score, player2_score, "
      "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
      "idempotency_key, created_by_telegram_user_id, created_at, is_undone) "
      "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW(), FALSE) "
      "RETURNING id, created_at",
      match.group_id,
      match.player1_id,
      match.player2_id,
      match.player1_score,
      match.player2_score,
      match.player1_elo_before,
      match.player2_elo_before,
      match.player1_elo_after,
      match.player2_elo_after,
      match.idempotency_key,
      match.created_by_telegram_user_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      logger->error("MatchRepository::create - Failed to create match (no result returned)");
      throw std::runtime_error("Failed to create match");
    }
    
    // Create match object with returned ID
    models::Match created_match = match;
    created_match.id = result[0]["id"].as<int64_t>();
    
    // Parse created_at timestamp
    auto created_at_str = result[0]["created_at"].as<std::string>();
    std::tm tm = {};
    std::istringstream ss(created_at_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
      ss.clear();
      ss.str(created_at_str);
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    if (!ss.fail()) {
      created_match.created_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else {
      created_match.created_at = std::chrono::system_clock::now();
    }
    
    logger->info("MatchRepository::create - Successfully created match id=" + std::to_string(created_match.id) + 
                 " group_id=" + std::to_string(match.group_id));
    return created_match;
  } catch (const pqxx::unique_violation& e) {
    pool_->release(conn);
    logger->warn("MatchRepository::create - Duplicate idempotency_key: " + match.idempotency_key);
    throw std::runtime_error("Match with this idempotency key already exists");
  } catch (const pqxx::foreign_key_violation& e) {
    pool_->release(conn);
    logger->error("MatchRepository::create - Foreign key violation: " + std::string(e.what()) + 
                  " group_id=" + std::to_string(match.group_id));
    throw std::runtime_error("Invalid group_id, player1_id, or player2_id (foreign key violation)");
  } catch (const pqxx::check_violation& e) {
    pool_->release(conn);
    logger->error("MatchRepository::create - Check constraint violation: " + std::string(e.what()));
    throw std::runtime_error("Match data violates database constraints: " + std::string(e.what()));
  } catch (const pqxx::sql_error& e) {
    pool_->release(conn);
    logger->error("MatchRepository::create - SQL error: " + std::string(e.what()) + " Query: " + e.query() + 
                  " group_id=" + std::to_string(match.group_id));
    throw std::runtime_error("Database error in create: " + std::string(e.what()));
  } catch (const std::exception& e) {
    pool_->release(conn);
    logger->error("MatchRepository::create - Error: " + std::string(e.what()) + 
                  " group_id=" + std::to_string(match.group_id));
    throw;
  }
}

std::optional<models::Match> MatchRepository::getById(int64_t id) {
  if (id <= 0) {
    return std::nullopt;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, group_id, player1_id, player2_id, player1_score, player2_score, "
      "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
      "idempotency_key, created_by_telegram_user_id, created_at, is_undone, "
      "undone_at, undone_by_telegram_user_id "
      "FROM matches "
      "WHERE id = $1",
      id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToMatch(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getById: " + std::string(e.what()));
    throw;
  }
}

std::optional<models::Match> MatchRepository::getByIdempotencyKey(
    const std::string& idempotency_key) {
  if (idempotency_key.empty()) {
    return std::nullopt;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, group_id, player1_id, player2_id, player1_score, player2_score, "
      "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
      "idempotency_key, created_by_telegram_user_id, created_at, is_undone, "
      "undone_at, undone_by_telegram_user_id "
      "FROM matches "
      "WHERE idempotency_key = $1",
      idempotency_key
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToMatch(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getByIdempotencyKey: " + std::string(e.what()));
    throw;
  }
}

std::vector<models::Match> MatchRepository::getByGroupId(int64_t group_id,
                                                         int limit,
                                                         int offset) {
  if (group_id <= 0) {
    return {};
  }
  
  if (limit <= 0) {
    limit = 50;
  }
  if (offset < 0) {
    offset = 0;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, group_id, player1_id, player2_id, player1_score, player2_score, "
      "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
      "idempotency_key, created_by_telegram_user_id, created_at, is_undone, "
      "undone_at, undone_by_telegram_user_id "
      "FROM matches "
      "WHERE group_id = $1 "
      "ORDER BY created_at DESC "
      "LIMIT $2 OFFSET $3",
      group_id, limit, offset
    );
    
    txn.commit();
    pool_->release(conn);
    
    std::vector<models::Match> matches;
    matches.reserve(result.size());
    
    for (const auto& row : result) {
      matches.push_back(rowToMatch(row));
    }
    
    return matches;
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getByGroupId: " + std::string(e.what()));
    throw;
  }
}

void MatchRepository::undoMatch(int64_t match_id, 
                               int64_t undone_by_user_id) {
  if (match_id <= 0) {
    throw std::invalid_argument("match_id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    txn.exec_params(
      "UPDATE matches SET "
      "is_undone = TRUE, "
      "undone_at = NOW(), "
      "undone_by_telegram_user_id = $1 "
      "WHERE id = $2 AND is_undone = FALSE",
      undone_by_user_id,
      match_id
    );
    
    txn.commit();
    pool_->release(conn);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in undoMatch: " + std::string(e.what()));
    throw;
  }
}

void MatchRepository::createEloHistory(const models::EloHistory& history) {
  auto logger = observability::Logger::getInstance();
  logger->debug("MatchRepository::createEloHistory called with group_id=" + std::to_string(history.group_id) + 
                " player_id=" + std::to_string(history.player_id));
  
  // Input validation
  try {
    utils::validateId(history.group_id, "history.group_id");
    utils::validateId(history.player_id, "history.player_id");
    if (history.match_id.has_value()) {
      utils::validateId(history.match_id.value(), "history.match_id");
    }
    utils::validateElo(history.elo_before, "elo_before");
    utils::validateElo(history.elo_after, "elo_after");
    // Validate elo_change matches the difference
    int expected_change = history.elo_after - history.elo_before;
    if (std::abs(history.elo_change - expected_change) > 1) {
      logger->warn("MatchRepository::createEloHistory - ELO change mismatch: expected=" + 
                   std::to_string(expected_change) + " got=" + std::to_string(history.elo_change));
    }
  } catch (const std::invalid_argument& e) {
    logger->error("MatchRepository::createEloHistory - Invalid input: " + std::string(e.what()) + 
                  " group_id=" + std::to_string(history.group_id) + " player_id=" + std::to_string(history.player_id));
    throw;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    logger->error("MatchRepository::createEloHistory - Failed to acquire database connection");
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    if (history.match_id.has_value()) {
      txn.exec_params(
        "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
        "elo_after, elo_change, created_at, is_undone) "
        "VALUES ($1, $2, $3, $4, $5, $6, NOW(), $7)",
        history.match_id.value(),
        history.group_id,
        history.player_id,
        history.elo_before,
        history.elo_after,
        history.elo_change,
        history.is_undone
      );
    } else {
      txn.exec_params(
        "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
        "elo_after, elo_change, created_at, is_undone) "
        "VALUES (NULL, $1, $2, $3, $4, $5, NOW(), $6)",
        history.group_id,
        history.player_id,
        history.elo_before,
        history.elo_after,
        history.elo_change,
        history.is_undone
      );
    }
    
    txn.commit();
    pool_->release(conn);
    logger->info("MatchRepository::createEloHistory - Successfully created ELO history group_id=" + 
                 std::to_string(history.group_id) + " player_id=" + std::to_string(history.player_id) + 
                 " elo_change=" + std::to_string(history.elo_change));
  } catch (const pqxx::check_violation& e) {
    pool_->release(conn);
    logger->error("MatchRepository::createEloHistory - Check constraint violation: " + std::string(e.what()) + 
                  " elo_after=" + std::to_string(history.elo_after));
    throw std::runtime_error("ELO value violates database constraints: " + std::string(e.what()));
  } catch (const pqxx::foreign_key_violation& e) {
    pool_->release(conn);
    logger->error("MatchRepository::createEloHistory - Foreign key violation: " + std::string(e.what()));
    throw std::runtime_error("Invalid group_id, player_id, or match_id (foreign key violation)");
  } catch (const pqxx::sql_error& e) {
    pool_->release(conn);
    logger->error("MatchRepository::createEloHistory - SQL error: " + std::string(e.what()) + " Query: " + e.query());
    throw std::runtime_error("Database error in createEloHistory: " + std::string(e.what()));
  } catch (const std::exception& e) {
    pool_->release(conn);
    logger->error("MatchRepository::createEloHistory - Error: " + std::string(e.what()));
    throw;
  }
}

models::Match MatchRepository::rowToMatch(const pqxx::row& row) {
  models::Match match;
  match.id = row["id"].as<int64_t>();
  match.group_id = row["group_id"].as<int64_t>();
  match.player1_id = row["player1_id"].as<int64_t>();
  match.player2_id = row["player2_id"].as<int64_t>();
  match.player1_score = row["player1_score"].as<int>();
  match.player2_score = row["player2_score"].as<int>();
  match.player1_elo_before = row["player1_elo_before"].as<int>();
  match.player2_elo_before = row["player2_elo_before"].as<int>();
  match.player1_elo_after = row["player1_elo_after"].as<int>();
  match.player2_elo_after = row["player2_elo_after"].as<int>();
  match.idempotency_key = row["idempotency_key"].as<std::string>();
  match.created_by_telegram_user_id = row["created_by_telegram_user_id"].as<int64_t>();
  match.is_undone = row["is_undone"].as<bool>();
  
  // Parse timestamps
  auto parseTimestamp = [](const std::string& timestamp_str) -> std::chrono::system_clock::time_point {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
      ss.clear();
      ss.str(timestamp_str);
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    if (ss.fail()) {
      return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
  };
  
  auto created_at_str = row["created_at"].as<std::string>();
  match.created_at = parseTimestamp(created_at_str);
  
  if (!row["undone_at"].is_null()) {
    auto undone_at_str = row["undone_at"].as<std::string>();
    match.undone_at = parseTimestamp(undone_at_str);
  }
  
  if (!row["undone_by_telegram_user_id"].is_null()) {
    match.undone_by_telegram_user_id = row["undone_by_telegram_user_id"].as<int64_t>();
  }
  
  return match;
}

}  // namespace repositories
