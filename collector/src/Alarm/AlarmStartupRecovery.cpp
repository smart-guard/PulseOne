/**
 * @file AlarmStartupRecovery.cpp  
 * @brief 시스템 시작 시 DB에서 활성 알람을 Redis로 복구하는 구현
 * @date 2025-08-31
 */

#include "Alarm/AlarmStartupRecovery.h"
#include "Storage/BackendFormat.h" 
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace PulseOne::Alarm;
using namespace PulseOne::Storage;
// =============================================================================
// 싱글톤 구현
// =============================================================================

AlarmStartupRecovery& AlarmStartupRecovery::getInstance() {
    static AlarmStartupRecovery instance;
    return instance;
}

AlarmStartupRecovery::AlarmStartupRecovery() {
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, 
                                  "AlarmStartupRecovery 인스턴스 생성");
    
    // 통계 초기화
    ResetRecoveryStats();
    
    // 컴포넌트 초기화는 지연로딩 (RecoverActiveAlarms 호출 시)
}

AlarmStartupRecovery::~AlarmStartupRecovery() {
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "AlarmStartupRecovery 소멸");
}

// =============================================================================
// 핵심 복구 메서드 구현
// =============================================================================

size_t AlarmStartupRecovery::RecoverActiveAlarms() {
    if (!recovery_enabled_.load()) {
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "알람 복구가 비활성화됨 - 건너뜀");
        return 0;
    }
    
    if (recovery_in_progress_.exchange(true)) {
        LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                      "알람 복구가 이미 진행 중입니다");
        return 0;
    }
    
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "=== 시스템 시작 시 활성 알람 복구 시작 ===");
    
    auto start_time = std::chrono::steady_clock::now();
    size_t total_recovered = 0;
    
    try {
        // 1. 컴포넌트 초기화
        if (!InitializeComponents()) {
            HandleRecoveryError("컴포넌트 초기화", "Repository 또는 Redis 연결 실패");
            recovery_in_progress_.store(false);
            return 0;
        }
        
        // 2. DB에서 활성 알람 로드
        auto active_alarms = LoadActiveAlarmsFromDB();
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        recovery_stats_.total_active_alarms = active_alarms.size();
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "DB에서 " + std::to_string(active_alarms.size()) + 
                                      "개의 활성 알람 발견");
        
        if (active_alarms.empty()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                          "복구할 활성 알람이 없습니다");
            recovery_completed_.store(true);
            recovery_in_progress_.store(false);
            return 0;
        }
        
        // 3. 배치 단위로 Redis 발행
        size_t batch_count = 0;
        size_t success_count = 0;
        
        for (const auto& alarm_entity : active_alarms) {
            try {
                // 유효성 검증
                if (!ValidateAlarmForRecovery(alarm_entity)) {
                    recovery_stats_.invalid_alarms++;
                    continue;
                }
                
                // Backend 포맷으로 변환
                auto backend_alarm = ConvertToBackendFormat(alarm_entity);
                
                // Redis 발행 (재시도 포함)
                bool published = false;
                for (int retry = 0; retry < REDIS_PUBLISH_RETRY_COUNT; ++retry) {
                    if (PublishAlarmToRedis(backend_alarm)) {
                        published = true;
                        break;
                    }
                    
                    if (retry < REDIS_PUBLISH_RETRY_COUNT - 1) {
                        std::this_thread::sleep_for(RETRY_DELAY);
                        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG_LEVEL,
                                                      "Redis 발행 재시도 " + std::to_string(retry + 1) + 
                                                      "/3: 알람 ID " + std::to_string(alarm_entity.getId()));
                    }
                }
                
                if (published) {
                    success_count++;
                    recovery_stats_.successfully_published++;
                    
                    LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG_LEVEL,
                                                  "활성 알람 복구 성공: rule_id=" + 
                                                  std::to_string(alarm_entity.getRuleId()) +
                                                  ", severity=" + std::to_string(static_cast<int>(alarm_entity.getSeverity())));
                } else {
                    recovery_stats_.failed_to_publish++;
                    LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                                  "활성 알람 Redis 발행 실패: rule_id=" + 
                                                  std::to_string(alarm_entity.getRuleId()));
                }
                
                // 배치 처리 진행상황
                batch_count++;
                if (batch_count % MAX_RECOVERY_BATCH_SIZE == 0) {
                    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                                  "복구 진행: " + std::to_string(batch_count) + "/" + 
                                                  std::to_string(active_alarms.size()) + " 처리됨");
                }
                
            } catch (const std::exception& e) {
                recovery_stats_.failed_to_publish++;
                HandleRecoveryError("개별 알람 복구", 
                                    "Rule ID " + std::to_string(alarm_entity.getRuleId()) + 
                                    ": " + e.what());
            }
        }
        
        total_recovered = success_count;
        
        // 4. 복구 완료 처리
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            recovery_stats_.recovery_duration = duration;
            
            // 현재 시간 문자열 생성
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            recovery_stats_.last_recovery_time = ss.str();
        }
        
        recovery_completed_.store(true);
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "=== 활성 알람 복구 완료 ===");
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "- 전체 활성 알람: " + std::to_string(active_alarms.size()) + "개");
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "- 성공적으로 복구: " + std::to_string(success_count) + "개");
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "- 실패: " + std::to_string(recovery_stats_.failed_to_publish) + "개");
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "- 소요시간: " + std::to_string(duration.count()) + "ms");
        
        if (success_count > 0) {
            LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                          "Frontend activealarm 페이지에서 " + 
                                          std::to_string(success_count) + 
                                          "개의 활성 알람을 확인할 수 있습니다");
        }
        
    } catch (const std::exception& e) {
        HandleRecoveryError("전체 복구 프로세스", e.what());
        total_recovered = 0;
    }
    
    recovery_in_progress_.store(false);
    return total_recovered;
}

