#ifndef DATABASE_CONNECTION_POOL_H
#define DATABASE_CONNECTION_POOL_H

#include <memory>
#include <string>
#include <pqxx/pqxx>

namespace database {

class ConnectionPool {
 public:
  struct Config {
    std::string connection_string;
    int min_size = 2;
    int max_size = 10;
    int idle_timeout_seconds = 300;
    int max_lifetime_seconds = 3600;
  };
  
  static std::unique_ptr<ConnectionPool> create(const Config& config);
  
  ~ConnectionPool();
  
  // Get a connection from the pool
  std::shared_ptr<pqxx::connection> acquire();
  
  // Return a connection to the pool
  void release(std::shared_ptr<pqxx::connection> conn);
  
  // Get pool statistics
  int getActiveConnections() const;
  int getTotalConnections() const;
  
  // Health check
  bool healthCheck();

 private:
  ConnectionPool(const Config& config);
  
  Config config_;
  std::vector<std::shared_ptr<pqxx::connection>> pool_;
  mutable std::mutex mutex_;  // Mutable to allow locking in const methods
  int active_connections_ = 0;
  
  std::shared_ptr<pqxx::connection> createConnection();
  void cleanupIdleConnections();
};

}  // namespace database

#endif  // DATABASE_CONNECTION_POOL_H

