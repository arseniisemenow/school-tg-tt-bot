#ifndef BOT_TEST_BOT_API_H
#define BOT_TEST_BOT_API_H

#include "bot_api.h"
#include <tgbotxx/Api.hpp>
#include <tgbotxx/objects/ChatMember.hpp>
#include <tgbotxx/objects/User.hpp>
#include <tgbotxx/objects/WebhookInfo.hpp>
#include <memory>
#include <map>

namespace bot {

// Test implementation of BotApi that doesn't require a valid Telegram token
// This allows testing bot logic without making actual API calls
class TestBotApi : public BotApi {
 public:
  TestBotApi();
  ~TestBotApi() override = default;
  
  tgbotxx::Api* api() override;
  
  tgbotxx::Ptr<tgbotxx::Message> sendMessage(
      int64_t chat_id,
      const std::string& text,
      int message_thread_id = 0,
      const std::string& parse_mode = "",
      const std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>& entities = {},
      bool disable_notification = false,
      bool protect_content = false,
      tgbotxx::Ptr<tgbotxx::IReplyMarkup> reply_markup = nullptr,
      const std::string& business_connection_id = "",
      int direct_messages_topic_id = 0,
      tgbotxx::Ptr<tgbotxx::LinkPreviewOptions> link_preview_options = nullptr,
      bool allow_paid_broadcast = false,
      const std::string& message_effect_id = "",
      tgbotxx::Ptr<tgbotxx::SuggestedPostParameters> suggested_post_parameters = nullptr,
      tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params = nullptr) override;
  
  bool setMessageReaction(
      int64_t chat_id,
      int message_id,
      const std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>>& reaction_types,
      bool is_big = false) override;
  
  tgbotxx::Ptr<tgbotxx::ChatMember> getChatMember(
      int64_t chat_id,
      int64_t user_id) override;
  
  // Webhook methods (mock implementations for testing)
  bool setWebhook(
      const std::string& url,
      const std::optional<cpr::File>& certificate = std::nullopt,
      const std::string& ip_address = "",
      int max_connections = 40,
      const std::vector<std::string>& allowed_updates = {},
      bool drop_pending_updates = false,
      const std::string& secret_token = "") override;
  
  bool deleteWebhook(bool drop_pending_updates = false) override;
  
  tgbotxx::Ptr<tgbotxx::WebhookInfo> getWebhookInfo() override;
  
  // Test helpers - allow tests to inspect sent messages
  struct SentMessage {
    int64_t chat_id;
    std::string text;
    int message_thread_id;
    int message_id;
  };
  
  std::vector<SentMessage> getSentMessages() const { return sent_messages_; }
  void clearSentMessages() { sent_messages_.clear(); }
  
  // Test helper: set mocked chat member status (e.g., "administrator", "creator", "member")
  void setMockChatMemberStatus(int64_t chat_id, int64_t user_id, const std::string& status);
  void clearMockChatMembers() { chat_members_.clear(); }
  
  // Webhook test helpers
  std::string getWebhookUrl() const { return webhook_url_; }
  std::string getWebhookSecretToken() const { return webhook_secret_token_; }
  bool isWebhookSet() const { return !webhook_url_.empty(); }
  
 private:
  // Mock API that doesn't make real calls
  std::unique_ptr<tgbotxx::Api> mock_api_;
  std::vector<SentMessage> sent_messages_;
  int next_message_id_ = 1;
  
  // Keyed by chat_id -> user_id -> status
  std::map<int64_t, std::map<int64_t, std::string>> chat_members_;
  
  // Webhook state for testing
  std::string webhook_url_;
  std::string webhook_secret_token_;
};

}  // namespace bot

#endif  // BOT_TEST_BOT_API_H

