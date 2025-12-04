#include <gtest/gtest.h>
#include "database/transaction.h"
#include "database/connection_pool.h"
#include <stdexcept>
#include <cstdlib>

class TransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get database connection string from environment
    const char* db_url = std::getenv("DATABASE_URL");
    if (!db_url) {
      // Try individual components
      std::string host = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
      std::string port = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5432";
      std::string db = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "school_tg_bot";
      std::string user = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "postgres";
      std::string password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "postgres";
      
      connection_string_ = "postgresql://" + user + ":" + password + "@" + host + ":" + port + "/" + db;
    } else {
      connection_string_ = db_url;
    }
    
    database::ConnectionPool::Config config;
    config.connection_string = connection_string_;
    config.min_size = 1;
    config.max_size = 5;
    
    auto pool_unique = database::ConnectionPool::create(config);
    pool_ = std::shared_ptr<database::ConnectionPool>(std::move(pool_unique));
    
    // Verify connection works
    if (!pool_->healthCheck()) {
      FAIL() << "Database connection failed. Cannot run transaction tests.";
    }
  }
  
  void TearDown() override {
    pool_.reset();
  }
  
  std::string connection_string_;
  std::shared_ptr<database::ConnectionPool> pool_;
};

TEST_F(TransactionTest, BasicTransactionCreation) {
  ASSERT_NE(pool_, nullptr);
  
  database::Transaction txn(pool_);
  EXPECT_TRUE(txn.isActive());
  
  // Transaction should be able to execute queries
  auto& work = txn.get();
  auto result = work.exec("SELECT 1 as test_value");
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result[0]["test_value"].as<int>(), 1);
}

TEST_F(TransactionTest, TransactionCommit) {
  database::Transaction txn(pool_);
  EXPECT_TRUE(txn.isActive());
  
  auto& work = txn.get();
  // Use a regular table that persists (clean up after)
  work.exec("CREATE TABLE IF NOT EXISTS test_transaction_commit (id INTEGER)");
  work.exec("DELETE FROM test_transaction_commit");
  work.exec("INSERT INTO test_transaction_commit VALUES (42)");
  
  txn.commit();
  EXPECT_FALSE(txn.isActive());
  
  // Verify data was committed by starting a new transaction
  database::Transaction txn2(pool_);
  auto& work2 = txn2.get();
  auto result = work2.exec("SELECT id FROM test_transaction_commit WHERE id = 42");
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result[0]["id"].as<int>(), 42);
  // Clean up
  work2.exec("DROP TABLE IF EXISTS test_transaction_commit");
  txn2.commit();
}

TEST_F(TransactionTest, TransactionRollback) {
  // First create the table in a separate committed transaction
  {
    database::Transaction setup_txn(pool_);
    auto& setup_work = setup_txn.get();
    setup_work.exec("CREATE TABLE IF NOT EXISTS test_rollback_table (id INTEGER)");
    setup_work.exec("DELETE FROM test_rollback_table");
    setup_txn.commit();
  }
  
  // Now test rollback
  database::Transaction txn(pool_);
  EXPECT_TRUE(txn.isActive());
  
  auto& work = txn.get();
  work.exec("INSERT INTO test_rollback_table VALUES (99)");
  
  txn.rollback();
  EXPECT_FALSE(txn.isActive());
  
  // Verify data was rolled back - table should be empty
  database::Transaction txn2(pool_);
  auto& work2 = txn2.get();
  auto result = work2.exec("SELECT COUNT(*) as cnt FROM test_rollback_table");
  // After rollback, the insert should not be visible
  EXPECT_FALSE(result.empty());
  int count = result[0]["cnt"].as<int>();
  EXPECT_EQ(count, 0) << "Data should be rolled back";
  // Clean up
  work2.exec("DROP TABLE IF EXISTS test_rollback_table");
  txn2.commit();
}

TEST_F(TransactionTest, TransactionAutoRollbackOnException) {
  database::Transaction txn(pool_);
  EXPECT_TRUE(txn.isActive());
  
  auto& work = txn.get();
  work.exec("CREATE TABLE IF NOT EXISTS test_auto_rollback (id INTEGER PRIMARY KEY)");
  work.exec("DELETE FROM test_auto_rollback");
  work.exec("INSERT INTO test_auto_rollback VALUES (1)");
  
  // Cause an exception (duplicate key)
  try {
    work.exec("INSERT INTO test_auto_rollback VALUES (1)");
    FAIL() << "Expected exception for duplicate key";
  } catch (const std::exception&) {
    // Expected - transaction should rollback automatically in destructor
  }
  
  // Transaction should be rolled back in destructor
  // Clean up table in a new transaction
  database::Transaction txn2(pool_);
  auto& work2 = txn2.get();
  work2.exec("DROP TABLE IF EXISTS test_auto_rollback");
  txn2.commit();
}

TEST_F(TransactionTest, CannotCommitTwice) {
  database::Transaction txn(pool_);
  txn.commit();
  
  EXPECT_THROW(txn.commit(), std::runtime_error);
}

TEST_F(TransactionTest, CannotUseAfterCommit) {
  database::Transaction txn(pool_);
  auto& work = txn.get();
  work.exec("SELECT 1");
  txn.commit();
  
  // Should not be able to use transaction after commit
  EXPECT_FALSE(txn.isActive());
}

TEST_F(TransactionTest, CannotUseAfterRollback) {
  database::Transaction txn(pool_);
  txn.rollback();
  
  EXPECT_FALSE(txn.isActive());
}

