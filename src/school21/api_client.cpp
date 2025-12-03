#include "school21/api_client.h"

#include <stdexcept>
#include <chrono>
#include <thread>
#include <mutex>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace school21 {

ApiClient::ApiClient(const Config& config) : config_(config) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

ApiClient::~ApiClient() {
  curl_global_cleanup();
}

bool ApiClient::isTokenValid() const {
  if (!token_) {
    return false;
  }
  
  auto now = std::chrono::system_clock::now();
  // Refresh if expires in less than 5 minutes
  auto refresh_time = token_->expires_at - std::chrono::minutes(5);
  return now < refresh_time;
}

std::string ApiClient::getAccessToken() {
  std::lock_guard<std::mutex> lock(token_mutex_);
  
  if (isTokenValid()) {
    return token_->access_token;
  }
  
  // Need to authenticate or refresh
  if (token_ && token_->refresh_token.empty() == false) {
    try {
      *token_ = refreshToken(token_->refresh_token);
      return token_->access_token;
    } catch (const std::exception&) {
      // Refresh failed, re-authenticate
    }
  }
  
  *token_ = authenticate();
  return token_->access_token;
}

ApiClient::Token ApiClient::authenticate() {
  std::string auth_url = "https://auth.21-school.ru/auth/realms/EduPowerKeycloak/protocol/openid-connect/token";
  
  std::string body = "client_id=" + config_.client_id +
                    "&username=" + config_.username +
                    "&password=" + config_.password +
                    "&grant_type=password";
  
  std::string response = httpPost(auth_url, body);
  
  try {
    auto json = nlohmann::json::parse(response);
    Token token;
    token.access_token = json["access_token"].get<std::string>();
    token.refresh_token = json.value("refresh_token", std::string());
    
    int expires_in = json.value("expires_in", 3600);
    token.expires_at = std::chrono::system_clock::now() + 
                      std::chrono::seconds(expires_in);
    
    return token;
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to parse auth response: " + 
                            std::string(e.what()));
  }
}

ApiClient::Token ApiClient::refreshToken(const std::string& refresh_token) {
  // TODO: Implement token refresh
  throw std::runtime_error("Token refresh not implemented");
}

std::optional<Participant> ApiClient::getParticipant(const std::string& login) {
  try {
    std::string token = getAccessToken();
    std::string url = config_.base_url + "/v1/participants/" + login;
    
    std::string response = httpGet(url, token);
    
    auto json = nlohmann::json::parse(response);
    Participant participant;
    participant.login = json["login"].get<std::string>();
    participant.status = json["status"].get<std::string>();
    participant.class_name = json.value("className", std::string());
    participant.parallel_name = json.value("parallelName", std::string());
    
    return participant;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool ApiClient::verifyParticipant(const std::string& login) {
  auto participant = getParticipant(login);
  if (!participant) {
    return false;
  }
  return participant->status == "ACTIVE";
}

// Simple HTTP client implementation (using libcurl)
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, 
                            std::string* data) {
  size_t total_size = size * nmemb;
  data->append((char*)contents, total_size);
  return total_size;
}

std::string ApiClient::httpGet(const std::string& url, 
                               const std::string& token) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }
  
  std::string response_data;
  
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);
  
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (!token.empty()) {
    std::string auth_header = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, auth_header.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  
  if (res != CURLE_OK) {
    throw std::runtime_error("HTTP GET failed: " + 
                            std::string(curl_easy_strerror(res)));
  }
  
  return response_data;
}

std::string ApiClient::httpPost(const std::string& url, 
                                const std::string& body,
                                const std::string& token) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }
  
  std::string response_data;
  
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);
  
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  if (!token.empty()) {
    std::string auth_header = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, auth_header.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  
  if (res != CURLE_OK) {
    throw std::runtime_error("HTTP POST failed: " + 
                            std::string(curl_easy_strerror(res)));
  }
  
  return response_data;
}

}  // namespace school21

