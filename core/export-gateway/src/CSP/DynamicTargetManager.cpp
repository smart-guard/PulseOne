/**
 * @file DynamicTargetManager.cpp
 * @brief 동적 타겟 관리자 구현 (컴파일 에러 완전 수정)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 6.2.2 - 컴파일 에러 완전 수정
 *
 * 🔧 수정 내역 (v6.2.1 → v6.2.2):
 * 1. ✅ ExportTargetEntity.h 헤더 포함 (완전한 타입 정의)
 * 2. ✅ export_target_repo_ 멤버 변수 제거
 * 3. ✅ loadFromDatabase()에서 RepositoryFactory 직접 사용
 * 4. ✅ ExportTargetEntity 타입을 명시적으로 선언
 *
 * 이전 변경사항 (v6.2.0 → v6.2.1):
 * 1. config.getString() → config.getOrDefault()
 * 2. fp_config.timeout_seconds → fp_config.recovery_timeout_ms
 * 3. fp_config.half_open_max_calls → fp_config.half_open_max_attempts
 * 4. 문자열 연결 타입 에러 수정
 * 5. factory.getRepository<>() → factory.getExportTargetRepository()
 * 6. response_time_ms → response_time
 * 7. BatchTargetResult 필드명 수정
 * 8. stats.state → stats.current_state
 * 9. allowRequest() → canExecute()
 * 10. send() → sendAlarm()
 */

#include "CSP/DynamicTargetManager.h"
#include "CSP/FileTargetHandler.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "Client/RedisClientImpl.h"
#include "Database/Repositories/ExportTargetMappingRepository.h" // Added missing include
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
// ✅ v6.2.2: ExportTargetEntity.h 명시적 include (완전한 타입 정의)
#include "Constants/ExportConstants.h" // ✅ Added Constants
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include <algorithm>
#include <iostream>

namespace PulseOne {
namespace CSP {

using namespace PulseOne::Export;
using namespace PulseOne::Export;
using namespace PulseOne::Database;
namespace ExportConst = PulseOne::Constants::Export;

// =============================================================================
// 싱글턴 구현
// =============================================================================

DynamicTargetManager &DynamicTargetManager::getInstance() {
  static DynamicTargetManager instance;
  return instance;
}

// =============================================================================
// 생성자 및 소멸자 (private)
// =============================================================================

DynamicTargetManager::DynamicTargetManager() : publish_client_(nullptr) {

  LogManager::getInstance().Info("DynamicTargetManager 싱글턴 생성");

  startup_time_ = std::chrono::system_clock::now();

  // 기본 핸들러 등록
  registerDefaultHandlers();

  // 글로벌 설정 초기화
  global_settings_ = json{{"health_check_interval_sec", 60},
                          {"metrics_collection_interval_sec", 30},
                          {"max_concurrent_requests", 100}};

  LogManager::getInstance().Info("✅ PUBLISH 전용 Redis 연결 준비 완료");
}

DynamicTargetManager::~DynamicTargetManager() {
  try {
    stop();

    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    targets_.clear();
    handlers_.clear();
    failure_protectors_.clear();

    LogManager::getInstance().Info("DynamicTargetManager 소멸 완료");
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("소멸자 에러: " + std::string(e.what()));
  }
}

// =============================================================================
// ✅ Redis 연결 관리 (PUBLISH 전용)
// =============================================================================

bool DynamicTargetManager::initializePublishClient() {
  try {
    LogManager::getInstance().Info("PUBLISH 전용 Redis 연결 초기화 시작...");

    // 🔧 수정 1: ConfigManager의 올바른 API 사용
    auto &config = ConfigManager::getInstance();
    std::string redis_host =
        config.getOrDefault("REDIS_PRIMARY_HOST", "127.0.0.1");
    int redis_port = config.getInt("REDIS_PRIMARY_PORT", 6379);
    std::string redis_password =
        config.getOrDefault("REDIS_PRIMARY_PASSWORD", "");

    LogManager::getInstance().Info("Redis 연결 정보: " + redis_host + ":" +
                                   std::to_string(redis_port));

    // RedisClientImpl 인스턴스 생성
    publish_client_ = std::make_unique<RedisClientImpl>();

    // 연결 시도
    if (!publish_client_->connect(redis_host, redis_port, redis_password)) {
      LogManager::getInstance().Error("Redis 연결 실패: " + redis_host + ":" +
                                      std::to_string(redis_port));
      publish_client_.reset();
      return false;
    }

    // 연결 테스트
    if (!publish_client_->ping()) {
      LogManager::getInstance().Error("Redis PING 실패");
      publish_client_.reset();
      return false;
    }

    LogManager::getInstance().Info(
        "✅ PUBLISH 전용 Redis 연결 성공: " + redis_host + ":" +
        std::to_string(redis_port));

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Redis 연결 초기화 예외: " +
                                    std::string(e.what()));
    publish_client_.reset();
    return false;
  }
}

bool DynamicTargetManager::isRedisConnected() const {
  if (!publish_client_) {
    return false;
  }

  try {
    return publish_client_->isConnected();
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Redis 연결 상태 확인 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool DynamicTargetManager::start() {
  if (is_running_.load()) {
    LogManager::getInstance().Warn("이미 실행 중");
    return true;
  }

  LogManager::getInstance().Info("DynamicTargetManager 시작...");

  // ✅ 1. PUBLISH 전용 Redis 연결 초기화 (최우선)
  if (!initializePublishClient()) {
    LogManager::getInstance().Warn("PUBLISH 전용 Redis 연결 실패 - 계속 진행");
    // Redis 연결 실패해도 계속 진행 (다른 기능은 동작 가능)
  }

  // 2. DB에서 타겟 로드
  if (!loadFromDatabase()) {
    LogManager::getInstance().Error("DB 로드 실패");
    return false;
  }

  LogManager::getInstance().Info("DB에서 타겟 로드 완료");

  // 3. 각 타겟에 Failure Protector 생성
  {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);

    for (const auto &target : targets_) {
      if (target.enabled) {
        // 🔧 수정 2-3: FailureProtectorConfig 올바른 필드명 사용
        FailureProtectorConfig fp_config;
        fp_config.failure_threshold = 5;
        fp_config.recovery_timeout_ms = 30000; // 30초 = 30000ms
        fp_config.half_open_max_attempts =
            3; // half_open_max_calls → half_open_max_attempts

        failure_protectors_[target.name] =
            std::make_unique<FailureProtector>(target.name, fp_config);

        LogManager::getInstance().Debug("Failure Protector 생성: " +
                                        target.name);
      }
    }
  }

  // 4. 백그라운드 스레드 시작
  is_running_.store(true, std::memory_order_release);
  startBackgroundThreads();

  LogManager::getInstance().Info("DynamicTargetManager 시작 완료 ✅");
  // 🔧 수정 4: 문자열 연결 타입 에러 수정
  LogManager::getInstance().Info(
      "- PUBLISH Redis: " +
      std::string(isRedisConnected() ? "연결됨" : "연결안됨"));
  LogManager::getInstance().Info(
      "- 활성 타겟: " + std::to_string(targets_.size()) + "개");

  return true;
}

void DynamicTargetManager::stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().Info("DynamicTargetManager 중지 중...");

  should_stop_ = true;
  is_running_ = false;

  // 백그라운드 스레드 중지
  stopBackgroundThreads();

  // ✅ PUBLISH 전용 Redis 연결 정리
  if (publish_client_) {
    try {
      if (publish_client_->isConnected()) {
        publish_client_->disconnect();
        LogManager::getInstance().Info("PUBLISH Redis 연결 종료");
      }
      publish_client_.reset();
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Redis 연결 종료 예외: " +
                                      std::string(e.what()));
    }
  }

  LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
}

void DynamicTargetManager::setGatewayId(int id) {
  gateway_id_ = id;
  LogManager::getInstance().Info("계이트웨이 ID 설정됨: " + std::to_string(id));
}

// =============================================================================
// ✅ DB 기반 설정 관리 - 핵심 수정 부분!
// =============================================================================

bool DynamicTargetManager::loadFromDatabase() {
  if (should_stop_.load()) {
    return false;
  }

  try {
    DynamicTargetLoader loader;
    loader.setGatewayId(gateway_id_);

    // Loader를 통해 데이터 로드 (DB 연결, 쿼리, 파싱, 캐시 구성 등 수행)
    auto data = loader.loadFromDatabase();

    if (data.targets.empty()) {
      LogManager::getInstance().Warn("활성화된 타겟이 없음 (Manager)");
      std::unique_lock<std::shared_mutex> lock(targets_mutex_);
      targets_.clear();
      return false;
    }

    // 4. 새로운 타겟 리스트 생성 (Handler 및 Protector 초기화)
    std::unordered_map<std::string, std::unique_ptr<ITargetHandler>>
        new_handlers;
    std::unordered_map<std::string, std::unique_ptr<FailureProtector>>
        new_protectors;

    int loaded_count = 0;
    // Loader가 반환한 targets는 이미 정렬되어 있음
    for (auto &target : data.targets) {
      // Handler 생성 및 초기화
      auto handler =
          TargetHandlerFactory::getInstance().createHandler(target.type);
      if (handler) {
        if (handler->initialize(target.config)) {
          new_handlers[target.name] = std::move(handler);
          target.handler_initialized = true;
        } else {
          LogManager::getInstance().Warn("핸들러 초기화 실패: " + target.name);
        }
      }

      // Failure Protector 생성
      FailureProtectorConfig fp_config;
      if (target.config.contains(ExportConst::ConfigKeys::FAILURE_THRESHOLD))
        fp_config.failure_threshold =
            target.config[ExportConst::ConfigKeys::FAILURE_THRESHOLD];

      new_protectors[target.name] =
          std::make_unique<FailureProtector>(target.name, fp_config);

      loaded_count++;
    }

    // 5. 멤버 변수 교체 (Lock 보호)
    {
      std::unique_lock<std::shared_mutex> lock(targets_mutex_);

      targets_ = std::move(data.targets);
      handlers_ = std::move(new_handlers);
      failure_protectors_ = std::move(new_protectors);

      // 매핑 캐시 업데이트
      {
        std::unique_lock<std::shared_mutex> m_lock(mappings_mutex_);
        target_point_mappings_ = std::move(data.target_point_mappings);
        target_point_site_mappings_ =
            std::move(data.target_point_site_mappings);
        target_point_scales_ = std::move(data.target_point_scales);
        target_point_offsets_ = std::move(data.target_point_offsets);
        target_site_mappings_ = std::move(data.target_site_mappings);
      }

      // 할당된 디바이스 ID 목록 갱신 (Loader에서 이미 계산됨)
      assigned_device_ids_ = std::move(data.assigned_device_ids);
    }

    LogManager::getInstance().Info(
        "✅ 타겟 매니저 갱신 완료: " + std::to_string(loaded_count) + "개");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("타겟 로딩 중 예외 발생: " +
                                    std::string(e.what()));
    return false;
  }
}

// ✅ Helper methods moved to DynamicTargetLoader.cpp

bool DynamicTargetManager::forceReload() {
  LogManager::getInstance().Info("강제 리로드...");
  return loadFromDatabase();
}

bool DynamicTargetManager::reloadDynamicTargets() { return loadFromDatabase(); }

