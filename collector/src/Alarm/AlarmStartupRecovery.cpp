/**
 * @file AlarmStartupRecovery.cpp
 * @brief 컴파일 에러 완전 수정 - Storage 네임스페이스 문제 해결
 * @date 2025-08-31
 * @version Fixed: Storage 타입 인식 및 네임스페이스 문제 해결
 */

#include "Alarm/AlarmStartupRecovery.h"

// 🔥 중요: Storage 관련 헤더를 명시적으로 포함
#include "Storage/BackendFormat.h"
#include "Storage/RedisDataWriter.h"

// Database 시스템
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"

// 기본 시스템
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// Common 타입들
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Alarm/AlarmTypes.h"

#include <chrono>
#include <thread>
#include <stdexcept>

// 🔥 네임스페이스 별칭으로 타입 명확화
using namespace PulseOne;
using namespace PulseOne::Alarm;
using LogLevel = PulseOne::Enums::LogLevel;

// 🔥 Storage 타입 별칭으로 컴파일 에러 방지
using BackendAlarmData = PulseOne::Storage::BackendFormat::AlarmEventData;

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 상수 정의
// =============================================================================

constexpr int REDIS_PUBLISH_RETRY_COUNT = 3;
constexpr std::chrono::milliseconds RETRY_DELAY{100};

// =============================================================================
// 싱글톤 인스턴스
// =============================================================================

std::unique_ptr<AlarmStartupRecovery> AlarmStartupRecovery::instance_ = nullptr;
std::mutex AlarmStartupRecovery::instance_mutex_;

AlarmStartupRecovery& AlarmStartupRecovery::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<AlarmStartupRecovery>(new AlarmStartupRecovery());
    }
    return *instance_;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmStartupRecovery::AlarmStartupRecovery()
    : recovery_enabled_(true)
    , recovery_completed_(false)
    , recovery_in_progress_(false)
    , alarm_occurrence_repo_(nullptr)
    , redis_data_writer_(nullptr) {
    
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "AlarmStartupRecovery 생성됨");
}

AlarmStartupRecovery::~AlarmStartupRecovery() {
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "AlarmStartupRecovery 소멸됨");
}

// =============================================================================
// 메인 복구 메서드 (수정된 버전)
// =============================================================================

size_t AlarmStartupRecovery::RecoverActiveAlarms() {
    if (!recovery_enabled_.load()) {
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "알람 복구 비활성화됨 - 건너뜀");
        return 0;
    }
    
    if (recovery_in_progress_.load()) {
        LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                      "알람 복구가 이미 진행 중입니다");
        return 0;
    }
    
    recovery_in_progress_.store(true);
    auto start_time = std::chrono::steady_clock::now();
    
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "🚨 시스템 시작 시 활성 알람 복구 시작");
    
    // 통계 초기화
    recovery_stats_ = RecoveryStats{};
    
    size_t total_recovered = 0;
    
    try {
        // 1. 컴포넌트 초기화
        if (!InitializeComponents()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "컴포넌트 초기화 실패");
            recovery_in_progress_.store(false);
            return 0;
        }
        
        // 2. DB에서 활성 알람 로드
        auto active_alarms = LoadActiveAlarmsFromDB();
        recovery_stats_.total_active_alarms = active_alarms.size();
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "📊 DB에서 " + std::to_string(active_alarms.size()) + 
                                      "개의 활성 알람 발견");
        
        if (active_alarms.empty()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                          "✅ 복구할 활성 알람이 없습니다");
            recovery_completed_.store(true);
            recovery_in_progress_.store(false);
            return 0;
        }
        
        // 3. 배치로 Redis 발행
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
                    }
                }
                
                if (published) {
                    success_count++;
                    recovery_stats_.successfully_published++;
                } else {
                    recovery_stats_.failed_to_publish++;
                }
                
            } catch (const std::exception& e) {
                recovery_stats_.failed_to_publish++;
                LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                              "개별 알람 복구 실패: " + std::string(e.what()));
            }
        }
        
        total_recovered = success_count;
        recovery_completed_.store(true);
        
        // 4. 결과 요약
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        recovery_stats_.recovery_duration = duration;
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "🎯 알람 복구 완료 - 성공: " + std::to_string(success_count) + 
                                      "개, 실패: " + std::to_string(recovery_stats_.failed_to_publish) + "개");
        
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
        auto& factory = Database::RepositoryFactory::getInstance();
        if (!factory.initialize()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "RepositoryFactory 초기화 실패");
            return false;
        }
        
        alarm_occurrence_repo_ = factory.getAlarmOccurrenceRepository();
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "AlarmOccurrenceRepository 획득 실패");
            return false;
        }
        
        // 2. RedisDataWriter 초기화
        if (!redis_data_writer_) {
            redis_data_writer_ = std::make_shared<Storage::RedisDataWriter>();
        }
        
        if (!redis_data_writer_->IsConnected()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "RedisDataWriter 연결 상태 불량");
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                      "AlarmStartupRecovery 컴포넌트 초기화 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "컴포넌트 초기화 중 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 핵심 로직 구현
// =============================================================================

std::vector<Database::Entities::AlarmOccurrenceEntity> AlarmStartupRecovery::LoadActiveAlarmsFromDB() {
    try {
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                          "AlarmOccurrenceRepository가 null입니다");
            return {};
        }
        
        // 활성 알람 조회 (state='active' AND acknowledged_time IS NULL)
        auto active_alarms = alarm_occurrence_repo_->findActive();
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "DB에서 " + std::to_string(active_alarms.size()) + 
                                      "개의 활성 알람 로드됨");
        
        return active_alarms;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::ERROR,
                                      "활성 알람 DB 로드 실패: " + std::string(e.what()));
        return {};
    }
}

