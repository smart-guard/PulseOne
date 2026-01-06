// =============================================================================
// AlarmStartupRecovery.cpp - 완전 버전 (enum 수정 후 단순화)
// 기존 AlarmTypes.h 함수들 활용, 불필요한 중복 정의 제거
// =============================================================================

#include "Alarm/AlarmStartupRecovery.h"
#include "Alarm/AlarmEngine.h"
#include "Storage/BackendFormat.h"
#include "Storage/RedisDataWriter.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Alarm/AlarmTypes.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DataPointRepository.h"

#include <chrono>
#include <thread>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace PulseOne;
using namespace PulseOne::Alarm;
using LogLevel = PulseOne::Enums::LogLevel;

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
    , recovery_paused_(false)
    , recovery_cancelled_(false)
    , alarm_occurrence_repo_(nullptr)
    , redis_data_writer_(nullptr)
    , current_alarm_index_(0)
    , total_alarms_to_process_(0)
    , filter_by_severity_(false)
    , min_severity_(AlarmSeverity::INFO)
    , filter_by_tenant_(false)
    , total_recovery_count_(0)
    , avg_recovery_time_(std::chrono::milliseconds{0})
    , enable_batch_processing_(true)
    , enable_priority_recovery_(false)
    , enable_duplicate_detection_(true)
    , enable_recovery_logging_(true)
    , recovery_policy_(RecoveryPolicy::ALL_ACTIVE_ALARMS) {
    
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "AlarmStartupRecovery 생성됨");
}

AlarmStartupRecovery::~AlarmStartupRecovery() {
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "AlarmStartupRecovery 소멸됨");
}

// =============================================================================
// 핵심 변환 로직 - 대폭 단순화!
// =============================================================================

Storage::BackendFormat::AlarmEventData AlarmStartupRecovery::ConvertToBackendFormat(
    const PulseOne::Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const {
    
    Storage::BackendFormat::AlarmEventData alarm_data;
    
    try {
        // 🔧 수정: occurrence_id는 string 타입
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId());
        alarm_data.rule_id = occurrence_entity.getRuleId();
        alarm_data.tenant_id = occurrence_entity.getTenantId();
        
        // 🔧 수정: device_id는 optional<int> → string 변환
        auto device_id_opt = occurrence_entity.getDeviceId();
        if (device_id_opt.has_value()) {
            alarm_data.device_id = std::to_string(device_id_opt.value());
        } else {
            alarm_data.device_id = "";
        }
        
        // 🔧 수정: point_id는 optional<int> → int 변환
        auto point_id_opt = occurrence_entity.getPointId();
        if (point_id_opt.has_value()) {
            alarm_data.point_id = point_id_opt.value();
        } else {
            alarm_data.point_id = 0;
        }
        
        alarm_data.message = occurrence_entity.getAlarmMessage();
        alarm_data.trigger_value = occurrence_entity.getTriggerValue();
        
        // 🔧 핵심 수정: enum → string 변환 (AlarmTypes.h 함수 사용)
        alarm_data.severity = PulseOne::Alarm::severityToString(occurrence_entity.getSeverity());
        alarm_data.state = PulseOne::Alarm::stateToString(occurrence_entity.getState());
        
        // 🔧 수정: getOccurredAt() → getOccurrenceTime()
        auto occurrence_time = occurrence_entity.getOccurrenceTime();
        auto duration = occurrence_time.time_since_epoch();
        alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        // 🔧 수정: source_name, location은 이미 string (value_or() 불필요)
        alarm_data.source_name = occurrence_entity.getSourceName();
        alarm_data.location = occurrence_entity.getLocation();
        
        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
            "Backend 포맷 변환 완료: ID=" + alarm_data.occurrence_id +  // 🔧 수정: string 직접 사용
            ", Rule=" + std::to_string(alarm_data.rule_id) +
            ", Severity=" + alarm_data.severity + " (" +               // 🔧 수정: string 직접 사용
            std::to_string(static_cast<int>(occurrence_entity.getSeverity())) + ")" +
            ", State=" + alarm_data.state + " (" +                     // 🔧 수정: string 직접 사용
            std::to_string(static_cast<int>(occurrence_entity.getState())) + ")");
        
        return alarm_data;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                      "Backend 포맷 변환 실패: " + std::string(e.what()));
        
        // 안전한 기본값으로 초기화
        alarm_data.occurrence_id = std::to_string(occurrence_entity.getId());
        alarm_data.rule_id = occurrence_entity.getRuleId();
        alarm_data.tenant_id = occurrence_entity.getTenantId();
        alarm_data.message = occurrence_entity.getAlarmMessage();
        alarm_data.severity = "CRITICAL";  // 🔧 수정: string 직접 할당
        alarm_data.state = "ACTIVE";       // 🔧 수정: string 직접 할당
        alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return alarm_data;
    }
}

