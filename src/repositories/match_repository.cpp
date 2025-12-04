#include "repositories/match_repository.h"
#include "database/connection_pool.h"
#include "database/transaction.h"
#include "observability/logger.h"
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
  if (match.group_id <= 0 || match.player1_id <= 0 || match.player2_id <= 0) {
    throw std::invalid_argument("group_id, player1_id, and player2_id must be positive");
  }
  
  if (match.idempotency_key.empty()) {
    throw std::invalid_argument("idempotency_key cannot be empty");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
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
    
    return created_match;
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in create: " + std::string(e.what()));
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
  if (history.group_id <= 0 || history.player_id <= 0) {
    throw std::invalid_argument("group_id and player_id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
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
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in createEloHistory: " + std::string(e.what()));
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
