#include "bot/bot.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>

#include "database/connection_pool.h"
#include "database/transaction.h"
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "repositories/match_repository.h"
#include "school21/api_client.h"
#include "utils/elo_calculator.h"
#include "utils/retry.h"
#include "observability/logger.h"
#include "config/config.h"
#include "models/group.h"
#include "models/player.h"
#include "models/match.h"
#include <tgbotxx/objects/ReplyParameters.hpp>
#include <tgbotxx/objects/ReactionType.hpp>
#include <tgbotxx/Api.hpp>
#include <pqxx/pqxx>

namespace bot {

Bot::Bot(const std::string& token) 
    : tgbotxx::Bot(token), 
      token_(token),
      logger_(observability::Logger::getInstance().get()) {
}

Bot::~Bot() {
  stop();
}

void Bot::initialize() {
  // Initialize ELO calculator with K-factor from config
  auto& config = config::Config::getInstance();
  int k_factor = config.getInt("elo.k_factor", 32);
  elo_calculator_ = std::make_unique<utils::EloCalculator>(k_factor);
  
  logger_->info("Bot initialized (dependencies must be set via setDependencies)");
}

void Bot::setDependencies(
    std::shared_ptr<database::ConnectionPool> db_pool,
    std::unique_ptr<repositories::GroupRepository> group_repo,
    std::unique_ptr<repositories::PlayerRepository> player_repo,
    std::unique_ptr<repositories::MatchRepository> match_repo,
    std::unique_ptr<school21::ApiClient> school21_client) {
  db_pool_ = db_pool;
  group_repo_ = std::move(group_repo);
  player_repo_ = std::move(player_repo);
  match_repo_ = std::move(match_repo);
  school21_client_ = std::move(school21_client);
  logger_->info("Bot dependencies set");
}

void Bot::onCommand(const tgbotxx::Ptr<tgbotxx::Message>& command) {
  try {
    if (!command) {
      logger_->warn("onCommand called with null command");
      return;
    }
    
    logger_->info("Command received: " + (command->text.empty() ? "empty" : command->text));
    
    std::string cmd = extractCommandName(command);
    logger_->info("Extracted command: " + (cmd.empty() ? "empty" : cmd));
    
    if (cmd == "start") {
      handleStart(command);
    } else if (cmd == "match") {
      handleMatch(command);
    } else if (cmd == "ranking" || cmd == "rank") {
      handleRanking(command);
    } else if (cmd == "id") {
      handleId(command);
    } else if (cmd == "id_guest") {
      handleIdGuest(command);
    } else if (cmd == "undo") {
      handleUndo(command);
    } else if (cmd == "config_topic") {
      handleConfigTopic(command);
    } else if (cmd == "help") {
      handleHelp(command);
    } else {
      logger_->info("Unknown command: " + cmd);
    }
  } catch (const std::exception& e) {
    logger_->error("Error in onCommand: " + std::string(e.what()));
  }
}

void Bot::onChatMemberUpdated(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& chatMember) {
  try {
    if (!chatMember || !chatMember->chat || !chatMember->newChatMember) {
      return;
    }
    
    int64_t chat_id = chatMember->chat->id;
    int64_t user_id = chatMember->from ? chatMember->from->id : 0;
    std::string status = chatMember->newChatMember->status;
    
    logger_->info("Chat member update: chat_id=" + std::to_string(chat_id) + 
                  ", user_id=" + std::to_string(user_id) + 
                  ", status=" + status);
    
    // Check if bot was removed
    // TODO: Get bot's own user ID from tgbotxx API
    // For now, check if the user is the bot by comparing with a cached bot user ID
    // This is a workaround - in production, we should get bot info at startup
    if (chatMember->newChatMember->user) {
      // We'll need to check this differently - maybe store bot user ID at initialization
      // For now, skip the check and handle all removals
    }
    
    // Handle member join
    if (status == "member") {
      handleMemberJoin(chatMember);
    }
    // Handle member leave
    else if (status == "left" || status == "kicked") {
      handleMemberLeave(chatMember);
    }
  } catch (const std::exception& e) {
    logger_->error("Error handling chat member update: " + std::string(e.what()));
  }
}

void Bot::onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    if (!message) return;
    
    // Check if this is a command - tgbotxx might not call onCommand for all commands
    // So we manually check and route commands here
    if (!message->text.empty() && message->text.front() == '/') {
      // This is a command - extract and handle it
      std::string cmd = extractCommandName(message);
      if (!cmd.empty()) {
        logger_->info("Command detected in onAnyMessage: " + cmd);
        // Route to command handler
        onCommand(message);
        return;
      }
    }
    
    // Command-only mode: ignore non-command messages
    logger_->debug("Message received (not a command): " + 
                  (message->text.empty() ? "empty" : message->text.substr(0, 50)));
  } catch (const std::exception& e) {
    logger_->error("Error in onAnyMessage: " + std::string(e.what()));
  }
}

std::string Bot::extractCommandName(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (message->text.empty() || message->text.front() != '/') {
    return "";
  }
  
  // Extract command name (everything after '/' until space or end)
  const std::string& text = message->text;
  size_t start = 1;  // Skip '/'
  size_t end = text.find_first_of(" \n", start);
  if (end == std::string::npos) {
    end = text.length();
  }
  
  std::string cmd = text.substr(start, end - start);
  
  // Remove bot name if present (e.g., "/command@botname" -> "command")
  size_t at_pos = cmd.find('@');
  if (at_pos != std::string::npos) {
    cmd = cmd.substr(0, at_pos);
  }
  
  return cmd;
}

void Bot::startPolling() {
  if (running_) {
    return;
  }
  
  running_ = true;
  try {
    start();  // tgbotxx::Bot::start() starts polling
  } catch (const std::exception& e) {
    running_ = false;
    throw std::runtime_error("Failed to start polling: " + 
                             std::string(e.what()));
  }
}

