#ifndef MODELS_PLAYER_H
#define MODELS_PLAYER_H

#include <cstdint>
#include <string>
#include <optional>
#include <chrono>

namespace models {

struct Player {
  int64_t id = 0;
  int64_t telegram_user_id = 0;
  std::optional<std::string> school_nickname;
  bool is_verified_student = false;
  bool is_allowed_non_student = false;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
  std::optional<std::chrono::system_clock::time_point> deleted_at;
};

struct GroupPlayer {
  int64_t id = 0;
  int64_t group_id = 0;
  int64_t player_id = 0;
  int current_elo = 1500;
  int matches_played = 0;
  int matches_won = 0;
  int matches_lost = 0;
  int version = 0;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
};

}  // namespace models

#endif  // MODELS_PLAYER_H

