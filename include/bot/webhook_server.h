#ifndef BOT_WEBHOOK_SERVER_H
#define BOT_WEBHOOK_SERVER_H

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <vector>

namespace bot {

// Lightweight HTTP server for receiving Telegram webhook updates
// Uses POSIX sockets for maximum portability and minimal dependencies
class WebhookServer {
 public:
  // Callback type for processing incoming webhook requests
  // Returns true if the update was processed successfully
  using UpdateCallback = std::function<bool(const std::string& json_body)>;
  
  // Configuration for the webhook server
  struct Config {
    int port = 8080;                          // Port to listen on
    std::string bind_address = "0.0.0.0";     // Address to bind to
    std::string secret_token;                  // Secret token for validation (X-Telegram-Bot-Api-Secret-Token)
    int backlog = 10;                          // Connection queue size
    int max_body_size = 1024 * 1024;           // Maximum request body size (1MB)
    int socket_timeout_seconds = 30;           // Socket read/write timeout
  };
  
  WebhookServer();
  ~WebhookServer();
  
  // Non-copyable, non-movable
  WebhookServer(const WebhookServer&) = delete;
  WebhookServer& operator=(const WebhookServer&) = delete;
  WebhookServer(WebhookServer&&) = delete;
  WebhookServer& operator=(WebhookServer&&) = delete;
  
  // Set configuration
  void configure(const Config& config);
  
  // Set callback for processing updates
  void setUpdateCallback(UpdateCallback callback);
  
  // Start the server (non-blocking - runs in a separate thread)
  // Returns true if server started successfully
  bool start();
  
  // Stop the server
  void stop();
  
  // Check if server is running
  bool isRunning() const { return running_.load(); }
  
  // Get the port the server is listening on
  int getPort() const { return config_.port; }
  
 private:
  // Server loop (runs in separate thread)
  void serverLoop();
  
  // Handle a single client connection
  void handleClient(int client_socket);
  
  // Parse HTTP request and extract body
  struct HttpRequest {
    std::string method;
    std::string path;
    std::string content_type;
    std::string secret_token;
    std::string body;
    bool valid = false;
  };
  HttpRequest parseRequest(int client_socket);
  
  // Send HTTP response
  void sendResponse(int client_socket, int status_code, const std::string& body = "");
  
  // Read from socket until \r\n\r\n (end of headers)
  std::string readHeaders(int client_socket);
  
  // Read exact number of bytes from socket
  std::string readBody(int client_socket, size_t content_length);
  
  Config config_;
  UpdateCallback callback_;
  
  std::atomic<bool> running_{false};
  std::thread server_thread_;
  int server_socket_ = -1;
  mutable std::mutex mutex_;
};

}  // namespace bot

#endif  // BOT_WEBHOOK_SERVER_H

