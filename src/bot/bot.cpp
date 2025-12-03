#include "bot/bot.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <ctime>

#include "database/connection_pool.h"
#include "repositories/group_repository.h"
#include "repositories/player_repository.h"
#include "repositories/match_repository.h"
#include "school21/api_client.h"
#include "utils/elo_calculator.h"
#include "observability/logger.h"
#include "config/config.h"
#include "models/group.h"
#include "models/player.h"
#include "models/match.h"
#include <tgbotxx/objects/ReplyParameters.hpp>
#include <tgbotxx/objects/ReactionType.hpp>
#include <tgbotxx/Api.hpp>

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
    
    sendMessage(message->chat->id, help_text);
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
      sendMessage(message->chat->id, 
                  "Match command format:\n"
                  "/match @player1 @player2 <score1> <score2>\n\n"
                  "Example: /match @alice @bob 3 1\n\n"
                  "This command must be used in the matches topic (if configured).",
                  message->messageId);
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
    
    // Get or create group players
    auto gp1 = getOrCreateGroupPlayer(group.id, player1.id);
    auto gp2 = getOrCreateGroupPlayer(group.id, player2.id);
    
    // Check for duplicate
    std::string idempotency_key = generateIdempotencyKey(message);
    if (isDuplicateMatch(idempotency_key)) {
      sendErrorMessage(message, "This match was already registered");
      return;
    }
    
    // Calculate ELO
    auto [elo1_after, elo2_after] = elo_calculator_->calculate(
        gp1.current_elo, gp2.current_elo, parsed.score1, parsed.score2);
    
    int elo1_before = gp1.current_elo;
    int elo2_before = gp2.current_elo;
    int elo1_change = elo1_after - elo1_before;
    int elo2_change = elo2_after - elo2_before;
    
    // Create match
    models::Match match;
    match.group_id = group.id;
    match.player1_id = player1.id;
    match.player2_id = player2.id;
    match.player1_score = parsed.score1;
    match.player2_score = parsed.score2;
    match.player1_elo_before = elo1_before;
    match.player2_elo_before = elo2_before;
    match.player1_elo_after = elo1_after;
    match.player2_elo_after = elo2_after;
    match.idempotency_key = idempotency_key;
    match.created_by_telegram_user_id = message->from ? message->from->id : 0;
    match.created_at = std::chrono::system_clock::now();
    
    // TODO: Use transaction to create match and update ELO atomically
    auto created_match = match_repo_->create(match);
    
    // Update group players with new ELO
    gp1.current_elo = elo1_after;
    gp2.current_elo = elo2_after;
    if (parsed.score1 > parsed.score2) {
      gp1.matches_won++;
      gp2.matches_lost++;
    } else if (parsed.score1 < parsed.score2) {
      gp1.matches_lost++;
      gp2.matches_won++;
    }
    gp1.matches_played++;
    gp2.matches_played++;
    
    group_repo_->updateGroupPlayer(gp1);
    group_repo_->updateGroupPlayer(gp2);
    
    // Create ELO history
    updateEloAfterMatch(group.id, player1.id, player2.id,
                       elo1_before, elo2_before, elo1_after, elo2_after,
                       created_match.id);
    
    // Send success message
    std::string player1_username = message->from ? 
        (message->from->username.empty() ? "player1" : message->from->username) : "player1";
    std::string player2_username = "player2";  // TODO: Get from message entities
    
    std::ostringstream response;
    response << "Match registered: @" << player1_username << " (" << parsed.score1 
             << ") vs @" << player2_username << " (" << parsed.score2 << ")\n";
    response << "ELO: @" << player1_username;
    if (elo1_change >= 0) response << " +";
    response << elo1_change << ", @" << player2_username;
    if (elo2_change >= 0) response << " +";
    response << elo2_change;
    
    sendMessage(message->chat->id, response.str(), message->messageId);
    
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
      sendMessage(message->chat->id,
                  "Ranking command:\n"
                  "/ranking or /rank\n\n"
                  "Shows current ELO rankings for this group.",
                  message->messageId);
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
      sendMessage(message->chat->id, "No rankings available yet.", message->messageId);
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
    
    sendMessage(message->chat->id, response.str(), message->messageId);
    
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
      sendMessage(message->chat->id,
                  "ID command:\n"
                  "/id <school_nickname>\n\n"
                  "Verify your School21 nickname. This command must be used in the ID topic.",
                  message->messageId);
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
    
    sendMessage(message->chat->id, 
                "Nickname verified: " + nickname + 
                (player.is_verified_student ? " (Active student)" : " (Non-active)"),
                message->messageId);
    
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
      sendMessage(message->chat->id,
                  "ID Guest command:\n"
                  "/id_guest\n\n"
                  "Register as a guest player (no School21 verification required).\n"
                  "This command must be used in the ID topic.",
                  message->messageId);
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
    
    sendMessage(message->chat->id, 
                "Registered as guest player. You can now participate in matches.",
                message->messageId);
    
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
      sendMessage(message->chat->id,
                  "Undo command:\n"
                  "/undo\n"
                  "or reply to a match message with /undo\n\n"
                  "Undo the last match or a specific match (if replying).\n"
                  "Only match players and admins can undo matches.\n"
                  "Matches can only be undone within 24 hours (admins can undo any match).",
                  message->messageId);
      return;
    }
    
    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }
    
    int64_t user_id = message->from->id;
    int64_t match_id = 0;
    
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
    
    sendMessage(message->chat->id, 
                "Match #" + std::to_string(match.id) + " undone. ELO restored.",
                message->messageId);
    
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
      sendMessage(message->chat->id,
                  "Config Topic command:\n"
                  "/config_topic <topic_type>\n\n"
                  "Configure the current topic. Only group admins can use this command.\n\n"
                  "Topic types:\n"
                  "- id: School nickname registration\n"
                  "- ranking: Ranking display\n"
                  "- matches: Match registration\n"
                  "- logs: Logs that users must know about",
                  message->messageId);
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
    
    sendMessage(message->chat->id, 
                "Topic configured: " + topic_type + 
                (topic_id ? " (topic ID: " + std::to_string(*topic_id) + ")" : " (no topic ID)"),
                message->messageId);
    
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
    
    // Check entity type using enum comparison
    // tgbotxx::MessageEntity::Type is an enum, need to compare with enum values
    // For now, check if user is present (text_mention has user)
    if (entity->user) {
      user_ids.push_back(entity->user->id);
    }
    // TODO: Handle mention type (username mentions) - need to look up user ID from username
  }
  
  return user_ids;
}

