#include "repositories/player_repository.h"
#include "database/connection_pool.h"
#include "database/transaction.h"
#include "observability/logger.h"
#include "utils/validation.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>

namespace repositories {

PlayerRepository::PlayerRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
  if (!pool_) {
    throw std::runtime_error("ConnectionPool is null");
  }
}

models::Player PlayerRepository::createOrGet(int64_t telegram_user_id) {
  auto logger = observability::Logger::getInstance();
  logger->debug("PlayerRepository::createOrGet called with telegram_user_id=" + std::to_string(telegram_user_id));
  
  // Input validation
  try {
    utils::validateId(telegram_user_id, "telegram_user_id");
  } catch (const std::invalid_argument& e) {
    logger->error("PlayerRepository::createOrGet - Invalid input: " + std::string(e.what()));
    throw;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    logger->error("PlayerRepository::createOrGet - Failed to acquire database connection");
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    // Try to insert, ignore if already exists
    txn.exec_params(
      "INSERT INTO players (telegram_user_id, created_at, updated_at) "
      "VALUES ($1, NOW(), NOW()) "
      "ON CONFLICT (telegram_user_id) WHERE deleted_at IS NULL DO NOTHING",
      telegram_user_id
    );
    
    // Get the player (either newly created or existing)
    auto result = txn.exec_params(
      "SELECT id, telegram_user_id, school_nickname, is_verified_student, "
      "is_allowed_non_student, created_at, updated_at, deleted_at "
      "FROM players "
      "WHERE telegram_user_id = $1 AND deleted_at IS NULL",
      telegram_user_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      logger->error("PlayerRepository::createOrGet - Failed to create or retrieve player with telegram_user_id=" + std::to_string(telegram_user_id));
      throw std::runtime_error("Failed to create or retrieve player");
    }
    
    auto player = rowToPlayer(result[0]);
    logger->info("PlayerRepository::createOrGet - Successfully retrieved player id=" + std::to_string(player.id) + " telegram_user_id=" + std::to_string(telegram_user_id));
    return player;
  } catch (const pqxx::unique_violation& e) {
    pool_->release(conn);
    logger->warn("PlayerRepository::createOrGet - Unique violation (should not happen): " + std::string(e.what()));
    // Retry to get existing player
    auto existing = getByTelegramId(telegram_user_id);
    if (existing.has_value()) {
      return existing.value();
    }
    throw std::runtime_error("Failed to create or retrieve player after unique violation");
  } catch (const pqxx::sql_error& e) {
    pool_->release(conn);
    logger->error("PlayerRepository::createOrGet - SQL error: " + std::string(e.what()) + " Query: " + e.query());
    throw std::runtime_error("Database error in createOrGet: " + std::string(e.what()));
  } catch (const std::exception& e) {
    pool_->release(conn);
    logger->error("PlayerRepository::createOrGet - Error: " + std::string(e.what()) + " telegram_user_id=" + std::to_string(telegram_user_id));
    throw;
  }
}

