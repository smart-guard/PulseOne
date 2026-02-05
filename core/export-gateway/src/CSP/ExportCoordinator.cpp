/**
 * @file ExportCoordinator.cpp
 * @brief Export 시스템 중앙 조율자 구현
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0
 *
 * v2.0 변경사항:
 * - AlarmSubscriber → EventSubscriber (범용 이벤트 구독)
 * - 스케줄 이벤트 지원 추가
 * - 하위 호환성 유지
 */

#include "CSP/ExportCoordinator.h"
#include "DatabaseManager.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>

#include "CSP/AlarmMessage.h"
#include "CSP/DynamicTargetManager.h"
#include "Constants/ExportConstants.h" // ✅ Added Constants
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/RepositoryFactory.h"
#include "Export/ExportTypes.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

namespace PulseOne {
namespace Coordinator {

// =============================================================================
// Forward declarations
// =============================================================================

// ScheduleEventHandler 내부 클래스
class ScheduleEventHandler : public PulseOne::Event::IEventHandler {
private:
  ExportCoordinator *coordinator_;

public:
  explicit ScheduleEventHandler(ExportCoordinator *coordinator)
      : coordinator_(coordinator) {}

  bool handleEvent(const std::string &channel,
                   const std::string &message) override {
    coordinator_->handleScheduleEvent(channel, message);
    return true;
  }

  std::string getName() const override { return "ScheduleEventHandler"; }
};

// ConfigEventHandler 내부 클래스 (NEW)
class ConfigEventHandler : public PulseOne::Event::IEventHandler {
private:
  ExportCoordinator *coordinator_;

public:
  explicit ConfigEventHandler(ExportCoordinator *coordinator)
      : coordinator_(coordinator) {}

  bool handleEvent(const std::string &channel,
                   const std::string &message) override {
    coordinator_->handleConfigEvent(channel, message);
    return true;
  }

  std::string getName() const override { return "ConfigEventHandler"; }
};

// CommandEventHandler 내부 클래스
class CommandEventHandler : public PulseOne::Event::IEventHandler {
private:
  ExportCoordinator *coordinator_;

public:
  explicit CommandEventHandler(ExportCoordinator *coordinator)
      : coordinator_(coordinator) {}

  bool handleEvent(const std::string &channel,
                   const std::string &message) override {
    coordinator_->handleCommandEvent(channel, message);
    return true;
  }

  std::string getName() const override { return "CommandEventHandler"; }
};

// =============================================================================
// 정적 멤버 초기화
// =============================================================================

std::shared_ptr<PulseOne::CSP::DynamicTargetManager>
    ExportCoordinator::shared_target_manager_ = nullptr;

std::shared_ptr<PulseOne::Transform::PayloadTransformer>
    ExportCoordinator::shared_payload_transformer_ = nullptr;

std::mutex ExportCoordinator::init_mutex_;
std::atomic<bool> ExportCoordinator::shared_resources_initialized_{false};

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportCoordinator::ExportCoordinator(const ExportCoordinatorConfig &config)
    : config_(config) {

  LogManager::getInstance().Info("ExportCoordinator v2.0 초기화 시작");
  LogManager::getInstance().Info("데이터베이스: " + config_.database_path);
  LogManager::getInstance().Info("Redis: " + config_.redis_host + ":" +
                                 std::to_string(config_.redis_port));
  LogManager::getInstance().Info("✅ EventSubscriber: 범용 이벤트 구독자");

  stats_.start_time = std::chrono::system_clock::now();

  // Redis 클라이언트 초기화
  try {
    redis_client_ = std::make_unique<RedisClientImpl>();
    redis_client_->connect(config_.redis_host, config_.redis_port,
                           config_.redis_password);
  } catch (const std::exception &e) {

    LogManager::getInstance().Error("Redis 클라이언트 초기화 실패: " +
                                    std::string(e.what()));
  }
}

ExportCoordinator::~ExportCoordinator() {
  try {
    stop();
    LogManager::getInstance().Info("ExportCoordinator 소멸 완료");
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportCoordinator 소멸 중 예외: " +
                                    std::string(e.what()));
  }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool ExportCoordinator::start() {
  if (is_running_.load()) {
    LogManager::getInstance().Warn("ExportCoordinator가 이미 실행 중입니다");
    return false;
  }

  LogManager::getInstance().Info("ExportCoordinator 시작 중...");

  try {
    // 1. 데이터베이스 초기화
    if (!initializeDatabase()) {
      LogManager::getInstance().Error("데이터베이스 초기화 실패");
      return false;
    }

    // 2. 공유 리소스 초기화
    if (!initializeSharedResources(gateway_id_)) {
      LogManager::getInstance().Error("공유 리소스 초기화 실패");
      return false;
    }

    // ✅ setGatewayId는 initializeSharedResources 내부에서 처리됨
    // if (shared_target_manager_) {
    //   shared_target_manager_->setGatewayId(gateway_id_);
    // }

    // 3. Repositories 초기화
    if (!initializeRepositories()) {
      LogManager::getInstance().Error("Repositories 초기화 실패");
      return false;
    }

    // 4. EventSubscriber 초기화 및 시작
    if (!initializeEventSubscriber()) {
      LogManager::getInstance().Error("EventSubscriber 초기화 실패");
      return false;
    }

    // 5. ScheduledExporter 초기화 및 시작
    if (!initializeScheduledExporter()) {
      LogManager::getInstance().Error("ScheduledExporter 초기화 실패");
      return false;
    }

    // 6. 하트비트 스레드 시작
    if (gateway_id_ > 0) {
      heartbeat_running_ = true;
      heartbeat_thread_ = std::thread([this]() {
        LogManager::getInstance().Info(
            "하트비트 스레드 시작 (ID: " + std::to_string(gateway_id_) + ")");
        while (heartbeat_running_) {
          updateHeartbeat();
          for (int i = 0; i < 30 && heartbeat_running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        }
        LogManager::getInstance().Info("하트비트 스레드 종료");
      });
    }

    is_running_ = true;
    startBatchTimers(); // Start batch timer
    LogManager::getInstance().Info("ExportCoordinator v2.0 시작 완료 ✅");

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportCoordinator 시작 실패: " +
                                    std::string(e.what()));
    stop();
    return false;
  }
}

void ExportCoordinator::stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().Info("ExportCoordinator 중지 중...");