std::optional<DynamicTarget>
DynamicTargetManager::getTargetWithTemplate(const std::string &name) {
  return getTarget(name);
}

std::optional<DynamicTarget>
DynamicTargetManager::getTarget(const std::string &name) {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  auto it = findTarget(name);
  if (it != targets_.end()) {
    return *it;
  }

  return std::nullopt;
}

std::vector<DynamicTarget> DynamicTargetManager::getAllTargets() {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);
  return targets_;
}

std::set<std::string> DynamicTargetManager::getAssignedDeviceIds() const {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);
  return assigned_device_ids_;
}

// =============================================================================
// 타겟 관리 (나머지 메서드들)
// =============================================================================

bool DynamicTargetManager::addOrUpdateTarget(const DynamicTarget &target) {
  std::unique_lock<std::shared_mutex> lock(targets_mutex_);

  auto it = findTarget(target.name);
  if (it != targets_.end()) {
    *it = target;
    LogManager::getInstance().Info("타겟 업데이트: " + target.name);
  } else {
    targets_.push_back(target);
    LogManager::getInstance().Info("타겟 추가: " + target.name);
  }

  return true;
}

bool DynamicTargetManager::removeTarget(const std::string &name) {
  std::unique_lock<std::shared_mutex> lock(targets_mutex_);

  auto it = findTarget(name);
  if (it != targets_.end()) {
    targets_.erase(it);
    failure_protectors_.erase(name);
    LogManager::getInstance().Info("타겟 제거: " + name);
    return true;
  }

  return false;
}

bool DynamicTargetManager::setTargetEnabled(const std::string &name,
                                            bool enabled) {
  std::unique_lock<std::shared_mutex> lock(targets_mutex_);

  auto it = findTarget(name);
  if (it != targets_.end()) {
    it->enabled = enabled;
    LogManager::getInstance().Info("타겟 " + name + " " +
                                   (enabled ? "활성화" : "비활성화"));
    return true;
  }

  return false;
}

// =============================================================================
// 알람 전송
// =============================================================================

std::vector<TargetSendResult>
DynamicTargetManager::sendAlarmToTargets(const AlarmMessage &alarm) {
  std::vector<TargetSendResult> results;

  // ✅ 1. Redis PUBLISH (옵션 - 다른 시스템에 알람 전파)
  if (publish_client_ && publish_client_->isConnected()) {
    try {
      json alarm_json = alarm.to_json();

      int subscriber_count = publish_client_->publish(
          PulseOne::Constants::Export::Redis::CHANNEL_ALARMS_PROCESSED,
          alarm_json.dump());

      LogManager::getInstance().Debug(
          "알람 발행 완료: " + std::to_string(subscriber_count) + "명 구독 중");
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("알람 발행 실패: " +
                                     std::string(e.what()));
    }
  }

  // 2. export_mode="alarm"인 타겟으로만 전송
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  int filtered_count = 0;
  int sent_count = 0;

  for (size_t i = 0; i < targets_.size(); ++i) {
    if (!targets_[i].enabled) {
      continue;
    }

    // ✅ export_mode 체크
    std::string export_mode = ExportConst::ExportMode::ALARM; // 기본값
    if (targets_[i].config.contains(ExportConst::ConfigKeys::EXPORT_MODE)) {
      export_mode = targets_[i]
                        .config[ExportConst::ConfigKeys::EXPORT_MODE]
                        .get<std::string>();
    }

    std::string mode_upper = export_mode;
    std::transform(mode_upper.begin(), mode_upper.end(), mode_upper.begin(),
                   ::toupper);

    if (mode_upper != "ALARM" &&
        mode_upper != ExportConst::ExportMode::EVENT) { // EVENT is already
                                                        // uppercase in constant
      filtered_count++;
      LogManager::getInstance().Debug("타겟 스킵 (export_mode=" + export_mode +
                                      "): " + targets_[i].name);
      continue;
    }

    // ✅ 지연 전송 적용
    if (targets_[i].execution_delay_ms > 0) {
      LogManager::getInstance().Info(
          "--- [DELAY] 타겟 '" + targets_[i].name + "' 전송 전 " +
          std::to_string(targets_[i].execution_delay_ms) + "ms 대기 중... ---");
      std::this_thread::sleep_for(
          std::chrono::milliseconds(targets_[i].execution_delay_ms));
    }

    TargetSendResult result;
    result.target_name = targets_[i].name;
    result.target_type = targets_[i].type;

    auto start_time = std::chrono::steady_clock::now();

    if (processTargetByIndex(i, alarm, result)) {
      total_successes_.fetch_add(1);
      sent_count++;
    } else {
      total_failures_.fetch_add(1);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.response_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time);

    results.push_back(result);
  }

  total_requests_.fetch_add(results.size());

  if (results.empty()) {
    LogManager::getInstance().Warn(
        "알람 타겟 없음 (필터링: " + std::to_string(filtered_count) + "개)");
  } else {
    LogManager::getInstance().Info(
        "알람 전송 완료: " + std::to_string(sent_count) + "개 타겟 " +
        "(필터링: " + std::to_string(filtered_count) + "개)");
  }

  return results;
}

TargetSendResult
DynamicTargetManager::sendAlarmToTarget(const std::string &target_name,
                                        const AlarmMessage &alarm) {

  TargetSendResult result;
  result.target_name = target_name;

  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  auto it = findTarget(target_name);
  if (it == targets_.end()) {
    result.success = false;
    result.error_message = "타겟을 찾을 수 없음: " + target_name;
    return result;
  }

  if (!it->enabled) {
    result.success = false;
    result.error_message = "타겟이 비활성화됨: " + target_name;
    return result;
  }

  result.target_type = it->type;

  auto start_time = std::chrono::steady_clock::now();

  size_t index = std::distance(targets_.begin(), it);
  processTargetByIndex(index, alarm, result);

  auto end_time = std::chrono::steady_clock::now();
  // 🔧 수정 6: response_time_ms → response_time (duration 타입)
  result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  return result;
}

