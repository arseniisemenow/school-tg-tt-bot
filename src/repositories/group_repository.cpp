#include "repositories/group_repository.h"
#include "database/connection_pool.h"
#include "database/transaction.h"
#include "observability/logger.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>

namespace repositories {

GroupRepository::GroupRepository(std::shared_ptr<database::ConnectionPool> pool)
    : pool_(pool) {
  if (!pool_) {
    throw std::runtime_error("ConnectionPool is null");
  }
}

models::Group GroupRepository::createOrGet(int64_t telegram_group_id,
                                          const std::string& name) {
  if (telegram_group_id == 0) {
    throw std::invalid_argument("telegram_group_id cannot be zero");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    // Try to insert, update name if exists
    if (name.empty()) {
      txn.exec_params(
        "INSERT INTO groups (telegram_group_id, created_at, updated_at) "
        "VALUES ($1, NOW(), NOW()) "
        "ON CONFLICT (telegram_group_id) DO UPDATE SET updated_at = NOW()",
        telegram_group_id
      );
    } else {
      txn.exec_params(
        "INSERT INTO groups (telegram_group_id, name, created_at, updated_at) "
        "VALUES ($1, $2, NOW(), NOW()) "
        "ON CONFLICT (telegram_group_id) DO UPDATE SET name = $2, updated_at = NOW()",
        telegram_group_id, name
      );
    }
    
    // Get the group (either newly created or existing)
    auto result = txn.exec_params(
      "SELECT id, telegram_group_id, name, created_at, updated_at, is_active "
      "FROM groups "
      "WHERE telegram_group_id = $1",
      telegram_group_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      throw std::runtime_error("Failed to create or retrieve group");
    }
    
    return rowToGroup(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in createOrGet: " + std::string(e.what()));
    throw;
  }
}

std::optional<models::Group> GroupRepository::getByTelegramId(
    int64_t telegram_group_id) {
  if (telegram_group_id == 0) {
    return std::nullopt;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, telegram_group_id, name, created_at, updated_at, is_active "
      "FROM groups "
      "WHERE telegram_group_id = $1",
      telegram_group_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToGroup(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getByTelegramId: " + std::string(e.what()));
    throw;
  }
}

std::optional<models::Group> GroupRepository::getById(int64_t id) {
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
      "SELECT id, telegram_group_id, name, created_at, updated_at, is_active "
      "FROM groups "
      "WHERE id = $1",
      id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToGroup(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getById: " + std::string(e.what()));
    throw;
  }
}

models::GroupPlayer GroupRepository::getOrCreateGroupPlayer(
    int64_t group_id, int64_t player_id) {
  if (group_id <= 0 || player_id <= 0) {
    throw std::invalid_argument("group_id and player_id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    // Try to insert, ignore if already exists
    txn.exec_params(
      "INSERT INTO group_players (group_id, player_id, current_elo, created_at, updated_at) "
      "VALUES ($1, $2, 1500, NOW(), NOW()) "
      "ON CONFLICT (group_id, player_id) DO NOTHING",
      group_id, player_id
    );
    
    // Get the group player (either newly created or existing)
    auto result = txn.exec_params(
      "SELECT id, group_id, player_id, current_elo, matches_played, "
      "matches_won, matches_lost, version, created_at, updated_at "
      "FROM group_players "
      "WHERE group_id = $1 AND player_id = $2",
      group_id, player_id
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      throw std::runtime_error("Failed to create or retrieve group player");
    }
    
    return rowToGroupPlayer(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getOrCreateGroupPlayer: " + std::string(e.what()));
    throw;
  }
}

bool GroupRepository::updateGroupPlayer(
    const models::GroupPlayer& group_player) {
  if (group_player.id <= 0) {
    throw std::invalid_argument("group_player.id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    // Optimistic locking: update with version check
    auto result = txn.exec_params(
      "UPDATE group_players SET "
      "current_elo = $1, "
      "matches_played = $2, "
      "matches_won = $3, "
      "matches_lost = $4, "
      "version = version + 1, "
      "updated_at = NOW() "
      "WHERE id = $5 AND version = $6",
      group_player.current_elo,
      group_player.matches_played,
      group_player.matches_won,
      group_player.matches_lost,
      group_player.id,
      group_player.version
    );
    
    txn.commit();
    pool_->release(conn);
    
    // Return true if any rows were affected (optimistic lock succeeded)
    return result.affected_rows() > 0;
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in updateGroupPlayer: " + std::string(e.what()));
    throw;
  }
}

std::vector<models::GroupPlayer> GroupRepository::getRankings(
    int64_t group_id, int limit) {
  if (group_id <= 0) {
    return {};
  }
  
  if (limit <= 0) {
    limit = 10;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, group_id, player_id, current_elo, matches_played, "
      "matches_won, matches_lost, version, created_at, updated_at "
      "FROM group_players "
      "WHERE group_id = $1 "
      "ORDER BY current_elo DESC "
      "LIMIT $2",
      group_id, limit
    );
    
    txn.commit();
    pool_->release(conn);
    
    std::vector<models::GroupPlayer> rankings;
    rankings.reserve(result.size());
    
    for (const auto& row : result) {
      rankings.push_back(rowToGroupPlayer(row));
    }
    
    return rankings;
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getRankings: " + std::string(e.what()));
    throw;
  }
}

void GroupRepository::configureTopic(const models::GroupTopic& topic) {
  if (topic.group_id <= 0) {
    throw std::invalid_argument("topic.group_id must be positive");
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    if (topic.telegram_topic_id.has_value()) {
      txn.exec_params(
        "INSERT INTO group_topics (group_id, telegram_topic_id, topic_type, is_active, created_at) "
        "VALUES ($1, $2, $3, $4, NOW()) "
        "ON CONFLICT (group_id, telegram_topic_id, topic_type) "
        "DO UPDATE SET is_active = $4",
        topic.group_id,
        topic.telegram_topic_id.value(),
        topic.topic_type,
        topic.is_active
      );
    } else {
      txn.exec_params(
        "INSERT INTO group_topics (group_id, telegram_topic_id, topic_type, is_active, created_at) "
        "VALUES ($1, NULL, $2, $3, NOW()) "
        "ON CONFLICT (group_id, telegram_topic_id, topic_type) "
        "DO UPDATE SET is_active = $3",
        topic.group_id,
        topic.topic_type,
        topic.is_active
      );
    }
    
    txn.commit();
    pool_->release(conn);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in configureTopic: " + std::string(e.what()));
    throw;
  }
}

std::optional<models::GroupTopic> GroupRepository::getTopic(
    int64_t group_id, int telegram_topic_id, const std::string& topic_type) {
  if (group_id <= 0) {
    return std::nullopt;
  }
  
  auto conn = pool_->acquire();
  if (!conn || !conn->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  try {
    pqxx::work txn(*conn);
    
    auto result = txn.exec_params(
      "SELECT id, group_id, telegram_topic_id, topic_type, is_active, created_at "
      "FROM group_topics "
      "WHERE group_id = $1 AND telegram_topic_id = $2 AND topic_type = $3",
      group_id, telegram_topic_id, topic_type
    );
    
    txn.commit();
    pool_->release(conn);
    
    if (result.empty()) {
      return std::nullopt;
    }
    
    return rowToGroupTopic(result[0]);
  } catch (const std::exception& e) {
    pool_->release(conn);
    auto logger = observability::Logger::getInstance();
    logger->error("Error in getTopic: " + std::string(e.what()));
    throw;
  }
}

models::Group GroupRepository::rowToGroup(const pqxx::row& row) {
  models::Group group;
  group.id = row["id"].as<int64_t>();
  group.telegram_group_id = row["telegram_group_id"].as<int64_t>();
  
  if (!row["name"].is_null()) {
    group.name = row["name"].as<std::string>();
  }
  
  group.is_active = row["is_active"].as<bool>();
  
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
  group.created_at = parseTimestamp(created_at_str);
  
  auto updated_at_str = row["updated_at"].as<std::string>();
  group.updated_at = parseTimestamp(updated_at_str);
  
  return group;
}

models::GroupPlayer GroupRepository::rowToGroupPlayer(const pqxx::row& row) {
  models::GroupPlayer gp;
  gp.id = row["id"].as<int64_t>();
  gp.group_id = row["group_id"].as<int64_t>();
  gp.player_id = row["player_id"].as<int64_t>();
  gp.current_elo = row["current_elo"].as<int>();
  gp.matches_played = row["matches_played"].as<int>();
  gp.matches_won = row["matches_won"].as<int>();
  gp.matches_lost = row["matches_lost"].as<int>();
  gp.version = row["version"].as<int>();
  
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
  gp.created_at = parseTimestamp(created_at_str);
  
  auto updated_at_str = row["updated_at"].as<std::string>();
  gp.updated_at = parseTimestamp(updated_at_str);
  
  return gp;
}

models::GroupTopic GroupRepository::rowToGroupTopic(const pqxx::row& row) {
  models::GroupTopic topic;
  topic.id = row["id"].as<int64_t>();
  topic.group_id = row["group_id"].as<int64_t>();
  
  if (!row["telegram_topic_id"].is_null()) {
    topic.telegram_topic_id = row["telegram_topic_id"].as<int>();
  }
  
  topic.topic_type = row["topic_type"].as<std::string>();
  topic.is_active = row["is_active"].as<bool>();
  
  // Parse timestamp
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
  topic.created_at = parseTimestamp(created_at_str);
  
  return topic;
}

}  // namespace repositories
