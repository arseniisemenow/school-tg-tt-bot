#include "config/config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace config {

Config& Config::getInstance() {
  static Config instance;
  return instance;
}

void Config::load(const std::string& config_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::ifstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open config file: " + config_path);
  }
  
  try {
    file >> config_data_;
    config_path_ = config_path;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("Failed to parse config file: " + 
                            std::string(e.what()));
  }
}

void Config::reload() {
  if (config_path_.empty()) {
    throw std::runtime_error("No config file loaded");
  }
  load(config_path_);
}

std::vector<std::string> Config::splitKey(const std::string& key) const {
  std::vector<std::string> parts;
  std::istringstream iss(key);
  std::string part;
  
  while (std::getline(iss, part, '.')) {
    parts.push_back(part);
  }
  
  return parts;
}

nlohmann::json Config::getValue(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto parts = splitKey(key);
  nlohmann::json current = config_data_;
  
  for (const auto& part : parts) {
    if (current.is_object() && current.contains(part)) {
      current = current[part];
    } else {
      return nlohmann::json();
    }
  }
  
  return current;
}

int Config::getInt(const std::string& key, int default_value) const {
  auto value = getValue(key);
  if (value.is_number_integer() || value.is_number_unsigned()) {
    return value.get<int>();
  }
  return default_value;
}

std::string Config::getString(const std::string& key, 
                              const std::string& default_value) const {
  auto value = getValue(key);
  if (value.is_string()) {
    return value.get<std::string>();
  }
  return default_value;
}

bool Config::getBool(const std::string& key, bool default_value) const {
  auto value = getValue(key);
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  return default_value;
}

double Config::getDouble(const std::string& key, double default_value) const {
  auto value = getValue(key);
  if (value.is_number()) {
    return value.get<double>();
  }
  return default_value;
}

nlohmann::json Config::getJson(const std::string& key) const {
  return getValue(key);
}

bool Config::hasKey(const std::string& key) const {
  auto value = getValue(key);
  return !value.is_null();
}

}  // namespace config