std::vector<TargetSendResult>
DynamicTargetManager::sendFileToTargets(const std::string &local_path) {
  std::vector<TargetSendResult> results;

  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  for (size_t i = 0; i < targets_.size(); ++i) {
    if (!targets_[i].enabled)
      continue;

    // export_mode가 ALARM 또는 EVENT인 경우에만 파일 전송 고려 (기본값)
    // S3 핸들러 등에서 sendFile을 지원하는지 확인
    auto it_handler = handlers_.find(targets_[i].name);
    if (it_handler != handlers_.end() && it_handler->second) {
      LogManager::getInstance().Info("[DynamicTargetManager] 파일 전송 시작: " +
                                     targets_[i].name + " -> " + local_path);

      auto result =
          it_handler->second->sendFile(local_path, targets_[i].config);
      result.target_name = targets_[i].name;
      result.target_type = targets_[i].type;

      results.push_back(result);

      if (result.success) {
        total_successes_++;
        total_bytes_sent_ += result.content_size;
      } else {
        total_failures_++;
      }
    }
  }

  return results;
}

BatchTargetResult DynamicTargetManager::sendAlarmBatchToTargets(
    const std::vector<AlarmMessage> &alarms,
    const std::string &specific_target) {
  BatchTargetResult batch_result;

  if (alarms.empty()) {
    return batch_result;
  }

  // 1. Redis PUBLISH (개별 알람 발행 - 배치는 알람별로 루프 필요)
  if (publish_client_ && publish_client_->isConnected()) {
    for (const auto &alarm : alarms) {
      try {
        json alarm_json = alarm.to_json(); // helper or manual packing
        publish_client_->publish(
            PulseOne::Constants::Export::Redis::CHANNEL_ALARMS_PROCESSED,
            alarm_json.dump());
      } catch (...) {
      }
    }
  }

  // 2. 모든 활성 타겟에 대해 배치 전송 호출
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  for (size_t i = 0; i < targets_.size(); ++i) {
    std::cout << "[DEBUG][DynamicTargetManager] Evaluating target: "
              << targets_[i].name << " Type: " << targets_[i].type
              << " Enabled: " << (targets_[i].enabled ? "Yes" : "No")
              << std::endl;
    LogManager::getInstance().Debug(
        "[DynamicTargetManager] Evaluating target: " + targets_[i].name +
        " (Type: " + targets_[i].type +
        ", Enabled: " + (targets_[i].enabled ? "Yes" : "No") + ")");

    if (!targets_[i].enabled)
      continue;

    // 특정 타겟 필터링 (비어있지 않은 경우 전용)
    if (!specific_target.empty() && targets_[i].name != specific_target) {
      LogManager::getInstance().Debug(
          "[DynamicTargetManager] Skipped target " + targets_[i].name +
          " due to specific_target filter: " + specific_target);
      continue;
    }

    // export_mode 체크
    std::string export_mode = ExportConst::ExportMode::ALARM;
    if (targets_[i].config.contains(ExportConst::ConfigKeys::EXPORT_MODE)) {
      export_mode = targets_[i]
                        .config[ExportConst::ConfigKeys::EXPORT_MODE]
                        .get<std::string>();
    }

    if (export_mode != ExportConst::ExportMode::ALARM &&
        export_mode != ExportConst::ExportMode::EVENT) {
      LogManager::getInstance().Debug("[DynamicTargetManager] Skipped target " +
                                      targets_[i].name +
                                      " due to export_mode: " + export_mode);
      continue;
    }

    // ✅ 지연 전송 적용
    if (targets_[i].execution_delay_ms > 0) {
      LogManager::getInstance().Info(
          "--- [BATCH DELAY] 타겟 '" + targets_[i].name + "' 배치 전송 전 " +
          std::to_string(targets_[i].execution_delay_ms) + "ms 대기 중... ---");
      std::this_thread::sleep_for(
          std::chrono::milliseconds(targets_[i].execution_delay_ms));
    }

    auto it_handler = handlers_.find(targets_[i].name);
    if (it_handler == handlers_.end() || !it_handler->second) {
      LogManager::getInstance().Warn(
          "[DynamicTargetManager] Handler not found for target: " +
          targets_[i].name + " (Type: " + targets_[i].type + ")");
      batch_result.failed_targets += alarms.size(); // 대략적인 실패 카운트
      continue;
    }

    // ✅ 배치 내 각 알람에 대해 매핑 로직 적용 (processTargetByIndex 로직 복제)
    std::vector<AlarmMessage> processed_batch;
    processed_batch.reserve(alarms.size());

    for (const auto &raw_alarm : alarms) {
      AlarmMessage alarm = raw_alarm; // 복사본 생성

      // 1. 포인트 이름 매핑
      {
        std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);
        auto it1 = target_point_mappings_.find(targets_[i].id);
        if (it1 != target_point_mappings_.end()) {
          auto it2 = it1->second.find(alarm.point_id);
          if (it2 != it1->second.end()) {
            if (!it2->second.empty()) {
              alarm.point_name = it2->second;
            }
          }
        }
      }

      // 1.5. Site ID 오버라이드
      int lookup_site_id = alarm.site_id;
      {
        std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);

        // [DEBUG] Override Check
        if (target_point_site_mappings_.count(targets_[i].id)) {
          if (target_point_site_mappings_[targets_[i].id].count(
                  alarm.point_id)) {
            lookup_site_id =
                target_point_site_mappings_[targets_[i].id].at(alarm.point_id);
          }
        }

        auto it1 = target_point_site_mappings_.find(targets_[i].id);
        if (it1 != target_point_site_mappings_.end()) {
          auto it2 = it1->second.find(alarm.point_id);
          if (it2 != it1->second.end()) {
            lookup_site_id = it2->second;
          }
        }
      }

      // 1.6 Building ID 직접 매핑 (Removed)

      // 2. 빌딩 ID 매핑
      std::string mapped_bd_str;
      {
        std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);
        auto it1 = target_site_mappings_.find(targets_[i].id);
        if (it1 != target_site_mappings_.end()) {
          auto it2 = it1->second.find(lookup_site_id);
          if (it2 != it1->second.end()) {
            mapped_bd_str = it2->second;
          }
        }
      }

      // fallback to lookup_site_id if no mapping found but override happened
      if (mapped_bd_str.empty()) {
        // If override differed from original site_id, use override as building
        // ID
        if (lookup_site_id != raw_alarm.site_id) {
          mapped_bd_str = std::to_string(lookup_site_id);
        }
        // Else check config (omitted for brevity, assume DB primary)
      }

      if (!mapped_bd_str.empty()) {
        try {
          alarm.site_id = std::stoi(mapped_bd_str);
        } catch (...) {
        }
      } else {
        // [FIX] 만약 사이트 매핑이 없으면 오버라이드된 lookup_site_id(280 등)를
        // 직접 bd로 사용
        alarm.site_id = lookup_site_id;
      }

      processed_batch.push_back(alarm);
    }

    std::cout << "[DEBUG][DynamicTargetManager] Dispatching "
              << processed_batch.size()
              << " alarms to target: " << targets_[i].name << std::endl;
    LogManager::getInstance().Info("[DynamicTargetManager] Dispatching " +
                                   std::to_string(alarms.size()) +
                                   " alarms to target: " + targets_[i].name);

    // auto start_time = std::chrono::steady_clock::now(); // Unused variable
    // removed

    std::vector<TargetSendResult> results =
        it_handler->second->sendAlarmBatch(processed_batch, targets_[i].config);

    // auto end_time = std::chrono::steady_clock::now(); // Unused variable
    // removed 배치 전체 처리 시간 (개별 결과에는 각각의 시간이 있을 수 있음)

    for (const auto &res : results) {
      if (res.success) {
        batch_result.successful_targets++;
        // 타겟 통계 업데이트 (성공)
        targets_[i].success_count++;
      } else {
        batch_result.failed_targets++;
        // 타겟 통계 업데이트 (실패)
        targets_[i].failure_count++;
      }
      batch_result.results.push_back(res);
    }
  }

  batch_result.total_targets =
      batch_result.successful_targets + batch_result.failed_targets;

  if (batch_result.successful_targets > 0) {
    LogManager::getInstance().Info(
        "배치 알람 전송 완료: 성공 " +
        std::to_string(batch_result.successful_targets) + ", 실패 " +
        std::to_string(batch_result.failed_targets));
  }

  return batch_result;
}

