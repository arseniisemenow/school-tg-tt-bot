#ifndef BOT_BOT_API_H
#define BOT_BOT_API_H

#include <tgbotxx/utils/Ptr.hpp>
#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/MessageEntity.hpp>
#include <tgbotxx/objects/ReactionType.hpp>
#include <tgbotxx/objects/ChatMember.hpp>
#include <tgbotxx/Api.hpp>
#include <tgbotxx/objects/IReplyMarkup.hpp>
#include <tgbotxx/objects/LinkPreviewOptions.hpp>
#include <tgbotxx/objects/SuggestedPostParameters.hpp>
#include <string>
#include <vector>
#include <optional>

namespace bot {

// Interface for bot API operations (abstracts away tgbotxx::Bot)
// This allows us to create test implementations without requiring a valid token
class BotApi {
 public:
  virtual ~BotApi() = default;
  
  // Get API reference (for direct API calls if needed)
  virtual tgbotxx::Api* api() = 0;
  
  // Send a message (matches tgbotxx::Api::sendMessage signature)
  virtual tgbotxx::Ptr<tgbotxx::Message> sendMessage(
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
      tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params = nullptr) = 0;
  
  // Set message reaction
  virtual bool setMessageReaction(
      int64_t chat_id,
      int message_id,
      const std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>>& reaction_types,
      bool is_big = false) = 0;
  
  // Fetch chat member (used for admin checks)
  virtual tgbotxx::Ptr<tgbotxx::ChatMember> getChatMember(
      int64_t chat_id,
      int64_t user_id) = 0;
};

}  // namespace bot

#endif  // BOT_BOT_API_H