void Bot::startWebhook(const std::string& webhook_url, int port) {
  if (running_) {
    return;
  }
  
  running_ = true;
  try {
    // TODO: Set webhook and start webhook server
    // tgbotxx may have setWebhook method, check API
    // For now, this is a placeholder
    (void)webhook_url;
    (void)port;
  } catch (const std::exception& e) {
    running_ = false;
    throw std::runtime_error("Failed to start webhook: " + 
                             std::string(e.what()));
  }
}

void Bot::stop() {
  if (!running_) {
    return;
  }
  
  running_ = false;
  tgbotxx::Bot::stop();  // Call parent stop method
}

// ============================================================================
// Group Event Handlers
// ============================================================================

void Bot::handleMemberJoin(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  try {
    if (!update->from) return;
    
    int64_t user_id = update->from->id;
    int64_t chat_id = update->chat->id;
    
    logger_->info("Member joined: user_id=" + std::to_string(user_id) + 
                  ", chat_id=" + std::to_string(chat_id));
    
    // Log member join - no automatic action per ADR-011
    // Users register themselves via /id or /id_guest commands
    
    // Send important info to logs topic if configured
    std::string username = update->from->username.empty() ? 
        ("User " + std::to_string(update->from->id)) : update->from->username;
    sendToLogsTopic(chat_id, "👋 " + username + " joined the group. Welcome!");
  } catch (const std::exception& e) {
    logger_->error("Error handling member join: " + std::string(e.what()));
  }
}

void Bot::handleMemberLeave(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  try {
    if (!update->from || !player_repo_) return;
    
    int64_t user_id = update->from->id;
    
    logger_->info("Member left: user_id=" + std::to_string(user_id));
    
    // Get player and soft delete
    auto player = player_repo_->getByTelegramId(user_id);
    if (player) {
      player_repo_->softDelete(player->id);
      logger_->info("Player soft deleted: player_id=" + std::to_string(player->id));
    }
  } catch (const std::exception& e) {
    logger_->error("Error handling member leave: " + std::string(e.what()));
  }
}

void Bot::handleBotRemoval(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  try {
    if (!update->chat || !group_repo_) return;
    
    int64_t chat_id = update->chat->id;
    
    logger_->warn("Bot removed from group: chat_id=" + std::to_string(chat_id));
    
    // Mark group as inactive
    auto group = group_repo_->getByTelegramId(chat_id);
    if (group) {
      // TODO: Update group.is_active = false when repository supports it
      logger_->info("Group marked as inactive: group_id=" + std::to_string(group->id));
    }
  } catch (const std::exception& e) {
    logger_->error("Error handling bot removal: " + std::string(e.what()));
  }
}

void Bot::handleGroupMigration(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    if (!message->migrateFromChatId || !group_repo_) return;
    
    int64_t old_chat_id = message->migrateFromChatId;
    int64_t new_chat_id = message->chat->id;
    
    logger_->info("Group migration: old_chat_id=" + std::to_string(old_chat_id) + 
                  ", new_chat_id=" + std::to_string(new_chat_id));
    
    // Update group telegram_group_id
    auto group = group_repo_->getByTelegramId(old_chat_id);
    if (group) {
      // TODO: Update group.telegram_group_id when repository supports it
      logger_->info("Group migrated: group_id=" + std::to_string(group->id));
    }
  } catch (const std::exception& e) {
    logger_->error("Error handling group migration: " + std::string(e.what()));
  }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Bot::handleStart(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    std::string help_text = 
        "Welcome to School Telegram Table Tennis Bot!\n\n"
        "Available commands:\n"
        "/match @player1 @player2 <score1> <score2> - Register a match\n"
        "/ranking - Show current rankings\n"
        "/id <school_nickname> - Verify your School21 nickname\n"
        "/id_guest - Register as guest player\n"
        "/undo - Undo last match (with reply) or last match\n"
        "/config_topic <topic_type> - Configure topic (admin only)\n"
        "/help - Show this help message\n\n"
        "For command-specific help, use: /<command> help";
    
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, help_text, std::nullopt, topic_id);
  } catch (const std::exception& e) {
    logger_->error("Error handling start command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to process command");
  }
}

