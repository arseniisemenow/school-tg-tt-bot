#include "database/transaction.h"
#include "database/connection_pool.h"

namespace database {

Transaction::Transaction(std::shared_ptr<ConnectionPool> pool)
    : pool_(pool), active_(true), committed_(false) {
  if (!pool_) {
    throw std::runtime_error("ConnectionPool is null");
  }
  
  // Acquire connection from pool
  conn_ = pool_->acquire();
  
  if (!conn_ || !conn_->is_open()) {
    throw std::runtime_error("Failed to acquire database connection");
  }
  
  // Create transaction
  txn_ = std::make_unique<pqxx::work>(*conn_);
}

Transaction::~Transaction() {
  if (active_ && !committed_) {
    try {
      if (txn_) {
        txn_->abort();
      }
    } catch (const std::exception&) {
      // Ignore errors during rollback in destructor
    }
  }
  
  // Release connection back to pool
  if (conn_ && pool_) {
    pool_->release(conn_);
  }
}

void Transaction::commit() {
  if (!active_) {
    throw std::runtime_error("Transaction is not active");
  }
  
  if (committed_) {
    throw std::runtime_error("Transaction already committed");
  }
  
  if (txn_) {
    txn_->commit();
    committed_ = true;
    active_ = false;
  }
}

void Transaction::rollback() {
  if (!active_) {
    return;  // Already rolled back or committed
  }
  
  if (committed_) {
    throw std::runtime_error("Cannot rollback committed transaction");
  }
  
  if (txn_) {
    txn_->abort();
    active_ = false;
  }
}

}  // namespace database

