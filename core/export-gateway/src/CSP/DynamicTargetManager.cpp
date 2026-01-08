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
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "Client/RedisClientImpl.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
// ✅ v6.2.2: ExportTargetEntity.h 명시적 include (완전한 타입 정의)
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include <algorithm>

namespace PulseOne {
namespace CSP {

using namespace PulseOne::Export;
using namespace PulseOne::Database;

// =============================================================================
// 싱글턴 구현
// =============================================================================

DynamicTargetManager& DynamicTargetManager::getInstance() {
    static DynamicTargetManager instance;
    return instance;
}

// =============================================================================
// 생성자 및 소멸자 (private)
// =============================================================================

DynamicTargetManager::DynamicTargetManager() 
    : publish_client_(nullptr) {
    
    LogManager::getInstance().Info("DynamicTargetManager 싱글턴 생성");
    
    startup_time_ = std::chrono::system_clock::now();
    
    // 기본 핸들러 등록
    registerDefaultHandlers();
    
    // 글로벌 설정 초기화
    global_settings_ = json{
        {"health_check_interval_sec", 60},
        {"metrics_collection_interval_sec", 30},
        {"max_concurrent_requests", 100}
    };
    
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
    } catch (const std::exception& e) {
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
        auto& config = ConfigManager::getInstance();
        std::string redis_host = config.getOrDefault("REDIS_PRIMARY_HOST", "127.0.0.1");
        int redis_port = config.getInt("REDIS_PRIMARY_PORT", 6379);
        std::string redis_password = config.getOrDefault("REDIS_PRIMARY_PASSWORD", "");
        
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
        
        LogManager::getInstance().Info("✅ PUBLISH 전용 Redis 연결 성공: " + 
                                      redis_host + ":" + std::to_string(redis_port));
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis 연결 초기화 예외: " + std::string(e.what()));
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
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis 연결 상태 확인 예외: " + std::string(e.what()));
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
        
        for (const auto& target : targets_) {
            if (target.enabled) {
                // 🔧 수정 2-3: FailureProtectorConfig 올바른 필드명 사용
                FailureProtectorConfig fp_config;
                fp_config.failure_threshold = 5;
                fp_config.recovery_timeout_ms = 30000;  // 30초 = 30000ms
                fp_config.half_open_max_attempts = 3;   // half_open_max_calls → half_open_max_attempts
                
                failure_protectors_[target.name] = 
                    std::make_unique<FailureProtector>(target.name, fp_config);
                
                LogManager::getInstance().Debug(
                    "Failure Protector 생성: " + target.name);
            }
        }
    }
    
    // 4. 백그라운드 스레드 시작
    is_running_.store(true, std::memory_order_release);
    startBackgroundThreads();
    
