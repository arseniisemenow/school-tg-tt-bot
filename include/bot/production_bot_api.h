#ifndef BOT_PRODUCTION_BOT_API_H
#define BOT_PRODUCTION_BOT_API_H

#include "bot_api.h"
#include <tgbotxx/Bot.hpp>
#include <memory>

namespace bot {

// Production implementation of BotApi that uses tgbotxx::Bot
class ProductionBotApi : public BotApi, public tgbotxx::Bot {
 public:
  explicit ProductionBotApi(const std::string& token);
  ~ProductionBotApi() override = default;
  
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
};

}  // namespace bot

#endif  // BOT_PRODUCTION_BOT_API_H

