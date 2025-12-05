#ifndef TESTS_FIXTURES_TELEGRAM_MOCKS_H
#define TESTS_FIXTURES_TELEGRAM_MOCKS_H

#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/ChatMemberUpdated.hpp>
#include <tgbotxx/objects/User.hpp>
#include <tgbotxx/objects/Chat.hpp>
#include <tgbotxx/utils/Ptr.hpp>
#include <string>
#include <optional>
#include <vector>

namespace test_fixtures {

// Helper functions to create mock Telegram objects for testing

/**
 * Create a mock Telegram Message object
 * @param chat_id Telegram chat ID (group/channel ID)
 * @param user_id Telegram user ID (sender)
 * @param text Message text
 * @param topic_id Optional topic/thread ID (for forum groups)
 * @return Mock Message object
 */
tgbotxx::Ptr<tgbotxx::Message> createMockMessage(
    int64_t chat_id,
    int64_t user_id,
    const std::string& text,
    std::optional<int> topic_id = std::nullopt);

/**
 * Create a mock ChatMemberUpdated event
 * @param chat_id Telegram chat ID
 * @param user_id User ID (the member being updated)
 * @param status Member status ("member", "left", "kicked", "administrator", etc.)
 * @param is_bot Whether the member is a bot
 * @return Mock ChatMemberUpdated object
 */
tgbotxx::Ptr<tgbotxx::ChatMemberUpdated> createMockChatMemberUpdate(
    int64_t chat_id,
    int64_t user_id,
    const std::string& status,
    bool is_bot = false);

/**
 * Add a mention entity to a message
 * NOTE: Currently disabled due to tgbotxx compilation issues
 * @param message Message to add mention to
 * @param username Username (without @)
 * @param user_id Telegram user ID
 * @param offset Character offset in message text where mention appears
 */
void addMention(
    tgbotxx::Ptr<tgbotxx::Message>& message,
    const std::string& username,
    int64_t user_id,
    size_t offset);

/**
 * Create a mock User object
 * @param user_id Telegram user ID
 * @param username Optional username
 * @param first_name Optional first name
 * @param is_bot Whether user is a bot
 * @return Mock User object
 */
tgbotxx::Ptr<tgbotxx::User> createMockUser(
    int64_t user_id,
    const std::optional<std::string>& username = std::nullopt,
    const std::optional<std::string>& first_name = std::nullopt,
    bool is_bot = false);

/**
 * Create a mock Chat object
 * @param chat_id Telegram chat ID
 * @param chat_type Chat type ("group", "supergroup", "channel")
 * @param title Optional chat title
 * @return Mock Chat object
 */
tgbotxx::Ptr<tgbotxx::Chat> createMockChat(
    int64_t chat_id,
    const std::string& chat_type = "supergroup",
    const std::optional<std::string>& title = std::nullopt);

}  // namespace test_fixtures

#endif  // TESTS_FIXTURES_TELEGRAM_MOCKS_H

