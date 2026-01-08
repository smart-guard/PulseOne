#include "Drivers/Ros/ROSDriver.h"
#include "Logging/LogManager.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace PulseOne {
namespace Drivers {

using namespace PulseOne::Structs;
using namespace PulseOne::Enums;
using json = nlohmann::json;

ROSDriver::ROSDriver() : socket_fd_(-1) {
    status_ = DriverStatus::UNINITIALIZED;
}

ROSDriver::~ROSDriver() {
    Stop();
}

bool ROSDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    robot_ip_ = config.endpoint; // Expecting IP address
    
    // Parse port from properties or default
    if (config.properties.find("port") != config.properties.end()) {
        try {
            robot_port_ = std::stoi(config.properties.at("port"));
        } catch (...) {
            robot_port_ = 9090;
        }
    }
    
    LogManager::getInstance().Info("ROS Driver Initialized. Target: " + robot_ip_ + ":" + std::to_string(robot_port_));
    UpdateStatus(DriverStatus::STOPPED);
    return true;
}

bool ROSDriver::Connect() {
    return OpenSocket();
}

bool ROSDriver::Disconnect() {
    CloseSocket();
    UpdateStatus(DriverStatus::STOPPED);
    return true;
}

bool ROSDriver::IsConnected() const {
    return is_connected_.load();
}

bool ROSDriver::Start() {
    if (is_running_.load()) {
        if (is_connected_.load()) return true;
        // If running but not connected, Stop and restart
        Stop();
    }

    if (!Connect()) return false;
    
    is_running_ = true;
    if (receive_thread_ && receive_thread_->joinable()) {
        receive_thread_->join();
    }
    receive_thread_ = std::make_unique<std::thread>(&ROSDriver::ReceiveLoop, this);
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
                    if (receive_buffer_[i] == '{') brace_count++;
                    else if (receive_buffer_[i] == '}') brace_count--;
                    
                    if (brace_count == 0) {
                        close_pos = i;
                        break;
                    }
                }
                
                if (close_pos != std::string::npos) {
                    // Extract Candidate
                    std::string potential_json = receive_buffer_.substr(open_pos, close_pos - open_pos + 1);
                    
                    try {
                        auto j = json::parse(potential_json); // Verification
                        
                        // Process Message
                        if (j.contains("op") && j["op"] == "publish") {
                            std::string topic = j.value("topic", "");
                            if (j.contains("msg")) {
                                std::lock_guard<std::mutex> lock(callback_mutex_);
                                if (message_callback_) {
                                    message_callback_(topic, j["msg"]);
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
             if (errno != EAGAIN && errno != EWOULDBLOCK) {
                 LogManager::getInstance().Error("Socket Error: " + std::string(strerror(errno)));
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
    if (socket_fd_ >= 0) close(socket_fd_);
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        SetError("Failed to create socket", PulseOne::Structs::ErrorCode::CONNECTION_FAILED);
        return false;
    }
    
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(robot_port_);
    if (inet_pton(AF_INET, robot_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        SetError("Invalid IP Address", PulseOne::Structs::ErrorCode::CONFIGURATION_ERROR);
        return false;
    }
    
    if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        SetError("Connection Refused", PulseOne::Structs::ErrorCode::CONNECTION_FAILED);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    is_connected_ = true;
    LogManager::getInstance().Info("Connected to ROS Bridge at " + robot_ip_);
    statistics_.successful_connections++;
    return true;
}

void ROSDriver::CloseSocket() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    is_connected_ = false;
}

bool ROSDriver::SendJson(const json& j) {
    if (!is_connected_) return false;
    
    std::string msg = j.dump();
    std::lock_guard<std::mutex> lock(socket_mutex_);
    ssize_t sent = send(socket_fd_, msg.c_str(), msg.length(), 0);
    return (sent == (ssize_t)msg.length());
}

bool ROSDriver::Subscribe(const std::string& topic) {
    json j;
    j["op"] = "subscribe";
    j["topic"] = topic;
    return SendJson(j);
}

void ROSDriver::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = callback;
}

// Stubs for interface
bool ROSDriver::ReadValues(const std::vector<DataPoint>& points, std::vector<TimestampedValue>& values) { return true; }
bool ROSDriver::WriteValue(const DataPoint& point, const DataValue& value) { return true; } // To implement Publish logic later
PulseOne::Enums::ProtocolType ROSDriver::GetProtocolType() const { return PulseOne::Enums::ProtocolType::ROS_BRIDGE; }
void ROSDriver::UpdateStatus(DriverStatus status) { status_ = status; }
void ROSDriver::SetError(const std::string& message, PulseOne::Structs::ErrorCode code) { last_error_ = {code, message}; }
PulseOne::Structs::DriverStatus ROSDriver::GetStatus() const { return status_; }
PulseOne::Structs::ErrorInfo ROSDriver::GetLastError() const { return last_error_; }

}
}