size_t AlarmStartupRecovery::RecoverActiveAlarmsByTenant(int tenant_id) {
    if (!recovery_enabled_.load()) {
        return 0;
    }
    
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "테넌트 " + std::to_string(tenant_id) + 
                                  "의 활성 알람 복구 시작");
    
    try {
        if (!InitializeComponents()) {
            return 0;
        }
        
        // 특정 테넌트의 활성 알람만 조회
        auto active_alarms = alarm_occurrence_repo_->findByTenant(tenant_id, "active");
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "테넌트 " + std::to_string(tenant_id) + 
                                      "에서 " + std::to_string(active_alarms.size()) + 
                                      "개의 활성 알람 발견");
        
        size_t success_count = 0;
        for (const auto& alarm_entity : active_alarms) {
            if (ValidateAlarmForRecovery(alarm_entity)) {
                auto backend_alarm = ConvertToBackendFormat(alarm_entity);
                if (PublishAlarmToRedis(backend_alarm)) {
                    success_count++;
                }
            }
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "테넌트 " + std::to_string(tenant_id) + 
                                      ": " + std::to_string(success_count) + "개 복구 완료");
        
        return success_count;
        
    } catch (const std::exception& e) {
        HandleRecoveryError("테넌트별 복구", e.what());
        return 0;
    }
}

// =============================================================================
// 초기화 메서드 구현
// =============================================================================

bool AlarmStartupRecovery::InitializeComponents() {
    try {
        // 1. Repository 초기화
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        alarm_occurrence_repo_ = repo_factory.getAlarmOccurrenceRepository();
        
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "AlarmOccurrenceRepository 획득 실패");
            return false;
        }
        
        // 2. RedisDataWriter 초기화
        if (!redis_data_writer_) {
            redis_data_writer_ = std::make_unique<Storage::RedisDataWriter>();
        }
        
        if (!redis_data_writer_->IsConnected()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "RedisDataWriter 연결 실패");
            return false;
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG_LEVEL,
                                      "알람 복구 컴포넌트 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "컴포넌트 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 핵심 복구 로직 구현
// =============================================================================