// =============================================================================
// 메인 복구 메서드
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
                                  "시스템 시작 시 활성 알람 복구 시작");
    
    // 통계 초기화
    recovery_stats_ = RecoveryStats{};
    
    size_t total_recovered = 0;
    
    try {
        // 1. 컴포넌트 초기화
        if (!InitializeComponents()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                          "컴포넌트 초기화 실패");
            recovery_in_progress_.store(false);
            return 0;
        }
        
        // 2. DB에서 활성 알람 로드
        auto active_alarms = LoadActiveAlarmsFromDB();
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
        
        // 3. 각 알람을 Redis로 발행
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
                        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                                      "Redis 발행 재시도 " + std::to_string(retry + 1) + 
                                                      "/3: 알람 ID " + std::to_string(alarm_entity.getId()));
                    }
                }
                
                if (published) {
                    success_count++;
                    recovery_stats_.successfully_published++;
                    
                    LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                                  "활성 알람 복구 성공: rule_id=" + 
                                                  std::to_string(alarm_entity.getRuleId()) +
                                                  ", severity=" + 
                                                  PulseOne::Alarm::severityToString(alarm_entity.getSeverity()));
                } else {
                    recovery_stats_.failed_to_publish++;
                    LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                                  "Redis 발행 최종 실패: rule_id=" + 
                                                  std::to_string(alarm_entity.getRuleId()));
                }
                
            } catch (const std::exception& e) {
                recovery_stats_.failed_to_publish++;
                LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                              "개별 알람 복구 실패: " + std::string(e.what()));
            }
        }
        
        total_recovered = success_count;
        recovery_completed_.store(true);
        
        // 4. 결과 요약 로그
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        recovery_stats_.recovery_duration = duration;
        recovery_stats_.last_recovery_time = GetCurrentTimeString();
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "알람 복구 완료 - 성공: " + std::to_string(success_count) + 
                                      "개, 실패: " + std::to_string(recovery_stats_.failed_to_publish) + "개");
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                      "복구 소요시간: " + std::to_string(duration.count()) + "ms");
        
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

// =============================================================================
// 기타 메인 메서드들
// =============================================================================

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
// 초기화 및 핵심 로직
// =============================================================================

bool AlarmStartupRecovery::InitializeComponents() {
    try {
        // 1. Repository 초기화
        auto& factory = Database::RepositoryFactory::getInstance();
        if (!factory.initialize()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                          "RepositoryFactory 초기화 실패");
            return false;
        }
        
        alarm_occurrence_repo_ = factory.getAlarmOccurrenceRepository();
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                          "AlarmOccurrenceRepository 획득 실패");
            return false;
        }
        
        // 2. RedisDataWriter 초기화
        if (!redis_data_writer_) {
            redis_data_writer_ = std::make_shared<Storage::RedisDataWriter>();
        }
        
        if (!redis_data_writer_->IsConnected()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                          "RedisDataWriter 연결 상태 불량 - 계속 진행");
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::DEBUG,
                                      "AlarmStartupRecovery 컴포넌트 초기화 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                      "컴포넌트 초기화 중 예외: " + std::string(e.what()));
        return false;
    }
}

std::vector<Database::Entities::AlarmOccurrenceEntity> AlarmStartupRecovery::LoadActiveAlarmsFromDB() {
    try {
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
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
        LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                      "활성 알람 DB 로드 실패: " + std::string(e.what()));
        return {};
    }
}

bool AlarmStartupRecovery::PublishAlarmToRedis(const Storage::BackendFormat::AlarmEventData& alarm_data) {
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
        LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR,
                                      "Redis 발행 중 예외: " + std::string(e.what()));
        return false;
    }
}

