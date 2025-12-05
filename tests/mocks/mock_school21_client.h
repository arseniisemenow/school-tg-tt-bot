#ifndef TESTS_MOCKS_MOCK_SCHOOL21_CLIENT_H
#define TESTS_MOCKS_MOCK_SCHOOL21_CLIENT_H

#include "school21/api_client.h"
#include <unordered_map>
#include <string>
#include <optional>

namespace test_mocks {

/**
 * Mock School21 API Client for testing
 * Allows configuring responses for different nicknames
 */
class MockSchool21Client : public school21::ApiClient {
 public:
  MockSchool21Client();
  ~MockSchool21Client() = default;
  
  // Override methods (note: base class uses 'login' parameter name)
  std::optional<school21::Participant> getParticipant(const std::string& login) override;
  bool verifyParticipant(const std::string& login) override;
  
  // Test configuration methods
  void addParticipant(const std::string& login, const school21::Participant& participant);
  void removeParticipant(const std::string& login);
  void clear();
  
  // Configure default behavior
  void setDefaultActive(bool active) { default_active_ = active; }
  void setDefaultExists(bool exists) { default_exists_ = exists; }

 private:
  std::unordered_map<std::string, school21::Participant> participants_;
  bool default_active_ = true;
  bool default_exists_ = true;
};

}  // namespace test_mocks

#endif  // TESTS_MOCKS_MOCK_SCHOOL21_CLIENT_H

