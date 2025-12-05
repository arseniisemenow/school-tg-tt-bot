#include "bot/test_bot_api.h"
#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/Chat.hpp>
#include <tgbotxx/objects/User.hpp>

namespace bot {

TestBotApi::TestBotApi() {
  // Create a mock API - we'll use a dummy token that won't be validated
  // The API won't actually make calls, but we need it for the interface
  try {
    mock_api_ = std::make_unique<tgbotxx::Api>("test_token_for_mocking");
  } catch (...) {
    // If API creation fails, we'll handle it gracefully
    // In tests, we won't actually use the API
  }
}

tgbotxx::Api* TestBotApi::api() {
  return mock_api_.get();
}

tgbotxx::Ptr<tgbotxx::Message> TestBotApi::sendMessage(
    int64_t chat_id,
    const std::string& text,
    int message_thread_id,
    const std::string& /* parse_mode */,
    const std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>& /* entities */,
    bool /* disable_notification */,
    bool /* protect_content */,
    tgbotxx::Ptr<tgbotxx::IReplyMarkup> /* reply_markup */,
    const std::string& /* business_connection_id */,
    int /* direct_messages_topic_id */,
    tgbotxx::Ptr<tgbotxx::LinkPreviewOptions> /* link_preview_options */,
    bool /* allow_paid_broadcast */,
    const std::string& /* message_effect_id */,
    tgbotxx::Ptr<tgbotxx::SuggestedPostParameters> /* suggested_post_parameters */,
    tgbotxx::Ptr<tgbotxx::ReplyParameters> /* reply_params */) {
  
  // Create a mock message response
  auto message = tgbotxx::Ptr<tgbotxx::Message>(new tgbotxx::Message());
  message->messageId = next_message_id_++;
  message->chat = tgbotxx::Ptr<tgbotxx::Chat>(new tgbotxx::Chat());
  message->chat->id = chat_id;
  message->text = text;
  message->messageThreadId = message_thread_id;
  
  // Store for test inspection
  SentMessage sent;
  sent.chat_id = chat_id;
  sent.text = text;
  sent.message_thread_id = message_thread_id;
  sent.message_id = message->messageId;
  sent_messages_.push_back(sent);
  
  return message;
}

bool TestBotApi::setMessageReaction(
    int64_t /* chat_id */,
    int /* message_id */,
    const std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>>& /* reaction_types */,
    bool /* is_big */) {
  // Mock implementation - always succeeds
  return true;
}

}  // namespace bot

