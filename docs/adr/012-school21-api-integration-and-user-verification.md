# ADR-012: School21 API Integration and User Verification

## Status
Accepted

## Context
The bot is only for School21 students, so we need to verify that players are actual students. The verification process involves:
- Users providing their School21 nickname in a dedicated topic
- Bot verifying the nickname exists in School21 system
- Associating the Telegram user with the School21 nickname
- Handling non-students and expelled students (with admin override)
- ID topic feature with emoji reactions and auto-deletion

Key requirements:
- Integration with School21 OpenAPI
- OAuth2/OpenID Connect authentication
- Student verification by nickname
- Topic-based nickname association
- Non-student handling with special marking
- ID topic feature with loading/success/failure reactions
- Auto-delete user after 5 minutes if no nickname provided

## Decision
We will implement School21 API integration with the following design:

### School21 API Authentication

#### OAuth2/OpenID Connect Flow
- **Authentication Endpoint**: `https://auth.21-school.ru/auth/realms/EduPowerKeycloak/protocol/openid-connect/token`
- **Client ID**: `s21-open-api`
- **Grant Type**: `password` (Resource Owner Password Credentials)
- **Request Format**: `application/x-www-form-urlencoded`
- **Parameters**:
  - `client_id`: `s21-open-api`
  - `username`: School21 login (from environment variable)
  - `password`: School21 password (from environment variable)
  - `grant_type`: `password`

#### Token Management
- **Token Storage**: In-memory cache with expiration tracking
- **Token Refresh**: 
  - Access token lifetime: Typically 1 hour (check `expires_in` from response)
  - Refresh token: Use refresh token to get new access token
  - Refresh before expiration: Refresh when <5 minutes remaining
- **Token Caching**: Cache access token to avoid frequent authentication
- **Error Handling**: 
  - On 401: Re-authenticate
  - On token expiration: Refresh or re-authenticate
  - Log authentication failures

#### API Base URL
- **Base URL**: `https://platform.21-school.ru/services/21-school/api/v1`
- **Authentication Header**: `Authorization: Bearer <access_token>`
- **Content-Type**: `application/json`

### Student Verification

#### Verification Endpoint
- **Endpoint**: `GET /v1/participants/{login}`
- **Purpose**: Verify if a School21 nickname (login) exists and is active
- **Response**: Participant information including status
- **Status Values**: 
  - `ACTIVE`: Active student (verified)
  - `TEMPORARY_BLOCKING`: Temporarily blocked (can be allowed)
  - `EXPELLED`: Expelled (can be allowed with admin override)
  - `BLOCKED`: Blocked (can be allowed with admin override)
  - `FROZEN`: Frozen (can be allowed with admin override)
  - `STUDY_COMPLETED`: Completed studies (can be allowed)

#### Verification Logic
- **Success Criteria**: 
  - Participant exists (200 response)
  - Status is `ACTIVE` → Mark as verified student
  - Status is non-ACTIVE → Can be allowed with `is_allowed_non_student` flag
- **Failure Cases**:
  - 404 Not Found → Nickname doesn't exist (verification failed)
  - 401 Unauthorized → Token expired/invalid (refresh and retry)
  - 403 Forbidden → Permission denied (log error)
  - 429 Too Many Requests → Rate limited (backoff and retry)
  - 500 Internal Server Error → School21 API error (retry with backoff)

#### Verification Process
1. User sends `/id <school_nickname>` in ID topic
2. Bot adds loading emoji (⏳) to message
3. Bot calls School21 API: `GET /v1/participants/{nickname}`
4. On success:
   - Update `players.school_nickname` and `players.is_verified_student`
   - Add thumbs up emoji (👍) to message
   - Remove loading emoji
5. On failure:
   - Add thumbs down emoji (👎) to message
   - Remove loading emoji
   - Delete user after 5 minutes and his messages (keep ID topic clean)

### School Nickname Association

#### Storage
- **Table**: `players` table (see ADR-002)
- **Fields**:
  - `school_nickname`: VARCHAR(255) - School21 nickname (NULL for guests)
  - `is_verified_student`: BOOLEAN - Verified via API (FALSE for guests)
  - `is_allowed_non_student`: BOOLEAN - TRUE for guests or admin-allowed non-students
- **Association**: 
  - Students: Telegram user ID → School21 nickname
  - Guests: Telegram user ID → NULL (no nickname)
- **Player Types**:
  - **Verified Student**: `is_verified_student = TRUE`, `school_nickname` set, `is_allowed_non_student = FALSE`
  - **Guest**: `is_verified_student = FALSE`, `school_nickname = NULL`, `is_allowed_non_student = TRUE`
  - **Admin-Allowed Non-Student**: `is_verified_student = FALSE`, `school_nickname` set, `is_allowed_non_student = TRUE`

