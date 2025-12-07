#include "bot/webhook_server.h"
#include <sstream>
#include <algorithm>
#include <poll.h>
#include <fcntl.h>

namespace bot {

WebhookServer::WebhookServer() = default;

WebhookServer::~WebhookServer() {
  stop();
}

void WebhookServer::configure(const Config& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_ = config;
}

void WebhookServer::setUpdateCallback(UpdateCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = std::move(callback);
}

bool WebhookServer::start() {
  if (running_.load()) {
    return true;  // Already running
  }
  
  // Create socket
  server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_ < 0) {
    return false;
  }
  
  // Allow socket reuse
  int opt = 1;
  if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    close(server_socket_);
    server_socket_ = -1;
    return false;
  }
  
  // Bind to address
  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(config_.port);
  
  if (config_.bind_address == "0.0.0.0") {
    server_addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, config_.bind_address.c_str(), &server_addr.sin_addr) <= 0) {
      close(server_socket_);
      server_socket_ = -1;
      return false;
    }
  }
  
  if (bind(server_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
    close(server_socket_);
    server_socket_ = -1;
    return false;
  }
  
  // Start listening
  if (listen(server_socket_, config_.backlog) < 0) {
    close(server_socket_);
    server_socket_ = -1;
    return false;
  }
  
  // Start server thread
  running_.store(true);
  server_thread_ = std::thread(&WebhookServer::serverLoop, this);
  
  return true;
}

void WebhookServer::stop() {
  if (!running_.load()) {
    return;
  }
  
  running_.store(false);
  
  // Close server socket to unblock accept()
  if (server_socket_ >= 0) {
    shutdown(server_socket_, SHUT_RDWR);
    close(server_socket_);
    server_socket_ = -1;
  }
  
  // Wait for server thread to finish
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}

void WebhookServer::serverLoop() {
  // Set server socket to non-blocking for poll
  int flags = fcntl(server_socket_, F_GETFL, 0);
  fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK);
  
  while (running_.load()) {
    struct pollfd pfd;
    pfd.fd = server_socket_;
    pfd.events = POLLIN;
    
    // Poll with timeout to allow checking running_ flag
    int poll_result = poll(&pfd, 1, 1000);  // 1 second timeout
    
    if (poll_result < 0) {
      if (errno == EINTR) continue;  // Interrupted, retry
      break;  // Error
    }
    
    if (poll_result == 0) {
      continue;  // Timeout, check running_ and retry
    }
    
    if (!(pfd.revents & POLLIN)) {
      continue;
    }
    
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    
    if (client_socket < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;  // No connection available
      }
      continue;  // Other error, continue serving
    }
    
    // Set socket timeout for reads/writes
    struct timeval timeout;
    timeout.tv_sec = config_.socket_timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Handle the client (synchronously for simplicity)
    handleClient(client_socket);
    
    close(client_socket);
  }
}

void WebhookServer::handleClient(int client_socket) {
  HttpRequest request = parseRequest(client_socket);
  
  if (!request.valid) {
    sendResponse(client_socket, 400, "Bad Request");
    return;
  }
  
  // Verify method is POST
  if (request.method != "POST") {
    sendResponse(client_socket, 405, "Method Not Allowed");
    return;
  }
  
  // Validate path matches configured webhook path
  std::string expected_path;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    expected_path = config_.path;
  }
  
  // Normalize paths: remove trailing slashes and ensure leading slash
  std::string request_path = request.path;
  if (!request_path.empty() && request_path.back() == '/') {
    request_path.pop_back();
  }
  if (!request_path.empty() && request_path.front() != '/') {
    request_path = "/" + request_path;
  }
  
  std::string normalized_expected = expected_path;
  if (!normalized_expected.empty() && normalized_expected.back() == '/') {
    normalized_expected.pop_back();
  }
  if (!normalized_expected.empty() && normalized_expected.front() != '/') {
    normalized_expected = "/" + normalized_expected;
  }
  
  if (request_path != normalized_expected) {
    sendResponse(client_socket, 404, "Not Found");
    return;
  }
  
  // Verify content type is JSON
  if (request.content_type.find("application/json") == std::string::npos) {
    sendResponse(client_socket, 415, "Unsupported Media Type");
    return;
  }
  
  // Verify secret token if configured
  if (!config_.secret_token.empty()) {
    if (request.secret_token != config_.secret_token) {
      sendResponse(client_socket, 403, "Forbidden");
      return;
    }
  }
  
  // Process the update
  UpdateCallback callback;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    callback = callback_;
  }
  
  if (callback) {
    bool success = callback(request.body);
    if (success) {
      sendResponse(client_socket, 200, "OK");
    } else {
      // Still return 200 to Telegram, but log failure internally
      sendResponse(client_socket, 200, "OK");
    }
  } else {
    sendResponse(client_socket, 200, "OK");
  }
}