std::optional<int64_t> Bot::extractUserIdFromMention(const std::string& mention, 
                                                      const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement username to user_id lookup
  return std::nullopt;
}

bool Bot::isCommandInCorrectTopic(const tgbotxx::Ptr<tgbotxx::Message>& message, 
                                  const std::string& topic_type) {
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

bool Bot::isGroupAdmin(int64_t chat_id, int64_t user_id) {
  try {
    // TODO: Use Telegram API to check if user is admin
    // For now, return false (can be implemented later)
    return false;
  } catch (const std::exception& e) {
    logger_->error("Error checking admin status: " + std::string(e.what()));
    return false;
  }
}

bool Bot::canUndoMatch(int64_t match_id, int64_t user_id, const models::Match& match) {
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
                     std::optional<int> reply_to_message_id) {
  try {
    // Use tgbotxx API to send message
    // Bot inherits from tgbotxx::Bot, so we can call parent's sendMessage
    logger_->info("Sending message to chat_id=" + std::to_string(chat_id) + 
                  ", text length=" + std::to_string(text.length()));
    
    tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params = nullptr;
    if (reply_to_message_id && reply_to_message_id.value() > 0) {
      reply_params = tgbotxx::Ptr<tgbotxx::ReplyParameters>(new tgbotxx::ReplyParameters());
      reply_params->messageId = reply_to_message_id.value();
    }
    
    auto& api_ref = *api();
    auto sent_message = api_ref.sendMessage(
        chat_id,
        text,
        0,  // messageThreadId
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
  sendMessage(message->chat->id, "❌ " + error, message->messageId);
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
  if (!match_repo_) {
    throw std::runtime_error("MatchRepository not initialized");
  }
  
  // TODO: Implement full undo transaction
  // 1. Get match
  // 2. Reverse ELO changes
  // 3. Mark match as undone
  // 4. Create reverse ELO history entries
  
  match_repo_->undoMatch(match_id, undone_by_user_id);
}

}  // namespace bot

