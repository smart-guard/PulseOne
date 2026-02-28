#include "Drivers/Ros/ROSDriver.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"

#if PULSEONE_LINUX
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <cstring>
#include <iostream>

using namespace PulseOne::Platform;

namespace PulseOne {
namespace Drivers {

using namespace PulseOne::Structs;
using namespace PulseOne::Enums;
using json = nlohmann::json;

ROSDriver::ROSDriver() : socket_fd_(-1) {
  status_ = DriverStatus::UNINITIALIZED;
}

ROSDriver::~ROSDriver() { Stop(); }

bool ROSDriver::Initialize(const DriverConfig &config) {
  config_ = config;
  std::string endpoint = config.endpoint;
  size_t colon_pos = endpoint.find(':');
  if (colon_pos != std::string::npos) {
    robot_ip_ = endpoint.substr(0, colon_pos);
    try {
      robot_port_ = std::stoi(endpoint.substr(colon_pos + 1));
    } catch (...) {
      robot_port_ = 9090;
    }
  } else {
    robot_ip_ = endpoint;
    // Parse port from properties or default
    if (config.properties.count("port")) {
      try {
        robot_port_ = std::stoi(config.properties.at("port"));
      } catch (...) {
        robot_port_ = 9090;
      }
    } else {
      robot_port_ = 9090;
    }
  }

  LogManager::getInstance().Info("ROS Driver Initialized. Target: " +
                                 robot_ip_ + ":" + std::to_string(robot_port_));
  UpdateStatus(DriverStatus::STOPPED);
  return true;
}

bool ROSDriver::Connect() { return OpenSocket(); }

bool ROSDriver::Disconnect() {
  CloseSocket();
  UpdateStatus(DriverStatus::STOPPED);
  return true;
}

bool ROSDriver::IsConnected() const { return is_connected_.load(); }

bool ROSDriver::Start() {
  if (is_running_.load()) {
    if (is_connected_.load())
      return true;
    // If running but not connected, Stop and restart
    Stop();
  }

  if (!Connect())
    return false;

  is_running_ = true;
  if (receive_thread_ && receive_thread_->joinable()) {
    receive_thread_->join();
  }
  receive_thread_ =
      std::make_unique<std::thread>(&ROSDriver::ReceiveLoop, this);
  UpdateStatus(DriverStatus::RUNNING);
  return true;
}

bool ROSDriver::Stop() {
  is_running_ = false;
  Disconnect();
  if (receive_thread_ && receive_thread_->joinable()) {
    receive_thread_->join();
  }
  return true;
}

void ROSDriver::ReceiveLoop() {
  LogManager::getInstance().Info("ROS Receive Loop Started");

  const int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];

  while (is_running_.load()) {
    if (!is_connected_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      // Try reconnect?
      continue;
    }

    int bytes_read = recv(socket_fd_, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      receive_buffer_.append(buffer);

      // Simple Parsing Logic: Try to find complete JSON objects
      // This is a naive implementation; production might need a robust parser
      size_t open_pos = receive_buffer_.find('{');
      while (open_pos != std::string::npos) {
        // Brute force check for matching brace
        // This assumes rosbridge sends valid JSON

        int brace_count = 0;
        size_t close_pos = std::string::npos;

        for (size_t i = open_pos; i < receive_buffer_.length(); ++i) {
          if (receive_buffer_[i] == '{')
            brace_count++;
          else if (receive_buffer_[i] == '}')
            brace_count--;

          if (brace_count == 0) {
            close_pos = i;
            break;
          }
        }

        if (close_pos != std::string::npos) {
          // Extract Candidate
          std::string potential_json =
              receive_buffer_.substr(open_pos, close_pos - open_pos + 1);

          try {
            auto j = json::parse(potential_json); // Verification

            // Process Message
            if (j.contains("op") && j["op"] == "publish") {
              std::string topic = j.value("topic", "");
              if (j.contains("msg")) {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (message_callback_) {
                  message_callback_(topic, j["msg"].dump());
                }
              }
            }

            // Remove processed part
            receive_buffer_.erase(0, close_pos + 1);
            open_pos = receive_buffer_.find('{');

          } catch (...) {
            // Malformed? Move past this valid brace but invalid json
            receive_buffer_.erase(0, close_pos + 1);
            open_pos = receive_buffer_.find('{');
          }
        } else {
          // Incomplete message, wait for more data
          break;
        }
      }

    } else if (bytes_read == 0) {
      LogManager::getInstance().Warn("ROS Server closed connection");
      is_connected_ = false;
      break;
    } else {
      // Error
#if defined(_WIN32)
      int err = WSAGetLastError();
      if (err != WSAEWOULDBLOCK && err != WSAETIMEDOUT) {
#else
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
        LogManager::getInstance().Error("Socket Error: " +
                                        std::string(strerror(errno)));
        is_connected_ = false;
        break;
      }
    }
  }

