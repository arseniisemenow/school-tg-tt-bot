#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

#include "config/config.h"
#include "database/connection_pool.h"
#include "bot/bot.h"
#include "observability/logger.h"

std::string getEnvVar(const std::string& name, 
                     const std::string& default_value = "") {
  const char* value = std::getenv(name.c_str());
  return value ? std::string(value) : default_value;
}

std::string findConfigFile() {
  // Check CONFIG_FILE environment variable
  std::string config_file = getEnvVar("CONFIG_FILE");
  if (!config_file.empty()) {
    return config_file;
  }
  
  // Check environment
  std::string env = getEnvVar("ENVIRONMENT", "development");
  
  // Try different paths
  std::vector<std::string> paths = {
    "./config/config." + env + ".json",
    "./config/config.json",
    "/etc/school-tg-bot/config.json"
  };
  
  for (const auto& path : paths) {
    std::ifstream file(path);
    if (file.good()) {
      return path;
    }
  }
  
  throw std::runtime_error("Config file not found");
}

std::string buildDatabaseConnectionString() {
  std::string db_url = getEnvVar("DATABASE_URL");
  if (!db_url.empty()) {
    return db_url;
  }
  
  std::string host = getEnvVar("POSTGRES_HOST", "localhost");
  std::string port = getEnvVar("POSTGRES_PORT", "5432");
  std::string db = getEnvVar("POSTGRES_DB", "school_tg_bot");
  std::string user = getEnvVar("POSTGRES_USER", "postgres");
  std::string password = getEnvVar("POSTGRES_PASSWORD");
  
  if (password.empty()) {
    throw std::runtime_error("POSTGRES_PASSWORD not set");
  }
  
  return "postgresql://" + user + ":" + password + "@" + host + 
         ":" + port + "/" + db;
}

int main(int argc, char* argv[]) {
  try {
    // Initialize logger
    auto logger = observability::Logger::getInstance();
    logger->info("Starting School Telegram Table Tennis Bot");
    
    // Load configuration
    std::string config_path = findConfigFile();
    auto& config = config::Config::getInstance();
    config.load(config_path);
    logger->info("Configuration loaded from: " + config_path);
    
    // Set log level from config
    std::string log_level_str = config.getString("observability.log_level", "INFO");
    observability::LogLevel log_level = observability::LogLevel::INFO;
    if (log_level_str == "DEBUG") log_level = observability::LogLevel::DEBUG;
    else if (log_level_str == "TRACE") log_level = observability::LogLevel::TRACE;
    else if (log_level_str == "WARN") log_level = observability::LogLevel::WARN;
    else if (log_level_str == "ERROR") log_level = observability::LogLevel::ERROR;
    logger->setLevel(log_level);
    
    // Initialize database connection pool
    std::string db_conn_str = buildDatabaseConnectionString();
    database::ConnectionPool::Config db_config;
    db_config.connection_string = db_conn_str;
    db_config.min_size = config.getInt("database.connection_pool.min_size", 2);
    db_config.max_size = config.getInt("database.connection_pool.max_size", 10);
    db_config.idle_timeout_seconds = config.getInt("database.connection_pool.idle_timeout_seconds", 300);
    db_config.max_lifetime_seconds = config.getInt("database.connection_pool.max_lifetime_seconds", 3600);
    
    auto db_pool = database::ConnectionPool::create(db_config);
    logger->info("Database connection pool initialized");
    
    // Health check
    if (!db_pool->healthCheck()) {
      logger->error("Database health check failed");
      return 1;
    }
    
    // Get Telegram bot token
    std::string bot_token = getEnvVar("TELEGRAM_BOT_TOKEN");
    if (bot_token.empty()) {
      throw std::runtime_error("TELEGRAM_BOT_TOKEN not set");
    }
    
    // Initialize bot
    bot::Bot telegram_bot(bot_token);
    telegram_bot.initialize();
    logger->info("Telegram bot initialized");
    
    // Start bot
    bool webhook_enabled = config.getBool("telegram.webhook.enabled", false);
    bool polling_enabled = config.getBool("telegram.polling.enabled", true);
    
    if (webhook_enabled) {
      int port = config.getInt("telegram.webhook.port", 8443);
      std::string path = config.getString("telegram.webhook.path", "/webhook");
      // TODO: Get webhook URL from environment or config
      std::string webhook_url = getEnvVar("WEBHOOK_URL");
      if (webhook_url.empty()) {
        throw std::runtime_error("WEBHOOK_URL not set but webhook enabled");
      }
      telegram_bot.startWebhook(webhook_url, port);
      logger->info("Bot started in webhook mode on port " + std::to_string(port));
    } else if (polling_enabled) {
      telegram_bot.startPolling();
      logger->info("Bot started in polling mode");
    } else {
      throw std::runtime_error("Neither webhook nor polling enabled");
    }
    
    // Keep running
    logger->info("Bot is running. Press Ctrl+C to stop.");
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  
  return 0;
}