bool AlarmStartupRecovery::ValidateAlarmForRecovery(
    const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const {
    
    try {
        // 1. 기본 ID 확인
        if (occurrence_entity.getId() <= 0) {
            return false;
        }
        
        // 2. 룰 ID 확인
        if (occurrence_entity.getRuleId() <= 0) {
            return false;
        }
        
        // 3. 상태 확인 (ACTIVE 여야 함)
        if (occurrence_entity.getState() != AlarmState::ACTIVE) {
            return false;
        }
        
        // 4. 테넌트 ID 확인
        if (occurrence_entity.getTenantId() <= 0) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("startup_recovery", LogLevel::WARN,
                                      "알람 유효성 검사 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

void AlarmStartupRecovery::HandleRecoveryError(const std::string& context, const std::string& error_msg) {
    std::string full_error = context + ": " + error_msg;
    
    LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR, full_error);
    
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
    std::lock_guard<std::mutex> lock(stats_mutex_);
    recovery_stats_ = RecoveryStats{};
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "복구 통계 리셋 완료");
}

// =============================================================================
// 헤더 호환성을 위한 나머지 메서드들
// =============================================================================

size_t AlarmStartupRecovery::PublishAlarmBatchToRedis(const std::vector<Storage::BackendFormat::AlarmEventData>& alarm_batch) {
    size_t success_count = 0;
    
    for (const auto& alarm : alarm_batch) {
        if (PublishAlarmToRedis(alarm)) {
            success_count++;
        }
    }
    
    return success_count;
}

size_t AlarmStartupRecovery::CalculateOptimalBatchSize(size_t total_alarms) const {
    if (total_alarms <= 50) return total_alarms;
    if (total_alarms <= 200) return 25;
    if (total_alarms <= 500) return 50;
    return MAX_RECOVERY_BATCH_SIZE;
}

void AlarmStartupRecovery::PauseRecovery() {
    recovery_paused_.store(true);
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "복구 작업 일시정지");
}

void AlarmStartupRecovery::ResumeRecovery() {
    recovery_paused_.store(false);
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "복구 작업 재개");
}

void AlarmStartupRecovery::CancelRecovery() {
    recovery_cancelled_.store(true);
    recovery_paused_.store(false);
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "복구 작업 중단");
}

double AlarmStartupRecovery::GetRecoveryProgress() const {
    size_t total = total_alarms_to_process_.load();
    size_t current = current_alarm_index_.load();
    
    if (total == 0) return 0.0;
    return static_cast<double>(current) / static_cast<double>(total);
}

void AlarmStartupRecovery::SetSeverityFilter(AlarmSeverity min_severity) {
    filter_by_severity_ = true;
    min_severity_ = min_severity;
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "심각도 필터 설정: " + PulseOne::Alarm::severityToString(min_severity));
}

void AlarmStartupRecovery::DisableSeverityFilter() {
    filter_by_severity_ = false;
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "심각도 필터 비활성화");
}

void AlarmStartupRecovery::SetTargetTenants(const std::vector<int>& tenant_ids) {
    target_tenants_ = tenant_ids;
    filter_by_tenant_ = !tenant_ids.empty();
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "대상 테넌트 설정: " + std::to_string(tenant_ids.size()) + "개");
}

void AlarmStartupRecovery::AddTargetTenant(int tenant_id) {
    target_tenants_.push_back(tenant_id);
    filter_by_tenant_ = true;
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "테넌트 추가: " + std::to_string(tenant_id));
}

void AlarmStartupRecovery::ClearTargetTenants() {
    target_tenants_.clear();
    filter_by_tenant_ = false;
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "대상 테넌트 목록 클리어");
}

void AlarmStartupRecovery::SetTimeFilter(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {
    
    recovery_start_time_filter_ = start_time;
    recovery_end_time_filter_ = end_time;
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "시간 필터 설정됨");
}

void AlarmStartupRecovery::ClearTimeFilter() {
    recovery_start_time_filter_.reset();
    recovery_end_time_filter_.reset();
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "시간 필터 해제됨");
}

std::string AlarmStartupRecovery::GetDiagnosticInfo() const {
    std::stringstream ss;
    ss << "=== AlarmStartupRecovery 진단 정보 ===\n";
    ss << "복구 활성화: " << (recovery_enabled_.load() ? "YES" : "NO") << "\n";
    ss << "복구 완료: " << (recovery_completed_.load() ? "YES" : "NO") << "\n";
    ss << "복구 진행 중: " << (recovery_in_progress_.load() ? "YES" : "NO") << "\n";
    ss << "일시정지 상태: " << (recovery_paused_.load() ? "YES" : "NO") << "\n";
    ss << "중단 상태: " << (recovery_cancelled_.load() ? "YES" : "NO") << "\n";
    ss << "복구 정책: " << static_cast<int>(recovery_policy_) << "\n";
    ss << "배치 처리 활성화: " << (enable_batch_processing_ ? "YES" : "NO") << "\n";
    
    auto stats = GetRecoveryStats();
    ss << "총 활성 알람: " << stats.total_active_alarms << "\n";
    ss << "성공 발행: " << stats.successfully_published << "\n";
    ss << "실패 발행: " << stats.failed_to_publish << "\n";
    ss << "무효 알람: " << stats.invalid_alarms << "\n";
    ss << "마지막 복구: " << stats.last_recovery_time << "\n";
    
    return ss.str();
}

