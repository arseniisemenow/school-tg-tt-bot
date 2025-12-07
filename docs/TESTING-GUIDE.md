# Testing Guide: Testing Without Telegram API

## Overview

This guide explains how to test the Telegram bot without manually interacting with Telegram. It covers testing all the scenarios you mentioned:
- Bot added into channel
- Admin configured topics
- Users registered in id topic
- Users registering their matches in matches topic

## Current Testing Infrastructure

The project already has:
- **Google Test (gtest)** framework integrated
- **Integration tests** for database operations (`tests/integration/`)
- **Unit tests** for utilities (`tests/unit/`)
- **Test database** support via Docker Compose
- **Test script**: `scripts/test.sh`

## Testing Approaches

### Approach 1: Mock Telegram API Objects (Recommended)

The bot uses `tgbotxx` library which provides C++ objects for Telegram entities. We can create mock objects directly in tests without needing a real Telegram API.

#### How It Works

1. **Create Mock Message Objects**: Build `tgbotxx::Message` objects with test data
2. **Call Bot Handlers Directly**: Invoke bot command handlers with mock messages
3. **Verify Database State**: Check that database operations completed correctly
4. **Verify Bot Responses**: Capture and verify messages the bot would send

#### Implementation Strategy

Create a test fixture that:
- Sets up database connection
- Creates mock Telegram message/event objects
- Provides helper methods to create different message types
- Captures bot responses (mock the `sendMessage` calls)

### Approach 2: Integration Tests with Mocked Telegram API Layer

Abstract the Telegram API behind an interface and mock it in tests. This requires refactoring but provides better testability.

### Approach 3: End-to-End Tests with Mock HTTP Server

Create a mock HTTP server that mimics Telegram Bot API endpoints. The bot makes HTTP calls to this mock server instead of real Telegram.

## Testing Your Specific Scenarios

### Scenario 1: Bot Added into Channel

**What to Test:**
- Bot receives `ChatMemberUpdated` event when added to a group
- Group is created in database
- Group is marked as active

**How to Test:**

```cpp
// Create mock ChatMemberUpdated event
auto chat_member = tgbotxx::Ptr<tgbotxx::ChatMemberUpdated>(new tgbotxx::ChatMemberUpdated());
chat_member->chat = tgbotxx::Ptr<tgbotxx::Chat>(new tgbotxx::Chat());
chat_member->chat->id = -1001234567890;  // Test group ID
chat_member->chat->type = "supergroup";
chat_member->chat->title = "Test Group";

// Create bot user (the bot itself)
auto bot_user = tgbotxx::Ptr<tgbotxx::User>(new tgbotxx::User());
bot_user->id = 123456789;  // Bot's user ID
bot_user->isBot = true;
bot_user->firstName = "TestBot";

chat_member->from = bot_user;
chat_member->newChatMember = tgbotxx::Ptr<tgbotxx::ChatMember>(new tgbotxx::ChatMember());
chat_member->newChatMember->status = "member";

// Call bot's handler
bot->onChatMemberUpdated(chat_member);

// Verify: Group exists in database and is active
auto group = group_repo->getByTelegramId(-1001234567890);
ASSERT_TRUE(group.has_value());
ASSERT_TRUE(group->is_active);
```

### Scenario 2: Admin Configured Topics

**What to Test:**
- Admin sends `/config_topic matches` in a topic
- Topic configuration is saved to database
- Topic type is correctly associated with topic ID

**How to Test:**

```cpp
// Create mock message from admin in a topic
auto message = createMockMessage();
message->chat->id = -1001234567890;  // Group ID
message->messageThreadId = 123;  // Topic ID
message->text = "/config_topic matches";
message->from->id = 987654321;  // Admin user ID

// Mock isAdmin to return true
// (You'll need to mock the Telegram API call or set up test data)

// Call handler
bot->onCommand(message);

// Verify: Topic configuration exists
auto group = group_repo->getByTelegramId(-1001234567890);
auto topic = group_repo->getTopic(group->id, 123, "matches");
ASSERT_TRUE(topic.has_value());
ASSERT_EQ(topic->topic_type, "matches");
ASSERT_EQ(topic->telegram_topic_id, 123);
ASSERT_TRUE(topic->is_active);
```

### Scenario 3: Users Registered in ID Topic

