#ifndef UTILS_VALIDATION_H
#define UTILS_VALIDATION_H

#include <cstdint>
#include <string>

namespace utils {

// ELO bounds per ADR-010
constexpr int MIN_ELO = 0;
constexpr int MAX_ELO = 10000;

// String validation constants
constexpr size_t MAX_STRING_LENGTH = 1000;
constexpr size_t MAX_IDEMPOTENCY_KEY_LENGTH = 255;
constexpr size_t MAX_TOPIC_TYPE_LENGTH = 50;

// Validation functions
inline void validateId(int64_t id, const std::string& field_name) {
  if (id <= 0) {
    throw std::invalid_argument(field_name + " must be positive, got: " + std::to_string(id));
  }
}

inline void validateElo(int elo, const std::string& field_name) {
  if (elo < MIN_ELO) {
    throw std::invalid_argument(field_name + " cannot be negative, got: " + std::to_string(elo));
  }
  if (elo > MAX_ELO) {
    throw std::invalid_argument(field_name + " cannot exceed " + std::to_string(MAX_ELO) + ", got: " + std::to_string(elo));
  }
}

inline void validateStringLength(const std::string& str, size_t max_length, const std::string& field_name) {
  if (str.length() > max_length) {
    throw std::invalid_argument(field_name + " exceeds maximum length of " + std::to_string(max_length) + " characters");
  }
}

inline void validateNonEmptyString(const std::string& str, const std::string& field_name) {
  if (str.empty()) {
    throw std::invalid_argument(field_name + " cannot be empty");
  }
}

inline void validateIdempotencyKey(const std::string& key) {
  validateNonEmptyString(key, "idempotency_key");
  validateStringLength(key, MAX_IDEMPOTENCY_KEY_LENGTH, "idempotency_key");
}

inline void validateTopicType(const std::string& topic_type) {
  validateNonEmptyString(topic_type, "topic_type");
  validateStringLength(topic_type, MAX_TOPIC_TYPE_LENGTH, "topic_type");
}

inline void validateScore(int score, const std::string& field_name) {
  if (score < 0) {
    throw std::invalid_argument(field_name + " cannot be negative, got: " + std::to_string(score));
  }
}

}  // namespace utils

#endif  // UTILS_VALIDATION_H