std::optional<models::Player> PlayerRepository::getByTelegramId(
    int64_t telegram_user_id) {
  if (telegram_user_id <= 0) {
    return std::nullopt;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, telegram_user_id, school_nickname, is_verified_student, "
      "is_allowed_non_student, created_at, updated_at, deleted_at "
      "FROM players "
      "WHERE telegram_user_id = $1 AND deleted_at IS NULL",
      telegram_user_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToPlayer(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getByTelegramId: " + std::string(e.what()));
    throw;
  }
}

std::optional<models::Player> PlayerRepository::getById(int64_t id) {
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
      "SELECT id, telegram_user_id, school_nickname, is_verified_student, "
      "is_allowed_non_student, created_at, updated_at, deleted_at "
      "FROM players "
      "WHERE id = $1",
      id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToPlayer(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getById: " + std::string(e.what()));
    throw;
  }
}

void PlayerRepository::update(const models::Player& player) {
  auto logger = observability::Logger::getInstance();
  logger->debug("PlayerRepository::update called with player_id=" + std::to_string(player.id));
  
  // Input validation
  try {
    utils::validateId(player.id, "player.id");
    if (player.school_nickname.has_value()) {
      utils::validateStringLength(player.school_nickname.value(), utils::MAX_STRING_LENGTH, "school_nickname");
    }
  } catch (const std::invalid_argument& e) {
    logger->error("PlayerRepository::update - Invalid input: " + std::string(e.what()) + " player_id=" + std::to_string(player.id));
    throw;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    logger->error("PlayerRepository::update - Failed to acquire database connection");
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    if (player.school_nickname.has_value()) {
      txn.exec_params(
        "UPDATE players SET "
        "school_nickname = $1, "
        "is_verified_student = $2, "
        "is_allowed_non_student = $3, "
        "updated_at = NOW() "
        "WHERE id = $4",
        player.school_nickname.value(),
        player.is_verified_student,
        player.is_allowed_non_student,
        player.id
      );
    } else {
      txn.exec_params(
        "UPDATE players SET "
        "school_nickname = NULL, "
        "is_verified_student = $1, "
        "is_allowed_non_student = $2, "
        "updated_at = NOW() "
        "WHERE id = $3",
        player.is_verified_student,
        player.is_allowed_non_student,
        player.id
      );
    }
    
    auto affected = txn.exec_params("SELECT COUNT(*) as cnt FROM players WHERE id = $1", player.id);
    if (affected.empty() || affected[0]["cnt"].as<int>() == 0) {
      logger->warn("PlayerRepository::update - Player not found: player_id=" + std::to_string(player.id));
      txn.commit();
      pool_->release(conn);
      throw std::runtime_error("Player not found");
    }
    
    txn.commit();
    pool_->release(conn);
    logger->info("PlayerRepository::update - Successfully updated player_id=" + std::to_string(player.id));
  } catch (const pqxx::sql_error& e) {
    pool_->release(conn);
    logger->error("PlayerRepository::update - SQL error: " + std::string(e.what()) + " Query: " + e.query() + " player_id=" + std::to_string(player.id));
    throw std::runtime_error("Database error in update: " + std::string(e.what()));
  } catch (const std::exception& e) {
    pool_->release(conn);
    logger->error("PlayerRepository::update - Error: " + std::string(e.what()) + " player_id=" + std::to_string(player.id));
    throw;
  }
}

void PlayerRepository::softDelete(int64_t player_id) {
  if (player_id <= 0) {
    throw std::invalid_argument("player_id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    txn.exec_params(
      "UPDATE players SET deleted_at = NOW(), updated_at = NOW() "
      "WHERE id = $1 AND deleted_at IS NULL",
      player_id
    );
    
    txn.commit();
    pool_->release(conn);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in softDelete: " + std::string(e.what()));
    throw;
  }
}

models::Player PlayerRepository::rowToPlayer(const pqxx::row& row) {
  models::Player player;
  player.id = row["id"].as<int64_t>();
  player.telegram_user_id = row["telegram_user_id"].as<int64_t>();
  
  if (!row["school_nickname"].is_null()) {
    player.school_nickname = row["school_nickname"].as<std::string>();
  }
  
  player.is_verified_student = row["is_verified_student"].as<bool>();
  player.is_allowed_non_student = row["is_allowed_non_student"].as<bool>();
  
  // Convert PostgreSQL timestamp to system_clock::time_point
  // Parse timestamp string from PostgreSQL
  auto parseTimestamp = [](const std::string& timestamp_str) -> std::chrono::system_clock::time_point {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    
    // Try parsing with timezone first (format: "2024-01-15 10:30:00+00")
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
      // Try ISO format
      ss.clear();
      ss.str(timestamp_str);
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    if (ss.fail()) {
      // Try with microseconds
      ss.clear();
      ss.str(timestamp_str);
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S.%f");
    }
    
    if (ss.fail()) {
      // If all parsing fails, return current time
      return std::chrono::system_clock::now();
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
  };
  
  auto created_at_str = row["created_at"].as<std::string>();
  player.created_at = parseTimestamp(created_at_str);
  
  auto updated_at_str = row["updated_at"].as<std::string>();
  player.updated_at = parseTimestamp(updated_at_str);
  
  if (!row["deleted_at"].is_null()) {
    auto deleted_at_str = row["deleted_at"].as<std::string>();
    player.deleted_at = parseTimestamp(deleted_at_str);
  }
  
  return player;
}

}  // namespace repositories

