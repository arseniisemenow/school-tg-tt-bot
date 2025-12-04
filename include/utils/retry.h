#ifndef UTILS_RETRY_H
#define UTILS_RETRY_H

#include <functional>
#include <chrono>
#include <thread>
#include <stdexcept>

namespace utils {

// Exception thrown when optimistic lock conflict occurs
class OptimisticLockException : public std::runtime_error {
 public:
  explicit OptimisticLockException(const std::string& message = "Optimistic lock conflict")
      : std::runtime_error(message) {}
};

// Retry configuration
struct RetryConfig {
  int max_retries = 3;
  std::chrono::milliseconds initial_delay{100};
  double backoff_multiplier = 2.0;
  std::chrono::milliseconds max_delay{1000};
};

// Retry a callable with exponential backoff
// The callable should throw OptimisticLockException on conflicts
// Returns the result of the callable if successful
template<typename Callable>
auto retryWithBackoff(Callable&& func, const RetryConfig& config = RetryConfig{}) 
    -> decltype(func()) {
  int attempt = 0;
  std::chrono::milliseconds delay = config.initial_delay;
  
  while (attempt <= config.max_retries) {
    try {
      return func();
    } catch (const OptimisticLockException& e) {
      if (attempt >= config.max_retries) {
        // Max retries exceeded, rethrow
        throw;
      }
      
      // Wait before retrying
      std::this_thread::sleep_for(delay);
      
      // Calculate next delay with exponential backoff
      delay = std::chrono::milliseconds(
          static_cast<long>(delay.count() * config.backoff_multiplier)
      );
      
      // Cap at max_delay
      if (delay > config.max_delay) {
        delay = config.max_delay;
      }
      
      attempt++;
    }
  }
  
  // Should never reach here, but satisfy compiler
  throw std::runtime_error("Retry logic error");
}

}  // namespace utils

#endif  // UTILS_RETRY_H

