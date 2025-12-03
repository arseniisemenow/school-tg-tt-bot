#include "observability/logger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mutex>
#include <map>
#include <nlohmann/json.hpp>

namespace observability {

std::shared_ptr<Logger> Logger::getInstance() {
  // Use new directly since we're in a member function and have access to private constructor
  static std::shared_ptr<Logger> instance(new Logger());
  return instance;
}

void Logger::setLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = level;
}

std::string Logger::levelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::TRACE: return "TRACE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARN: return "WARN";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::FATAL: return "FATAL";
    default: return "UNKNOWN";
  }
}

std::string Logger::formatMessage(LogLevel level, const std::string& message,
                                  const std::map<std::string, std::string>& context) const {
  nlohmann::json log_entry;
  
  // Timestamp
  auto now = std::time(nullptr);
  auto tm = *std::gmtime(&now);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  log_entry["timestamp"] = oss.str();
  
  log_entry["level"] = levelToString(level);
  log_entry["message"] = message;
  
  // Add context
  if (!context.empty()) {
    for (const auto& [key, value] : context) {
      log_entry[key] = value;
    }
  }
  
  return log_entry.dump();
}

void Logger::log(LogLevel level, const std::string& message) {
  log(level, message, {});
}

void Logger::log(LogLevel level, const std::string& message,
                const std::map<std::string, std::string>& context) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (level < level_) {
    return;
  }
  
  std::string formatted = formatMessage(level, message, context);
  std::cout << formatted << std::endl;
}

void Logger::trace(const std::string& message) {
  log(LogLevel::TRACE, message);
}

void Logger::debug(const std::string& message) {
  log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
  log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
  log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
  log(LogLevel::ERROR, message);
}

void Logger::fatal(const std::string& message) {
  log(LogLevel::FATAL, message);
}

}  // namespace observability