**What to Test:**
- User sends `/id nickname` in ID topic
- User is registered in database
- School21 nickname is verified (mocked)
- User can now participate in matches

**How to Test:**

```cpp
// Create mock message in ID topic
auto message = createMockMessage();
message->chat->id = -1001234567890;
message->messageThreadId = 456;  // ID topic ID
message->text = "/id testuser";
message->from->id = 111222333;  // User ID

// Configure ID topic in database first
models::GroupTopic id_topic;
id_topic.group_id = group_id;
id_topic.telegram_topic_id = 456;
id_topic.topic_type = "id";
id_topic.is_active = true;
group_repo->configureTopic(id_topic);

// Mock School21 API to return valid participant
// (You'll need to mock school21_client)

// Call handler
bot->onCommand(message);

// Verify: Player registered with nickname
auto player = player_repo->getByTelegramId(111222333);
ASSERT_TRUE(player.has_value());
ASSERT_EQ(player->school_nickname.value(), "testuser");
ASSERT_TRUE(player->is_verified_student || player->is_allowed_non_student);
```

### Scenario 4: Users Registering Matches in Matches Topic

**What to Test:**
- User sends `/match @player1 @player2 3 1` in matches topic
- Match is created in database
- ELO is updated for both players
- Match statistics are correct

**How to Test:**

```cpp
// Create mock message in matches topic
auto message = createMockMessage();
message->chat->id = -1001234567890;
message->messageThreadId = 789;  // Matches topic ID
message->text = "/match @player1 @player2 3 1";
message->from->id = 111222333;

// Add message entities for mentions
auto entity1 = tgbotxx::Ptr<tgbotxx::MessageEntity>(new tgbotxx::MessageEntity());
entity1->type = "mention";
entity1->offset = 8;  // Position of @player1
entity1->length = 8;
entity1->user = tgbotxx::Ptr<tgbotxx::User>(new tgbotxx::User());
entity1->user->id = 444555666;  // player1 user ID

auto entity2 = tgbotxx::Ptr<tgbotxx::MessageEntity>(new tgbotxx::MessageEntity());
entity2->type = "mention";
entity2->offset = 17;  // Position of @player2
entity2->length = 8;
entity2->user = tgbotxx::Ptr<tgbotxx::User>(new tgbotxx::User());
entity2->user->id = 777888999;  // player2 user ID

message->entities = {entity1, entity2};

// Configure matches topic
models::GroupTopic matches_topic;
matches_topic.group_id = group_id;
matches_topic.telegram_topic_id = 789;
matches_topic.topic_type = "matches";
matches_topic.is_active = true;
group_repo->configureTopic(matches_topic);

// Ensure players exist and are registered
auto player1 = player_repo->createOrGet(444555666);
auto player2 = player_repo->createOrGet(777888999);
auto gp1 = group_repo->getOrCreateGroupPlayer(group_id, player1.id);
auto gp2 = group_repo->getOrCreateGroupPlayer(group_id, player2.id);

int elo1_before = gp1.current_elo;
int elo2_before = gp2.current_elo;

// Call handler
bot->onCommand(message);

// Verify: Match created
auto matches = match_repo->getByGroup(group_id);
ASSERT_EQ(matches.size(), 1);
auto match = matches[0];
ASSERT_EQ(match.player1_id, player1.id);
ASSERT_EQ(match.player2_id, player2.id);
ASSERT_EQ(match.player1_score, 3);
ASSERT_EQ(match.player2_score, 1);

// Verify: ELO updated
auto gp1_after = group_repo->getOrCreateGroupPlayer(group_id, player1.id);
auto gp2_after = group_repo->getOrCreateGroupPlayer(group_id, player2.id);
ASSERT_GT(gp1_after.current_elo, elo1_before);  // Winner's ELO increased
ASSERT_LT(gp2_after.current_elo, elo2_before);  // Loser's ELO decreased
ASSERT_EQ(gp1_after.matches_played, 1);
ASSERT_EQ(gp2_after.matches_played, 1);
```

## Implementation Plan

### Step 1: Create Test Fixtures and Helpers

Create `tests/fixtures/telegram_mocks.h` and `tests/fixtures/telegram_mocks.cpp`:

```cpp
// Helper functions to create mock Telegram objects
tgbotxx::Ptr<tgbotxx::Message> createMockMessage(
    int64_t chat_id,
    int64_t user_id,
    const std::string& text,
    std::optional<int> topic_id = std::nullopt);

tgbotxx::Ptr<tgbotxx::ChatMemberUpdated> createMockChatMemberUpdate(
    int64_t chat_id,
    int64_t user_id,
    const std::string& status);

// Helper to add message entities (mentions, etc.)
void addMention(tgbotxx::Ptr<tgbotxx::Message>& message,
                const std::string& username,
                int64_t user_id,
                size_t offset);
```

### Step 2: Create Mock Response Capturer

Since the bot calls `sendMessage()`, we need to capture these calls:

**Option A**: Make `sendMessage` virtual and create a testable bot class
**Option B**: Use dependency injection for message sending
**Option C**: Check database/logs instead of capturing messages

### Step 3: Mock School21 API Client

Create a mock `school21::ApiClient` that returns test data:

```cpp
class MockSchool21Client : public school21::ApiClient {
public:
    std::optional<school21::Participant> getParticipant(const std::string& nickname) override {
        if (nickname == "testuser") {
            school21::Participant p;
            p.nickname = "testuser";
            p.status = "ACTIVE";
            return p;
        }
        return std::nullopt;
    }
};
```

### Step 4: Create Integration Test Suite

Create `tests/integration/test_bot_scenarios.cpp`:

```cpp
class BotScenarioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup database
        // Create bot instance
        // Set up dependencies with mocks
    }
    
    std::unique_ptr<bot::Bot> bot_;
    std::shared_ptr<database::ConnectionPool> db_pool_;
    std::unique_ptr<repositories::GroupRepository> group_repo_;
    std::unique_ptr<repositories::PlayerRepository> player_repo_;
    std::unique_ptr<repositories::MatchRepository> match_repo_;
    std::unique_ptr<MockSchool21Client> school21_client_;
};

TEST_F(BotScenarioTest, BotAddedToChannel) {
    // Test scenario 1
}

TEST_F(BotScenarioTest, AdminConfiguresTopic) {
    // Test scenario 2
}

TEST_F(BotScenarioTest, UserRegistersInIdTopic) {
    // Test scenario 3
}

TEST_F(BotScenarioTest, UserRegistersMatch) {
    // Test scenario 4
}
```

## Recommended Testing Structure

```
tests/
├── fixtures/
│   ├── telegram_mocks.h          # Mock Telegram object creators
│   ├── telegram_mocks.cpp
│   └── test_bot_fixture.h        # Test fixture for bot tests
├── integration/
│   ├── test_bot_scenarios.cpp    # Your scenarios
│   ├── test_bot_commands.cpp     # Command handler tests
│   └── test_bot_events.cpp        # Event handler tests
└── mocks/
    ├── mock_school21_client.h    # Mock School21 API
    └── mock_telegram_api.h        # Mock Telegram API (if needed)
```

## Running the Tests

```bash
# Inside bot container
docker compose exec bot bash

# Build tests
./scripts/build.sh

# Run all tests
./scripts/test.sh

# Run specific test
./build/school_tg_tt_bot_tests --gtest_filter="BotScenarioTest.*"

### Coverage

```bash
# Inside bot container
docker compose exec bot bash -lc "./scripts/build.sh --coverage"
docker compose exec bot bash -lc "./scripts/test.sh"
```

Artifacts:
- Coverage HTML: `tests/coverage-html/index.html`
- Line coverage value (numeric, no `%`): `tests/coverage.txt`
- Raw lcov data: `tests/coverage.filtered.info`
```

## Benefits of This Approach

1. **No Telegram API Required**: Tests run completely offline
2. **Fast**: No network calls, tests run in milliseconds
3. **Deterministic**: Same inputs always produce same outputs
4. **Comprehensive**: Can test error cases, edge cases, boundary conditions
5. **CI/CD Friendly**: Tests can run in any environment
6. **Isolated**: Each test is independent and doesn't affect others

## Next Steps

1. **Create mock helpers** (`tests/fixtures/telegram_mocks.*`)
2. **Create test fixture** for bot integration tests
3. **Implement mock School21 client**
4. **Write test cases** for each scenario
5. **Add to CI/CD pipeline**

## Example: Complete Test Implementation

See `tests/integration/test_bot_scenarios_example.cpp` (to be created) for a complete working example of all four scenarios.