void Bot::handleMatch(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id, 
                  "Match command format:\n"
                  "/match @player1 @player2 <score1> <score2>\n\n"
                  "Example: /match @alice @bob 3 1\n\n"
                  "This command must be used in the matches topic (if configured).",
                  message->messageId, topic_id);
      return;
    }
    
    // Validate topic
    if (!isCommandInCorrectTopic(message, "matches")) {
      sendErrorMessage(message, "Match commands must be used in the matches topic");
      return;
    }
    
    // Parse command
    auto parsed = parseMatchCommand(message);
    if (!parsed.valid) {
      sendErrorMessage(message, 
                      "Invalid format. Use: /match @player1 @player2 <score1> <score2>\n"
                      "Example: /match @alice @bob 3 1");
      return;
    }
    
    // Get or create group
    std::string group_name = message->chat->title.empty() ? "" : message->chat->title;
    auto group = getOrCreateGroup(message->chat->id, group_name);
    
    // Get or create players
    auto player1 = getOrCreatePlayer(parsed.player1_user_id);
    auto player2 = getOrCreatePlayer(parsed.player2_user_id);
    
    // Get or create group players (ensure they exist in the group)
    getOrCreateGroupPlayer(group.id, player1.id);
    getOrCreateGroupPlayer(group.id, player2.id);
    
    // Check for duplicate
    std::string idempotency_key = generateIdempotencyKey(message);
    if (isDuplicateMatch(idempotency_key)) {
      sendErrorMessage(message, "This match was already registered");
      return;
    }
    
    // Use retry logic with exponential backoff for optimistic locking
    utils::RetryConfig retry_config;
    retry_config.max_retries = 3;
    retry_config.initial_delay = std::chrono::milliseconds(100);
    retry_config.backoff_multiplier = 2.0;
    
    int elo1_before = 0;
    int elo2_before = 0;
    int elo1_after = 0;
    int elo2_after = 0;
    int elo1_change = 0;
    int elo2_change = 0;
    models::Match created_match;
    
    utils::retryWithBackoff([&]() {
      // Start transaction
      database::Transaction txn(db_pool_);
      auto& work = txn.get();
      
      // 1. Check idempotency key (SELECT in transaction)
      auto idempotency_result = work.exec_params(
        "SELECT id FROM matches WHERE idempotency_key = $1",
        idempotency_key
      );
      if (!idempotency_result.empty()) {
        throw std::runtime_error("Match with this idempotency key already exists");
      }
      
      // 2. Read current ELO and version for both players (SELECT ... FOR UPDATE)
      auto gp1_result = work.exec_params(
        "SELECT id, current_elo, matches_played, matches_won, matches_lost, version "
        "FROM group_players "
        "WHERE group_id = $1 AND player_id = $2 FOR UPDATE",
        group.id, player1.id
      );
      if (gp1_result.empty()) {
        throw std::runtime_error("Group player 1 not found");
      }
      
      auto gp2_result = work.exec_params(
        "SELECT id, current_elo, matches_played, matches_won, matches_lost, version "
        "FROM group_players "
        "WHERE group_id = $1 AND player_id = $2 FOR UPDATE",
        group.id, player2.id
      );
      if (gp2_result.empty()) {
        throw std::runtime_error("Group player 2 not found");
      }
      
      // Extract current state
      int64_t gp1_id = gp1_result[0]["id"].as<int64_t>();
      int gp1_current_elo = gp1_result[0]["current_elo"].as<int>();
      int gp1_matches_played = gp1_result[0]["matches_played"].as<int>();
      int gp1_matches_won = gp1_result[0]["matches_won"].as<int>();
      int gp1_matches_lost = gp1_result[0]["matches_lost"].as<int>();
      int gp1_version = gp1_result[0]["version"].as<int>();
      
      int64_t gp2_id = gp2_result[0]["id"].as<int64_t>();
      int gp2_current_elo = gp2_result[0]["current_elo"].as<int>();
      int gp2_matches_played = gp2_result[0]["matches_played"].as<int>();
      int gp2_matches_won = gp2_result[0]["matches_won"].as<int>();
      int gp2_matches_lost = gp2_result[0]["matches_lost"].as<int>();
      int gp2_version = gp2_result[0]["version"].as<int>();
      
      // 3. Calculate new ELO values
      auto [new_elo1, new_elo2] = elo_calculator_->calculate(
          gp1_current_elo, gp2_current_elo, parsed.score1, parsed.score2);
      
      elo1_before = gp1_current_elo;
      elo2_before = gp2_current_elo;
      elo1_after = new_elo1;
      elo2_after = new_elo2;
      elo1_change = elo1_after - elo1_before;
      elo2_change = elo2_after - elo2_before;
      
      // 4. Update player1 ELO with optimistic locking
      int gp1_new_matches_played = gp1_matches_played + 1;
      int gp1_new_matches_won = gp1_matches_won;
      int gp1_new_matches_lost = gp1_matches_lost;
      if (parsed.score1 > parsed.score2) {
        gp1_new_matches_won++;
      } else if (parsed.score1 < parsed.score2) {
        gp1_new_matches_lost++;
      }
      
      auto update1_result = work.exec_params(
        "UPDATE group_players SET "
        "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
        "version = version + 1, updated_at = NOW() "
        "WHERE id = $5 AND version = $6",
        elo1_after, gp1_new_matches_played, gp1_new_matches_won, gp1_new_matches_lost,
        gp1_id, gp1_version
      );
      
      if (update1_result.affected_rows() == 0) {
        throw utils::OptimisticLockException("Optimistic lock conflict for player 1");
      }
      
      // 5. Update player2 ELO with optimistic locking
      int gp2_new_matches_played = gp2_matches_played + 1;
      int gp2_new_matches_won = gp2_matches_won;
      int gp2_new_matches_lost = gp2_matches_lost;
      if (parsed.score2 > parsed.score1) {
        gp2_new_matches_won++;
      } else if (parsed.score2 < parsed.score1) {
        gp2_new_matches_lost++;
      }
      
      auto update2_result = work.exec_params(
        "UPDATE group_players SET "
        "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
        "version = version + 1, updated_at = NOW() "
        "WHERE id = $5 AND version = $6",
        elo2_after, gp2_new_matches_played, gp2_new_matches_won, gp2_new_matches_lost,
        gp2_id, gp2_version
      );
      
      if (update2_result.affected_rows() == 0) {
        throw utils::OptimisticLockException("Optimistic lock conflict for player 2");
      }
      
      // 6. Insert match record
      auto match_result = work.exec_params(
        "INSERT INTO matches (group_id, player1_id, player2_id, player1_score, player2_score, "
        "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
        "idempotency_key, created_by_telegram_user_id, created_at, is_undone) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW(), FALSE) "
        "RETURNING id, created_at",
        group.id, player1.id, player2.id, parsed.score1, parsed.score2,
        elo1_before, elo2_before, elo1_after, elo2_after,
        idempotency_key, message->from ? message->from->id : 0
      );
      
      if (match_result.empty()) {
        throw std::runtime_error("Failed to create match");
      }
      
      created_match.id = match_result[0]["id"].as<int64_t>();
      created_match.group_id = group.id;
      created_match.player1_id = player1.id;
      created_match.player2_id = player2.id;
      created_match.player1_score = parsed.score1;
      created_match.player2_score = parsed.score2;
      created_match.player1_elo_before = elo1_before;
      created_match.player2_elo_before = elo2_before;
      created_match.player1_elo_after = elo1_after;
      created_match.player2_elo_after = elo2_after;
      created_match.idempotency_key = idempotency_key;
      created_match.created_by_telegram_user_id = message->from ? message->from->id : 0;
      created_match.is_undone = false;
      
      // Parse created_at timestamp
      auto created_at_str = match_result[0]["created_at"].as<std::string>();
      std::tm tm = {};
      std::istringstream ss(created_at_str);
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (ss.fail()) {
        ss.clear();
        ss.str(created_at_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
      }
      if (!ss.fail()) {
        created_match.created_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));
      } else {
        created_match.created_at = std::chrono::system_clock::now();
      }
      
      // 7. Insert elo_history records (2 rows)
      work.exec_params(
        "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
        "elo_after, elo_change, created_at, is_undone) "
        "VALUES ($1, $2, $3, $4, $5, $6, NOW(), FALSE)",
        created_match.id, group.id, player1.id, elo1_before, elo1_after, elo1_change
      );
      
      work.exec_params(
        "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
        "elo_after, elo_change, created_at, is_undone) "
        "VALUES ($1, $2, $3, $4, $5, $6, NOW(), FALSE)",
        created_match.id, group.id, player2.id, elo2_before, elo2_after, elo2_change
      );
      
      // Commit transaction
      txn.commit();
    }, retry_config);
    
    // Send success message
    std::string player1_username = "player1";
    std::string player2_username = "player2";
    
    // Try to get usernames from message entities or database
    if (message->from) {
      player1_username = message->from->username.empty() ? 
          ("player" + std::to_string(parsed.player1_user_id)) : message->from->username;
    }
    
    // Get player2 username from database if available
    auto p2 = player_repo_->getByTelegramId(parsed.player2_user_id);
    if (p2) {
      // We don't store username in database, so use user ID
      player2_username = "player" + std::to_string(parsed.player2_user_id);
    }
    
    std::ostringstream response;
    response << "Match registered: @" << player1_username << " (" << parsed.score1 
             << ") vs @" << player2_username << " (" << parsed.score2 << ")\n";
    response << "ELO: @" << player1_username;
    auto topic_id = getTopicId(message);
    if (elo1_change >= 0) response << " +";
    response << elo1_change << ", @" << player2_username;
    if (elo2_change >= 0) response << " +";
    response << elo2_change;
    
    sendMessage(message->chat->id, response.str(), message->messageId, topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling match command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to register match");
  }
}

