#include "bot/bot.h"

#include <iostream>
#include <stdexcept>

namespace bot {

Bot::Bot(const std::string& token) : tgbotxx::Bot(token), token_(token) {
}

Bot::~Bot() {
  stop();
}

void Bot::initialize() {
  // Bot is ready, handlers are set up via virtual function overrides
}

void Bot::onCommand(const tgbotxx::Ptr<tgbotxx::Message>& command) {
  std::string cmd = extractCommandName(command);
  
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
  }
  // Unknown commands are handled by onUnknownCommand (if we override it)
}

void Bot::onChatMemberUpdated(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& chatMember) {
  // TODO: Implement group event handling
  std::cout << "Chat member update received" << std::endl;
}

void Bot::onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // Command-only mode: ignore non-command messages
  // Commands are handled by onCommand
  // This function is called for all messages, but we only process commands
  (void)message;  // Suppress unused parameter warning
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

// Command handlers (stubs - to be implemented)
void Bot::handleStart(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement
  std::cout << "Start command received" << std::endl;
}

void Bot::handleMatch(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement match registration
  std::cout << "Match command received" << std::endl;
}

void Bot::handleRanking(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement ranking display
  std::cout << "Ranking command received" << std::endl;
}

void Bot::handleId(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement ID verification
  std::cout << "ID command received" << std::endl;
}

void Bot::handleIdGuest(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement guest registration
  std::cout << "ID guest command received" << std::endl;
}

void Bot::handleUndo(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement undo operation
  std::cout << "Undo command received" << std::endl;
}

void Bot::handleConfigTopic(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement topic configuration
  std::cout << "Config topic command received" << std::endl;
}

void Bot::handleHelp(const tgbotxx::Ptr<tgbotxx::Message>& message) {
  // TODO: Implement help command
  std::cout << "Help command received" << std::endl;
}

// handleChatMemberUpdate is now onChatMemberUpdated (override)
// handleMessage is now onAnyMessage (override)

}  // namespace bot