  // 1. EventSubscriber 중지
  if (event_subscriber_) {
    LogManager::getInstance().Info("EventSubscriber 중지 중...");
    event_subscriber_->stop();
  }

  // 2. ScheduledExporter 중지
  if (scheduled_exporter_) {
    LogManager::getInstance().Info("ScheduledExporter 중지 중...");
    scheduled_exporter_->stop();
  }

  // 2.5 하트비트 중지
  heartbeat_running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }

  // 3. 공유 리소스 정리
  cleanupSharedResources();

  is_running_ = false;
  stopBatchTimers(); // Stop batch timer
  LogManager::getInstance().Info("ExportCoordinator 중지 완료");
}

// =============================================================================
// 공유 리소스 관리
// =============================================================================

bool ExportCoordinator::initializeSharedResources(int gateway_id) {
  std::lock_guard<std::mutex> lock(init_mutex_);

  if (shared_resources_initialized_.load()) {
    LogManager::getInstance().Info("공유 리소스가 이미 초기화되었습니다");
    return true;
  }

  try {
    LogManager::getInstance().Info("공유 리소스 초기화 시작...");

    // 1. DynamicTargetManager 싱글턴
    if (!shared_target_manager_) {
      LogManager::getInstance().Info("DynamicTargetManager 초기화 중...");

      shared_target_manager_ =
          std::shared_ptr<PulseOne::CSP::DynamicTargetManager>(
              &PulseOne::CSP::DynamicTargetManager::getInstance(),
              [](PulseOne::CSP::DynamicTargetManager *) {});

      shared_target_manager_->setGatewayId(gateway_id);

      if (!shared_target_manager_->start()) {
        LogManager::getInstance().Error("DynamicTargetManager 시작 실패");
        return false;
      }

      LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
    }

    // 2. PayloadTransformer 싱글턴
    if (!shared_payload_transformer_) {
      LogManager::getInstance().Info("PayloadTransformer 초기화 중...");

      shared_payload_transformer_ =
          std::shared_ptr<PulseOne::Transform::PayloadTransformer>(
              &PulseOne::Transform::PayloadTransformer::getInstance(),
              [](PulseOne::Transform::PayloadTransformer *) {});

      LogManager::getInstance().Info("PayloadTransformer 초기화 완료");
    }

    shared_resources_initialized_ = true;
    LogManager::getInstance().Info("공유 리소스 초기화 완료 ✅");

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("공유 리소스 초기화 실패: " +
                                    std::string(e.what()));
    shared_target_manager_.reset();
    shared_payload_transformer_.reset();
    return false;
  }
}

void ExportCoordinator::cleanupSharedResources() {
  std::lock_guard<std::mutex> lock(init_mutex_);

  if (!shared_resources_initialized_.load()) {
    return;
  }

  LogManager::getInstance().Info("공유 리소스 정리 중...");

  if (shared_target_manager_) {
    shared_target_manager_->stop();
    shared_target_manager_.reset();
    LogManager::getInstance().Info("DynamicTargetManager 정리 완료");
  }

  if (shared_payload_transformer_) {
    shared_payload_transformer_.reset();
    LogManager::getInstance().Info("PayloadTransformer 정리 완료");
  }

  shared_resources_initialized_ = false;
  LogManager::getInstance().Info("공유 리소스 정리 완료");
}

std::shared_ptr<PulseOne::CSP::DynamicTargetManager>
ExportCoordinator::getTargetManager() {
  std::lock_guard<std::mutex> lock(init_mutex_);
  return shared_target_manager_;
}

std::shared_ptr<PulseOne::Transform::PayloadTransformer>
ExportCoordinator::getPayloadTransformer() {
  std::lock_guard<std::mutex> lock(init_mutex_);
  return shared_payload_transformer_;
}

// =============================================================================
// 내부 초기화 메서드
// =============================================================================