std::map<std::string, double> AlarmStartupRecovery::GetPerformanceMetrics() const {
    std::map<std::string, double> metrics;
    
    auto stats = GetRecoveryStats();
    metrics["total_active_alarms"] = static_cast<double>(stats.total_active_alarms);
    metrics["success_rate"] = stats.total_active_alarms > 0 ? 
        static_cast<double>(stats.successfully_published) / static_cast<double>(stats.total_active_alarms) : 0.0;
    metrics["recovery_duration_ms"] = static_cast<double>(stats.recovery_duration.count());
    metrics["avg_recovery_time_ms"] = static_cast<double>(avg_recovery_time_.load().count());
    metrics["total_recovery_count"] = static_cast<double>(total_recovery_count_.load());
    
    return metrics;
}

std::vector<int> AlarmStartupRecovery::GetProcessedAlarmIds() const {
    std::lock_guard<std::mutex> lock(processed_ids_mutex_);
    std::vector<int> ids(processed_alarm_ids_.begin(), processed_alarm_ids_.end());
    return ids;
}

void AlarmStartupRecovery::ClearProcessedAlarmCache() {
    std::lock_guard<std::mutex> lock(processed_ids_mutex_);
    processed_alarm_ids_.clear();
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO,
                                  "처리된 알람 캐시 정리됨");
}

bool AlarmStartupRecovery::PassesSeverityFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const {
    if (!filter_by_severity_) return true;
    return static_cast<int>(entity.getSeverity()) >= static_cast<int>(min_severity_);
}

bool AlarmStartupRecovery::PassesTenantFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const {
    if (!filter_by_tenant_) return true;
    return std::find(target_tenants_.begin(), target_tenants_.end(), entity.getTenantId()) != target_tenants_.end();
}

bool AlarmStartupRecovery::PassesTimeFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const {
    if (!recovery_start_time_filter_.has_value() && !recovery_end_time_filter_.has_value()) {
        return true;
    }
    
    auto occurrence_time = entity.getOccurrenceTime();
    
    if (recovery_start_time_filter_.has_value() && occurrence_time < recovery_start_time_filter_.value()) {
        return false;
    }
    
    if (recovery_end_time_filter_.has_value() && occurrence_time > recovery_end_time_filter_.value()) {
        return false;
    }
    
    return true;
}

bool AlarmStartupRecovery::IsAlreadyProcessed(int alarm_id) const {
    if (!enable_duplicate_detection_) return false;
    
    std::lock_guard<std::mutex> lock(processed_ids_mutex_);
    return processed_alarm_ids_.find(alarm_id) != processed_alarm_ids_.end();
}

void AlarmStartupRecovery::MarkAsProcessed(int alarm_id) {
    if (!enable_duplicate_detection_) return;
    
    std::lock_guard<std::mutex> lock(processed_ids_mutex_);
    processed_alarm_ids_.insert(alarm_id);
}

std::vector<Database::Entities::AlarmOccurrenceEntity> AlarmStartupRecovery::SortByPriority(
    const std::vector<Database::Entities::AlarmOccurrenceEntity>& alarms) const {
    
    if (!enable_priority_recovery_) {
        return alarms;
    }
    
    auto sorted_alarms = alarms;
    
    // 심각도순 정렬 (CRITICAL → HIGH → MEDIUM → LOW → INFO)
    std::sort(sorted_alarms.begin(), sorted_alarms.end(),
        [](const auto& a, const auto& b) {
            return static_cast<int>(a.getSeverity()) > static_cast<int>(b.getSeverity());
        });
    
    return sorted_alarms;
}

void AlarmStartupRecovery::UpdateProgress(size_t current_index, size_t total) {
    current_alarm_index_.store(current_index);
    total_alarms_to_process_.store(total);
}