BatchTargetResult DynamicTargetManager::sendValueBatchToTargets(
    const std::vector<PulseOne::CSP::ValueMessage> &values,
    const std::string & /*type*/, const std::string &specific_target) {
  BatchTargetResult batch_result;

  if (values.empty()) {
    return batch_result;
  }

  LogManager::getInstance().Info(
      "[v3.2.0 Debug] sendValueBatchToTargets called for specific_target: " +
      specific_target + ", values: " + std::to_string(values.size()));
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  for (size_t i = 0; i < targets_.size(); ++i) {
    LogManager::getInstance().Info(
        "[v3.2.0 Debug] Checking target: " + targets_[i].name +
        " (Enabled: " + (targets_[i].enabled ? "Yes" : "No") + ")");
    if (!targets_[i].enabled)
      continue;

    // ✅ 특정 타겟 요청 시 필터링
    if (!specific_target.empty() && targets_[i].name != specific_target) {
      continue;
    }

    // export_mode 체크 (value 모드 확인)
    std::string export_mode = ExportConst::ExportMode::ALARM;
    if (targets_[i].config.contains(ExportConst::ConfigKeys::EXPORT_MODE)) {
      export_mode = targets_[i]
                        .config[ExportConst::ConfigKeys::EXPORT_MODE]
                        .get<std::string>();
    }

    // "value" 또는 "batch" 모드여야 함 (단, 특정 타켓 지정시 예외 허용)
    if (specific_target.empty() &&
        export_mode != ExportConst::ExportMode::VALUE &&
        export_mode != ExportConst::ExportMode::BATCH) {
      continue;
    }

    auto it_handler = handlers_.find(targets_[i].name);
    if (it_handler == handlers_.end() || !it_handler->second) {
      continue;
    }

    // HANDLER에게 배치 위임
    std::vector<TargetSendResult> results =
        it_handler->second->sendValueBatch(values, targets_[i].config);

    for (const auto &res : results) {
      if (res.success) {
        batch_result.successful_targets++;
        targets_[i].success_count++;
      } else {
        batch_result.failed_targets++;
        targets_[i].failure_count++;
      }
      batch_result.results.push_back(res);
    }
  }

  batch_result.total_targets =
      batch_result.successful_targets + batch_result.failed_targets;

  return batch_result;
}

std::future<std::vector<TargetSendResult>>
DynamicTargetManager::sendAlarmAsync(const AlarmMessage &alarm) {
  return std::async(std::launch::async,
                    [this, alarm]() { return sendAlarmToTargets(alarm); });
}

// =============================================================================
// Failure Protector 관리
// =============================================================================

