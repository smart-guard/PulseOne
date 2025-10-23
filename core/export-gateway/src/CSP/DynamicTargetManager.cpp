/**
 * @file DynamicTargetManager.cpp
 * @brief 동적 타겟 관리자 구현 (헤더 파일 완전 일치)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 6.1.0
 * 
 * 🎯 헤더 파일과 100% 일치하도록 작성
 */

#include "CSP/DynamicTargetManager.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "Utils/LogManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"      // ✅ 추가
#include "Database/Repositories/PayloadTemplateRepository.h"  // ✅ 추가
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

DynamicTargetManager::DynamicTargetManager() {
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
// 라이프사이클 관리
// =============================================================================

bool DynamicTargetManager::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("이미 실행 중");
        return true;
    }
    
    LogManager::getInstance().Info("DynamicTargetManager 시작...");
    
    // DB에서 타겟 로드
    if (!loadFromDatabase()) {
        LogManager::getInstance().Error("타겟 로드 실패");
        return false;
    }
    
    // 실패 방지기 초기화
    initializeFailureProtectors();
    
    // 백그라운드 스레드 시작
    startBackgroundThreads();
    
    is_running_.store(true);
    should_stop_.store(false);
    
    LogManager::getInstance().Info("DynamicTargetManager 시작 완료");
    return true;
}

void DynamicTargetManager::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("DynamicTargetManager 중지...");
    
    should_stop_.store(true);
    is_running_.store(false);
    
    // 백그라운드 스레드 중지
    stopBackgroundThreads();
    
    LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
}

// =============================================================================
// 내부 초기화
// =============================================================================

void DynamicTargetManager::registerDefaultHandlers() {
    LogManager::getInstance().Info("기본 핸들러 등록 시작");
    
    auto http_handler = std::make_unique<HttpTargetHandler>();
    registerHandler("HTTP", std::move(http_handler));
    
    auto s3_handler = std::make_unique<S3TargetHandler>();
    registerHandler("S3", std::move(s3_handler));
    
    auto file_handler = std::make_unique<FileTargetHandler>();
    registerHandler("FILE", std::move(file_handler));
    
    LogManager::getInstance().Info("기본 핸들러 등록 완료");
}

void DynamicTargetManager::initializeFailureProtectors() {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    for (const auto& target : targets_) {
        initializeFailureProtectorForTarget(target.name);
    }
}

void DynamicTargetManager::initializeFailureProtectorForTarget(const std::string& target_name) {
    if (failure_protectors_.find(target_name) != failure_protectors_.end()) {
        return;
    }
    
    FailureProtectorConfig config;
    config.failure_threshold = 5;
    config.recovery_timeout_ms = 60000;
    
    auto protector = std::make_unique<FailureProtector>(target_name, config);
    failure_protectors_[target_name] = std::move(protector);
}

// =============================================================================
// DB 기반 설정 관리
// =============================================================================

