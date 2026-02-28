/**
 * @file CommandSubscriber.cpp
 * @brief Collector-specific Command Subscriber Implementation
 * @author PulseOne Development Team
 * @date 2026-02-17
 */

#include "Event/CommandSubscriber.h"
#include "Client/RedisClientImpl.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Workers/WorkerManager.h"
#include <chrono>
#include <csignal>

namespace PulseOne {
namespace Core {

CommandSubscriber::CommandSubscriber(
    const PulseOne::Event::EventSubscriberConfig &config)
    : EventSubscriber(config) {
  collector_id_ = ConfigManager::getInstance().getCollectorId();

  // publish 전용 Redis 클라이언트 초기화 (subscriber 모드와 분리)
  // RedisClientImpl 생성자 내부에서 loadConfiguration() → 자동 연결
  publisher_client_ = std::make_shared<PulseOne::RedisClientImpl>();
}

bool CommandSubscriber::start() {
  // Subscribe to broadcast channels
  subscribeChannel("config:reload");
  subscribeChannel("target:reload"); // Mirror export-gateway

  // Subscribe to collector-specific channel
  if (collector_id_ > 0) {
    std::string my_channel = "cmd:collector:" + std::to_string(collector_id_);
    subscribeChannel(my_channel);
    LogManager::getInstance().Info("CommandSubscriber - Subscribed to " +
                                   my_channel);
  } else {
    LogManager::getInstance().Warn(
        "CommandSubscriber - Collector ID not set, specific commands disabled");
  }

  return EventSubscriber::start();
}

void CommandSubscriber::routeMessage(const std::string &channel,
                                     const std::string &message) {
  LogManager::getInstance().Info("[C2-COMMAND] Received on channel: " +
                                 channel);

  if (channel == "config:reload") {
    handleConfigReload(message);
  } else if (channel.find("cmd:collector:") == 0) {
    handleCollectorCommand(channel, message);
  } else {
    // Fallback to base class handlers (registered via registerHandler)
    EventSubscriber::routeMessage(channel, message);
  }
}

void CommandSubscriber::handleConfigReload(const std::string &message) {
  LogManager::getInstance().Info(
      "🔄 CommandSubscriber - Reloading configuration...");

  try {
    ConfigManager::getInstance().reload();
    LogManager::getInstance().Info("✅ Configuration reloaded successfully");
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to reload configuration: " +
                                    std::string(e.what()));
  }
}

void CommandSubscriber::handleCollectorCommand(const std::string &channel,
                                               const std::string &message) {
  LogManager::getInstance().Info(
      "⚡ CommandSubscriber - Specific command received: " + message);

  try {
    auto j = json::parse(message);
    std::string command = j.value("command", "");

    if (command == "scan") {
      // 온디맨드 네트워크 스캔 (폴링 주기와 무관한 수동 트리거)
      // payload 예시:
      // {"command":"scan","protocol":"modbus","range":"192.168.1.0/24","timeout_ms":5000}
      std::string protocol = j.value("protocol", "modbus");
      std::string range = j.value("range", "192.168.1.0/24");
      int timeout_ms = j.value("timeout_ms", 5000);

      LogManager::getInstance().Info(
          "🚀 수동 네트워크 스캔 시작 - protocol=" + protocol + ", range=" +
          range + ", timeout=" + std::to_string(timeout_ms) + "ms");

      auto &wm = PulseOne::Workers::WorkerManager::getInstance();
      if (wm.StartNetworkScan(protocol, range, timeout_ms)) {
        LogManager::getInstance().Info("✅ 네트워크 스캔 시작 성공");
      } else {
        LogManager::getInstance().Warn(
            "⚠️ 네트워크 스캔 시작 실패 (드라이버 미지원 또는 이미 진행 중)");
      }

    } else if (command == "stop") {
      // [BUG #21 FIX] Collector 프로세스 정상 종료
      // raise(SIGTERM) → main.cpp SignalHandler → g_app->Stop()
      // 이렇게 하면 워커/스레드 정리(Cleanup)를 거친 후 종료됨
      LogManager::getInstance().Info(
          "🛑 C2 stop 커맨드 수신 — SIGTERM으로 정상 종료 요청");
      raise(SIGTERM);

    } else if (command == "restart_worker") {
      // 특정 worker 재시작
      // payload 예시: {"command":"restart_worker","device_id":"device_42"}
      std::string device_id = j.value("device_id", "");
      if (!device_id.empty()) {
        auto &wm = PulseOne::Workers::WorkerManager::getInstance();
        if (wm.RestartWorker(device_id)) {
          LogManager::getInstance().Info("✅ Worker 재시작 성공: " + device_id);
        } else {
          LogManager::getInstance().Warn("⚠️ Worker 재시작 실패: " + device_id);
        }
      } else {
        LogManager::getInstance().Warn("restart_worker: device_id 누락");
      }

    } else if (command == "write") {
      // 데이터 포인트 쓰기 (프로토콜 제어)
      // payload:
      // {"command":"write","device_id":"2","point_id":"4","value":"75","request_id":"uuid"}
      std::string device_id = j.value("device_id", "");
      std::string point_id = j.value("point_id", "");
      std::string value = j.value("value", "");
      std::string request_id = j.value("request_id", ""); // 감사 로그 추적 UUID

      if (!device_id.empty() && !point_id.empty()) {
        auto &wm = PulseOne::Workers::WorkerManager::getInstance();

        // ─ 타이밍 측정 시작 ──────────────────────────────────
        auto t_start = std::chrono::steady_clock::now();
        bool success = wm.WriteDataPoint(device_id, point_id, value);
        auto t_end = std::chrono::steady_clock::now();
        long duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t_end -
                                                                  t_start)
                .count();

        if (success) {
          LogManager::getInstance().Info(
              "✅ 데이터 쓰기 성공: device=" + device_id + ", point=" +
              point_id + ", duration=" + std::to_string(duration_ms) + "ms");
        } else {
          LogManager::getInstance().Warn("⚠️ 데이터 쓰기 실패: device=" +
                                         device_id + ", point=" + point_id);
        }

        // ─ 제어 결과를 Redis로 Backend에 통보 ────────────────
        if (!request_id.empty()) {
          try {
            // 프로토콜 타입에 따라 is_async 결정
            // TODO: WorkerManager에서 protocol_type을 가져올 수 있으면 더
            // 정확하게 판별 가능 현재는 Worker의 GetProtocolType()이 없으므로
            // false(동기) 기본값 사용
            bool is_async =
                false; // MQTT/ROS Worker는 WriteDataPoint 내에서 async 처리

            nlohmann::json result_json = {
                {"request_id", request_id},
                {"success", success},
                {"is_async", is_async},
                {"device_id", device_id},
                {"point_id", point_id},
                {"duration_ms", duration_ms},
                {"error_message",
                 success ? "" : "WriteDataPoint returned false"}};

            if (publisher_client_ && publisher_client_->isConnected()) {
              publisher_client_->publish("control:result", result_json.dump());
            } else {
              LogManager::getInstance().Warn(
                  "control:result: publisher_client_ 미연결");
            }

            LogManager::getInstance().Debug(
                "📡 control:result published: request_id=" + request_id);
          } catch (const std::exception &e) {
            LogManager::getInstance().Warn("control:result publish 실패: " +
                                           std::string(e.what()));
          }
        }

      } else {
        LogManager::getInstance().Warn("write: device_id 또는 point_id 누락");
      }

    } else {
      LogManager::getInstance().Warn("Unknown C2 command: " + command);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to parse command JSON: " +
                                    std::string(e.what()));
  }
}

} // namespace Core
} // namespace PulseOne