std::vector<PulseOne::Database::Entities::AlarmOccurrenceEntity> AlarmStartupRecovery::LoadActiveAlarmsFromDB() {
    try {
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "Repository가 초기화되지 않음");
            return {};
        }
        
        // DB에서 활성 알람 조회 (state='active')
        auto active_alarms = alarm_occurrence_repo_->findActive();
        
        // 추가 필터: acknowledged되지 않은 것만 (더 중요한 알람들)
        std::vector<Database::Entities::AlarmOccurrenceEntity> unacknowledged_alarms;
        
        for (const auto& alarm : active_alarms) {
            // acknowledged_time이 없는 것들만 복구 (더 긴급한 알람)
            if (!alarm.getAcknowledgedTime().has_value()) {
                unacknowledged_alarms.push_back(alarm);
            }
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "전체 활성 알람: " + std::to_string(active_alarms.size()) + 
                                      "개, 미인지 알람: " + std::to_string(unacknowledged_alarms.size()) + "개");
        
        // 미인지 알람이 더 중요하므로 우선 복구
        return unacknowledged_alarms.empty() ? active_alarms : unacknowledged_alarms;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "활성 알람 DB 로드 실패: " + std::string(e.what()));
        return {};
    }
}

Storage::BackendFormat::AlarmEventData AlarmStartupRecovery::ConvertToBackendFormat(
    const PulseOne::Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const {
    
    Storage::BackendFormat::AlarmEventData alarm_data;
    
    try {
        // 기본 정보 복사
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId()); // ✅ string으로 변환
        alarm_data.rule_id = occurrence_entity.getRuleId();
        alarm_data.device_id = std::to_string(occurrence_entity.getDeviceId()); // ✅ string으로 변환
        alarm_data.point_id = occurrence_entity.getPointId();
        alarm_data.tenant_id = occurrence_entity.getTenantId();
        
        // 메시지 및 값
        alarm_data.message = occurrence_entity.getAlarmMessage();
        alarm_data.trigger_value = occurrence_entity.getTriggerValue();
        
        // ✅ 타입 수정: enum → string 직접 변환
        alarm_data.severity = PulseOne::Alarm::severityToString(occurrence_entity.getSeverity());
        alarm_data.state = PulseOne::Alarm::stateToString(occurrence_entity.getState());
        
        // 시간 변환
        auto duration = occurrence_entity.getOccurrenceTime().time_since_epoch();
        alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        // 추가 정보
        alarm_data.source_name = occurrence_entity.getSourceName().value_or("");
        alarm_data.location = occurrence_entity.getLocation().value_or("");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("alarm_recovery", LogLevel::ERROR,
            "Backend 포맷 변환 실패: " + std::string(e.what()));
        
        // ✅ 올바른 기본값 초기화 
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId());
        alarm_data.rule_id = occurrence_entity.getRuleId();
        alarm_data.device_id = std::to_string(occurrence_entity.getDeviceId());
        alarm_data.point_id = occurrence_entity.getPointId();
        alarm_data.tenant_id = occurrence_entity.getTenantId();
        alarm_data.message = "시스템 복구된 알람";
        alarm_data.severity = "MEDIUM";  // ✅ string 기본값
        alarm_data.state = "active";     // ✅ string 기본값
        alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        alarm_data.trigger_value = "";
        alarm_data.source_name = "";
        alarm_data.location = "";
    }
    
    return alarm_data;
}


bool AlarmStartupRecovery::PublishAlarmToRedis(const Storage::BackendFormat::AlarmEventData& alarm_data) {
    try {
        if (!redis_data_writer_ || !redis_data_writer_->IsConnected()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "RedisDataWriter 연결되지 않음");
            return false;
        }
        
        // Redis Pub/Sub으로 알람 발행 (기존 채널들 사용)
        bool success = redis_data_writer_->PublishAlarmEvent(alarm_data);
        
        if (success) {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG_LEVEL,
                                          "Redis 알람 발행 성공: rule_id=" + 
                                          std::to_string(alarm_data.rule_id) + 
                                          ", channels=[alarms:all, tenant:" + 
                                          std::to_string(alarm_data.tenant_id) + ":alarms, device:" + 
                                          alarm_data.device_id + ":alarms]");
        } else {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "Redis 알람 발행 실패: rule_id=" + 
                                          std::to_string(alarm_data.rule_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "Redis 발행 중 예외: " + std::string(e.what()));
        return false;
    }
}