#### Topic-Based Storage
- **Topic Detection**: Use `message.message_thread_id` to identify ID topic
- **Topic Configuration**: Stored in `group_topics` table (topic_type = 'id')
- **Validation**: Only process `/id` and `/id_guest` commands in configured ID topic

#### Nickname Validation
- **Format**: Alphanumeric with hyphens/underscores (typical School21 format)
- **Length**: Reasonable limits (e.g., 3-50 characters)
- **Uniqueness**: One nickname per Telegram user (enforced in application)
- **Case Sensitivity**: School21 logins are case-sensitive, preserve case

### Non-Student Player Handling

#### Guest Registration (`/id_guest`)
- **Command**: `/id_guest` (no parameters)
- **Purpose**: Allow non-students to register without School21 verification
- **Process**:
  1. User sends `/id_guest` in ID topic
  2. Bot immediately marks user as guest:
     - Set `players.is_allowed_non_student = TRUE`
     - Set `players.is_verified_student = FALSE`
     - Set `players.school_nickname = NULL` (no nickname)
  3. Add 👍 emoji to message
  4. User can now participate in matches
- **Use Cases**:
  - Guests who want to play
  - Alumni who are no longer students
  - Non-students invited to play
  - Users who don't have School21 account
- **No API Call**: Guest registration doesn't call School21 API
- **No Verification**: No verification needed for guests

#### Allowed Non-Students (Admin Override)
- **Flag**: `players.is_allowed_non_student` (BOOLEAN)
- **Use Cases**:
  - Expelled students who should still play
  - Students with non-ACTIVE status who want to use `/id` command
- **Setting**: Admin-only operation (via database or admin command)
- **Verification**: Still verify nickname exists via API (even if not ACTIVE)
- **Difference from Guest**: Admin override is for users who tried `/id` but have non-ACTIVE status

#### Status Handling
- **ACTIVE**: Automatically verified, `is_verified_student = TRUE`
- **Non-ACTIVE Statuses**: 
  - If admin allows: `is_allowed_non_student = TRUE`
  - Can still play and participate
  - Marked differently in UI (optional visual indicator)

### ID Topic Feature Implementation

#### Command Formats
- **Student Command**: `/id <school_nickname>`
  - For School21 students
  - Requires API verification
  - Location: Only in ID topic (topic_type = 'id')
- **Guest Command**: `/id_guest`
  - For non-students (guests, alumni, etc.)
  - No API verification required
  - Location: Only in ID topic (topic_type = 'id')
- **Validation**: 
  - Must be in ID topic
  - User must be in group
  - For `/id`: Nickname format validation

#### Emoji Reactions

##### For `/id` Command (Student Verification)
- **Loading State**: 
  - Add ⏳ (hourglass) emoji when verification starts
  - Indicates processing in progress
- **Success State**:
  - Remove ⏳ emoji
  - Add 👍 (thumbs up) emoji
  - Message remains (successful verification)
- **Failure State**:
  - Remove ⏳ emoji
  - Add 👎 (thumbs down) emoji

##### For `/id_guest` Command (Guest Registration)
- **Success State**:
  - Add 👍 (thumbs up) emoji immediately
  - No loading state (instant registration)
  - Message remains
- **No Failure State**: Guest registration always succeeds (no API call)

#### Auto-Deletion Logic
- **Trigger**: Verification failure (404, invalid nickname, etc.)
- **Timing**: Delete message 5 minutes after failure
- **Rationale**: Keep ID topic clean (no failed verification messages)
- **Notification**: 
  - Send message in logs topic: "@user, Your nickname verification failed. Please check your nickname and try again in the ID topic."

#### Message Tracking
- **Storage**: `player_verifications` table (see ADR-002)
- **Fields**:
  - `telegram_message_id`: Track message for deletion
  - `verification_status`: 'pending', 'verified', 'failed', 'expired'
  - `expires_at`: Timestamp for auto-deletion
- **Cleanup**: Background job deletes expired failed messages

### API Rate Limiting

#### School21 API Rate Limits
- **Rate Limit**: Unknown (documented as 429 Too Many Requests)
- **Strategy**: 
  - Implement exponential backoff on 429 responses
  - Respect `Retry-After` header if provided
  - Cache verification results (avoid re-verifying same nickname)
- **Caching**: 
  - Cache successful verifications (TTL: 24 hours)
  - Cache failed verifications (TTL: 1 hour, shorter to allow retry)
  - Invalidate cache on manual admin override

#### Request Throttling
- **Max Concurrent Requests**: Limit concurrent API calls (e.g., 10)
- **Request Queue**: Queue verification requests if rate limited
- **Backoff**: Exponential backoff (1s, 2s, 4s, 8s, 16s, ...)

