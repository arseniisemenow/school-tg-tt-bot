#include "mock_school21_client.h"

namespace test_mocks {

MockSchool21Client::MockSchool21Client()
    : school21::ApiClient(school21::ApiClient::Config{
          .base_url = "http://mock",
          .username = "test",
          .password = "test",
          .client_id = "test"
      }) {
}

std::optional<school21::Participant> MockSchool21Client::getParticipant(const std::string& login) {
  auto it = participants_.find(login);
  if (it != participants_.end()) {
    return it->second;
  }
  
  // If default behavior is enabled and participant not found, return default
  if (default_exists_) {
    school21::Participant p;
    p.login = login;
    p.status = default_active_ ? "ACTIVE" : "EXPELLED";
    return p;
  }
  
  return std::nullopt;
}

bool MockSchool21Client::verifyParticipant(const std::string& login) {
  auto participant = getParticipant(login);
  if (!participant.has_value()) {
    return false;
  }
  return participant->status == "ACTIVE";
}

void MockSchool21Client::addParticipant(const std::string& login, const school21::Participant& participant) {
  participants_[login] = participant;
}

void MockSchool21Client::removeParticipant(const std::string& login) {
  participants_.erase(login);
}

void MockSchool21Client::clear() {
  participants_.clear();
}

}  // namespace test_mocks

