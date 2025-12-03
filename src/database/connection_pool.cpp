#include "database/connection_pool.h"

#include <stdexcept>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>

namespace database {

ConnectionPool::ConnectionPool(const Config& config) : config_(config) {
  // Initialize minimum connections
  for (int i = 0; i < config_.min_size; ++i) {
    try {
      auto conn = createConnection();
      pool_.push_back(conn);
    } catch (const std::exception& e) {
      // Log error but continue
    }
  }
}

ConnectionPool::~ConnectionPool() {
  std::lock_guard<std::mutex> lock(mutex_);
  pool_.clear();
}

std::unique_ptr<ConnectionPool> ConnectionPool::create(
    const Config& config) {
  return std::make_unique<ConnectionPool>(config);
}

std::shared_ptr<pqxx::connection> ConnectionPool::createConnection() {
  try {
    auto conn = std::make_shared<pqxx::connection>(config_.connection_string);
    return conn;
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to create database connection: " + 
                             std::string(e.what()));
  }
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Try to find an available connection
  for (auto it = pool_.begin(); it != pool_.end(); ++it) {
    if ((*it)->is_open()) {
      auto conn = *it;
      pool_.erase(it);
      active_connections_++;
      return conn;
    }
  }
  
  // Create new connection if pool not at max
  if (pool_.size() + active_connections_ < config_.max_size) {
    active_connections_++;
    return createConnection();
  }
  
  // Pool exhausted
  throw std::runtime_error("Connection pool exhausted");
}

void ConnectionPool::release(std::shared_ptr<pqxx::connection> conn) {
  if (!conn || !conn->is_open()) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(mutex_);
  active_connections_--;
  pool_.push_back(conn);
}

int ConnectionPool::getActiveConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_connections_;
}

int ConnectionPool::getTotalConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pool_.size() + active_connections_;
}

bool ConnectionPool::healthCheck() {
  try {
    auto conn = acquire();
    pqxx::work txn(*conn);
    txn.exec("SELECT 1");
    release(conn);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

void ConnectionPool::cleanupIdleConnections() {
  // TODO: Implement cleanup logic
}

}  // namespace database