// 🔥 컴파일 에러 수정: 올바른 타입 별칭 사용
BackendAlarmData AlarmStartupRecovery::ConvertToBackendFormat(
    const PulseOne::Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const {
    
    BackendAlarmData alarm_data;
    
    try {
        // 기본 정보 복사
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId());
        alarm_data.rule_id = occurrence_entity.getRuleId();
        
        // 🔥 수정: std::optional<int> 안전 처리
        if (occurrence_entity.getDeviceId().has_value()) {
            alarm_data.device_id = std::to_string(occurrence_entity.getDeviceId().value());
        } else {
            alarm_data.device_id = "0";
        }
        
        if (occurrence_entity.getPointId().has_value()) {
            alarm_data.point_id = occurrence_entity.getPointId().value();
        } else {
            alarm_data.point_id = 0;
        }
        
        alarm_data.tenant_id = occurrence_entity.getTenantId();
        
        // 메시지 및 값
        alarm_data.message = occurrence_entity.getAlarmMessage();
        alarm_data.trigger_value = occurrence_entity.getTriggerValue();
        
        // enum → string 변환
        alarm_data.severity = AlarmSeverityToString(occurrence_entity.getSeverity());
        alarm_data.state = AlarmStateToString(occurrence_entity.getState());
        
        // 시간 변환
        auto duration = occurrence_entity.getOccurrenceTime().time_since_epoch();
        alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        // 🔥 수정: std::string 직접 할당 (value_or() 없음)
        alarm_data.source_name = occurrence_entity.getSourceName();
        alarm_data.location = occurrence_entity.getLocation();
        
        // 타입 지정
        alarm_data.type = "alarm_event";
        
        LogManager::getInstance().log("alarm_recovery", LogLevel::DEBUG,
            "Backend 포맷 변환 완료: ID=" + alarm_data.occurrence_id + 
            ", Severity=" + alarm_data.severity + 
            ", State=" + alarm_data.state);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("alarm_recovery", LogLevel::ERROR,
            "Backend 포맷 변환 실패: " + std::string(e.what()));
        
        // 기본값으로 초기화
        alarm_data = BackendAlarmData{};
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId());
        alarm_data.device_id = "0";
        alarm_data.point_id = 0;
        alarm_data.severity = "MEDIUM";
        alarm_data.state = "ACTIVE";
        alarm_data.type = "alarm_event";
    }
    
    return alarm_data;
}

bool AlarmStartupRecovery::PublishAlarmToRedis(const BackendAlarmData& alarm_data) {
    try {
        if (!redis_data_writer_ || !redis_data_writer_->IsConnected()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "RedisDataWriter 연결되지 않음");
            return false;
        }
        
        // RedisDataWriter를 통해 알람 발행
        bool success = redis_data_writer_->PublishAlarmEvent(alarm_data);
        
        if (success) {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                          "Redis 알람 발행 성공: rule_id=" + 
                                          std::to_string(alarm_data.rule_id));
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
        // 1. 기본 ID 확인
        if (occurrence_entity.getId() <= 0) {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                          "무효한 알람 ID: " + std::to_string(occurrence_entity.getId()));
            return false;
        }
        
        // 2. 룰 ID 확인
        if (occurrence_entity.getRuleId() <= 0) {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                          "무효한 룰 ID: " + std::to_string(occurrence_entity.getRuleId()));
            return false;
        }
        
        // 3. 상태 확인 (ACTIVE 여야 함)
        if (occurrence_entity.getState() != AlarmState::ACTIVE) {
            return false; // ACTIVE가 아니면 복구 대상 아님
        }
        
        // 4. 메시지 확인
        if (occurrence_entity.getAlarmMessage().empty()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                          "빈 알람 메시지: ID=" + std::to_string(occurrence_entity.getId()));
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                      "알람 유효성 검증 중 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드 구현
// =============================================================================

void AlarmStartupRecovery::HandleRecoveryError(const std::string& context, const std::string& error_msg) {
    std::string full_error = context + ": " + error_msg;
    
    LogManager::getInstance().log("startup_recovery", LogLevel::ERROR, full_error);
    
    recovery_stats_.last_error = full_error;
    recovery_stats_.last_recovery_time = GetCurrentTimeString();
    
    recovery_completed_.store(false);
}

std::string AlarmStartupRecovery::GetCurrentTimeString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void AlarmStartupRecovery::ResetRecoveryStats() {
    recovery_stats_ = RecoveryStats{};
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "복구 통계 리셋 완료");
}

// =============================================================================
// enum → string 변환 헬퍼 함수들 (컴파일 에러 방지)
// =============================================================================

std::string AlarmStartupRecovery::AlarmSeverityToString(AlarmSeverity severity) const {
    switch (severity) {
        case AlarmSeverity::INFO:     return "INFO";
        case AlarmSeverity::LOW:      return "LOW";
        case AlarmSeverity::MEDIUM:   return "MEDIUM";
        case AlarmSeverity::HIGH:     return "HIGH";
        case AlarmSeverity::CRITICAL: return "CRITICAL";
        default:                      return "MEDIUM";
    }
}

std::string AlarmStartupRecovery::AlarmStateToString(AlarmState state) const {
    switch (state) {
        case AlarmState::ACTIVE:       return "ACTIVE";
        case AlarmState::ACKNOWLEDGED: return "ACKNOWLEDGED";
        case AlarmState::CLEARED:      return "CLEARED";
        case AlarmState::SUPPRESSED:   return "SUPPRESSED";
        default:                       return "ACTIVE";
    }
}

} // namespace Alarm
} // namespace PulseOne