FailureProtectorStats DynamicTargetManager::getFailureProtectorStatus(
    const std::string &target_name) const {
  auto it = failure_protectors_.find(target_name);
  if (it != failure_protectors_.end()) {
    return it->second->getStats();
  }

  return FailureProtectorStats{};
}

void DynamicTargetManager::resetFailureProtector(
    const std::string &target_name) {
  auto it = failure_protectors_.find(target_name);
  if (it != failure_protectors_.end()) {
    it->second->reset();
  }
}

void DynamicTargetManager::resetAllFailureProtectors() {
  for (auto &[name, protector] : failure_protectors_) {
    protector->reset();
  }
}

void DynamicTargetManager::forceOpenFailureProtector(
    const std::string &target_name) {
  auto it = failure_protectors_.find(target_name);
  if (it != failure_protectors_.end()) {
    LogManager::getInstance().Info("강제 OPEN: " + target_name);
  }
}

std::unordered_map<std::string, FailureProtectorStats>
DynamicTargetManager::getFailureProtectorStats() const {
  std::unordered_map<std::string, FailureProtectorStats> stats;

  for (const auto &[name, protector] : failure_protectors_) {
    stats[name] = protector->getStats();
  }

  return stats;
}

// =============================================================================
// 핸들러 관리
// =============================================================================

void DynamicTargetManager::registerDefaultHandlers() {
  // ✅ v3.0: REGISTER_TARGET_HANDLER 매크로 사용으로 변경되어
  // 명시적 등록이 필요 없으나, 하위 호환성을 위해 유지하거나 빈 함수로 둠
  LogManager::getInstance().Info("기본 핸들러 등록 완료 (Factory 기반)");
}

bool DynamicTargetManager::registerHandler(
    const std::string &type_name, std::unique_ptr<ITargetHandler> handler) {

  if (!handler) {
    return false;
  }

  handlers_[type_name] = std::move(handler);
  LogManager::getInstance().Info("핸들러 등록: " + type_name);

  return true;
}

bool DynamicTargetManager::unregisterHandler(const std::string &type_name) {
  return handlers_.erase(type_name) > 0;
}

std::vector<std::string>
DynamicTargetManager::getSupportedHandlerTypes() const {
  std::vector<std::string> types;

  for (const auto &[type, _] : handlers_) {
    types.push_back(type);
  }

  return types;
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

json DynamicTargetManager::getStatistics() const {
  auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - startup_time_)
                    .count();

  uint64_t total_reqs = total_requests_.load();
  uint64_t avg_response_time =
      total_reqs > 0 ? (total_response_time_ms_.load() / total_reqs) : 0;

  return json{
      {"total_requests", total_reqs},
      {"total_successes", total_successes_.load()},
      {"total_failures", total_failures_.load()},
      {"success_rate", total_reqs > 0
                           ? (double)total_successes_.load() / total_reqs * 100
                           : 0.0},
      {"concurrent_requests", concurrent_requests_.load()},
      {"peak_concurrent_requests", peak_concurrent_requests_.load()},
      {"total_bytes_sent", total_bytes_sent_.load()},
      {"avg_response_time_ms", avg_response_time},
      {"uptime_seconds", uptime}};
}

void DynamicTargetManager::resetStatistics() {
  total_requests_ = 0;
  total_successes_ = 0;
  total_failures_ = 0;
  concurrent_requests_ = 0;
  peak_concurrent_requests_ = 0;
  total_bytes_sent_ = 0;
  total_response_time_ms_ = 0;

  LogManager::getInstance().Info("통계 리셋 완료");
}

json DynamicTargetManager::healthCheck() const {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);

  int enabled_count = 0;
  int healthy_count = 0;

  for (const auto &target : targets_) {
    if (target.enabled) {
      enabled_count++;

      auto it = failure_protectors_.find(target.name);
      if (it != failure_protectors_.end()) {
        auto stats = it->second->getStats();
        // 🔧 수정 8: stats.state → stats.current_state
        if (stats.current_state != "OPEN") {
          healthy_count++;
        }
      }
    }
  }

  bool redis_connected = isRedisConnected();

  return json{{"status", is_running_.load() ? "running" : "stopped"},
              {"redis_connected", redis_connected},
              {"total_targets", targets_.size()},
              {"enabled_targets", enabled_count},
              {"healthy_targets", healthy_count},
              {"handlers_count", handlers_.size()}};
}

void DynamicTargetManager::updateGlobalSettings(const json &settings) {
  global_settings_ = settings;
  LogManager::getInstance().Info("글로벌 설정 업데이트");
}

// =============================================================================
// Private 유틸리티 메서드들
// =============================================================================

std::vector<DynamicTarget>::iterator
DynamicTargetManager::findTarget(const std::string &target_name) {
  return std::find_if(
      targets_.begin(), targets_.end(),
      [&target_name](const DynamicTarget &t) { return t.name == target_name; });
}

std::vector<DynamicTarget>::const_iterator
DynamicTargetManager::findTarget(const std::string &target_name) const {
  return std::find_if(
      targets_.begin(), targets_.end(),
      [&target_name](const DynamicTarget &t) { return t.name == target_name; });
}

