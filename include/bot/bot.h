#ifndef BOT_BOT_H
#define BOT_BOT_H

#include <memory>
#include <string>
#include <atomic>
#include <vector>
#include <optional>
#include <regex>
#include <tgbotxx/Bot.hpp>
#include <tgbotxx/utils/Ptr.hpp>
#include <tgbotxx/objects/Message.hpp>
#include <tgbotxx/objects/ChatMemberUpdated.hpp>
#include <tgbotxx/objects/User.hpp>
#include <tgbotxx/objects/Chat.hpp>

// Forward declarations
namespace database {
class ConnectionPool;
}

namespace repositories {
class GroupRepository;
class PlayerRepository;
class MatchRepository;
}

namespace school21 {
class ApiClient;
}

namespace utils {
class EloCalculator;
}

namespace observability {
class Logger;
}

namespace config {
class Config;
}

namespace models {
struct Group;
struct Player;
struct GroupPlayer;
struct Match;
}

namespace bot {

class Bot : public tgbotxx::Bot {
 public:
  Bot(const std::string& token);
  ~Bot();
  
  // Initialize bot (set up handlers, dependencies, etc.)
  void initialize();
  
  // Set dependencies (called from main.cpp after creating them)
  void setDependencies(
      std::shared_ptr<database::ConnectionPool> db_pool,
      std::unique_ptr<repositories::GroupRepository> group_repo,
      std::unique_ptr<repositories::PlayerRepository> player_repo,
      std::unique_ptr<repositories::MatchRepository> match_repo,
      std::unique_ptr<school21::ApiClient> school21_client);
  
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
  
  // Dependencies
  std::shared_ptr<database::ConnectionPool> db_pool_;
  std::unique_ptr<repositories::GroupRepository> group_repo_;
  std::unique_ptr<repositories::PlayerRepository> player_repo_;
  std::unique_ptr<repositories::MatchRepository> match_repo_;
  std::unique_ptr<school21::ApiClient> school21_client_;
  std::unique_ptr<utils::EloCalculator> elo_calculator_;
  observability::Logger* logger_;
  
  // Command handlers
  void handleStart(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleMatch(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleRanking(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleId(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleIdGuest(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleUndo(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleConfigTopic(const tgbotxx::Ptr<tgbotxx::Message>& message);
  void handleHelp(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Group event handlers
  void handleMemberJoin(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update);
  void handleMemberLeave(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update);
  void handleBotRemoval(const tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>& update);
  void handleGroupMigration(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Helper methods
  std::string extractCommandName(const tgbotxx::Ptr<tgbotxx::Message>& message);
  std::string extractCommandArgs(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Match command parsing
  struct ParsedMatchCommand {
    int64_t player1_user_id = 0;
    int64_t player2_user_id = 0;
    int score1 = 0;
    int score2 = 0;
    bool valid = false;
  };
  ParsedMatchCommand parseMatchCommand(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Player mention parsing
  std::vector<int64_t> extractMentionedUserIds(const tgbotxx::Ptr<tgbotxx::Message>& message);
  std::optional<int64_t> extractUserIdFromMention(const std::string& mention, 
                                                   const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Topic validation
  bool isCommandInCorrectTopic(const tgbotxx::Ptr<tgbotxx::Message>& message, 
                               const std::string& topic_type);
  std::optional<int> getTopicId(const tgbotxx::Ptr<tgbotxx::Message>& message);
  
  // Permission checks
  bool isAdmin(const tgbotxx::Ptr<tgbotxx::Message>& message);
  bool isGroupAdmin(int64_t chat_id, int64_t user_id);
  bool canUndoMatch(int64_t match_id, int64_t user_id, const models::Match& match);
  
  // Message sending helpers
  void sendMessage(int64_t chat_id, const std::string& text, 
                   std::optional<int> reply_to_message_id = std::nullopt);
  void sendErrorMessage(const tgbotxx::Ptr<tgbotxx::Message>& message, 
                       const std::string& error);
  void reactToMessage(int64_t chat_id, int message_id, const std::string& emoji);
  
  // Group and player helpers
  models::Group getOrCreateGroup(int64_t telegram_group_id, const std::string& name = "");
  models::Player getOrCreatePlayer(int64_t telegram_user_id);
  models::GroupPlayer getOrCreateGroupPlayer(int64_t group_id, int64_t player_id);
  
  // Match helpers
  std::string generateIdempotencyKey(const tgbotxx::Ptr<tgbotxx::Message>& message);
  bool isDuplicateMatch(const std::string& idempotency_key);
  
  // ELO helpers
  void updateEloAfterMatch(int64_t group_id, int64_t player1_id, int64_t player2_id,
                           int elo1_before, int elo2_before,
                           int elo1_after, int elo2_after,
                           int64_t match_id);
  
  // Undo helpers
  bool isMatchUndoable(const models::Match& match, int64_t user_id);
  void undoMatchTransaction(int64_t match_id, int64_t undone_by_user_id);
};

}  // namespace bot

#endif  // BOT_BOT_H

