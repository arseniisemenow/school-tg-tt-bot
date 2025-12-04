#ifndef DATABASE_TRANSACTION_H
#define DATABASE_TRANSACTION_H

#include <memory>
#include <pqxx/pqxx>

namespace database {
class ConnectionPool;
}

namespace database {

// RAII-style transaction wrapper
// Automatically commits on success or rolls back on exception
class Transaction {
 public:
  // Acquire connection and start transaction
  explicit Transaction(std::shared_ptr<ConnectionPool> pool);
  
  // Destructor: commits if not already committed/rolled back
  ~Transaction();
  
  // Get the underlying pqxx::work transaction
  pqxx::work& get() { return *txn_; }
  const pqxx::work& get() const { return *txn_; }
  
  // Explicitly commit the transaction
  void commit();
  
  // Explicitly rollback the transaction
  void rollback();
  
  // Check if transaction is still active
  bool isActive() const { return active_; }
  
  // Non-copyable
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  
  // Movable
  Transaction(Transaction&&) = default;
  Transaction& operator=(Transaction&&) = default;

 private:
  std::shared_ptr<ConnectionPool> pool_;
  std::shared_ptr<pqxx::connection> conn_;
  std::unique_ptr<pqxx::work> txn_;
  bool active_;
  bool committed_;
};

}  // namespace database

#endif  // DATABASE_TRANSACTION_H

