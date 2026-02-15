#include "Workers/Protocol/ROSWorker.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>

namespace PulseOne {
namespace Workers {

using namespace PulseOne::Structs;
using namespace PulseOne::Drivers;

ROSWorker::ROSWorker(const DeviceInfo &device_info)
    : BaseDeviceWorker(device_info) {

  // Initialize Driver via Factory
  driver_ = DriverFactory::GetInstance().CreateDriver("ROS");

  if (!driver_) {
    LogManager::getInstance().Error("[ROSWorker] Failed to create ROS driver");
    return;
  }

  // Configure Driver
  DriverConfig config;
  config.endpoint = device_info_.endpoint; // IP
  config.properties = device_info_.driver_config.properties;

  if (driver_->Initialize(config)) {
    driver_->SetMessageCallback(
        [this](const std::string &topic, const std::string &payload) {
          this->HandleRosMessage(topic, payload);
        });
  }

  // Set shorter keep-alive for faster disconnection detection
  ReconnectionSettings settings = GetReconnectionSettings();
  settings.keep_alive_interval_seconds = 2; // Check every 2 seconds
  UpdateReconnectionSettings(settings);
}

std::future<bool> ROSWorker::Start() {
  return std::async(std::launch::async, [this]() {
    if (is_started_)
      return true;

    // Prepare topics before connecting
    DetermineDataPointsFromTopics();

    if (!EstablishConnection()) {
      // Start reconnection thread if initial connection fails
      StartReconnectionThread();
      is_started_ = true; // Mark as started so reconnection thread can work
      return true;
    }

    is_started_ = true;
    StartReconnectionThread();

    LogManager::getInstance().Info("[ROSWorker] Started and connected");
    return true;
  });
}

std::future<bool> ROSWorker::Stop() {
  return std::async(std::launch::async, [this]() {
    is_started_ = false;
    current_state_ = WorkerState::STOPPED;
    if (driver_)
      driver_->Stop();

    // Clear mapping
    std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
    topic_to_points_.clear();
    subscribed_topics_.clear();

    return true;
  });
}

bool ROSWorker::EstablishConnection() {
  if (!driver_)
    return false;

  LogManager::getInstance().Info("[ROSWorker] Establishing connection...");
  bool connected = driver_->Start();

  if (connected) {
    current_state_ = WorkerState::RUNNING;
    SetConnectionState(true);

    // Re-subscribe to all topics on every successful connection
    for (const auto &topic : subscribed_topics_) {
      driver_->Subscribe(topic);
      LogManager::getInstance().Info("[ROSWorker] Subscribed to " + topic);
    }
  } else {
    SetConnectionState(false);
  }

  return connected;
}

bool ROSWorker::CloseConnection() {
  if (driver_)
    driver_->Stop();
  current_state_ = WorkerState::STOPPED;
  return true;
}

bool ROSWorker::CheckConnection() {
  bool driver_connected = driver_ && driver_->IsConnected();
  if (!driver_connected && is_connected_.load()) {
    HandleConnectionError("Driver connection lost");
  } else if (driver_connected) {
    // Data Watchdog: If no data flow for 60 seconds, log warning
    auto now = std::chrono::system_clock::now();
    if (last_success_time_ != std::chrono::system_clock::time_point()) {
      auto diff = std::chrono::duration_cast<std::chrono::seconds>(
                      now - last_success_time_)
                      .count();
      if (diff > 60) {
        LogMessage(LogLevel::WARN,
                   "Data Watchdog: No data received from ROS Bridge for " +
                       std::to_string(diff) + " seconds");
      }
    }
  }
  return driver_connected;
}

void ROSWorker::HandleRosMessage(const std::string &topic,
                                 const std::string &payload) {
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(payload);
  } catch (...) {
    return;
  }

  // 🔥 ROS Bridge 표준 처리: "op": "publish"인 경우 내부 "msg" 필드를 실제
  // 데이터로 사용
  nlohmann::json msg = root;
  if (root.contains("op") && root["op"] == "publish" && root.contains("msg")) {
    msg = root["msg"];
  }

  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);

  // 토픽 매칭 강화: 선행 슬래시 유무와 상관없이 매칭되도록 처리
  std::string normalized_topic = topic;
  auto it = topic_to_points_.find(normalized_topic);

  if (it == topic_to_points_.end()) {
    if (!normalized_topic.empty() && normalized_topic[0] == '/') {
      normalized_topic = normalized_topic.substr(1);
    } else {
      normalized_topic = "/" + normalized_topic;
    }
    it = topic_to_points_.find(normalized_topic);
  }

  if (it == topic_to_points_.end())
    return;

