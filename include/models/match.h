#ifndef MODELS_MATCH_H
#define MODELS_MATCH_H

#include <cstdint>
#include <string>
#include <chrono>
#include <optional>

namespace models {

struct Match {
  int64_t id = 0;
  int64_t group_id = 0;
  int64_t player1_id = 0;
  int64_t player2_id = 0;
  int player1_score = 0;
  int player2_score = 0;
  int player1_elo_before = 0;
  int player2_elo_before = 0;
  int player1_elo_after = 0;
  int player2_elo_after = 0;
  std::string idempotency_key;
  int64_t created_by_telegram_user_id = 0;
  std::chrono::system_clock::time_point created_at;
  bool is_undone = false;
  std::optional<std::chrono::system_clock::time_point> undone_at;
  std::optional<int64_t> undone_by_telegram_user_id;
};

struct EloHistory {
  int64_t id = 0;
  std::optional<int64_t> match_id;
  int64_t group_id = 0;
  int64_t player_id = 0;
  int elo_before = 0;
  int elo_after = 0;
  int elo_change = 0;
  std::chrono::system_clock::time_point created_at;
  bool is_undone = false;
};

}  // namespace models

#endif  // MODELS_MATCH_H

