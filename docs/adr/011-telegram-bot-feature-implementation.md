# ADR-011: Telegram Bot Feature Implementation

## Status
Accepted

## Context
The bot needs to implement Telegram-specific features:
- Handle group events (member joins/leaves, bot removal, group migration)
- Parse and validate commands
- Handle webhook message queue
- Support topic-based command routing
- Manage match lifecycle
- Support undo operations
- Handle bot-specific Telegram behaviors
- Each command must provide `help` options. Example `/match help`, or `/config_topic help` 

Key requirements:
- Bot triggers only on commands
- Multiple topic support with admin configuration
- Match format: `/match @player1 @player2 3 1`
- One-time match records (immutable after creation)
- Undo operations with permission checks
- Group event handling

## Decision
We will implement Telegram bot features with the following design:

### Group Event Handling

#### Member Join Events
- **Event**: `chat_member` update with status `member`
- **Action**: 
  - Log member join
  - No automatic action (users register themselves)
  - Optional: Send welcome message with instructions
- **Implementation**: Handle `ChatMemberUpdated` event from tgbotxx

#### Member Leave Events
- **Event**: `chat_member` update with status `left` or `kicked`
- **Action**:
  - Mark player as inactive (soft delete: set `deleted_at`)
  - Preserve match history (don't delete)
  - Preserve ELO history (for audit trail)
- **Implementation**: Handle `ChatMemberUpdated` event

#### Bot Removal from Group
- **Event**: Bot receives `chat_member` update with status `left` or `kicked`
- **Action**:
  - Mark group as inactive (`groups.is_active = FALSE`)
  - Stop processing commands from that group
  - Log bot removal
  - Alert admin (if monitoring enabled)
- **Recovery**: If bot re-added, reactivate group (`is_active = TRUE`)

#### Group Migration (Supergroup Conversion)
- **Event**: Group upgraded to supergroup (new `chat.id`)
- **Action**:
  - Detect migration (old group ID → new group ID)
  - Update `groups.telegram_group_id` to new ID
  - Preserve all data (matches, ELO, players)
  - Log migration
- **Implementation**: Handle `migrate_from_chat_id` in `Message` object

### Command Parsing and Validation

#### Command Format
- **Match Command**: `/match @player1 @player2 <score1> <score2>`
  - Example: `/match @username1 @username2 3 1`
  - Format: `/match @<mention1> @<mention2> <int> <int>`
- **Ranking Command**: `/ranking` or `/rank`
  - Shows current ELO rankings for the group
- **ID Command**: `/id <school_nickname>` (in ID topic only)
  - Associates school nickname with Telegram user
- **Help Command**: `/help`
  - Shows available commands

#### Command Parsing Strategy
- **Regex Parsing**: Use regex to extract command components
- **Match Command Regex**: `^/match\s+@(\w+)\s+@(\w+)\s+(\d+)\s+(\d+)$`
- **Extraction**:
  1. Extract command type (`/match`, `/ranking`, etc.)
  2. Extract mentions (for match command)
  3. Extract scores (for match command)
  4. Validate each component
- **Validation**:
  - Command must start with `/`
  - Mentions must be valid (extract user ID from mention)
  - Scores must be non-negative integers
  - Players must be different

#### Player Mention Parsing
- **Telegram Mentions**: `@username` in message text
- **User ID Resolution**:
  - Extract username from mention
  - Look up user ID from `message.entities` (Telegram provides user IDs)
  - Fallback: Query database for username → user_id mapping
- **Validation**:
  - Verify mentioned users exist in Telegram
  - Verify mentioned users are in the group
  - Reject if user not found

#### Command Validation
- **Format Validation**: Command matches expected format
- **Parameter Validation**: All parameters valid (user IDs, scores)
- **Context Validation**: 
  - Command sent in correct topic (if topic-based)
  - User has permission
  - Group is active
- **State Validation**: 
  - Players exist
  - Players are in group
  - Match not duplicate

### Message Queue Handling (Webhooks)

#### Webhook Message Processing
- **Receipt**: Receive webhook POST request
- **Acknowledgment**: Immediately return 200 OK (acknowledge to Telegram)
- **Async Processing**: Process message asynchronously after acknowledgment
- **Queue**: Use in-memory queue for message processing
  - Simple queue (std::queue or similar)
  - Worker threads process queue
  - Max queue size: 1000 messages
  - Drop messages if queue full (log warning)

#### Duplicate Message Handling
- **Idempotency**: Use `message_id` as idempotency key
- **Deduplication**: Check if message already processed
- **Storage**: Track processed message IDs (in-memory cache, TTL: 24 hours)
- **Response**: If duplicate, return success without processing

#### Message Acknowledgment Strategy
- **Immediate Acknowledgment**: Acknowledge webhook immediately (200 OK)
- **Processing**: Process message after acknowledgment
- **Error Handling**: If processing fails, log error (message already acknowledged)
- **Retry**: No retry (Telegram doesn't retry on 200 OK)

### Topic-Based Command Routing

#### Topic Configuration
- **Storage**: `group_topics` table (see ADR-002)
- **Topic Types**:
  - `id`: School nickname registration topic
  - `ranking`: Ranking display topic
  - `matches`: Match registration topic
  - `logs`: Logs that users must know about
- **Configuration**: Group admins configure which topics to listen to

#### Topic Detection
- **Forum Groups**: Use `message.message_thread_id` (topic ID)
- **Regular Groups**: `message_thread_id` is null (no topics)
- **Topic Mapping**: Map topic ID to topic type via `group_topics` table

#### Command Routing
- **Route by Topic**: 
  - `/match` commands only processed in `matches` topic
  - `/id` commands only processed in `id` topic
  - `/ranking` commands only processed in `ranking` topic
- **Default Behavior**: If no topic configured, process in any topic (backward compatibility)
- **Error Handling**: If command in wrong topic, ignore or send error message

#### Admin Topic Configuration
- **Command**: `/config_topic <topic_type>`
  - Example: `/config_topic matches`
  - Only group admins can use this command
  - It will configure current topic
- **Validation**: Verify admin permission
- **Storage**: Insert/update `group_topics` record

### Match Lifecycle Management

#### Match Creation
- **Trigger**: `/match` command in matches topic
- **Process**:
  1. Parse command (extract players, scores)
  2. Validate (players exist, scores valid, not duplicate)
  3. Calculate ELO changes
  4. Begin transaction
  5. Update player ELOs (with optimistic locking)
  6. Insert match record
  7. Insert ELO history records
  8. Commit transaction
  9. Send confirmation message (or set an emoji on message)
- **Idempotency**: Use `message_id` as idempotency key

#### Match Immutability
- **After Creation**: Match record is immutable
- **No Updates**: Never update match record (except undo flag)
- **Undo Instead**: Use undo operation to reverse match
- **Rationale**: Preserves audit trail, prevents data corruption

#### Match State
- **States**: `active`, `undone`
- **Flag**: `matches.is_undone` boolean flag
- **State Transitions**:
  - `active` → `undone` (via undo operation)
  - No other transitions allowed

### Undo Operations

#### Undo Command
- **Format**: `/undo` with reply on undoning message or `/undo` (undo last match)
- **Permission**: Only match players and group admins
- **Validation**:
  - Match exists
  - Match not already undone
  - User has permission (is player or admin)
  - Need to receive permissions to undo from players
  - Match is recent (within 24 hours, configurable)

#### Undo Process
- **Transaction**:
  1. Begin transaction
  2. Validate undo permission
  3. Reverse ELO changes (update `group_players`)
  4. Mark match as undone (`is_undone = TRUE`)
  5. Insert reverse `elo_history` entries (with `is_undone = TRUE`)
  6. Commit transaction
- **ELO Rollback**: 
  - Restore ELO to values before match
  - Use `elo_history` to find previous ELO values
  - Update `group_players.current_elo`

#### Undo Limitations
- **Time Limit**: Can only undo matches within 24 hours (configurable)
- **Reason**: Prevent abuse, maintain data integrity
- **Admin Override**: Admins can undo any match (no time limit)

### Bot Command Triggers

#### Command-Only Mode
- **Behavior**: Bot only responds to commands (messages starting with `/`)
- **Ignore**: Ignore all non-command messages
- **Rationale**: Reduces noise, clear user intent

#### Command Recognition
- **Pattern**: Message text starts with `/`
- **Bot Commands**: Telegram provides `message.entities` with command info
- **Validation**: Verify command is for this bot (if multiple bots in group)

### Error Handling

#### Command Errors
- **Invalid Format**: Send error message with correct format example
- **Invalid Players**: Send error: "Player not found" or "Player not in group"
- **Permission Denied**: Send error: "You don't have permission for this operation"
- **Duplicate Match**: Send error: "This match was already registered"
- **Validation Failure**: Send generic error: "Invalid match data"

#### Error Messages
- **User-Friendly**: Clear, actionable error messages
- **No Internal Details**: Don't expose internal errors (database errors, etc.)
- **Logging**: Log detailed errors internally for debugging

### Response Messages

#### Match Registration Response
- **Success**: "Match registered: @player1 (3) vs @player2 (1). ELO: @player1 +15, @player2 -15"
- **Format**: Include players, scores, ELO changes

#### Ranking Response
- **Format**: List top N players with ELO
- **Example**:
  ```
  Current Rankings:
  1. @player1 - 1650 ELO
  2. @player2 - 1620 ELO
  3. @player3 - 1580 ELO
  ```

#### Undo Response
- **Success**: "Match #42 undone. ELO restored."
- **Failure**: "Cannot undo: [reason]"

## Consequences

### Positive
- **User Experience**: Clear commands and responses
- **Flexibility**: Topic-based routing enables organized groups
- **Data Integrity**: Immutable matches preserve audit trail
- **Permission Control**: Undo permissions prevent abuse
- **Event Handling**: Proper handling of group events

### Negative
- **Complexity**: Topic-based routing adds complexity
- **Maintenance**: Need to maintain command parsing logic
- **Telegram API Changes**: Dependent on Telegram API (may change)

### Neutral
- **Command Format**: Strict format may be less flexible
- **Topic Requirement**: Requires forum groups for topic support

## Alternatives Considered

### Process All Messages (Not Just Commands)
- **Rejected**: Would process too much noise, commands are clearer

### No Topic-Based Routing
- **Rejected**: Requirement is topic-based organization

### Editable Matches (Not Immutable)
- **Rejected**: Immutability preserves audit trail, undo is better approach

### No Undo Time Limit
- **Rejected**: Time limit prevents abuse

### Automatic Player Registration on Join
- **Rejected**: Requirement is self-registration

### Hard Delete on Member Leave
- **Rejected**: Soft delete preserves history (see ADR-002)

### No Group Event Handling
- **Rejected**: Need to handle bot removal and group migration

### More Flexible Command Format
- **Considered**: Allow variations in command format
- **Decision**: Strict format is clearer and easier to parse