bool AlarmStartupRecovery::ValidateAlarmForRecovery(
    const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const {
    
    try {
        // 1. 필수 필드 확인
        if (occurrence_entity.getRuleId() <= 0) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "유효하지 않은 rule_id: " + 
                                          std::to_string(occurrence_entity.getRuleId()));
            return false;
        }
        
        if (occurrence_entity.getTenantId() <= 0) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "유효하지 않은 tenant_id: " + 
                                          std::to_string(occurrence_entity.getTenantId()));
            return false;
        }
        
        // 2. 상태 확인
        std::string state = (occurrence_entity.getState() == PulseOne::Alarm::AlarmState::ACTIVE) ? "active" : "inactive";
        if (state != "active") {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG_LEVEL,
                                          "비활성 상태 알람 건너뜀: " + state);
            return false;
        }
        
        // 3. 메시지 확인
        if (occurrence_entity.getAlarmMessage().empty()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "빈 알람 메시지: rule_id=" + 
                                          std::to_string(occurrence_entity.getRuleId()));
            // 빈 메시지여도 복구는 진행 (기본 메시지로 대체)
        }
        
        // 4. 타임스탬프 유효성 (너무 오래된 알람 제외)
        auto occurrence_time = occurrence_entity.getOccurrenceTime();
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - occurrence_time);
        
        // 7일 이상 오래된 알람은 복구하지 않음
        if (age.count() > 24 * 7) {
            LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                          "7일 이상 오래된 알람 건너뜀: rule_id=" + 
                                          std::to_string(occurrence_entity.getRuleId()) + 
                                          " (나이: " + std::to_string(age.count()) + "시간)");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "알람 유효성 검사 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 유틸리티 메서드 구현
// =============================================================================

int AlarmStartupRecovery::ConvertSeverityToInt(const std::string& severity_str) const {
    if (severity_str == "CRITICAL" || severity_str == "critical") return 4;
    if (severity_str == "HIGH" || severity_str == "high") return 3;
    if (severity_str == "MEDIUM" || severity_str == "medium") return 2;
    if (severity_str == "LOW" || severity_str == "low") return 1;
    if (severity_str == "INFO" || severity_str == "info") return 0;
    
    // 숫자 문자열인 경우
    try {
        return std::stoi(severity_str);
    } catch (...) {
        return 3; // 기본값: HIGH
    }
}

int AlarmStartupRecovery::ConvertStateToInt(const std::string& state_str) const {
    if (state_str == "active") return 1;
    if (state_str == "acknowledged") return 2;
    if (state_str == "cleared") return 3;
    return 0; // INACTIVE
}

int64_t AlarmStartupRecovery::ConvertTimestampToMillis(
    const std::chrono::system_clock::time_point& timestamp) const {
    
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
}

void AlarmStartupRecovery::HandleRecoveryError(const std::string& operation, 
                                               const std::string& error_message) {
    std::string full_error = operation + " 실패: " + error_message;
    
    LogManager::getInstance().log("startup_recovery", LogLevel::ERROR, full_error);
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    recovery_stats_.last_error = full_error;
}

// =============================================================================
// 통계 관리 구현
// =============================================================================

void AlarmStartupRecovery::ResetRecoveryStats() {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    recovery_stats_.total_active_alarms = 0;
    recovery_stats_.successfully_published = 0;
    recovery_stats_.failed_to_publish = 0;
    recovery_stats_.invalid_alarms = 0;
    recovery_stats_.recovery_duration = std::chrono::milliseconds{0};
    recovery_stats_.last_recovery_time.clear();
    recovery_stats_.last_error.clear();
}