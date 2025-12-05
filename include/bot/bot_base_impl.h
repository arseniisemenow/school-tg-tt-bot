#ifndef BOT_BOT_BASE_IMPL_H
#define BOT_BOT_BASE_IMPL_H

// Template implementation file for BotBase
// This file contains all the method implementations as templates

#include "bot/bot_base.h"
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
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace bot {

template<typename Derived>
BotBase<Derived>::~BotBase() {
  stop();
}

template<typename Derived>
void BotBase<Derived>::initialize() {
  // Initialize ELO calculator with K-factor from config
  auto& config = config::Config::getInstance();
  int k_factor = config.getInt("elo.k_factor", 32);
  elo_calculator_ = std::make_unique<utils::EloCalculator>(k_factor);
  
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->info("BotBase initialized (dependencies must be set via setDependencies)");
}

template<typename Derived>
void BotBase<Derived>::setDependencies(
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
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->info("BotBase dependencies set");
}

template<typename Derived>
void BotBase<Derived>::onCommand(const tgbotxx::Ptr<tgbotxx::Message>& command) {
  try {
    if (!command) {
      if (!logger_) logger_ = observability::Logger::getInstance().get();
      logger_->warn("onCommand called with null command");
      return;
    }
    
    if (!logger_) logger_ = observability::Logger::getInstance().get();
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
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error in onCommand: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::onChatMemberUpdated(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& chatMember) {
  try {
    if (!chatMember || !chatMember->chat || !chatMember->newChatMember) {
      return;
    }
    
    int64_t chat_id = chatMember->chat->id;
    int64_t user_id = chatMember->from ? chatMember->from->id : 0;
    std::string status = chatMember->newChatMember->status;
    
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->info("Chat member update: chat_id=" + std::to_string(chat_id) + 
                  ", user_id=" + std::to_string(user_id) + 
                  ", status=" + status);
    
    // Handle member join
    if (status == "member") {
      handleMemberJoin(chatMember);
    }
    // Handle member leave
    else if (status == "left" || status == "kicked") {
      handleMemberLeave(chatMember);
    }
  } catch (const std::exception& e) {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error handling chat member update: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    if (!message) return;
    
    // Check if this is a command - tgbotxx might not call onCommand for all commands
    // So we manually check and route commands here
    if (!message->text.empty() && message->text.front() == '/') {
      // This is a command - extract and handle it
      std::string cmd = extractCommandName(message);
      if (!cmd.empty()) {
        if (!logger_) logger_ = observability::Logger::getInstance().get();
        logger_->info("Command detected in onAnyMessage: " + cmd);
        // Route to command handler
        onCommand(message);
        return;
      }
    }
    
    // Command-only mode: ignore non-command messages
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->debug("Message received (not a command): " + 
                  (message->text.empty() ? "empty" : message->text.substr(0, 50)));
  } catch (const std::exception& e) {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error in onAnyMessage: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::startPolling() {
  if (running_) {
    return;
  }
  
  // For production bot, this will call tgbotxx::Bot::start()
  // For test bot, this is a no-op
  running_ = true;
  // Note: ProductionBotApi (which inherits from tgbotxx::Bot) will handle start()
  // TestBot will just set running_ = true
}

template<typename Derived>
void BotBase<Derived>::startWebhook(const std::string& /* webhook_url */, int /* port */) {
  // TODO: Implement webhook support
  if (!logger_) logger_ = observability::Logger::getInstance().get();
  logger_->warn("Webhook not yet implemented");
}

template<typename Derived>
void BotBase<Derived>::stop() {
  running_ = false;
  // ProductionBotApi (tgbotxx::Bot) will handle stop()
  // TestBot just sets running_ = false
}

template<typename Derived>
std::string BotBase<Derived>::extractCommandName(const tgbotxx::Ptr<tgbotxx::Message>& message) {
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

template<typename Derived>
void BotBase<Derived>::sendMessage(int64_t chat_id, const std::string& text, 
                   std::optional<int> reply_to_message_id,
                   std::optional<int> message_thread_id) {
  try {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->info("Sending message to chat_id=" + std::to_string(chat_id) + 
                  ", text length=" + std::to_string(text.length()));
    
    tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params = nullptr;
    if (reply_to_message_id && reply_to_message_id.value() > 0) {
      reply_params = tgbotxx::Ptr<tgbotxx::ReplyParameters>(new tgbotxx::ReplyParameters());
      reply_params->messageId = reply_to_message_id.value();
      // Explicitly set chat to avoid Telegram rejecting replies with chat_id=0.
      reply_params->chatId = chat_id;
    }
    
    int thread_id = message_thread_id.value_or(0);
    
    auto* api_impl = getBotApi();
    if (!api_impl) {
      logger_->error("API not available for sending message");
      return;
    }
    auto sent_message = api_impl->sendMessage(
        chat_id,
        text,
        thread_id,
        "",  // parseMode
        std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>(),  // entities
        false,  // disableNotification
        false,  // protectContent
        nullptr,  // replyMarkup
        "",       // businessConnectionId
        0,        // directMessagesTopicId
        nullptr,  // linkPreviewOptions
        false,    // allowPaidBroadcast
        "",       // messageEffectId
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
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error sending message: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::reactToMessage(int64_t chat_id, int message_id, const std::string& emoji) {
  try {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->info("Reacting to message_id=" + std::to_string(message_id) + 
                  " with emoji: " + emoji);
    
    auto reaction_type = tgbotxx::Ptr<tgbotxx::ReactionTypeEmoji>(
        new tgbotxx::ReactionTypeEmoji());
    reaction_type->emoji = emoji;
    
    std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>> reactions;
    reactions.push_back(tgbotxx::Ptr<tgbotxx::ReactionType>(reaction_type));
    
    auto* api_impl = getBotApi();
    if (!api_impl) {
      logger_->error("API not available for setting reaction");
      return;
    }
    bool success = api_impl->setMessageReaction(
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
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error reacting to message: " + std::string(e.what()));
  }
}

// Note: Most bot logic methods (handleStart, handleMatch, etc.) are still in bot.cpp
// as Bot:: methods. BotBase only provides the API abstraction via CRTP.
// When Bot inherits from BotBase<Bot>, it can use getBotApi() to access the API.
// TestBot inherits from BotBase<TestBot> and provides TestBotApi.

// Stub implementations for methods that BotBase calls but are implemented in Bot
// These will be overridden or implemented in the derived class
template<typename Derived>
void BotBase<Derived>::handleStart(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
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
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling start command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to process command");
  }
}

template<typename Derived>
void BotBase<Derived>::handleMatch(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
    std::string args = extractCommandArgs(message);
    if (args == "help" || args.find("help") == 0) {
      auto topic_id = getTopicId(message);
      sendMessage(
          message->chat->id,
          "Match command format:\n"
          "/match @player1 @player2 <score1> <score2>\n\n"
          "Example: /match @alice @bob 3 1\n\n"
          "This command must be used in the matches topic (if configured).",
          message->messageId, topic_id);
      return;
    }

    if (!isCommandInCorrectTopic(message, "matches")) {
      sendErrorMessage(message, "Match commands must be used in the matches topic");
      return;
    }

    auto parsed = parseMatchCommand(message);
    if (!parsed.valid) {
      sendErrorMessage(
          message,
          "Invalid format. Use: /match @player1 @player2 <score1> <score2>\n"
          "Example: /match @alice @bob 3 1");
      return;
    }

    std::string group_name = message->chat->title.empty() ? "" : message->chat->title;
    auto group = getOrCreateGroup(message->chat->id, group_name);

    auto player1 = getOrCreatePlayer(parsed.player1_user_id);
    auto player2 = getOrCreatePlayer(parsed.player2_user_id);

    auto gp1 = getOrCreateGroupPlayer(group.id, player1.id);
    auto gp2 = getOrCreateGroupPlayer(group.id, player2.id);

    std::string idempotency_key = generateIdempotencyKey(message);
    if (isDuplicateMatch(idempotency_key)) {
      sendErrorMessage(message, "This match was already registered");
      return;
    }

    if (!elo_calculator_) {
      auto& config = config::Config::getInstance();
      int k_factor = config.getInt("elo.k_factor", 32);
      elo_calculator_ = std::make_unique<utils::EloCalculator>(k_factor);
    }

    auto elo_pair = elo_calculator_->calculate(
        gp1.current_elo, gp2.current_elo, parsed.score1, parsed.score2);
    int elo1_after = elo_pair.first;
    int elo2_after = elo_pair.second;
    int elo1_change = elo1_after - gp1.current_elo;
    int elo2_change = elo2_after - gp2.current_elo;

    models::GroupPlayer gp1_updated = gp1;
    models::GroupPlayer gp2_updated = gp2;
    gp1_updated.current_elo = elo1_after;
    gp2_updated.current_elo = elo2_after;
    gp1_updated.matches_played += 1;
    gp2_updated.matches_played += 1;
    if (parsed.score1 > parsed.score2) {
      gp1_updated.matches_won += 1;
      gp2_updated.matches_lost += 1;
    } else if (parsed.score1 < parsed.score2) {
      gp2_updated.matches_won += 1;
      gp1_updated.matches_lost += 1;
    }

    // Use a single DB transaction to keep ELO updates and match creation atomic
    database::Transaction txn(db_pool_);
    auto& work = txn.get();

    auto update1 = work.exec_params(
      "UPDATE group_players SET "
      "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
      "version = version + 1, updated_at = NOW() "
      "WHERE id = $5",
      gp1_updated.current_elo, gp1_updated.matches_played, gp1_updated.matches_won,
      gp1_updated.matches_lost, gp1_updated.id);
    if (update1.affected_rows() == 0) {
      throw std::runtime_error("Failed to update player1 stats");
    }

    auto update2 = work.exec_params(
      "UPDATE group_players SET "
      "current_elo = $1, matches_played = $2, matches_won = $3, matches_lost = $4, "
      "version = version + 1, updated_at = NOW() "
      "WHERE id = $5",
      gp2_updated.current_elo, gp2_updated.matches_played, gp2_updated.matches_won,
      gp2_updated.matches_lost, gp2_updated.id);
    if (update2.affected_rows() == 0) {
      throw std::runtime_error("Failed to update player2 stats");
    }

    auto match_result = work.exec_params(
      "INSERT INTO matches (group_id, player1_id, player2_id, player1_score, player2_score, "
      "player1_elo_before, player2_elo_before, player1_elo_after, player2_elo_after, "
      "idempotency_key, created_by_telegram_user_id, created_at, is_undone) "
      "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW(), FALSE) "
      "RETURNING id, created_at",
      group.id, player1.id, player2.id, parsed.score1, parsed.score2,
      gp1.current_elo, gp2.current_elo, elo1_after, elo2_after,
      idempotency_key, message->from ? message->from->id : 0);

    if (match_result.empty()) {
      throw std::runtime_error("Failed to create match");
    }

    int64_t match_id = match_result[0]["id"].template as<int64_t>();

    work.exec_params(
      "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, elo_after, elo_change, created_at, is_undone) "
      "VALUES ($1, $2, $3, $4, $5, $6, NOW(), FALSE)",
      match_id, group.id, player1.id, gp1.current_elo, elo1_after, elo1_change);

    work.exec_params(
      "INSERT INTO elo_history (match_id, group_id, player_id, elo_before, elo_after, elo_change, created_at, is_undone) "
      "VALUES ($1, $2, $3, $4, $5, $6, NOW(), FALSE)",
      match_id, group.id, player2.id, gp2.current_elo, elo2_after, elo2_change);

    txn.commit();

    std::ostringstream response;
    response << "Match registered: @" << parsed.player1_user_id << " (" << parsed.score1
             << ") vs @" << parsed.player2_user_id << " (" << parsed.score2 << ")\n";
    response << "ELO: ";
    response << "P1 ";
    if (elo1_change >= 0) response << "+";
    response << elo1_change << ", P2 ";
    if (elo2_change >= 0) response << "+";
    response << elo2_change;
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, response.str(), message->messageId, topic_id);
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling match command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to register match");
  }
}

template<typename Derived>
void BotBase<Derived>::handleRanking(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
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

    auto group = getOrCreateGroup(message->chat->id);
    auto rankings = group_repo_->getRankings(group.id, 10);
    if (rankings.empty()) {
      auto topic_id = getTopicId(message);
      sendMessage(message->chat->id, "No rankings available yet.", message->messageId,
                  topic_id);
      return;
    }

    std::ostringstream response;
    response << "Current Rankings:\n";
    int rank = 1;
    for (const auto& gp : rankings) {
      response << rank << ". Player " << gp.player_id << " - " << gp.current_elo
               << " ELO\n";
      rank++;
    }
    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id, response.str(), message->messageId, topic_id);
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling ranking command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to get rankings");
  }
}

template<typename Derived>
void BotBase<Derived>::handleId(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
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

    if (!isCommandInCorrectTopic(message, "id")) {
      sendErrorMessage(message, "ID commands must be used in the ID topic");
      return;
    }

    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }

    size_t space_pos = message->text.find(' ');
    if (space_pos == std::string::npos || space_pos + 1 >= message->text.length()) {
      sendErrorMessage(message, "Please provide your School21 nickname: /id <nickname>");
      return;
    }

    std::string nickname = message->text.substr(space_pos + 1);
    auto first = nickname.find_first_not_of(" \t");
    if (first == std::string::npos) {
      nickname.clear();
    } else {
      auto last = nickname.find_last_not_of(" \t");
      nickname = nickname.substr(first, last - first + 1);
    }

    if (nickname.empty()) {
      sendErrorMessage(message, "Nickname cannot be empty");
      return;
    }

    reactToMessage(message->chat->id, message->messageId, "⏳");

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

    auto player = getOrCreatePlayer(message->from->id);
    player.school_nickname = nickname;
    player.is_verified_student = (participant->status == "ACTIVE");
    player_repo_->update(player);

    reactToMessage(message->chat->id, message->messageId, "👍");

    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id,
                "Nickname verified: " + nickname +
                    (player.is_verified_student ? " (Active student)" : " (Non-active)"),
                message->messageId, topic_id);
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling ID command: " + std::string(e.what()));
    reactToMessage(message->chat->id, message->messageId, "👎");
    sendErrorMessage(message, "Failed to verify nickname");
  }
}

template<typename Derived>
void BotBase<Derived>::handleIdGuest(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  try {
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

    if (!isCommandInCorrectTopic(message, "id")) {
      sendErrorMessage(message, "ID guest commands must be used in the ID topic");
      return;
    }

    if (!message->from) {
      sendErrorMessage(message, "Unable to identify user");
      return;
    }

    auto player = getOrCreatePlayer(message->from->id);
    player.is_allowed_non_student = true;
    player.is_verified_student = false;
    player.school_nickname = std::nullopt;
    player_repo_->update(player);

    reactToMessage(message->chat->id, message->messageId, "👍");

    auto topic_id = getTopicId(message);
    sendMessage(message->chat->id,
                "Registered as guest player. You can now participate in matches.",
                message->messageId, topic_id);
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling ID guest command: " + std::string(e.what()));
    sendErrorMessage(message, "Failed to register as guest");
  }
}

template<typename Derived>
void BotBase<Derived>::handleUndo(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->warn("handleUndo not implemented in BotBase for tests");
}

template<typename Derived>
void BotBase<Derived>::handleConfigTopic(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->warn("handleConfigTopic not implemented in BotBase for tests");
}

template<typename Derived>
void BotBase<Derived>::handleHelp(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  handleStart(message);
}

template<typename Derived>
void BotBase<Derived>::handleMemberJoin(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  try {
    if (!update || !update->from || !update->chat) {
      return;
    }
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    std::string username = update->from->username.empty()
                               ? ("User " + std::to_string(update->from->id))
                               : update->from->username;
    sendToLogsTopic(update->chat->id,
                    "👋 " + username + " joined the group. Welcome!");
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling member join: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::handleMemberLeave(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  try {
    if (!update || !update->from || !player_repo_) {
      return;
    }
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    auto player = player_repo_->getByTelegramId(update->from->id);
    if (player) {
      player_repo_->softDelete(player->id);
    }
  } catch (const std::exception& e) {
    if (!logger_) {
      logger_ = observability::Logger::getInstance().get();
    }
    logger_->error("Error handling member leave: " + std::string(e.what()));
  }
}

template<typename Derived>
void BotBase<Derived>::handleBotRemoval(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update) {
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->warn("handleBotRemoval not implemented in BotBase for tests");
}

template<typename Derived>
void BotBase<Derived>::handleGroupMigration(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (!logger_) {
    logger_ = observability::Logger::getInstance().get();
  }
  logger_->warn("handleGroupMigration not implemented in BotBase for tests");
}

template<typename Derived>
std::string BotBase<Derived>::extractCommandArgs(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (message->text.empty()) return "";
  size_t space_pos = message->text.find(' ');
  if (space_pos == std::string::npos) return "";
  return message->text.substr(space_pos + 1);
}

template<typename Derived>
typename BotBase<Derived>::ParsedMatchCommand BotBase<Derived>::parseMatchCommand(
    const tgbotxx::Ptr<tgbotxx::Message>& message) {
  ParsedMatchCommand result;
  result.valid = false;

  if (message->text.empty()) return result;

  std::regex match_regex(R"(^/match\s+@(\w+)\s+@(\w+)\s+(\d+)\s+(\d+)$)");
  std::smatch matches;

  if (!std::regex_match(message->text, matches, match_regex)) {
    return result;
  }

  if (matches.size() != 5) return result;

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

template<typename Derived>
std::vector<int64_t> BotBase<Derived>::extractMentionedUserIds(
    const tgbotxx::Ptr<tgbotxx::Message>& message) {
  std::vector<int64_t> user_ids;

  if (message->entities.empty()) return user_ids;

  for (const auto& entity : message->entities) {
    if (!entity) continue;

    if (entity->user) {
      int64_t user_id = entity->user->id;
      user_ids.push_back(user_id);

      if (!entity->user->username.empty()) {
        std::lock_guard<std::mutex> lock(username_cache_mutex_);
        username_cache_[entity->user->username] = user_id;
      }
    } else {
      if (entity->offset >= 0 && entity->length > 0 &&
          entity->offset + entity->length <= static_cast<int>(message->text.length())) {
        std::string mention_text = message->text.substr(entity->offset, entity->length);
        if (!mention_text.empty() && mention_text[0] == '@') {
          std::string username = mention_text.substr(1);
          auto user_id = lookupUserIdByUsername(username, message->chat->id);
          if (user_id) {
            user_ids.push_back(*user_id);
          } else {
            if (!logger_) logger_ = observability::Logger::getInstance().get();
            logger_->warn("Could not resolve username mention: @" + username);
          }
        }
      }
    }
  }

  return user_ids;
}

template<typename Derived>
std::optional<int64_t> BotBase<Derived>::extractUserIdFromMention(
    const std::string& mention, const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (mention.empty() || mention[0] != '@') {
    return std::nullopt;
  }

  std::string username = mention.substr(1);
  if (message && message->chat) {
    return lookupUserIdByUsername(username, message->chat->id);
  }

  return std::nullopt;
}

template<typename Derived>
std::optional<int64_t> BotBase<Derived>::lookupUserIdByUsername(
    const std::string& username, int64_t /* chat_id */) {
  try {
    {
      std::lock_guard<std::mutex> lock(username_cache_mutex_);
      auto it = username_cache_.find(username);
      if (it != username_cache_.end()) {
        return it->second;
      }
    }

    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->debug("Username not found in cache: @" + username);
    return std::nullopt;
  } catch (const std::exception& e) {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error looking up username: " + std::string(e.what()));
    return std::nullopt;
  }
}

template<typename Derived>
bool BotBase<Derived>::areTopicsEnabled() {
  auto& config = config::Config::getInstance();
  return config.getBool("telegram.topics.enabled", true);
}

template<typename Derived>
bool BotBase<Derived>::isCommandInCorrectTopic(
    const tgbotxx::Ptr<tgbotxx::Message>& message, const std::string& topic_type) {
  if (!areTopicsEnabled()) {
    return true;
  }

  if (!group_repo_) return true;

  auto topic_id = getTopicId(message);
  auto group = getOrCreateGroup(message->chat->id);

  auto topic = group_repo_->getTopic(group.id, topic_id.value_or(0), topic_type);

  if (topic) {
    return topic->is_active && topic->telegram_topic_id == topic_id;
  }

  // If no topic configured with this id, allow only if there is no active topic
  // configured for the given type at all.
  auto topic_by_type = group_repo_->getTopicByType(group.id, topic_type);
  if (topic_by_type) {
    return false;
  }

  return true;
}

template<typename Derived>
std::optional<int> BotBase<Derived>::getTopicId(
    const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (message->messageThreadId) {
    return message->messageThreadId;
  }
  return std::nullopt;
}

template<typename Derived>
bool BotBase<Derived>::isAdmin(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  if (!message->chat || !message->from) return false;
  return isGroupAdmin(message->chat->id, message->from->id);
}

template<typename Derived>
bool BotBase<Derived>::isGroupAdmin(int64_t /* chat_id */, int64_t /* user_id */) {
  return false;
}

template<typename Derived>
bool BotBase<Derived>::canUndoMatch(int64_t /* match_id */, int64_t /* user_id */,
                                   const models::Match& /* match */) {
  return false;
}

template<typename Derived>
void BotBase<Derived>::sendErrorMessage(
    const tgbotxx::Ptr<tgbotxx::Message>& message, const std::string& error) {
  if (!message) return;
  auto topic_id = getTopicId(message);
  sendMessage(message->chat->id, "❌ " + error, message->messageId, topic_id);
}

template<typename Derived>
void BotBase<Derived>::sendToLogsTopic(int64_t chat_id, const std::string& text) {
  if (!areTopicsEnabled() || !group_repo_) {
    sendMessage(chat_id, text);
    return;
  }

  try {
    auto group = getOrCreateGroup(chat_id);
    auto logs_topic = group_repo_->getTopic(group.id, 0, "logs");

    if (logs_topic && logs_topic->is_active) {
      sendMessage(chat_id, text, std::nullopt, logs_topic->telegram_topic_id);
    } else {
      sendMessage(chat_id, text);
    }
  } catch (const std::exception& e) {
    if (!logger_) logger_ = observability::Logger::getInstance().get();
    logger_->error("Error sending to logs topic: " + std::string(e.what()));
    sendMessage(chat_id, text);
  }
}

template<typename Derived>
models::Group BotBase<Derived>::getOrCreateGroup(int64_t telegram_group_id,
                                                const std::string& name) {
  if (!group_repo_) {
    throw std::runtime_error("GroupRepository not initialized");
  }
  auto group = group_repo_->getByTelegramId(telegram_group_id);
  if (group) {
    return *group;
  }
  return group_repo_->createOrGet(telegram_group_id, name);
}

template<typename Derived>
models::Player BotBase<Derived>::getOrCreatePlayer(int64_t telegram_user_id) {
  if (!player_repo_) {
    throw std::runtime_error("PlayerRepository not initialized");
  }
  auto player = player_repo_->getByTelegramId(telegram_user_id);
  if (player) {
    return *player;
  }
  return player_repo_->createOrGet(telegram_user_id);
}

template<typename Derived>
models::GroupPlayer BotBase<Derived>::getOrCreateGroupPlayer(int64_t group_id,
                                                            int64_t player_id) {
  if (!group_repo_) {
    throw std::runtime_error("GroupRepository not initialized");
  }
  return group_repo_->getOrCreateGroupPlayer(group_id, player_id);
}

template<typename Derived>
std::string BotBase<Derived>::generateIdempotencyKey(
    const tgbotxx::Ptr<tgbotxx::Message>& message) {
  return std::to_string(message->chat->id) + "_" + std::to_string(message->messageId);
}

template<typename Derived>
bool BotBase<Derived>::isDuplicateMatch(const std::string& idempotency_key) {
  if (!match_repo_) return false;
  auto match = match_repo_->getByIdempotencyKey(idempotency_key);
  return match.has_value();
}

template<typename Derived>
void BotBase<Derived>::updateEloAfterMatch(int64_t /* group_id */, int64_t /* player1_id */,
                                          int64_t /* player2_id */, int /* elo1_before */,
                                          int /* elo2_before */, int /* elo1_after */,
                                          int /* elo2_after */, int64_t /* match_id */) {}

template<typename Derived>
bool BotBase<Derived>::isMatchUndoable(const models::Match& /* match */,
                                      int64_t /* user_id */) {
  return false;
}

template<typename Derived>
void BotBase<Derived>::undoMatchTransaction(int64_t /* match_id */,
                                           int64_t /* undone_by_user_id */) {}

}  // namespace bot

#endif  // BOT_BOT_BASE_IMPL_H