bool DynamicTargetManager::processTargetByIndex(size_t index,
                                                const AlarmMessage &alarm,
                                                TargetSendResult &result) {

  const auto &target = targets_[index];

  // ✅ 타겟 정보 설정
  result.target_name = target.name;
  result.target_id = target.id; // ✅ Target ID 보장 (v3.2.0)
  result.target_type = target.type;

  auto handler_it = handlers_.find(target.name);
  if (handler_it == handlers_.end()) {
    result.success = false;
    result.error_message = "핸들러를 찾을 수 없음 (이름): " + target.name +
                           " (타입: " + target.type + ")";
    return false;
  }

  auto fp_it = failure_protectors_.find(target.name);
  if (fp_it != failure_protectors_.end() && !fp_it->second->canExecute()) {
    result.success = false;
    result.error_message = "Circuit Breaker OPEN 상태";

    auto stats = fp_it->second->getStats();
    LogManager::getInstance().Warn(
        "Circuit Breaker OPEN: " + target.name +
        " (실패: " + std::to_string(stats.total_failures) + "회)");

    return false;
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    // ✅ 1. 포인트 이름/Scale/Offset 매핑
    std::string mapped_name;
    double applied_scale = 1.0;
    double applied_offset = 0.0;
    {
      std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);
      if (target_point_mappings_.count(target.id)) {
        auto &m = target_point_mappings_[target.id];
        if (m.count(alarm.point_id)) {
          mapped_name = m.at(alarm.point_id);
          LogManager::getInstance().Info("[DEBUG-MAPPING] NAME FOUND: Point " +
                                         std::to_string(alarm.point_id) +
                                         " -> " + mapped_name);
        }
      }

      if (target_point_scales_.count(target.id)) {
        auto &m = target_point_scales_[target.id];
        if (m.count(alarm.point_id)) {
          applied_scale = m.at(alarm.point_id);
          LogManager::getInstance().Info("[DEBUG-MAPPING] SCALE FOUND: " +
                                         std::to_string(applied_scale));
        }
      }

      if (target_point_offsets_.count(target.id)) {
        auto &m = target_point_offsets_[target.id];
        if (m.count(alarm.point_id)) {
          applied_offset = m.at(alarm.point_id);
          LogManager::getInstance().Info("[DEBUG-MAPPING] OFFSET FOUND: " +
                                         std::to_string(applied_offset));
        }
      }
    }

    // ✅ 1.5. 포인트 기반 Site ID 오버라이드
    int lookup_site_id = alarm.site_id;
    {
      std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);

      if (target_point_site_mappings_.count(target.id)) {
        auto &m = target_point_site_mappings_[target.id];
        LogManager::getInstance().Info(
            "[DEBUG-MAPPING] Target " + std::to_string(target.id) +
            " mapping check: PointID=" + std::to_string(alarm.point_id) +
            ", MapSize=" + std::to_string(m.size()));

        if (m.count(alarm.point_id)) {
          lookup_site_id = m.at(alarm.point_id);
          LogManager::getInstance().Info(
              "[DEBUG-MAPPING] OVERRIDE SUCCESS! Point " +
              std::to_string(alarm.point_id) +
              " -> SiteID: " + std::to_string(lookup_site_id));
        } else {
          LogManager::getInstance().Info(
              "[DEBUG-MAPPING] Point override NOT found for ID: " +
              std::to_string(alarm.point_id));
        }
      } else {
        LogManager::getInstance().Info(
            "[DEBUG-MAPPING] No point-site mappings for target ID: " +
            std::to_string(target.id));
      }
    }

    // ✅ 2. 빌딩 ID 매핑
    std::string mapped_bd_str;
    // int mapped_bd_int = 0; // Unused variable removed

    {
      std::shared_lock<std::shared_mutex> m_lock(mappings_mutex_);

      // 2.2 사이트 기반 빌딩 ID 매핑 확인
      if (true) {
        auto it1 = target_site_mappings_.find(target.id);
        if (it1 != target_site_mappings_.end()) {
          auto it2 = it1->second.find(lookup_site_id); // ✅ lookup_site_id 사용
          if (it2 != it1->second.end()) {
            mapped_bd_str = it2->second;
            LogManager::getInstance().Info(
                "[DEBUG-MAPPING] SITE-TO-BD FOUND: SiteID " +
                std::to_string(lookup_site_id) + " -> BD: " + mapped_bd_str);
          } else {
            LogManager::getInstance().Info(
                "[DEBUG-MAPPING] SITE-TO-BD NOT FOUND for SiteID: " +
                std::to_string(lookup_site_id));
          }
        }
      }
    }

    // DB 매핑이 없으면 Config에서 찾음 (fallback)
    if (mapped_bd_str.empty() &&
        target.config.contains(ExportConst::ConfigKeys::SITE_MAPPING) &&
        target.config[ExportConst::ConfigKeys::SITE_MAPPING].is_object()) {
      std::string site_id_str =
          std::to_string(lookup_site_id); // ✅ lookup_site_id 사용
      if (target.config[ExportConst::ConfigKeys::SITE_MAPPING].contains(
              site_id_str)) {
        auto val =
            target.config[ExportConst::ConfigKeys::SITE_MAPPING][site_id_str];
        mapped_bd_str = val.is_number() ? std::to_string(val.get<int>())
                                        : val.get<std::string>();
        LogManager::getInstance().Info(
            "[DEBUG-MAPPING] CONFIG-TO-BD FOUND: SiteID " + site_id_str +
            " -> BD: " + mapped_bd_str);
      }
    }

    int mapped_bd = 0;
    if (!mapped_bd_str.empty()) {
      try {
        mapped_bd = std::stoi(mapped_bd_str);
      } catch (...) {
        LogManager::getInstance().Warn("변환된 빌딩 ID가 숫자가 아님: " +
                                       mapped_bd_str);
      }
    }

    // ✅ 3. 매핑된 알람 메시지 생성
    AlarmMessage mapped_alarm = alarm;

    // Scale/Offset 적용
    if (applied_scale != 1.0 || applied_offset != 0.0) {
      mapped_alarm.measured_value =
          (alarm.measured_value * applied_scale) + applied_offset;
      LogManager::getInstance().Debug(
          "값 보정 적용: " + std::to_string(alarm.measured_value) + " -> " +
          std::to_string(mapped_alarm.measured_value) +
          " (Scale=" + std::to_string(applied_scale) +
          ", Offset=" + std::to_string(applied_offset) + ")");
    }

    if (!mapped_name.empty()) {
      mapped_alarm.point_name = mapped_name;
      LogManager::getInstance().Debug(
          "포인트 이름 매핑 적용: " + alarm.point_name + " -> " + mapped_name);
    }

    // [FIX] 수동 전송시 유저 입력 우선, 아니면 매핑값 적용, 없으면
    // lookup_site_id 적용
    if (alarm.manual_override && alarm.site_id > 0) {
      mapped_alarm.site_id = alarm.site_id;
      LogManager::getInstance().Info("[PRIORITY] Manual BD preserved: " +
                                     std::to_string(alarm.site_id));
    } else if (mapped_bd > 0) {
      mapped_alarm.site_id = mapped_bd;
      LogManager::getInstance().Debug(
          "빌딩 ID 매핑 적용: " + std::to_string(alarm.site_id) + " -> " +
          std::to_string(mapped_bd));
    } else {
      mapped_alarm.site_id = lookup_site_id;
      LogManager::getInstance().Debug("Site ID 매핑 적용 (BD): " +
                                      std::to_string(lookup_site_id));
    }

    // ✅ 4. 핸들러 호출
    auto handler_result =
        handler_it->second->sendAlarm(mapped_alarm, target.config);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // ✅ 필요한 필드만 복사
    result.success = handler_result.success;
    result.status_code =
        handler_result.status_code; // http_status_code → status_code
    result.response_time = handler_result.response_time;
    result.error_message = handler_result.error_message;
    result.content_size =
        handler_result.content_size; // bytes_sent → content_size
    result.retry_count = handler_result.retry_count;

    // Circuit Breaker 업데이트
    if (fp_it != failure_protectors_.end()) {
      if (result.success) {
        fp_it->second->recordSuccess(); // 파라미터 제거
      } else {
        fp_it->second->recordFailure();
      }
    }

    // 통계 업데이트
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    if (result.success) {
      total_successes_.fetch_add(1, std::memory_order_relaxed);
    } else {
      total_failures_.fetch_add(1, std::memory_order_relaxed);
    }

    total_response_time_ms_.fetch_add(duration.count(),
                                      std::memory_order_relaxed);
    // total_bytes_sent_ 라인 제거 (content_size는 TargetSendResult에만 있음)

    return result.success;

  } catch (const std::exception &e) {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    result.success = false;
    result.error_message = "핸들러 예외: " + std::string(e.what());
    result.response_time = duration;

    if (fp_it != failure_protectors_.end()) {
      fp_it->second->recordFailure();
    }

    total_requests_.fetch_add(1, std::memory_order_relaxed);
    total_failures_.fetch_add(1, std::memory_order_relaxed);

    LogManager::getInstance().Error("타겟 처리 예외: " + target.name + " - " +
                                    std::string(e.what()));

    return false;
  }
}