void Bot::handleRanking(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id,
                  "Ranking command:\n"
                  "/ranking or /rank\n\n"
                  "Shows current ELO rankings for this group.",
                  message->messageId, topic_id);
      return;
    }
    
    // Validate topic (optional - ranking can be in ranking topic or anywhere)
    // if (!isCommandInCorrectTopic(message, "ranking")) {
    //   sendErrorMessage(message, "Ranking commands must be used in the ranking topic");
    //   return;
    // }
    
    // Get group
    auto group = getOrCreateGroup(message->chat->id);
    
    // Get rankings
    auto rankings = group_repo_->getRankings(group.id, 10);
    
    if (rankings.empty()) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id, "No rankings available yet.", message->messageId, topic_id);
      return;
    }
    
    // Format rankings
    std::ostringstream response;
    response << "Current Rankings:\n";
    int rank = 1;
    for (const auto& gp : rankings) {
      // TODO: Get player username from database
      response << rank << ". Player " << gp.player_id 
               << " - " << gp.current_elo << " ELO\n";
      rank++;
    }
    
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, response.str(), message->messageId, topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling ranking command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to get rankings");
  }
}

void Bot::handleId(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id,
                  "ID command:\n"
                  "/id <school_nickname>\n\n"
                  "Verify your School21 nickname. This command must be used in the ID topic.",
                  message->messageId, topic_id);
      return;
    }
    
    // Validate topic
    if (!isCommandInCorrectTopic(message, "id")) {
      sendErrorMessage(message, "ID commands must be used in the ID topic");
      return;
    }
    
    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }
    
    // Extract nickname
    size_t space_pos = message->text.find(' ');
    if (space_pos == std::string::npos || space_pos + 1 >= message->text.length()) {
      sendErrorMessage(message, "Please provide your School21 nickname: /id <nickname>");
      return;
    }
    
    std::string nickname = message->text.substr(space_pos + 1);
    // Trim whitespace
    nickname.erase(0, nickname.find_first_not_of(" \t"));
    nickname.erase(nickname.find_last_not_of(" \t") + 1);
    
    if (nickname.empty()) {
      sendErrorMessage(message, "Nickname cannot be empty");
      return;
    }
    
    // Add loading emoji
    reactToMessage(message->chat->id, message->messageId, "⏳");
    
    // Verify via School21 API
    if (!school21_client_) {
      reactToMessage(message->chat->id, message->messageId, "👎");
      sendErrorMessage(message, "School21 API not configured");
      return;
    }
    
    auto participant = school21_client_->getParticipant(nickname);
    
    if (!participant) {
      reactToMessage(message->chat->id, message->messageId, "👎");
      sendErrorMessage(message, "Nickname not found in School21 system");
      return;
    }
    
    // Update player
    auto player = getOrCreatePlayer(message->from->id);
    player.school_nickname = nickname;
    player.is_verified_student = (participant->status == "ACTIVE");
    player_repo_->update(player);
    
    // Add success emoji and remove loading
    reactToMessage(message->chat->id, message->messageId, "👍");
    
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, 
                "Nickname verified: " + nickname + 
                (player.is_verified_student ? " (Active student)" : " (Non-active)"),
                message->messageId, topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling ID command: " + std::string(e.what()));
    reactToMessage(message->chat->id, message->messageId, "👎");
    sendErrorMessage(message, "Failed to verify nickname");
  }
}

void Bot::handleIdGuest(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id,
                  "ID Guest command:\n"
                  "/id_guest\n\n"
                  "Register as a guest player (no School21 verification required).\n"
                  "This command must be used in the ID topic.",
                  message->messageId, topic_id);
      return;
    }
    
    // Validate topic
    if (!isCommandInCorrectTopic(message, "id")) {
      sendErrorMessage(message, "ID guest commands must be used in the ID topic");
      return;
    }
    
    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }
    
    // Update player as guest
    auto player = getOrCreatePlayer(message->from->id);
    player.is_allowed_non_student = true;
    player.is_verified_student = false;
    player.school_nickname = std::nullopt;
    player_repo_->update(player);
    
    // Add success emoji
    reactToMessage(message->chat->id, message->messageId, "👍");
    
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, 
                "Registered as guest player. You can now participate in matches.",
                message->messageId, topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling ID guest command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to register as guest");
  }
}