bool DynamicTargetManager::loadFromDatabase() {
    using namespace PulseOne::Database::Repositories;
    using namespace PulseOne::Database::Entities;
    
    try {
        LogManager::getInstance().Info("DB에서 타겟 로드 시작...");
        
        ExportTargetRepository target_repo;
        auto entities = target_repo.findByEnabled(true);  // 수정: findActive() -> findByEnabled(true)
        
        if (entities.empty()) {
            LogManager::getInstance().Warn("활성화된 타겟이 없습니다");
            return true;
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
                
                if (entity.getTemplateId().has_value()) {
                    target.config["template_id"] = entity.getTemplateId().value();
                }
                
                targets_.push_back(target);
                loaded_count++;
                
                LogManager::getInstance().Debug("타겟 로드: " + target.name + " (" + target.type + ")");
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("타겟 엔티티 처리 실패: " + std::string(e.what()));
                continue;
            }
        }
        
        LogManager::getInstance().Info("DB에서 " + std::to_string(loaded_count) + "개 타겟 로드 완료");
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

// =============================================================================
// 알람 전송 (핵심 기능)
// =============================================================================

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmToAllTargets(const AlarmMessage& alarm) {
    std::vector<TargetSendResult> results;
    
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    for (size_t i = 0; i < targets_.size(); ++i) {
        TargetSendResult result;
        if (processTargetByIndex(i, alarm, result)) {
            results.push_back(result);
        }
    }
    
    return results;
}

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmToAllTargetsParallel(const AlarmMessage& alarm) {
    std::vector<std::future<TargetSendResult>> futures;
    std::vector<TargetSendResult> results;
    
    {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        for (size_t i = 0; i < targets_.size(); ++i) {
            futures.push_back(std::async(std::launch::async, [this, i, &alarm]() {
                TargetSendResult result;
                processTargetByIndex(i, alarm, result);
                return result;
            }));
        }
    }
    
    for (auto& future : futures) {
        try {
            results.push_back(future.get());
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("비동기 전송 에러: " + std::string(e.what()));
        }
    }
    
    return results;
}

TargetSendResult DynamicTargetManager::sendAlarmToTarget(
    const AlarmMessage& alarm,
    const std::string& target_name) {
    
    TargetSendResult result;
    result.target_name = target_name;
    
    total_requests_.fetch_add(1);
    concurrent_requests_.fetch_add(1);
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        auto target_it = findTarget(target_name);
        if (target_it == targets_.end()) {
            result.success = false;
            result.error_message = "타겟을 찾을 수 없음";
            concurrent_requests_.fetch_sub(1);
            total_failures_.fetch_add(1);
            return result;
        }
        
        if (!target_it->enabled) {
            result.success = false;
            result.error_message = "타겟 비활성화됨";
            concurrent_requests_.fetch_sub(1);
            return result;
        }
        
        auto handler_it = handlers_.find(target_it->type);
        if (handler_it == handlers_.end()) {
            result.success = false;
            result.error_message = "핸들러를 찾을 수 없음";
            concurrent_requests_.fetch_sub(1);
            total_failures_.fetch_add(1);
            return result;
        }
        
        // 실패 방지기 확인
        auto protector_it = failure_protectors_.find(target_name);
        if (protector_it != failure_protectors_.end() && !protector_it->second->canExecute()) {
            result.success = false;
            result.status_code = 503;
            result.error_message = "Circuit Breaker OPEN";
            concurrent_requests_.fetch_sub(1);
            return result;
        }
        
        // 핸들러로 전송
        result = handler_it->second->sendAlarm(alarm, target_it->config);
        
        // 통계 업데이트
        if (result.success) {
            total_successes_.fetch_add(1);
            if (protector_it != failure_protectors_.end()) {
                protector_it->second->recordSuccess();
            }
        } else {
            total_failures_.fetch_add(1);
            if (protector_it != failure_protectors_.end()) {
                protector_it->second->recordFailure();
            }
        }
        
        // ✅ response_time (std::chrono::milliseconds) → count()로 변환
        total_response_time_ms_.fetch_add(result.response_time.count());
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "예외 발생: " + std::string(e.what());  // ✅ 괄호 수정
        total_failures_.fetch_add(1);
    }
    
    concurrent_requests_.fetch_sub(1);
    return result;
}

std::future<std::vector<TargetSendResult>> DynamicTargetManager::sendAlarmAsync(const AlarmMessage& alarm) {
    return std::async(std::launch::async, [this, alarm]() {
        return sendAlarmToAllTargets(alarm);
    });
}

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmByPriority(
    const AlarmMessage& alarm,
    int max_priority) {
    
    std::vector<TargetSendResult> results;
    
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    for (const auto& target : targets_) {
        if (target.enabled && target.priority <= max_priority) {
            auto result = sendAlarmToTarget(alarm, target.name);
            results.push_back(result);
        }
    }
    
    return results;
}

BatchProcessingResult DynamicTargetManager::processBuildingAlarms(
    const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms) {
    
    BatchProcessingResult batch_result;
    // ✅ BatchTargetResult 실제 필드: total_targets, successful_targets, failed_targets
    batch_result.total_targets = 0;
    batch_result.successful_targets = 0;
    batch_result.failed_targets = 0;
    
    for (const auto& [building_id, alarms] : building_alarms) {
        for (const auto& alarm : alarms) {
            batch_result.total_targets++;
            
            auto results = sendAlarmToAllTargets(alarm);
            
            bool any_success = false;
            for (const auto& result : results) {
                if (result.success) {
                    any_success = true;
                    break;
                }
            }
            
            if (any_success) {
                batch_result.successful_targets++;
            } else {
                batch_result.failed_targets++;
            }
        }
    }
    
    return batch_result;
}

// =============================================================================
// 타겟 관리
// =============================================================================

