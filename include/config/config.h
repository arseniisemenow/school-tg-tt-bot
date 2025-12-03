#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H

#include <string>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

namespace config {

class Config {
 public:
  static Config& getInstance();
  
  // Load configuration from file
  void load(const std::string& config_path);
  
  // Reload configuration (thread-safe)
  void reload();
  
  // Getters with default values
  int getInt(const std::string& key, int default_value = 0) const;
  std::string getString(const std::string& key, 
                       const std::string& default_value = "") const;
  bool getBool(const std::string& key, bool default_value = false) const;
  double getDouble(const std::string& key, double default_value = 0.0) const;
  
  // Nested access (e.g., "database.connection_pool.max_size")
  nlohmann::json getJson(const std::string& key) const;
  
  // Check if key exists
  bool hasKey(const std::string& key) const;
  
  // Get current config path
  std::string getConfigPath() const { return config_path_; }

 private:
  Config() = default;
  ~Config() = default;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  
  std::string config_path_;
  nlohmann::json config_data_;
  mutable std::mutex mutex_;
  
  nlohmann::json getValue(const std::string& key) const;
  std::vector<std::string> splitKey(const std::string& key) const;
};

}  // namespace config

#endif  // CONFIG_CONFIG_H