void Bot::handleUndo(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id,
                  "Undo command:\n"
                  "/undo\n"
                  "or reply to a match message with /undo\n\n"
                  "Undo the last match or a specific match (if replying).\n"
                  "Only match players and admins can undo matches.\n"
                  "Matches can only be undone within 24 hours (admins can undo any match).",
                  message->messageId, topic_id);
      return;
    }
    
    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }
    
    int64_t user_id = message->from->id;
    
    // Check if replying to a message (undo specific match)
    if (message->replyToMessage && message->replyToMessage->messageId) {
      // TODO: Extract match_id from replied message or find match by message_id
      // For now, we'll undo the last match
    }
    
    // Get last match for the group
    auto group = getOrCreateGroup(message->chat->id);
    auto matches = match_repo_->getByGroupId(group.id, 1, 0);
    
    if (matches.empty()) {
      sendErrorMessage(message, "No matches found to undo");
      return;
    }
    
    auto match = matches[0];
    
    // Check if match is undoable
    if (!isMatchUndoable(match, user_id)) {
      sendErrorMessage(message, "You don't have permission to undo this match, or the time limit has passed");
      return;
    }
    
    // Undo match
    undoMatchTransaction(match.id, user_id);
    
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, 
                "Match #" + std::to_string(match.id) + " undone. ELO restored.",
                message->messageId, topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling undo command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to undo match");
  }
}

void Bot::handleConfigTopic(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    // Check for help
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id,
                  "Config Topic command:\n"
                  "/config_topic <topic_type>\n\n"
                  "Configure the current topic. Only group admins can use this command.\n\n"
                  "Topic types:\n"
                  "- id: School nickname registration\n"
                  "- ranking: Ranking display\n"
                  "- matches: Match registration\n"
                  "- logs: Logs that users must know about",
                  message->messageId, topic_id);
      return;
    }
    
    // Check admin permission
    if (!isAdmin(message)) {
      sendErrorMessage(message, "Only group admins can configure topics");
      return;
    }
    
    // Extract topic type
    size_t space_pos = message->text.find(' ');
    if (space_pos == std::string::npos || space_pos + 1 >= message->text.length()) {
      sendErrorMessage(message, "Please provide topic type: /config_topic <topic_type>");
      return;
    }
    
    std::string topic_type = message->text.substr(space_pos + 1);
    topic_type.erase(0, topic_type.find_first_not_of(" \t"));
    topic_type.erase(topic_type.find_last_not_of(" \t") + 1);
    
    if (topic_type != "id" && topic_type != "ranking" && 
        topic_type != "matches" && topic_type != "logs") {
      sendErrorMessage(message, "Invalid topic type. Use: id, ranking, matches, or logs");
      return;
    }
    
    // Get topic ID
    auto topic_id = getTopicId(message);
    
    // Configure topic
    models::GroupTopic topic;
    topic.group_id = getOrCreateGroup(message->chat->id).id;
    topic.telegram_topic_id = topic_id;
    topic.topic_type = topic_type;
    topic.is_active = true;
    topic.created_at = std::chrono::system_clock::now();
    
    group_repo_->configureTopic(topic);
    
    auto reply_topic_id = getTopicId(message);
    sendMessage(message->chat->id, 
                "Topic configured: " + topic_type + 
                (topic_id ? " (topic ID: " + std::to_string(*topic_id) + ")" : " (no topic ID)"),
                message->messageId, reply_topic_id);
    
  } catch (const std::exception& e) {
    logger_->error("Error handling config topic command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to configure topic");
  }
}