json DynamicTargetManager::expandConfigVariables(
    const json &config, const AlarmMessage & /*alarm*/) {
  json expanded = config;

  // 간단한 변수 치환 로직
  if (config.contains("url") && config["url"].is_string()) {
    std::string url = config["url"].get<std::string>();
    // 예: {bd}, {nm} 등을 실제 값으로 치환
    // 구현 생략 (필요시 추가)
    expanded["url"] = url;
  }

  return expanded;
}

// =============================================================================
// 백그라운드 스레드
// =============================================================================

void DynamicTargetManager::startBackgroundThreads() {
  LogManager::getInstance().Info("백그라운드 스레드 시작");

  health_check_thread_ =
      std::make_unique<std::thread>([this]() { healthCheckThread(); });

  metrics_thread_ =
      std::make_unique<std::thread>([this]() { metricsCollectorThread(); });

  cleanup_thread_ =
      std::make_unique<std::thread>([this]() { cleanupThread(); });
}

void DynamicTargetManager::stopBackgroundThreads() {
  LogManager::getInstance().Info("백그라운드 스레드 중지");

  // 스레드들이 자고 있다면 즉시 깨움
  should_stop_.store(true);
  cv_.notify_all();

  if (health_check_thread_ && health_check_thread_->joinable()) {
    health_check_thread_->join();
  }

  if (metrics_thread_ && metrics_thread_->joinable()) {
    metrics_thread_->join();
  }

  if (cleanup_thread_ && cleanup_thread_->joinable()) {
    cleanup_thread_->join();
  }
}

// 헬스체크 스레드 (60초 주기)
void DynamicTargetManager::healthCheckThread() {
  while (!should_stop_.load()) {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    cv_.wait_for(lock, std::chrono::seconds(60),
                 [this] { return should_stop_.load(); });

    if (should_stop_.load())
      break;

    // 헬스체크 로직 (생략)
  }
}

// 메트릭 수집 스레드 (30초 주기)
void DynamicTargetManager::metricsCollectorThread() {
  while (!should_stop_.load()) {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    cv_.wait_for(lock, std::chrono::seconds(30),
                 [this] { return should_stop_.load(); });

    if (should_stop_.load())
      break;

    // 메트릭 수집 로직 (생략)
  }
}

// 정리 스레드 (300초 주기)
void DynamicTargetManager::cleanupThread() {
  while (!should_stop_.load()) {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    cv_.wait_for(lock, std::chrono::seconds(300),
                 [this] { return should_stop_.load(); });

    if (should_stop_.load())
      break;

    // 정리 로직 (생략)
  }
}

} // namespace CSP
} // namespace PulseOne