WebhookServer::HttpRequest WebhookServer::parseRequest(int client_socket) {
  HttpRequest request;
  
  // Read headers
  std::string headers = readHeaders(client_socket);
  if (headers.empty()) {
    return request;
  }
  
  // Parse request line
  size_t first_line_end = headers.find("\r\n");
  if (first_line_end == std::string::npos) {
    return request;
  }
  
  std::string request_line = headers.substr(0, first_line_end);
  std::istringstream iss(request_line);
  std::string version;
  iss >> request.method >> request.path >> version;
  
  if (request.method.empty() || request.path.empty()) {
    return request;
  }
  
  // Parse headers
  size_t content_length = 0;
  std::string header_section = headers.substr(first_line_end + 2);
  std::istringstream header_stream(header_section);
  std::string line;
  
  while (std::getline(header_stream, line)) {
    if (line.empty() || line == "\r") break;
    
    // Remove trailing \r if present
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    
    // Trim whitespace from value
    size_t start = value.find_first_not_of(" \t");
    if (start != std::string::npos) {
      value = value.substr(start);
    }
    
    // Convert header name to lowercase for comparison
    std::string name_lower = name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    if (name_lower == "content-length") {
      content_length = std::stoull(value);
    } else if (name_lower == "content-type") {
      request.content_type = value;
    } else if (name_lower == "x-telegram-bot-api-secret-token") {
      request.secret_token = value;
    }
  }
  
  // Validate content length
  if (content_length > static_cast<size_t>(config_.max_body_size)) {
    return request;  // Body too large
  }
  
  // Read body if present
  if (content_length > 0) {
    request.body = readBody(client_socket, content_length);
    if (request.body.size() != content_length) {
      return request;  // Failed to read complete body
    }
  }
  
  request.valid = true;
  return request;
}

std::string WebhookServer::readHeaders(int client_socket) {
  std::string headers;
  char buffer[4096];
  
  while (true) {
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
      break;  // Error or connection closed
    }
    
    buffer[bytes_read] = '\0';
    headers += buffer;
    
    // Check if we've received all headers (\r\n\r\n marks end of headers)
    if (headers.find("\r\n\r\n") != std::string::npos) {
      // Return only the headers part
      size_t end = headers.find("\r\n\r\n");
      return headers.substr(0, end + 4);
    }
    
    // Safety limit
    if (headers.size() > 8192) {
      break;  // Headers too large
    }
  }
  
  return "";
}

std::string WebhookServer::readBody(int client_socket, size_t content_length) {
  std::string body;
  body.reserve(content_length);
  
  char buffer[4096];
  size_t remaining = content_length;
  
  while (remaining > 0) {
    size_t to_read = std::min(remaining, sizeof(buffer));
    ssize_t bytes_read = recv(client_socket, buffer, to_read, 0);
    
    if (bytes_read <= 0) {
      break;  // Error or connection closed
    }
    
    body.append(buffer, bytes_read);
    remaining -= bytes_read;
  }
  
  return body;
}

void WebhookServer::sendResponse(int client_socket, int status_code, const std::string& body) {
  std::string status_text;
  switch (status_code) {
    case 200: status_text = "OK"; break;
    case 400: status_text = "Bad Request"; break;
    case 403: status_text = "Forbidden"; break;
    case 404: status_text = "Not Found"; break;
    case 405: status_text = "Method Not Allowed"; break;
    case 415: status_text = "Unsupported Media Type"; break;
    case 500: status_text = "Internal Server Error"; break;
    default: status_text = "Unknown"; break;
  }
  
  std::ostringstream response;
  response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
  response << "Content-Type: text/plain\r\n";
  response << "Content-Length: " << body.size() << "\r\n";
  response << "Connection: close\r\n";
  response << "\r\n";
  response << body;
  
  std::string response_str = response.str();
  send(client_socket, response_str.c_str(), response_str.size(), 0);
}

}  // namespace bot

