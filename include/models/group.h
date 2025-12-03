#ifndef MODELS_GROUP_H
#define MODELS_GROUP_H

#include <cstdint>
#include <string>
#include <chrono>
#include <optional>

namespace models {

struct Group {
  int64_t id = 0;
  int64_t telegram_group_id = 0;
  std::optional<std::string> name;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
  bool is_active = true;
};

struct GroupTopic {
  int64_t id = 0;
  int64_t group_id = 0;
  std::optional<int> telegram_topic_id;
  std::string topic_type;  // 'id', 'ranking', 'matches', 'logs'
  bool is_active = true;
  std::chrono::system_clock::time_point created_at;
};

}  // namespace models

#endif  // MODELS_GROUP_H

