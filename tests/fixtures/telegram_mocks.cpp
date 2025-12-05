#include "telegram_mocks.h"
#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/MessageEntity.hpp>
#include <tgbotxx/objects/ChatMemberUpdated.hpp>
#include <tgbotxx/objects/User.hpp>
#include <tgbotxx/objects/Chat.hpp>
#include <tgbotxx/objects/ChatMember.hpp>
#include <tgbotxx/utils/Ptr.hpp>
#include <ctime>
// Note: MessageEntity.hpp has compilation issues with current nlohmann_json version
// We'll work around this by not using entities in tests for now

namespace test_fixtures {

tgbotxx::Ptr<tgbotxx::Message> createMockMessage(
    int64_t chat_id,
    int64_t user_id,
    const std::string& text,
    std::optional<int> topic_id) {
  
  auto message = tgbotxx::Ptr<tgbotxx::Message>(new tgbotxx::Message());
  
  // Set chat
  message->chat = createMockChat(chat_id);
  
  // Set sender
  message->from = createMockUser(user_id);
  
  // Set message content
  message->text = text;
  message->messageId = static_cast<int>(std::time(nullptr)) % 1000000;  // Simple ID generation
  
  // Set topic/thread ID if provided
  if (topic_id.has_value()) {
    message->messageThreadId = topic_id.value();
  }
  
  // Initialize entities vector
  message->entities = std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>();
  
  return message;
}

tgbotxx::Ptr<tgbotxx::ChatMemberUpdated> createMockChatMemberUpdate(
    int64_t chat_id,
    int64_t user_id,
    const std::string& status,
    bool is_bot) {
  
  auto update = tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>(new tgbotxx::ChatMemberUpdated());
  
  // Set chat
  update->chat = createMockChat(chat_id);
  
  // Set user (the member being updated)
  update->from = createMockUser(user_id, std::nullopt, std::nullopt, is_bot);
  
  // Set old and new chat member status
  update->oldChatMember = tgbotxx::Ptr<tgbotxx::ChatMember>(new tgbotxx::ChatMember());
  update->oldChatMember->status = "left";  // Assume they were not a member before
  
  update->newChatMember = tgbotxx::Ptr<tgbotxx::ChatMember>(new tgbotxx::ChatMember());
  update->newChatMember->status = status;
  update->newChatMember->user = update->from;
  
  return update;
}

void addMention(
    tgbotxx::Ptr<tgbotxx::Message>& message,
    const std::string& username,
    int64_t user_id,
    size_t offset) {
  if (!message) {
    return;
  }

  auto entity = tgbotxx::Ptr<tgbotxx::MessageEntity>(new tgbotxx::MessageEntity());
  entity->offset = static_cast<int>(offset);
  entity->length = static_cast<int>(username.length() + 1);  // include '@'
  entity->type = tgbotxx::MessageEntity::Type::TextMention;

  auto user = tgbotxx::Ptr<tgbotxx::User>(new tgbotxx::User());
  user->id = user_id;
  user->username = username;
  entity->user = user;

  if (message->entities.empty()) {
    message->entities = std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>();
  }
  message->entities.push_back(entity);
}

tgbotxx::Ptr<tgbotxx::User> createMockUser(
    int64_t user_id,
    const std::optional<std::string>& username,
    const std::optional<std::string>& first_name,
    bool is_bot) {
  
  auto user = tgbotxx::Ptr<tgbotxx::User>(new tgbotxx::User());
  user->id = user_id;
  user->isBot = is_bot;
  
  if (username.has_value()) {
    user->username = username.value();
  }
  
  if (first_name.has_value()) {
    user->firstName = first_name.value();
  } else {
    user->firstName = "TestUser" + std::to_string(user_id);
  }
  
  return user;
}

tgbotxx::Ptr<tgbotxx::Chat> createMockChat(
    int64_t chat_id,
    const std::string& chat_type,
    const std::optional<std::string>& title) {
  
  auto chat = tgbotxx::Ptr<tgbotxx::Chat>(new tgbotxx::Chat());
  chat->id = chat_id;
  
  // Convert string to enum
  if (chat_type == "supergroup") {
    chat->type = tgbotxx::Chat::Type::Supergroup;
  } else if (chat_type == "group") {
    chat->type = tgbotxx::Chat::Type::Group;
  } else if (chat_type == "channel") {
    chat->type = tgbotxx::Chat::Type::Channel;
  } else {
    chat->type = tgbotxx::Chat::Type::Supergroup;  // Default to supergroup
  }
  
  if (title.has_value()) {
    chat->title = title.value();
  } else {
    chat->title = "Test " + chat_type;
  }
  
  return chat;
}

}  // namespace test_fixtures