    LogManager::getInstance().Info("DynamicTargetManager 시작 완료 ✅");
    // 🔧 수정 4: 문자열 연결 타입 에러 수정
    LogManager::getInstance().Info("- PUBLISH Redis: " + 
                                  std::string(isRedisConnected() ? "연결됨" : "연결안됨"));
    LogManager::getInstance().Info("- 활성 타겟: " + 
                                  std::to_string(targets_.size()) + "개");
    
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
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 연결 종료 예외: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
}

// =============================================================================
// ✅ DB 기반 설정 관리 - 핵심 수정 부분!
// =============================================================================

bool DynamicTargetManager::loadFromDatabase() {
    try {
        LogManager::getInstance().Info("DB에서 타겟 로드 시작...");
        
        // ✅ RepositoryFactory 자동 초기화 추가
        auto& factory = RepositoryFactory::getInstance();
        if (!factory.isInitialized()) {
            LogManager::getInstance().Warn("⚠️ RepositoryFactory 미초기화 감지 - 자동 초기화 시도");
            
            if (!factory.initialize()) {
                LogManager::getInstance().Error("❌ RepositoryFactory 초기화 실패");
                return false;
            }
            
            LogManager::getInstance().Info("✅ RepositoryFactory 자동 초기화 완료");
        }
        
        auto export_target_repo = factory.getExportTargetRepository();
        
        if (!export_target_repo) {
            LogManager::getInstance().Error("ExportTargetRepository를 가져올 수 없음");
            return false;
        }
        
        using PulseOne::Database::Entities::ExportTargetEntity;
        auto entities = export_target_repo->findByEnabled(true);
        
        if (entities.empty()) {
            LogManager::getInstance().Warn("활성화된 타겟이 없음");
            return false;
        }
        
        std::unique_lock<std::shared_mutex> lock(targets_mutex_);
        targets_.clear();
        
        int loaded_count = 0;
        for (const auto& entity : entities) {
            try {
                DynamicTarget target;
                target.name = entity.getName();
                target.type = entity.getTargetType();
                target.enabled = entity.isEnabled();
                target.priority = 100;
                target.description = entity.getDescription();
                
                try {
                    target.config = json::parse(entity.getConfig());
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error("Config JSON 파싱 실패: " + 
                        entity.getName() + " - " + std::string(e.what()));
                    continue;
                }
                
                // export_mode 설정
                std::string export_mode = entity.getExportMode();
                if (export_mode.empty() || export_mode == "0") {
                    export_mode = "alarm";
                }
                target.config["export_mode"] = export_mode;
                
                if (entity.getExportInterval() > 0) {
                    target.config["export_interval"] = entity.getExportInterval();
                }
                
                if (entity.getBatchSize() > 0) {
                    target.config["batch_size"] = entity.getBatchSize();
                }
                
                if (entity.getTemplateId().has_value()) {
                    target.config["template_id"] = entity.getTemplateId().value();
                }
                
                targets_.push_back(target);
                loaded_count++;
                
                LogManager::getInstance().Debug(
                    "타겟 로드: " + target.name + " (" + target.type + "), " +
                    "export_mode=" + export_mode
                );
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("타겟 엔티티 처리 실패: " + std::string(e.what()));
                continue;
            }
        }
        
        LogManager::getInstance().Info("✅ DB에서 " + std::to_string(loaded_count) + "개 타겟 로드 완료");
        return (loaded_count > 0);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DB 로드 실패: " + std::string(e.what()));
        return false;
    }
}






bool DynamicTargetManager::forceReload() {
    LogManager::getInstance().Info("강제 리로드...");
    return loadFromDatabase();
}

bool DynamicTargetManager::reloadDynamicTargets() {
    return loadFromDatabase();
}

std::optional<DynamicTarget> DynamicTargetManager::getTargetWithTemplate(const std::string& name) {
    return getTarget(name);
}

std::optional<DynamicTarget> DynamicTargetManager::getTarget(const std::string& name) {
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

// =============================================================================
// 타겟 관리 (나머지 메서드들)
// =============================================================================

bool DynamicTargetManager::addOrUpdateTarget(const DynamicTarget& target) {
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

bool DynamicTargetManager::removeTarget(const std::string& name) {
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

bool DynamicTargetManager::setTargetEnabled(const std::string& name, bool enabled) {
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

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmToTargets(const AlarmMessage& alarm) {
    std::vector<TargetSendResult> results;
    
    // ✅ 1. Redis PUBLISH (옵션 - 다른 시스템에 알람 전파)
    if (publish_client_ && publish_client_->isConnected()) {
        try {
            json alarm_json;
            alarm_json["bd"] = alarm.bd;
            alarm_json["nm"] = alarm.nm;
            alarm_json["vl"] = alarm.vl;
            alarm_json["tm"] = alarm.tm;
            alarm_json["al"] = alarm.al;
            alarm_json["des"] = alarm.des;
            alarm_json["st"] = alarm.st;
            
            int subscriber_count = publish_client_->publish(
                "alarms:processed", 
                alarm_json.dump()
            );
            
            LogManager::getInstance().Debug(
                "알람 발행 완료: " + std::to_string(subscriber_count) + "명 구독 중"
            );
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn(
                "알람 발행 실패: " + std::string(e.what())
            );
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
        std::string export_mode = "alarm"; // 기본값
        if (targets_[i].config.contains("export_mode")) {
            export_mode = targets_[i].config["export_mode"].get<std::string>();
        }
        
        // alarm 모드가 아니면 스킵
        if (export_mode != "alarm") {
            filtered_count++;
            LogManager::getInstance().Debug(
                "타겟 스킵 (export_mode=" + export_mode + "): " + targets_[i].name
            );
            continue;
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
        result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        results.push_back(result);
    }
    
    total_requests_.fetch_add(results.size());
    
    if (results.empty()) {
        LogManager::getInstance().Warn(
            "알람 타겟 없음 (필터링: " + std::to_string(filtered_count) + "개)"
        );
    } else {
        LogManager::getInstance().Info(
            "알람 전송 완료: " + std::to_string(sent_count) + "개 타겟 " +
            "(필터링: " + std::to_string(filtered_count) + "개)"
        );
    }
    
    return results;
}

TargetSendResult DynamicTargetManager::sendAlarmToTarget(
    const std::string& target_name, 
    const AlarmMessage& alarm) {
    
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

BatchTargetResult DynamicTargetManager::sendBatchAlarms(const std::vector<AlarmMessage>& alarms) {
    BatchTargetResult batch_result;
    
    for (const auto& alarm : alarms) {
        auto results = sendAlarmToTargets(alarm);
        
        for (const auto& result : results) {
            if (result.success) {
                // 🔧 수정 7: success_count → successful_targets
                batch_result.successful_targets++;
            } else {
                // 🔧 수정 7: failure_count → failed_targets
                batch_result.failed_targets++;
            }
        }
        
        // 🔧 수정 7: target_results → results
        batch_result.results.insert(batch_result.results.end(),
                                    results.begin(), results.end());
    }
    
    // 🔧 수정 7: success_count, failure_count → successful_targets, failed_targets
    batch_result.total_targets = batch_result.successful_targets + batch_result.failed_targets;
    
    return batch_result;
}

std::future<std::vector<TargetSendResult>> DynamicTargetManager::sendAlarmAsync(const AlarmMessage& alarm) {
    return std::async(std::launch::async, [this, alarm]() {
        return sendAlarmToTargets(alarm);
    });
}

// =============================================================================
// Failure Protector 관리
// =============================================================================

FailureProtectorStats DynamicTargetManager::getFailureProtectorStatus(const std::string& target_name) const {
    auto it = failure_protectors_.find(target_name);
    if (it != failure_protectors_.end()) {
        return it->second->getStats();
    }
    
    return FailureProtectorStats{};
}

void DynamicTargetManager::resetFailureProtector(const std::string& target_name) {
    auto it = failure_protectors_.find(target_name);
    if (it != failure_protectors_.end()) {
        it->second->reset();
    }
}

void DynamicTargetManager::resetAllFailureProtectors() {
    for (auto& [name, protector] : failure_protectors_) {
        protector->reset();
    }
}

void DynamicTargetManager::forceOpenFailureProtector(const std::string& target_name) {
    auto it = failure_protectors_.find(target_name);
    if (it != failure_protectors_.end()) {
        LogManager::getInstance().Info("강제 OPEN: " + target_name);
    }
}

std::unordered_map<std::string, FailureProtectorStats> DynamicTargetManager::getFailureProtectorStats() const {
    std::unordered_map<std::string, FailureProtectorStats> stats;
    
    for (const auto& [name, protector] : failure_protectors_) {
        stats[name] = protector->getStats();
    }
    
    return stats;
}

// =============================================================================
// 핸들러 관리
// =============================================================================

void DynamicTargetManager::registerDefaultHandlers() {
    handlers_["http"] = std::make_unique<HttpTargetHandler>();
    handlers_["s3"] = std::make_unique<S3TargetHandler>();
    handlers_["file"] = std::make_unique<FileTargetHandler>();
    
    LogManager::getInstance().Info("기본 핸들러 등록 완료: HTTP, S3, File");
}

bool DynamicTargetManager::registerHandler(
    const std::string& type_name,
    std::unique_ptr<ITargetHandler> handler) {
    
    if (!handler) {
        return false;
    }
    
    handlers_[type_name] = std::move(handler);
    LogManager::getInstance().Info("핸들러 등록: " + type_name);
    
    return true;
}

bool DynamicTargetManager::unregisterHandler(const std::string& type_name) {
    return handlers_.erase(type_name) > 0;
}

std::vector<std::string> DynamicTargetManager::getSupportedHandlerTypes() const {
    std::vector<std::string> types;
    
    for (const auto& [type, _] : handlers_) {
        types.push_back(type);
    }
    
    return types;
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

json DynamicTargetManager::getStatistics() const {
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - startup_time_).count();
    
    uint64_t total_reqs = total_requests_.load();
    uint64_t avg_response_time = total_reqs > 0 ? 
        (total_response_time_ms_.load() / total_reqs) : 0;
    
    return json{
        {"total_requests", total_reqs},
        {"total_successes", total_successes_.load()},
        {"total_failures", total_failures_.load()},
        {"success_rate", total_reqs > 0 ? 
            (double)total_successes_.load() / total_reqs * 100 : 0.0},
        {"concurrent_requests", concurrent_requests_.load()},
        {"peak_concurrent_requests", peak_concurrent_requests_.load()},
        {"total_bytes_sent", total_bytes_sent_.load()},
        {"avg_response_time_ms", avg_response_time},
        {"uptime_seconds", uptime}
    };
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
    
    for (const auto& target : targets_) {
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
    
    return json{
        {"status", is_running_.load() ? "running" : "stopped"},
        {"redis_connected", redis_connected},
        {"total_targets", targets_.size()},
        {"enabled_targets", enabled_count},
        {"healthy_targets", healthy_count},
        {"handlers_count", handlers_.size()}
    };
}

void DynamicTargetManager::updateGlobalSettings(const json& settings) {
    global_settings_ = settings;
    LogManager::getInstance().Info("글로벌 설정 업데이트");
}

// =============================================================================
// Private 유틸리티 메서드들
// =============================================================================

std::vector<DynamicTarget>::iterator DynamicTargetManager::findTarget(const std::string& target_name) {
    return std::find_if(targets_.begin(), targets_.end(),
        [&target_name](const DynamicTarget& t) {
            return t.name == target_name;
        });
}

std::vector<DynamicTarget>::const_iterator DynamicTargetManager::findTarget(const std::string& target_name) const {
    return std::find_if(targets_.begin(), targets_.end(),
        [&target_name](const DynamicTarget& t) {
            return t.name == target_name;
        });
}

bool DynamicTargetManager::processTargetByIndex(
    size_t index, 
    const AlarmMessage& alarm, 
    TargetSendResult& result) {
    
    const auto& target = targets_[index];
    
    // ✅ 타겟 이름 먼저 설정
    result.target_name = target.name;
    result.target_type = target.type;
    
    auto handler_it = handlers_.find(target.type);
    if (handler_it == handlers_.end()) {
        result.success = false;
        result.error_message = "핸들러를 찾을 수 없음: " + target.type;
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
        // ✅ send() → sendAlarm()
        auto handler_result = handler_it->second->sendAlarm(alarm, target.config);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // ✅ 필요한 필드만 복사
        result.success = handler_result.success;
        result.status_code = handler_result.status_code;  // http_status_code → status_code
        result.response_time = handler_result.response_time;
        result.error_message = handler_result.error_message;
        result.content_size = handler_result.content_size;  // bytes_sent → content_size
        result.retry_count = handler_result.retry_count;
        
        // Circuit Breaker 업데이트
        if (fp_it != failure_protectors_.end()) {
            if (result.success) {
                fp_it->second->recordSuccess();  // 파라미터 제거
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
        
        total_response_time_ms_.fetch_add(duration.count(), std::memory_order_relaxed);
        // total_bytes_sent_ 라인 제거 (content_size는 TargetSendResult에만 있음)
        
        return result.success;
        
    } catch (const std::exception& e) {
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
        
        LogManager::getInstance().Error(
            "타겟 처리 예외: " + target.name + " - " + std::string(e.what()));
        
        return false;
    }
}


json DynamicTargetManager::expandConfigVariables(const json& config, const AlarmMessage& alarm) {
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
    
    health_check_thread_ = std::make_unique<std::thread>([this]() {
        healthCheckThread();
    });
    
    metrics_thread_ = std::make_unique<std::thread>([this]() {
        metricsCollectorThread();
    });
    
    cleanup_thread_ = std::make_unique<std::thread>([this]() {
        cleanupThread();
    });
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
        cv_.wait_for(lock, std::chrono::seconds(60), [this]{ return should_stop_.load(); });
        
        if (should_stop_.load()) break;
        
        // 헬스체크 로직 (생략)
    }
}

// 메트릭 수집 스레드 (30초 주기)
void DynamicTargetManager::metricsCollectorThread() {
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::seconds(30), [this]{ return should_stop_.load(); });
        
        if (should_stop_.load()) break;
        
        // 메트릭 수집 로직 (생략)
    }
}

// 정리 스레드 (300초 주기)
void DynamicTargetManager::cleanupThread() {
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::seconds(300), [this]{ return should_stop_.load(); });
        
        if (should_stop_.load()) break;
        
        // 정리 로직 (생략)
    }
}

} // namespace CSP
} // namespace PulseOne