  std::vector<PulseOne::Structs::TimestampedValue> points_to_send;
  auto now = std::chrono::system_clock::now();

  // 1. Existing Mappings Processing
  for (const auto &point_id_str : it->second) {
    const PulseOne::Structs::DataPoint *target_dp = nullptr;
    for (const auto &dp : data_points_) {
      if (dp.id == point_id_str) {
        target_dp = &dp;
        break;
      }
    }
    auto &point = const_cast<PulseOne::Structs::DataPoint &>(*target_dp);
    PulseOne::BasicTypes::DataVariant extracted_value;
    bool value_found = false;

    if (!point.mapping_key.empty()) {
      try {
        std::string path = point.mapping_key;
        if (!path.empty() && path.front() != '/') {
          std::replace(path.begin(), path.end(), '.', '/');
          if (path.front() != '/')
            path = "/" + path;
        }
        nlohmann::json::json_pointer ptr(path);
        if (msg.contains(ptr)) {
          const auto &json_val = msg.at(ptr);
          LogManager::getInstance().Info("[ROSWorker] Extracted value for " +
                                         point.name + ": " + json_val.dump());
          if (json_val.is_number())
            extracted_value = json_val.get<double>();
          else if (json_val.is_boolean())
            extracted_value = json_val.get<bool>();
          else if (json_val.is_string())
            extracted_value = json_val.get<std::string>();
          else
            extracted_value = json_val.dump();
          value_found = true;
        }
      } catch (...) {
      }
    }

    if (!value_found) {
      if (topic.find("battery") != std::string::npos) {
        if (msg.contains("percentage")) {
          extracted_value = msg["percentage"].get<double>();
          value_found = true;
        } else if (msg.contains("charge")) {
          extracted_value = msg["charge"].get<double>();
          value_found = true;
        }
      } else if (topic.find("odom") != std::string::npos) {
        if (msg.contains("pose") && msg["pose"].contains("pose") &&
            msg["pose"]["pose"].contains("position")) {
          auto pos = msg["pose"]["pose"]["position"];
          if (point.name.find("X") != std::string::npos) {
            extracted_value = pos["x"].get<double>();
            value_found = true;
          } else if (point.name.find("Y") != std::string::npos) {
            extracted_value = pos["y"].get<double>();
            value_found = true;
          }
        }
      } else {
        if (msg.is_number()) {
          extracted_value = msg.get<double>();
          value_found = true;
        } else if (msg.is_boolean()) {
          extracted_value = msg.get<bool>();
          value_found = true;
        }
      }
    }

    if (value_found) {
      point.UpdateCurrentValue(extracted_value);
      points_to_send.push_back(point.ToTimestampedValue());
      last_success_time_ = now;
    }
  }

  // 2. [Auto-Registration] Scan for new fields in JSON object
  if (msg.is_object()) {
    for (auto &[key, value] : msg.items()) {
      // Check if this field is already mapped for this topic
      bool already_mapped = false;
      for (const auto &dp : data_points_) {
        if (dp.address_string == topic && dp.mapping_key == key) {
          already_mapped = true;
          break;
        }
      }

      if (!already_mapped && device_info_.is_auto_registration_enabled &&
          (value.is_number() || value.is_boolean())) {
        // Register new data point
        uint32_t new_id = RegisterNewDataPoint(topic + "/" + key, topic, key);
        if (new_id > 0) {
          // Update current mapping for this session
          topic_to_points_[topic].push_back(std::to_string(new_id));

          PulseOne::Structs::TimestampedValue val;
          val.point_id = new_id;
          val.timestamp = now;
          val.source = "ROS_auto:" + topic;
          val.quality = PulseOne::Enums::DataQuality::GOOD;
          val.value = value.is_number() ? value.get<double>()
                                        : (value.get<bool>() ? 1.0 : 0.0);
          points_to_send.push_back(val);
        }
      }
    }
  }

  if (!points_to_send.empty()) {
    LogManager::getInstance().Info("[ROSWorker] Submitting " +
                                   std::to_string(points_to_send.size()) +
                                   " points to pipeline");
    SendDataToPipeline(points_to_send);
  }
}

void ROSWorker::DetermineDataPointsFromTopics() {
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
  topic_to_points_.clear();
  subscribed_topics_.clear();

  for (const auto &dp : data_points_) {
    std::string topic = dp.address_string;
    if (topic.empty())
      continue;

    topic_to_points_[topic].push_back(dp.id);

    // Add to unique topics list
    if (std::find(subscribed_topics_.begin(), subscribed_topics_.end(),
                  topic) == subscribed_topics_.end()) {
      subscribed_topics_.push_back(topic);
    }
  }
}

} // namespace Workers
} // namespace PulseOne
