#ifndef OBSERVABILITY_LOGGER_H
#define OBSERVABILITY_LOGGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>

namespace observability {

enum class LogLevel {
  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  FATAL
};

class Logger {
 public:
  static std::shared_ptr<Logger> getInstance();
  
  void log(LogLevel level, const std::string& message);
  void log(LogLevel level, const std::string& message, 
          const std::map<std::string, std::string>& context);
  
  // Convenience methods
  void trace(const std::string& message);
  void debug(const std::string& message);
  void info(const std::string& message);
  void warn(const std::string& message);
  void error(const std::string& message);
  void fatal(const std::string& message);
  
  void setLevel(LogLevel level);
  LogLevel getLevel() const { return level_; }

 private:
  Logger() = default;
  LogLevel level_ = LogLevel::INFO;
  mutable std::mutex mutex_;  // Mutable to allow locking in const methods
  
  std::string levelToString(LogLevel level) const;
  std::string formatMessage(LogLevel level, const std::string& message,
                            const std::map<std::string, std::string>& context) const;
};

}  // namespace observability

#endif  // OBSERVABILITY_LOGGER_H

