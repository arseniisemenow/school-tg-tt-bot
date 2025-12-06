#include "bot/production_bot_api.h"
#include <tgbotxx/objects/ReplyParameters.hpp>
#include <tgbotxx/objects/ReactionType.hpp>
#include <tgbotxx/objects/ChatMember.hpp>

namespace bot {

ProductionBotApi::ProductionBotApi(const std::string& token)
    : tgbotxx::Bot(token) {
}

tgbotxx::Api* ProductionBotApi::api() {
  return Bot::api().get();
}

tgbotxx::Ptr<tgbotxx::Message> ProductionBotApi::sendMessage(
    int64_t chat_id,
    const std::string& text,
    int message_thread_id,
    const std::string& parse_mode,
    const std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>& entities,
    bool disable_notification,
    bool protect_content,
    tgbotxx::Ptr<tgbotxx::IReplyMarkup> reply_markup,
    const std::string& business_connection_id,
    int direct_messages_topic_id,
    tgbotxx::Ptr<tgbotxx::LinkPreviewOptions> link_preview_options,
    bool allow_paid_broadcast,
    const std::string& message_effect_id,
    tgbotxx::Ptr<tgbotxx::SuggestedPostParameters> suggested_post_parameters,
    tgbotxx::Ptr<tgbotxx::ReplyParameters> reply_params) {
  
  return api()->sendMessage(
      chat_id,
      text,
      message_thread_id,
      parse_mode,
      entities,
      disable_notification,
      protect_content,
      reply_markup,
      business_connection_id,
      direct_messages_topic_id,
      link_preview_options,
      allow_paid_broadcast,
      message_effect_id,
      suggested_post_parameters,
      reply_params
  );
}

bool ProductionBotApi::setMessageReaction(
    int64_t chat_id,
    int message_id,
    const std::vector<tgbotxx::Ptr<tgbotxx::ReactionType>>& reaction_types,
    bool is_big) {
  
  return api()->setMessageReaction(chat_id, message_id, reaction_types, is_big);
}

tgbotxx::Ptr<tgbotxx::ChatMember> ProductionBotApi::getChatMember(
    int64_t chat_id,
    int64_t user_id) {
  return api()->getChatMember(chat_id, user_id);
}

}  // namespace bot