void Bot::handleHelp(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  handleStart(message);  // Reuse start command for help
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string Bot::extractCommandArgs(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (message->text.empty()) return "";
  
  size_t space_pos = message->text.find(' ');
  if (space_pos == std::string::npos) return "";
  
  return message->text.substr(space_pos + 1);
}

Bot::ParsedMatchCommand Bot::parseMatchCommand(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  ParsedMatchCommand result;
  result.valid = false;
  
  if (message->text.empty()) return result;
  
  // Regex: /match @player1 @player2 <score1> <score2>
  std::regex match_regex(R"(^/match\s+@(\w+)\s+@(\w+)\s+(\d+)\s+(\d+)$)");
  std::smatch matches;
  
  if (!std::regex_match(message->text, matches, match_regex)) {
    return result;
  }
  
  if (matches.size() != 5) return result;
  
  // Extract user IDs from mentions
  std::vector<int64_t> mentioned_ids = extractMentionedUserIds(message);
  if (mentioned_ids.size() < 2) {
    return result;
  }
  
  result.player1_user_id = mentioned_ids[0];
  result.player2_user_id = mentioned_ids[1];
  result.score1 = std::stoi(matches[3].str());
  result.score2 = std::stoi(matches[4].str());
  result.valid = true;
  
  return result;
}

std::vector<int64_t> Bot::extractMentionedUserIds(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  std::vector<int64_t> user_ids;
  
  if (message->entities.empty()) return user_ids;
  
  for (const auto& entity : message->entities) {
    if (!entity) continue;
    
    // Check if user is present (text_mention has user)
    if (entity->user) {
      int64_t user_id = entity->user->id;
      user_ids.push_back(user_id);
      
      // Cache username if available
      if (!entity->user->username.empty()) {
        std::lock_guard<std::mutex> lock(username_cache_mutex_);
        username_cache_[entity->user->username] = user_id;
      }
    } else {
      // Handle username mentions (@username)
      // Extract username from text and look it up
      if (entity->offset >= 0 && entity->length > 0 && 
          entity->offset + entity->length <= static_cast<int>(message->text.length())) {
        std::string mention_text = message->text.substr(entity->offset, entity->length);
        // Remove @ if present
        if (!mention_text.empty() && mention_text[0] == '@') {
          std::string username = mention_text.substr(1);
          auto user_id = lookupUserIdByUsername(username, message->chat->id);
          if (user_id) {
            user_ids.push_back(*user_id);
          } else {
            logger_->warn("Could not resolve username mention: @" + username + 
                         " (user should use text mention or be in chat)");
          }
        }
      }
    }
  }
  
  return user_ids;
}

std::optional<int64_t> Bot::extractUserIdFromMention(const std::string& mention, 
                                                      const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (mention.empty() || mention[0] != '@') {
    return std::nullopt;
  }
  
  std::string username = mention.substr(1);
  if (message && message->chat) {
    return lookupUserIdByUsername(username, message->chat->id);
  }
  
  return std::nullopt;
}

std::optional<int64_t> Bot::lookupUserIdByUsername(const std::string& username, int64_t /* chat_id */) {
  try {
    // First, check cache
    {
      std::lock_guard<std::mutex> lock(username_cache_mutex_);
      auto it = username_cache_.find(username);
      if (it != username_cache_.end()) {
        return it->second;
      }
    }
    
    // Try to get from database (if we stored it)
    // Note: We don't currently store username in players table
    
    // Try Telegram API - use getChatMember if we can
    // Note: Telegram API doesn't have direct "get user by username" method
    // We would need searchChatMembers which might not be available in tgbotxx
    
    // For now, return nullopt - username will be cached when we see it in message entities
    logger_->debug("Username not found in cache: @" + username);
    return std::nullopt;
  } catch (const std::exception& e) {
    logger_->error("Error looking up username: " + std::string(e.what()));
    return std::nullopt;
  }
}

bool Bot::areTopicsEnabled() {
  auto& config = config::Config::getInstance();
  return config.getBool("telegram.topics.enabled", true);
}

bool Bot::isCommandInCorrectTopic(const tgbotxx::Ptr<tgbotxx::Message>& message, 
                                  const std::string& topic_type) {
  // If topics are disabled, always allow
  if (!areTopicsEnabled()) {
    return true;
  }
  
  if (!group_repo_) return true;  // If no repo, allow (backward compatibility)
  
  auto topic_id = getTopicId(message);
  auto group = getOrCreateGroup(message->chat->id);
  
  auto topic = group_repo_->getTopic(group.id, topic_id.value_or(0), topic_type);
  
  // If topic is configured, command must be in that topic
  // If not configured, allow anywhere (backward compatibility)
  if (topic) {
    return topic->is_active && topic->telegram_topic_id == topic_id;
  }
  
  // No topic configured - allow (backward compatibility)
  return true;
}

std::optional<int> Bot::getTopicId(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (message->messageThreadId) {
    return message->messageThreadId;
  }
  return std::nullopt;
}

bool Bot::isAdmin(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (!message->chat || !message->from) return false;
  return isGroupAdmin(message->chat->id, message->from->id);
}

bool Bot::isGroupAdmin(int64_t /* chat_id */, int64_t /* user_id */) {
  try {
    // TODO: Use Telegram API to check if user is admin
    // For now, return false (can be implemented later)
    return false;
  } catch (const std::exception& e) {
    logger_->error("Error checking admin status: " + std::string(e.what()));
    return false;
  }
}

bool Bot::canUndoMatch(int64_t /* match_id */, int64_t user_id, const models::Match& match) {
  // Check if user is one of the players
  auto player = player_repo_->getByTelegramId(user_id);
  if (player && (player->id == match.player1_id || player->id == match.player2_id)) {
    return true;
  }
  
  // Check if user is admin
  // TODO: Get chat_id from match and check admin status
  // For now, return false
  return false;
}

void Bot::sendMessage(int64_t chat_id, const std::string& text, 
                     std::optional<int> reply_to_message_id,
                     std::optional<int> message_thread_id) {
  try {
    // Use tgbotxx API to send message
    // Bot inherits from tgbotxx::Bot, so we can call parent's sendMessage
    logger_->info("Sending message to chat_id=" + std::to_string(chat_id) + 
                  ", text length=" + std::to_string(text.length()));
    
    tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params = nullptr;
    if (reply_to_message_id && reply_to_message_id.value() > 0) {
      reply_params = tgbotxx::Ptr<tgbotxx::ReplyParameters>(new tgbotxx::ReplyParameters());
      reply_params->messageId = reply_to_message_id.value();
      // Note: messageThreadId is set via the sendMessage parameter, not in ReplyParameters
    }
    
    // Use message_thread_id if provided, otherwise 0 (main chat)
    int thread_id = message_thread_id.value_or(0);
    
    auto& api_ref = *api();
    auto sent_message = api_ref.sendMessage(
        chat_id,
        text,
        thread_id,  // messageThreadId
        "",  // parseMode
        std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>(),  // entities
        false,  // disableNotification
        false,  // protectContent
        nullptr,  // replyMarkup
        "",  // businessConnectionId
        0,  // directMessagesTopicId
        nullptr,  // linkPreviewOptions
        false,  // allowPaidBroadcast
        "",  // messageEffectId
        nullptr,  // suggestedPostParameters
        reply_params  // replyParameters
    );
    
    if (sent_message) {
      logger_->info("Message sent successfully, message_id=" + 
                    std::to_string(sent_message->messageId));
    } else {
      logger_->warn("Message sent but returned null");
    }
  } catch (const std::exception& e) {
    logger_->error("Error sending message: " + std::string(e.what()));
    // Don't throw - log and continue
  }
}

void Bot::sendErrorMessage(const tgbotxx::Ptr<tgbotxx::Message>& message, 
                           const std::string& error) {
  if (!message) return;
  auto topic_id = getTopicId(message);
  sendMessage(message->chat->id, "❌ " + error, message->messageId, topic_id);
}

void Bot::sendToLogsTopic(int64_t chat_id, const std::string& text) {
  if (!areTopicsEnabled() || !group_repo_) {
    // If topics disabled or no repo, send to main chat
    sendMessage(chat_id, text);
    return;
  }
  
  try {
    auto group = getOrCreateGroup(chat_id);
    auto logs_topic = group_repo_->getTopic(group.id, 0, "logs");
    
    if (logs_topic && logs_topic->is_active) {
      // Send to logs topic
      sendMessage(chat_id, text, std::nullopt, logs_topic->telegram_topic_id);
    } else {
      // No logs topic configured, send to main chat
      sendMessage(chat_id, text);
    }
  } catch (const std::exception& e) {
    logger_->error("Error sending to logs topic: " + std::string(e.what()));
    // Fallback to main chat
    sendMessage(chat_id, text);
  }
}

void Bot::reactToMessage(int64_t chat_id, int message_id, const std::string& emoji) {
  try {
    // Use tgbotxx API to react to message
    logger_->info("Reacting to message_id=" + std::to_string(message_id) + 
                  " with emoji: " + emoji);
    
    // Create ReactionTypeEmoji - it's defined in ReactionType.hpp
    auto reaction_type = tgbotxx::Ptr<tgbotxx::ReactionTypeEmoji>(
        new tgbotxx::ReactionTypeEmoji());
    reaction_type->emoji = emoji;
    
    std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>> reactions;
    reactions.push_back(tgbotxx::Ptr<tgbotxx::ReactionType>(reaction_type));
    
    // Call setMessageReaction through the API
    bool success = (*api()).setMessageReaction(
        chat_id,
        message_id,
        reactions,
        false  // isBig
    );
    
    if (success) {
      logger_->info("Reaction set successfully");
    } else {
      logger_->warn("Reaction set returned false");
    }
  } catch (const std::exception& e) {
    logger_->error("Error reacting to message: " + std::string(e.what()));
  }
}

models::Group Bot::getOrCreateGroup(int64_t telegram_group_id, const std::string& name) {
  if (!group_repo_) {
    throw std::runtime_error("GroupRepository not initialized");
  }
  
  auto group = group_repo_->getByTelegramId(telegram_group_id);
  if (group) {
    return *group;
  }
  
  return group_repo_->createOrGet(telegram_group_id, name);
}

models::Player Bot::getOrCreatePlayer(int64_t telegram_user_id) {
  if (!player_repo_) {
    throw std::runtime_error("PlayerRepository not initialized");
  }
  
  auto player = player_repo_->getByTelegramId(telegram_user_id);
  if (player) {
    return *player;
  }
  
  return player_repo_->createOrGet(telegram_user_id);
}

models::GroupPlayer Bot::getOrCreateGroupPlayer(int64_t group_id, int64_t player_id) {
  if (!group_repo_) {
    throw std::runtime_error("GroupRepository not initialized");
  }
  
  return group_repo_->getOrCreateGroupPlayer(group_id, player_id);
}

std::string Bot::generateIdempotencyKey(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // Use message_id as idempotency key
  return std::to_string(message->chat->id) + "_" + std::to_string(message->messageId);
}

bool Bot::isDuplicateMatch(const std::string& idempotency_key) {
  if (!match_repo_) return false;
  
  auto match = match_repo_->getByIdempotencyKey(idempotency_key);
  return match.has_value();
}

void Bot::updateEloAfterMatch(int64_t group_id, int64_t player1_id, int64_t player2_id,
                             int elo1_before, int elo2_before,
                             int elo1_after, int elo2_after,
                             int64_t match_id) {
  if (!match_repo_) return;
  
  // Create ELO history for player1
  models::EloHistory history1;
  history1.match_id = match_id;
  history1.group_id = group_id;
  history1.player_id = player1_id;
  history1.elo_before = elo1_before;
  history1.elo_after = elo1_after;
  history1.elo_change = elo1_after - elo1_before;
  history1.created_at = std::chrono::system_clock::now();
  history1.is_undone = false;
  match_repo_->createEloHistory(history1);
  
  // Create ELO history for player2
  models::EloHistory history2;
  history2.match_id = match_id;
  history2.group_id = group_id;
  history2.player_id = player2_id;
  history2.elo_before = elo2_before;
  history2.elo_after = elo2_after;
  history2.elo_change = elo2_after - elo2_before;
  history2.created_at = std::chrono::system_clock::now();
  history2.is_undone = false;
  match_repo_->createEloHistory(history2);
}

bool Bot::isMatchUndoable(const models::Match& match, int64_t user_id) {
  // Check if already undone
  if (match.is_undone) {
    return false;
  }
  
  // Check permission
  if (!canUndoMatch(match.id, user_id, match)) {
    return false;
  }
  
  // Check time limit (24 hours, configurable)
  auto now = std::chrono::system_clock::now();
  auto match_time = match.created_at;
  auto hours_since_match = std::chrono::duration_cast<std::chrono::hours>(now - match_time).count();
  
  // TODO: Check if user is admin (admins can undo any match)
  // For now, apply 24-hour limit to everyone
  if (hours_since_match > 24) {
    return false;
  }
  
  return true;
}

void Bot::undoMatchTransaction(int64_t match_id, int64_t undone_by_user_id) {
  if (!match_repo_ || !group_repo_ || !db_pool_) {
    throw std::runtime_error("Repositories or connection pool not initialized");
  }
  
  // Start transaction
  database::Transaction txn(db_pool_);
  auto& work = txn.get();
  
  // 1. Get match
  auto match_result = work.exec_params(
    "SELECT id, group_id, player1_id, player2_id, player1_elo_before, player2_elo_before, "
    "player1_elo_after, player2_elo_after, is_undone "
    "FROM matches WHERE id = $1 FOR UPDATE",
    match_id
  );
  
  if (match_result.empty()) {
    throw std::runtime_error("Match not found");
  }
  
  if (match_result[0]["is_undone"].as<bool>()) {
    throw std::runtime_error("Match is already undone");
  }
  
  int64_t group_id = match_result[0]["group_id"].as<int64_t>();
  int64_t player1_id = match_result[0]["player1_id"].as<int64_t>();
  int64_t player2_id = match_result[0]["player2_id"].as<int64_t>();
  int elo1_before = match_result[0]["player1_elo_before"].as<int>();
  int elo2_before = match_result[0]["player2_elo_before"].as<int>();
  int elo1_after = match_result[0]["player1_elo_after"].as<int>();
  int elo2_after = match_result[0]["player2_elo_after"].as<int>();
  
  // 2. Get current group player states (with FOR UPDATE for consistency)
  auto gp1_result = work.exec_params(
    "SELECT id, current_elo, matches_played, matches_won, matches_lost, version "
    "FROM group_players "
    "WHERE group_id = $1 AND player_id = $2 FOR UPDATE",
    group_id, player1_id
  );
  
  if (gp1_result.empty()) {
    throw std::runtime_error("Group player 1 not found");
  }
  
  auto gp2_result = work.exec_params(
    "SELECT id, current_elo, matches_played, matches_won, matches_lost, version "
    "FROM group_players "
    "WHERE group_id = $1 AND player_id = $2 FOR UPDATE",
    group_id, player2_id
  );
  
  if (gp2_result.empty()) {
    throw std::runtime_error("Group player 2 not found");
  }
  
  int64_t gp1_id = gp1_result[0]["id"].as<int64_t>();
  int gp1_current_elo = gp1_result[0]["current_elo"].as<int>();
  int gp1_matches_played = gp1_result[0]["matches_played"].as<int>();
  int gp1_matches_won = gp1_result[0]["matches_won"].as<int>();
  int gp1_matches_lost = gp1_result[0]["matches_lost"].as<int>();
  int gp1_version = gp1_result[0]["version"].as<int>();
  
  int64_t gp2_id = gp2_result[0]["id"].as<int64_t>();
  int gp2_current_elo = gp2_result[0]["current_elo"].as<int>();
  int gp2_matches_played = gp2_result[0]["matches_played"].as<int>();
  int gp2_matches_won = gp2_result[0]["matches_won"].as<int>();
  int gp2_matches_lost = gp2_result[0]["matches_lost"].as<int>();
  int gp2_version = gp2_result[0]["version"].as<int>();
  
  // Calculate reversed ELO (subtract the change)
  int elo1_reversed = gp1_current_elo - (elo1_after - elo1_before);
  int elo2_reversed = gp2_current_elo - (elo2_after - elo2_before);
  
  // Determine match result to reverse statistics
  int score1 = 0, score2 = 0;
  auto score_result = work.exec_params(
    "SELECT player1_score, player2_score FROM matches WHERE id = $1",
    match_id
  );
  if (!score_result.empty()) {
    score1 = score_result[0]["player1_score"].as<int>();
    score2 = score_result[0]["player2_score"].as<int>();
  }
  
  // 3. Reverse ELO changes and update statistics with optimistic locking
  int gp1_new_matches_played = std::max(0, gp1_matches_played - 1);
  int gp1_new_matches_won = gp1_matches_won;
  int gp1_new_matches_lost = gp1_matches_lost;
  if (score1 > score2) {
    gp1_new_matches_won = std::max(0, gp1_matches_won - 1);
  } else if (score1 < score2) {
    gp1_new_matches_lost = std::max(0, gp1_matches_lost - 1);
  }
  
  auto update1_result = work.exec_params(
    "UPDATE group_players SET "
    "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
    "version = version + 1, updated_at = NOW() "
    "WHERE id = $5 AND version = $6",
    elo1_reversed, gp1_new_matches_played, gp1_new_matches_won, gp1_new_matches_lost,
    gp1_id, gp1_version
  );
  
  if (update1_result.affected_rows() == 0) {
    throw utils::OptimisticLockException("Optimistic lock conflict for player 1 during undo");
  }
  
  int gp2_new_matches_played = std::max(0, gp2_matches_played - 1);
  int gp2_new_matches_won = gp2_matches_won;
  int gp2_new_matches_lost = gp2_matches_lost;
  if (score2 > score1) {
    gp2_new_matches_won = std::max(0, gp2_matches_won - 1);
  } else if (score2 < score1) {
    gp2_new_matches_lost = std::max(0, gp2_matches_lost - 1);
  }
  
  auto update2_result = work.exec_params(
    "UPDATE group_players SET "
    "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
    "version = version + 1, updated_at = NOW() "
    "WHERE id = $5 AND version = $6",
    elo2_reversed, gp2_new_matches_played, gp2_new_matches_won, gp2_new_matches_lost,
    gp2_id, gp2_version
  );
  
  if (update2_result.affected_rows() == 0) {
    throw utils::OptimisticLockException("Optimistic lock conflict for player 2 during undo");
  }
  
  // 4. Mark match as undone
  work.exec_params(
    "UPDATE matches SET "
    "is_undone = TRUE, undone_at = NOW(), undone_by_telegram_user_id = $1 "
    "WHERE id = $2",
    undone_by_user_id, match_id
  );
  
  // 5. Create reverse ELO history entries (mark as undone)
  int elo1_change = elo1_before - elo1_after;  // Reverse change
  int elo2_change = elo2_before - elo2_after;  // Reverse change
  
  work.exec_params(
    "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
    "elo_after, elo_change, created_at, is_undone) "
    "VALUES ($1, $2, $3, $4, $5, $6, NOW(), TRUE)",
    match_id, group_id, player1_id, elo1_after, elo1_before, elo1_change
  );
  
  work.exec_params(
    "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, "
    "elo_after, elo_change, created_at, is_undone) "
    "VALUES ($1, $2, $3, $4, $5, $6, NOW(), TRUE)",
    match_id, group_id, player2_id, elo2_after, elo2_before, elo2_change
  );
  
  // Commit transaction
  txn.commit();
}

}  // namespace bot