void AlarmStartupRecovery::UpdatePerformanceMetrics(std::chrono::milliseconds duration) {
    auto current_count = total_recovery_count_.fetch_add(1);
    auto current_avg = avg_recovery_time_.load();
    
    auto new_avg = std::chrono::milliseconds{
        (current_avg.count() * current_count + duration.count()) / (current_count + 1)
    };
    
    avg_recovery_time_.store(new_avg);
}

// =============================================================================
// 🗑️ 제거된 함수들 - AlarmTypes.h의 기존 함수 사용
// =============================================================================

/*
 * ConvertSeverityToInt() - 제거됨 (enum 직접 사용)
 * ConvertStateToInt() - 제거됨 (enum 직접 사용)  
 * AlarmSeverityToString() - 제거됨 (severityToString() 사용)
 * AlarmStateToString() - 제거됨 (stateToString() 사용)
 * ConvertTimestampToMillis() - 제거됨 (chrono 직접 사용)
 */

size_t AlarmStartupRecovery::RecoverLatestPointValues() {
    LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "RDB 최신 포인트 값 Redis 복구 시작");
    
    try {
        if (!InitializeComponents() || !redis_data_writer_) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR, "컴포넌트 초기화 실패로 복구 중단");
            return 0;
        }
        
        auto& factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto current_value_repo = factory.getCurrentValueRepository();
        auto data_point_repo = factory.getDataPointRepository();
        
        if (!current_value_repo || !data_point_repo) {
            LogManager::getInstance().log("startup_recovery", LogLevel::LOG_ERROR, "Repository 획득 실패");
            return 0;
        }
        
        // 1. RDB에서 모든 최신값 조회
        auto current_values = current_value_repo->findAll();
        if (current_values.empty()) {
            LogManager::getInstance().log("startup_recovery", LogLevel::INFO, "복구할 포인트 값이 RDB에 없습니다.");
            return 0;
        }
        
        size_t success_count = 0;
        
        // 2. Redis로 데이터 마이그레이션
        for (const auto& entity : current_values) {
            try {
                int point_id = entity.getPointId();
                
                // 포인트 설정 정보 조회 (device_id 확인용)
                auto point_config = data_point_repo->findById(point_id);
                if (!point_config.has_value()) {
                    continue;
                }
                
                // TimestampedValue 구조체 생성
                PulseOne::Structs::TimestampedValue point_val;
                point_val.point_id = point_id;
                point_val.timestamp = entity.getValueTimestamp();
                point_val.quality = entity.getQualityCode();
                point_val.value_changed = false; // 복구 시에는 변화 없음으로 간주
                
                // JSON 문자열로부터 DataValue 복원
                std::string cv_json_str = entity.getCurrentValue();
                if (!cv_json_str.empty()) {
                    try {
                        auto j = nlohmann::json::parse(cv_json_str);
                        if (j.contains("value")) {
                            auto v = j["value"];
                            if (v.is_boolean()) point_val.value = v.get<bool>();
                            else if (v.is_number_integer()) point_val.value = v.get<int32_t>();
                            else if (v.is_number_float()) point_val.value = v.get<double>();
                            else if (v.is_string()) point_val.value = v.get<std::string>();
                        }
                    } catch (...) {
                        LogManager::getInstance().log("startup_recovery", LogLevel::WARN, 
                            "Point " + std::to_string(point_id) + " 값 파싱 실패");
                        continue;
                    }
                }
                
                // Redis에 저장 (SaveSinglePoint는 device:id:name 및 point:id:latest 둘 다 처리)
                std::string device_id_str = "device_" + std::to_string(point_config->getDeviceId());
                if (redis_data_writer_->SaveSinglePoint(point_val, device_id_str)) {
                    success_count++;
                    
                    // 🎯 AlarmEngine RAM 캐시도 함께 시딩 (Warm Startup 핵심)
                    AlarmEngine::getInstance().SeedPointValue(point_id, point_val.value);
                }
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("startup_recovery", LogLevel::WARN, 
                    "개별 포인트 복구 실패: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("startup_recovery", LogLevel::INFO, 
            "포인트 값 복구 완료: " + std::to_string(success_count) + "/" + 
            std::to_string(current_values.size()) + "개 성공");
            
        return success_count;
        
    } catch (const std::exception& e) {
        HandleRecoveryError("RecoverLatestPointValues", e.what());
        return 0;
    }
}

} // namespace Alarm
} // namespace PulseOne