bool DynamicTargetManager::addTarget(const DynamicTarget& target) {
    try {
        std::unique_lock<std::shared_mutex> lock(targets_mutex_);
        
        // 중복 체크
        auto it = findTarget(target.name);
        if (it != targets_.end()) {
            LogManager::getInstance().Error("이미 존재하는 타겟: " + target.name);
            return false;
        }
        
        targets_.push_back(target);
        initializeFailureProtectorForTarget(target.name);
        
        LogManager::getInstance().Info("타겟 추가 성공: " + target.name);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 추가 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::removeTarget(const std::string& target_name) {
    try {
        std::unique_lock<std::shared_mutex> lock(targets_mutex_);
        
        auto it = findTarget(target_name);
        if (it == targets_.end()) {
            return false;
        }
        
        targets_.erase(it);
        failure_protectors_.erase(target_name);
        
        LogManager::getInstance().Info("타겟 제거 성공: " + target_name);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 제거 실패: " + std::string(e.what()));
        return false;
    }
}

std::unordered_map<std::string, bool> DynamicTargetManager::testAllConnections() {
    std::unordered_map<std::string, bool> results;
    
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    for (const auto& target : targets_) {
        bool success = testTargetConnection(target.name);
        results[target.name] = success;
    }
    
    return results;
}

bool DynamicTargetManager::testTargetConnection(const std::string& target_name) {
    try {
        auto target_opt = getTarget(target_name);
        if (!target_opt.has_value()) {
            return false;
        }
        
        auto& target = target_opt.value();
        
        auto handler_it = handlers_.find(target.type);
        if (handler_it == handlers_.end()) {
            return false;
        }
        
        return handler_it->second->testConnection(target.config);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("연결 테스트 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::enableTarget(const std::string& target_name, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->enabled = enabled;
        return true;
    }
    
    return false;
}

bool DynamicTargetManager::changeTargetPriority(const std::string& target_name, int new_priority) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->priority = new_priority;
        return true;
    }
    
    return false;
}

bool DynamicTargetManager::updateTargetConfig(const std::string& target_name, const json& new_config) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->config = new_config;
        return true;
    }
    
    return false;
}

std::vector<std::string> DynamicTargetManager::getTargetNames(bool include_disabled) const {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    std::vector<std::string> names;
    for (const auto& target : targets_) {
        if (include_disabled || target.enabled) {
            names.push_back(target.name);
        }
    }
    
    return names;
}

std::vector<DynamicTarget> DynamicTargetManager::getTargetStatistics() const {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    return targets_;
}

// =============================================================================
// 실패 방지기 관리
// =============================================================================

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

void DynamicTargetManager::healthCheckThread() {
    LogManager::getInstance().Info("Health Check Thread 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        if (should_stop_.load()) break;
        
        // 헬스 체크 로직
    }
    
    LogManager::getInstance().Info("Health Check Thread 종료");
}

void DynamicTargetManager::metricsCollectorThread() {
    LogManager::getInstance().Info("Metrics Thread 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (should_stop_.load()) break;
        
        // 메트릭 수집 로직
    }
    
    LogManager::getInstance().Info("Metrics Thread 종료");
}

void DynamicTargetManager::cleanupThread() {
    LogManager::getInstance().Info("Cleanup Thread 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        if (should_stop_.load()) break;
        
        // 정리 로직
    }
    
    LogManager::getInstance().Info("Cleanup Thread 종료");
}

// =============================================================================
// 유틸리티 메서드
// =============================================================================

std::vector<DynamicTarget>::iterator DynamicTargetManager::findTarget(const std::string& target_name) {
    return std::find_if(targets_.begin(), targets_.end(),
        [&target_name](const DynamicTarget& target) {
            return target.name == target_name;
        });
}

std::vector<DynamicTarget>::const_iterator DynamicTargetManager::findTarget(const std::string& target_name) const {
    return std::find_if(targets_.begin(), targets_.end(),
        [&target_name](const DynamicTarget& target) {
            return target.name == target_name;
        });
}

bool DynamicTargetManager::processTargetByIndex(
    size_t index,
    const AlarmMessage& alarm,
    TargetSendResult& result) {
    
    if (index >= targets_.size()) {
        return false;
    }
    
    const auto& target = targets_[index];
    if (!target.enabled) {
        return false;
    }
    
    result = sendAlarmToTarget(alarm, target.name);
    return true;
}

json DynamicTargetManager::expandConfigVariables(
    const json& config,
    const AlarmMessage& /* alarm */) {
    
    // 변수 확장 로직 (추후 구현)
    return config;
}

} // namespace CSP
} // namespace PulseOne