bool ExportCoordinator::initializeDatabase() {
  try {
    LogManager::getInstance().Info("데이터베이스 초기화 중...");

    std::string db_path = getDatabasePath();

    auto &db_manager = DbLib::DatabaseManager::getInstance();

    // ✅ FIX: DatabaseManager를 명시적으로 초기화하여 올바른 경로 설정
    DbLib::DatabaseConfig db_config;
    db_config.type = "SQLITE";
    db_config.sqlite_path = db_path;
    db_config.use_redis = false; // Redis는 별도로 관리됨

    if (!db_manager.initialize(db_config)) {
      LogManager::getInstance().Error(
          "DatabaseManager 초기화 실패 (경로: " + db_path + ")");
      return false;
    }

    // ✅ RepositoryFactory 명시적 초기화
    auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
    if (!factory.initialize()) {
      LogManager::getInstance().Warn(
          "RepositoryFactory 초기화 실패 - 계속 진행");
    }

    std::vector<std::vector<std::string>> test_result;
    if (!db_manager.executeQuery("SELECT 1", test_result)) {
      LogManager::getInstance().Error("데이터베이스 연결 실패");
      return false;
    }

    LogManager::getInstance().Info("데이터베이스 초기화 완료: " + db_path);

    // ✅ 게이트웨이 설정 로드 (subscription_mode)
    if (gateway_id_ > 0) {
      std::vector<std::vector<std::string>> gw_result;
      std::string query =
          "SELECT subscription_mode FROM edge_servers WHERE id = " +
          std::to_string(gateway_id_);
      if (db_manager.executeQuery(query, gw_result) && !gw_result.empty() &&
          !gw_result[0].empty()) {
        config_.subscription_mode = gw_result[0][0];
        LogManager::getInstance().Info("Gateway Subscription Mode 로드 완료: " +
                                       config_.subscription_mode);
      }
    }

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("데이터베이스 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool ExportCoordinator::initializeRepositories() {
  try {
    LogManager::getInstance().Info("Repositories 초기화 중...");

    log_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportLogRepository>();

    target_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportTargetRepository>();

    LogManager::getInstance().Info("Repositories 초기화 완료");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Repositories 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool ExportCoordinator::initializeEventSubscriber() {
  try {
    LogManager::getInstance().Info("EventSubscriber 초기화 중...");

    // EventSubscriber 설정
    PulseOne::Event::EventSubscriberConfig event_config;
    event_config.redis_host = config_.redis_host;
    event_config.redis_port = config_.redis_port;
    event_config.redis_password = config_.redis_password;

    // ✅ 알람 채널 + 스케줄 이벤트 채널
    // gateway_id가 설정되고 subscription_mode가 'selective'인 경우
    // alarms:all 대신 디바이스별 채널을 사용하도록 유도
    if (gateway_id_ > 0 && shared_target_manager_ &&
        config_.subscription_mode == "selective") {
      auto device_ids = shared_target_manager_->getAssignedDeviceIds();
      event_config.subscribe_channels
          .clear(); // ✅ FIX: 기본 채널(alarms:all) 제거
      for (const auto &id : device_ids) {
        event_config.subscribe_channels.push_back("device:" + id + ":alarms");
      }
      // ✅ FIX: Selective 모드에서는 패턴 구독 비활성화 (중복 방지)
      event_config.subscribe_patterns.clear();

      LogManager::getInstance().Info("Selective Subscription 활성화: " +
                                     std::to_string(device_ids.size()) +
                                     "개 디바이스 채널 설정 (패턴 구독 차단)");
    } else {
      event_config.subscribe_channels = config_.alarm_channels;
      event_config.subscribe_patterns = config_.alarm_patterns;
    }

    event_config.subscribe_channels.push_back("schedule:reload");
    event_config.subscribe_channels.push_back("schedule:execute:*");
    // ✅ 설정 리로드 채널 추가
    event_config.subscribe_channels.push_back("config:reload");
    event_config.subscribe_channels.push_back("target:reload");

    event_config.subscribe_patterns = config_.alarm_patterns;
    event_config.worker_thread_count = config_.alarm_worker_threads;
    event_config.max_queue_size = config_.alarm_max_queue_size;
    event_config.enable_debug_log = config_.enable_debug_log;

    // EventSubscriber 생성
    event_subscriber_ =
        std::make_unique<PulseOne::Event::EventSubscriber>(event_config);

    // ✅ 알람 처리 콜백 등록 (로깅 및 통합 처리를 위해 Coordinator로 연결)
    event_subscriber_->setAlarmCallback(
        [this](const PulseOne::CSP::AlarmMessage &alarm) {
          this->handleAlarmEvent(alarm);
        });

    // ✅ 스케줄 이벤트 핸들러 등록
    auto schedule_handler = std::make_shared<ScheduleEventHandler>(this);
    event_subscriber_->registerHandler("schedule:*", schedule_handler);

    // ✅ 설정 이벤트 핸들러 등록 (NEW)
    auto config_handler = std::make_shared<ConfigEventHandler>(this);
    event_subscriber_->registerHandler("config:*", config_handler);
    event_subscriber_->registerHandler("target:*", config_handler);

    // ✅ 명령 이벤트 핸들러 등록 (Manual Export 등)
    // cmd:* 패턴이 모든 명령을 처리하므로 게이트웨이 전용 채널을 별도로 구독할
    // 필요 없음
    auto command_handler = std::make_shared<CommandEventHandler>(this);
    event_subscriber_->registerHandler("cmd:*", command_handler);
    event_subscriber_->subscribePattern("cmd:*");

    // EventSubscriber 시작
    if (!event_subscriber_->start()) {
      LogManager::getInstance().Error("EventSubscriber 시작 실패");
      return false;
    }

    LogManager::getInstance().Info("EventSubscriber 초기화 완료 ✅");
    LogManager::getInstance().Info(
        "  - 알람 채널: " + std::to_string(config_.alarm_channels.size()) +
        "개");
    LogManager::getInstance().Info("  - 스케줄 이벤트: 활성화");

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("EventSubscriber 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool ExportCoordinator::initializeScheduledExporter() {
  try {
    LogManager::getInstance().Info("ScheduledExporter 초기화 중...");

    PulseOne::Schedule::ScheduledExporterConfig schedule_config;
    schedule_config.redis_host = config_.redis_host;
    schedule_config.redis_port = config_.redis_port;
    schedule_config.redis_password = config_.redis_password;
    schedule_config.check_interval_seconds =
        config_.schedule_check_interval_seconds;
    schedule_config.reload_interval_seconds =
        config_.schedule_reload_interval_seconds;
    schedule_config.batch_size = config_.schedule_batch_size;
    schedule_config.enable_debug_log = config_.enable_debug_log;

    scheduled_exporter_ =
        &PulseOne::Schedule::ScheduledExporter::getInstance(schedule_config);

    if (!scheduled_exporter_->start()) {
      LogManager::getInstance().Error("ScheduledExporter 시작 실패");
      return false;
    }

    LogManager::getInstance().Info("ScheduledExporter 초기화 완료");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ScheduledExporter 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

void ExportCoordinator::updateHeartbeat() {
  if (gateway_id_ <= 0) {
    return;
  }

  try {
    auto &db_manager = DbLib::DatabaseManager::getInstance();
    std::string query =
        "UPDATE edge_servers SET last_seen = CURRENT_TIMESTAMP, status = "
        "'active' WHERE id = " +
        std::to_string(gateway_id_);

    db_manager.executeNonQuery(query);

    // Redis 하트비트 추가
    if (redis_client_ && redis_client_->isConnected()) {
      nlohmann::json status_json;
      status_json["status"] = PulseOne::Constants::Export::Redis::STATUS_ONLINE;
      status_json[PulseOne::Constants::Export::Redis::KEY_LAST_SEEN] =
          std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::now());
      status_json["gatewayId"] = gateway_id_;
      status_json["hostname"] = "docker-container"; // 간단하게 상수로 처리

      // gateway:status:{id} 키에 90초 만료로 저장
      redis_client_->setex(
          PulseOne::Constants::Export::Redis::KEY_GATEWAY_STATUS_PREFIX +
              std::to_string(gateway_id_),
          status_json.dump(), 90);
    }
  } catch (const std::exception &e) {

    LogManager::getInstance().Warn("Export Gateway 하트비트 업데이트 실패: " +
                                   std::string(e.what()));
  }
}

// =============================================================================
// ✅ 이벤트 핸들러 (간소화)
// =============================================================================

void ExportCoordinator::handleScheduleEvent(const std::string &channel,
                                            const std::string &message) {
  try {
    LogManager::getInstance().Info("🔄 스케줄 이벤트 수신: " + channel);

    if (!scheduled_exporter_) {
      LogManager::getInstance().Warn("ScheduledExporter가 초기화되지 않음");
      return;
    }

    // ✅ schedule:reload 처리
    if (channel ==
        PulseOne::Constants::Export::Redis::CHANNEL_SCHEDULE_RELOAD) {
      int loaded = scheduled_exporter_->reloadSchedules();
      LogManager::getInstance().Info(
          "✅ 스케줄 리로드 완료: " + std::to_string(loaded) + "개");
    }
    // ✅ schedule:execute:{id} 처리
    else if (channel.find(
                 PulseOne::Constants::Export::Redis::PATTERN_SCHEDULE_EXECUTE
                     .substr(0, 17)) == 0) {
      std::string id_str = channel.substr(17); // "schedule:execute:" 이후
      try {
        int schedule_id = std::stoi(id_str);
        LogManager::getInstance().Info("⚡ 스케줄 실행 요청: ID=" +
                                       std::to_string(schedule_id));

        auto result = scheduled_exporter_->executeSchedule(schedule_id);

        if (result.success) {
          LogManager::getInstance().Info(
              "✅ 스케줄 실행 완료: " +
              std::to_string(result.data_point_count) + "개 데이터 포인트");
        } else {
          LogManager::getInstance().Error("❌ 스케줄 실행 실패: " +
                                          result.error_message);
        }
      } catch (const std::exception &e) {
        LogManager::getInstance().Error("스케줄 ID 파싱 실패: " +
                                        std::string(e.what()));
      }
    }

    // 통계 업데이트
    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.schedule_events++;
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("스케줄 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }
}

void ExportCoordinator::handleConfigEvent(const std::string &channel,
                                          const std::string &message) {
  try {
    LogManager::getInstance().Info("🔄 설정 이벤트 수신: " + channel);

    if (channel == PulseOne::Constants::Export::Redis::CHANNEL_CONFIG_RELOAD ||
        channel == PulseOne::Constants::Export::Redis::CHANNEL_TARGET_RELOAD) {
      int loaded = reloadTargets();
      LogManager::getInstance().Info(
          "✅ 타겟 리로드 트리거 완료: " + std::to_string(loaded) + "개");
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("설정 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }
}

void ExportCoordinator::handleCommandEvent(const std::string &channel,
                                           const std::string &message) {
  try {
    LogManager::getInstance().Info("Gateway 명령 수신: " + message);

    auto j = nlohmann::json::parse(message);
    std::string command = j.value("command", "");
    nlohmann::json payload =
        j.contains("payload") ? j["payload"] : nlohmann::json::object();

    if (command == PulseOne::Constants::Export::Command::MANUAL_EXPORT) {
      std::string target_name =
          payload.value(PulseOne::Constants::Export::JsonKeys::TARGET_NAME, "");
      int target_id =
          payload.value(PulseOne::Constants::Export::JsonKeys::TARGET_ID, 0);

      if (target_name == PulseOne::Constants::Export::JsonKeys::ALL_TARGETS ||
          target_name == "all") { // "all" for backward compatibility
        auto target_manager = getTargetManager();
        if (target_manager) {
          auto all_targets = target_manager->getAllTargets();
          bool overall_success = true;
          std::string error_summary = "";

          for (const auto &target : all_targets) {
            if (!target.enabled)
              continue;
            auto res = handleManualExport(target.name, payload);
            if (!res.success) {
              overall_success = false;
              if (!error_summary.empty())
                error_summary += ", ";
              error_summary += target.name + ": " + res.error_message;
            }
          }

          if (redis_client_ && redis_client_->isConnected()) {
            nlohmann::json res_payload;
            res_payload[PulseOne::Constants::Export::JsonKeys::SUCCESS] =
                overall_success;
            res_payload[PulseOne::Constants::Export::JsonKeys::ERROR] =
                error_summary;
            res_payload[PulseOne::Constants::Export::JsonKeys::TARGET] =
                PulseOne::Constants::Export::JsonKeys::ALL_TARGETS;
            res_payload[PulseOne::Constants::Export::JsonKeys::COMMAND_ID] =
                payload.value(PulseOne::Constants::Export::JsonKeys::COMMAND_ID,
                              "");
            nlohmann::json res_msg;
            res_msg[PulseOne::Constants::Export::JsonKeys::COMMAND] =
                PulseOne::Constants::Export::Command::MANUAL_EXPORT_RESULT;
            res_msg[PulseOne::Constants::Export::JsonKeys::PAYLOAD] =
                res_payload;
            redis_client_->publish(
                PulseOne::Constants::Export::Redis::CHANNEL_CMD_GATEWAY_RESULT,
                res_msg.dump());
          }
        }
        return;
      }

      if (target_name.empty() && target_id > 0 && target_repo_) {
        auto target = target_repo_->findById(target_id);
        if (target.has_value()) {
          target_name = target->getName();
        }
      }

      if (target_name.empty()) {
        LogManager::getInstance().Error(
            "수동 전송 실패: 타겟 이름 또는 ID가 없습니다.");
        return;
      }

      auto result = handleManualExport(target_name, payload);

      // Publish result to redis so UI can show it
      if (redis_client_ && redis_client_->isConnected()) {
        nlohmann::json res_payload;
        res_payload[PulseOne::Constants::Export::JsonKeys::SUCCESS] =
            result.success;
        res_payload[PulseOne::Constants::Export::JsonKeys::ERROR] =
            result.error_message;
        res_payload[PulseOne::Constants::Export::JsonKeys::TARGET] =
            target_name;
        res_payload[PulseOne::Constants::Export::JsonKeys::COMMAND_ID] =
            payload.value(PulseOne::Constants::Export::JsonKeys::COMMAND_ID,
                          "");

        nlohmann::json res_msg;
        res_msg[PulseOne::Constants::Export::JsonKeys::COMMAND] =
            PulseOne::Constants::Export::Command::MANUAL_EXPORT_RESULT;
        res_msg[PulseOne::Constants::Export::JsonKeys::PAYLOAD] = res_payload;

        redis_client_->publish(
            PulseOne::Constants::Export::Redis::CHANNEL_CMD_GATEWAY_RESULT,
            res_msg.dump());
      }
    } else {
      LogManager::getInstance().Warn("지원되지 않는 명령: " + command);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("handleCommandEvent 에러: " +
                                    std::string(e.what()));
  }
}

// =============================================================================
// 알람 전송 조율
// =============================================================================

std::vector<ExportResult>
ExportCoordinator::handleAlarmEvent(PulseOne::CSP::AlarmMessage alarm) {

  std::vector<ExportResult> results;

  try {
    // ✅ 1. Point Metadata Enrichment (st: Control Status mapping)
    if (alarm.point_id > 0) {
      try {
        auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto point_repo = factory.getDataPointRepository();
        if (point_repo) {
          auto point_opt = point_repo->findById(alarm.point_id);
          if (point_opt.has_value()) {
            alarm.st = point_opt->isWritable()
                           ? 1
                           : 0; // ✅ 제어가능여부(0: Manual/Read-only, 1:
                                // Auto/Writable)
          }
        }
      } catch (...) {
      }
    }

    // ✅ 2. Site ID Enrichment (Resolving site_id from point_id if missing)
    if (alarm.site_id <= 0 && alarm.point_id > 0) {
      try {
        auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto point_repo = factory.getDataPointRepository();
        auto device_repo = factory.getDeviceRepository();

        if (point_repo && device_repo) {
          auto point_opt = point_repo->findById(alarm.point_id);
          if (point_opt.has_value()) {
            int device_id = point_opt->getDeviceId();
            auto device_opt = device_repo->findById(device_id);
            if (device_opt.has_value()) {
              alarm.site_id = device_opt->getSiteId();
              LogManager::getInstance().Debug(
                  "알람 사이트 정보 보정 완료: point=" +
                  std::to_string(alarm.point_id) +
                  ", site=" + std::to_string(alarm.site_id));
            }
          }
        }
      } catch (const std::exception &e) {
        LogManager::getInstance().Warn("사이트 ID 보정 중 오류: " +
                                       std::string(e.what()));
      }
    }

    // Batching Logic Support
    if (config_.enable_alarm_batching) {
      std::lock_guard<std::mutex> lock(batch_mutex_);
      pending_alarms_.push_back(alarm);
      std::cout
          << "[DEBUG][ExportCoordinator] Alarm enqueued to batch. Queue size: "
          << pending_alarms_.size() << std::endl;
      LogManager::getInstance().Debug(
          "[ExportCoordinator] Alarm enqueued to batch. Queue size: " +
          std::to_string(pending_alarms_.size()));

      if (pending_alarms_.size() >=
          static_cast<size_t>(config_.alarm_batch_max_size)) {
        flushAlarmBatch();
      }
      return results; // Return empty results as actual send is delayed
    }

    std::cout << "[v3.2.0 Debug][ExportCoordinator] handleAlarmEvent: "
              << alarm.nm << " [extra_info=" << alarm.extra_info.dump() << "]"
              << std::endl;
    LogManager::getInstance().Info(
        "[v3.2.0 Debug] [ExportCoordinator] 알람 이벤트 수신. Name: " +
        alarm.nm + ", Condition: " + alarm.des +
        ", Raw Extra: " + alarm.extra_info.dump());

    auto target_manager = getTargetManager();
    if (!target_manager) {
      LogManager::getInstance().Error("TargetManager가 초기화되지 않았습니다");
      return results;
    }

    // Single Alarm Dispatch (Corrected)
    auto target_results = target_manager->sendAlarmToTargets(alarm);

    for (const auto &target_result : target_results) {
      ExportResult result = convertTargetSendResult(target_result);
      results.push_back(result);

      std::cout << "[DEBUG][ExportCoordinator] Target result: "
                << target_result.target_name
                << " success=" << target_result.success << std::endl;
      logExportResult(result, &alarm);
      updateStats(result);
    }

    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.alarm_events++;
      stats_.alarm_exports += results.size();
    }

    LogManager::getInstance().Info(
        "알람 즉시 전송 완료: " + std::to_string(results.size()) + "개 타겟");

    // ✅ NEW: 자동 파일 업로드 처리 (v3.2.0)
    if (alarm.extra_info.contains("file_ref")) {
      std::string file_ref = alarm.extra_info["file_ref"].get<std::string>();
      if (!file_ref.empty()) {
        std::cout << "[v3.2.0 Debug] Automated file upload triggered for: "
                  << file_ref << std::endl;

        // file_ref는 "file:///app/data/blobs/20260203_..." 형식 또는 단순 ID
        std::string local_path = file_ref;
        if (local_path.find("file://") == 0) {
          local_path = local_path.substr(7);
        } else if (local_path.find("/") == std::string::npos) {
          // 단순 ID인 경우 기본 경로 부여
          local_path = "/app/data/blobs/" + local_path;
        }

        LogManager::getInstance().Info(
            "[v3.2.0 Debug] 자동 파일 업로드 트리거됨: " + local_path);

        // 🚀 타겟 대상으로 파일 전송
        auto file_results = target_manager->sendFileToTargets(local_path);

        // 파일 전송 결과도 통합 (필요시)
        for (const auto &fr : file_results) {
          LogManager::getInstance().Info(
              "   └─ 타겟 '" + fr.target_name + "' 파일 전송 " +
              (fr.success ? "성공" : "실패: " + fr.error_message));
        }
      }
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("알람 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }

  return results;
}

void ExportCoordinator::flushAlarmBatch() {
  std::vector<PulseOne::CSP::AlarmMessage> batch_to_send;
  {
    // Move pending alarms to local batch to minimize lock time
    std::lock_guard<std::mutex> lock(batch_mutex_);
    if (pending_alarms_.empty()) {
      LogManager::getInstance().Debug(
          "[ExportCoordinator] flushAlarmBatch called but no alarms pending");
      return;
    }
    batch_to_send = std::move(pending_alarms_);
    pending_alarms_.clear(); // Reset vector
    last_batch_flush_time_ = std::chrono::system_clock::now();
  }

  try {
    LogManager::getInstance().Info(
        "알람 배치 플러시: " + std::to_string(batch_to_send.size()) + "개");

    auto target_manager = getTargetManager();
    if (!target_manager) {
      LogManager::getInstance().Error(
          "[ExportCoordinator] TargetManager is null in flushAlarmBatch!");
      return;
    }
    LogManager::getInstance().Debug("[ExportCoordinator] Sending batch of " +
                                    std::to_string(batch_to_send.size()) +
                                    " alarms to TargetManager");

    auto target_results =
        target_manager->sendAlarmBatchToTargets(batch_to_send);

    // Log results and handle failures
    for (const auto &target_result : target_results.results) {
      ExportResult result = convertTargetSendResult(target_result);
      logExportResult(result);
      updateStats(result);

      // ✅ 전송 실패 시 로컬 저장을 통한 복구 시스템 연동
      if (!target_result.success && scheduled_exporter_) {
        auto target = target_manager->getTarget(target_result.target_name);
        if (target) {
          scheduled_exporter_->saveFailedAlarmBatchToFile(
              target->name, batch_to_send, target->config);
        }
      }
    }

    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.alarm_exports += batch_to_send.size();
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("알람 배치 플러시 실패: " +
                                    std::string(e.what()));
  }
}

void ExportCoordinator::startBatchTimers() {
  if (batch_timer_running_)
    return;

  batch_timer_running_ = true;
  last_batch_flush_time_ = std::chrono::system_clock::now();

  batch_timer_thread_ = std::thread([this]() {
    LogManager::getInstance().Info("배치 타이머 스레드 시작");
    while (batch_timer_running_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (!config_.enable_alarm_batching)
        continue;

      auto now = std::chrono::system_clock::now();
      std::chrono::duration<double, std::milli> elapsed =
          now - last_batch_flush_time_;

      if (elapsed.count() >= config_.alarm_batch_latency_ms) {
        std::unique_lock<std::mutex> lock(batch_mutex_);
        if (!pending_alarms_.empty()) {
          lock.unlock();
          flushAlarmBatch();
          lock.lock();
        }
      }
    }
    LogManager::getInstance().Info("배치 타이머 스레드 종료");
  });
}

void ExportCoordinator::stopBatchTimers() {
  batch_timer_running_ = false;
  if (batch_timer_thread_.joinable()) {
    batch_timer_thread_.join();
  }
  // Flush remaining
  flushAlarmBatch();
}

std::vector<ExportResult> ExportCoordinator::handleAlarmBatch(
    std::vector<PulseOne::CSP::AlarmMessage> alarms) {

  std::vector<ExportResult> all_results;

  try {
    LogManager::getInstance().Info(
        "알람 배치 처리: " + std::to_string(alarms.size()) + "개");

    for (const auto &alarm : alarms) {
      auto results = handleAlarmEvent(alarm);
      all_results.insert(all_results.end(), results.begin(), results.end());
    }

    LogManager::getInstance().Info(
        "알람 배치 처리 완료: " + std::to_string(all_results.size()) +
        "개 전송");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("알람 배치 처리 실패: " +
                                    std::string(e.what()));
  }

  return all_results;
}

// =============================================================================
// 스케줄 전송 조율
// =============================================================================

std::vector<ExportResult>
ExportCoordinator::handleScheduledExport(int schedule_id) {
  std::vector<ExportResult> results;

  try {
    LogManager::getInstance().Info("스케줄 전송: ID=" +
                                   std::to_string(schedule_id));

    if (!scheduled_exporter_) {
      LogManager::getInstance().Error(
          "ScheduledExporter가 초기화되지 않았습니다");
      return results;
    }

    auto execution_result = scheduled_exporter_->executeSchedule(schedule_id);

    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.schedule_executions++;
      stats_.schedule_exports += execution_result.exported_points;
    }

    LogManager::getInstance().Info(
        "스케줄 전송 완료: " +
        std::to_string(execution_result.exported_points) + "개 포인트 전송");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("스케줄 전송 실패: " +
                                    std::string(e.what()));
  }

  return results;
}

ExportResult
ExportCoordinator::handleManualExport(const std::string &target_name,
                                      const nlohmann::json &data) {

  ExportResult result;
  result.target_name = target_name;
  result.success = false;

  try {
    int point_id = data.value("point_id", 0);
    if (point_id <= 0) {
      result.error_message = "유효하지 않은 point_id";
      return result;
    }

    // 1. Redis에서 최신값 조회
    if (!redis_client_ || !redis_client_->isConnected()) {
      result.error_message = "Redis 연결 안 됨";
      return result;
    }

    std::string redis_key = "point:" + std::to_string(point_id) + ":latest";
    std::string val_json_str = redis_client_->get(redis_key);
    if (val_json_str.empty()) {
      result.error_message = "Redis 데이터를 찾을 수 없음: " + redis_key;
      return result;
    }

    auto val_json = nlohmann::json::parse(val_json_str);

    // 2. AlarmMessage 준비 (매핑 및 템플릿 지원)
    PulseOne::CSP::AlarmMessage alarm;
    alarm.point_id = point_id;
    alarm.site_id = val_json.value("bd", val_json.value("site_id", 0));
    alarm.nm = val_json.value("nm", val_json.value("point_name", ""));

    // 값 파싱 (문자열 또는 숫자 대응) 및 페이로드 오버라이드 지원
    try {
      if (data.contains("value")) {
        if (data["value"].is_string()) {
          alarm.vl = std::stod(data["value"].get<std::string>());
        } else if (data["value"].is_number()) {
          alarm.vl = data["value"].get<double>();
        } else {
          alarm.vl = 0.0;
        }
      } else if (val_json.contains("vl") && val_json["vl"].is_string()) {
        alarm.vl = std::stod(val_json["vl"].get<std::string>());
      } else {
        alarm.vl = val_json.value("vl", 0.0);
      }
    } catch (...) {
      alarm.vl = 0.0;
    }

    // 타임스탬프 처리
    long long tm_ms = val_json.value("tm_ms", 0LL);
    if (tm_ms > 0) {
      auto tp = std::chrono::system_clock::time_point(
          std::chrono::milliseconds(tm_ms));
      alarm.tm = PulseOne::CSP::AlarmMessage::time_to_csharp_format(tp, true);
    } else {
      alarm.tm =
          PulseOne::CSP::AlarmMessage::current_time_to_csharp_format(true);
    }

    // Allows override of al, st, and des from manual command data
    alarm.al = data.value("al", 0);
    alarm.st = data.value("st", val_json.value("st", 1));
    alarm.des = data.value("des", std::string("Manual Export Triggered"));

    // 3. DynamicTargetManager를 통해 전송 (매핑 포인트 이름 자동 적용)
    auto target_manager = getTargetManager();
    if (!target_manager) {
      result.error_message = "TargetManager 초기화 안 됨";
      return result;
    }

    LogManager::getInstance().Info("수동 전송 시작: " + target_name +
                                   " (Point=" + std::to_string(point_id) + ")");

    PulseOne::CSP::TargetSendResult send_res =
        target_manager->sendAlarmToTarget(target_name, alarm);

    // 4. 결과 변환 및 통계 업데이트
    result = convertTargetSendResult(send_res);
    updateStats(result);
    // ✅ 수동 전송 기록 로그 저장 (UI 히스토리용)
    logExportResult(result, &alarm);

    if (result.success) {
      LogManager::getInstance().Info("수동 전송 완료: " + target_name);
    } else {
      LogManager::getInstance().Error("수동 전송 실패 [" + target_name +
                                      "]: " + result.error_message);
    }

  } catch (const std::exception &e) {
    result.error_message = "수동 전송 중 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
  }

  return result;
}

// =============================================================================
// 로깅 및 통계
// =============================================================================

void ExportCoordinator::logExportResult(
    const ExportResult &result,
    const PulseOne::CSP::AlarmMessage *alarm_message) {

  if (!log_repo_) {
    return;
  }

  try {
    using namespace PulseOne::Database::Entities;

    ExportLogEntity log_entity;
    log_entity.setLogType("alarm_export");
    log_entity.setTargetId(result.target_id);
    log_entity.setStatus(result.success ? "success" : "failure");
    log_entity.setHttpStatusCode(result.http_status_code);
    log_entity.setErrorMessage(result.error_message);
    log_entity.setProcessingTimeMs(
        static_cast<int>(result.processing_time.count()));

    if (alarm_message) {
      log_entity.setSourceValue(
          nlohmann::json::array({alarm_message->to_json()}).dump());
    }

    log_repo_->save(log_entity);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("로그 저장 실패: " + std::string(e.what()));
  }
}

void ExportCoordinator::logExportResults(
    const std::vector<ExportResult> &results) {
  for (const auto &result : results) {
    logExportResult(result);
  }
}

ExportCoordinatorStats ExportCoordinator::getStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  ExportCoordinatorStats current_stats = stats_;

  // EventSubscriber의 통계 합산 (v3.0 통합)
  if (event_subscriber_) {
    auto sub_stats = event_subscriber_->getStats();
    current_stats.total_exports += sub_stats.total_processed;
    current_stats.alarm_events += sub_stats.total_processed;

    // 성공/실패 합산 (EventSubscriber는 현재 성공만 카운트하거나 실패는 따로
    // 관리)
    current_stats.successful_exports += sub_stats.total_processed;
    current_stats.failed_exports += sub_stats.total_failed;
  }

  return current_stats;
}

void ExportCoordinator::resetStats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = ExportCoordinatorStats();
  stats_.start_time = std::chrono::system_clock::now();
  LogManager::getInstance().Info("통계 초기화 완료");
}

nlohmann::json
ExportCoordinator::getTargetStats(const std::string &target_name) const {
  nlohmann::json stats = nlohmann::json::object();

  try {
    if (!log_repo_) {
      return stats;
    }

    stats["target_name"] = target_name;
    stats["total"] = 0;
    stats["success"] = 0;
    stats["failure"] = 0;
    stats["avg_time_ms"] = 0.0;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("타겟 통계 조회 실패: " +
                                    std::string(e.what()));
  }

  return stats;
}

// =============================================================================
// 설정 관리
// =============================================================================

int ExportCoordinator::reloadTargets() {
  try {
    LogManager::getInstance().Info("타겟 리로드 중...");

    auto target_manager = getTargetManager();
    if (!target_manager) {
      LogManager::getInstance().Error("TargetManager 초기화 안 됨");
      return 0;
    }

    // ✅ FIX: 리로드 전 게이트웨이 ID 재설정 (ID 변경 가능성 대비)
    target_manager->setGatewayId(gateway_id_);

    if (!target_manager->forceReload()) {
      LogManager::getInstance().Error("타겟 리로드 실패");
      return 0;
    }

    auto targets = target_manager->getAllTargets();
    int reloaded_count = targets.size();

    LogManager::getInstance().Info(
        "타겟 리로드 완료: " + std::to_string(reloaded_count) + "개");

    // ✅ Selective Subscription 업데이트
    if (event_subscriber_) {
      auto device_ids = target_manager->getAssignedDeviceIds();
      event_subscriber_->updateSubscriptions(device_ids);
    }

    return reloaded_count;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("타겟 리로드 실패: " +
                                    std::string(e.what()));
    return 0;
  }
}

int ExportCoordinator::reloadTemplates() {
  try {
    LogManager::getInstance().Info("템플릿 리로드 중...");

    auto transformer = getPayloadTransformer();
    if (!transformer) {
      LogManager::getInstance().Error("PayloadTransformer 초기화 안 됨");
      return 0;
    }

    LogManager::getInstance().Info("템플릿 리로드 완료");
    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("템플릿 리로드 실패: " +
                                    std::string(e.what()));
    return 0;
  }
}

void ExportCoordinator::updateConfig(
    const ExportCoordinatorConfig &new_config) {
  std::lock_guard<std::mutex> lock(export_mutex_);
  config_ = new_config;
  LogManager::getInstance().Info("설정 업데이트 완료");
}

// =============================================================================
// 헬스 체크
// =============================================================================

bool ExportCoordinator::healthCheck() const {
  try {
    if (!is_running_.load()) {
      return false;
    }

    if (event_subscriber_ && !event_subscriber_->isRunning()) {
      return false;
    }

    if (scheduled_exporter_ && !scheduled_exporter_->isRunning()) {
      return false;
    }

    auto target_manager = getTargetManager();
    if (!target_manager || !target_manager->isRunning()) {
      return false;
    }

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("헬스 체크 실패: " + std::string(e.what()));
    return false;
  }
}

nlohmann::json ExportCoordinator::getComponentStatus() const {
  nlohmann::json status = nlohmann::json::object();

  try {
    status["coordinator_running"] = is_running_.load();

    status["event_subscriber"] =
        event_subscriber_ ? event_subscriber_->isRunning() : false;

    status["scheduled_exporter"] =
        scheduled_exporter_ ? scheduled_exporter_->isRunning() : false;

    auto target_manager = getTargetManager();
    status["target_manager"] =
        target_manager ? target_manager->isRunning() : false;

    status["shared_resources_initialized"] =
        shared_resources_initialized_.load();

    status["version"] = "2.0";
    status["features"] =
        json::array({"alarm_events", "schedule_events", "manual_export"});

  } catch (const std::exception &e) {
    status["error"] = e.what();
  }

  return status;
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

ExportResult ExportCoordinator::convertTargetSendResult(
    const PulseOne::CSP::TargetSendResult &target_result) const {

  ExportResult result;
  result.success = target_result.success;
  result.target_name = target_result.target_name;
  result.error_message = target_result.error_message;
  result.http_status_code = target_result.status_code;
  result.processing_time = target_result.response_time;
  result.data_size = target_result.content_size;

  try {
    if (target_repo_) {
      auto target_entity = target_repo_->findByName(result.target_name);
      if (target_entity.has_value()) {
        result.target_id = target_entity->getId();
      }
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Warn("타겟 ID 조회 실패: " +
                                   std::string(e.what()));
  }

  return result;
}

void ExportCoordinator::updateStats(const ExportResult &result) {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  stats_.total_exports++;

  if (result.success) {
    stats_.successful_exports++;
  } else {
    stats_.failed_exports++;
  }

  stats_.last_export_time = std::chrono::system_clock::now();

  if (stats_.total_exports > 0) {
    double current_avg = stats_.avg_processing_time_ms;
    double new_time = static_cast<double>(result.processing_time.count());
    stats_.avg_processing_time_ms =
        (current_avg * (stats_.total_exports - 1) + new_time) /
        stats_.total_exports;
  }
}

std::string ExportCoordinator::getDatabasePath() const {
  return ConfigManager::getInstance().getSQLiteDbPath();
}

} // namespace Coordinator
} // namespace PulseOne