  is_running_ = false;
  is_connected_ = false;
  LogManager::getInstance().Info("ROS Receive Loop Stopped");
}

bool ROSDriver::OpenSocket() {
  std::lock_guard<std::mutex> lock(socket_mutex_);
  if (socket_fd_ >= 0)
    Socket::CloseSocket(socket_fd_);

  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    SetError("Failed to create socket",
             PulseOne::Structs::ErrorCode::CONNECTION_FAILED);
    return false;
  }

  // [CRITICAL FIX] ReceiveLoop 스레드 Deadlock 방지 (recv 무한 블로킹 해제)
#if PULSEONE_LINUX
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
#elif defined(_WIN32)
  DWORD timeout = 1000;
  setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
             sizeof timeout);
#endif

  struct addrinfo hints, *res;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(robot_port_);
  if (getaddrinfo(robot_ip_.c_str(), port_str.c_str(), &hints, &res) != 0) {
    SetError("Failed to resolve hostname: " + robot_ip_,
             PulseOne::Structs::ErrorCode::CONFIGURATION_ERROR);
    return false;
  }

  if (connect(socket_fd_, res->ai_addr, res->ai_addrlen) < 0) {
    SetError("Connection Refused to " + robot_ip_ + ":" + port_str,
             PulseOne::Structs::ErrorCode::CONNECTION_FAILED);
    freeaddrinfo(res);
    Socket::CloseSocket(socket_fd_);
    socket_fd_ = -1;
    return false;
  }
  freeaddrinfo(res);

  is_connected_ = true;
  LogManager::getInstance().Info("Connected to ROS Bridge at " + robot_ip_);
  statistics_.successful_connections++;
  return true;
}

void ROSDriver::CloseSocket() {
  std::lock_guard<std::mutex> lock(socket_mutex_);
  if (socket_fd_ >= 0) {
    Socket::CloseSocket(socket_fd_);
    socket_fd_ = -1;
  }
  is_connected_ = false;
}

bool ROSDriver::SendJson(const json &j) {
  if (!is_connected_)
    return false;

  std::string msg = j.dump();
  std::lock_guard<std::mutex> lock(socket_mutex_);
  ssize_t sent = send(socket_fd_, msg.c_str(), msg.length(), 0);
  return (sent == (ssize_t)msg.length());
}

bool ROSDriver::Subscribe(const std::string &topic, int qos) {
  (void)qos;
  json j;
  j["op"] = "subscribe";
  j["topic"] = topic;
  return SendJson(j);
}

// IProtocolDriver::SetMessageCallback is used (defined in base class)

// Stubs for interface
bool ROSDriver::ReadValues(const std::vector<DataPoint> &points,
                           std::vector<TimestampedValue> &values) {
  return true;
}
bool ROSDriver::WriteValue(const DataPoint &point, const DataValue &value) {
  if (!is_connected_)
    return false;

  json j;
  j["op"] = "publish";
  j["topic"] = point.address_string; // Using address_string as the topic

  // Construct message based on value type
  if (std::holds_alternative<double>(value))
    j["msg"] = std::get<double>(value);
  else if (std::holds_alternative<bool>(value))
    j["msg"] = std::get<bool>(value);
  else if (std::holds_alternative<std::string>(value)) {
    // If it's a string, try to parse as JSON if it looks like it
    std::string s = std::get<std::string>(value);
    if (!s.empty() && (s.front() == '{' || s.front() == '[')) {
      try {
        j["msg"] = json::parse(s);
      } catch (...) {
        j["msg"] = s;
      }
    } else {
      j["msg"] = s;
    }
  } else {
    j["msg"] = Utils::DataVariantToString(value);
  }

  LogManager::getInstance().Info("[ROSDriver] Publishing to " +
                                 point.address_string + ": " + j["msg"].dump());
  return SendJson(j);
}
std::string ROSDriver::GetProtocolType() const { return "ROS_BRIDGE"; }
void ROSDriver::UpdateStatus(DriverStatus status) { status_ = status; }
void ROSDriver::SetError(const std::string &message,
                         PulseOne::Structs::ErrorCode code) {
  last_error_ = {code, message};
}
PulseOne::Structs::DriverStatus ROSDriver::GetStatus() const { return status_; }
PulseOne::Structs::ErrorInfo ROSDriver::GetLastError() const {
  return last_error_;
}

// Plugin registration logic moved outside namespace to avoid mangling and stay
// consistent.

} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Entry Point
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void RegisterPlugin() {
  // 1. DB에 프로토콜 정보 자동 등록 (없을 경우)
  try {
    auto &repo_factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();
    if (protocol_repo) {
      if (!protocol_repo->findByType("ROS_BRIDGE").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("ROS_BRIDGE");
        entity.setDisplayName("ROS Bridge");
        entity.setCategory("industrial");
        entity.setDescription(
            "ROS (Robot Operating System) Driver via ROS Bridge");
        protocol_repo->save(entity);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[ROSDriver] DB Registration failed: " << e.what()
              << std::endl;
  }

  // 2. 메모리 Factory에 드라이버 생성자 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "ROS_BRIDGE",
      []() { return std::make_unique<PulseOne::Drivers::ROSDriver>(); });
}

// Legacy wrapper for static linking
void RegisterRosDriver() { RegisterPlugin(); }
}
#endif
