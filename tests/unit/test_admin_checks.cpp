#include <gtest/gtest.h>
#include "bot/test_bot.h"

// Helper test bot that exposes admin check for verification
class AdminCheckBot : public bot::TestBot {
 public:
  using bot::TestBot::initialize;
  bool isAdminForTest(int64_t chat_id, int64_t user_id) {
    return isGroupAdmin(chat_id, user_id);
  }
};

TEST(AdminChecks, AdministratorAndCreatorAreAllowed) {
  AdminCheckBot bot;
  bot.initialize();

  const int64_t chat_id = 12345;

  bot.setMockChatMemberStatus(chat_id, 111, "administrator");
  EXPECT_TRUE(bot.isAdminForTest(chat_id, 111));

  bot.setMockChatMemberStatus(chat_id, 222, "creator");
  EXPECT_TRUE(bot.isAdminForTest(chat_id, 222));
}

TEST(AdminChecks, NonAdminOrMissingReturnsFalse) {
  AdminCheckBot bot;
  bot.initialize();

  const int64_t chat_id = 67890;

  bot.setMockChatMemberStatus(chat_id, 333, "member");
  EXPECT_FALSE(bot.isAdminForTest(chat_id, 333));

  // No record for this user/chat should also return false
  EXPECT_FALSE(bot.isAdminForTest(chat_id, 444));
}

