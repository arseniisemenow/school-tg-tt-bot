#ifndef SCHOOL21_API_CLIENT_H
#define SCHOOL21_API_CLIENT_H

#include <string>
#include <optional>
#include <memory>
#include <chrono>
#include <mutex>

namespace school21 {

struct Participant {
  std::string login;
  std::string status;  // ACTIVE, TEMPORARY_BLOCKING, EXPELLED, etc.
  std::optional<std::string> class_name;
  std::optional<std::string> parallel_name;
};

class ApiClient {
 public:
  struct Config {
    std::string base_url;
    std::string username;
    std::string password;
    std::string client_id;
    int timeout_seconds = 10;
    int max_retries = 3;
  };
  
  ApiClient(const Config& config);
  ~ApiClient();
  
  // Get participant by login
  std::optional<Participant> getParticipant(const std::string& login);
  
  // Check if participant exists and is active
  bool verifyParticipant(const std::string& login);

 private:
  Config config_;
  
  struct Token {
    std::string access_token;
    std::string refresh_token;
    std::chrono::system_clock::time_point expires_at;
  };
  
  std::optional<Token> token_;
  std::mutex token_mutex_;
  
  // OAuth2 token management
  Token authenticate();
  Token refreshToken(const std::string& refresh_token);
  bool isTokenValid() const;
  std::string getAccessToken();
  
  // HTTP client methods
  std::string httpGet(const std::string& url, const std::string& token);
  std::string httpPost(const std::string& url, 
                      const std::string& body,
                      const std::string& token = "");
};

}  // namespace school21

#endif  // SCHOOL21_API_CLIENT_H

