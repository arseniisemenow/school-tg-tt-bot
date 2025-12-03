#ifndef BOT_BOT_H
#define BOT_BOT_H

#include <memory>
#include <string>
#include <atomic>
#include <tgbotxx/Bot.hpp>
#include <tgbotxx/utils/Ptr.hpp>
#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/ChatMemberUpdated.hpp>

namespace bot {

class Bot : public tgbotxx::Bot {
 public:
  Bot(const std::string& token);
  ~Bot();
  
  // Initialize bot (set up handlers, etc.)
  void initialize();
  
  // Start polling (override tgbotxx::Bot::start)
  void startPolling();
  
  // Start webhook server
  void startWebhook(const std::string& webhook_url, int port);
  
  // Stop bot
  void stop();

 protected:
  // Override tgbotxx::Bot virtual functions
  void onCommand(const tgbotxx::Ptr<tgbotxx::Message>& command) override;
  void onChatMemberUpdated(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& chatMember) override;
  void onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) override;

 private:
  std::string token_;
  bool running_ = false;
  
  // Command handlers
  void handleStart(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleMatch(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleRanking(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleId(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleIdGuest(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleUndo(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleConfigTopic(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleHelp(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Helper: Extract command name from message
  std::string extractCommandName(const tgbotxx::Ptr<tgbotxx::Message>& message);
};

}  // namespace bot

#endif  // BOT_BOT_H

