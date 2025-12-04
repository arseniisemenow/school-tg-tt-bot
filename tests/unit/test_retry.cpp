#include <gtest/gtest.h>
#include "utils/retry.h"
#include <chrono>
#include <thread>
#include <atomic>

class RetryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    call_count_ = 0;
    start_time_ = std::chrono::steady_clock::now();
  }
  
  void TearDown() override {
    call_count_ = 0;
  }
  
  std::atomic<int> call_count_{0};
  std::chrono::steady_clock::time_point start_time_;
  
  // Helper to get elapsed time in milliseconds
  long getElapsedMs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
  }
};

TEST_F(RetryTest, RetrySuccessOnFirstAttempt) {
  int result = utils::retryWithBackoff([this]() {
    call_count_++;
    return 42;
  });
  
  EXPECT_EQ(result, 42);
  EXPECT_EQ(call_count_, 1);
}

TEST_F(RetryTest, RetrySuccessAfterOneFailure) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(10);  // Short delay for testing
  config.backoff_multiplier = 2.0;
  
  int result = utils::retryWithBackoff([this]() {
    call_count_++;
    if (call_count_ == 1) {
      throw utils::OptimisticLockException("Lock conflict");
    }
    return 100;
  }, config);
  
  EXPECT_EQ(result, 100);
  EXPECT_EQ(call_count_, 2);
}

TEST_F(RetryTest, RetrySuccessAfterMultipleFailures) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.backoff_multiplier = 2.0;
  
  int result = utils::retryWithBackoff([this]() {
    call_count_++;
    if (call_count_ <= 2) {
      throw utils::OptimisticLockException("Lock conflict");
    }
    return 200;
  }, config);
  
  EXPECT_EQ(result, 200);
  EXPECT_EQ(call_count_, 3);
}

TEST_F(RetryTest, RetryFailsAfterMaxRetries) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.backoff_multiplier = 2.0;
  
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw utils::OptimisticLockException("Lock conflict");
    }, config);
  }, utils::OptimisticLockException);
  
  // Should have tried max_retries + 1 times (initial + retries)
  EXPECT_EQ(call_count_, config.max_retries + 1);
}

TEST_F(RetryTest, RetryExponentialBackoffTiming) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(50);  // 50ms for testing
  config.backoff_multiplier = 2.0;
  
  start_time_ = std::chrono::steady_clock::now();
  
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw utils::OptimisticLockException("Lock conflict");
    }, config);
  }, utils::OptimisticLockException);
  
  // Verify timing: should have delays of 50ms, 100ms, 200ms
  // Total time should be approximately 350ms (with some tolerance)
  long elapsed = getElapsedMs();
  EXPECT_GE(elapsed, 300) << "Should have waited at least 300ms (50+100+200)";
  EXPECT_LE(elapsed, 500) << "Should not wait more than 500ms (with tolerance)";
  
  EXPECT_EQ(call_count_, 4);  // Initial + 3 retries
}

TEST_F(RetryTest, RetryRespectsMaxDelay) {
  utils::RetryConfig config;
  config.max_retries = 5;
  config.initial_delay = std::chrono::milliseconds(100);
  config.backoff_multiplier = 2.0;
  config.max_delay = std::chrono::milliseconds(200);  // Cap at 200ms
  
  start_time_ = std::chrono::steady_clock::now();
  
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw utils::OptimisticLockException("Lock conflict");
    }, config);
  }, utils::OptimisticLockException);
  
  // Verify timing: delays should be 100ms, 200ms, 200ms, 200ms, 200ms
  // Total should be approximately 900ms
  long elapsed = getElapsedMs();
  EXPECT_GE(elapsed, 800) << "Should have waited at least 800ms";
  EXPECT_LE(elapsed, 1200) << "Should not wait more than 1200ms (with tolerance)";
}

TEST_F(RetryTest, RetryOnlyRetriesOptimisticLockException) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  
  // Other exceptions should not be retried
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw std::runtime_error("Other error");
    }, config);
  }, std::runtime_error);
  
  // Should only be called once (no retries)
  EXPECT_EQ(call_count_, 1);
}

TEST_F(RetryTest, RetryWithDefaultConfig) {
  // Test the simplified version without explicit config
  int result = utils::retryWithBackoff([this]() {
    call_count_++;
    if (call_count_ == 1) {
      throw utils::OptimisticLockException("Lock conflict");
    }
    return 999;
  });
  
  EXPECT_EQ(result, 999);
  EXPECT_EQ(call_count_, 2);
}

TEST_F(RetryTest, RetryWithVoidReturnType) {
  utils::RetryConfig config;
  config.max_retries = 2;
  config.initial_delay = std::chrono::milliseconds(10);
  
  bool executed = false;
  
  utils::retryWithBackoff([&executed, this]() {
    call_count_++;
    if (call_count_ == 1) {
      throw utils::OptimisticLockException("Lock conflict");
    }
    executed = true;
  }, config);
  
  EXPECT_TRUE(executed);
  EXPECT_EQ(call_count_, 2);
}

TEST_F(RetryTest, RetryWithCustomBackoffMultiplier) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(50);
  config.backoff_multiplier = 1.5;  // Custom multiplier
  
  start_time_ = std::chrono::steady_clock::now();
  
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw utils::OptimisticLockException("Lock conflict");
    }, config);
  }, utils::OptimisticLockException);
  
  // Delays should be: 50ms, 75ms (50*1.5), 112ms (75*1.5)
  // Total approximately 237ms
  long elapsed = getElapsedMs();
  EXPECT_GE(elapsed, 200) << "Should have waited at least 200ms";
  EXPECT_LE(elapsed, 400) << "Should not wait more than 400ms (with tolerance)";
}

TEST_F(RetryTest, RetryZeroMaxRetries) {
  utils::RetryConfig config;
  config.max_retries = 0;
  config.initial_delay = std::chrono::milliseconds(10);
  
  // With max_retries = 0, should only try once
  EXPECT_THROW({
    utils::retryWithBackoff([this]() {
      call_count_++;
      throw utils::OptimisticLockException("Lock conflict");
    }, config);
  }, utils::OptimisticLockException);
  
  EXPECT_EQ(call_count_, 1);
}

TEST_F(RetryTest, RetrySuccessAfterMaxRetriesMinusOne) {
  utils::RetryConfig config;
  config.max_retries = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  
  // Should succeed on the last allowed attempt
  int result = utils::retryWithBackoff([this]() {
    call_count_++;
    if (call_count_ <= 3) {  // Fail first 3 times, succeed on 4th
      throw utils::OptimisticLockException("Lock conflict");
    }
    return 555;
  }, config);
  
  EXPECT_EQ(result, 555);
  EXPECT_EQ(call_count_, 4);  // Initial + 3 retries
}