### Error Handling

#### API Error Responses
- **401 Unauthorized**: 
  - Token expired → Refresh token and retry
  - Invalid credentials → Log error, alert admin
- **403 Forbidden**: 
  - Permission denied → Log error, alert admin
  - May indicate API access revoked
- **404 Not Found**: 
  - Nickname doesn't exist → Verification failed
  - Return failure to user
- **429 Too Many Requests**: 
  - Rate limited → Backoff and retry
  - Queue request if needed
- **500 Internal Server Error**: 
  - School21 API error → Retry with backoff
  - Log error for monitoring

#### Network Errors
- **Timeout**: 
  - Request timeout (e.g., 10 seconds)
  - Retry with backoff
- **Connection Errors**: 
  - Network failures → Retry with backoff
  - Log errors for monitoring

### Verification Workflow

#### Student Verification Flow (`/id` Command)
1. User sends `/id <nickname>` in ID topic
2. Bot validates: command format, topic, user in group
3. Bot adds ⏳ emoji to message
4. Bot checks cache for nickname verification
5. If not cached:
   - Get/refresh School21 API token
   - Call `GET /v1/participants/{nickname}`
   - Handle errors (retry, backoff, etc.)
6. On API success:
   - Check participant status
   - If ACTIVE: Mark as verified student
   - If non-ACTIVE: Check if allowed (admin override)
   - Update database
   - Remove ⏳, add 👍
   - Cache result
7. On API failure:
   - Remove ⏳, add 👎
   - Schedule message deletion (5 minutes)
   - Notify user
   - Cache failure (short TTL)

#### Guest Registration Flow (`/id_guest` Command)
1. User sends `/id_guest` in ID topic
2. Bot validates: topic, user in group
3. Bot immediately:
   - Set `players.is_allowed_non_student = TRUE`
   - Set `players.is_verified_student = FALSE`
   - Set `players.school_nickname = NULL`
   - Create or update player record
4. Add 👍 emoji to message
5. User is now registered as guest and can participate
- **No API Call**: Guest registration doesn't require School21 API
- **No Loading State**: Instant registration
- **No Failure**: Always succeeds (no external dependency)

### Configuration

#### Environment Variables
- `SCHOOL21_API_USERNAME`: School21 login for API access
- `SCHOOL21_API_PASSWORD`: School21 password for API access
- `SCHOOL21_API_CLIENT_ID`: `s21-open-api` (constant)

#### Config File
- `school21.api.base_url`: API base URL (default: production URL)
- `school21.api.timeout`: Request timeout in seconds (default: 10)
- `school21.api.max_retries`: Max retry attempts (default: 3)
- `school21.verification.cache_ttl_success`: Cache TTL for successes (default: 24 hours)
- `school21.verification.cache_ttl_failure`: Cache TTL for failures (default: 1 hour)
- `school21.verification.auto_delete_delay`: Auto-delete delay in minutes (default: 5)

## Consequences

### Positive
- **Student Verification**: Ensures only School21 students (or allowed non-students) can play
- **API Integration**: Proper OAuth2 authentication and token management
- **User Experience**: Clear feedback with emoji reactions
- **Topic Organization**: ID topic keeps verification organized
- **Flexibility**: Guest registration and admin override allow non-students when needed
- **Guest Support**: `/id_guest` provides easy way for non-students to register without API dependency

### Negative
- **External Dependency**: Depends on School21 API availability
- **Rate Limiting**: Subject to School21 API rate limits
- **Complexity**: OAuth2 flow and token management add complexity
- **Credentials**: Need to store School21 API credentials securely

### Neutral
- **API Changes**: School21 API changes may require updates
- **Performance**: API calls add latency to verification

## Alternatives Considered

### No Verification (Allow Anyone)
- **Rejected**: Requirement is bot only for School21 students

### No Auth (Allow Any Nicknames)
- **Rejected**: Complex business logic.

### Manual Verification (Admin Approves)
- **Rejected**: Too much manual work, API verification is automated

### Different Authentication Method
- **Considered**: API keys instead of OAuth2
- **Rejected**: School21 uses OAuth2/OpenID Connect

### No Auto-Deletion of Failed Messages
- **Rejected**: Requirement is to keep ID topic clean

### Longer Cache TTL
- **Considered**: Cache verifications longer
- **Decision**: 24 hours balances freshness with API load

### No Caching
- **Rejected**: Caching reduces API load and improves performance

### Verify on Every Command
- **Rejected**: Too slow, verify once and cache result

### No Guest Registration Command
- **Rejected**: `/id_guest` provides simple way for non-students to register without admin intervention

